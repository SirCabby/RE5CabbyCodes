#include "mt.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "image.h"
#include "log.h"
#include "mem.h"

namespace re5cc::mt {
namespace {

std::vector<Class> g_classes;   // in name order
std::unordered_map<uintptr_t, int> g_by_dti;  // record -> index
std::unordered_map<uint32_t, int> g_by_id;    // record's id -> index (the first, for an id two share)
bool g_ready = false;

bool in(const mem::Range& r, uintptr_t a, size_t n = 4) { return a >= r.begin && a + n <= r.end; }

// A record's fields, read only when they lie in the data sections.
bool record_ok(uintptr_t dti) {
  const mem::Range data = image::data(), rdata = image::rdata();
  if (!in(data, dti, 0x24)) return false;
  const auto vt = mem::read<uintptr_t>(dti), name = mem::read<uintptr_t>(dti + 4);
  return in(rdata, vt) && (!name || in(rdata, name, 1) || in(data, name, 1));
}

int by_name(const char* name) {
  auto it = std::lower_bound(g_classes.begin(), g_classes.end(), name,
                             [](const Class& c, const char* n) { return std::strcmp(c.name, n) < 0; });
  return it != g_classes.end() && std::strcmp(it->name, name) == 0 ? static_cast<int>(it - g_classes.begin()) : -1;
}

}  // namespace

bool init() {
  if (g_ready) return true;
  const mem::Range rdata = image::rdata(), data = image::data();
  if (rdata.empty() || data.empty()) {
    logf("ERROR: mt: the image's sections are unknown - no class records");
    return false;
  }
  // MtObject's record: the one whose name is "MtObject" and whose parent is a
  // record with no name - the root.
  uintptr_t root = 0;
  for (uintptr_t s : mem::find_cstrings({rdata}, "MtObject", 4)) {
    for (uintptr_t ref : mem::find_dwords(data, static_cast<uint32_t>(s), 16)) {
      const uintptr_t dti = ref - 4;
      if (!record_ok(dti)) continue;
      const auto parent = mem::read<uintptr_t>(dti + 0x10);
      if (record_ok(parent) && !mem::read<uintptr_t>(parent + 4) && mem::read<uintptr_t>(parent + 0x0C) == dti) {
        root = parent;
        break;
      }
    }
    if (root) break;
  }
  if (!root) {
    logf("ERROR: mt: MtObject's class record (and the root above it) not found - not MT Framework as known");
    return false;
  }
  // Every record under the root: depth first, children then siblings.
  std::vector<uintptr_t> stack{mem::read<uintptr_t>(root + 0x0C)};
  int bad = 0;
  while (!stack.empty() && g_classes.size() < 40000) {
    const uintptr_t dti = stack.back();
    stack.pop_back();
    if (!dti) continue;
    if (!record_ok(dti)) {
      ++bad;
      continue;
    }
    Class c;
    c.dti = dti;
    const auto name = mem::read<uintptr_t>(dti + 4);
    c.name = name ? reinterpret_cast<const char*>(name) : "";
    c.size = mem::read<uint32_t>(dti + 0x1C) & 0x7FFFFF;
    c.parent = mem::read<uintptr_t>(dti + 0x10);
    c.id = mem::read<uint32_t>(dti + 0x20);
    g_classes.push_back(c);
    stack.push_back(mem::read<uintptr_t>(dti + 0x08));
    stack.push_back(mem::read<uintptr_t>(dti + 0x0C));
  }
  std::sort(g_classes.begin(), g_classes.end(), [](const Class& a, const Class& b) { return std::strcmp(a.name, b.name) < 0; });
  for (size_t i = 0; i < g_classes.size(); ++i) {
    g_by_dti[g_classes[i].dti] = static_cast<int>(i);
    g_by_id.emplace(g_classes[i].id, static_cast<int>(i));
  }
  g_ready = !g_classes.empty();
  logf("mt: %d class records under the root at %08X%s", static_cast<int>(g_classes.size()), static_cast<unsigned>(root),
       bad ? " (some links left the data sections - skipped)" : "");
  return g_ready;
}

int class_count() { return static_cast<int>(g_classes.size()); }

namespace {

// getDTI functions (`mov eax, record; ret`) by record, then the vtables holding
// each in slot 4 - the first one seen; more than one vtable answers a record
// when a class without a record of its own derives from it.
bool g_vtables_indexed = false;
std::unordered_multimap<uintptr_t, uintptr_t> g_all_vtables;  // record -> every vtable answering it

void index_vtables() {
  g_vtables_indexed = true;
  const mem::Range text = image::text(), rdata = image::rdata(), data = image::data();
  std::unordered_map<uintptr_t, uintptr_t> getdti;  // function -> record
  const auto* t = reinterpret_cast<const uint8_t*>(text.begin);
  for (size_t i = 0; i + 6 <= text.size(); ++i) {
    if (t[i] != 0xB8 || t[i + 5] != 0xC3) continue;
    uintptr_t rec = 0;
    std::memcpy(&rec, t + i + 1, 4);
    if (data.contains(rec) && g_by_dti.count(rec)) getdti.emplace(text.begin + i, rec);
  }
  int found = 0, shared = 0;
  for (uintptr_t a = (rdata.begin + 3) & ~uintptr_t(3); a + 4 <= rdata.end; a += 4) {
    const auto fn = *reinterpret_cast<const uintptr_t*>(a);
    const auto it = getdti.find(fn);
    if (it == getdti.end() || a < rdata.begin + 16) continue;
    Class& c = g_classes[g_by_dti[it->second]];
    g_all_vtables.emplace(it->second, a - 4 * 4);
    if (!c.vtable) {
      c.vtable = a - 4 * 4;
      ++found;
    } else {
      ++shared;
    }
  }
  logf("mt: %d class vtables found from their getDTI (%d more vtables answer a record of another class's)", found,
       shared);
}

}  // namespace

const Class* find(const char* name) {
  const int i = g_ready ? by_name(name) : -1;
  if (i < 0) return nullptr;
  if (!g_vtables_indexed) index_vtables();
  return &g_classes[i];
}

std::vector<uintptr_t> vtables_of(const Class* c) {
  std::vector<uintptr_t> out;
  if (!c) return out;
  if (!g_vtables_indexed) index_vtables();
  const auto range = g_all_vtables.equal_range(c->dti);
  for (auto it = range.first; it != range.second; ++it) out.push_back(it->second);
  return out;
}

const Class* classes(int* count) {
  if (!g_vtables_indexed && g_ready) index_vtables();
  *count = static_cast<int>(g_classes.size());
  return g_classes.data();
}

const Class* by_dti(uintptr_t dti) {
  const auto it = g_by_dti.find(dti);
  return it == g_by_dti.end() ? nullptr : &g_classes[it->second];
}

const Class* by_id(uint32_t id) {
  const auto it = g_by_id.find(id);
  return it == g_by_id.end() ? nullptr : &g_classes[it->second];
}

uintptr_t dti_of_object(uintptr_t obj) {
  uintptr_t vt = 0, fn = 0;
  if (!mem::read_safe(obj, &vt) || !in(image::rdata(), vt + 16)) return 0;
  fn = mem::read<uintptr_t>(vt + 16);
  if (!in(image::text(), fn, 6)) return 0;
  const auto* b = reinterpret_cast<const uint8_t*>(fn);
  if (b[0] != 0xB8 || b[5] != 0xC3) return 0;
  uintptr_t dti = 0;
  std::memcpy(&dti, b + 1, 4);
  return in(image::data(), dti, 0x24) ? dti : 0;
}

const char* class_name_of(uintptr_t obj) {
  const uintptr_t dti = dti_of_object(obj);
  if (!dti) return nullptr;
  const auto name = mem::read<uintptr_t>(dti + 4);
  return name ? reinterpret_cast<const char*>(name) : nullptr;
}

bool derives(uintptr_t dti, uintptr_t ancestor) {
  for (int depth = 0; dti && depth < 64; ++depth) {
    if (dti == ancestor) return true;
    if (!in(image::data(), dti, 0x24)) return false;
    dti = mem::read<uintptr_t>(dti + 0x10);
  }
  return false;
}

bool is_a(uintptr_t obj, const Class* cls) { return cls && derives(dti_of_object(obj), cls->dti); }

}  // namespace re5cc::mt
