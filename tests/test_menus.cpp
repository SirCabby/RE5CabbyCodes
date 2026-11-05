// Offline test of Chapter Select from Item Management and from the pause (src/menus.cpp,
// built in): a made-up game - sBioMain with its cGameSys, sArea with the play flow (aGame,
// its step and its copy of the profile's cGameInfo as at the last checkpoint) and later
// the menus (aDemo: its start, its load of the front end's archives by sLoadMan's field,
// then the start-up menu's cDemoStartUp), Item Management's screen and its save, the
// pause menu and its panes, cGameInfo's working block of the save data (the cases) and its
// checkpoint copy, cSystemInfo's data and its copy, at the real offsets - run through the
// module's own tick, the game's side played by the test as the game's code does it.
// The discovery is not run: its results are set as the dump gives them (checked there by
// tools/sig.py and the operand reads replayed over it). Build (no object of the mod's
// linked):
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Isrc -static tests/test_menus.cpp
//       -o tests/build/test_menus.exe
//   wine tests/build/test_menus.exe
//
// - Item Management between chapters (the play flow's screen) is offered, one first seen
//   at its main step too; the main menu's (no play flow) is not;
// - offered only in a story game: the game's mode the story's, started from the story's
//   menu (the return reason a start leaves, the top menu's story) - in any other, not at
//   all; ready only with its panes idle at its main step, offline;
// - asked: the screen's own save made once (the cases into the saved state), then the
//   checkpoint copies the play flow's quit takes back brought to what is kept - the
//   profile's data (the box, the gold) and the save data's working block as the save
//   left it, the rest of cGameInfo's copies left alone - then the screen's own quit (its
//   byte, the message, a partner's flags, the fade's timer and its fade out); nothing the
//   screen changed is put back; asked when it cannot go, nothing is called or written;
// - the menus' area made: the loader's field left alone until its start has set it;
//   started, before its load, the menus' archive asked for with the title's (the
//   field's bit) - not again past its load;
// - the menus up: nothing while the start-up menu's background has not started; once it
//   has (the menu fading in), the reason back to the choice made, the story menu's choice
//   parked and the background as the story's choice and CHAPTER SELECT's leave it (the
//   main menu's list picked and on its way, the header shown, the panel's segment asked
//   for, played forward); the fade waited for; the reason taken (the story flow parked),
//   its step made the choice's, CHAPTER SELECT's own steps from the first, the choice
//   CHAPTER SELECT's - a tick late is waited for;
// - the background not understood: handed over the tick before the start-up menu's step
//   takes the reason, the background as it is;
// - given up, nothing more written: the screen not closing, a game starting instead,
//   the menus coming back otherwise (another reason), the menus' own load gone by
//   before it was asked for (their archive not loaded: the game's own menus, no chapter
//   list without its layout - the background as it is), the story flow not parked;
// - at the pause in a story chapter: offered, ready with this machine's pane at the pause
//   menu's first page, offline - not on another page (the options), nor with two players
//   on this machine, nor online (greyed, saying why); not offered at all (no button, no
//   why) in another mode - Lost in Nightmares and Desperate Escape (started from the
//   story's menu, their own modes and the return reason 9), Versus -, in a game not
//   started from the story's menu, in the Mercenaries, nor with the pause's QUIT not
//   understood; that logged once for the screen; asked when it cannot go, nothing
//   written; asked: the pane's QUIT from its save question answered yes (its option, the
//   question's step, timer and answer, then the step waiting for the save manager) - from
//   the question's first
//   step for a player the save manager does not let save -, the menu's no-cancel byte, its
//   and the play flow's "on its way" - nothing of the menus yet; the moves let go meanwhile
//   (leaving()); the save waited for while the pane runs QUIT, then the play flow quitting,
//   the game's end and the menus as from Item Management, CHAPTER SELECT at the end; given
//   up, said so, when the QUIT does not go through (the pause closed, the chapter going on);
// - at the Game Over screen in a story chapter: offered once it is shown, until a choice is
//   made, not at the step waiting for a partner's; not offered in the Mercenaries nor in
//   an extra episode (Desperate Escape's Game Over); asked: QUIT's bit of sBioMain's
//   flags set (under the game's lock while it uses one) and the screen at
//   the step that takes the choice made elsewhere, the choice none; waited for while the
//   bit is set, then the menus as from Item Management;
// - at the chapter's results in a story chapter: offered, ready at their menu - the last
//   chapter's too (no quit of their own: their continue quits), said to be the last - not
//   offered in the Mercenaries nor in an extra episode (Lost in Nightmares' results); asked:
//   their quit set, its wait and timer 0, the prompts off, the step after their question's
//   yes; waited for while their quit is set and while the play
//   flow is at its step after a game's end, then the menus as from Item Management - from
//   the last chapter's the same;
// - at the Game Over screen and the results, whether it can go logged as it changes (once
//   a change, with the screen's steps), again for the next screen;
// - the saved copies for the item box: the save data's blocks, and the play flow's copy of
//   the profile's cGameInfo only while it is one (none: 0), nothing when not understood.
#include "../src/menus.cpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace re5cc {
bool g_verbose = false;
std::vector<std::string> g_log;  // every line logged, for the checks of what is logged
void logf(const char* fmt, ...) {
  char line[1024];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  g_log.emplace_back(line);
  if (g_verbose) std::printf("      log: %s\n", line);
}
// The lines logged since `from` that hold `text`.
int logged(size_t from, const char* text) {
  int n = 0;
  for (size_t i = from; i < g_log.size(); ++i) n += g_log[i].find(text) != std::string::npos;
  return n;
}
}  // namespace re5cc

// --- what the module asks of the rest of the mod, made up ----------------------------------
namespace {
re5cc::game::Layout g_layout;
uintptr_t g_sarea = 0, g_loadman = 0, g_game_info = 0, g_system_info = 0;
bool g_idle = true;  // Item Management's panes
// The menus' archive, as the loader is asked for it, and whether the menus' own load
// brought it in (the game's side, below).
const char* const kMenusPath = "Image\\Archive\\MenuResource";
bool g_menus_loaded = false;
}  // namespace
namespace re5cc::game {
std::vector<uintptr_t> sig_all(const char*) { return {}; }
uintptr_t sig_one(const char*) { return 0; }
uintptr_t call_target(uintptr_t) { return 0; }
bool ready() { return true; }
const Layout& layout() { return g_layout; }
uintptr_t system(const char* name) {
  const std::string n(name);
  return n == "sBioArea" ? g_sarea : n == "sLoadMan" ? g_loadman : 0;
}
uintptr_t game_info() { return g_game_info; }
uintptr_t system_info() { return g_system_info; }
}  // namespace re5cc::game
namespace re5cc::mt {
// An object's class: its first word, the class's vtable - read as the real one reads it
// (a word that is not a pointer is no object).
const Class* find(const char*) { return nullptr; }
bool is_a(uintptr_t obj, const Class* cls) {
  uintptr_t vt = 0;
  return cls && mem::read_safe(obj, &vt) && vt == cls->vtable;
}
}  // namespace re5cc::mt
namespace re5cc::image {
mem::Range text() { return {}; }
mem::Range rdata() { return {}; }
}  // namespace re5cc::image
namespace re5cc::inventory {
bool lists_known() { return true; }
const std::string& archive_dir() {
  static const std::string dir = "Image\\Archive\\";
  return dir;
}
bool archive_loaded(const char* path) { return g_menus_loaded && path && std::string(path) == kMenusPath; }
}  // namespace re5cc::inventory
namespace re5cc::itembox {
bool screen_idle(uintptr_t screen) { return screen && g_idle; }
}  // namespace re5cc::itembox

// --- the made-up game ----------------------------------------------------------------------
namespace {
using namespace re5cc;

// As the dump gives them (the replay of discover() over it, 2026-10-31).
constexpr uint32_t kAreaList = 0x382C, kAreaCount = 8, kFlowScreen = 0x2C;
constexpr uint32_t kStep = 0x14, kQuit = 0x3E, kMessage = 0x16C, kJoin1 = 0x5B6C, kJoin2 = 0x5B6D, kTimer = 0x194;
constexpr uint32_t kCases0 = 0x17D0, kCases1 = 0x2D60, kCasesSize = 0x1080;  // in the working block and the next
constexpr uint32_t kBlock = 0x1690, kBlockSize = 0x1590, kBlockCopy = 0x42D0;  // cGameInfo: the working block, its copy
constexpr uint32_t kSaved = 0x240;      // cGameInfo: the saved state's cases
constexpr uint32_t kPlayCopy = 0x4240;  // cGameInfo: the copies of its play data (the chapter's), before the block's
constexpr uint32_t kProfileSize = 0x8C74, kProfileData = 0x4638;  // cSystemInfo: its data from +4, then its copy
constexpr uint32_t kSys = 0x1042C, kOnline = 0x10434, kReason = 0x18, kStory = 0x1C, kTop = 0xC, kMode = 0x58;
constexpr uint32_t kMenuStep = 4, kSub = 5, kStoryStep = 6, kCsStep = 7, kMenuSub = 0x25;
constexpr uint32_t kStarted = 0x20, kFrontStep = 4, kFrontScene = 0x28;  // an area's started byte; aDemo's
constexpr uint32_t kLoaderSize = 0x208E8, kFrontMask = 0x207D4, kMenusBit = 2;  // sLoadMan: the front end's archives
constexpr uint32_t kTopAt = 0x14, kList = 0x54, kHeader = 0x5C;  // the start-up menu's background, its list and header
constexpr uint32_t kPicked = 0xD51, kGoOn = 0xD64, kShown = 0xD84;  // the list's flags, the header's
constexpr uint32_t kAsk = 0x68, kSegment = 0x6C, kBack = 0x70, kPanel = 8;  // the panel: a segment asked for
// The pause menu (uIdPause00): its panes, a pane's fields, QUIT's option and steps, the
// question's; the play flow's step and its quit's, its "on its way", its copy of cGameInfo.
constexpr uint32_t kPanes = 0x48, kPaneStep = 0x68, kNPanes = 4, kPaneDone = 4, kOption = 0x34, kPaneAt = 0x58;
constexpr uint32_t kAskAt = 0x59, kAskTimer = 0x5C, kAskAnswer = 0x60, kNoCancel = 0x3F, kPauseLeaving = 0x1F4;
constexpr uint32_t kQuitOption = 3, kQuitWait = 10, kQuitAsk = 11, kQuitOut = 0x14, kAskStep = 2;
constexpr uint32_t kFlowStep = 4, kFlowOut = 0xC, kFlowLeaving = 0x365, kSnapshot = 0x130;
constexpr uint32_t kPanePlayer = 0x10, kSaveLast = 3, kSaveSlots = 0x78, kSaveSlot = 0x20, kSaveCannot = 8;
constexpr uint32_t kMainFlags = 0x10438, kQuitFlag = 0x10000, kMainMt = 0x1C, kMainLock = 4;  // sBioMain
constexpr uint32_t kOverShown = 0x194, kOverStep = 0x14, kOverElsewhere = 0xA, kOverChoice = 0x190, kOverDecided = 0x196;
constexpr uint32_t kResNoQuit = 0x189, kResStep = 0x14, kResMenu = 0xA, kResQuitting = 0x15, kResQuit = 0x188;
constexpr uint32_t kResWait = 0x170, kResTimer = 0x174, kResPrompts = 0x2EC, kResPromptsOff = 3, kFlowAfter = 0xB;
std::vector<uint8_t> g_save_man_obj(0x200);  // the save manager: four players' slots
uintptr_t g_save_man_ptr = 0;                // ... its global
uint8_t g_mt_on = 0;                         // the game's "uses its lock" global
constexpr uint32_t kState = 0x100;  // cGameInfo: the save data's saved state, the block before the working block

struct Obj {
  std::vector<uint8_t> b;
  explicit Obj(size_t n, uintptr_t vtable = 0) : b(n) { mem::write<uintptr_t>(at(), vtable); }
  uintptr_t at() const { return reinterpret_cast<uintptr_t>(b.data()); }
  uint8_t u8(uint32_t o) const { return b[o]; }
  uint32_t u32(uint32_t o) const { return mem::read<uint32_t>(at() + o); }
  float f32(uint32_t o) const { return mem::read<float>(at() + o); }
  void set8(uint32_t o, uint8_t v) { b[o] = v; }
  void set32(uint32_t o, uint32_t v) { mem::write<uint32_t>(at() + o, v); }
  void setf(uint32_t o, float v) { mem::write<float>(at() + o, v); }
};

// The classes' vtables: only told apart.
uintptr_t g_vt_flow = 0x1001, g_vt_front = 0x1002, g_vt_startup = 0x1003, g_vt_sys = 0x1004;
uintptr_t g_vt_top = 0x1005, g_vt_list = 0x1006, g_vt_header = 0x1007, g_vt_info = 0x1008;

// The screen's save, as the game's does it: each pane's case put in order in the working
// block (here a slot number), then the working block's cases into the saved state.
int g_saves = 0;
uintptr_t g_saved_screen = 0;
void __thiscall fake_save(void* screen) {
  ++g_saves;
  g_saved_screen = reinterpret_cast<uintptr_t>(screen);
  mem::write<uint8_t>(g_game_info + kCases0 + 0x14, 0x5E);
  std::memcpy(reinterpret_cast<void*>(g_game_info + kSaved), reinterpret_cast<const void*>(g_game_info + kCases0),
              kCasesSize);
}
mt::Class g_flow{0, g_vt_flow, "aGame", 0x510}, g_front{0, g_vt_front, "aDemo", 0x3C},
    g_startup{0, g_vt_startup, "cDemoStartUp", 0x5C}, g_sys{0, g_vt_sys, "cGameSys", 0x7A8},
    g_top_cls{0, g_vt_top, "uIdTopMenu00", 0x5EC}, g_list_cls{0, g_vt_list, "uIdStartUp11", 0xDA0},
    g_header_cls{0, g_vt_header, "uIdStartUp14", 0xD90}, g_info_cls{0, g_vt_info, "cGameInfo", 0x5940};

struct Game {
  Obj main{0x10570}, sys{0x7A8, g_vt_sys}, area{0x384C}, flow{0x510, g_vt_flow}, front{0x3C, g_vt_front},
      startup{0x5C, g_vt_startup}, screen{0x5B88}, info{0x5940}, profile{kProfileSize}, loader{kLoaderSize},
      top{0x5EC, g_vt_top}, list{0xDA0, g_vt_list}, header{0xD90, g_vt_header}, pause{0x1F8},
      snapshot{0x5940, g_vt_info}, over{0x1F0}, results{0x330};
  game::Run run;
  Game() {
    main.set32(kSys, static_cast<uint32_t>(sys.at()));
    g_sarea = area.at();
    g_loadman = loader.at();
    loader.set32(kFrontMask, 4);  // the play flow's Item Management had the shop's archive loaded
    g_game_info = info.at();
    g_system_info = profile.at();
    for (uint32_t i = 0; i < kProfileData; ++i) {
      profile.set8(4 + i, static_cast<uint8_t>(i * 5 + 3));             // its data
      profile.set8(4 + kProfileData + i, static_cast<uint8_t>(i * 11));  // its copy, older
    }
    run.main = main.at();
    sys.set32(kReason, 1);  // as a game's start leaves it
    sys.set32(kTop, 0);     // the story
    sys.set32(kStory, 2);   // a choice made, CONTINUE's (0) or JOIN GAME's (2): overwritten
    for (uint32_t i = 0; i < kBlockSize; ++i) {
      info.set8(kBlock + i, static_cast<uint8_t>(i * 7 + 1));         // the working block
      info.set8(kBlock + kBlockSize + i, static_cast<uint8_t>(i * 3 + 5));  // the partner's, the next
      info.set8(kBlockCopy + i, static_cast<uint8_t>(i * 13 + 2));    // its checkpoint copy, older
    }
    for (uint32_t i = kPlayCopy; i < kBlockCopy; ++i) info.set8(i, static_cast<uint8_t>(i * 17));
  }
  void areas(std::initializer_list<const Obj*> list) {
    for (uint32_t i = 0; i < kAreaCount; ++i) area.set32(kAreaList + 4 * i, 0);
    uint32_t i = 0;
    for (const Obj* a : list) area.set32(kAreaList + 4 * i++, static_cast<uint32_t>(a->at()));
  }
  // Item Management opened by the play flow between chapters, fading in.
  void open_box() {
    areas({&flow});
    flow.set32(kFlowScreen, static_cast<uint32_t>(screen.at()));
    screen.set8(kStep, 0);
    run.item_screen = screen.at();
    run.screen = game::Screen::kItemBox;
  }
  // The pause opened in a story chapter: the play flow's, this machine's pane at the menu's
  // first page (its option 0, waiting at its step 1).
  void open_pause() {
    areas({&flow});
    flow.set32(kSnapshot, static_cast<uint32_t>(snapshot.at()));
    flow.set8(kFlowStep, 6);  // in play
    pause.set8(kPanes, 1);
    pause.set32(kPanes + kOption, 0);
    pause.set8(kPanes + kPaneAt, 1);
    run.in_game = true;
    run.nchars = 2;
    run.pause_menu = pause.at();
    run.screen = game::Screen::kPause;
  }
  // The Game Over screen up in a story chapter, its menu shown, waiting for a choice.
  void open_over() {
    areas({&flow});
    flow.set32(kSnapshot, static_cast<uint32_t>(snapshot.at()));
    flow.set8(kFlowStep, 9);  // the play flow's game over
    over.set8(kOverShown, 1);
    over.set8(kOverStep, 2);
    over.set32(kOverChoice, 0);
    run.in_game = true;
    run.nchars = 2;
    run.game_over = over.at();
    run.screen = game::Screen::kPlay;
  }
  // The chapter's results up, at their menu.
  void open_results() {
    areas({&flow});
    flow.set32(kSnapshot, static_cast<uint32_t>(snapshot.at()));
    flow.set8(kFlowStep, 8);  // the play flow's chapter end
    results.set8(kResStep, kResMenu);
    results.set32(kResPrompts, 2);
    results.setf(kResTimer, 0.5f);
    results.set32(kResWait, 7);
    run.in_game = true;
    run.nchars = 2;
    run.results = results.at();
    run.screen = game::Screen::kPlay;
  }
  uint8_t pane8(uint32_t o) const { return pause.u8(kPanes + o); }
  uint32_t pane32(uint32_t o) const { return pause.u32(kPanes + o); }
  // The pause menu's QUIT as its handler runs it from its save manager's step: the save
  // question's "no" (the profile taken back), the play flow told to quit (its step the
  // quit's), the pane done.
  void quit_from_pause() {
    pause.set8(kPanes + kPaneAt, kQuitAsk);
    pause.set8(kPanes + kAskAt, 0);
    pause.set8(kPanes + kPaneAt, kQuitOut);
    flow.set8(kFlowStep, kFlowOut);
    pause.set8(kPanes + kPaneDone, 1);
  }
  // The play flow's quit: the screen let go, the game ended, the menus' area made (not
  // started yet).
  void to_menus() {
    run.item_screen = 0;
    run.pause_menu = 0;
    run.game_over = 0;
    run.results = 0;
    run.in_game = false;
    run.nchars = 0;
    run.screen = game::Screen::kNone;
    flow.set32(kFlowScreen, 0);
    areas({&front});
  }
  // aDemo's start - the loader's field the title's alone, the story menu's choice 0 -
  // and its frame's first step, on to the load (sArea runs both in one frame).
  void front_start() {
    loader.set32(kFrontMask, 1);
    sys.set32(kStory, 0);
    front.set8(kStarted, 1);
    front.set8(kFrontStep, 1);
  }
  // Its load: the archives the field asks for (the menus' by its bit), waited for, then
  // the start-up menu made.
  void front_load() {
    g_menus_loaded = (loader.u32(kFrontMask) & kMenusBit) != 0;
    front.set8(kFrontStep, 4);
    front.set32(kFrontScene, static_cast<uint32_t>(startup.at()));
    startup.set8(kMenuStep, 0);
    startup.set8(kSub, 0);
  }
  // The start-up menu's first step: its background made, fading in; the background's
  // start makes its list and header (the main menu's look: nothing set).
  void fade_in() {
    startup.set32(kTopAt, static_cast<uint32_t>(top.at()));
    startup.set8(kSub, 0x13);
    top.set32(kList, static_cast<uint32_t>(list.at()));
    top.set32(kHeader, static_cast<uint32_t>(header.at()));
    top.set32(kSegment, 7);
  }
  // The background as the game left it: none of the choices' writes.
  bool look_untouched() const {
    return list.u8(kPicked) == 0 && list.u8(kGoOn) == 0 && header.u8(kShown) == 0 && top.u8(kAsk) == 0 &&
           top.u32(kSegment) == 7;
  }
};

// The game's way to the menus, a tick of the mod's in each frame: the menus' area made,
// started, then its load.
void to_menus_loaded(Game& g) {
  g.to_menus();
  menus::tick(g.run);
  g.front_start();
  menus::tick(g.run);
  g.front_load();
}

// cSystemInfo's data and its checkpoint copy.
std::vector<uint8_t> profile_data(const Game& g) { return {g.profile.b.begin() + 4, g.profile.b.begin() + 4 + kProfileData}; }
std::vector<uint8_t> profile_copy(const Game& g) {
  return {g.profile.b.begin() + 4 + kProfileData, g.profile.b.begin() + 4 + 2 * kProfileData};
}
// cGameInfo's bytes from `at`.
std::vector<uint8_t> info_bytes(const Game& g, uint32_t at, uint32_t n) {
  return {g.info.b.begin() + at, g.info.b.begin() + at + n};
}

int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

void setup() {
  using namespace menus;
  g_found = true;
  g_area_list = kAreaList;
  g_area_count = kAreaCount;
  g_cls_flow = &g_flow;
  g_cls_front = &g_front;
  g_cls_startup = &g_startup;
  g_cls_sys = &g_sys;
  g_flow_screen = kFlowScreen;
  g_step = kStep;
  g_main = 2;
  g_out = 3;
  g_quit = kQuit;
  g_message = kMessage;
  g_join1 = kJoin1;
  g_join2 = kJoin2;
  g_timer = kTimer;
  menus::g_save = fake_save;
  g_block = kBlock;
  g_block_size = kBlockSize;
  g_block_copy = kBlockCopy;
  menus::g_sys = kSys;
  g_online = kOnline;
  g_reason = kReason;
  g_story = kStory;
  g_top = kTop;
  g_mode = kMode;
  g_mode_sys = kSys;
  g_reason_normal = 1;
  g_reason_back = 7;
  g_top_story = 0;
  g_chapter_select = 1;
  g_story_park = 13;
  g_menu_step = kMenuStep;
  g_menu_reason_step = 2;
  g_sub = kSub;
  g_sub_back = 3;
  g_story_step = kStoryStep;
  g_story_pick = 2;
  g_story_parked = 0x5A;
  g_cs_step = kCsStep;
  g_front_step = kFrontStep;
  g_front_load = 1;
  g_started = kStarted;
  g_front_mask = kFrontMask;
  g_menus_bit = kMenusBit;
  g_menus_path = kMenusPath;
  g_look = true;
  g_cls_top = &g_top_cls;
  g_cls_list = &g_list_cls;
  g_cls_header = &g_header_cls;
  menus::g_list = kList;
  menus::g_header = kHeader;
  g_picked = kPicked;
  g_go_on = kGoOn;
  g_shown = kShown;
  g_shown_value = 1;
  g_request = kAsk;
  g_segment = kSegment;
  g_backward = kBack;
  g_panel = kPanel;
  // The save data's blocks and the play flow's copy of cGameInfo; the pause menu's QUIT.
  menus::g_saves = true;
  g_state = kState;
  g_snapshot = kSnapshot;
  g_cls_info = &g_info_cls;
  menus::g_pause = true;
  g_panes = kPanes;
  menus::g_pane_step = kPaneStep;
  g_npanes = kNPanes;
  g_pane_done = kPaneDone;
  g_option = kOption;
  g_pane_at = kPaneAt;
  g_quit_option = kQuitOption;
  g_quit_wait = kQuitWait;
  g_quit_ask = kQuitAsk;
  g_quit_out = kQuitOut;
  g_ask_at = kAskAt;
  g_ask_timer = kAskTimer;
  g_ask_answer = kAskAnswer;
  g_ask_step = kAskStep;
  g_no_cancel = kNoCancel;
  g_pause_leaving = kPauseLeaving;
  g_flow_leaving = kFlowLeaving;
  g_flow_step = kFlowStep;
  g_flow_out = kFlowOut;
  g_pane_player = kPanePlayer;
  g_save_man_ptr = reinterpret_cast<uintptr_t>(g_save_man_obj.data());
  g_save_man = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_save_man_ptr));
  g_save_last = kSaveLast;
  g_save_slots = kSaveSlots;
  g_save_slot = kSaveSlot;
  g_save_cannot = kSaveCannot;
  g_main_flags = kMainFlags;
  g_quit_flag = kQuitFlag;
  g_main_mt = kMainMt;
  g_main_lock = kMainLock;
  g_mt_global = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_mt_on));
  // The Game Over screen.
  menus::g_over = true;
  g_over_shown = kOverShown;
  g_over_step = kOverStep;
  g_over_elsewhere = kOverElsewhere;
  g_over_choice = kOverChoice;
  g_over_decided = kOverDecided;
  // The chapter's results.
  menus::g_results = true;
  g_res_noquit = kResNoQuit;
  g_res_step = kResStep;
  g_res_menu = kResMenu;
  g_res_quitting = kResQuitting;
  g_res_quit = kResQuit;
  g_res_wait = kResWait;
  g_res_timer = kResTimer;
  g_res_prompts = kResPrompts;
  g_res_prompts_off = kResPromptsOff;
  g_flow_after = kFlowAfter;
}

// The module's state let go between cases.
void reset() {
  menus::g_phase = menus::Phase::kIdle;
  menus::g_age = 0;
  menus::g_screen = 0;
  menus::g_view = menus::View{};
  menus::g_asked = 0;
  menus::g_menus_asked = false;
  g_idle = true;
  g_saves = 0;
  g_saved_screen = 0;
  g_menus_loaded = false;
  menus::g_look = true;
  menus::g_pause = true;
  menus::g_saves = true;
  menus::g_over = true;
  menus::g_results = true;
  menus::g_from = menus::Where::kNone;
  menus::g_pane = 0;
  menus::g_end_logged = false;
  menus::g_end_why = nullptr;
  menus::g_withheld = 0;
  std::fill(g_save_man_obj.begin(), g_save_man_obj.end(), 0);
  g_mt_on = 0;
}

// The pause menu as the mod leaves it untouched: its pane at the first page, nothing set.
bool pause_untouched(const Game& g) {
  return g.pane32(kOption) == 0 && g.pane8(kPaneAt) == 1 && g.pane8(kAskAt) == 0 && g.pane32(kAskAnswer) == 0 &&
         g.pause.u8(kNoCancel) == 0 && g.pause.u8(kPauseLeaving) == 0 && g.flow.u8(kFlowLeaving) == 0;
}

// The screen as the mod leaves it untouched: none of its quit's fields written.
bool screen_untouched(const Game& g) {
  return g.screen.u8(kQuit) == 0 && g.screen.u32(kMessage) == 0x77 && g.screen.f32(kTimer) == 0.5f &&
         g.screen.u8(kStep) == 2;
}

}  // namespace

int main(int argc, char** argv) {
  re5cc::g_verbose = argc > 1 && std::string(argv[1]) == "-v";
  menus::init();
  setup();
  g_layout.system_size = kProfileSize;
  g_layout.checkpoint = kProfileData;

  // --- the way there ---------------------------------------------------------------------
  {
    reset();
    Game g;
    g.open_box();
    menus::tick(g.run);
    check(menus::view().offered && !menus::view().ready, "Item Management between chapters: offered, not while it fades in");
    g.screen.set8(kStep, 2);
    g.screen.set32(kMessage, 0x77);
    g.screen.setf(kTimer, 0.5f);
    menus::tick(g.run);
    check(menus::view().offered && menus::view().ready, "... ready at its main step, its panes idle");
    // The panes change the working block (an item moved, one upgraded) and the profile
    // (the box, the gold).
    g.info.set8(kCases0 + 0x10, 0xAA);
    g.info.set8(kCases0 + 0x400, 0xBB);
    g.profile.set8(4 + 0x3840, 0xCC);
    g.profile.set8(4 + 0x184, 0xDD);
    const std::vector<uint8_t> block_copy = info_bytes(g, kBlockCopy, kBlockSize), sys_copy = profile_copy(g);
    g_idle = false;
    menus::tick(g.run);
    check(!menus::view().ready && menus::view().why[0], "... not while a pane is busy (an item picked up), saying why");
    menus::chapter_select();
    menus::tick(g.run);
    check(screen_untouched(g) && g_saves == 0 && profile_copy(g) == sys_copy &&
              info_bytes(g, kBlockCopy, kBlockSize) == block_copy && !menus::view().going && menus::view().note[0],
          "asked while it cannot go: nothing saved or written, the reason said");
    g_idle = true;
    g.main.set32(kOnline, 1);
    menus::tick(g.run);
    check(!menus::view().ready, "... nor in an online game");
    g.main.set32(kOnline, 0);
    g.sys.set32(kReason, 3);
    menus::tick(g.run);
    check(!menus::view().offered && !menus::view().ready && menus::view().where == menus::Where::kNone,
          "... not offered with another return reason (a game not started from the story's menu)");
    g.sys.set32(kReason, 1);
    g.sys.set32(kTop, 1);
    menus::tick(g.run);
    check(!menus::view().offered, "... nor from another top menu choice");
    g.sys.set32(kTop, 0);
    g.sys.set32(kMode, 4);
    menus::tick(g.run);
    check(!menus::view().offered, "... nor in another mode");
    g.sys.set32(kMode, 0);
    menus::tick(g.run);
    check(menus::view().ready, "... ready again");

    const std::vector<uint8_t> play_copy = info_bytes(g, kPlayCopy, kBlockCopy - kPlayCopy);
    menus::chapter_select();
    menus::tick(g.run);
    check(g_saves == 1 && g_saved_screen == g.screen.at(), "asked: the screen's own save made, once, of the screen");
    check(info_bytes(g, kBlockCopy, kBlockSize) == info_bytes(g, kBlock, kBlockSize) &&
              g.info.u8(kBlockCopy + (kCases0 - kBlock) + 0x14) == 0x5E,
          "... then the save data's working block over its checkpoint copy, as the save left it (the play flow's "
          "quit takes it back)");
    check(profile_copy(g) == profile_data(g) && g.profile.u8(4 + kProfileData + 0x3840) == 0xCC &&
              g.profile.u8(4 + kProfileData + 0x184) == 0xDD,
          "... and the profile's checkpoint copy made its data: the box and the gold as the screen left them");
    check(g.info.u8(kCases0 + 0x10) == 0xAA && g.info.u8(kCases0 + 0x400) == 0xBB && g.profile.u8(4 + 0x3840) == 0xCC &&
              g.profile.u8(4 + 0x184) == 0xDD && info_bytes(g, kPlayCopy, kBlockCopy - kPlayCopy) == play_copy,
          "... nothing the screen changed put back, cGameInfo's other copies left alone");
    check(g.screen.u8(kQuit) == 1 && g.screen.u32(kMessage) == 0 && g.screen.u8(kJoin1) == 0 &&
              g.screen.u8(kJoin2) == 0 && g.screen.f32(kTimer) == 0.0f && g.screen.u8(kStep) == 3,
          "... the screen's own quit: its byte, the message, a partner's flags, the timer, its fade out");
    check(g.sys.u32(kReason) == 1 && g.sys.u32(kStory) == 2, "... and nothing of the menus yet");
    check(menus::view().going && !menus::view().offered, "... on its way, no longer offered");
    // Its fade out, then the play flow's quit; the menus' area made.
    g.screen.set8(kStep, 4);
    menus::tick(g.run);
    g.to_menus();
    menus::tick(g.run);
    check(g.loader.u32(kFrontMask) == 4 && menus::view().going,
          "the menus' area made, not started: the loader's field left alone (its start sets it)");
    g.front_start();
    menus::tick(g.run);
    check(g.loader.u32(kFrontMask) == (1 | kMenusBit),
          "... started, its load next: the menus' archive asked for with the title's (the field's bit)");
    g.front_load();
    g.loader.set32(kFrontMask, 1);  // set again by the game
    for (int i = 0; i < 20; ++i) menus::tick(g.run);
    check(g.loader.u32(kFrontMask) == 1 && g.sys.u32(kReason) == 1 && g.sys.u32(kStory) == 0 && menus::view().going,
          "the menus up: the field not written again past its load; nothing while the start-up menu's background "
          "has not started");
    // Its first step: the background made, not started yet (no list, no header), then
    // started, the menu fading in.
    g.startup.set32(kTopAt, static_cast<uint32_t>(g.top.at()));
    menus::tick(g.run);
    check(g.sys.u32(kReason) == 1 && g.sys.u32(kStory) == 0 && g.list.u8(kPicked) == 0 && g.header.u8(kShown) == 0 &&
              g.top.u8(kAsk) == 0,
          "its background made, not started (its start reads the reason): not handed over yet");
    g.fade_in();
    menus::tick(g.run);
    check(g.sys.u32(kReason) == 7 && g.sys.u32(kStory) == 13,
          "its background started (fading in): the reason back to the choice made, the story menu's choice parked");
    check(g.list.u8(kPicked) == 1 && g.list.u8(kGoOn) == 1 && g.header.u8(kShown) == 1,
          "... the background as the story's choice leaves it: the main menu's list picked and on its way, the "
          "header shown");
    check(g.top.u8(kAsk) == 1 && g.top.u32(kSegment) == kPanel && g.top.u8(kBack) == 0,
          "... and its panel's segment from the story menu to CHAPTER SELECT's screen asked for, played forward");
    // The fade (about a second), then the step that takes the reason.
    g.list.set8(kPicked, 0);  // the list's own frame goes on
    g.top.set8(kAsk, 0);
    for (int i = 0; i < 90; ++i) menus::tick(g.run);
    g.startup.set8(kMenuStep, 2);
    g.startup.set8(kSub, 0);
    menus::tick(g.run);
    check(menus::view().going && g.sys.u32(kReason) == 7 && g.sys.u32(kStory) == 13 && g.list.u8(kPicked) == 0 &&
              g.top.u8(kAsk) == 0,
          "... the fade and the step that takes the reason waited for, nothing written again");
    // A frame late: the reason not taken yet - waited for.
    menus::tick(g.run);
    check(menus::view().going && g.startup.u8(kStoryStep) == 0, "... a tick before it is taken: waited for");
    // The game's frame: the back reason taken (cleared; its step's next 3, the story
    // flow's step 2, CHAPTER SELECT's 2), the story flow parked by the choice past its last.
    g.sys.set32(kReason, 0);
    g.startup.set8(kSub, 3);
    g.startup.set8(kStoryStep, 0x5A);
    g.startup.set8(kCsStep, 2);
    g.startup.set8(kMenuSub, 0);
    menus::tick(g.run);
    check(g.startup.u8(kStoryStep) == 2 && g.startup.u8(kCsStep) == 0 && g.sys.u32(kStory) == 1,
          "the reason taken: the story flow's step the choice's, CHAPTER SELECT's steps from the first, the choice "
          "CHAPTER SELECT");
    check(!menus::view().going && !menus::view().note[0], "... done, nothing said");
    // Nothing more written afterwards.
    g.startup.set8(kStoryStep, 9);
    g.sys.set32(kStory, 4);
    for (int i = 0; i < 10; ++i) menus::tick(g.run);
    check(g.startup.u8(kStoryStep) == 9 && g.sys.u32(kStory) == 4, "... and nothing written afterwards");
  }

  // --- the background not understood: handed over at the reason's step, as it is -----------
  {
    reset();
    menus::g_look = false;
    Game g;
    g.open_box();
    menus::tick(g.run);
    g.screen.set8(kStep, 2);
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    to_menus_loaded(g);
    g.fade_in();
    for (int i = 0; i < 10; ++i) menus::tick(g.run);
    check(g.sys.u32(kReason) == 1 && menus::view().going,
          "the background not understood: not handed over while the start-up menu fades in");
    g.startup.set8(kMenuStep, 2);
    g.startup.set8(kSub, 0);
    menus::tick(g.run);
    check(g.sys.u32(kReason) == 7 && g.sys.u32(kStory) == 13 && g.look_untouched(),
          "... handed over the tick before its step takes the reason, the background as it is");
  }

  // --- not offered, or offered at once -----------------------------------------------------
  {
    reset();
    Game g;
    g.open_box();
    g.areas({&g.front});  // the main menu's Item Management: no play flow
    menus::tick(g.run);
    check(!menus::view().offered, "the main menu's Item Management (no play flow): not offered");
    menus::chapter_select();
    menus::tick(g.run);
    check(g.screen.u8(kQuit) == 0 && !menus::view().going, "... asked: nothing written");
  }
  {
    reset();
    Game g;
    g.open_box();
    g.screen.set8(kStep, 2);  // first seen past its fade in (the mod's tick came late)
    menus::tick(g.run);
    check(menus::view().offered && menus::view().ready,
          "a screen first seen past its fade in: ready at once (nothing is noted as it opens)");
  }

  // --- given up --------------------------------------------------------------------------
  {
    reset();
    Game g;
    g.open_box();
    menus::tick(g.run);
    g.screen.set8(kStep, 2);
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    g.screen.set8(kStep, 2);  // the screen not closing
    for (int i = 0; i < menus::kCloseTicks + 2; ++i) menus::tick(g.run);
    check(!menus::view().going && menus::view().note[0], "the screen not closing: given up, said so");
  }
  {
    reset();
    Game g;
    g.open_box();
    menus::tick(g.run);
    g.screen.set8(kStep, 2);
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    g.run.item_screen = 0;
    g.run.in_game = true;  // a game starting instead
    menus::tick(g.run);
    check(!menus::view().going, "a game starting instead: given up");
  }
  {
    reset();
    Game g;
    g.open_box();
    menus::tick(g.run);
    g.screen.set8(kStep, 2);
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    to_menus_loaded(g);
    g.sys.set32(kReason, 0);  // the menus coming back otherwise (signed out: the title)
    g.startup.set8(kMenuStep, 2);
    menus::tick(g.run);
    check(!menus::view().going && g.sys.u32(kReason) == 0 && g.sys.u32(kStory) == 0,
          "the menus back with another reason: given up, nothing written");
  }
  {
    reset();
    Game g;
    g.open_box();
    menus::tick(g.run);
    g.screen.set8(kStep, 2);
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    g.to_menus();
    menus::tick(g.run);
    g.front_start();
    g.front_load();  // the menus' own load before a tick of the mod's (the title's archive alone)
    menus::tick(g.run);
    check(g.loader.u32(kFrontMask) == 1 && menus::view().going,
          "the menus' own load gone by before the mod's tick: the loader's field left alone past it");
    g.fade_in();
    menus::tick(g.run);
    check(!menus::view().going && menus::view().note[0] && g.sys.u32(kReason) == 1 && g.sys.u32(kStory) == 0 &&
              g.startup.u8(kStoryStep) == 0 && g.look_untouched(),
          "... their archive not loaded as the start-up menu comes: given up, nothing written (its own menus, its "
          "background as it is)");
  }
  {
    reset();
    Game g;
    g.open_box();
    menus::tick(g.run);
    g.screen.set8(kStep, 2);
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    to_menus_loaded(g);
    g.startup.set8(kMenuStep, 2);
    menus::tick(g.run);
    g.sys.set32(kReason, 0);
    g.startup.set8(kSub, 1);  // the start-up menu went another way
    g.startup.set8(kStoryStep, 0);
    menus::tick(g.run);
    check(!menus::view().going && g.startup.u8(kStoryStep) == 0 && g.startup.u8(kCsStep) == 0 &&
              g.sys.u32(kStory) == 13,
          "the start-up menu not parked as expected: given up, its steps left alone");
  }

  // --- at the pause ------------------------------------------------------------------------
  {
    reset();
    Game g;
    g.open_pause();
    menus::tick(g.run);
    check(menus::view().offered && menus::view().where == menus::Where::kPause && menus::view().ready,
          "the pause in a story chapter: offered there, ready at the pause menu's first page");
    g.pause.set32(kPanes + kOption, 4);  // the options
    menus::tick(g.run);
    check(!menus::view().ready && std::strstr(menus::view().why, "first page"), "... not on another page, saying why");
    menus::chapter_select();
    menus::tick(g.run);
    check(g.pane32(kOption) == 4 && g.pane8(kPaneAt) == 1 && g.pause.u8(kNoCancel) == 0 &&
              g.flow.u8(kFlowLeaving) == 0 && !menus::view().going && menus::view().note[0],
          "... asked there: nothing written, the reason said");
    g.pause.set32(kPanes + kOption, 0);
    g.pause.set8(kPanes + kPaneStep, 1);  // a second player on this machine
    menus::tick(g.run);
    check(!menus::view().ready, "... not with two players on this machine");
    g.pause.set8(kPanes + kPaneStep, 0);
    g.main.set32(kOnline, 1);
    menus::tick(g.run);
    check(menus::view().offered && !menus::view().ready && std::strstr(menus::view().why, "online"),
          "... nor in an online story game: offered, greyed, saying why");
    g.main.set32(kOnline, 0);
    // The extra episodes: started from the story's menu (its top menu's choice), in modes of
    // their own and with the return reason 9 (FUN_00BF9560, FUN_00BF55E0).
    size_t mark = g_log.size();
    g.sys.set32(kMode, 3);
    g.sys.set32(kReason, 9);
    menus::tick(g.run);
    menus::tick(g.run);
    check(!menus::view().offered && !menus::view().ready && menus::view().where == menus::Where::kNone &&
              !menus::view().why[0],
          "the pause in Lost in Nightmares (mode 3, the return reason 9): not offered - no button, greyed or not");
    check(g_log.size() == mark + 1 &&
              logged(mark, "menus: no Chapter Select on this screen - not a story game (the game's mode 3, the return "
                           "reason 9, the top menu's choice 0)") == 1,
          "... logged once for the screen, with the mode, the reason and the choice");
    menus::chapter_select();
    menus::tick(g.run);
    check(pause_untouched(g) && !menus::view().going, "... asked there all the same: nothing written");
    g.sys.set32(kMode, 4);
    g.sys.set32(kReason, 1);
    menus::tick(g.run);
    check(!menus::view().offered && g_log.size() == mark + 1,
          "... nor in Desperate Escape (mode 4) - by its mode alone (the reason a story start's), the screen logged "
          "already");
    g.sys.set32(kMode, 1);
    g.main.set32(kOnline, 1);
    menus::tick(g.run);
    check(!menus::view().offered && !menus::view().why[0], "... nor in Versus (mode 1, online): no button, no why");
    g.main.set32(kOnline, 0);
    g.sys.set32(kMode, 0);
    g.sys.set32(kReason, 9);
    menus::tick(g.run);
    check(!menus::view().offered, "... nor with the extra episodes' return reason alone (the story's mode)");
    g.sys.set32(kReason, 1);
    g.sys.set32(kTop, 1);
    menus::tick(g.run);
    check(!menus::view().offered, "... nor in a game not started from the story's menu (another top menu choice)");
    g.sys.set32(kTop, 0);
    menus::tick(g.run);
    check(menus::view().offered && menus::view().ready && menus::view().where == menus::Where::kPause,
          "... back in a story game: offered and ready again");
    g.run.mercenaries = true;
    menus::tick(g.run);
    check(!menus::view().offered, "the Mercenaries' pause: not offered");
    g.run.mercenaries = false;
    menus::g_pause = false;
    menus::tick(g.run);
    check(!menus::view().offered, "the pause menu's QUIT not understood: not offered");
    menus::g_pause = true;
    menus::tick(g.run);
    check(menus::view().ready && !menus::leaving() && pause_untouched(g), "... ready again, nothing written so far");

    menus::chapter_select();
    menus::tick(g.run);
    check(g.pane32(kOption) == kQuitOption && g.pane8(kAskAt) == kAskStep && g.pause.f32(kPanes + kAskTimer) == 0.0f &&
              g.pane32(kAskAnswer) == 0 && g.pane8(kPaneAt) == kQuitWait,
          "asked at the pause: the pane's QUIT, its save question answered yes (its step, timer, answer), from the "
          "step waiting for the save manager");
    check(g.pause.u8(kNoCancel) == 1 && g.pause.u8(kPauseLeaving) == 1 && g.flow.u8(kFlowLeaving) == 1,
          "... the menu's no-cancel byte, its and the play flow's \"on its way\" set, as QUIT's yes sets them");
    check(menus::view().going && !menus::view().offered && menus::leaving() && g.sys.u32(kReason) == 1 &&
              g.sys.u32(kStory) == 2,
          "... on its way, the moves let go (leaving), nothing of the menus yet");
    // The game's steps: the save manager waited for, the question's yes - the save, which takes
    // its time (the pane still runs QUIT) - then the play flow quitting.
    for (int i = 0; i < menus::kQuitTicks + 60; ++i) menus::tick(g.run);
    check(menus::view().going, "... the save waited for while the pane runs QUIT, past the quit's own wait");
    g.quit_from_pause();
    for (int i = 0; i < 200; ++i) menus::tick(g.run);
    check(menus::view().going && menus::leaving(), "the play flow quitting (its fade out, the chapter let go): waited for");
    // The menus' area in while the chapter's characters are still let go: on its way still.
    g.areas({&g.front});
    for (int i = 0; i < 3; ++i) menus::tick(g.run);
    check(menus::view().going, "... the menus' area in, the characters not let go yet: on to the menus all the same");
    // The game ended; the menus as from Item Management.
    to_menus_loaded(g);
    check(g_menus_loaded && menus::view().going, "the game over, the menus' area started: their archive asked for and "
                                                 "loaded with the title's");
    g.fade_in();
    menus::tick(g.run);
    check(g.sys.u32(kReason) == 7 && g.sys.u32(kStory) == 13 && g.list.u8(kPicked) == 1 && g.top.u32(kSegment) == kPanel,
          "... the start-up menu handed over as it fades in, as from Item Management");
    g.startup.set8(kMenuStep, 2);
    g.startup.set8(kSub, 0);
    menus::tick(g.run);
    g.sys.set32(kReason, 0);
    g.startup.set8(kSub, 3);
    g.startup.set8(kStoryStep, 0x5A);
    menus::tick(g.run);
    check(g.startup.u8(kStoryStep) == 2 && g.startup.u8(kCsStep) == 0 && g.sys.u32(kStory) == 1 &&
              !menus::view().going && !menus::leaving(),
          "... CHAPTER SELECT at the end, done");
  }
  {
    reset();
    Game g;
    g.open_pause();
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    g.run.screen = game::Screen::kPlay;  // the pause closed, the chapter goes on
    g.run.pause_menu = 0;
    for (int i = 0; i < menus::kQuitTicks + 2; ++i) menus::tick(g.run);
    check(!menus::view().going && !menus::leaving() && menus::view().note[0],
          "the pause's QUIT not going through (the chapter going on): given up, said so");
  }
  {
    reset();
    Game g;
    g.open_pause();
    g.pause.set32(kPanes + kPanePlayer, 1);
    g_save_man_obj[kSaveSlots + kSaveSlot + kSaveCannot] = 1;  // player 1 may not save
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    check(g.pane32(kOption) == kQuitOption && g.pane8(kAskAt) == 0 && g.pane8(kPaneAt) == kQuitWait,
          "a player the save manager does not let save: the question from its first step (it goes on without it)");
  }

  // --- at the Game Over screen -----------------------------------------------------------------
  {
    reset();
    Game g;
    g.open_over();
    g.over.set8(kOverShown, 0);
    size_t mark = g_log.size();
    menus::tick(g.run);
    menus::tick(g.run);
    check(menus::view().offered && menus::view().where == menus::Where::kGameOver && !menus::view().ready,
          "the Game Over screen in a story chapter: offered there, not while its opening plays");
    check(g_log.size() == mark + 1 &&
              logged(mark, "menus: Chapter Select at the Game Over screen greyed: Wait for the Game Over screen's "
                           "choice. (shown 0, decided 0, its step 2; the return reason 1, the top menu's choice 0)") == 1,
          "... logged once: greyed, why, with the screen's steps");
    g.over.set8(kOverShown, 1);
    mark = g_log.size();
    menus::tick(g.run);
    menus::tick(g.run);
    check(menus::view().ready, "... ready once its menu is shown");
    check(g_log.size() == mark + 1 && logged(mark, "menus: Chapter Select at the Game Over screen ready (shown 1,") == 1,
          "... logged once as it changes: ready");
    g.over.set8(kOverStep, kOverElsewhere);
    menus::tick(g.run);
    check(!menus::view().ready, "... not at the step waiting for a partner's choice");
    g.over.set8(kOverStep, 2);
    g.over.set8(kOverDecided, 1);
    menus::tick(g.run);
    check(!menus::view().ready, "... nor once a choice is made");
    g.over.set8(kOverDecided, 0);
    g.run.mercenaries = true;
    menus::tick(g.run);
    check(!menus::view().offered, "the Mercenaries' Game Over: not offered");
    g.run.mercenaries = false;
    g.sys.set32(kMode, 4);
    g.sys.set32(kReason, 9);
    menus::tick(g.run);
    check(!menus::view().offered && menus::view().where == menus::Where::kNone,
          "an extra episode's Game Over (Desperate Escape: mode 4, the return reason 9): not offered (no window)");
    g.sys.set32(kMode, 0);
    g.sys.set32(kReason, 1);
    menus::g_over = false;
    menus::tick(g.run);
    check(!menus::view().offered, "the Game Over screen not understood: not offered");
    menus::g_over = true;
    g_mt_on = 1;  // the game uses its lock
    InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(g.main.at() + kMainLock));
    g.main.set32(kMainFlags, 0x800);
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    check(g.main.u32(kMainFlags) == (0x800 | kQuitFlag) && static_cast<int32_t>(g.over.u32(kOverChoice)) == -1 &&
              g.over.u8(kOverStep) == kOverElsewhere,
          "asked at the Game Over screen: QUIT's bit set (under the game's lock), the screen at the step taking the "
          "choice made elsewhere, the choice none");
    check(menus::view().going && menus::view().where == menus::Where::kGameOver && menus::leaving(),
          "... on its way from there");
    for (int i = 0; i < menus::kQuitTicks + 60; ++i) menus::tick(g.run);
    check(menus::view().going && menus::view().where == menus::Where::kGameOver,
          "... waited for while QUIT's bit is set (the game over's own steps, the checkpoint, the save), still from "
          "the Game Over screen (its window says so)");
    DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(g.main.at() + kMainLock));
    g_mt_on = 0;
    to_menus_loaded(g);
    g.fade_in();
    menus::tick(g.run);
    check(g.sys.u32(kReason) == 7 && g.sys.u32(kStory) == 13, "... then the start-up menu handed over as from Item "
                                                              "Management");
  }

  // --- at the chapter's results -----------------------------------------------------------------
  {
    reset();
    Game g;
    g.open_results();
    g.results.set8(kResStep, 3);  // still counting up
    size_t mark = g_log.size();
    menus::tick(g.run);
    g.results.set8(kResStep, 7);  // the save made, the records sent
    menus::tick(g.run);
    check(menus::view().offered && menus::view().where == menus::Where::kResults && !menus::view().ready,
          "the chapter's results: offered there, not before their menu");
    check(g_log.size() == mark + 1 &&
              logged(mark, "menus: Chapter Select at the chapter's results greyed: Wait for the results' menu. (their "
                           "step 3, their quit offered; the return reason 1, the top menu's choice 0)") == 1,
          "... logged once while it waits (their steps on, the same why), with their step");
    g.results.set8(kResStep, kResMenu);
    mark = g_log.size();
    menus::tick(g.run);
    menus::tick(g.run);
    check(menus::view().ready && !menus::view().last, "... ready at their menu, not the last chapter's");
    check(g_log.size() == mark + 1 && logged(mark, "menus: Chapter Select at the chapter's results ready (their step "
                                                   "10, their quit offered;") == 1,
          "... logged once as it changes: ready");
    g.results.set8(kResNoQuit, 1);  // the last chapter: their continue quits
    menus::tick(g.run);
    check(menus::view().ready && menus::view().last && !menus::view().why[0],
          "... ready at the last chapter's too (no quit of their own: their continue quits), said to be the last");
    g.results.set8(kResNoQuit, 0);
    g.results.set8(kResStep, 0x14);  // their own quit's question
    mark = g_log.size();
    menus::tick(g.run);
    g.results.set8(kResStep, kResMenu);  // answered no
    menus::tick(g.run);
    check(logged(mark, "results greyed: Wait for the results' menu. (their step 20,") == 1 &&
              logged(mark, "results ready (their step 10,") == 1 && g_log.size() == mark + 2,
          "... their own question and back: each change logged once");
    Obj next{0x330};  // the next chapter's results, at their menu as these are
    next.set8(kResStep, kResMenu);
    g.run.results = next.at();
    mark = g_log.size();
    menus::tick(g.run);
    g.run.results = g.results.at();
    menus::tick(g.run);
    check(g_log.size() == mark + 2 && logged(mark, "results ready (their step 10,") == 2,
          "... logged again for the next results, ready as these were");
    g.run.mercenaries = true;
    menus::tick(g.run);
    check(!menus::view().offered, "the Mercenaries' results: not offered");
    g.run.mercenaries = false;
    g.sys.set32(kMode, 3);
    g.sys.set32(kReason, 9);
    menus::tick(g.run);
    check(!menus::view().offered && menus::view().where == menus::Where::kNone,
          "an extra episode's results (Lost in Nightmares: mode 3, the return reason 9): not offered (no window)");
    g.sys.set32(kMode, 0);
    g.sys.set32(kReason, 1);
    menus::g_results = false;
    menus::tick(g.run);
    check(!menus::view().offered, "the results not understood: not offered");
    menus::g_results = true;
    menus::tick(g.run);
    menus::chapter_select();
    menus::tick(g.run);
    check(g.results.u8(kResQuit) == 1 && g.results.u32(kResWait) == 0 && g.results.f32(kResTimer) == 0.0f &&
              g.results.u32(kResPrompts) == kResPromptsOff && g.results.u8(kResStep) == kResQuitting,
          "asked at the results: their quit set, its wait and timer 0, the prompts off, the step after their "
          "question's yes");
    check(menus::view().going && menus::view().where == menus::Where::kResults && menus::leaving(),
          "... on its way from there");
    for (int i = 0; i < menus::kQuitTicks + 60; ++i) menus::tick(g.run);
    check(menus::view().going, "... waited for while their quit is set (the session let go, the save manager)");
    g.run.results = 0;  // done and let go: the play flow at its step after a game's end
    g.flow.set8(kFlowStep, kFlowAfter);
    for (int i = 0; i < menus::kQuitTicks + 60; ++i) menus::tick(g.run);
    check(menus::view().going, "... and while the play flow is at its step after a game's end");
    to_menus_loaded(g);
    g.fade_in();
    menus::tick(g.run);
    check(g.sys.u32(kReason) == 7 && g.sys.u32(kStory) == 13,
          "... then the start-up menu handed over as from Item Management");
  }

  // --- at the last chapter's results (6-3: their continue is their quit) --------------------------
  {
    reset();
    Game g;
    g.open_results();
    g.results.set8(kResNoQuit, 1);
    g.run.in_game = false;  // the ending played before them: no character left
    g.run.nchars = 0;
    g.run.screen = game::Screen::kNone;
    size_t mark = g_log.size();
    menus::tick(g.run);
    check(menus::view().offered && menus::view().ready && menus::view().last &&
              logged(mark, "menus: Chapter Select at the chapter's results ready (their step 10, the last chapter's "
                           "(no quit of their own);") == 1,
          "the last chapter's results (the characters gone): offered, ready, said to be the last - and logged so");
    mark = g_log.size();
    menus::chapter_select();
    menus::tick(g.run);
    check(g.results.u8(kResQuit) == 1 && g.results.u32(kResWait) == 0 && g.results.f32(kResTimer) == 0.0f &&
              g.results.u32(kResPrompts) == kResPromptsOff && g.results.u8(kResStep) == kResQuitting &&
              g.results.u8(kResNoQuit) == 1,
          "asked there: their quit set (as their continue sets it), its wait and timer 0, the prompts off, the step "
          "after their question's yes - the rest left alone");
    check(menus::view().going && !menus::view().last && menus::leaving() &&
              logged(mark, "menus: Chapter Select asked at the chapter's results (the last chapter's: as their "
                           "continue quits)") == 1,
          "... on its way, logged as the last chapter's");
    for (int i = 0; i < 30; ++i) menus::tick(g.run);
    check(menus::view().going, "... and on until the menus come (no character all the while)");
    g.run.results = 0;  // done and let go: the play flow at its step after a game's end
    g.flow.set8(kFlowStep, kFlowAfter);
    for (int i = 0; i < 30; ++i) menus::tick(g.run);
    to_menus_loaded(g);
    g.fade_in();
    menus::tick(g.run);
    check(g.sys.u32(kReason) == 7 && g.sys.u32(kStory) == 13,
          "... then the start-up menu handed over as from Item Management - the chapter list, not the main menu");
  }

  // --- the saved copies for the item box --------------------------------------------------
  {
    reset();
    Game g;
    g.open_pause();
    menus::Saves sv;
    check(menus::saves(&sv) && sv.state == kState && sv.work == kBlock && sv.copy == kBlockCopy &&
              sv.size == kBlockSize && sv.snapshot == g.snapshot.at(),
          "saves: the saved state, the working block and its copy, the play flow's copy of cGameInfo");
    g.flow.set32(kSnapshot, static_cast<uint32_t>(g.screen.at()));  // not a cGameInfo
    check(menus::saves(&sv) && sv.snapshot == 0, "... that copy only while it is a cGameInfo");
    g.areas({});
    check(menus::saves(&sv) && sv.snapshot == 0, "... none without the play flow");
    menus::g_saves = false;
    check(!menus::saves(&sv), "... nothing when the save data was not understood");
  }

  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  return g_fail ? 1 : 0;
}
