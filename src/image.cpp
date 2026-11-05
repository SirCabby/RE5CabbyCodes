#include "image.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "log.h"

namespace re5cc::image {
namespace {

HMODULE g_exe = nullptr;
uintptr_t g_base = 0;
mem::Range g_text, g_rdata, g_data, g_whole;
const char* g_problem = "not looked at yet";
mem::Range g_iat;

// Our own import hooks, so a dump shows what the game had in each slot.
struct Noted {
  uintptr_t slot, original;
};
Noted g_noted[16];
int g_n_noted = 0;

// The module an address lies in (null for none).
HMODULE owner(uintptr_t a) {
  HMODULE m = nullptr;
  if (!a || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCSTR>(a), &m))
    return nullptr;
  return m;
}

// "module.dll!Export" for an address: the export of its module whose address it
// is (a forwarded import holds the forward's target, so the module is the
// target's - ntdll's for kernel32's heap functions).
void export_name(uintptr_t a, char* out, size_t n) {
  std::snprintf(out, n, "?");
  HMODULE m = owner(a);
  if (!m) return;
  char path[MAX_PATH] = "?";
  GetModuleFileNameA(m, path, MAX_PATH);
  const char* leaf = std::strrchr(path, '\\');
  leaf = leaf ? leaf + 1 : path;
  char dll[MAX_PATH];
  std::snprintf(dll, sizeof(dll), "%s", leaf);
  for (char* c = dll; *c; ++c) *c = static_cast<char>(tolower(static_cast<unsigned char>(*c)));
  const IMAGE_NT_HEADERS* nt = mem::nt_headers(m);
  std::snprintf(out, n, "%s!unknown_%08x", dll, static_cast<unsigned>(a));
  if (!nt) return;
  const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (!dir.VirtualAddress) return;
  const auto mb = reinterpret_cast<uintptr_t>(m);
  const auto* ex = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(mb + dir.VirtualAddress);
  const auto* fns = reinterpret_cast<const DWORD*>(mb + ex->AddressOfFunctions);
  const auto* names = reinterpret_cast<const DWORD*>(mb + ex->AddressOfNames);
  const auto* ords = reinterpret_cast<const WORD*>(mb + ex->AddressOfNameOrdinals);
  for (DWORD i = 0; i < ex->NumberOfNames; ++i)
    if (mb + fns[ords[i]] == a) {
      std::snprintf(out, n, "%s!%s", dll, reinterpret_cast<const char*>(mb + names[i]));
      return;
    }
  for (DWORD i = 0; i < ex->NumberOfFunctions; ++i)
    if (mb + fns[i] == a) {
      std::snprintf(out, n, "%s!#%lu", dll, static_cast<unsigned long>(i + ex->Base));
      return;
    }
}

}  // namespace

void init() {
  if (g_exe) return;
  g_exe = GetModuleHandleA(nullptr);
  g_base = reinterpret_cast<uintptr_t>(g_exe);
  IMAGE_NT_HEADERS* nt = mem::nt_headers(g_exe);
  if (!nt) {
    g_problem = "the program's headers are not a PE's";
    return;
  }
  const IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
  const unsigned n = nt->FileHeader.NumberOfSections;
  // Enigma keeps the game's sections first, names blanked, and .rsrc fifth.
  if (n < 5 || std::strncmp(reinterpret_cast<const char*>(s[4].Name), ".rsrc", 8) != 0) {
    g_problem = "the program's sections are not laid out as Enigma's wrapper of the game lays them out";
    return;
  }
  g_text = mem::section_range(g_exe, s[0]);
  g_rdata = mem::section_range(g_exe, s[1]);
  const mem::Range d2 = mem::section_range(g_exe, s[2]), d3 = mem::section_range(g_exe, s[3]);
  g_data = mem::Range{d2.begin, d3.end > d2.end ? d3.end : d2.end};
  g_whole = mem::Range{g_base, mem::section_range(g_exe, s[4]).end};
  g_problem = nullptr;
  logf("image: re5dx9.exe at %p, %u sections; the game's: code %08X+%X, rdata %08X+%X, data %08X+%X, .rsrc ends %08X",
       static_cast<void*>(g_exe), n, static_cast<unsigned>(g_text.begin), static_cast<unsigned>(g_text.size()),
       static_cast<unsigned>(g_rdata.begin), static_cast<unsigned>(g_rdata.size()), static_cast<unsigned>(g_data.begin),
       static_cast<unsigned>(g_data.size()), static_cast<unsigned>(g_whole.end));
}

uintptr_t base() { return g_base; }
HMODULE module() { return g_exe; }
mem::Range text() { return g_text; }
mem::Range rdata() { return g_rdata; }
mem::Range data() { return g_data; }
mem::Range whole() { return g_whole; }
const char* layout_problem() { return g_problem; }

mem::Range iat() {
  if (!g_iat.empty() || g_rdata.empty()) return g_iat;
  uintptr_t a = g_rdata.begin, last = 0;
  int zeros = 0;
  for (; a + 4 <= g_rdata.end; a += 4) {
    const auto v = mem::read<uintptr_t>(a);
    if (!v) {
      if (++zeros > 2) break;  // the separators between DLLs are single zeros
      continue;
    }
    zeros = 0;
    HMODULE m = owner(v);
    if (!m || m == g_exe) break;
    last = a;
  }
  if (last) g_iat = mem::Range{g_rdata.begin, last + 4};
  return g_iat;
}

bool unpacked() {
  if (g_problem || g_text.size() < 0x20000) return false;
  // Compressed bytes hold almost no "ret; int3" pairs and no runs of int3
  // padding; the game's code has them between functions throughout.
  const auto* p = reinterpret_cast<const uint8_t*>(g_text.begin);
  int rets = 0, pads = 0;
  for (size_t i = 0; i + 4 < 0x20000; ++i) {
    if (p[i] == 0xC3 && p[i + 1] == 0xCC) ++rets;
    if (p[i] == 0xCC && p[i + 1] == 0xCC && p[i + 2] == 0xCC && p[i + 3] == 0xCC) ++pads;
  }
  const uintptr_t first = mem::read<uintptr_t>(g_rdata.begin);
  return rets > 200 && pads > 200 && owner(first) && owner(first) != g_exe;
}

uintptr_t slot_of(void* fn) {
  const mem::Range r = iat();
  if (r.empty() || !fn) return 0;
  const std::vector<uintptr_t> hits = mem::find_dwords(r, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(fn)), 2);
  return hits.empty() ? 0 : hits[0];
}

void note_hooked_slot(uintptr_t slot, uintptr_t original) {
  for (int i = 0; i < g_n_noted; ++i)
    if (g_noted[i].slot == slot) return;
  if (g_n_noted < 16) g_noted[g_n_noted++] = Noted{slot, original};
}

void* hook_import(const char* what, void* fn, void* hook, uintptr_t* slot_out) {
  const uintptr_t slot = slot_of(fn);
  if (slot_out) *slot_out = slot;
  if (!slot) {
    if (!mem::find_dwords(iat(), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook)), 1).empty())
      logf("imports: the game's %s slot already holds our hook", what);
    else
      logf("imports: %s is not in the game's import table as %p - not hooked", what, fn);
    return nullptr;
  }
  if (!mem::write<uintptr_t>(slot, reinterpret_cast<uintptr_t>(hook))) {
    logf("imports: the game's %s slot (exe+0x%X) could not be written", what, static_cast<unsigned>(slot - g_base));
    return nullptr;
  }
  note_hooked_slot(slot, reinterpret_cast<uintptr_t>(fn));
  logf("imports: the game's %s slot (exe+0x%X) now points at our hook", what, static_cast<unsigned>(slot - g_base));
  return fn;
}

// The unpacked image - the game's five sections, not the wrappers' - written
// with raw offsets == RVAs, so the file maps 1:1 to the loaded image (the
// headers still describe Enigma's; tools/fix_dump.py makes it a PE again),
// and the import map beside it.
bool dump(const char* dir) {
  if (g_problem || !unpacked()) {
    logf("ERROR: dump: %s - not dumping", g_problem ? g_problem : "the image is not unpacked yet");
    return false;
  }
  char path[MAX_PATH];
  std::snprintf(path, sizeof(path), "%sre5dx9.imports.txt", dir);
  if (FILE* f = std::fopen(path, "w")) {
    std::fprintf(f, "# slotVA value dll!name - re5dx9.exe's import table as Enigma rebuilt it\n");
    const mem::Range r = iat();
    for (uintptr_t a = r.begin; a < r.end; a += 4) {
      uintptr_t v = mem::read<uintptr_t>(a);
      for (int i = 0; i < g_n_noted; ++i)
        if (g_noted[i].slot == a) v = g_noted[i].original;
      if (!v) continue;
      char name[MAX_PATH + 64];
      export_name(v, name, sizeof(name));
      std::fprintf(f, "%08x %08x %s\n", static_cast<unsigned>(a), static_cast<unsigned>(v), name);
    }
    std::fclose(f);
    logf("dump: import map written to %s", path);
  }
  const size_t size = g_whole.size();
  std::vector<uint8_t> buf(size, 0);
  for (size_t off = 0; off < size; off += 0x1000) {
    SIZE_T got = 0;
    const size_t chunk = size - off < 0x1000 ? size - off : 0x1000;
    ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(g_base + off), buf.data() + off, chunk, &got);
  }
  for (int i = 0; i < g_n_noted; ++i) {
    const uintptr_t off = g_noted[i].slot - g_base;
    if (off + 4 <= size) std::memcpy(buf.data() + off, &g_noted[i].original, 4);
  }
  std::snprintf(path, sizeof(path), "%sre5dx9.dumped.exe", dir);
  FILE* f = std::fopen(path, "wb");
  if (!f) {
    logf("ERROR: dump: cannot write %s", path);
    return false;
  }
  std::fwrite(buf.data(), 1, size, f);
  std::fclose(f);
  logf("dump: wrote %s (%u bytes) - tools/fix_dump.py makes it a PE for Ghidra", path, static_cast<unsigned>(size));
  return true;
}

}  // namespace re5cc::image
