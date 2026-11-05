// Offline test of the Mercenaries' cheats (src/merc.cpp, built in): Endless combo, Combo Bonus always on and
// Kill enemies as they appear, over a made-up play state (cGameSys at the real offsets, its score slot's block),
// made-up enemies with their hit records, a made-up character with a gun in hand, the guns' table and the attacks'
// data, and a made-up scenario with its area tasks' boxes. The game's own code the cheats lean on is run here as the
// dump has it: a kill's scoring (FUN_0071CD10), the combo's clock and its end (FUN_0071C350, FUN_00717BD0), the
// Combo Bonus' clock and a pickup's start (FUN_00717C60, FUN_00718100) - each clock reading its own `subss` site, so
// the patches' bytes decide - and a box's test of a place (AHShapeBox's FUN_00BE6560). The discovery is not run: its
// results are set as the dump gives them (checked there by tools/sig.py and the operand reads).
// Build (no object of the mod's linked):
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Isrc -static tests/test_merc.cpp
//       -o tests/build/test_merc.exe
//   wine tests/build/test_merc.exe
//
// - out of a match (the story's mode, the Mercenaries' HUD not up), online, or in a game another machine runs:
//   nothing written, no patch in;
// - Endless combo off: a combo ends 10 s after its last kill and its bonus goes onto the score then; on: kills 15 s
//   apart keep one combo climbing, each kill's bonus onto the score as it is earned (nothing left waiting), the
//   combo's clock held where the kill set it; switched off, the combo runs out 10 s on with nothing left to bank;
// - Combo Bonus always on: nothing in the count-in; started at the clock's start (on, 30 s, no kills) and held there
//   through 100 s of play, its kills counted; a kill's bonus at least 1,000 from a combo's second; a pickup's
//   while on held too; switched off, 30 s of it left and then over; not started again once the match has ended
//   (the game's end turns it off) or with no time left;
// - Kill enemies as they appear: in play, the clock running, this machine's character up: after 12 frames in play,
//   each enemy's record holds a shot of the gun in hand (hit, its health as damage, the attack data's flags with
//   the whole-health bit, no part - a Reaper's weak spot for a Reaper -, the attack's number, the character as the
//   attacker, the character's own class - the game's hand-to-hand kill, +5 s; the gun's when that rule is not
//   found -, the place hit between the enemy and the shooter, the centre the enemy's place), a
//   chicken's too (every stage counts its chickens among its 150); not an enemy's part or a companion; at most 8 a
//   frame; one still standing hit again only after 400 ms,
//   and after 25 only every 5 s; a record holding a hit of the game's own left alone; the M92F's shot when a knife is
//   in hand; nothing at the pause (and 12 frames in play again after it), with the clock out, or with the character
//   down; the switch to be turned off as a match begins, begins again (a retry: play back to the count-in) and ends;
// - the Chainsaw Majini's second life, the game's rule run as the dump has it (the hit path lets it die only while
//   "may not die" is clear, its frame sets that until it has got up again, its brain lies it down - hits doing nothing
//   while it lies there - to get up with 2000): one with a second life to come dies at its first hit (the life and
//   that frame's "may not die" taken away with the hit), one without or that has got up already dies at its hit with
//   its bytes as they were, another class's bytes untouched; the second life not understood: it lies down and gets
//   up, and the hits every 5 s after the 25 kill it once it is up;
// - the stage's areas: while it acts each area task's box widened a thousandfold about its centre, from far below to
//   far above - a place far off is inside by the game's own test - but not a box with no extent nor a shape of
//   another kind; put back exactly the moment it stops (switched off, time up); a stage loaded anew (its tasks
//   replaced) never written into; a box changed since by the game forgotten.
#include "../src/merc.cpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>
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

// --- what merc.cpp asks of the rest of the mod, made up ------------------------------------
namespace {
using namespace re5cc;
game::Layout g_layout;
units::Layout g_units;
uintptr_t g_sunit = 0, g_scenario = 0;
std::vector<std::unique_ptr<mt::Class>> g_classes;
std::map<uintptr_t, const mt::Class*> g_class_of;  // a made-up object's class
std::map<uintptr_t, int> g_in_hand;                // a character's item in hand
uint64_t g_clock_ms = 1000;

const mt::Class* cls(const char* name, const mt::Class* parent = nullptr, uint32_t id = 0) {
  auto c = std::make_unique<mt::Class>();
  c->name = name;
  c->dti = 0x10000 + g_classes.size() * 0x40;
  c->parent = parent ? parent->dti : 0;
  c->id = id;
  c->size = 0x3000;
  c->vtable = c->dti + 8;
  g_classes.push_back(std::move(c));
  return g_classes.back().get();
}
}  // namespace

namespace re5cc::game {
std::vector<uintptr_t> sig_all(const char*) { return {}; }
uintptr_t sig_one(const char*) { return 0; }
uintptr_t call_target(uintptr_t) { return 0; }
const Layout& layout() { return g_layout; }
uintptr_t system(const char* name) {
  const std::string n = name;
  return n == "sUnit" ? g_sunit : n == "sScenario" ? g_scenario : 0;
}
bool item_in(uintptr_t chara, int slot, Item* out) {
  const auto it = g_in_hand.find(chara);
  if (it == g_in_hand.end() || slot != 0) return false;
  *out = Item{};
  out->id = it->second;
  out->equip = 1;
  return true;
}
const char* character_name(uintptr_t) { return "Chris"; }
}  // namespace re5cc::game
namespace re5cc::units {
const Layout& layout() { return g_units; }
const mt::Class* class_of(uintptr_t u) {
  const auto it = g_class_of.find(u);
  return it == g_class_of.end() ? nullptr : it->second;
}
}  // namespace re5cc::units
namespace re5cc::mt {
const Class* find(const char* name) {
  for (const auto& c : g_classes)
    if (std::string(c->name) == name) return c.get();
  return nullptr;
}
const Class* by_dti(uintptr_t dti) {
  for (const auto& c : g_classes)
    if (c->dti == dti) return c.get();
  return nullptr;
}
const Class* by_id(uint32_t id) {
  for (const auto& c : g_classes)
    if (c->id == id) return c.get();
  return nullptr;
}
bool derives(uintptr_t dti, uintptr_t ancestor) {
  for (const Class* c = by_dti(dti); c; c = by_dti(c->parent))
    if (c->dti == ancestor) return true;
  return false;
}
bool is_a(uintptr_t obj, const Class* c) {
  const auto it = g_class_of.find(obj);
  return c && it != g_class_of.end() && derives(it->second->dti, c->dti);
}
}  // namespace re5cc::mt
namespace re5cc::image {
uintptr_t base() { return 0x400000; }
mem::Range text() { return {}; }
mem::Range rdata() { return {}; }
mem::Range data() { return {}; }
}  // namespace re5cc::image

// --- the made-up game ----------------------------------------------------------------------
namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

struct Block {
  std::vector<uint8_t> b;
  explicit Block(size_t n) : b(n) {}
  uintptr_t at() { return reinterpret_cast<uintptr_t>(b.data()); }
};

// The play state's fields as the dump gives them (kSigMercCombo, kSigMercComboEnd, kSigMercBonusClock,
// kSigMercBonusStart, kSigMercClear, kSigMercFrame): cGameSys in sBioMain +0x1042C.
constexpr uint32_t kSys = 0x1042C, kMode = 0x58, kStep = 0x67C, kOnline = 0x56C, kHostA = 0x574, kHostB = 0x578,
                   kClock = 0x680, kKills = 0x58C, kBig = 0x79E, kComboOn = 0x698, kCombo = 0x69C, kBest = 0x6A0,
                   kComboClock = 0x6A4, kBonusOn = 0x6A8, kBonusClock = 0x6AC, kBonusKills = 0x6B0, kWaiting = 0x6B4,
                   kScore = 0x6B8;
// An enemy's hit record (kSigHitRebuild, kSigHitTaken, kSigWholeHealth, kSigEnemyParts): the cHit +0x1290, the record
// +0x129C, the parts +0x1BFC (4); a unit's place +0x30; health +0x1364, its most +0x1366.
constexpr uint32_t kRecord = 0x129C, kParts = 0x1BFC, kPos = 0x30, kHp = 0x1364, kHpMax = 0x1366;
constexpr uint32_t kRecPlace = 0, kRecCentre = 0x10, kRecAttacker = 0x20, kRecDamage = 0x28, kRecZero = 0x30,
                   kRecImpact = 0x34, kRecScale = 0x38, kRecFlags = 0x3C, kRecFlags2 = 0x40, kRecFlags3 = 0x44,
                   kRecHit = 0x48, kRecPart = 0x49, kRecB94 = 0x94, kRecF98 = 0x98, kRecAttack = 0x9C, kRecCls = 0xA4;
constexpr uint32_t kWhole = 0x10000000;
// The scenario (kSigAreaTasks, kSigTaskLoop, kSigAreaInside, kSigAreaHitFrame, kSigBoxInside).
constexpr uint32_t kLists = 0xD4, kAreaType = 6, kListCount = 8, kListItems = 0x14, kTaskArea = 8, kAreaShape = 0x14,
                   kBoxFloor = 0x60, kBoxHeight = 0x64, kBoxCorner = 0x20;

// Chris's class record's id, as the dump has it (uPl00Chris): a kill whose hit names it is his hand-to-hand one.
constexpr uint32_t kChris = 0x1E9D3E0B;
const mt::Class *c_chris, *c_sys, *c_enemy, *c_npc, *c_animal, *c_weapon, *c_em10, *c_reaper, *c_soft, *c_chicken, *c_josh,
    *c_m92f, *c_knife, *c_sscenario, *c_task, *c_area, *c_box, *c_cylinder, *c_hit_param, *c_chainsaw;

// The game's own scoring and clocks, as the dump has them (slot 0; mode 2's bonus table).
struct Play {
  Block main{kSys + 4}, sys{0x800};
  Block combo_site{8}, bonus_site{8};  // each clock's `subss` (the patches' sites)
  Play() {
    mem::write<uintptr_t>(main.at() + kSys, sys.at());
    g_class_of[sys.at()] = c_sys;
    const uint8_t combo[6] = {0xF3, 0x0F, 0x5C, 0x44, 0x24, 0x10}, bonus[6] = {0xF3, 0x0F, 0x5C, 0x44, 0x24, 0x0C};
    std::memcpy(combo_site.b.data(), combo, 6);
    std::memcpy(bonus_site.b.data(), bonus, 6);
    mem::write<uint32_t>(sys.at() + kMode, 2);
    mem::write<uint32_t>(sys.at() + kStep, 1);
    mem::write<float>(sys.at() + kClock, 120.0f);
  }
  uintptr_t s() { return sys.at(); }
  int i32(uint32_t o) { return mem::read<int32_t>(s() + o); }
  void set(uint32_t o, int v) { mem::write<int32_t>(s() + o, v); }
  bool on(uint32_t o) { return mem::read<uint8_t>(s() + o) != 0; }
  float f(uint32_t o) { return mem::read<float>(s() + o); }
  static bool frozen(Block& site) { return site.b[0] == 0x90; }
  // FUN_0071CD10: a kill worth `points`, column 0.
  void kill(int points) {
    set(kScore, i32(kScore) + points);
    set(kCombo, i32(kCombo) + 1);
    mem::write<float>(s() + kComboClock, 10.0f);
    mem::write<uint8_t>(s() + kComboOn, 1);
    static const int kAt[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 30, 40, 50};
    static const int kBonus[] = {0, 20, 50, 100, 200, 250, 300, 350, 400, 500, 600, 700, 850, 1000};
    int row = 0;
    while (row < 13 && i32(kCombo) >= kAt[row]) ++row;
    int bonus = kBonus[row];
    if (bonus > 0) {
      if (on(kBonusOn)) {
        if (bonus < 1000) bonus = 1000;
        set(kBonusKills, i32(kBonusKills) + 1);
      }
      set(kWaiting, i32(kWaiting) + bonus);
    }
    set(kKills, i32(kKills) + 1);
  }
  // FUN_0071C350 (and FUN_00717BD0 at 0).
  void combo_clock(float dt) {
    if (!on(kComboOn)) return;
    float t = f(kComboClock) - (frozen(combo_site) ? 0.0f : dt);
    if (t <= 0) {
      t = 0;
      if (i32(kCombo) > i32(kBest)) set(kBest, i32(kCombo));
      set(kCombo, 0);
      set(kScore, i32(kScore) + i32(kWaiting));
      set(kWaiting, 0);
      mem::write<uint8_t>(s() + kComboOn, 0);
    }
    mem::write<float>(s() + kComboClock, t);
  }
  // FUN_00717C60.
  void bonus_clock(float dt) {
    if (!on(kBonusOn)) return;
    float t = f(kBonusClock) - (frozen(bonus_site) ? 0.0f : dt);
    if (t <= 0) {
      t = 0;
      mem::write<uint8_t>(s() + kBonusOn, 0);
    }
    mem::write<float>(s() + kBonusClock, t);
  }
  // FUN_00718100: a Combo Bonus picked up.
  void pickup() {
    mem::write<uint8_t>(s() + kBonusOn, 1);
    mem::write<float>(s() + kBonusClock, 30.0f);
    set(kBonusKills, 0);
  }
};

void set_layouts(Play& p) {
  merc::Match m;
  m.sys = kSys;
  m.mode = kMode;
  m.step = kStep;
  m.online = kOnline;
  m.host_a = kHostA;
  m.host_b = kHostB;
  m.clock = kClock;
  m.kills = kKills;
  m.big = kBig;
  m.base = 150;
  m.extra = 250;
  m.combo_on = kComboOn;
  m.combo = kCombo;
  m.best = kBest;
  m.combo_clock = kComboClock;
  m.waiting = kWaiting;
  m.score = kScore;
  m.bonus_on = kBonusOn;
  m.bonus_clock = kBonusClock;
  m.bonus_kills = kBonusKills;
  m.bonus_time = 30.0f;
  merc::g_m = m;
  merc::g_cls_sys = c_sys;
  merc::g_match_ok = true;
  merc::g_combo_clock = mem::Patch{};
  merc::g_bonus_clock = mem::Patch{};
  merc::g_combo_clock.prepare(p.combo_site.at(), "F3 0F 5C 44 24 10", mem::nops(6), "combo clock");
  merc::g_bonus_clock.prepare(p.bonus_site.at(), "F3 0F 5C 44 24 0C", mem::nops(6), "Combo Bonus clock");
}

// The guns' table and the attacks' data: the M92F's entry (item 0x102) names attack 2, whose data (a HIT_PARAM) is
// the dump's - 0.1, 1.0, the flags 0x80000 and 0x200.
struct Arms {
  Block guns{0x140 * 0x50}, data{0x1C * 8}, groups{8 * 4}, hit_param_vt{16};
  Arms() {
    mem::write<uint16_t>(guns.at() + 0x140 * 2 + 8, 2);
    mem::write<uint16_t>(guns.at() + 0x140 * 1 + 8, 3);  // the knife's (never taken: no shot)
    const uintptr_t d = data.at() + 2 * 0x1C;
    mem::write<uintptr_t>(d, hit_param_vt.at());
    mem::write<uint32_t>(d + 4, 1);
    mem::write<float>(d + 8, 0.1f);
    mem::write<float>(d + 12, 1.0f);
    mem::write<uint32_t>(d + 0x10, 0x80000);
    mem::write<uint32_t>(d + 0x14, 0x200);
    mem::write<uint32_t>(d + 0x18, 0);
    mem::write<uintptr_t>(groups.at(), data.at());
    mem::write<uint32_t>(groups.at() + 4, 8);
  }
};

void set_hit_layout(Arms& a) {
  merc::HitLayout h;
  h.record = kRecord;
  h.chit = 0x1290;
  h.parts = kParts;
  h.nparts = 4;
  h.pos = kPos;
  h.place[0] = kRecPlace;
  h.place[1] = kRecPlace + 4;
  h.place[2] = kRecPlace + 8;
  h.centre[0] = kRecCentre;
  h.centre[1] = kRecCentre + 4;
  h.centre[2] = kRecCentre + 8;
  h.damage = kRecDamage;
  h.zero = kRecZero;
  h.impact = kRecImpact;
  h.scale = kRecScale;
  h.flags[0] = kRecFlags;
  h.flags[1] = kRecFlags2;
  h.flags[2] = kRecFlags3;
  h.hit = kRecHit;
  h.part = kRecPart;
  h.b94 = kRecB94;
  h.f98 = kRecF98;
  h.attack = kRecAttack;
  h.attacker = kRecAttacker;
  h.cls = kRecCls;
  h.def_impact = 8;
  h.def_flags[0] = 0x10;
  h.def_flags[1] = 0x14;
  h.def_flags[2] = 0x18;
  h.whole = kWhole;
  h.groups = a.groups.at();
  h.guns = a.guns.at();
  h.gun_stride = 0x140;
  h.gun_attack = 8;
  h.hit_param = a.hit_param_vt.at();
  merc::g_h = h;
  merc::g_cls_enemy = c_enemy;
  merc::g_cls_npc = c_npc;
  merc::g_cls_weapon = c_weapon;
  merc::g_not_shots = {mt::find("uWpKnife"),       mt::find("uWpStunrod"), mt::find("uWpBomb"),
                       mt::find("uWpMine"),        mt::find("uWpLauncher"), mt::find("uWpShell"),
                       mt::find("uWpShellCommon"), mt::find("uWpGunEmp")};
  merc::g_kill_ok = true;
  merc::g_melee_seconds = 5;  // kSigMeleeKind and kSigMeleeTime found, as in the dump
}

// A unit of sUnit's one line (state +4: 2 running; next +8), its record (0xE0 bytes) its own.
struct Unit {
  Block b{0x2200}, rec{0x100};
  Unit(const mt::Class* c, int hp, float x, float y, float z) {
    g_class_of[b.at()] = c;
    mem::write<uint32_t>(b.at() + 4, 2);
    mem::write<int16_t>(b.at() + kHp, static_cast<int16_t>(hp));
    mem::write<int16_t>(b.at() + kHpMax, static_cast<int16_t>(hp));
    mem::write<uintptr_t>(b.at() + kRecord, rec.at());
    mem::write<float>(b.at() + kPos, x);
    mem::write<float>(b.at() + kPos + 4, y);
    mem::write<float>(b.at() + kPos + 8, z);
  }
  uintptr_t at() { return b.at(); }
  uintptr_t r() { return rec.at(); }
  bool hit() { return mem::read<uint8_t>(r() + kRecHit) != 0; }
  void clear() { mem::write<uint8_t>(r() + kRecHit, 0); }  // the enemy's move, after its damage check
  void hp(int v) { mem::write<int16_t>(at() + kHp, static_cast<int16_t>(v)); }
};

struct Line {
  Block sunit{0x40};
  void set(std::vector<Unit*> us) {
    uintptr_t prev = 0;
    mem::write<uintptr_t>(sunit.at() + 0xC, 0);
    for (Unit* u : us) {
      mem::write<uintptr_t>(u->at() + 8, 0);
      if (prev) mem::write<uintptr_t>(prev + 8, u->at());
      else mem::write<uintptr_t>(sunit.at() + 0xC, u->at());
      prev = u->at();
    }
    g_sunit = sunit.at();
  }
};

game::Run run_of(Play& p, Unit* chara, game::Screen screen = game::Screen::kPlay) {
  game::Run r;
  r.main = p.main.at();
  r.in_game = true;
  r.mercenaries = true;
  r.screen = screen;
  if (chara) {
    r.nchars = 1;
    r.chars[0].obj = chara->at();
    r.chars[0].control = game::Control::kLocal;
    r.chars[0].hp = mem::read<int16_t>(chara->at() + kHp);
    r.chars[0].hp_max = 1000;
    r.chars[0].name = "Chris";
  }
  return r;
}

// The scenario: list 6 of area tasks, each with its cAreaHit and shape.
struct Shape {
  Block b{0x70};
  Shape(const mt::Class* c, float x0, float z0, float x1, float z1, float floor, float height) {
    g_class_of[b.at()] = c;
    // the corners in order round the quad: (x0,z0) (x0,z1) (x1,z1) (x1,z0)
    const float xs[4] = {x0, x0, x1, x1}, zs[4] = {z0, z1, z1, z0};
    for (int i = 0; i < 4; ++i) {
      mem::write<float>(b.at() + kBoxCorner + 16 * i, xs[i]);
      mem::write<float>(b.at() + kBoxCorner + 16 * i + 8, zs[i]);
    }
    mem::write<float>(b.at() + kBoxFloor, floor);
    mem::write<float>(b.at() + kBoxHeight, height);
  }
  uintptr_t at() { return b.at(); }
  float f(uint32_t o) { return mem::read<float>(at() + o); }
  // FUN_00BE6560: a place inside the box.
  bool inside(float x, float y, float z) {
    const uintptr_t s = at();
    auto F = [&](uint32_t o) { return mem::read<float>(s + o); };
    if (F(0x60) > y || y > F(0x64) + F(0x60)) return false;
    const float x0 = F(0x20), z0 = F(0x28), px = x - x0, pz = z - z0;
    const float ax = F(0x30) - x0, az = F(0x38) - z0, cx = F(0x50) - x0, cz = F(0x58) - z0;
    if (ax * pz > az * px || cz * px > cx * pz) return false;
    const float bx = F(0x40) - x0, bz = F(0x48) - z0;
    return (az - bz) * (px - bx) <= (ax - bx) * (pz - bz) && (cx - bx) * (pz - bz) <= (cz - bz) * (px - bx);
  }
};
struct Task {
  Block b{0x28}, area{0x80};
  explicit Task(Shape* s) {
    g_class_of[b.at()] = c_task;
    g_class_of[area.at()] = c_area;
    mem::write<uintptr_t>(b.at() + kTaskArea, area.at());
    mem::write<uintptr_t>(area.at() + kAreaShape, s ? s->at() : 0);
  }
};
struct Scenario {
  Block sys{0xF8}, list{0x20}, items{4 * 16};
  void set(std::vector<Task*> ts) {
    g_class_of[sys.at()] = c_sscenario;
    mem::write<uintptr_t>(sys.at() + kLists + 4 * kAreaType, list.at());
    mem::write<uint32_t>(list.at() + kListCount, static_cast<uint32_t>(ts.size()));
    mem::write<uintptr_t>(list.at() + kListItems, items.at());
    for (size_t i = 0; i < ts.size(); ++i) mem::write<uintptr_t>(items.at() + 4 * i, ts[i]->b.at());
    g_scenario = sys.at();
  }
};

void set_area_layout() {
  merc::AreaLayout a;
  a.lists = kLists;
  a.type = kAreaType;
  a.count = kListCount;
  a.items = kListItems;
  a.area = kTaskArea;
  a.shape = kAreaShape;
  a.floor = kBoxFloor;
  a.height = kBoxHeight;
  for (int i = 0; i < 4; ++i) {
    a.x[i] = kBoxCorner + 16 * i;
    a.z[i] = kBoxCorner + 16 * i + 8;
  }
  merc::g_a = a;
  merc::g_cls_scenario_sys = c_sscenario;
  merc::g_cls_task = c_task;
  merc::g_cls_area = c_area;
  merc::g_cls_box = c_box;
  merc::g_area_ok = true;
}

void classes() {
  const mt::Class* coord = cls("uCoord");
  c_sys = cls("cGameSys");
  const mt::Class* pawn = cls("uPawn", coord);
  c_enemy = cls("uEnemy", pawn);
  c_npc = cls("uEmNpc", c_enemy);
  c_animal = cls("uEmAnimal", c_enemy);
  c_em10 = cls("uEm10", c_enemy, 0x03001000);
  c_reaper = cls("uEm34", c_enemy, 0x03003400);
  c_soft = cls("uEm3401", c_enemy);
  c_chicken = cls("uEm62", c_animal);
  c_chainsaw = cls("uEm22", c_enemy, 0x03002200);
  c_josh = cls("uEm88Josh", c_npc);
  c_weapon = cls("uWeapon", pawn);
  for (const char* n : {"uWpStunrod", "uWpBomb", "uWpMine", "uWpLauncher", "uWpShell", "uWpShellCommon",
                        "uWpGunEmp"})
    cls(n, c_weapon);
  const mt::Class* knives = cls("uWpKnife", c_weapon);
  c_m92f = cls("uWp02", c_weapon, 0x07010200);
  c_knife = cls("uWp01", knives, 0x07010100);
  c_sscenario = cls("sScenario");
  c_task = cls("cScenario");
  c_area = cls("cAreaHit");
  const mt::Class* shapes = cls("AreaHitShape");
  c_box = cls("AHShapeBox", shapes);
  c_cylinder = cls("AHShapeCylinder", shapes);
  c_hit_param = cls("HIT_PARAM");
  c_chris = cls("uPl00Chris", cls("uPlayer", pawn), kChris);
}

void reset_merc() {
  merc::g_in_match = false;
  merc::g_last_step = -1;
  merc::forget_kills();
  merc::g_widened.clear();
  merc::g_logged.clear();
  merc::g_now = []() -> uint64_t { return g_clock_ms; };
}

// --- the tests -------------------------------------------------------------------------------
void test_combo() {
  std::printf("-- Endless combo\n");
  Play p;
  set_layouts(p);
  reset_merc();
  game::Run run = run_of(p, nullptr);
  merc::Wanted off, on;
  on.endless_combo = true;
  // Switched off: two kills 3 s apart, then the combo runs out 10 s after the last.
  merc::tick(run, off);
  p.kill(300);
  for (int i = 0; i < 3 * 60; ++i) p.combo_clock(1.0f / 60), merc::tick(run, off);
  p.kill(300);
  check(p.i32(kCombo) == 2 && p.i32(kWaiting) == 20 && p.i32(kScore) == 600, "off: two kills make a combo of 2, 20 waiting");
  for (int i = 0; i < 9 * 60; ++i) p.combo_clock(1.0f / 60), merc::tick(run, off);
  check(p.on(kComboOn), "off: still running 9 s after the last kill");
  for (int i = 0; i < 2 * 60; ++i) p.combo_clock(1.0f / 60), merc::tick(run, off);
  check(!p.on(kComboOn) && p.i32(kScore) == 620 && p.i32(kBest) == 2, "off: over at 10 s, its 20 onto the score");
  // On: kills 15 s apart keep one combo going, each bonus onto the score as it comes.
  merc::tick(run, on);
  check(merc::g_combo_clock.applied, "on: the combo clock's subss out");
  int want = p.i32(kScore);
  const int bonus[] = {0, 20, 50, 100, 200, 250};
  for (int k = 0; k < 6; ++k) {
    p.kill(300);
    want += 300 + bonus[k];
    for (int i = 0; i < 15 * 60; ++i) p.combo_clock(1.0f / 60), merc::tick(run, on);
  }
  check(p.on(kComboOn) && p.i32(kCombo) == 6, "on: kills 15 s apart - one combo of 6");
  check(p.i32(kScore) == want && p.i32(kWaiting) == 0, "on: each kill's bonus on the score as earned, none waiting");
  check(p.f(kComboClock) == 10.0f, "on: its clock held where the last kill set it");
  // Switched off: it runs out 10 s on with nothing left to bank.
  merc::tick(run, off);
  check(!merc::g_combo_clock.applied, "off again: the subss back");
  for (int i = 0; i < 11 * 60; ++i) p.combo_clock(1.0f / 60), merc::tick(run, off);
  check(!p.on(kComboOn) && p.i32(kScore) == want && p.i32(kBest) == 6, "off again: the combo over 10 s on, nothing more banked");
  // Not in a match of this machine's: nothing.
  p.kill(300);
  p.kill(300);
  const int waiting = p.i32(kWaiting);
  for (auto make : {+[](Play& q, game::Run& r) { mem::write<uint32_t>(q.s() + kMode, 0); (void)r; },
                    +[](Play& q, game::Run& r) { r.mercenaries = false; (void)q; },
                    +[](Play& q, game::Run& r) { mem::write<uint8_t>(q.s() + kOnline, 1); (void)r; },
                    +[](Play& q, game::Run& r) { mem::write<uint32_t>(q.s() + kHostA, 7); (void)r; }}) {
    game::Run r = run;
    make(p, r);
    merc::tick(r, on);
    check(!merc::g_combo_clock.applied && p.i32(kWaiting) == waiting,
          "the story's mode / no HUD / online / another machine's game: nothing done");
    mem::write<uint32_t>(p.s() + kMode, 2);
    mem::write<uint8_t>(p.s() + kOnline, 0);
    mem::write<uint32_t>(p.s() + kHostA, 0);
  }
  merc::tick(run, off);
}

void test_bonus() {
  std::printf("-- Combo Bonus always on\n");
  Play p;
  set_layouts(p);
  reset_merc();
  game::Run run = run_of(p, nullptr);
  merc::Wanted on, off;
  on.combo_bonus = true;
  mem::write<uint32_t>(p.s() + kStep, 0);
  merc::tick(run, on);
  check(!p.on(kBonusOn), "the count-in: not started yet");
  mem::write<uint32_t>(p.s() + kStep, 1);
  merc::tick(run, on);
  check(p.on(kBonusOn) && p.f(kBonusClock) == 30.0f && p.i32(kBonusKills) == 0, "the clock's start: started, 30 s, no kills");
  check(merc::g_bonus_clock.applied, "its clock's subss out");
  for (int s = 0; s < 100; ++s) {
    if (s % 10 == 0 && s < 80) p.kill(300);
    for (int i = 0; i < 60; ++i) p.bonus_clock(1.0f / 60), p.combo_clock(1.0f / 60), merc::tick(run, on);
  }
  check(p.on(kBonusOn) && p.f(kBonusClock) == 30.0f, "100 s of play: still on, held at 30 s");
  check(!p.on(kComboOn), "(the last combo over)");
  // Kills in a row: a combo's second and on earn at least 1,000.
  const int before = p.i32(kWaiting) + p.i32(kScore);
  p.kill(300);
  p.kill(300);
  p.kill(300);
  check(p.i32(kWaiting) + p.i32(kScore) - before == 900 + 1000 + 1000, "three kills in a row: 1,000 each from the second");
  check(p.i32(kBonusKills) >= 2, "its kills counted");
  // A pickup while on: held too.
  p.pickup();
  for (int i = 0; i < 60 * 40; ++i) p.bonus_clock(1.0f / 60), merc::tick(run, on);
  check(p.on(kBonusOn) && p.f(kBonusClock) == 30.0f, "a pickup's while on: held");
  // Off: 30 s left, then over.
  merc::tick(run, off);
  check(!merc::g_bonus_clock.applied, "off: its subss back");
  for (int i = 0; i < 29 * 60; ++i) p.bonus_clock(1.0f / 60), merc::tick(run, off);
  check(p.on(kBonusOn), "off: still on 29 s later");
  for (int i = 0; i < 2 * 60; ++i) p.bonus_clock(1.0f / 60), merc::tick(run, off);
  check(!p.on(kBonusOn), "off: over at 30 s, as a pickup's");
  // The match's end turns it off; it is not started again - nor with no time left.
  merc::tick(run, on);
  mem::write<uint32_t>(p.s() + kStep, 3);
  mem::write<uint8_t>(p.s() + kBonusOn, 0);
  merc::tick(run, on);
  check(!p.on(kBonusOn), "the match's end: not started again");
  mem::write<uint32_t>(p.s() + kStep, 1);
  mem::write<float>(p.s() + kClock, 0.0f);
  merc::tick(run, on);
  check(!p.on(kBonusOn), "no time left: not started");
  merc::tick(run, off);
}

bool shot_in(Unit& e, Unit& chara, int part, uint32_t cls_id) {
  const uintptr_t r = e.r();
  const float ex = mem::read<float>(e.at() + kPos), cx = mem::read<float>(chara.at() + kPos);
  const float px = mem::read<float>(r + kRecPlace);
  const struct {
    bool ok;
    const char* what;
  } fields[] = {
      {e.hit(), "hit"},
      {mem::read<int32_t>(r + kRecDamage) == mem::read<int16_t>(e.at() + kHp), "damage"},
      {mem::read<uint32_t>(r + kRecZero) == 0, "zero"},
      // (x87 evaluates a float constant at long double precision: a tolerance, not ==)
      {std::fabs(mem::read<float>(r + kRecImpact) - 0.1f) < 1e-6f, "impact"},
      {mem::read<float>(r + kRecScale) == 1.0f, "scale"},
      {mem::read<uint32_t>(r + kRecFlags) == (0x80000 | kWhole), "flags"},
      {mem::read<uint32_t>(r + kRecFlags2) == 0x200, "flags2"},
      {mem::read<uint32_t>(r + kRecFlags3) == 0, "flags3"},
      {mem::read<uint8_t>(r + kRecPart) == part, "part"},
      {mem::read<uint8_t>(r + kRecB94) == 0, "b94"},
      {mem::read<float>(r + kRecF98) == 0.0f, "f98"},
      {mem::read<uint32_t>(r + kRecAttack) == 2, "attack"},
      {mem::read<uintptr_t>(r + kRecAttacker) == chara.at(), "attacker"},
      {mem::read<uint32_t>(r + kRecCls) == cls_id, "class"},
      {mem::read<float>(r + kRecCentre) == ex, "centre"},
      {px != ex && (px - ex) * (cx - ex) > 0 && std::abs(px - ex) < std::abs(cx - ex) * 0.1f, "place"},
  };
  for (const auto& f : fields)
    if (!f.ok) {
      std::printf("      (the record's %s is not as a shot's)\n", f.what);
      return false;
    }
  return true;
}

void test_kill() {
  std::printf("-- Kill enemies as they appear\n");
  Play p;
  set_layouts(p);
  Arms arms;
  set_hit_layout(arms);
  merc::set_weak_spot(c_reaper, 27);
  reset_merc();
  Unit chara(c_chris, 1000, 0, 0, 0);
  g_in_hand[chara.at()] = 0x102;
  Unit majini(c_em10, 400, 1000, 0, 0), reaper(c_reaper, 10000, -2000, 0, 500), soft(c_soft, 500, -2000, 0, 500);
  Unit chicken(c_chicken, 10, 300, 0, 300), josh(c_josh, 500, 50, 0, 50);
  mem::write<uintptr_t>(reaper.at() + kParts, soft.at());
  Line line;
  line.set({&majini, &reaper, &soft, &chicken, &josh});
  game::Run run = run_of(p, &chara);
  merc::Wanted on, off;
  on.kill = true;
  // Entering a match with the switch on: to be switched off (it lasts one match).
  merc::g_in_match = false;
  check(merc::tick(run, on), "a match begun with the switch on: switched off");
  merc::g_in_match = true;
  for (int i = 0; i < 11; ++i) merc::tick(run, on);
  check(!majini.hit(), "the first 11 frames in play: nothing yet");
  merc::tick(run, on);
  merc::tick(run, on);
  check(shot_in(majini, chara, 0xFF, kChris),
        "a Majini: a shot of the M92F in its record (no part), its class Chris's own - a hand-to-hand kill (+5 s)");
  check(shot_in(reaper, chara, 27, kChris), "a Reaper: the same on its weak spot");
  check(shot_in(chicken, chara, 0xFF, kChris), "a chicken (an animal, counted in a stage's 150): the same");
  check(!soft.hit() && !josh.hit(), "not the Reaper's soft body (its part) or Josh (a companion)");
  // The enemy's move takes it and clears it; one still standing is hit again only after 400 ms.
  majini.clear();
  merc::tick(run, on);
  check(!majini.hit(), "standing: not again at once");
  g_clock_ms += 401;
  merc::tick(run, on);
  check(majini.hit(), "standing: again 400 ms on");
  // A hit of the game's own waiting: left alone.
  majini.clear();
  mem::write<uint8_t>(majini.r() + kRecHit, 1);
  mem::write<int32_t>(majini.r() + kRecDamage, 77);
  g_clock_ms += 401;
  merc::tick(run, on);
  check(mem::read<int32_t>(majini.r() + kRecDamage) == 77, "a hit of the game's own: not written over");
  // After 25 still standing: only every 5 s from then on.
  auto tries_of = [](Unit& u) {
    for (const merc::Target& t : merc::g_targets)
      if (t.unit == u.at()) return t.tries;
    return -1;
  };
  for (int i = 0; i < 100 && tries_of(majini) < 25; ++i) {
    majini.clear();
    reaper.clear();
    g_clock_ms += 401;
    merc::tick(run, on);
  }
  majini.clear();
  g_clock_ms += 401;
  merc::tick(run, on);
  check(tries_of(majini) == 25 && !majini.hit() && merc::g_slow >= 1, "25 hits and still standing: not again 400 ms on");
  g_clock_ms += 4000;
  merc::tick(run, on);
  check(!majini.hit(), "... nor 4.4 s on");
  g_clock_ms += 600;
  merc::tick(run, on);
  check(majini.hit() && tries_of(majini) == 26, "... but 5 s on, and so on while it stands");
  // Dead: dropped; a new one hit.
  majini.hp(0);
  reaper.hp(0);
  chicken.hp(0);
  merc::tick(run, on);
  check(merc::g_targets.empty(), "dead ones dropped");
  // Eight a frame.
  std::vector<std::unique_ptr<Unit>> many;
  std::vector<Unit*> us;
  for (int i = 0; i < 10; ++i) {
    many.push_back(std::make_unique<Unit>(c_em10, 300, 100.0f * i + 50, 0, 900));
    us.push_back(many.back().get());
  }
  line.set(us);
  merc::tick(run, on);
  int n = 0;
  for (Unit* u : us) n += u->hit();
  check(n == 8, "ten enemies: eight hit this frame");
  merc::tick(run, on);
  n = 0;
  for (Unit* u : us) n += u->hit();
  check(n == 10, "... the other two the next");
  // A knife in hand: the M92F's shot.
  for (Unit* u : us) u->clear();
  g_in_hand[chara.at()] = 0x101;
  g_clock_ms += 401;
  merc::tick(run, on);
  check(shot_in(*us[0], chara, 0xFF, kChris), "a knife in hand: the M92F's shot, still a hand-to-hand kill");
  // The hand-to-hand rule not found: the gun's class (a shot's kill, no time).
  for (Unit* u : us) u->clear();
  merc::g_melee_seconds = 0;
  g_in_hand[chara.at()] = 0x102;
  g_clock_ms += 401;
  merc::tick(run, on);
  check(shot_in(*us[0], chara, 0xFF, 0x07010200), "the hand-to-hand rule not found: the gun's class (a shot's kill)");
  merc::g_melee_seconds = 5;
  // The pause: nothing, and 12 frames in play again after it.
  for (Unit* u : us) u->clear();
  g_clock_ms += 401;
  merc::tick(run_of(p, &chara, game::Screen::kPause), on);
  check(!us[0]->hit(), "the pause: nothing");
  for (int i = 0; i < 12; ++i) merc::tick(run, on);
  check(!us[0]->hit(), "after the pause: not in the first 12 frames");
  merc::tick(run, on);
  check(us[0]->hit(), "... then again");
  // The clock out, the character down: nothing.
  for (Unit* u : us) u->clear();
  g_clock_ms += 401;
  mem::write<float>(p.s() + kClock, 0.0f);
  merc::tick(run, on);
  check(!us[0]->hit(), "the clock out: nothing");
  mem::write<float>(p.s() + kClock, 60.0f);
  chara.hp(0);
  merc::tick(run_of(p, &chara), on);
  check(!us[0]->hit(), "the character down: nothing");
  chara.hp(1000);
  // A retry (play back to the count-in): a match anew, to be switched off.
  mem::write<uint32_t>(p.s() + kStep, 0);
  check(merc::tick(run, on), "a retry (back to the count-in): switched off");
  mem::write<uint32_t>(p.s() + kStep, 1);
  check(!merc::tick(run, on), "... the count-in to play: the same match");
  // The match over: to be switched off.
  mem::write<uint32_t>(p.s() + kStep, 2);
  check(merc::tick(run, on), "the match over (time up): switched off");
  check(!merc::tick(run, off), "... and nothing more to switch");
}

// The Chainsaw Majini's second life as the dump has it (kSigChainsawLife, kSigChainsawDown, kSigDeathGate): uEm22's
// bytes +0x1F5F (it may get up again) and +0x1F5E (it has), uEnemy's +0x1E38 (it may not die), the health it gets up
// with +0x1F5C (2000).
constexpr uint32_t kMayGetUp = 0x1F5F, kGotUp = 0x1F5E, kNoDeath = 0x1E38, kUpHealth = 0x1F5C;
constexpr uint64_t kLyingMs = 11000;  // its lie-down: the motion, then 8 s; then it gets up

void set_life_layout() {
  merc::SecondLife s;
  s.may = kMayGetUp;
  s.got_up = kGotUp;
  s.no_death = kNoDeath;
  s.up_health = kUpHealth;
  s.lie = 0x20;
  merc::g_life = s;
  merc::g_cls_chainsaw = c_chainsaw;
  merc::g_life_ok = true;
}

// A Chainsaw Majini's frame, the game's: the hit path first (FUN_009DEA50: the whole health, then a death only while
// "may not die" is clear - else 1 left - with the frame before's flag, the order that needs the mod's clear of it);
// its brain lying it down at 1 (FUN_009848A0, with its second life to come; the lie-down's start: got up, 2000) and
// doing nothing with hits while it lies there (they are cleared, as the enemy's move does); then uEnemy's frame
// clearing "may not die" (0x9E0A61) and uEm22's own (FUN_0087B280) setting it again until it has got up.
struct Chainsaw {
  Unit u;
  uint64_t lying_until = 0;
  Chainsaw(int hp, float x, float z, bool may, bool got_up) : u(c_chainsaw, hp, x, 0, z) {
    mem::write<uint8_t>(u.at() + kMayGetUp, may ? 1 : 0);
    mem::write<uint8_t>(u.at() + kGotUp, got_up ? 1 : 0);
    mem::write<int16_t>(u.at() + kUpHealth, 2000);
    frame();  // a frame of its own before the mod's first
  }
  uint8_t b(uint32_t o) { return mem::read<uint8_t>(u.at() + o); }
  int hp() { return mem::read<int16_t>(u.at() + kHp); }
  bool lying() { return g_clock_ms < lying_until; }
  void frame() {
    if (u.hit() && !lying()) {
      if (mem::read<uint32_t>(u.r() + kRecFlags) & kWhole) u.hp(0);
      if (hp() <= 0 && b(kNoDeath)) u.hp(1);
      if (hp() == 1 && b(kMayGetUp) && !b(kGotUp)) {
        mem::write<uint8_t>(u.at() + kGotUp, 1);
        u.hp(mem::read<int16_t>(u.at() + kUpHealth));
        lying_until = g_clock_ms + kLyingMs;
      }
    }
    u.clear();
    mem::write<uint8_t>(u.at() + kNoDeath, 0);
    if (!b(kGotUp) && b(kMayGetUp)) mem::write<uint8_t>(u.at() + kNoDeath, 1);
  }
};

void test_second_life() {
  std::printf("-- the Chainsaw Majini's second life\n");
  Play p;
  set_layouts(p);
  Arms arms;
  set_hit_layout(arms);
  set_life_layout();
  reset_merc();
  Unit chara(c_chris, 1000, 0, 0, 0);
  g_in_hand[chara.at()] = 0x102;
  Chainsaw two(6500, 800, 0, true, false), one(6500, -800, 0, false, false), up(2000, 0, 800, true, true);
  Unit majini(c_em10, 400, 0, 0, -800);
  mem::write<uint8_t>(majini.at() + kMayGetUp, 1);  // another class's bytes at those places
  mem::write<uint8_t>(majini.at() + kNoDeath, 1);
  check(two.b(kNoDeath) && !one.b(kNoDeath) && !up.b(kNoDeath),
        "(the game's frame: may not die only for the one with a second life to come)");
  Line line;
  line.set({&two.u, &one.u, &up.u, &majini});
  game::Run run = run_of(p, &chara);
  merc::g_in_match = true;
  merc::Wanted on;
  on.kill = true;
  for (int i = 0; i < 13; ++i) merc::tick(run, on);
  check(two.u.hit() && !two.b(kMayGetUp) && !two.b(kNoDeath),
        "a second life to come: taken away with its hit, and this frame's may not die");
  check(one.u.hit() && !one.b(kMayGetUp) && !one.b(kGotUp) && up.u.hit() && up.b(kMayGetUp) && up.b(kGotUp),
        "one without, and one that has got up again: its hit, its bytes as they were");
  check(majini.hit() && mem::read<uint8_t>(majini.at() + kMayGetUp) == 1 && mem::read<uint8_t>(majini.at() + kNoDeath) == 1,
        "another class: those bytes not touched");
  two.frame();
  one.frame();
  up.frame();
  check(two.hp() == 0 && !two.b(kGotUp) && !two.lying(), "... the one with a second life dead at its first hit: no lying down");
  check(one.hp() == 0 && up.hp() == 0, "... the other two dead at theirs");
  check(merc::g_lives == 1, "one life taken this match");
  // The second life not understood: it lies down at the first hit and gets up with 2000 - the hits while it lies
  // there do nothing, the 25 run out, and the ones every 5 s after kill it once it is up.
  merc::g_life_ok = false;
  reset_merc();
  Chainsaw again(6500, 800, 0, true, false);
  line.set({&again.u});
  merc::g_in_match = true;
  for (int i = 0; i < 13; ++i) merc::tick(run, on);
  check(again.u.hit(), "not understood: its hit");
  again.frame();
  check(again.hp() == 2000 && again.b(kGotUp) && again.lying(), "... it lies down, to get up with 2000");
  bool dead = false;
  int ms = 0;
  for (; ms < 30000 && !dead; ms += 100) {
    g_clock_ms += 100;
    merc::tick(run, on);
    again.frame();
    dead = again.hp() <= 0;
  }
  check(dead && ms > static_cast<int>(kLyingMs) && merc::g_slow == 1,
        "... the 25 hits run out while it lies there; one of those every 5 s kills it once it is up");
  merc::g_life_ok = true;
}

void test_areas() {
  std::printf("-- the stage's areas\n");
  Play p;
  set_layouts(p);
  Arms arms;
  set_hit_layout(arms);
  set_area_layout();
  reset_merc();
  Unit chara(c_chris, 1000, 0, 0, 0);
  Line line;
  line.set({});
  Shape a(c_box, 100, 100, 600, 400, -50, 300), b(c_box, -900, -900, -700, -600, 1000, 200);
  Shape flat(c_box, 5, 5, 5, 5, 0, 1917), round(c_cylinder, 0, 0, 10, 10, 0, 10);
  Task ta(&a), tb(&b), tflat(&flat), tround(&round), tnone(nullptr);
  Scenario sc;
  sc.set({&ta, &tb, &tflat, &tround, &tnone});
  const std::vector<uint8_t> was_a = a.b.b, was_b = b.b.b, was_flat = flat.b.b, was_round = round.b.b;
  game::Run run = run_of(p, &chara);
  merc::g_in_match = true;
  merc::Wanted on, off;
  on.kill = true;
  check(!a.inside(5000, 0, -3000) && !b.inside(0, 5000, 0), "before: far places outside both boxes");
  merc::tick(run, on);
  check(a.f(kBoxFloor) == -1.0e7f && a.f(kBoxHeight) == 2.0e7f && b.f(kBoxFloor) == -1.0e7f,
        "on: the boxes from far below to far above");
  check(a.f(kBoxCorner) == 350 + (100 - 350) * 1000.0f && a.f(kBoxCorner + 8) == 250 + (100 - 250) * 1000.0f,
        "on: the corners a thousandfold about the centre");
  check(a.inside(5000, 0, -3000) && b.inside(0, 5000, 0) && a.inside(350, 0, 250),
        "on: far places inside by the game's own test");
  check(flat.b.b == was_flat && round.b.b == was_round, "a box with no extent and a cylinder: untouched");
  check(merc::status().areas == 2, "the panel: 2 areas open");
  merc::tick(run, on);
  check(merc::g_widened.size() == 2, "each widened once");
  merc::tick(run, off);
  check(a.b.b == was_a && b.b.b == was_b, "off: both put back exactly");
  // Time up: put back too.
  merc::tick(run, on);
  mem::write<uint32_t>(p.s() + kStep, 2);
  merc::tick(run, on);
  check(a.b.b == was_a && b.b.b == was_b, "time up: put back");
  mem::write<uint32_t>(p.s() + kStep, 1);
  merc::g_in_match = true;
  // The stage loaded anew: the old boxes never written.
  merc::tick(run, on);
  Shape c(c_box, 0, 0, 50, 50, 0, 100);
  Task tc(&c);
  sc.set({&tc});
  const std::vector<uint8_t> a_now = a.b.b;
  merc::tick(run, on);
  check(a.b.b == a_now && c.f(kBoxFloor) == -1.0e7f, "a new stage: its box widened, the old ones not written");
  merc::tick(run, off);
  check(a.b.b == a_now && c.f(kBoxFloor) == 0.0f, "off: the new one put back, the old ones still not written");
  // A box the game changed since: forgotten.
  merc::tick(run, on);
  mem::write<float>(c.at() + kBoxHeight, 7.0f);
  merc::tick(run, off);
  check(c.f(kBoxHeight) == 7.0f && merc::g_widened.empty(), "a box changed by the game: forgotten, not written");
}

}  // namespace

int main(int argc, char** argv) {
  re5cc::g_verbose = argc > 1 && std::string(argv[1]) == "-v";
  g_layout.hp = kHp;
  g_layout.hp_max = kHpMax;
  g_units.stride = 0x20;
  g_units.line_flags = 0x8;
  g_units.line_head = 0xC;
  g_units.unit_flags = 4;
  g_units.unit_next = 8;
  g_units.lines = 1;
  classes();
  InitializeCriticalSection(&merc::g_lock);  // discover_match() makes it in the mod
  merc::g_lock_ready = true;
  test_combo();
  test_bonus();
  test_kill();
  test_second_life();
  test_areas();
  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  return g_fail ? 1 : 0;
}
