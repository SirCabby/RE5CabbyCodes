#include "dispatch.h"

#include "cheats.h"
#include "config.h"
#include "crash.h"
#include "image.h"
#include "input.h"
#include "inventory.h"
#include "itembox.h"
#include "log.h"
#include "mem.h"
#include "menus.h"
#include "mt.h"
#include "overlay.h"

namespace re5cc::dispatch {
namespace {

// The process's primary thread: DllMain(PROCESS_ATTACH) runs on it (d3d9.dll
// is one of the exe's own imports), and so does the game's frame loop.
DWORD g_main_thread = 0;
constexpr int kFrameSlot = 6;  // sBioMain's frame function (sMain::vtable[6])
using FrameFn = void(__thiscall*)(void*);
FrameFn g_orig_frame = nullptr;
uintptr_t g_frame_entry = 0;  // the vtable entry re-pointed at hk_frame
Snapshot g_snap;
CRITICAL_SECTION g_snap_lock;
bool g_lock_ready = false;
volatile LONG g_first_tick = 0;
volatile LONG g_foreign_logged = 0;
HANDLE g_first_event = nullptr;
DWORD g_last_filter = 0;
DWORD g_last_rates = 0;
bool g_in_game_prev = false;
game::Screen g_screen_prev = game::Screen::kNone;

void tick(void* main) {
  const DWORD now = GetTickCount();
  if (!g_first_tick) {
    InterlockedExchange(&g_first_tick, 1);
    if (g_first_event) SetEvent(g_first_event);
    logf("main-thread tick running on thread %lu (sBioMain %p's frame)", GetCurrentThreadId(), main);
  }
  if (now - g_last_filter > 1000) {
    g_last_filter = now;
    reassert_crash_filter();
  }
  if (now - g_last_rates >= 1000) {
    g_last_rates = now;
    overlay::dx9::log_rates();
  }

  Snapshot s;
  s.main_thread = g_main_thread;
  s.ticks = g_snap.ticks + 1;
  s.ready = game::ready();
  if (s.ready && !config::get().disable_game) {
    game::tick(main, &s.run);
    if (s.run.in_game != g_in_game_prev) {
      logf("game %s", s.run.in_game ? "started" : "ended");
      g_in_game_prev = s.run.in_game;
    }
    if (s.run.screen != g_screen_prev) {
      logf("screen: %s", game::screen_name(s.run.screen));
      g_screen_prev = s.run.screen;
    }
    // The cheats run every tick, paused or not: the holds are put back every frame.
    cheats::tick(s.run);
    // The characters' cases: the view, and a count changed while paused.
    inventory::tick(s.run);
    // The item box: the view, and its changes while Item Management is browsing.
    itembox::tick(s.run);
    // Chapter Select from Item Management between chapters, and the way back to it.
    menus::tick(s.run);
  }
  s.show_panel = s.run.screen == game::Screen::kPause;
  s.show_itembox = s.run.screen == game::Screen::kItemBox;
  const menus::Where cs = menus::view().where;
  s.show_end = s.ready && !s.show_panel && !s.show_itembox &&
               ((s.run.game_over && cs == menus::Where::kGameOver) || (s.run.results && cs == menus::Where::kResults));
  // The game's mouse is let go while the panel is up (its own thread does it) -
  // at the pause, or opened over the game with the toggle key. Beside Item
  // Management, the Game Over screen and the results it is not: those screens take the mouse too,
  // and only a click over the window is kept from them (input.cpp).
  input::tick(!s.show_itembox && !s.show_end &&
              (s.show_panel || config::get().always_show || overlay::visible()));
  if (g_lock_ready) {
    EnterCriticalSection(&g_snap_lock);
    g_snap = s;
    LeaveCriticalSection(&g_snap_lock);
  }
}

void __thiscall hk_frame(void* main) {
  if (GetCurrentThreadId() == g_main_thread) {
    tick(main);
  } else if (!InterlockedExchange(&g_foreign_logged, 1)) {
    logf("dispatch: sBioMain's frame called from thread %lu, not the main thread %lu - no tick", GetCurrentThreadId(),
         g_main_thread);
  }
  g_orig_frame(main);
}

}  // namespace

void init() {
  g_main_thread = GetCurrentThreadId();
  g_first_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  InitializeCriticalSection(&g_snap_lock);
  g_lock_ready = true;
}

bool install() {
  const mt::Class* c = mt::find("sBioMain");
  if (!c || !c->vtable) {
    logf("ERROR: dispatch: sBioMain's vtable not found - no tick");
    return false;
  }
  const uintptr_t entry = c->vtable + kFrameSlot * sizeof(void*);
  void* was = mem::read<void*>(entry);
  // The frame function is the game's own code, not another tool's hook.
  if (!image::text().contains(reinterpret_cast<uintptr_t>(was))) {
    logf("ERROR: dispatch: sBioMain's frame slot holds %p, outside the game's code - no tick", was);
    return false;
  }
  g_orig_frame = reinterpret_cast<FrameFn>(was);
  if (!mem::write<void*>(entry, reinterpret_cast<void*>(&hk_frame))) {
    logf("ERROR: dispatch: sBioMain's vtable could not be written - no tick");
    g_orig_frame = nullptr;
    return false;
  }
  g_frame_entry = entry;
  logf("dispatch: sBioMain's frame (vtable %08X slot %d, exe+0x%X) hooked - the game's frame ticks the mod",
       static_cast<unsigned>(c->vtable), kFrameSlot, static_cast<unsigned>(reinterpret_cast<uintptr_t>(was) - image::base()));
  return true;
}

void uninstall() {
  if (g_frame_entry && g_orig_frame && mem::read<uintptr_t>(g_frame_entry) == reinterpret_cast<uintptr_t>(&hk_frame))
    mem::write<uintptr_t>(g_frame_entry, reinterpret_cast<uintptr_t>(g_orig_frame));
}

Snapshot snapshot() {
  if (!g_lock_ready) return Snapshot{};
  EnterCriticalSection(&g_snap_lock);
  const Snapshot s = g_snap;
  LeaveCriticalSection(&g_snap_lock);
  return s;
}

bool on_main_thread() { return GetCurrentThreadId() == g_main_thread; }
HANDLE first_tick_event() { return g_first_event; }

}  // namespace re5cc::dispatch
