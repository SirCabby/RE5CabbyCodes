#include "menus.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "image.h"
#include "inventory.h"
#include "itembox.h"
#include "log.h"
#include "mem.h"
#include "mt.h"

namespace re5cc::menus {
namespace {

CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
View g_view;
volatile LONG g_asked = 0;  // the panel's button, for the next tick
bool g_found = false;

// sArea's areas (kSigAreaFrame): a pointer each, as many as g_area_count, the first
// null ending them - the play flow (aGame) while a game is on, the title and the
// menus (aDemo) otherwise.
uint32_t g_area_list = 0, g_area_count = 0;
const mt::Class* g_cls_flow = nullptr;     // aGame
const mt::Class* g_cls_front = nullptr;    // aDemo
const mt::Class* g_cls_startup = nullptr;  // cDemoStartUp: the start-up menu's flow, aDemo's
const mt::Class* g_cls_sys = nullptr;      // cGameSys

// The play flow: the screen its step between chapters keeps (Item Management).
uint32_t g_flow_screen = 0;
// cGameSys: the game's mode - 0 the story's, the only one the play flow's Item Management
// step goes on in (1 Versus, 2 the Mercenaries, 3 Lost in Nightmares, 4 Desperate Escape, 5
// Reunion) - and sBioMain's field that step reads cGameSys through (discover_menus: the
// menus' own, g_sys). Chapter Select is offered in the story's mode only (story_game).
uint32_t g_mode = 0, g_mode_sys = 0;
uintptr_t g_withheld = 0;  // the screen last logged as left without Chapter Select (another mode)
// Item Management (uIdItemBox00): its frame's step - the main one (the panes), then
// the fade out its own quit and close go to - and what its quit writes besides: the
// quit byte the play flow takes, the message shown, the flags of a partner joining or
// leaving, the fade's timer.
uint32_t g_step = 0, g_main = 0, g_out = 0, g_quit = 0, g_message = 0, g_join1 = 0, g_join2 = 0, g_timer = 0;
// ... and the save its quit and its close both make first (thiscall, the screen): each
// pane's case put in order in the save data's working block, then the working block of
// each character in play into the saved state (the call the user approved, 2026-10-31).
using SaveFn = void(__thiscall*)(void* screen);
SaveFn g_save = nullptr;
// cGameInfo: the save data's working block, whose cases the screen changes, and the
// checkpoint copy of it the game's commit keeps - which the play flow's quit takes the
// working block (and the saved state) back from; the saved state, the block before it.
uint32_t g_block = 0, g_block_size = 0, g_block_copy = 0, g_state = 0;
uint32_t g_info_global = 0;  // sInfoMan's global, as the screen's working copy reads it
// The play flow's copy of the profile's cGameInfo as at the last checkpoint (its field),
// which RESTART and a death's CONTINUE take the save data's blocks back from - with the
// blocks, what the item box's moves at the pause are made in too (saves()).
uint32_t g_snapshot = 0;
const mt::Class* g_cls_info = nullptr;  // cGameInfo
bool g_saves = false;                   // understood

// The pause menu (uIdPause00): its panes - a local player's each - from +g_panes, g_pane_step
// apart, g_npanes of them; a pane's in-use byte (its first), its done byte, the option it
// runs (0 its menu) and its step in it, its player; QUIT's option and its steps: waiting for
// the save manager, asking the save question, quitting; the question's own step, timer and
// answer (0 yes) and the step that takes the answer; the menu's no-cancel byte and its byte
// set as a quit goes (network events let go), and the play flow's (no game over while it
// goes). The question is asked only of a player the save manager lets save: its slot of four
// (the save manager's global, its slots, a slot's step, its "cannot" byte).
bool g_pause = false;  // understood: Chapter Select at the pause
uint32_t g_panes = 0, g_pane_step = 0, g_npanes = 0, g_pane_done = 0, g_option = 0, g_pane_at = 0, g_pane_player = 0;
uint32_t g_quit_option = 0, g_quit_wait = 0, g_quit_ask = 0, g_quit_out = 0;
uint32_t g_ask_at = 0, g_ask_timer = 0, g_ask_answer = 0, g_ask_step = 0;
uint32_t g_no_cancel = 0, g_pause_leaving = 0, g_flow_leaving = 0;
uint32_t g_save_man = 0, g_save_last = 0, g_save_slots = 0, g_save_slot = 0, g_save_cannot = 0;
uint32_t g_flow_step = 0, g_flow_out = 0;  // the play flow's step and its quit's (kSigFlowQuit)
// sBioMain's flags (QUIT's bit among them), set as the game's own setter does: under its lock
// (sBioMain +g_main_lock) while the game uses it (its byte, or a global's).
uint32_t g_main_flags = 0, g_quit_flag = 0, g_main_mt = 0, g_main_lock = 0, g_mt_global = 0;
// The Game Over screen (uIdGameover00): shown once its opening is over, its step (the one that
// waits for a choice made elsewhere: a network partner's), its choice (0 CONTINUE, 1 QUIT, -1
// none), decided.
bool g_over = false;  // understood: Chapter Select at the Game Over screen
const mt::Class* g_cls_over = nullptr;
uint32_t g_over_shown = 0, g_over_step = 0, g_over_elsewhere = 0, g_over_choice = 0, g_over_decided = 0;
// The chapter's results (uIdChapEd00): their quit not offered (the last chapter, the
// Mercenaries: their continue is their quit then, back to the menus), their step - the
// menu's, and the one after their question's yes (the session let go, the save manager
// waited for, done) - their quit, its wait and timer, the prompts shown (none while it
// goes); the play flow's step after a game's end (the results' quit, the Game Over's QUIT).
bool g_results = false;  // understood: Chapter Select at the chapter's results
const mt::Class* g_cls_results = nullptr;
uint32_t g_res_noquit = 0, g_res_step = 0, g_res_menu = 0, g_res_quitting = 0, g_res_quit = 0, g_res_wait = 0;
uint32_t g_res_timer = 0, g_res_prompts = 0, g_res_prompts_off = 0, g_flow_after = 0;
// Whether Chapter Select can go at the Game Over screen or the results, as last logged: each
// change is logged with what the game shows there, so a report of the button greyed has its
// why in the log (the user, 2026-11-01: "sometimes on the chapter end screen, the chapter
// select button remains greyed out" - the last chapter's results, greyed then).
bool g_end_logged = false;
const char* g_end_why = nullptr;  // what it waited for (one of tick()'s texts), null: it could go
// sBioMain: cGameSys, the online flags (bit 0).
uint32_t g_sys = 0, g_online = 0;
// cGameSys: the menus' return reason, the story menu's choice, the top menu's - and
// their values here: the reason a game's start leaves (the menus from the top), the
// reason that goes back to the choice made (the menus' music on), the top menu's
// story, the story menu's CHAPTER SELECT and one past its last choice (parked).
uint32_t g_reason = 0, g_story = 0, g_top = 0;
uint32_t g_reason_normal = 0, g_reason_back = 0, g_top_story = 0, g_chapter_select = 0, g_story_park = 0;
// cDemoStartUp: its step, the one that takes the return reason, the step within it
// (0 the reason, then the back reason's), the story flow's step (the back reason's
// takes the choice; parked, it goes back to the top) and CHAPTER SELECT's own (0:
// its setup, then the chapter list).
uint32_t g_menu_step = 0, g_menu_reason_step = 0, g_sub = 0, g_sub_back = 0;
uint32_t g_story_step = 0, g_story_pick = 0, g_story_parked = 0, g_cs_step = 0;
// The menus' own load (aDemo's): its step, and the one after its start that has the
// loader (sLoadMan) choose the front end's archives - by a field its start sets from the
// return reason (a game's end: the title's alone) - before the start-up menu is made;
// sArea's "started" byte of an area. The story menu adds the menus' archive (a bit of
// that field) as a choice is made there, and the chapter list's layout is in it. The way
// back passes no choice: the chapter list made without it sent the game to a file that is
// not there, and it ended itself (the user's play, 2026-10-31: "using the chapter select
// option crashed the game" - top05.lsp). So the bit is added before that load, and the
// start-up menu handed over only with the archive loaded.
uint32_t g_front_step = 0, g_front_load = 0, g_started = 0;
uint32_t g_front_mask = 0, g_menus_bit = 0;  // the loader: the front end's archives, the menus' bit
const char* g_menus_path = nullptr;          // the menus' archive as the loader is asked for it (offline)
bool g_menus_asked = false;                  // the bit added, this way back
// The start-up menu's background (uIdTopMenu00: the title, the main menu's list and its
// cursor, a header, an animated panel), made as the start-up menu fades in and kept
// through all its menus. It reads the return reason once, as it starts - a game's end
// leaves the main menu's look - and the main menu's choice of the story and the story
// menu's of CHAPTER SELECT change it: choices the way back never passes, so the chapter
// list came up in front of the main menu (the user's play, 2026-10-31: "The chapter select
// screen is opened in front of the main menu for the game. Usually there is no background
// menu behind it"). So the way back makes what those choices write: the list told the
// story was picked (its "picked" and "go on" flags: it blinks, then leaves), the header
// shown, the panel's segment of the change from the story menu to CHAPTER SELECT's screen.
const mt::Class* g_cls_top = nullptr;     // uIdTopMenu00
const mt::Class* g_cls_list = nullptr;    // uIdStartUp11: the main menu's list
const mt::Class* g_cls_header = nullptr;  // uIdStartUp14: the header
bool g_look = false;                      // understood
uint32_t g_list = 0, g_header = 0;        // the background's list and header
uint32_t g_picked = 0, g_go_on = 0, g_shown = 0, g_shown_value = 0;  // their flags, the header's value
uint32_t g_request = 0, g_segment = 0, g_backward = 0;  // its panel: a segment asked for, its number, played back
int g_panel = -1;  // the panel's segment from the story menu to CHAPTER SELECT's screen (-1: none)

// The way back, once asked: at the pause the pause menu's QUIT taking the profile back and
// the play flow quitting (quitting); Item Management closing and the game ending (leaving),
// then the start-up menu fading in and taking its reason, its story flow parked for a
// tick.
enum class Phase { kIdle, kQuitting, kLeaving, kParked };
Phase g_phase = Phase::kIdle;
int g_age = 0;  // ticks in the phase
Where g_from = Where::kNone;  // where it was asked
uintptr_t g_pane = 0;         // at the pause: the pane told to quit
// The longest each part waits: the pause menu's steps to the quit (a few frames), the
// screen's own close (it fades out at once), the game's end and the menus' load, the
// start-up menu fading in (about a second) and taking its reason.
constexpr int kQuitTicks = 600, kCloseTicks = 600, kLeaveTicks = 60 * 120, kParkTicks = 60 * 10;

// The Item Management last seen (a new one lets go of the last note).
uintptr_t g_screen = 0;

// cSystemInfo: its data, then its checkpoint copy - the two fill the class past its
// vtable (game.cpp, kSigCheckpoint).
uint32_t profile_data() { return game::layout().checkpoint; }
uint32_t profile_start() { return game::layout().system_size - 2 * profile_data(); }

uint32_t u8at(uintptr_t a) { return mem::read<uint8_t>(a); }
uint32_t u32at(uintptr_t a) { return mem::read<uint32_t>(a); }
int s8at(uintptr_t a) { return mem::read<int8_t>(a); }
uintptr_t jump_case(uintptr_t table, uint32_t k) { return mem::read<uintptr_t>(table + 4 * k); }
// Where a `jmp rel32` (E9) at `at` goes, or 0.
uintptr_t jmp_target(uintptr_t at) { return u8at(at) == 0xE9 ? at + 5 + mem::read<int32_t>(at + 1) : 0; }

bool off(const char* why) {
  logf("menus: %s - no Chapter Select beside Item Management", why);
  return false;
}

bool no_saves(const char* why) {
  logf("menus: %s - the item box makes no moves at the pause", why);
  return false;
}

bool no_pause(const char* why) {
  logf("menus: %s - no Chapter Select at the pause", why);
  return false;
}

bool no_over(const char* why) {
  logf("menus: %s - no Chapter Select at the Game Over screen", why);
  return false;
}

bool no_results(const char* why) {
  logf("menus: %s - no Chapter Select at the chapter's results", why);
  return false;
}

// sBioMain's flags: one set, as the game's own setter does it.
void set_main_flag(uintptr_t main, uint32_t flag) {
  const bool lock = u8at(main + g_main_mt) || u8at(g_mt_global);
  auto* cs = reinterpret_cast<CRITICAL_SECTION*>(main + g_main_lock);
  if (lock) EnterCriticalSection(cs);
  mem::write<uint32_t>(main + g_main_flags, u32at(main + g_main_flags) | flag);
  if (lock) LeaveCriticalSection(cs);
}

void note(View& v, const char* text) { std::snprintf(v.note, sizeof(v.note), "%s", text); }

// Chapter Select at the Game Over screen or the results: what it waits for (the button
// greyed) or that it can go, with what the game shows there - the screen's steps, the menus'
// return reason and the top menu's choice the readiness goes by.
void log_end(uintptr_t over, uintptr_t results, uintptr_t gs, bool online, const char* why) {
  char state[96];
  if (over)
    std::snprintf(state, sizeof(state), "shown %u, decided %u, its step %u", u8at(over + g_over_shown),
                  u8at(over + g_over_decided), u8at(over + g_over_step));
  else
    std::snprintf(state, sizeof(state), "their step %u, %s", u8at(results + g_res_step),
                  u8at(results + g_res_noquit) ? "the last chapter's (no quit of their own)" : "their quit offered");
  logf("menus: Chapter Select at %s %s%s (%s; the return reason %d, the top menu's choice %d%s)",
       over ? "the Game Over screen" : "the chapter's results", why ? "greyed: " : "ready", why ? why : "", state,
       gs ? static_cast<int>(u32at(gs + g_reason)) : -1, gs ? static_cast<int>(u32at(gs + g_top)) : -1,
       online ? "; online" : "");
}

// The area of a class among sArea's, or 0.
uintptr_t area_of(const mt::Class* c) {
  const uintptr_t s = game::system("sBioArea");
  if (!s) return 0;
  for (uint32_t i = 0; i < g_area_count; ++i) {
    const uintptr_t a = mem::read<uintptr_t>(s + g_area_list + 4 * i);
    if (!a) break;
    if (mt::is_a(a, c)) return a;
  }
  return 0;
}

// The start-up menu: the cDemoStartUp aDemo runs, or 0.
uintptr_t start_up_menu() {
  const uintptr_t front = area_of(g_cls_front);
  for (uint32_t off = 4; front && off + 4 <= g_cls_front->size; off += 4) {
    const uintptr_t p = mem::read<uintptr_t>(front + off);
    if (p && !(p & 3) && mt::is_a(p, g_cls_startup)) return p;
  }
  return 0;
}

// The start-up menu's background once it has started (its list and header made), or 0.
uintptr_t background(uintptr_t menu) {
  for (uint32_t off = 4; g_look && off + 4 <= g_cls_startup->size; off += 4) {
    const uintptr_t p = mem::read<uintptr_t>(menu + off);
    if (!p || (p & 3) || !mt::is_a(p, g_cls_top)) continue;
    const uintptr_t list = mem::read<uintptr_t>(p + g_list), header = mem::read<uintptr_t>(p + g_header);
    return list && header && mt::is_a(list, g_cls_list) && mt::is_a(header, g_cls_header) ? p : 0;
  }
  return 0;
}

// The background as the main menu's choice of the story and the story menu's of CHAPTER
// SELECT leave it: the list picked and on its way (it blinks, then leaves), the header
// shown, the panel's segment of that change asked for, played forward.
void story_chosen(uintptr_t top) {
  const uintptr_t list = mem::read<uintptr_t>(top + g_list), header = mem::read<uintptr_t>(top + g_header);
  mem::write<uint8_t>(list + g_picked, 1);
  mem::write<uint8_t>(list + g_go_on, 1);
  mem::write<uint8_t>(header + g_shown, static_cast<uint8_t>(g_shown_value));
  if (g_panel >= 0) {
    mem::write<uint32_t>(top + g_segment, static_cast<uint32_t>(g_panel));
    mem::write<uint8_t>(top + g_backward, 0);
    mem::write<uint8_t>(top + g_request, 1);
  }
}

uintptr_t game_sys(const game::Run& run) {
  const uintptr_t p = run.main ? mem::read<uintptr_t>(run.main + g_sys) : 0;
  return p && mt::is_a(p, g_cls_sys) ? p : 0;
}

// A story game, started from the story's menu: the game's mode the story's (as the play
// flow's Item Management step tests it), the menus' return reason and the top menu's choice
// what a story start leaves. Lost in Nightmares and Desperate Escape are started from the
// story's menu too, but in modes of their own (3, 4) and with the return reason 9; Versus
// and the Mercenaries have their own. None has a chapter list to go back to, so there
// Chapter Select is not offered at all - no button, greyed or not (the user, 2026-11-03:
// "Instead of saying the game wasn't launched from chapter select, it should just not show
// up"). Logged once for each screen it is left off.
bool story_game(const game::Run& run, uintptr_t screen) {
  const uintptr_t gs = game_sys(run);
  if (gs && u32at(gs + g_mode) == 0 && u32at(gs + g_reason) == g_reason_normal && u32at(gs + g_top) == g_top_story)
    return true;
  const uintptr_t at = screen ? screen : run.pause_menu ? run.pause_menu : run.game_over ? run.game_over : run.results;
  if (at != g_withheld) {
    g_withheld = at;
    logf("menus: no Chapter Select on this screen - not a story game (the game's mode %d, the return reason %d, the "
         "top menu's choice %d)",
         gs ? static_cast<int>(u32at(gs + g_mode)) : -1, gs ? static_cast<int>(u32at(gs + g_reason)) : -1,
         gs ? static_cast<int>(u32at(gs + g_top)) : -1);
  }
  return false;
}

// The pause menu's pane of this machine's player: the one in use, not done - 0 when none,
// or more than one (two players on this machine).
uintptr_t pause_pane(uintptr_t pause) {
  uintptr_t pane = 0;
  int used = 0;
  for (uint32_t k = 0; k < g_npanes; ++k) {
    const uintptr_t p = pause + g_panes + k * g_pane_step;
    if (!u8at(p)) continue;
    ++used;
    pane = p;
  }
  return used == 1 && !u8at(pane + g_pane_done) ? pane : 0;
}

void stop(View& v, const char* text) {
  logf("menus: %s", text);
  note(v, text);
  g_phase = Phase::kIdle;
  g_age = 0;
}

// --- discovery --------------------------------------------------------------------------------
// The play flow's readiness (aGame's vtable slot 5) runs the steps before a chapter
// loads, Item Management's among them; after each, a quit asked by one takes the
// profile back from its copy and ends the game (back to the menus). Its Item
// Management step makes the screen (by its class record) into a field of its own,
// and as the screen finishes copies the screen's quit byte into that quit.
bool discover_flow(const mt::Class* box) {
  const uintptr_t quit = game::sig_one("kSigFlowQuit");
  const uintptr_t make = game::sig_one("kSigFlowItemBox");
  const uintptr_t done = game::sig_one("kSigFlowItemBoxDone");
  const uintptr_t ready = mem::read<uintptr_t>(g_cls_flow->vtable + 4 * 5);
  const uintptr_t step = quit ? game::call_target(quit + 2) : 0;
  if (!quit || !make || !done || quit < ready || quit > ready + 0x100 || !image::text().contains(step) ||
      make < step || make > step + 0x400 || done < step || done > step + 0x400)
    return off("the play flow's step for Item Management was not found");
  g_flow_screen = u8at(make + 14);
  const uint32_t flow_quit = u32at(quit + 18);
  if (u32at(make + 1) != box->dti || u8at(make + 30) != g_flow_screen || u8at(done + 11) != g_flow_screen ||
      u8at(done + 25) != g_flow_screen || u8at(done + 35) != g_flow_screen || u32at(done + 5) != flow_quit ||
      u8at(quit + 38) != u8at(quit + 45) || g_flow_screen + 4 > g_cls_flow->size || flow_quit >= g_cls_flow->size)
    return off("the play flow's step for Item Management (it disagrees)");
  g_quit = u8at(done + 2);
  // Its first step goes on only offline, in the story and not in No Mercy: each test, failing,
  // clears the flag the screen is shown by (the same push, the same call). The story's is
  // cGameSys's mode against 0 (`xor ebx,ebx; cmp [eax+mode],ebx`), cGameSys read through
  // sBioMain's field.
  const uintptr_t first =
      mem::matches(step, mem::parse_pattern("56 8B F1 8A 46 ?? 0F B6 C8 83 F9 ?? 0F 87 ?? ?? ?? ?? 53 57 33 DB FF 24 8D"))
          ? jump_case(u32at(step + 25), 0)
          : 0;
  const uintptr_t story =
      image::text().contains(first)
          ? mem::find_pattern(mem::Range{first, first + 0x60}, "8B 81 ?? ?? ?? ?? 33 DB 39 58 ?? 74 ?? 68 ?? ?? ?? ?? E8")
          : 0;
  if (!story || u8at(first + 16) != 0x68 || u32at(first + 17) != u32at(story + 14) || !u32at(story + 14) ||
      !game::call_target(story + 18) || game::call_target(first + 21) != game::call_target(story + 18))
    return off("the play flow's step for Item Management (its test of the story's mode)");
  g_mode_sys = u32at(story + 2);
  g_mode = u8at(story + 10);
  return true;
}

// Item Management's own quit (at the end of a co-op session there): the panes' cases
// saved, then the quit byte, the message, a partner's flags, the step of its fade out
// and the fade's timer. Its close, once both panes are done: the same save, its step
// on by one (main to fade out), "ok", not the quit, the timer; the fade out's handler,
// then the end's. Its frame's switch goes to them. The save: each of the two panes'
// cases put in order, then the copy into the saved state through sInfoMan (whose
// global discover_cases checks).
bool discover_screen(const mt::Class* box) {
  const uintptr_t quit = game::sig_one("kSigBoxQuit");
  const uintptr_t close = game::sig_one("kSigBoxClose");
  const uintptr_t frame = mem::read<uintptr_t>(box->vtable + 4 * 6);
  const uintptr_t sw = image::text().contains(frame)
                           ? mem::find_pattern(mem::Range{frame, frame + 0x40},
                                               "8B 45 ?? 25 FF 00 00 00 83 F8 ?? 0F 87 ?? ?? ?? ?? 0F B6 88 ?? ?? ?? ?? "
                                               "FF 24 8D ?? ?? ?? ??")
                           : 0;
  if (!quit || !close || !sw || close < frame || close > frame + 0x400)
    return off("Item Management's own quit and close were not found");
  g_message = u32at(quit + 13);
  g_join1 = u32at(quit + 19);
  g_join2 = u32at(quit + 26);
  g_step = u8at(quit + 36);
  g_out = u8at(quit + 37);
  g_timer = u32at(quit + 42);
  g_main = g_out - 1;
  const uint32_t steps = u8at(sw + 10) + 1;
  const uintptr_t t1 = u32at(sw + 20), t2 = u32at(sw + 27);
  auto handler = [&](uint32_t s) { return s < steps ? jump_case(t2, u8at(t1 + s)) : 0; };
  const uintptr_t save = game::call_target(quit + 2);
  const bool save_shape =
      image::text().contains(save) &&
      mem::matches(save, mem::parse_pattern("56 57 8B F9 33 F6 56 8B CF E8 ?? ?? ?? ?? 83 C6 01 83 FE 02 7C ?? 8B 0D "
                                            "?? ?? ?? ?? 5F 5E E9 ?? ?? ?? ??")) &&
      image::text().contains(game::call_target(save + 9)) && image::text().contains(jmp_target(save + 30));
  if (u8at(quit + 9) != g_quit || game::call_target(close + 2) != save || !save_shape ||
      u32at(save + 24) != g_info_global || u8at(close + 17) != g_step ||
      u8at(close + 89) != g_step || u8at(sw + 2) != g_step || u8at(close + 25) != g_quit ||
      u32at(close + 31) != g_timer || u32at(close + 62) != g_timer || u32at(close + 81) != g_timer || g_out < 1 ||
      handler(g_out) != close + 37 || handler(g_out + 1) != close + 93 || handler(g_main) < frame ||
      handler(g_main) > close || g_message + 4 > box->size || g_join1 >= box->size || g_join2 >= box->size ||
      g_timer + 4 > box->size || g_quit >= box->size)
    return off("Item Management's own quit and close (they disagree)");
  g_save = reinterpret_cast<SaveFn>(save);
  return true;
}

// The screen's working copy of a character's case (thiscall-free: (character, entry,
// the partner's)): the save data in sInfoMan's profile - cGameInfo's (the profile
// index and its size as discovery has them) - the entry (24 a character: (character x
// 3) x 8), its size (a cItem's data from its id on), the partner's copy and its own, a
// block of the save data apart. That block is the save data's working block: cGameInfo's
// commit copies it over its checkpoint copy (the save data's own copy of it: the size, the
// block), and its take-back - the play flow's quit among others - copies it back from
// there (the same size and block).
bool discover_cases() {
  const game::Layout& l = game::layout();
  const uintptr_t at = game::sig_one("kSigBoxCases");
  const uintptr_t commit = game::sig_one("kSigCasesCommit");
  const uintptr_t restore = game::sig_one("kSigCasesRestore");
  if (!at || !commit || !restore) return off("Item Management's working copy of the cases was not found");
  constexpr int kEntries = 24;
  const uint32_t save = u32at(at + 22) - l.game_base, entry = u8at(at + 44);
  const uint32_t own = save + u32at(at + 71), partner = save + u32at(at + 48);
  const uint32_t cases = static_cast<uint32_t>(game::kMaxChars * kEntries) * entry;
  const uintptr_t put = game::call_target(commit + 32), take = game::call_target(restore + 17);
  if (!image::text().contains(put) || !image::text().contains(take) ||
      !mem::matches(put, mem::parse_pattern("8B 44 24 04 68 ?? ?? ?? ?? 81 C1 ?? ?? ?? ?? 51 50 E8 ?? ?? ?? ?? 83 C4 0C "
                                            "C2 04 00")) ||
      !mem::matches(take, mem::parse_pattern("8B 44 24 04 53 55 56 8B F1 68 ?? ?? ?? ?? 50 8D AE ?? ?? ?? ?? 55 E8")))
    return off("the save data's checkpoint copy of its working block was not found");
  g_block = save + u32at(put + 11);
  g_block_size = u32at(put + 5);
  g_block_copy = u32at(commit + 21);
  g_info_global = u32at(at + 1);  // the screen's save reads it too (discover_screen)
  if (u8at(at + 7) != l.profile_index || u32at(at + 10) != l.game_size || u32at(at + 22) <= l.game_base ||
      entry != l.item_size - l.it_id || u8at(at + 67) != entry || l.nitems != kEntries ||
      u32at(commit + 28) != save || u32at(restore + 10) != save ||
      u32at(restore + 4) != g_block_copy || u32at(take + 10) != g_block_size || u32at(take + 17) != g_block - save ||
      own < g_block || own + cases > g_block + g_block_size || partner != own + g_block_size ||
      g_block_copy < g_block + 2 * g_block_size || g_block_copy + g_block_size > l.game_size ||
      g_block < save + g_block_size)
    return off("Item Management's working copy of the cases and its checkpoint copy (they disagree)");
  g_state = g_block - g_block_size;
  return true;
}

// The play flow's copy of the profile's cGameInfo as at its last checkpoint (made at the
// area's start, copied over at each checkpoint): its field, made a cGameInfo (the size)
// and copied from the profile's (sInfoMan's, as discovery has them) by a copy whose last
// part is the save data's: the saved state and the working block (then the partner's),
// each one block of the save data - where discover_cases has them.
bool discover_snapshot() {
  const game::Layout& l = game::layout();
  const uintptr_t snap = game::sig_one("kSigFlowSnapshot");
  g_cls_info = mt::find("cGameInfo");
  if (!snap || !g_cls_info) return no_saves("the play flow's copy of the save data was not found");
  g_snapshot = u32at(snap + 2);
  const uintptr_t copy = game::call_target(snap + 72);
  const uintptr_t tail = image::text().contains(copy)
                             ? mem::find_pattern(mem::Range{copy, copy + 0x100}, "81 C5 ?? ?? ?? ?? 55 8D 8B ?? ?? ?? ?? E8")
                             : 0;
  const uintptr_t blocks = tail ? game::call_target(tail + 13) : 0;
  if (!blocks || !image::text().contains(blocks) ||
      !mem::matches(blocks, mem::parse_pattern("56 57 8B 7C 24 0C 8B F1 68 ?? ?? ?? ?? 8D 46 ?? 50 8D 4F ?? 51 E8 ?? ?? "
                                               "?? ?? 68 ?? ?? ?? ?? 8D 96 ?? ?? ?? ?? 52 8D 87 ?? ?? ?? ?? 50 E8")))
    return no_saves("the play flow's copy of the save data (its copy was not understood)");
  const uint32_t save = u32at(tail + 2);
  if (u32at(snap + 39) != g_snapshot || u32at(snap + 54) != g_snapshot || u32at(snap + 10) != l.game_size ||
      u32at(snap + 60) != l.game_size || g_cls_info->size != l.game_size || u8at(snap + 51) != l.profile_index ||
      u32at(snap + 68) != l.game_base || u32at(tail + 9) != save || u32at(blocks + 9) != g_block_size ||
      u32at(blocks + 27) != g_block_size || save + s8at(blocks + 15) != g_state || s8at(blocks + 19) != s8at(blocks + 15) ||
      save + u32at(blocks + 33) != g_block || u32at(blocks + 40) != u32at(blocks + 33) ||
      g_snapshot + 4 > g_cls_flow->size)
    return no_saves("the play flow's copy of the save data (it disagrees)");
  return true;
}

// The pause menu's QUIT (uIdPause00's): its panes, walked by the menu's run (the pause's
// frame, its vtable slot 6, calls it) - a pane in use (its first byte) and not done runs
// its option: 0 its menu, RESUME, RESTART and QUIT their handler; the cancel byte that
// keeps a pane's cancel from ending it. The handler's QUIT once asked: a step waiting for
// the save manager, then one asking the save question, then the play flow told to quit
// (its step made the quit's, kSigFlowQuit's); the question's (its own function's) step
// that takes the answer - "no": the profile taken back from its checkpoint copies, the
// take-back the play flow's quit makes (cSystemInfo's return to its copy, kSigCheckpoint,
// then cGameInfo's, kSigCasesRestore's) - and what the handler sets as QUIT is answered yes.
bool discover_pause() {
  const mt::Class* pause = mt::find("uIdPause00");
  const uintptr_t panes = game::sig_one("kSigPausePanes"), cancel = game::sig_one("kSigPauseCancel");
  const uintptr_t option = game::sig_one("kSigPauseOption"), quit = game::sig_one("kSigPauseQuit");
  const uintptr_t leaving = game::sig_one("kSigPauseLeaving"), no = game::sig_one("kSigPauseNoSave");
  const uintptr_t back = game::sig_one("kSigCheckpoint"), restore = game::sig_one("kSigCasesRestore");
  const uintptr_t flow_quit = game::sig_one("kSigFlowQuit");
  const uintptr_t frame = pause && pause->vtable ? mem::read<uintptr_t>(pause->vtable + 4 * 6) : 0;
  if (!panes || !cancel || !option || !quit || !leaving || !no || !back || !restore || !flow_quit ||
      !image::text().contains(frame))
    return no_pause("the pause menu's QUIT was not found");
  // The run: called from the pause's frame, the panes' walk in it.
  bool run = false;
  for (uintptr_t p = frame; p < frame + 0x90 && !run; ++p) {
    const uintptr_t t = u8at(p) == 0xE8 ? game::call_target(p) : 0;
    run = t && panes > t && panes < t + 0x200;
  }
  // The panes: the walk's start, the pane that far back from it, its done byte; the walk's
  // end (one more pane, the step, as many) after the options.
  const int walk = s8at(panes + 15);
  const uintptr_t end = mem::find_pattern(mem::Range{panes, panes + 0x400}, "83 C0 01 83 C6 ?? 83 F8 ?? 89 44 24 10 0F 8C");
  if (!run || !end || walk >= 0 || option < panes || option > end || cancel < panes || cancel > option)
    return no_pause("the pause menu's panes (not in its run)");
  g_panes = u32at(panes + 2) + static_cast<uint32_t>(walk);
  g_pane_done = static_cast<uint32_t>(s8at(panes + 34) - walk);
  g_pane_step = u8at(end + 5);
  g_npanes = u8at(end + 8);
  g_option = static_cast<uint32_t>(s8at(option + 2) - walk);
  g_no_cancel = u8at(cancel + 2);
  // The options: 0 the menu, 1 RESUME, RESTART and QUIT the handler's stub.
  const uint32_t last = u8at(option + 5);
  const uintptr_t table = u32at(option + 15), handler = game::call_target(option + 44);
  if (jump_case(table, 0) != option + 19 || jump_case(table, 1) != option + 32 || !image::text().contains(handler) ||
      static_cast<uint32_t>(s8at(cancel + 28) - walk) != g_option)
    return no_pause("the pause menu's options (they disagree)");
  // The handler: the other pane by this one's number, the panes' place and step as the run
  // has them; its step and the steps' tables.
  if (!mem::matches(handler, mem::parse_pattern("55 56 8B 74 24 0C 8B 46 ?? 83 F0 01 6B C0 ?? 8B E9 57 8D 7C 28 ?? 8A 46 "
                                                "?? 0F B6 C8 83 F9 ?? 0F 87 ?? ?? ?? ?? 0F B6 89 ?? ?? ?? ?? 53 FF 24 8D")) ||
      u8at(handler + 14) != g_pane_step || u8at(handler + 21) != g_panes || quit < handler || quit > handler + 0x600 ||
      leaving < handler || leaving > quit)
    return no_pause("the pause menu's RESTART and QUIT (their handler)");
  g_pane_at = u8at(handler + 24);
  const uint32_t steps = u8at(handler + 30) + 1;
  const uintptr_t t1 = u32at(handler + 40), t2 = u32at(handler + 48);
  auto step_case = [&](uint32_t s) { return jump_case(t2, u8at(t1 + s)); };
  // QUIT: the option after RESTART's; its steps by where they go.
  const uint32_t restart = u8at(quit + 82);
  g_quit_option = restart + u8at(quit + 87);
  g_quit_out = u8at(quit + 56);
  int waits = 0, asks = 0;
  for (uint32_t s = 0; s < steps && s < 0x100; ++s) {
    if (step_case(s) == quit) g_quit_wait = s, ++waits;
    if (step_case(s) == quit + 31) g_quit_ask = s, ++asks;
  }
  if (waits != 1 || asks != 1 || g_quit_ask != g_quit_wait + 1 || g_quit_out >= steps ||
      step_case(g_quit_out) != quit + 62 || g_quit_option > last || jump_case(table, g_quit_option) != option + 41 ||
      jump_case(table, restart) != option + 41 || u8at(quit + 25) != g_pane_at || u8at(quit + 55) != g_pane_at ||
      u8at(quit + 79) != g_option)
    return no_pause("the pause menu's QUIT (its steps disagree)");
  // ... the play flow told to quit: its getter (as the handler's yes calls it too), then its
  // quit making the flow's step the quit's.
  const uintptr_t flow = game::call_target(quit + 115), flow_out = game::call_target(quit + 156);
  g_flow_step = u8at(flow_quit + 38);
  g_flow_out = u8at(flow_quit + 46);
  if (!flow || game::call_target(quit + 149) != flow || game::call_target(leaving + 13) != flow ||
      game::call_target(leaving + 28) != flow || !image::text().contains(flow_out) ||
      !mem::matches(flow_out, mem::parse_pattern("B0 ?? 38 41 ?? 74 ?? C7 41 ?? 00 00 00 00 88 41 ?? C3")) ||
      u8at(flow_out + 1) != g_flow_out || u8at(flow_out + 4) != g_flow_step || u8at(flow_out + 9) != g_flow_step ||
      u8at(flow_out + 16) != g_flow_step)
    return no_pause("the pause menu's QUIT (the play flow's quit disagrees)");
  g_pause_leaving = u32at(leaving + 2);
  g_flow_leaving = u32at(leaving + 35);
  // The question: its function (the handler's step asking it), its step and table; the
  // step taking the answer, and the "no": unless RESTART, the take-back.
  const uintptr_t ask = game::call_target(quit + 34);
  if (!image::text().contains(ask) ||
      !mem::matches(ask, mem::parse_pattern("53 56 8B 74 24 0C 8A 46 ?? 57 8B F9 0F B6 C8 83 F9 ?? 0F 87 ?? ?? ?? ?? BB "
                                            "01 00 00 00 FF 24 8D")) ||
      no < ask || no > ask + 0x300)
    return no_pause("the pause's save question was not understood");
  g_ask_at = u8at(ask + 8);
  g_ask_timer = u8at(no + 10);
  g_ask_answer = u8at(no + 31);
  int answers = 0;
  for (uint32_t s = 0; s <= u8at(ask + 17); ++s)
    if (jump_case(u32at(ask + 32), s) == no) g_ask_step = s, ++answers;
  // ... asked of a player who may save (its first step's check: the pane's player, then the
  // save manager's slot of that player - none past the last: "cannot").
  const uintptr_t first_step = jump_case(u32at(ask + 32), 0);
  const uintptr_t may = image::text().contains(first_step) &&
                                mem::matches(first_step, mem::parse_pattern("56 8B CF E8 ?? ?? ?? ?? 84 C0 0F 84"))
                            ? game::call_target(first_step + 3)
                            : 0;
  const uintptr_t slot = image::text().contains(may) ? game::call_target(may + 19) : 0;
  if (!image::text().contains(slot) ||
      !mem::matches(may, mem::parse_pattern("56 8B 74 24 08 8B 46 ?? 85 C0 7C ?? 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 84 C0 "
                                            "75 ?? 8B 46 ??")) ||
      !mem::matches(slot, mem::parse_pattern("8B 44 24 04 83 F8 ?? 77 ?? C1 E0 ?? 8D 44 08 ?? 85 C0 75 ?? B0 01 C2 04 00 8A "
                                             "40 ?? C2 04 00")) ||
      u8at(may + 30) != g_option || u8at(slot + 11) > 8)
    return no_pause("the pause's save question (who may save was not understood)");
  g_pane_player = u8at(may + 7);
  g_save_man = u32at(may + 14);
  g_save_last = u8at(slot + 6);
  g_save_slot = 1u << u8at(slot + 11);
  g_save_slots = u8at(slot + 15);
  g_save_cannot = u8at(slot + 27);
  // QUIT's bit of sBioMain's flags (pushed to their setter): the setter's lock and field.
  const uintptr_t setter = game::call_target(quit + 138);
  if (!image::text().contains(setter) ||
      !mem::matches(setter, mem::parse_pattern("56 8B F1 80 7E ?? 00 75 ?? 80 3D ?? ?? ?? ?? 00 74 ?? 8D 46 ?? 50 FF 15 ?? ?? "
                                               "?? ?? 8B 4C 24 08 09 8E ?? ?? ?? ?? 80 7E ?? 00")) ||
      u8at(setter + 40) != u8at(setter + 5) || !mem::readable(u32at(setter + 11), 1))
    return no_pause("sBioMain's flags' setter was not understood");
  g_quit_flag = u32at(quit + 134);
  g_main_mt = u8at(setter + 5);
  g_mt_global = u32at(setter + 11);
  g_main_lock = u8at(setter + 20);
  g_main_flags = u32at(setter + 34);
  const uintptr_t take = game::call_target(no + 82);
  const uintptr_t first = image::text().contains(take) ? game::call_target(take + 16) : 0;
  const uintptr_t second = image::text().contains(take) ? game::call_target(take + 37) : 0;
  const game::Layout& l = game::layout();
  if (answers != 1 || u8at(no + 40) != g_ask_at || u8at(no + 26) != g_ask_timer || u8at(no + 57) != g_option ||
      u8at(no + 58) != restart ||
      !mem::matches(take, mem::parse_pattern("56 8B F1 8B 46 ?? 69 C0 ?? ?? ?? ?? 8D 4C 30 ?? E8 ?? ?? ?? ?? 8B 4E ?? 69 "
                                             "C9 ?? ?? ?? ?? 8D 8C 31 ?? ?? ?? ?? E8")) ||
      u8at(take + 5) != l.profile_index || u32at(take + 8) != l.system_size || u8at(take + 15) != l.system_base ||
      first != back || u8at(take + 23) != l.profile_index || u32at(take + 26) != l.game_size ||
      u32at(take + 33) != l.game_base || !second || restore < second || restore > second + 0x100)
    return no_pause("the pause's save question (its no disagrees)");
  // The fields fit their classes.
  const mt::Class* main = mt::find("sBioMain");
  if (!main || !g_quit_flag || (g_quit_flag & (g_quit_flag - 1)) || g_main_flags + 4 > main->size ||
      g_main_lock + sizeof(CRITICAL_SECTION) > main->size || g_main_mt >= main->size || g_pane_player + 4 > g_pane_step ||
      !g_npanes || g_npanes > 8 || g_pane_step < g_ask_answer + 4 || g_panes + g_npanes * g_pane_step > pause->size ||
      g_pane_done >= g_pane_step || g_option + 4 > g_pane_step || g_pane_at >= g_pane_step || g_ask_at >= g_pane_step ||
      g_ask_timer + 4 > g_pane_step || g_no_cancel >= pause->size || g_pause_leaving >= pause->size ||
      g_flow_leaving >= g_cls_flow->size || g_flow_step >= g_cls_flow->size)
    return no_pause("the pause menu's fields (they do not fit)");
  return true;
}

// The start-up menu (cDemoStartUp, aDemo's): its frame's steps, one of them taking
// the return reason the game left in cGameSys (its step 0) - the reason a game's start
// leaves (cleared, then the menus from the top) and the one back to the choice made
// (the menus' music, the story flow at its choice, CHAPTER SELECT's own step past its
// chapter list - the mod sets that one 0) - then going on by the top menu's choice to
// the story flow, which goes by the story menu's (CHAPTER SELECT the one whose own
// steps make the chapter list, uIdStartUp05; past the last, it is parked: the menus
// from the top).
bool discover_menus(const mt::Class* main) {
  const mt::Class* list = mt::find("uIdStartUp05");
  const uintptr_t step = game::sig_one("kSigMenuStep");
  const uintptr_t reason = game::sig_one("kSigMenuReason");
  const uintptr_t back = game::sig_one("kSigMenuReasonBack");
  const uintptr_t top = game::sig_one("kSigMenuTop");
  const uintptr_t story = game::sig_one("kSigMenuStory");
  const uintptr_t frame = mem::read<uintptr_t>(g_cls_startup->vtable + 4 * 5);
  if (!list || !step || !reason || !back || !top || !story || step < frame || step > frame + 0x200)
    return off("the start-up menu's steps were not found");

  // The step that takes the reason: a stub calling the function whose step 0 is the
  // reason's switch, and whose step after the back reason runs the top menu's choice.
  const mem::Pattern stub = mem::parse_pattern("8B CF E8 ?? ?? ?? ??");
  const mem::Pattern reason_start =
      mem::parse_pattern("56 8B F1 8A 4E ?? 0F B6 C1 83 F8 ?? 0F 87 ?? ?? ?? ?? 0F B6 80 ?? ?? ?? ?? 53 33 DB FF 24 85");
  g_menu_step = u8at(step + 2);
  const uint32_t steps = u8at(step + 10) + 1;
  const uintptr_t step_table = u32at(step + 16);
  uintptr_t fn = 0;
  int found = 0;
  for (uint32_t k = 0; k < steps && k < 16; ++k) {
    const uintptr_t s = jump_case(step_table, k);
    const uintptr_t f = image::text().contains(s) && mem::matches(s, stub) ? game::call_target(s + 2) : 0;
    if (f && reason > f && reason < f + 0x40 && mem::matches(f, reason_start)) {
      fn = f;
      g_menu_reason_step = k;
      ++found;
    }
  }
  if (found != 1) return off("the start-up menu's step that takes the return reason");
  g_sub = u8at(fn + 5);
  g_sub_back = u8at(back + 20);
  const uint32_t subs = u8at(fn + 11) + 1;
  const uintptr_t sub1 = u32at(fn + 21), sub2 = u32at(fn + 31);
  auto sub_handler = [&](uint32_t s) { return s < subs ? jump_case(sub2, u8at(sub1 + s)) : 0; };

  // The reasons: one each, from 1. The normal one only clears it and goes on as an
  // unknown one does; the back one is kSigMenuReasonBack's, which plays the music the
  // normal one plays.
  g_sys = u32at(reason + 8);
  g_reason = u8at(reason + 14);
  const uint32_t reasons = u8at(reason + 20) + 1;
  const uintptr_t reason_table = u32at(reason + 26);
  const uintptr_t fallback = reason + 23 + static_cast<int8_t>(u8at(reason + 22));
  g_reason_normal = g_reason_back = 0;
  for (uint32_t k = 0; k < reasons && k < 32; ++k) {
    const uintptr_t h = jump_case(reason_table, k);
    if (h == back) g_reason_back = k + 1;
    if (h + 3 == fallback && u8at(h) == 0x89 && u8at(h + 1) == 0x58 && u8at(h + 2) == g_reason) g_reason_normal = k + 1;
  }
  if (!g_reason_normal || !g_reason_back || sub_handler(0) != reason || sub_handler(g_sub_back) != top ||
      u8at(back + 4) != g_reason || u8at(back + 19) != g_sub || jmp_target(back + 72) != top ||
      game::call_target(back + 67) != game::call_target(fallback + 27) || !game::call_target(back + 67))
    return off("the start-up menu's return reasons (they disagree)");

  // The top menu's choice: the stub of the story's goes to the story flow, whose step
  // after the back reason is kSigMenuStory's choice; the choice's default parks it.
  const mem::Pattern top_stub = mem::parse_pattern("5B 8B CE 5E E9 ?? ?? ?? ??");
  const mem::Pattern story_start = mem::parse_pattern(
      "56 8B F1 0F B6 46 ?? B9 ?? ?? ?? ?? 3B C1 0F 87 ?? ?? ?? ?? 0F B6 80 ?? ?? ?? ?? FF 24 85 ?? ?? ?? ??");
  g_top = u8at(top + 14);
  const uint32_t tops = u8at(top + 17) + 1;
  const uintptr_t top_table = u32at(top + 27);
  uintptr_t flow = 0;
  found = 0;
  for (uint32_t k = 0; k < tops && k < 16; ++k) {
    const uintptr_t s = jump_case(top_table, k);
    const uintptr_t f = image::text().contains(s) && mem::matches(s, top_stub) ? jmp_target(s + 4) : 0;
    if (f && story > f && story < f + 0x200 && mem::matches(f, story_start)) {
      flow = f;
      g_top_story = k;
      ++found;
    }
  }
  if (found != 1) return off("the start-up menu's story flow");
  g_story_step = u8at(flow + 6);
  g_story_parked = u32at(flow + 8);
  g_story_pick = u8at(back + 24);
  const uintptr_t st1 = u32at(flow + 23), st2 = u32at(flow + 30);
  const uintptr_t parked = story + 77 + static_cast<int8_t>(u8at(story + 76));
  if (g_story_parked > 0xFF || jump_case(st2, u8at(st1 + g_story_pick)) != story ||
      jump_case(st2, u8at(st1 + g_story_parked)) == story || u8at(back + 23) != g_story_step ||
      parked != story + 42 || u8at(story + 44) != g_story_step)
    return off("the start-up menu's story flow (it disagrees)");

  // The story menu's choice: CHAPTER SELECT's own steps - its setup, then (its step 1)
  // the chapter list, uIdStartUp05, made by its record.
  g_story = u8at(story + 71);
  g_story_park = u8at(story + 74) + 1;
  const uintptr_t story_table = u32at(story + 80);
  const mem::Pattern pick_stub = mem::parse_pattern("8B CE 5E E9 ?? ?? ?? ??");
  const mem::Pattern cs_start =
      mem::parse_pattern("56 8B F1 8A 46 ?? 0F B6 C8 83 F9 ?? 0F 87 ?? ?? ?? ?? 53 33 DB FF 24 8D ?? ?? ?? ??");
  const mem::Pattern call_this = mem::parse_pattern("8B CE E8 ?? ?? ?? ??");
  uint8_t rec[5] = {0xB9};
  std::memcpy(rec + 1, &list->dti, 4);
  found = 0;
  for (uint32_t k = 0; k < g_story_park && k < 32; ++k) {
    const uintptr_t s = jump_case(story_table, k);
    const uintptr_t f = image::text().contains(s) && mem::matches(s, pick_stub) ? jmp_target(s + 3) : 0;
    if (!f || !mem::matches(f, cs_start)) continue;
    const uintptr_t cs_table = u32at(f + 24);
    const uintptr_t setup = jump_case(cs_table, 0), first = jump_case(cs_table, 1);
    const uintptr_t make = image::text().contains(first) && mem::matches(first, call_this) ? game::call_target(first + 2) : 0;
    if (!make || setup >= first || !image::text().contains(make)) continue;
    bool makes_list = false;
    for (uintptr_t p = make; p < make + 0x80 && !makes_list; ++p)
      makes_list = !std::memcmp(reinterpret_cast<const void*>(p), rec, 5) && u8at(p + 5) == 0xE8;
    if (!makes_list) continue;
    g_chapter_select = k;
    g_cs_step = u8at(f + 5);
    ++found;
  }
  if (found != 1) return off("the story menu's CHAPTER SELECT");

  // The fields agree, and fit their classes.
  g_online = u32at(back + 12);
  if (u32at(top + 8) != g_sys || u32at(story + 16) != g_sys || u32at(story + 65) != g_sys ||
      u8at(story + 22) != g_story || u32at(story + 7) != g_online || u8at(back + 29) != g_cs_step ||
      u8at(back + 35) != g_cs_step || g_sys + 4 > main->size || g_online + 4 > main->size || g_mode_sys != g_sys ||
      g_mode + 4 > g_cls_sys->size || g_mode == g_reason || g_mode == g_top || g_mode == g_story ||
      g_reason + 4 > g_cls_sys->size || g_story + 4 > g_cls_sys->size || g_top + 4 > g_cls_sys->size ||
      g_menu_step >= g_cls_startup->size || g_sub >= g_cls_startup->size || g_story_step >= g_cls_startup->size ||
      g_cs_step >= g_cls_startup->size)
    return off("the start-up menu's fields (they disagree)");
  return true;
}

// The menus' own load: aDemo's frame (the slot sArea calls each area's frame through) -
// its first step goes on to the one that has the loader choose the front end's archives
// (its chooser, whose first test is of the same field), then to one waiting for the
// loader; an area's "started" byte as sArea's frame tests and sets it. The chooser asks
// for the menus' archive by a bit of that field, the trial's in place of it when
// sBioMain's flags have the bit the mod reads as online - so offline, the path pushed
// past them, one of the loader's archive folder.
bool discover_front(uintptr_t area_frame) {
  const uintptr_t load = game::sig_one("kSigFrontLoad");
  const uintptr_t menus = game::sig_one("kSigMenusArchive");
  const mt::Class* lm = mt::find("sLoadMan");
  const uintptr_t frame = mem::read<uintptr_t>(g_cls_front->vtable + u8at(area_frame + 62));
  if (!load || !menus || !lm || !inventory::lists_known() || !image::text().contains(frame) || load < frame ||
      load > frame + 0x100)
    return off("the menus' own load of their archives was not found");
  g_front_step = u8at(load + 2);
  g_front_load = u8at(load + 23);
  g_started = u8at(area_frame + 24);
  g_front_mask = u32at(menus + 2);
  g_menus_bit = u8at(menus + 6);
  const uint32_t steps = u8at(load + 10) + 1, after = u8at(load + 40);
  const uintptr_t table = u32at(load + 16), chooser = game::call_target(load + 32);
  const uintptr_t request = game::call_target(menus + 72), path = u32at(menus + 68);
  const char* p = image::rdata().contains(path) && mem::readable(path, 64) ? reinterpret_cast<const char*>(path) : nullptr;
  const size_t len = p ? strnlen(p, 64) : 0;
  const std::string& dir = inventory::archive_dir();
  if (u8at(load + 22) != g_front_step || u8at(load + 39) != g_front_step || g_front_load >= steps || after >= steps ||
      jump_case(table, 0) != load + 20 || jump_case(table, g_front_load) != load + 26 ||
      jump_case(table, after) != load + 43 || u32at(load + 28) != u32at(load + 45) || !image::text().contains(chooser) ||
      !mem::matches(chooser, mem::parse_pattern("56 6A 01 8B F1 E8 ?? ?? ?? ?? F6 86")) ||
      u32at(chooser + 12) != g_front_mask || menus < chooser || menus > chooser + 0x100 ||
      !image::text().contains(request) || game::call_target(menus + 35) != request || u32at(menus + 47) != g_online ||
      u8at(menus + 51) != 1 || u8at(menus + 59) != 7 || !g_menus_bit || (g_menus_bit & (g_menus_bit - 1)) ||
      g_front_mask + 4 > lm->size || g_front_step >= g_cls_front->size || g_started >= g_cls_front->size ||
      u8at(area_frame + 54) != g_started || !len || len >= 64 || dir.empty() ||
      std::strncmp(p, dir.c_str(), dir.size()) != 0)
    return off("the menus' own load of their archives (it disagrees)");
  g_menus_path = p;
  return true;
}

// The start-up menu's background: what the main menu's choice of the story does to it
// (its own setter of that, telling its list and its header by theirs - the story the
// top menu's choice discovery has), and the segment its change of screens asks its panel
// for from the story menu (its own screen, offline) to CHAPTER SELECT's (the story menu's
// table of its choices' screens, which its close hands that change), each read out of the
// code that does it. Not understood: the way back leaves the background as it is.
bool discover_look() {
  const uintptr_t chosen = game::sig_one("kSigMenuChosen");
  const uintptr_t change = game::sig_one("kSigMenuTransition");
  const uintptr_t close = game::sig_one("kSigStoryClose");
  const mt::Class* story = mt::find("uIdStartUp01");
  g_cls_top = mt::find("uIdTopMenu00");
  g_cls_list = mt::find("uIdStartUp11");
  g_cls_header = mt::find("uIdStartUp14");
  const uintptr_t frame = story && story->vtable ? mem::read<uintptr_t>(story->vtable + 4 * 6) : 0;
  if (!chosen || !change || !close || !g_cls_top || !g_cls_list || !g_cls_header || !image::text().contains(frame) ||
      close < frame || close > frame + 0x800)
    return false;
  const uintptr_t list_set = game::call_target(chosen + 15), header_set = jmp_target(chosen + 54);
  const uintptr_t step = game::call_target(close + 81);
  if (!image::text().contains(list_set) || !image::text().contains(header_set) || !image::text().contains(step) ||
      !mem::matches(list_set, mem::parse_pattern("8A 44 24 04 B2 01 84 C2 74 ?? 88 91 ?? ?? ?? ?? C6 81 ?? ?? ?? ?? 00 "
                                                 "A8 02 74 ?? 88 91 ?? ?? ?? ?? C2 04 00")) ||
      !mem::matches(header_set, mem::parse_pattern("8A 44 24 04 88 81 ?? ?? ?? ?? C2 04 00")) ||
      !mem::matches(step, mem::parse_pattern("8B 89 ?? ?? ?? ?? 85 C9 74 ?? E9")) || jmp_target(step + 10) != change)
    return false;
  g_list = u8at(chosen + 5);
  g_header = u8at(chosen + 40);
  g_picked = u32at(list_set + 12);
  g_go_on = u32at(list_set + 18);
  g_shown = u32at(header_set + 6);
  g_shown_value = u32at(chosen + 50);
  g_request = u8at(change + 58);
  g_segment = u8at(change + 66);
  g_backward = u8at(change + 69);
  const uintptr_t rows = u32at(change + 12), screens = u32at(close + 39);
  const uint32_t nrows = u8at(change + 41), own = u32at(close + 13);
  if (u32at(chosen + 28) != g_sys || u8at(chosen + 34) != g_top || u8at(chosen + 35) != g_top_story ||
      u32at(list_set + 29) != g_go_on || u32at(close + 8) != g_online || !g_shown_value || g_shown_value > 0xFF ||
      !nrows || !mem::readable(rows, 3 * nrows) || !mem::readable(screens + g_chapter_select, 1) || own > 0x7F ||
      g_list + 4 > g_cls_top->size || g_header + 4 > g_cls_top->size || g_request >= g_cls_top->size ||
      g_segment + 4 > g_cls_top->size || g_backward >= g_cls_top->size || g_picked >= g_cls_list->size ||
      g_go_on >= g_cls_list->size || g_shown >= g_cls_header->size)
    return false;
  // The panel's segment of that change: its row (none, or none asked for: the panel left as it is).
  const int8_t to = mem::read<int8_t>(screens + g_chapter_select);
  g_panel = -1;
  for (uint32_t r = 0; r < nrows; ++r)
    if (mem::read<int8_t>(rows + 3 * r) == static_cast<int8_t>(own) && mem::read<int8_t>(rows + 3 * r + 1) == to) {
      g_panel = mem::read<int8_t>(rows + 3 * r + 2);
      break;
    }
  return true;
}

// The Game Over screen (uIdGameover00's frame, offline): once its opening is over it is shown
// and goes on to its menu - or, with a network partner, to the step that waits for the choice
// made elsewhere, the choice none - and that step takes the choice from sBioMain's flags:
// CONTINUE's bit 0, QUIT's 1 (QUIT's the pause's), then decided, its delay, the next step. The
// play flow's game over then ends the game as the pause's QUIT does (the game back to the
// last checkpoint first, then the profile taken back and saved).
bool discover_over() {
  g_cls_over = mt::find("uIdGameover00");
  const uintptr_t choice = game::sig_one("kSigGameOverChoice"), decided = game::sig_one("kSigGameOverDecided");
  const uintptr_t frame = g_cls_over && g_cls_over->vtable ? mem::read<uintptr_t>(g_cls_over->vtable + 4 * 6) : 0;
  if (!choice || !decided || !image::text().contains(frame) || choice < frame || choice > frame + 0x500 ||
      decided < frame || decided > frame + 0x500)
    return no_over("the Game Over screen's menu was not found");
  g_over_shown = u32at(choice + 2);
  g_over_step = u8at(choice + 33);
  g_over_elsewhere = u8at(choice + 42);
  g_over_choice = u32at(choice + 45);
  g_over_decided = u32at(decided + 83);
  if (u32at(choice + 7) != u32at(decided + 2) || u8at(choice + 41) != g_over_step || u8at(decided + 80) != g_over_step ||
      u32at(decided + 21) != g_over_choice || u32at(decided + 40) != g_over_choice ||
      u32at(decided + 46) != g_over_choice || u32at(decided + 8) != g_main_flags || u32at(decided + 32) != g_quit_flag ||
      u32at(choice + 13) != g_sys || g_over_shown >= g_cls_over->size || g_over_step >= g_cls_over->size ||
      g_over_choice + 4 > g_cls_over->size || g_over_decided >= g_cls_over->size)
    return no_over("the Game Over screen's menu (it disagrees)");
  return true;
}

// The chapter's results (uIdChapEd00's frame): with their quit offered, a quit pressed at
// their menu asks first (the question's step); answered yes, the step after it (the
// session let go, the save manager waited for, done) with the quit set, its wait 0 and its
// timer the delay; answered no, back to the menu's step. Without it (the last chapter) their
// continue sets the quit itself. The play flow's chapter end, once they are done: their quit
// set, its step made the one after a game's end (as the Game Over's QUIT: the profile taken
// back from what the chapter's end saved, saved, the quit), else the next chapter.
bool discover_results() {
  g_cls_results = mt::find("uIdChapEd00");
  const uintptr_t ask = game::sig_one("kSigResultsAsk"), quit = game::sig_one("kSigResultsQuit");
  const uintptr_t done = game::sig_one("kSigResultsDone");
  const uintptr_t frame =
      g_cls_results && g_cls_results->vtable ? mem::read<uintptr_t>(g_cls_results->vtable + 4 * 6) : 0;
  if (!ask || !quit || !done || !image::text().contains(frame) || ask < frame || ask > frame + 0xC00 || quit < frame ||
      quit > frame + 0xC00)
    return no_results("the chapter's results' menu was not found");
  g_res_noquit = u32at(ask + 2);
  g_res_prompts = u32at(ask + 56);
  g_res_prompts_off = u32at(ask + 60);
  g_res_step = u8at(ask + 92);
  g_res_quitting = u8at(ask + 93) + 1u;  // the question's yes: its step one on
  g_res_menu = u8at(quit + 15);
  g_res_quit = u32at(quit + 58);
  g_res_wait = u32at(quit + 65);
  g_res_timer = u32at(quit + 73);
  g_flow_after = u8at(done + 25);
  if (u32at(ask + 82) != g_res_wait || u32at(ask + 72) != u32at(quit + 8) || u8at(quit + 14) != g_res_step ||
      u8at(quit + 54) != g_res_step || u32at(done + 8) != g_res_quit || u8at(done + 17) != g_flow_step ||
      u8at(done + 24) != g_flow_step || g_flow_after == g_flow_out || u32at(done + 2) + 4 > g_cls_flow->size ||
      g_res_menu == g_res_quitting || g_res_noquit >= g_cls_results->size || g_res_step >= g_cls_results->size ||
      g_res_quit >= g_cls_results->size || g_res_wait + 4 > g_cls_results->size ||
      g_res_timer + 4 > g_cls_results->size || g_res_prompts + 4 > g_cls_results->size)
    return no_results("the chapter's results' menu (it disagrees)");
  return true;
}

}  // namespace

void init() {
  if (g_lock_ready) return;
  InitializeCriticalSection(&g_lock);
  g_lock_ready = true;
}

bool discover() {
  const mt::Class* box = mt::find("uIdItemBox00");
  const mt::Class* main = mt::find("sBioMain");
  const mt::Class* areas = mt::find("sArea");
  g_cls_flow = mt::find("aGame");
  g_cls_front = mt::find("aDemo");
  g_cls_startup = mt::find("cDemoStartUp");
  g_cls_sys = mt::find("cGameSys");
  const uintptr_t area_frame = game::sig_one("kSigAreaFrame");
  if (!game::ready() || !box || !box->vtable || !main || !areas || !areas->vtable || !g_cls_flow ||
      !g_cls_flow->vtable || !g_cls_front || !g_cls_startup || !g_cls_startup->vtable || !g_cls_sys || !area_frame)
    return off("the play flow, Item Management or the start-up menu was not found");
  // sArea's areas: out of its own frame (its vtable slot 6).
  g_area_list = u32at(area_frame + 9);
  g_area_count = u8at(area_frame + 73);
  if (mem::read<uintptr_t>(areas->vtable + 4 * 6) != area_frame || !g_area_count ||
      g_area_list + 4 * g_area_count > areas->size)
    return off("sArea's areas (they disagree)");
  // The save data's blocks and the play flow's copy of them: the way back's, and where the
  // item box's moves at the pause are made (saves()).
  g_saves = discover_cases() && discover_snapshot();
  if (g_saves)
    logf("menus: the save data in cGameInfo: the saved state +0x%X, the working block +0x%X and its checkpoint copy "
         "+0x%X (0x%X bytes each), and the play flow's copy of the profile's cGameInfo as at its last checkpoint "
         "(aGame +0x%X) - the item box's moves at the pause are made in each",
         g_state, g_block, g_block_copy, g_block_size, g_snapshot);
  else
    logf("menus: the save data's blocks were not understood - the item box makes no moves at the pause");
  if (!g_saves || !discover_flow(box) || !discover_screen(box) || !discover_menus(main) || !discover_front(area_frame))
    return false;
  g_found = true;
  logf("menus: Chapter Select beside Item Management: the play flow's screen +0x%X (aGame; its quit from the "
       "screen's +0x%X); the screen's save exe+0x%X, step +0x%X (%u its panes, %u its fade out), message +0x%X, a "
       "partner's flags +0x%X +0x%X, timer +0x%X; the save data's working block cGameInfo +0x%X (0x%X bytes) and its "
       "checkpoint copy +0x%X; cGameSys (sBioMain +0x%X, online flags +0x%X): the game's mode +0x%X (0 the story: "
       "Chapter Select in no other), the menus' return reason +0x%X (%u normal, %u back to the choice), the top menu's "
       "choice +0x%X (%u the story), the story menu's +0x%X (%u CHAPTER SELECT, %u parked); cDemoStartUp: step +0x%X "
       "(%u takes the reason), then +0x%X (%u after the back reason), the story's +0x%X (%u the choice, %u parked), "
       "CHAPTER SELECT's +0x%X",
       g_flow_screen, g_quit, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_save) - image::base()), g_step,
       g_main, g_out, g_message, g_join1, g_join2, g_timer, g_block, g_block_size, g_block_copy, g_sys, g_online,
       g_mode, g_reason, g_reason_normal, g_reason_back, g_top, g_top_story, g_story, g_chapter_select, g_story_park,
       g_menu_step, g_menu_reason_step, g_sub, g_sub_back, g_story_step, g_story_pick, g_story_parked, g_cs_step);
  logf("menus: the menus' own load: aDemo's step +0x%X (%u has the loader choose the front end's archives, once its "
       "area's +0x%X is set) by sLoadMan +0x%X - the menus' bit 0x%X (%s) added before it, the start-up menu handed "
       "over only with that archive loaded",
       g_front_step, g_front_load, g_started, g_front_mask, g_menus_bit, g_menus_path);
  g_look = discover_look();
  if (g_look)
    logf("menus: the start-up menu's background (uIdTopMenu00) as the story's choice and CHAPTER SELECT's leave it: "
         "its list +0x%X (uIdStartUp11: picked +0x%X, go on +0x%X), its header +0x%X (uIdStartUp14: shown +0x%X), "
         "its panel's segment %d (asked +0x%X, number +0x%X, played back +0x%X) - made as it starts, the start-up "
         "menu handed over then",
         g_list, g_picked, g_go_on, g_header, g_shown, g_panel, g_request, g_segment, g_backward);
  else
    logf("menus: the start-up menu's background was not understood - the way back leaves it as a game's end has it "
         "(the main menu's look)");
  g_pause = discover_pause();
  g_over = g_pause && discover_over();  // its QUIT's bit is the pause's
  g_results = g_pause && discover_results();  // the play flow's steps as the pause's has them
  if (g_results)
    logf("menus: Chapter Select at the chapter's results (uIdChapEd00): their menu's step %u (+0x%X; quit offered "
         "while +0x%X is clear), then %u with their quit +0x%X set, its wait +0x%X 0, its timer +0x%X 0, the prompts "
         "+0x%X made %u; then the play flow's step %u (the quit after a game's end)",
         g_res_menu, g_res_step, g_res_noquit, g_res_quitting, g_res_quit, g_res_wait, g_res_timer, g_res_prompts,
         g_res_prompts_off, g_flow_after);
  if (g_over)
    logf("menus: Chapter Select at the Game Over screen (uIdGameover00): shown +0x%X, its step +0x%X (%u waits for the "
         "choice made elsewhere), the choice +0x%X (1 QUIT, by sBioMain +0x%X's bit 0x%X), decided +0x%X",
         g_over_shown, g_over_step, g_over_elsewhere, g_over_choice, g_main_flags, g_quit_flag, g_over_decided);
  if (g_pause)
    logf("menus: Chapter Select at the pause: the pause menu's panes +0x%X (0x%X each, %u; done +0x%X, option "
         "+0x%X, its step +0x%X, its player +0x%X; no cancel +0x%X) - QUIT (option %u) from its step %u (the save "
         "manager), then %u (the save question: its step +0x%X made %u, timer +0x%X, answer +0x%X made 0, yes - "
         "asked of a player the save manager [0x%X] lets save: slot +0x%X + 0x%X each up to %u, its +0x%X clear), "
         "then %u (sBioMain +0x%X |= 0x%X, the play flow's step +0x%X made %u); as it goes the menu's +0x%X and the "
         "play flow's +0x%X set",
         g_panes, g_pane_step, g_npanes, g_pane_done, g_option, g_pane_at, g_pane_player, g_no_cancel, g_quit_option,
         g_quit_wait, g_quit_ask, g_ask_at, g_ask_step, g_ask_timer, g_ask_answer, g_save_man, g_save_slots,
         g_save_slot, g_save_last, g_save_cannot, g_quit_out, g_main_flags, g_quit_flag, g_flow_step, g_flow_out,
         g_pause_leaving, g_flow_leaving);
  return true;
}

void tick(const game::Run& run) {
  if (!g_lock_ready) return;
  View v;
  v.found = g_found;
  EnterCriticalSection(&g_lock);
  std::snprintf(v.note, sizeof(v.note), "%s", g_view.note);
  LeaveCriticalSection(&g_lock);
  const bool asked = InterlockedExchange(&g_asked, 0) != 0;
  if (!g_found) {
    if (asked) note(v, "Chapter Select was not understood in this game (see RE5CabbyCodes.log).");
  } else {
    // Item Management between chapters (the play flow's screen), or the pause, the Game Over
    // screen or the results in a story chapter (the play flow's too) - in a story game only:
    // in another mode nothing is offered.
    const uintptr_t screen = run.item_screen, gi = game::game_info(), sys = game::system_info();
    const uint32_t data = profile_data();
    uintptr_t flow = screen || run.pause_menu || run.game_over || run.results ? area_of(g_cls_flow) : 0;
    if (flow && !story_game(run, screen)) flow = 0;
    const bool at_box = flow && screen && mem::read<uintptr_t>(flow + g_flow_screen) == screen;
    const uintptr_t pause = g_pause && flow && !screen && !run.mercenaries ? run.pause_menu : 0;
    const uintptr_t over = g_over && flow && !screen && !pause && !run.mercenaries ? run.game_over : 0;
    const uintptr_t results = g_results && flow && !screen && !pause && !over && !run.mercenaries ? run.results : 0;
    if (!at_box && !pause && !over && !results) flow = 0;
    const uintptr_t place = at_box ? screen : pause ? pause : over ? over : results;
    if (place != g_screen) {
      g_screen = place;
      g_end_logged = false;
      if (g_phase == Phase::kIdle) v.note[0] = '\0';
    }
    const uintptr_t gs = flow || g_phase != Phase::kIdle ? game_sys(run) : 0;
    const bool online = run.main && (u32at(run.main + g_online) & 1);
    v.offered = flow && g_phase == Phase::kIdle;
    v.where = !v.offered ? Where::kNone
              : at_box   ? Where::kItemBox
              : pause    ? Where::kPause
              : over     ? Where::kGameOver
                         : Where::kResults;
    uintptr_t pane = 0;
    if (v.offered) {
      const char* why = nullptr;
      if (pause) pane = pause_pane(pause);
      if (online) why = "Not in an online game.";
      else if (!gi || !sys || !data) why = "The profile in use was not found.";
      else if (at_box && (u8at(screen + g_step) != g_main || !itembox::screen_idle(screen)))
        why = "Wait for the screen to settle - go back to its menu, your inventory or the box.";
      else if (pause && !pane) why = "Not with two players on this machine.";
      else if (pause && u32at(pane + g_option) != 0) why = "Go back to the pause menu's first page.";
      else if (over && (!u8at(over + g_over_shown) || u8at(over + g_over_decided)))
        why = "Wait for the Game Over screen's choice.";
      else if (over && u8at(over + g_over_step) == g_over_elsewhere) why = "Not while a partner decides.";
      else if (results && u8at(results + g_res_step) != g_res_menu) why = "Wait for the results' menu.";
      // The last chapter's results offer no quit of their own: their continue is it (the
      // quit set, the game back to its menus), so Chapter Select goes from them as from any.
      v.last = results && u8at(results + g_res_noquit);
      v.ready = !why;
      if (why) std::snprintf(v.why, sizeof(v.why), "%s", why);
      if (asked) note(v, why ? why : "");
      if ((over || results) && (!g_end_logged || why != g_end_why)) {
        g_end_logged = true;
        g_end_why = why;
        log_end(over, results, gs, online, why);
      }
    }
    if (asked && v.ready && at_box) {
      // The screen's own quit, its save first: the panes' cases into the saved state, as
      // its close saves them. The play flow's quit then takes the profile and the save
      // data's working block back from their checkpoint copies, so those are brought to
      // what is kept first, as the game's commit after its own Item Management does: the
      // profile's data (the box, the gold, the treasures) and the working block (the
      // cases, put in order by the save).
      g_save(reinterpret_cast<void*>(screen));
      mem::write_bytes(sys + profile_start() + data, reinterpret_cast<const void*>(sys + profile_start()), data);
      mem::write_bytes(gi + g_block_copy, reinterpret_cast<const void*>(gi + g_block), g_block_size);
      mem::write<uint32_t>(screen + g_message, 0);
      mem::write<uint8_t>(screen + g_join1, 0);
      mem::write<uint8_t>(screen + g_join2, 0);
      mem::write<float>(screen + g_timer, 0.0f);
      mem::write<uint8_t>(screen + g_quit, 1);
      mem::write<uint8_t>(screen + g_step, static_cast<uint8_t>(g_out));
      g_phase = Phase::kLeaving;
      g_age = 0;
      g_from = Where::kItemBox;
      g_pane = 0;
      g_menus_asked = false;
      v.offered = v.ready = false;
      note(v, "");
      logf("menus: Chapter Select asked at Item Management - the screen's own save of the cases made, the profile's "
           "and the cases' checkpoint copies brought to what is kept, the screen closing with its quit");
    } else if (asked && v.ready && pause) {
      // The pause menu's QUIT, from its save question answered yes (the user, 2026-10-31: "I
      // want the answer to be yes on quit to save status of equipment and item box") - its
      // own steps from there: the save manager waited for, the equipment, the item box and
      // the treasures saved as the game's yes saves them (the save file written), the play
      // flow told to quit. Asked only of a player the save manager lets save, as the
      // question is: else from its first step, which goes on without it. What its "yes" to
      // "Quit game?" sets first is set too: no cancel, the menu's and the play flow's "on its
      // way".
      const int player = mem::read<int32_t>(pane + g_pane_player);
      const uintptr_t sm = mem::read<uintptr_t>(g_save_man);
      const bool may = player >= 0 && static_cast<uint32_t>(player) <= g_save_last && sm &&
                       !u8at(sm + g_save_slots + static_cast<uint32_t>(player) * g_save_slot + g_save_cannot);
      mem::write<uint8_t>(pause + g_no_cancel, 1);
      mem::write<uint8_t>(pause + g_pause_leaving, 1);
      mem::write<uint8_t>(flow + g_flow_leaving, 1);
      mem::write<uint32_t>(pane + g_option, g_quit_option);
      mem::write<uint8_t>(pane + g_ask_at, static_cast<uint8_t>(may ? g_ask_step : 0));
      mem::write<float>(pane + g_ask_timer, 0.0f);
      mem::write<uint32_t>(pane + g_ask_answer, 0);
      mem::write<uint8_t>(pane + g_pane_at, static_cast<uint8_t>(g_quit_wait));
      g_phase = Phase::kQuitting;
      g_age = 0;
      g_from = Where::kPause;
      g_pane = pane;
      g_menus_asked = false;
      v.offered = v.ready = false;
      note(v, "");
      logf("menus: Chapter Select asked at the pause - the pause menu's QUIT from its save question %s, then the play "
           "flow's quit",
           may ? "answered yes (the equipment, the item box and the treasures saved)"
               : "(this player may not save: none asked, as the game does)");
    } else if (asked && v.ready && over) {
      // The Game Over screen's QUIT: its bit of sBioMain's flags, as its own choice sets it,
      // and the screen at the step that takes the choice made elsewhere - its own steps from
      // there, then the play flow's game over: the game back to the last checkpoint, the
      // profile taken back and saved, the quit.
      set_main_flag(run.main, g_quit_flag);
      mem::write<int32_t>(over + g_over_choice, -1);
      mem::write<uint8_t>(over + g_over_step, static_cast<uint8_t>(g_over_elsewhere));
      g_phase = Phase::kQuitting;
      g_age = 0;
      g_from = Where::kGameOver;
      g_pane = 0;
      g_menus_asked = false;
      v.offered = v.ready = false;
      note(v, "");
      logf("menus: Chapter Select asked at the Game Over screen - its QUIT (the game back to the last checkpoint, "
           "then saved), then the play flow's quit");
    } else if (asked && v.ready && results) {
      // The results' own quit, from their question answered yes: the quit set, no wait, no
      // delay, the prompts off, the step after the question's - their own steps from there,
      // then the play flow's quit after a game's end. After the last chapter their continue
      // sets the same quit; the steps after the question's let the session go and wait for
      // the save manager, as at any other chapter.
      const bool last = v.last;
      mem::write<uint32_t>(results + g_res_prompts, g_res_prompts_off);
      mem::write<uint8_t>(results + g_res_quit, 1);
      mem::write<uint32_t>(results + g_res_wait, 0);
      mem::write<float>(results + g_res_timer, 0.0f);
      mem::write<uint8_t>(results + g_res_step, static_cast<uint8_t>(g_res_quitting));
      g_phase = Phase::kQuitting;
      g_age = 0;
      g_from = Where::kResults;
      g_pane = 0;
      g_menus_asked = false;
      v.offered = v.ready = v.last = false;
      note(v, "");
      logf("menus: Chapter Select asked at the chapter's results%s - their own quit (the chapter's end saved), then the "
           "play flow's quit",
           last ? " (the last chapter's: as their continue quits)" : "");
    } else if (asked && !v.offered && g_phase == Phase::kIdle) {
      note(v, "Chapter Select is offered at Item Management between chapters, and at the pause, the Game Over screen "
              "or the results in a story chapter.");
    }

    if (g_phase == Phase::kQuitting) {
      // The pause menu's steps (a few frames, the save and the save manager's own wait) or
      // the Game Over screen's, then the play flow's quit (its fade out, the chapter let go):
      // over once the game is gone or the menus' area is in - then as from Item Management.
      // On its way while the pane still runs QUIT, QUIT's bit is set or the play flow quits.
      ++g_age;
      const uintptr_t f = area_of(g_cls_flow);
      const uint32_t fstep = f ? u8at(f + g_flow_step) : 0;
      const bool quitting = (f && (fstep == g_flow_out || (g_flow_after && fstep == g_flow_after))) ||
                            (run.main && (u32at(run.main + g_main_flags) & g_quit_flag)) ||
                            (g_pane && run.pause_menu && u32at(g_pane + g_option) == g_quit_option) ||
                            (g_results && run.results && u8at(run.results + g_res_quit));
      if (!run.in_game || area_of(g_cls_front)) {
        g_phase = Phase::kLeaving;
        g_age = 0;
        logf("menus: the chapter has quit - on to the menus");
      } else if ((!quitting && g_age > kQuitTicks) || g_age > kLeaveTicks) {
        stop(v, g_from == Where::kGameOver  ? "Chapter Select: the Game Over screen's QUIT did not go through."
                : g_from == Where::kResults ? "Chapter Select: the results' quit did not go through."
                                            : "Chapter Select: the pause menu's QUIT did not go through - the chapter "
                                              "goes on.");
      }
    }
    switch (g_phase) {
      case Phase::kIdle:
      case Phase::kQuitting: break;
      case Phase::kLeaving: {
        // The game ends and the menus come: aDemo, started, has the loader choose the
        // front end's archives at its next step - the menus' asked for here with the
        // title's, as the story menu's choice asks for them. The start-up menu then
        // comes to the step that takes the return reason - this tick, before its frame
        // takes it - and is handed over only with that archive loaded.
        ++g_age;
        const uintptr_t front = area_of(g_cls_front), lm = game::system("sLoadMan");
        if (front && lm && !g_menus_asked && u8at(front + g_started) && u8at(front + g_front_step) <= g_front_load) {
          mem::write<uint32_t>(lm + g_front_mask, u32at(lm + g_front_mask) | g_menus_bit);
          g_menus_asked = true;
          logf("menus: the menus' own load asks for %s with the title's, as the story menu's choice does",
               g_menus_path);
        }
        // Handed over as soon as the start-up menu's background has started (the menu
        // still fading in, before the step that takes the reason: the background shown
        // under the fade as the choices leave it), at the latest the tick before that step
        // takes the reason.
        const uintptr_t menu = start_up_menu();
        const uint32_t step = menu ? u8at(menu + g_menu_step) : 0;
        const uintptr_t early = menu && step < g_menu_reason_step ? background(menu) : 0;
        if (early || (menu && step == g_menu_reason_step && u8at(menu + g_sub) == 0)) {
          if (!gs || u32at(gs + g_reason) != g_reason_normal || online) {
            stop(v, "Chapter Select: the start-up menu was not coming back as a game's end has it - left to the game.");
            break;
          }
          if (!inventory::archive_loaded(g_menus_path)) {
            logf("menus: %s is not loaded (%s)", g_menus_path,
                 g_menus_asked ? "asked for with the title's" : "the menus' own load went by before it was asked for");
            stop(v, "Chapter Select: the menus' archive is not loaded - left to the game's own menus.");
            break;
          }
          mem::write<uint32_t>(gs + g_reason, g_reason_back);
          mem::write<uint32_t>(gs + g_story, g_story_park);
          const uintptr_t top = early ? early : background(menu);
          if (top) story_chosen(top);
          g_phase = Phase::kParked;
          g_age = 0;
          logf("menus: the start-up menu is up%s - its return reason %u (back to the choice made), the story menu's "
               "choice parked; %s",
               early ? " (fading in)" : "", g_reason_back,
               top ? "its background as the story's choice and CHAPTER SELECT's leave it (the main menu's list on "
                     "its way out, the header in, the panel's segment asked for)"
                   : "its background as it is");
        } else if ((run.in_game && !front) || (screen && g_age > kCloseTicks) || g_age > kLeaveTicks) {
          stop(v, "Chapter Select: the game did not go back to its menus - left as it is.");
        }
        break;
      }
      case Phase::kParked: {
        // The start-up menu fading in (handed over as its background started), then its
        // step taking the reason: waited for. The reason taken: the menus' music on, the
        // story flow parked at its choice - made CHAPTER SELECT's, from its own first step.
        ++g_age;
        const uintptr_t menu = start_up_menu();
        const uint32_t step = menu ? u8at(menu + g_menu_step) : 0;
        const bool at_step = menu && step == g_menu_reason_step;
        if (at_step && u8at(menu + g_sub) == g_sub_back && u8at(menu + g_story_step) == g_story_parked && gs) {
          mem::write<uint8_t>(menu + g_story_step, static_cast<uint8_t>(g_story_pick));
          mem::write<uint8_t>(menu + g_cs_step, 0);
          mem::write<uint32_t>(gs + g_story, g_chapter_select);
          g_phase = Phase::kIdle;
          g_age = 0;
          logf("menus: the story menu's choice made CHAPTER SELECT - its own steps from the first, then the chapter "
               "list");
        } else if (!(menu && gs && u32at(gs + g_reason) == g_reason_back && g_age < kParkTicks &&
                     (step < g_menu_reason_step || (at_step && u8at(menu + g_sub) == 0)))) {
          stop(v, "Chapter Select: the start-up menu did not take it as expected - it goes on to its own menus.");
        }
        break;
      }
    }
  }
  v.going = g_phase != Phase::kIdle;
  if (v.going) v.where = g_from;
  EnterCriticalSection(&g_lock);
  g_view = v;
  LeaveCriticalSection(&g_lock);
}

View view() {
  if (!g_lock_ready) return View{};
  EnterCriticalSection(&g_lock);
  const View v = g_view;
  LeaveCriticalSection(&g_lock);
  return v;
}

void chapter_select() { InterlockedExchange(&g_asked, 1); }

bool leaving() { return g_phase != Phase::kIdle; }

bool saves(Saves* out) {
  if (!g_saves || !out) return false;
  *out = Saves{g_state, g_block, g_block_copy, g_block_size, 0};
  const uintptr_t flow = area_of(g_cls_flow);
  const uintptr_t snap = flow ? mem::read<uintptr_t>(flow + g_snapshot) : 0;
  if (snap && !(snap & 3) && mt::is_a(snap, g_cls_info)) out->snapshot = snap;
  return true;
}

}  // namespace re5cc::menus
