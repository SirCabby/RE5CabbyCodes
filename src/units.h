#pragma once

#include <cstdint>

#include "mt.h"

// The units in play: MT Framework's sUnit keeps every live cUnit (players,
// enemies, items, the menus and the HUD alike) in 32 "move lines", each a
// singly linked list it runs every frame. The layout - the lines' stride and
// their fields, a unit's state flags and its next link - is read out of sUnit's
// own code for walking a line (its vtable slot 9), never written down here; the
// line count out of its frame (slot 6).
namespace re5cc::units {

bool discover();  // from game::discover(): sUnit's code read; false (logged) when it is not as known

// Calls `fn(unit, cls)` for every unit that is starting or running (state 1 or 2)
// - the class is the unit's record's (mt.h), null for a unit of a class with
// none. On the main thread, from the tick (the lists are the game's own and
// change only in its frame). Stops early when fn returns false.
template <typename Fn>
void each(uintptr_t sunit, Fn&& fn);

struct Layout {
  uint32_t stride = 0, line_flags = 0, line_head = 0, unit_flags = 0, unit_next = 0, lines = 0;
};
const Layout& layout();
// The class of a unit by its vtable (cached; mt::find's record for it).
const mt::Class* class_of(uintptr_t unit);

template <typename Fn>
void each(uintptr_t sunit, Fn&& fn) {
  const Layout& l = layout();
  if (!sunit || !l.lines) return;
  for (uint32_t i = 0; i < l.lines; ++i) {
    const uintptr_t line = sunit + i * l.stride;
    if ((*reinterpret_cast<const uint32_t*>(line + l.line_flags) >> 1) & 1) continue;  // the line is off
    int guard = 0;
    for (uintptr_t u = *reinterpret_cast<const uintptr_t*>(line + l.line_head); u && guard < 4096;
         u = *reinterpret_cast<const uintptr_t*>(u + l.unit_next), ++guard) {
      const uint32_t state = *reinterpret_cast<const uint32_t*>(u + l.unit_flags) & 7;
      if (state != 1 && state != 2) continue;
      if (!fn(u, class_of(u))) return;
    }
  }
}

}  // namespace re5cc::units
