#include "units.h"

#include <unordered_map>

#include "image.h"
#include "log.h"
#include "mem.h"

namespace re5cc::units {
namespace {

Layout g_layout;
std::unordered_map<uintptr_t, const mt::Class*> g_by_vtable;

// The first match of `pattern` in the `span` bytes from `fn`, or 0.
uintptr_t find_in(uintptr_t fn, size_t span, const char* pattern) {
  if (!image::text().contains(fn)) return 0;
  return mem::find_pattern(mem::Range{fn, fn + span}, pattern);
}

}  // namespace

bool discover() {
  const mt::Class* c = mt::find("sUnit");
  if (!c || !c->vtable) {
    logf("ERROR: units: sUnit's vtable not found");
    return false;
  }
  // Slot 9 walks one line: the line's address is sUnit + (index << shift); its
  // flags and first unit, then each unit's state flags and next link.
  //   mov eax,[esp+4]; shl eax,5; add eax,ecx; mov ecx,[eax+28]; shr ecx,1; test cl,1; jne;
  //   push esi; mov esi,[eax+2C]; test esi,esi; je; lea esp,[esp]; mov eax,[esi+4]; mov edx,eax;
  //   and dl,7; cmp dl,2; jne; test eax,400; je; mov eax,[esi]; mov edx,[eax+20]; mov ecx,esi;
  //   call edx; mov esi,[esi+8]; test esi,esi; jne
  const uintptr_t walk = find_in(mem::read<uintptr_t>(c->vtable + 9 * 4), 0x60,
                                 "8B 44 24 04 C1 E0 ?? 03 C1 8B 48 ?? D1 E9 F6 C1 01 75 ?? 56 8B 70 ?? 85 F6 74 ?? "
                                 "8D 64 24 00 8B 46 ?? 8B D0 80 E2 07 80 FA 02 75 ?? A9 00 04 00 00 74 ?? 8B 06 8B 50 "
                                 "?? 8B CE FF D2 8B 76 ?? 85 F6 75");
  // Slot 6 runs every line in its order: the order table, and how many lines.
  //   lea edi,[esi+420]; mov ebp,20; mov ecx,[edi]; mov eax,[esi]; mov edx,[eax+20]; push ecx
  const uintptr_t run = find_in(mem::read<uintptr_t>(c->vtable + 6 * 4), 0xC0,
                                "8D BE ?? ?? 00 00 BD ?? 00 00 00 8B 0F 8B 06 8B 50 ?? 51");
  if (!walk || !run) {
    logf("ERROR: units: sUnit's line walk (slot 9: %s) or its frame (slot 6: %s) is not the code known - no units",
         walk ? "found" : "missing", run ? "found" : "missing");
    return false;
  }
  Layout l;
  l.stride = 1u << mem::read<uint8_t>(walk + 6);
  l.line_flags = mem::read<uint8_t>(walk + 11);
  l.line_head = mem::read<uint8_t>(walk + 22);
  l.unit_flags = mem::read<uint8_t>(walk + 33);
  l.unit_next = mem::read<uint8_t>(walk + 62);
  l.lines = mem::read<uint8_t>(run + 7);
  // The lines must lie inside sUnit.
  if (!l.lines || l.lines > 64 || (l.lines - 1) * l.stride + l.line_head + 4 > c->size) {
    logf("ERROR: units: %u lines of %u bytes do not fit sUnit's %u bytes - no units", l.lines, l.stride, c->size);
    return false;
  }
  g_layout = l;
  logf("units: %u lines of %u bytes (flags +0x%X, first unit +0x%X); a unit's state +0x%X, next +0x%X", l.lines,
       l.stride, l.line_flags, l.line_head, l.unit_flags, l.unit_next);
  return true;
}

const Layout& layout() { return g_layout; }

const mt::Class* class_of(uintptr_t unit) {
  const uintptr_t vt = *reinterpret_cast<const uintptr_t*>(unit);
  const auto it = g_by_vtable.find(vt);
  if (it != g_by_vtable.end()) return it->second;
  const mt::Class* c = mt::by_dti(mt::dti_of_object(unit));
  g_by_vtable[vt] = c;
  return c;
}

}  // namespace re5cc::units
