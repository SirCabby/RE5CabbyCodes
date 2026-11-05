#pragma once

#include "switches.h"

namespace re5cc::config {

// One value the Watch diagnostic logs whenever it changes (reverse engineering
// aid only: the addresses are typed into the ini as offsets from re5dx9.exe's
// base; nothing the mod does depends on them).
struct Watch {
  unsigned rva = 0;
  unsigned size = 0;  // 1, 2 or 4 bytes
};
constexpr int kMaxWatch = 24;

struct Settings {
  int toggle_key = 0x76;  // VK_F7: hide/show the panel at its screen; opens the cheats over the game in play
  // Each cheat's switch (switches.h). The panel changes them, and every change
  // is written back to the ini (save_switches), so they come back on the next start.
  bool on[sw::kCount] = {};
  bool trace = false;        // verbose diagnostics
  bool always_show = false;  // debug: draw the panel everywhere, not only on its screens
  bool dump_image = false;   // write the unpacked exe and its import map beside the DLL once
  int nwatch = 0;            // "Watch = 0x846F9C:2,..." - log those values as they change (offsets from the exe's base)
  Watch watch[kMaxWatch];
  // "Disable = overlay,tick,game,input" turns whole subsystems off so a fault
  // can be bisected to one of them.
  bool disable_overlay = false;
  bool disable_tick = false;
  bool disable_game = false;
  bool disable_input = false;
};

const Settings& get();

// RE5CabbyCodes.ini next to the DLL. Written with documented defaults the
// first time so the options are discoverable.
void load(const char* dir);
const char* dir();  // the DLL's directory, with a trailing separator

// The switches as they are now, written into the ini: each one's line
// rewritten in place (or added, for an ini written before it existed), every
// other line and comment kept as it was.
bool save_switches(const bool (&on)[sw::kCount]);

}  // namespace re5cc::config
