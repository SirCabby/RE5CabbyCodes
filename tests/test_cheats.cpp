// Offline test of Steady aim's hold through a scope, of the stages' countdowns, of
// Automatic QTEs' answer to a crank and to the volcano's boulder and of One hit kills on a Reaper and Uroboros
// Aheri (src/cheats.cpp, built in): a made-up character with its eight motion layers (the real offsets) and its scope
// flag, and a made-up sArea with its areas - the play flow and two stages laid out as
// chapter 3-2's escape and Desperate Escape's last wait are - run through the cheats' own
// tick; made-up prompts (a button manager's unit and its request, the real offsets) put to
// the action buttons' hook, and a crank and the boulder run as the game's code runs them; a made-up Reaper
// (its marks, action and health at the real offsets) hit through the hit check's hook. The
// discovery is not run: its results are set as the dump gives them (checked there by
// tools/sig.py and the operand reads).
// Build (no object of the mod's linked):
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Isrc -static tests/test_cheats.cpp
//       -o tests/build/test_cheats.exe
//   wine tests/build/test_cheats.exe
//
// - nothing is held while no scope is up, nor with the switch off or unavailable;
// - with a scope up, this machine's character's looping motions are held (speed 0),
//   a motion that plays once and an empty layer are not, nor a layer past those the
//   model runs; the AI's and the network partner's never;
// - a loop the game starts again, or speeds up again (a turn's steps), is held again;
//   a shot's motion (it plays once) put on a held layer runs and is not touched;
// - as the scope comes down, or the switch goes off, each held loop gets its speed
//   back - not one the game has replaced since;
// - a character that leaves play is forgotten without a write;
// - a stage's countdown is the panel's while its seconds are above what it runs down to
//   (0, or the stage's floor), not before it starts nor once it is out; an area of
//   another class, or one past the list's first null, is never taken for it;
// - Freeze countdown timer puts each stage's patch in and takes it out with the switch;
// - the countdown's edit is made only at the pause and with a countdown running, kept
//   above what it runs down to and within what the HUD shows;
// - a crank's Turn (the stick turned round, in the cranks' category) is pressed for this
//   machine's character every frame it is up, and not drawn; its Release, the AI's and
//   the network partner's, the switch off, the stick held to one side (a laser mirror's
//   Turn), the stick turned round in another category (the boulder's Push, AUTO's) and an
//   answered request go to the game's check; a grab's Break free is still pressed;
// - a crank held with nothing pressed, run as the game runs one (its ring of turns, its
//   speed, its most), at 60, 144 and 20 frames a second: wound to its most within a few
//   seconds, every slot of its ring marked (the game's top speed), its Turn never drawn,
//   then held there - the player not let go - until Release is pressed;
// - the volcano's boulder (category 8): its Hooks and its Ram hidden and pressed for this
//   machine's character once up 0.75 s, a press of the player's own at once; its Pushes
//   AUTO's (the game's check: hidden, pressed 2 s in); the AI's, the network partner's,
//   the switch off, the boulder not found and a Hook's label in another category the
//   game's check; the boulder run as the game runs it (its steps, points and limits, each
//   Hook's prompt made afresh) with nothing pressed, keyboard and pad, at 60, 144 and 20
//   frames a second: pushed over, every step begun, no prompt drawn, each punch's prompt
//   up 0.75 s at least; the switch off (AUTO alone): the Hooks wait, the push fails;
// - One hit kills on a Reaper (its check stood in for as the dump has it, the hit path's
//   tail and subtract-health with the mod's hooks in their places): the switch off, a shot
//   on its body takes nothing, one on its weak spot its damage, a grenade's blast down to
//   its last point; on, a shot anywhere on its body - and on its weak spot, and a blast -
//   takes all its health through the game's check's own yes, both marks cleared after;
//   hanging from the ceiling or holding a player it takes nothing (the next shot on its
//   feet kills it); a hit past its body's parts or one without damage is the game's;
// - One hit kills on Uroboros Aheri (its check and its cores' wear stood in for as the dump
//   has them, the hit path's tail and subtract-health with the mod's hooks in their places):
//   the switch off, a shot on an open core wears it, on a closed one only its flinch, in its
//   doom attack into the escape, on its main core its damage; on, a hit with damage on a core
//   breaks it - open, closed, flinching, the fourth, grown back, broken already - and nothing
//   else; in the doom attack one hit breaks the attack off and no core; its main core all its
//   health; a hit without damage or past its cores' parts is the game's;
// - Open map at chapter start over a made-up profile (its chapter's start at the real
//   offset), play state (cGameSys in sBioMain: its mode and each player's map byte) and
//   characters with their maps (uIdMap00: open, toggled by the key): nothing while no
//   chapter starts, nor as the chapter is set to begin and loads; as the chapter's start
//   is cleared (its title up) in the story, the map of this machine's character opened -
//   its byte in the play state and its own flag - not the AI's nor the network partner's;
//   a map the player has shut stays shut until the next chapter's start, which opens it
//   again; with the switch off nothing (and switching it on mid-chapter waits for the next
//   start); the start let go in the menus (no game) and then a game started without one -
//   nothing; the extra episodes' starts (Lost in Nightmares, Desperate Escape) opened as a
//   story chapter's; the Mercenaries', Reunion's and Versus' nothing; a map not set up yet, or not in the toggle
//   mode, or none, only the byte in the play state; a player's number past the bytes
//   nothing; the switch unavailable nothing.
#include "../src/cheats.cpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace re5cc {
bool g_verbose = false;
void logf(const char* fmt, ...) {
  if (!g_verbose) return;
  va_list ap;
  va_start(ap, fmt);
  std::printf("      log: ");
  std::vprintf(fmt, ap);
  std::printf("\n");
  va_end(ap);
}
}  // namespace re5cc

// --- what the cheats ask of the rest of the mod, made up ---------------------------------
namespace {
re5cc::game::Layout g_layout;
re5cc::config::Settings g_settings;
uintptr_t g_sarea = 0;       // the made-up sArea (sBioArea)
uintptr_t g_charas[4] = {};  // the characters in play, by their index
uintptr_t g_gameinfo = 0;    // the profile's cGameInfo (made up for Open map at chapter start)
}  // namespace
namespace re5cc::game {
std::vector<uintptr_t> sig_all(const char*) { return {}; }
uintptr_t sig_one(const char*) { return 0; }
uintptr_t call_target(uintptr_t) { return 0; }
bool ready() { return true; }
const Layout& layout() { return g_layout; }
uintptr_t system(const char* name) { return std::string(name) == "sBioArea" ? g_sarea : 0; }
uintptr_t character_at(int i) { return i >= 0 && i < 4 ? g_charas[i] : 0; }
void set_health(uintptr_t, int) {}
uintptr_t system_info() { return 0; }
uintptr_t game_info() { return g_gameinfo; }
}  // namespace re5cc::game
namespace re5cc::mt {
const Class* find(const char*) { return nullptr; }
const Class* classes(int* count) {
  *count = 0;
  return nullptr;
}
std::vector<uintptr_t> vtables_of(const Class*) { return {}; }
bool derives(uintptr_t, uintptr_t) { return false; }
bool is_a(uintptr_t, const Class* cls) { return cls != nullptr; }
const Class* by_id(uint32_t) { return nullptr; }
const char* class_name_of(uintptr_t) { return nullptr; }
}  // namespace re5cc::mt
namespace re5cc::image {
uintptr_t base() { return 0x400000; }
mem::Range text() { return {}; }
mem::Range rdata() { return {}; }
}  // namespace re5cc::image
namespace re5cc::merc {  // the Mercenaries (merc.cpp, tested in test_merc.cpp): not found here
bool discover_match() { return false; }
bool discover_kills() { return false; }
bool match_found() { return false; }
void set_weak_spot(const mt::Class*, int) {}
bool tick(const game::Run&, const Wanted&) { return false; }
void remove_patches() {}
}  // namespace re5cc::merc
namespace re5cc::config {
const Settings& get() { return g_settings; }
bool save_switches(const bool (&)[sw::kCount]) { return true; }
}  // namespace re5cc::config

// --- the made-up game ----------------------------------------------------------------------
namespace {
using namespace re5cc;

// The layout as the dump gives it (kSigScopeOpen/Close, kSigMotionSet/Loop/Clock).
constexpr uint32_t kScope = 0x2998, kLayersAt = 0x380, kLayerSize = 0xE0, kInUse = 0x374;
constexpr uint32_t kMotion = 0x4, kSpeed = 0x24, kAttr = 0xA, kOnce = 0x4;

struct Chara {
  std::vector<uint8_t> b = std::vector<uint8_t>(0x3000);
  uintptr_t at() { return reinterpret_cast<uintptr_t>(b.data()); }
  uintptr_t layer(int k) { return at() + kLayersAt + static_cast<uint32_t>(k) * kLayerSize; }
  void play(int k, uint16_t motion, float speed, uint16_t attr = 0) {
    mem::write<uint16_t>(layer(k) + kMotion, motion);
    mem::write<float>(layer(k) + kSpeed, speed);
    mem::write<uint16_t>(layer(k) + kAttr, attr);
  }
  float speed(int k) { return mem::read<float>(layer(k) + kSpeed); }
  void set_speed(int k, float s) { mem::write<float>(layer(k) + kSpeed, s); }
  void scope(bool up) { mem::write<uint8_t>(at() + kScope, up ? 1 : 0); }
};

// The aim through a scope, as the game has it: the aim's three loops and the
// stance's three, the hand's grip (a loop), the face's layer empty.
void aim(Chara& c) {
  mem::write<uint32_t>(c.at() + kInUse, 8);
  c.play(0, 0x51A, 1.0f);
  c.play(1, 0x51B, 1.0f);
  c.play(2, 0x519, 1.0f);
  c.play(3, 0x53D, 1.0f);
  c.play(4, 0x53E, 1.0f);
  c.play(5, 0x53C, 1.0f);
  c.play(6, 0x403, 1.0f);
  c.play(7, 0xFFFF, 1.0f);
}

game::Run run_of(std::vector<std::pair<Chara*, game::Control>> chars) {
  game::Run r;
  r.in_game = true;
  r.screen = game::Screen::kPlay;
  for (auto& [c, control] : chars) {
    game::Character& ch = r.chars[r.nchars];
    ch.obj = c->at();
    ch.index = r.nchars;
    ch.control = control;
    ch.name = r.nchars ? "Sheva" : "Chris";
    ++r.nchars;
  }
  return r;
}

int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

bool held(Chara& c, std::initializer_list<int> layers) {
  for (int k : layers)
    if (c.speed(k) != 0.0f) return false;
  return true;
}

// sArea and its areas as the dump gives them (kSigAreaFrame, kSigStageCountdown,
// kSigHudCountdown): the areas from +0x382C, eight at most, the first null ending them;
// an area's frame in its vtable's slot 7; a stage's seconds left at +0x650, what
// Desperate Escape's runs down to at +0x654; the HUD shows up to 5940 seconds. The
// frames are the dump's (aGame's, aS209's, aS704's): only compared, never run.
constexpr uint32_t kAreaList = 0x382C, kAreaCount = 8, kAreaFrame = 0x1C, kTime = 0x650, kFloor = 0x654;
constexpr float kMost = 5940.0f;
constexpr uintptr_t kFrameGame = 0x725E80, kFrame209 = 0x745240, kFrame704 = 0x760CE0;

struct Area {
  std::vector<uintptr_t> vtable = std::vector<uintptr_t>(32);
  std::vector<uint8_t> b = std::vector<uint8_t>(0x700);
  explicit Area(uintptr_t frame) {
    vtable[kAreaFrame / 4] = frame;
    mem::write<uintptr_t>(at(), reinterpret_cast<uintptr_t>(vtable.data()));
  }
  uintptr_t at() { return reinterpret_cast<uintptr_t>(b.data()); }
  float time() { return mem::read<float>(at() + kTime); }
  void set_time(float t) { mem::write<float>(at() + kTime, t); }
  void set_floor(float t) { mem::write<float>(at() + kFloor, t); }
};

// The action buttons as the dump gives them (kSigQteCheck, kSigQteType, kSigQteKind,
// kSigQteAuto): a button manager's unit holds its request at +0xC, its "drawn" flag at
// +5, its kind of input at +0x10; a request's "answered" flag is at +0xC, its button at
// +0x20, its category at +0x1C; who drives a character, +0x2DA8. A crank's prompts as
// its action makes them (FUN_00AB4790): Turn - category 5, the stick turned round (kind
// 7), button 0x1C - and Release - category 2, a press (kind 1), button 1.
constexpr uint32_t kUnitRequest = 0xC, kUnitDrawn = 5, kUnitKind = 0x10, kControl = 0x2DA8;
constexpr uint32_t kReqAnswered = 0xC, kReqType = 0x20, kReqCategory = 0x1C;
// ... a request's label at +0x24 and the seconds it has been up at +0x74 (kSigQteAuto,
// kSigQteAutoPush).
constexpr uint32_t kReqLabel = 0x24, kReqSeconds = 0x74;

struct Prompt {
  std::vector<uint8_t> unit = std::vector<uint8_t>(0x48), req = std::vector<uint8_t>(0x7C);
  Prompt(uint32_t category, uint32_t kind, uint32_t type, uint32_t label = 0) {
    mem::write<uintptr_t>(u() + kUnitRequest, r());
    mem::write<uint32_t>(u() + kUnitKind, kind);
    mem::write<uint32_t>(r() + kReqCategory, category);
    mem::write<uint32_t>(r() + kReqType, type);
    mem::write<uint32_t>(r() + kReqLabel, label);
  }
  uintptr_t u() { return reinterpret_cast<uintptr_t>(unit.data()); }
  uintptr_t r() { return reinterpret_cast<uintptr_t>(req.data()); }
  float up() { return mem::read<float>(r() + kReqSeconds); }
  void set_up(float s) { mem::write<float>(r() + kReqSeconds, s); }
};

// The game's check, stood in for: it draws the prompt it checks and answers what the
// player pressed - nothing, unless a test presses it. And AUTO's tail (FUN_00766F00 at
// 0x76714F; the mod makes "AUTO?" yes): the boulder's two Pushes (category 8, labels 0xB5
// and 0xC0) hidden, and pressed once up two seconds.
int g_game_checks = 0;
bool g_pressed = false;
bool __thiscall game_check(void* unit, void*, int) {
  ++g_game_checks;
  const auto u = reinterpret_cast<uintptr_t>(unit);
  mem::write<uint8_t>(u + kUnitDrawn, 1);
  const uintptr_t req = mem::read<uintptr_t>(u + kUnitRequest);
  const uint32_t label = mem::read<uint32_t>(req + kReqLabel);
  if (mem::read<uint32_t>(req + kReqCategory) == 0x08 && (label == 0xB5 || label == 0xC0)) {
    mem::write<uint8_t>(u + kUnitDrawn, 0);
    if (mem::read<float>(req + kReqSeconds) > 2.0f) return true;
  }
  return g_pressed;
}

// The managers' call of the check, the hook in its place (the unit as the manager leaves
// it: drawn): pressed, drawn, and whether the game's own check was asked.
struct Answer {
  bool pressed, drawn, game;
};
Answer ask(Prompt& p, int player) {
  const int before = g_game_checks;
  p.unit[kUnitDrawn] = 1;
  const bool pressed = cheats::hk_qte_check(reinterpret_cast<void*>(p.u()), nullptr, player);
  return {pressed, p.unit[kUnitDrawn] != 0, g_game_checks != before};
}

// A crank as the game runs one (uOmCrank: its ring FUN_00AB1840, its speed FUN_00AB2750,
// its winding FUN_00AB11A0, its Turn offered FUN_00AB2130): a Turn answered marks the
// slot of the moment; every thirtieth of a second (a slot a frame at most) the ring moves
// on, the new slot taking the mark; the crank winds 30 x its top speed x how full the
// ring is a second, up to its most; the Turn is offered while it is below it. A ring of
// 32, the keyboard's top speed (x1.5), a most of 90 (a winding motion of 60 frames: half
// of them x 6 x 0.5, FUN_00AB28F0).
struct Crank {
  std::vector<uint8_t> ring = std::vector<uint8_t>(32);
  size_t slot = 0;
  bool turned = false;
  float clock = 0, wound = 0, most = 90, top = 1.5f;
  bool offered() const { return wound < most; }
  float fill() const {
    int n = 0;
    for (uint8_t s : ring) n += s;
    return static_cast<float>(n) / static_cast<float>(ring.size());
  }
  void frame(float dt) {
    clock += dt;
    if (clock >= 1.0f / 30) {
      slot = (slot + 1) % ring.size();
      ring[slot] = turned;
      turned = false;
      clock -= 1.0f / 30;
    }
    wound = std::min(most, wound + 30 * dt * top * fill());
  }
};

// A frame of a crank held by character 0: its button manager asks the prompts it holds,
// the lower category first (Release, then Turn while it is offered), and answers the first
// one pressed - Release lets the crank go, Turn marks it; then the crank's own frame.
bool crank_frame(Crank& c, Prompt& release, Prompt& turn, float dt, bool* turn_drawn) {
  bool let_go = false;
  if (ask(release, 0).pressed) {
    let_go = true;
  } else if (c.offered()) {
    const Answer a = ask(turn, 0);
    if (a.drawn) *turn_drawn = true;
    if (a.pressed) c.turned = true;
  }
  c.frame(dt);
  return let_go;
}

// The volcano's boulder as the game runs it once the player has taken hold of it
// (uOm095e_Kick: its push's start FUN_00B2B620 with AUTO on, its frame FUN_00B28A30, its
// events FUN_00B28B70; the action set again started afresh, FUN_00B56480; a request's
// clock FUN_00BC30E0): step 2 Push (a button held, 30 points), 3 Push (the stick turned
// round, 90), 4 and 5 a Hook each (five), 6 the Ram, 7 the boulder over - 1 the push
// failed. A Push answered takes a point (five with a pad), a Hook one and makes the next
// Hook's prompt afresh, the Ram ends it. A step's limit (10 s; the Ram's 2 s) runs down
// while it has points left: out, the push fails. With AUTO on (the mod's), each Hook and
// the Ram are one plain press (kind 1, button 1).
struct Boulder {
  int step = 2, points = 0, hooks = 0, started = 0;  // started: a bit for each step begun
  float limit = 0, at = 0;     // the step's limit; the time since the push began
  float shortest = 1e9f;       // the shortest time a Hook's or the Ram's prompt was up as it was answered
  bool pad = false, restart = true, drawn = false;
  std::unique_ptr<Prompt> prompt;

  void set(int s) {
    step = s;
    restart = true;
  }
  void make(uint32_t label, uint32_t kind, uint32_t type) {
    prompt = std::make_unique<Prompt>(0x08, kind, type, label);
  }
  void start() {
    started |= 1 << step;
    switch (step) {
      case 2: points = 30, limit = 10, make(0xC0, 5, 0x16); break;
      case 3: points = 90, limit = 10, make(0xB5, 7, 0x1C); break;
      case 4: points = 5, limit = 10, make(0xB7, 1, 0x01); break;
      case 5: make(0xB7, 1, 0x01); break;
      case 6: points = 1, limit = 2, make(0xB6, 1, 0x01); break;
      default: prompt.reset();
    }
  }
  // A frame: the boulder's action set (again) started afresh - its prompt with it; the
  // request's clock and its manager's check (the hook); then the boulder's own frame.
  void frame(float dt) {
    at += dt;
    if (restart) {
      restart = false;
      start();
    }
    if (prompt) {
      prompt->set_up(prompt->up() + dt);
      const float up = prompt->up();
      const Answer a = ask(*prompt, 0);
      if (a.drawn) drawn = true;
      if (a.pressed) {
        if (step == 2 || step == 3) points -= pad ? 5 : 1;
        if (step == 4 || step == 5) points -= 1, ++hooks, set(5), shortest = std::min(shortest, up);
        if (step == 6) set(7), shortest = std::min(shortest, up);
      }
    }
    if (step >= 2 && step <= 6) {
      if (points > 0) limit -= dt;
      if (points <= 0 && step == 2) set(3);
      else if (points <= 0 && step == 3) set(4);
      else if (points <= 0 && (step == 4 || step == 5)) set(6);
      else if (limit <= 0) set(1);
    }
  }
};

// The Reaper as the dump gives it (kSigReaperHit, kSigReaperKill, kSigEnemySubtract): its
// two marks +0x1F60 and +0x1F61, its action +0x10E0 (2 hanging from the ceiling, 0x17
// holding a player), its body's parts below 60 and its weak spot's from 27 (three); its
// health +0x1364 and its most +0x1366, 10000 (its constructor's).
constexpr uint32_t kThrough = 0x1F60, kWhole = 0x1F61, kAction = 0x10E0, kHp = 0x1364, kHpMost = 0x1366;
constexpr uint32_t kHang = 2, kCatch = 0x17, kBody = 60;
constexpr int kWeak = 27;

struct Reaper {
  std::vector<uint8_t> b = std::vector<uint8_t>(0x1FB0);
  Reaper() {
    mem::write<int16_t>(at() + kHp, 10000);
    mem::write<int16_t>(at() + kHpMost, 10000);
  }
  uintptr_t at() { return reinterpret_cast<uintptr_t>(b.data()); }
  void* self() { return b.data(); }
  int hp() { return mem::read<int16_t>(at() + kHp); }
  void act(uint32_t a) { mem::write<uint32_t>(at() + kAction, a); }
  bool marked() { return b[kThrough] || b[kWhole]; }
};

// uEnemy's subtract-health (FUN_0084F7C0), stood in for: off the health, kept within 0 and
// its most.
void __thiscall game_subtract(void* self, int n) {
  const uintptr_t p = reinterpret_cast<uintptr_t>(self);
  const int hp = mem::read<int16_t>(p + kHp) - static_cast<int16_t>(n);
  mem::write<int16_t>(p + kHp, static_cast<int16_t>(std::clamp(hp, 0, static_cast<int>(mem::read<int16_t>(p + kHpMost)))));
}

// The Reaper's check (FUN_008A1CF0) as the dump has it: a part past its body's no, nor any
// while it hangs or holds a player (both marks cleared); its weak spot yes (cleared); any
// other part no without the first mark, yes with both, and with the first alone health to
// its last point at most - through subtract-health's slot, whose function is the mod's hook
// (as the hooked vtable has it) - and no. The marks as it found them are noted.
uint8_t g_found_through = 0, g_found_whole = 0;
bool __thiscall game_reaper_check(void* self, int part, int damage) {
  const uintptr_t p = reinterpret_cast<uintptr_t>(self);
  uint8_t& through = *reinterpret_cast<uint8_t*>(p + kThrough);
  uint8_t& whole = *reinterpret_cast<uint8_t*>(p + kWhole);
  g_found_through = through;
  g_found_whole = whole;
  const int at = static_cast<uint16_t>(part);
  const uint32_t action = mem::read<uint32_t>(p + kAction);
  if (at >= static_cast<int>(kBody) || action == kHang || action == kCatch) {
    through = whole = 0;
    return false;
  }
  if (at >= kWeak && at <= kWeak + 2) {
    through = whole = 0;
    return true;
  }
  if (!through) return false;
  through = 0;
  if (whole) {
    whole = 0;
    return true;
  }
  const int last = mem::read<int16_t>(p + kHp) - 1;
  cheats::hk_enemy_subtract(self, damage < last ? damage : last);
  return false;
}

// A hit on the Reaper as the hit path takes it (FUN_009DEA50): its check (the mod's hook in
// the vtable), then on a yes the hit's damage off its health through subtract-health (the
// mod's hook) - its death once none is left. Answers the check's yes.
bool hit_reaper(Reaper& r, int part, int damage) {
  if (!cheats::hk_reaper_hit(r.self(), part, damage)) return false;
  cheats::hk_enemy_subtract(r.self(), damage);
  return true;
}

// Uroboros Aheri as the dump gives it (kSigAheriHit, kSigCoreWear): its body's parts below 60,
// each core's 60 more; its flags +0x18DC (4: its doom attack), the doom attack's escape +0x2154
// (broken off past 2000); its cores +0x2104 (four), each one's flinch +0x211C and factor
// +0x1FE4, each one's health +0x1FC4 and most +0x1FC6 (8 apart; 7000, its constructor's);
// the frame's hits +0x1E2C; its body's two counters +0x212C (past 3000 a flinch) and +0x2130
// (past 12000 its main core hides); a core's flags +0x18DC (2 closed, 0x40 flinching); its
// health 28000 (its start's).
constexpr uint32_t kAheriParts = 60, kAheriFlags = 0x18DC, kAheriBeat = 4, kAheriEscape = 0x2154,
                   kAheriEscapeMost = 2000, kAheriCores = 0x2104, kAheriFlinch = 0x211C, kAheriFactor = 0x1FE4,
                   kAheriCoreHp = 0x1FC4, kAheriCoreStep = 8, kAheriHits = 0x1E2C, kAheriBodyHit = 0x212C,
                   kAheriBodyRun = 0x2130, kCoreFlags = 0x18DC, kCoreClosed = 2, kCoreFlinching = 0x40;

struct Aheri {
  std::vector<uint8_t> b = std::vector<uint8_t>(0x21E0);
  std::vector<uint8_t> core[4] = {std::vector<uint8_t>(0x1FC0), std::vector<uint8_t>(0x1FC0),
                                  std::vector<uint8_t>(0x1FC0), std::vector<uint8_t>(0x1FC0)};
  Aheri() {
    mem::write<int16_t>(at() + kHp, 28000);
    mem::write<int16_t>(at() + kHpMost, 28000);
    for (int i = 0; i < 4; ++i) {
      mem::write<int16_t>(at() + kAheriCoreHp + kAheriCoreStep * i, 7000);
      mem::write<int16_t>(at() + kAheriCoreHp + kAheriCoreStep * i + 2, 7000);
      mem::write<float>(at() + kAheriFactor + 4 * i, 1.0f);
      mem::write<uintptr_t>(at() + kAheriCores + 4 * i, reinterpret_cast<uintptr_t>(core[i].data()));
    }
  }
  uintptr_t at() { return reinterpret_cast<uintptr_t>(b.data()); }
  void* self() { return b.data(); }
  int hp() { return mem::read<int16_t>(at() + kHp); }
  int core_hp(int i) { return mem::read<int16_t>(at() + kAheriCoreHp + kAheriCoreStep * i); }
  void set_core_hp(int i, int hp) { mem::write<int16_t>(at() + kAheriCoreHp + kAheriCoreStep * i, static_cast<int16_t>(hp)); }
  uint32_t hits() { return mem::read<uint32_t>(at() + kAheriHits); }
  void frame() { mem::write<uint32_t>(at() + kAheriHits, 0); }  // uEnemy's frame clears them
  int escape() { return mem::read<int32_t>(at() + kAheriEscape); }
  int flinch(int i) { return mem::read<int32_t>(at() + kAheriFlinch + 4 * i); }
  void doom(bool on) { b[kAheriFlags] = static_cast<uint8_t>(on ? b[kAheriFlags] | kAheriBeat : b[kAheriFlags] & ~kAheriBeat); }
  void core_flags(int i, uint8_t f) { core[i][kCoreFlags] = f; }
};

// Its cores' wear (FUN_008BF0B0) as the dump has it - this machine hosting: the change off the
// core's health (a word), kept within 0 and its most; worn through below 1.
bool aheri_wear(uintptr_t p, int core, int change) {
  const uintptr_t h = p + kAheriCoreHp + kAheriCoreStep * core;
  int hp = mem::read<int16_t>(h) - static_cast<int16_t>(change);
  hp = std::clamp(hp, 0, static_cast<int>(mem::read<int16_t>(h + 2)));
  mem::write<int16_t>(h, static_cast<int16_t>(hp));
  return hp < 1;
}

// Its check (FUN_008C0CD0) as the dump has it: a body's part takes health (yes), counted
// towards a flinch and towards its main core's hiding; a core's says no - in the doom attack
// into the escape (past its most: broken off, 0x8000), else into the core's flinch (past 1500,
// 800 while it is closed, times its factor: 0x20 << core) and, the core open or flinching, its
// health; worn through, the core breaks (1 << core).
bool __thiscall game_aheri_check(void* self, int part, int damage) {
  const uintptr_t p = reinterpret_cast<uintptr_t>(self);
  const uint32_t at = static_cast<uint16_t>(part);
  auto add = [&](uint32_t field, int n) { mem::write<int32_t>(p + field, mem::read<int32_t>(p + field) + n); };
  auto mark = [&](uint32_t bits) { mem::write<uint32_t>(p + kAheriHits, mem::read<uint32_t>(p + kAheriHits) | bits); };
  if (at >= kAheriParts) {
    const int i = static_cast<int>(at / kAheriParts) - 1;
    if (i < 0 || i > 3) return false;
    if (mem::read<uint8_t>(p + kAheriFlags) & kAheriBeat) {
      add(kAheriEscape, damage);
      if (mem::read<int32_t>(p + kAheriEscape) > static_cast<int>(kAheriEscapeMost)) mark(0x8000);
      return false;
    }
    const uint8_t cf = mem::read<uint8_t>(mem::read<uintptr_t>(p + kAheriCores + 4 * i) + kCoreFlags);
    const bool closed = (cf & kCoreClosed) && !(cf & kCoreFlinching);
    const int limit = static_cast<int>((closed ? 800 : 1500) * mem::read<float>(p + kAheriFactor + 4 * i));
    add(kAheriFlinch + 4 * i, damage);
    if (!closed) aheri_wear(p, i, damage);
    if (aheri_wear(p, i, 0)) {
      mem::write<int32_t>(p + kAheriFlinch + 4 * i, 0);
      mark(1u << i);
      return false;
    }
    if (limit < mem::read<int32_t>(p + kAheriFlinch + 4 * i)) {
      mem::write<int32_t>(p + kAheriFlinch + 4 * i, 0);
      mark(0x20u << i);
    }
    return false;
  }
  add(kAheriBodyHit, damage);
  add(kAheriBodyRun, damage);
  if (mem::read<int32_t>(p + kAheriBodyRun) < 12001) {
    if (mem::read<int32_t>(p + kAheriBodyHit) > 3000) {
      mark(0x200);
      mem::write<int32_t>(p + kAheriBodyHit, 0);
    }
    return true;
  }
  mark(0x10000);
  mem::write<int32_t>(p + kAheriBodyRun, 0);
  return true;
}

// A hit on it as the hit path takes it (FUN_009DEA50): its check (the mod's hook in the
// vtable), then on a yes the damage off its health through subtract-health (the mod's hook).
bool hit_aheri(Aheri& a, int part, int damage) {
  if (!cheats::hk_aheri_hit(a.self(), part, damage)) return false;
  cheats::hk_enemy_subtract(a.self(), damage);
  return true;
}

// Open map at chapter start, as the dump gives it (kSigChapterStart, kSigMapRestore,
// kSigMapToggle, kSigMapMake): the profile's mTempBase.mChapterStart cGameInfo +0x41C4;
// cGameSys at sBioMain +0x1042C, its mode +0x58 (0 the story), each player's map open from
// +0x554, five bytes; a character's map uPlayer +0x264C, its player's number +0x2D9C (a
// byte); the map's open +0x1E, its toggle mode +0x1D.
constexpr uint32_t kChapterStart = 0x41C4, kSysField = 0x1042C, kMode = 0x58, kKept = 0x554, kPlayers = 5;
constexpr uint32_t kCharaMap = 0x264C, kCharaPlayer = 0x2D9C, kMapOpen = 0x1E, kMapToggles = 0x1D;

struct MapGame {
  std::vector<uint8_t> info = std::vector<uint8_t>(0x5940), main = std::vector<uint8_t>(0x10500),
                       sys = std::vector<uint8_t>(0x7A8), map0 = std::vector<uint8_t>(0x1B00),
                       map1 = std::vector<uint8_t>(0x1B00);
  Chara chris, sheva;
  MapGame() {
    mem::write<uintptr_t>(at(main) + kSysField, at(sys));
    // Each character's map as its start makes it: its player's number, the toggle mode set.
    mem::write<uint8_t>(chris.at() + kCharaPlayer, 0);
    mem::write<uint8_t>(sheva.at() + kCharaPlayer, 1);
    mem::write<uintptr_t>(chris.at() + kCharaMap, at(map0));
    mem::write<uintptr_t>(sheva.at() + kCharaMap, at(map1));
    map0[kMapToggles] = map1[kMapToggles] = 1;
  }
  static uintptr_t at(std::vector<uint8_t>& v) { return reinterpret_cast<uintptr_t>(v.data()); }
  void chapter_start(bool set) { mem::write<uint32_t>(at(info) + kChapterStart, set ? 1 : 0); }
  uint8_t kept(int player) { return sys[kKept + static_cast<uint32_t>(player)]; }
  // The game in play: Chris this machine's, Sheva the AI's (or as given); none out of a game.
  game::Run play(game::Control second = game::Control::kAi) {
    game::Run r = run_of({{&chris, game::Control::kLocal}, {&sheva, second}});
    r.main = at(main);
    return r;
  }
  game::Run menus() {
    game::Run r;
    r.main = at(main);
    return r;
  }
  // The map key, as the game's toggle makes it: the map's flag and its byte in the play state.
  void key(int player, std::vector<uint8_t>& map, bool open) {
    map[kMapOpen] = open;
    sys[kKept + static_cast<uint32_t>(player)] = open;
  }
};

}  // namespace

int main(int argc, char** argv) {
  using namespace re5cc;
  re5cc::g_verbose = argc > 1 && std::string(argv[1]) == "-v";
  cheats::init();
  // Discovery's results (the dump's), and the sway's patches found (the switch available).
  cheats::g_stance = cheats::Stance{kScope, kLayersAt, kLayerSize, kInUse, kMotion, kSpeed, kAttr, kOnce};
  cheats::g_stance_ok = true;
  cheats::g_ok[sw::kSteadyAim] = true;
  cheats::set(sw::kSteadyAim, true);

  Chara chris, sheva;
  aim(chris);
  aim(sheva);
  const game::Run both = run_of({{&chris, game::Control::kLocal}, {&sheva, game::Control::kAi}});

  cheats::tick(both);
  check(chris.speed(0) == 1.0f && chris.speed(3) == 1.0f, "no scope up: nothing held");

  chris.scope(true);
  sheva.scope(true);
  cheats::tick(both);
  check(held(chris, {0, 1, 2, 3, 4, 5, 6}), "a scope up: the aim's, the stance's and the grip's loops held");
  check(mem::read<uint16_t>(chris.layer(7) + kMotion) == 0xFFFF && chris.speed(7) == 1.0f,
        "... an empty layer left alone");
  check(sheva.speed(0) == 1.0f && sheva.speed(3) == 1.0f, "... the AI partner's never");

  // The game, during the frame: a turn's steps sped up on the stance's layers, the
  // aim's loop started over after a shot, a shot's motion (it plays once) on layer 1.
  chris.set_speed(3, 0.5f);
  chris.play(0, 0x51A, 1.0f);
  chris.play(1, 0x520, 1.0f, kOnce);
  cheats::tick(both);
  check(held(chris, {0, 3}), "a loop sped up again or started over: held again");
  check(chris.speed(1) == 1.0f, "a shot's motion (it plays once): left to run");

  // The game puts a loop of its own at speed 0 on layer 2 (as its own freeze does).
  chris.play(2, 0x5FF, 0.0f);
  cheats::tick(both);
  chris.scope(false);
  cheats::tick(both);
  check(chris.speed(0) == 1.0f && chris.speed(4) == 1.0f && chris.speed(5) == 1.0f && chris.speed(6) == 1.0f,
        "the scope down: each held loop's speed back");
  check(chris.speed(3) == 0.5f, "... the turn's steps at the speed they last had");
  check(chris.speed(1) == 1.0f && mem::read<uint16_t>(chris.layer(1) + kMotion) == 0x520,
        "... the shot's motion untouched");
  check(chris.speed(2) == 0.0f, "... a loop the game put on the layer since: not touched");
  cheats::tick(both);
  check(chris.speed(0) == 1.0f && chris.speed(3) == 0.5f, "... and nothing more once it is down");

  // The switch going off while the scope is up.
  aim(chris);
  chris.scope(true);
  cheats::tick(both);
  check(held(chris, {0, 1, 2, 3, 4, 5, 6}), "the scope up again: held");
  cheats::set(sw::kSteadyAim, false);
  cheats::tick(both);
  check(chris.speed(0) == 1.0f && chris.speed(2) == 1.0f && chris.speed(5) == 1.0f,
        "the switch off with the scope up: the speeds back");
  cheats::set(sw::kSteadyAim, true);

  // Unavailable (the sway's patches not found): nothing held.
  cheats::g_ok[sw::kSteadyAim] = false;
  cheats::tick(both);
  check(chris.speed(0) == 1.0f, "the switch unavailable: nothing held");
  cheats::g_ok[sw::kSteadyAim] = true;

  // The network partner's character, and the layers past those the model runs.
  const game::Run net = run_of({{&chris, game::Control::kNetwork}});
  cheats::tick(net);
  check(chris.speed(0) == 1.0f, "the network partner's character: never held");
  mem::write<uint32_t>(chris.at() + kInUse, 6);
  const game::Run mine = run_of({{&chris, game::Control::kLocal}});
  cheats::tick(mine);
  check(held(chris, {0, 5}) && chris.speed(6) == 1.0f, "a layer past those the model runs: not held");

  // A character that leaves play is forgotten: no write to it, nothing back later.
  const game::Run none = run_of({});
  chris.set_speed(5, 7.0f);  // the memory gone, someone else's now
  cheats::tick(none);
  check(chris.speed(0) == 0.0f && chris.speed(5) == 7.0f, "a character gone from play: no write to it");
  chris.scope(false);
  cheats::tick(mine);
  check(chris.speed(0) == 0.0f, "... and nothing given back to it later");

  // --- the stages' countdowns ---------------------------------------------------------------
  // Discovery's results: chapter 3-2's escape runs down to 0, Desperate Escape's last wait
  // to a floor of its own; each one's patch on code bytes of its own (the subtraction).
  std::vector<uint8_t> code = {0xF3, 0x0F, 0x5C, 0x40, 0x28, 0xCC, 0xF3, 0x0F, 0x5C, 0x40, 0x28, 0xCC};
  const uintptr_t sub209 = reinterpret_cast<uintptr_t>(code.data()), sub704 = sub209 + 6;
  cheats::StageClock s209, s704;
  s209.frame = kFrame209;
  s209.name = "aS209";
  s209.time = kTime;
  s704.frame = kFrame704;
  s704.name = "aS704";
  s704.time = kTime;
  s704.floor = kFloor;
  check(s209.frozen.prepare(sub209, "F3 0F 5C 40 28", mem::nops(5), "aS209") &&
            s704.frozen.prepare(sub704, "F3 0F 5C 40 28", mem::nops(5), "aS704"),
        "countdown: the stages' patches prepared");
  cheats::g_clocks = {s209, s704};
  static const mt::Class areas_class{};
  cheats::g_cls_areas = &areas_class;
  cheats::g_area_list = kAreaList;
  cheats::g_area_count = kAreaCount;
  cheats::g_area_frame = kAreaFrame;
  cheats::g_clock_most = kMost;
  cheats::g_ok[sw::kFreezeCountdown] = true;
  std::vector<uint8_t> sarea(kAreaList + 4 * kAreaCount);
  g_sarea = reinterpret_cast<uintptr_t>(sarea.data());
  Area flow(kFrameGame), escape(kFrame209), last_wait(kFrame704);
  const auto areas = [&](std::initializer_list<Area*> list) {
    std::fill(sarea.begin(), sarea.end(), 0);
    uint32_t i = 0;
    for (Area* a : list) mem::write<uintptr_t>(g_sarea + kAreaList + 4 * i++, a ? a->at() : 0);
  };
  game::Run play = run_of({{&chris, game::Control::kLocal}});
  game::Run paused = play;
  paused.screen = game::Screen::kPause;

  // The escape's stage loaded, its countdown not started yet (the stage's own 0).
  areas({&flow, &escape});
  flow.set_time(99.0f);  // the play flow's field there: not a countdown
  cheats::tick(play);
  check(!cheats::status().countdown, "countdown: none before the scene starts it (the play flow's never)");
  escape.set_time(240.5f);
  cheats::tick(play);
  cheats::Status st = cheats::status();
  check(st.countdown && st.countdown_left == 240.5f && st.countdown_most == kMost,
        "countdown: the escape's running - its seconds left, the HUD's most");

  // Freeze countdown timer: each stage's patch in with the switch, out without it.
  cheats::set(sw::kFreezeCountdown, true);
  cheats::tick(play);
  check(code[0] == 0x90 && code[4] == 0x90 && code[6] == 0x90 && code[10] == 0x90 && code[5] == 0xCC,
        "freeze: both stages' subtractions nops with the switch on");
  cheats::set(sw::kFreezeCountdown, false);
  cheats::tick(play);
  check(code[0] == 0xF3 && code[4] == 0x28 && code[6] == 0xF3 && code[10] == 0x28,
        "freeze: ... the game's bytes back with it off");

  // The edit: only at the pause, kept within what the countdown can be.
  cheats::edit(cheats::Edit::kCountdown, 0, 90);
  cheats::tick(play);
  check(escape.time() == 240.5f, "edit: not made in play");
  cheats::edit(cheats::Edit::kCountdown, 0, 90);
  cheats::tick(paused);
  check(escape.time() == 90.0f && cheats::status().countdown_left == 90.0f, "edit: at the pause, the seconds set");
  cheats::edit(cheats::Edit::kCountdown, 0, 0);
  cheats::tick(paused);
  check(escape.time() == 1.0f, "edit: never down to 0 (the scene's time-out) - 1 second at least");
  cheats::edit(cheats::Edit::kCountdown, 0, 100000);
  cheats::tick(paused);
  check(escape.time() == kMost, "edit: no more than the HUD shows");

  // Once it is out (or the scene zeroes it): no countdown, and no edit.
  escape.set_time(0.0f);
  cheats::tick(paused);
  check(!cheats::status().countdown, "countdown: out - none");
  cheats::edit(cheats::Edit::kCountdown, 0, 60);
  cheats::tick(paused);
  check(escape.time() == 0.0f, "edit: not made with no countdown running (it would start it again)");

  // Desperate Escape's runs down to a floor of its own: above it running, at it out.
  areas({&flow, &last_wait});
  last_wait.set_floor(30.0f);
  last_wait.set_time(31.0f);
  cheats::tick(paused);
  check(cheats::status().countdown && cheats::status().countdown_left == 31.0f,
        "floor: Desperate Escape's above its floor - running");
  cheats::edit(cheats::Edit::kCountdown, 0, 5);
  cheats::tick(paused);
  check(last_wait.time() == 31.0f, "floor: an edit kept above the floor (1 second at least)");
  last_wait.set_time(30.0f);
  cheats::tick(paused);
  check(!cheats::status().countdown, "floor: at its floor - out");

  // The list ends at its first null: an area after it is not looked at.
  escape.set_time(120.0f);
  areas({&flow, nullptr, &escape});
  cheats::tick(play);
  check(!cheats::status().countdown, "areas: none past the list's first null");
  areas({&flow, &last_wait, &escape});
  cheats::tick(play);
  check(cheats::status().countdown && cheats::status().countdown_left == 120.0f,
        "areas: the running one found among several");

  // --- Automatic QTEs: a crank's Turn ---------------------------------------------------------
  // Discovery's results (the dump's), the game's check stood in for, the switch available.
  cheats::g_qte = cheats::QteLayout{kUnitRequest, kUnitDrawn, kUnitKind, kReqAnswered, kReqType, kReqCategory,
                                   kReqLabel, kReqSeconds};
  cheats::g_boulder_ok = true;
  cheats::g_qte_check = &game_check;
  cheats::g_ok[sw::kAutoQte] = true;
  cheats::set(sw::kAutoQte, true);
  g_layout.control = kControl;
  Chara you, partner;
  mem::write<uint32_t>(you.at() + kControl, static_cast<uint32_t>(game::Control::kLocal));
  mem::write<uint32_t>(partner.at() + kControl, static_cast<uint32_t>(game::Control::kAi));
  g_charas[0] = you.at();
  g_charas[1] = partner.at();
  Prompt turn(0x05, 7, 0x1C), release(0x02, 1, 0x01);

  Answer a = ask(turn, 0);
  check(a.pressed && !a.drawn && !a.game, "crank: its Turn (the stick turned round) pressed for you, not drawn - "
                                          "the game's check not asked");
  bool every = true;
  for (int i = 0; i < 5; ++i) every = ask(turn, 0).pressed && every;
  check(every, "crank: ... pressed every frame it is up");
  a = ask(release, 0);
  check(!a.pressed && a.drawn && a.game, "crank: its Release - the game's check (yours to press)");
  a = ask(turn, 1);
  check(!a.pressed && a.game, "crank: the AI partner's Turn - the game's check (the AI's own turning)");
  mem::write<uint32_t>(partner.at() + kControl, static_cast<uint32_t>(game::Control::kNetwork));
  a = ask(turn, 1);
  check(!a.pressed && a.game, "crank: the network partner's Turn - the game's check");
  cheats::set(sw::kAutoQte, false);
  a = ask(turn, 0);
  check(!a.pressed && a.drawn && a.game, "crank: the switch off - the game's check");
  cheats::set(sw::kAutoQte, true);
  mem::write<uint8_t>(turn.r() + kReqAnswered, 1);
  a = ask(turn, 0);
  check(!a.pressed && a.game, "crank: a Turn answered already this frame - the game's check");
  mem::write<uint8_t>(turn.r() + kReqAnswered, 0);

  // The stick's other prompts are left as they were; a grab is still broken free of.
  Prompt mirror(0x08, 9, 0x1A), kick(0x08, 7, 0x1C, 0xB5), grab(0x00, 6, 0x16);
  mirror.set_up(5.0f);
  a = ask(mirror, 0);
  check(!a.pressed && a.game, "the stick held to one side (a laser mirror's Turn, the boulder's category) - the "
                              "game's check");
  a = ask(kick, 0);
  check(!a.pressed && a.game, "the stick turned round in another category (the boulder's Push, AUTO's) - the game's "
                              "check");
  a = ask(grab, 0);
  check(a.pressed && !a.drawn && !a.game, "a grab's Break free - still pressed");

  // A crank held and nothing pressed: wound to its most at the game's top speed, then held
  // there until Release.
  for (const float fps : {60.0f, 144.0f, 20.0f}) {
    Crank c;
    bool drawn = false, let_go = false;
    int full_at = -1;
    float fill = 0;  // how full its ring was as it reached its most
    const int frames = static_cast<int>(fps * 8);
    for (int f = 0; f < frames; ++f) {
      let_go = crank_frame(c, release, turn, 1.0f / fps, &drawn) || let_go;
      if (full_at < 0 && c.wound >= c.most) {
        full_at = f;
        fill = c.fill();
      }
    }
    char what[160];
    std::snprintf(what, sizeof what, "crank at %.0f fps: fully wound in %.2f s (its ring full: the game's top speed), "
                  "its Turn never drawn", fps, full_at < 0 ? -1.0f : static_cast<float>(full_at + 1) / fps);
    check(full_at >= 0 && full_at < static_cast<int>(fps * 4) && fill == 1.0f && !drawn, what);
    check(c.wound == c.most && !let_go, "... then held there with nothing pressed: not let go");
    g_pressed = true;
    check(crank_frame(c, release, turn, 1.0f / fps, &drawn), "... until Release is pressed (the game's check)");
    g_pressed = false;
  }

  // --- Automatic QTEs: the volcano's boulder ---------------------------------------------------
  // Its Hooks and its Ram (AUTO's one plain press each): hidden, pressed for you once up
  // 0.75 s; its Pushes AUTO's own.
  {
    Prompt hook(0x08, 1, 0x01, 0xB7), ram(0x08, 1, 0x01, 0xB6), push(0x08, 5, 0x16, 0xC0);
    a = ask(hook, 0);
    check(!a.pressed && !a.drawn && a.game, "boulder: a Hook just up - not drawn, not pressed yet (the game's check "
                                            "asked)");
    hook.set_up(0.74f);
    a = ask(hook, 0);
    check(!a.pressed && !a.drawn, "boulder: ... nor at 0.74 s");
    hook.set_up(0.75f);
    a = ask(hook, 0);
    check(a.pressed && !a.drawn, "boulder: ... pressed for you once up 0.75 s");
    ram.set_up(0.8f);
    a = ask(ram, 0);
    check(a.pressed && !a.drawn, "boulder: its Ram the same");
    hook.set_up(0.1f);
    g_pressed = true;
    a = ask(hook, 0);
    check(a.pressed && !a.drawn && a.game, "boulder: a press of your own still counts at once");
    g_pressed = false;
    push.set_up(1.0f);
    a = ask(push, 0);
    check(!a.pressed && !a.drawn && a.game, "boulder: its Push - AUTO's (the game's check), hidden, not pressed "
                                            "before 2 s");
    push.set_up(2.5f);
    a = ask(push, 0);
    check(a.pressed && a.game, "boulder: ... pressed by AUTO once up 2 s");
    hook.set_up(5.0f);
    mem::write<uint32_t>(partner.at() + kControl, static_cast<uint32_t>(game::Control::kAi));
    a = ask(hook, 1);
    check(!a.pressed && a.drawn && a.game, "boulder: the AI's Hook - the game's check (the AI's own presses)");
    mem::write<uint32_t>(partner.at() + kControl, static_cast<uint32_t>(game::Control::kNetwork));
    a = ask(hook, 1);
    check(!a.pressed && a.game, "boulder: the network partner's - the game's check");
    cheats::set(sw::kAutoQte, false);
    a = ask(hook, 0);
    check(!a.pressed && a.drawn && a.game, "boulder: the switch off - the game's check (yours to press)");
    cheats::set(sw::kAutoQte, true);
    cheats::g_boulder_ok = false;
    a = ask(hook, 0);
    check(!a.pressed && a.drawn && a.game, "boulder: AUTO's answer to its Pushes not as known - its Hooks the game's "
                                           "check");
    cheats::g_boulder_ok = true;
    Prompt other(0x19, 1, 0x01, 0xB7);
    other.set_up(5.0f);
    a = ask(other, 0);
    check(!a.pressed && a.drawn && a.game, "boulder: its Hook's label in another category - the game's check");
  }

  // The boulder taken hold of and nothing pressed, run as the game runs it: through to its
  // end, no prompt of it drawn, each punch's prompt up 0.75 s at least - with the keyboard
  // and a pad, at 60, 144 and 20 frames a second.
  for (const bool pad : {false, true}) {
    for (const float fps : {60.0f, 144.0f, 20.0f}) {
      Boulder b;
      b.pad = pad;
      for (int f = 0; f < static_cast<int>(fps * 30) && b.step != 7 && b.step != 1; ++f) b.frame(1.0f / fps);
      char what[200], end[48];
      if (b.step == 7) std::snprintf(end, sizeof end, "pushed over in %.2f s", b.at);
      else std::snprintf(end, sizeof end, "left at step %d after %.2f s", b.step, b.at);
      std::snprintf(what, sizeof what, "boulder (%s, %.0f fps): %s - both Pushes AUTO's, five Hooks (%d) and the "
                    "Ram, each up %.2f s at least, nothing drawn", pad ? "a pad" : "the keyboard", fps, end, b.hooks,
                    b.shortest > 1e8f ? 0.0f : b.shortest);
      check(b.step == 7 && b.started == 0x7C && b.hooks == 5 && !b.drawn && b.shortest >= 0.75f &&
                b.shortest < 0.9f && b.at > 2 + 2 + 6 * 0.75f && b.at < 20, what);
    }
  }
  // The switch off (the game's AUTO alone, as the build before had it): the Pushes go by
  // themselves, then the Hooks wait for a press - nothing pressed, the push fails at their
  // limit.
  cheats::set(sw::kAutoQte, false);
  {
    Boulder b;
    for (int f = 0; f < 60 * 30 && b.step != 7 && b.step != 1; ++f) b.frame(1.0f / 60);
    check(b.step == 1 && b.hooks == 0 && b.drawn, "boulder, the switch off: the Pushes AUTO's, the Hooks drawn and "
                                                  "waiting - nothing pressed, the push fails");
  }
  cheats::set(sw::kAutoQte, true);

  // --- One hit kills: the Reaper ---------------------------------------------------------------
  // Discovery's results (the dump's), the game's check and subtract-health stood in for.
  g_layout.hp = kHp;
  g_layout.hp_max = kHpMost;
  cheats::g_enemy_subtract = &game_subtract;
  cheats::g_reaper_hit = &game_reaper_check;
  cheats::g_reaper_parts = kBody;
  cheats::g_reaper_through = kThrough;
  cheats::g_reaper_whole = kWhole;
  cheats::g_ok[sw::kOneHitKills] = true;
  constexpr int kLeg = 12, kShot = 25;  // a leg's part; a shot's 250, cut to a tenth as the Reaper took it
  {
    Reaper r;
    cheats::set(sw::kOneHitKills, false);
    check(!hit_reaper(r, kLeg, kShot) && r.hp() == 10000 && !r.marked(),
          "Reaper, the switch off: a shot on its leg takes nothing (the game's own)");
    check(hit_reaper(r, kWeak, 250) && r.hp() == 9750, "Reaper, the switch off: ... one on its weak spot its damage");
    r.b[kThrough] = 1;  // the game's mark for a grenade's blast
    check(!hit_reaper(r, kLeg, 20000) && r.hp() == 1 && !r.marked(),
          "Reaper, the switch off: ... a grenade's blast to its last point, no further");
  }
  cheats::set(sw::kOneHitKills, true);
  {
    Reaper r;
    const bool yes = hit_reaper(r, kLeg, kShot);
    check(yes && g_found_through && g_found_whole && r.hp() == 0 && !r.marked(),
          "Reaper: a shot on its leg - both marks set for the game's check, which says yes: all its health, its death; "
          "the marks cleared");
  }
  {
    Reaper r;
    check(hit_reaper(r, 0, kShot) && r.hp() == 0, "Reaper: ... a shot on its body's first part as well");
  }
  {
    Reaper r;
    check(hit_reaper(r, kWeak + 2, kShot) && r.hp() == 0, "Reaper: ... one on its weak spot too");
  }
  {
    Reaper r;
    r.b[kThrough] = 1;  // the game's mark for a grenade's blast
    check(hit_reaper(r, kLeg, 400) && r.hp() == 0 && !r.marked(),
          "Reaper: ... a grenade's blast all of it, not to its last point");
  }
  {
    Reaper r;
    r.act(kHang);
    check(!hit_reaper(r, kLeg, kShot) && r.hp() == 10000 && !r.marked(),
          "Reaper: hanging from the ceiling it takes nothing (the game's rule), the marks cleared");
    r.act(kCatch);
    check(!hit_reaper(r, kWeak, kShot) && r.hp() == 10000 && !r.marked(),
          "Reaper: ... nor while it holds a player");
    r.act(0);
    check(hit_reaper(r, kLeg, kShot) && r.hp() == 0, "Reaper: ... back on its feet, the next shot kills it");
  }
  {
    Reaper r;
    check(!hit_reaper(r, static_cast<int>(kBody) + 3, kShot) && !g_found_through && !g_found_whole &&
              r.hp() == 10000,
          "Reaper: a hit on what it keeps (a part past its body's) - the game's check as it was, no mark set");
    check(!hit_reaper(r, kLeg, 0) && !g_found_through && !g_found_whole && r.hp() == 10000,
          "Reaper: a hit without damage - the game's check as it was");
  }

  // --- One hit kills: Uroboros Aheri -----------------------------------------------------------
  // Discovery's results (the dump's), its check stood in for.
  cheats::g_aheri_hit = &game_aheri_check;
  cheats::g_aheri = cheats::AheriLayout{kAheriParts, 4,          kAheriFlags,  kAheriBeat,
                                        kAheriEscape, kAheriEscapeMost, kAheriCoreHp, kAheriCoreStep};
  constexpr int kCore0 = 60 + 5, kCore1 = 120 + 2, kCore2 = 180 + 1, kCore3 = 240 + 3, kAheriBody = 5;
  {
    Aheri a;
    cheats::set(sw::kOneHitKills, false);
    check(!hit_aheri(a, kCore0, 250) && a.core_hp(0) == 6750 && !(a.hits() & 1) && a.hp() == 28000,
          "Uroboros Aheri, the switch off: a shot on an open core takes its damage off the core (the game's own)");
    a.core_flags(1, kCoreClosed);
    check(!hit_aheri(a, kCore1, 250) && a.core_hp(1) == 7000 && a.flinch(1) == 250 && !(a.hits() & 2),
          "Uroboros Aheri, the switch off: ... one on a closed core only towards its flinch");
    a.doom(true);
    check(!hit_aheri(a, kCore2, 250) && a.escape() == 250 && !(a.hits() & 0x8000) && a.core_hp(2) == 7000,
          "Uroboros Aheri, the switch off: ... in its doom attack a core's hit into the escape, not broken off");
    a.doom(false);
    check(hit_aheri(a, kAheriBody, 250) && a.hp() == 27750, "Uroboros Aheri, the switch off: ... its main core its damage");
  }
  cheats::set(sw::kOneHitKills, true);
  {
    Aheri a;
    const bool yes = hit_aheri(a, kCore0, 250);
    check(!yes && a.core_hp(0) == 0 && (a.hits() & 1) && a.core_hp(1) == 7000 && a.core_hp(2) == 7000 &&
              a.core_hp(3) == 7000 && a.hp() == 28000,
          "Uroboros Aheri: a shot on an open core breaks it (its health 0, the game's own break) - the other cores and "
          "its health as they were, the check's no");
    a.frame();
    a.core_flags(1, kCoreClosed);
    check(!hit_aheri(a, kCore1, 25) && a.core_hp(1) == 0 && (a.hits() & 2) && !(a.hits() & 1),
          "Uroboros Aheri: ... a closed one at one hit too");
    a.frame();
    a.core_flags(2, kCoreClosed | kCoreFlinching);
    check(!hit_aheri(a, kCore2, 25) && a.core_hp(2) == 0 && (a.hits() & 4), "Uroboros Aheri: ... a flinching one");
    a.frame();
    check(!hit_aheri(a, kCore3, 25) && a.core_hp(3) == 0 && (a.hits() & 8) && !(a.hits() & 7),
          "Uroboros Aheri: ... the fourth (its parts from 240)");
    a.frame();
    check(!hit_aheri(a, kCore0, 25) && a.core_hp(0) == 0 && (a.hits() & 1),
          "Uroboros Aheri: ... a broken core hit again: broken again, as the game has it");
    a.frame();
    a.set_core_hp(0, 4900);  // grown back, at 70% of its most
    check(!hit_aheri(a, 60, 25) && a.core_hp(0) == 0 && (a.hits() & 1),
          "Uroboros Aheri: ... grown back, broken again at one hit (its first part)");
    a.frame();
    a.set_core_hp(1, 7000);  // grown back
    check(!hit_aheri(a, kCore1, 0) && a.core_hp(1) == 7000 && !a.hits(),
          "Uroboros Aheri: a hit without damage - the game's check as it was (nothing worn, nothing broken)");
    check(!hit_aheri(a, 300 + 5, 25) && !a.hits() && a.core_hp(1) == 7000,
          "Uroboros Aheri: a part past its cores' - the game's (nothing)");
  }
  {
    Aheri a;
    a.doom(true);
    check(!hit_aheri(a, kCore2, 25) && (a.hits() & 0x8000) && a.escape() > static_cast<int>(kAheriEscapeMost) &&
              a.core_hp(2) == 7000 && a.hp() == 28000,
          "Uroboros Aheri: in its doom attack one hit on a core breaks it off (the escape past 2000) - no core broken");
    a.frame();
    check(!hit_aheri(a, kCore0, 25) && (a.hits() & 0x8000) && a.core_hp(0) == 7000,
          "Uroboros Aheri: ... and again at the next hit");
    a.doom(false);
  }
  {
    Aheri a;
    const bool yes = hit_aheri(a, kAheriBody, 250);
    check(yes && a.hp() == 0 && mem::read<int32_t>(a.at() + kAheriBodyRun) == 250 && a.core_hp(0) == 7000,
          "Uroboros Aheri: a hit on its main core - the check's yes, all its health (its death the game's), its "
          "counters given the hit's own damage, the cores untouched");
  }
  {
    Aheri a;
    cheats::set(sw::kOneHitKills, false);
    check(!hit_aheri(a, kCore0, 25) && a.core_hp(0) == 6975, "Uroboros Aheri: the switch off again - the game's own");
    cheats::set(sw::kOneHitKills, true);
  }

  // --- Open map at chapter start -----------------------------------------------------------------
  // Discovery's results (the dump's): the game's classes stood in for (the test's is_a takes
  // any object for a class found).
  {
    static const mt::Class map_class{}, sys_class{};
    cheats::g_map = cheats::MapLayout{kChapterStart, kSysField, kMode, kKept, kPlayers, kCharaMap, kCharaPlayer,
                                      kMapOpen, kMapToggles};
    cheats::g_cls_map = &map_class;
    cheats::g_cls_gamesys = &sys_class;
    cheats::g_ok[sw::kOpenMap] = true;
    cheats::set(sw::kOpenMap, true);
    MapGame g;
    g_gameinfo = MapGame::at(g.info);
    cheats::tick(g.play());
    cheats::tick(g.play());
    check(!g.kept(0) && !g.map0[kMapOpen], "map: nothing while no chapter starts (a CONTINUE mid-chapter, an area)");
    g.chapter_start(true);  // CHAPTER SELECT, NEW GAME, the next chapter: set to begin
    cheats::tick(g.menus());
    cheats::tick(g.play());  // the area loading, the characters made
    check(!g.kept(0) && !g.map0[kMapOpen], "map: nothing as the chapter is set to begin and loads");
    g.chapter_start(false);  // play begins: the chapter's title up, its start cleared
    cheats::tick(g.play());
    check(g.kept(0) == 1 && g.map0[kMapOpen] == 1,
          "map: the chapter's start cleared in the story - this machine's character's map opened (its byte in the "
          "play state, its own flag)");
    check(!g.kept(1) && !g.map1[kMapOpen], "map: ... not the AI partner's");
    g.key(0, g.map0, false);  // the player shuts it
    for (int i = 0; i < 5; ++i) cheats::tick(g.play());
    g.map0[kMapOpen] = g.kept(0);  // the next area's map, set up from the play state
    cheats::tick(g.play());
    check(!g.kept(0) && !g.map0[kMapOpen], "map: shut by the key - it stays shut through the chapter");
    g.chapter_start(true);  // the next chapter after the results
    cheats::tick(g.play());
    g.chapter_start(false);
    cheats::tick(g.play());
    check(g.kept(0) == 1 && g.map0[kMapOpen] == 1, "map: the next chapter's start opens it again");

    cheats::set(sw::kOpenMap, false);
    g.key(0, g.map0, false);
    g.chapter_start(true);
    cheats::tick(g.play());
    g.chapter_start(false);
    cheats::tick(g.play());
    check(!g.kept(0) && !g.map0[kMapOpen], "map: the switch off - a chapter's start leaves it as it is");
    cheats::set(sw::kOpenMap, true);
    for (int i = 0; i < 3; ++i) cheats::tick(g.play());
    check(!g.kept(0) && !g.map0[kMapOpen], "map: switched on mid-chapter - it waits for the next chapter's start");

    g.chapter_start(true);  // the menus set a start, then let it go (no game)
    cheats::tick(g.menus());
    g.chapter_start(false);
    cheats::tick(g.menus());
    cheats::tick(g.play());  // a game started without one (CONTINUE mid-chapter)
    check(!g.kept(0) && !g.map0[kMapOpen], "map: a start let go in the menus - nothing then, nor in the game after");

    // The other games' starts (their modes, cGameSys +0x58): the extra episodes begin as a
    // story chapter does (their start sets the field, the play flow clears it); the
    // Mercenaries' rounds set and clear it too, and are left alone, as Reunion's and Versus' are.
    const struct {
      uint32_t mode;
      bool opens;
      const char* what;
      const char* early;  // ... and nothing before play begins
      const char* stays;  // ... shut by the key, it stays shut
    } kStarts[] = {
        {2, false, "map: the Mercenaries' round starting - nothing (only a story chapter's or an extra episode's start)",
         nullptr, nullptr},
        {3, true, "map: Lost in Nightmares beginning - this machine's character's map opened, not the partner's",
         "map: Lost in Nightmares - nothing as it is set to begin and loads",
         "map: Lost in Nightmares - shut by the key, it stays shut through its areas"},
        {4, true, "map: Desperate Escape beginning - this machine's character's map opened, not the partner's",
         "map: Desperate Escape - nothing as it is set to begin and loads",
         "map: Desperate Escape - shut by the key, it stays shut through its areas"},
        {5, false, "map: a round of the Mercenaries Reunion starting - nothing", nullptr, nullptr},
        {1, false, "map: a game of Versus starting - nothing", nullptr, nullptr},
        {6, false, "map: a mode the mod does not know - nothing", nullptr, nullptr},
    };
    for (const auto& s : kStarts) {
      g.key(0, g.map0, false);
      mem::write<uint32_t>(MapGame::at(g.sys) + kMode, s.mode);
      g.chapter_start(true);
      cheats::tick(g.menus());  // the episode's start, from the story's menu
      cheats::tick(g.play());   // its first area loading, the characters made
      if (s.early) check(!g.kept(0) && !g.map0[kMapOpen], s.early);
      g.chapter_start(false);   // play begins: its start cleared
      cheats::tick(g.play());
      if (!s.opens) {
        check(!g.kept(0) && !g.map0[kMapOpen] && !g.kept(1) && !g.map1[kMapOpen], s.what);
        continue;
      }
      check(g.kept(0) == 1 && g.map0[kMapOpen] == 1 && !g.kept(1) && !g.map1[kMapOpen], s.what);
      g.key(0, g.map0, false);  // the player shuts it
      for (int i = 0; i < 5; ++i) cheats::tick(g.play());
      g.map0[kMapOpen] = g.kept(0);  // the next area's map, set up from the play state
      cheats::tick(g.play());
      check(!g.kept(0) && !g.map0[kMapOpen], s.stays);
    }
    mem::write<uint32_t>(MapGame::at(g.sys) + kMode, 0);

    g.map0[kMapToggles] = 0;  // a map not in the toggle mode (held while the key is)
    g.chapter_start(true);
    cheats::tick(g.play());
    g.chapter_start(false);
    cheats::tick(g.play());
    check(g.kept(0) == 1 && !g.map0[kMapOpen], "map: not in the toggle mode - only its byte in the play state");
    g.map0[kMapToggles] = 1;
    g.key(0, g.map0, false);
    mem::write<uintptr_t>(g.chris.at() + kCharaMap, 0);  // no map (yet)
    g.chapter_start(true);
    cheats::tick(g.play());
    g.chapter_start(false);
    cheats::tick(g.play());
    check(g.kept(0) == 1, "map: none made yet - its byte in the play state, which the map takes as it sets up");
    mem::write<uintptr_t>(g.chris.at() + kCharaMap, MapGame::at(g.map0));

    g.key(0, g.map0, false);
    g.sys[kKept + kPlayers] = 0x5A;  // the byte past the players'
    mem::write<uint8_t>(g.chris.at() + kCharaPlayer, kPlayers);
    g.chapter_start(true);
    cheats::tick(g.play());
    g.chapter_start(false);
    cheats::tick(g.play());
    check(g.sys[kKept + kPlayers] == 0x5A && !g.map0[kMapOpen], "map: a player's number past the bytes - nothing written");
    mem::write<uint8_t>(g.chris.at() + kCharaPlayer, 0);

    g.chapter_start(true);
    cheats::tick(g.play(game::Control::kNetwork));
    g.chapter_start(false);
    cheats::tick(g.play(game::Control::kNetwork));
    check(g.kept(0) == 1 && g.map0[kMapOpen] == 1 && !g.kept(1) && !g.map1[kMapOpen],
          "map: online - yours opened, the network partner's not");

    g.key(0, g.map0, false);
    cheats::g_ok[sw::kOpenMap] = false;
    g.chapter_start(true);
    cheats::tick(g.play());
    g.chapter_start(false);
    cheats::tick(g.play());
    check(!g.kept(0) && !g.map0[kMapOpen], "map: the switch unavailable (not found) - nothing");
    g_gameinfo = 0;
  }

  cheats::set(sw::kFreezeCountdown, true);
  cheats::tick(play);
  const bool frozen = code[0] == 0x90 && code[6] == 0x90;
  cheats::remove_hooks();
  check(frozen && code[0] == 0xF3 && code[4] == 0x28 && code[6] == 0xF3 && code[10] == 0x28,
        "unload: the stages' bytes the game's again");

  std::printf(g_fail ? "%d FAILED\n" : "all passed (0 failed)\n", g_fail);
  return g_fail ? 1 : 0;
}
