#pragma once

#include <windows.h>

#include "game.h"

// The tick: everything that reads or writes the game runs here, once a frame,
// on the game's own thread. re5dx9.exe's WinMain makes the application object
// (sApp) and runs its loop on the main thread: messages first, then - once the
// render thread has taken the last frame - one call of the main system's frame
// function, sBioMain's vtable slot 6, which runs every system's update in turn
// (units, players, enemies, the menus). That slot of sBioMain's vtable - found
// from the class's name (mt.h) - is re-pointed at a hook that ticks the mod and
// then runs the game's frame. sBioMain's vtable is its own (sMain's is another),
// so nothing else goes through it.
namespace re5cc::dispatch {

void init();      // from DllMain: records the main thread, makes the locks
bool install();   // from boot::unpacked(): the hook in sBioMain's vtable
void uninstall();

struct Snapshot {
  bool ready = false;        // discovery found what the panel needs
  game::Run run;             // the run as this tick saw it
  bool show_panel = false;   // the visibility rule, evaluated on the main thread
  bool show_itembox = false; // the item box window's (beside Item Management)
  bool show_end = false;     // Chapter Select's window alone (a Game Over screen's or a chapter's results', where offered)
  unsigned long main_thread = 0;
  unsigned long long ticks = 0;
};
Snapshot snapshot();  // any thread

bool on_main_thread();
HANDLE first_tick_event();  // signalled once, from the first tick

}  // namespace re5cc::dispatch
