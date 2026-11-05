#include "merc.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "image.h"
#include "log.h"
#include "mem.h"
#include "mt.h"
#include "units.h"

namespace re5cc::merc {
namespace {

uint32_t u8at(uintptr_t a) { return mem::read<uint8_t>(a); }
uint32_t u32at(uintptr_t a) { return mem::read<uint32_t>(a); }

// The Mercenaries' two modes in the play state: The Mercenaries and Reunion - the compares
// kSigMercClear fixes (`cmp ecx,2; je; cmp ecx,5`). The match's steps: the count-in, play,
// time up or the stage cleared (the players' results asked for), the end.
constexpr uint32_t kModeMercenaries = 2, kModeReunion = 5;
constexpr int kStepCountIn = 0, kStepPlay = 1;

// --- the match ------------------------------------------------------------------------------
// The play state (cGameSys) as the Mercenaries' own code uses it. The combo: every kill
// adds one and sets its clock to 10 seconds; the clock runs down each frame and at 0 the
// combo ends - what it earned (a bonus for every kill from the second, by how long the combo
// is) goes onto the score then. The Combo Bonus (a pickup) runs 30 seconds, and while it
// runs every kill's bonus is at least 1000.
struct Match {
  uint32_t sys = 0;                 // sBioMain: the play state (cGameSys)
  uint32_t mode = 0, step = 0;      // cGameSys: the game's mode, the match's step
  uint32_t online = 0;              // cGameSys: an online game (a byte)
  uint32_t host_a = 0, host_b = 0;  // cGameSys: equal where this machine runs the match
  uint32_t clock = 0;               // cGameSys: the clock the HUD shows (a float)
  uint32_t kills = 0;               // cGameSys: the stage's kills
  uint32_t big = 0;                 // cGameSys: a byte - the target is base + extra
  int base = 0, extra = 0;          // the stage's kill target
  // Slot 0's block (the team's in both modes), from cGameSys:
  uint32_t combo_on = 0, combo = 0, best = 0, combo_clock = 0, waiting = 0, score = 0;
  uint32_t bonus_on = 0, bonus_clock = 0, bonus_kills = 0;
  float bonus_time = 0;  // a Combo Bonus' seconds, as the pickup gives them
};
Match g_m;
bool g_match_ok = false;
const mt::Class* g_cls_sys = nullptr;
mem::Patch g_combo_clock, g_bonus_clock;  // each clock's `subss` of the frame time

// --- the hit a shot leaves on an enemy ------------------------------------------------------
// Every pawn has a cHit (the hit system's) and in it a record (0xE0 bytes, made with the
// pawn) into which the hit system writes the hit a frame brought: where, how much, the
// attack's data, the part, the attacker and the attacker's class. The pawn's own move reads
// it - for an enemy its damage check, in the actions' update: the hit path takes the health
// (the whole of it with the flags' instant-kill bit), the death is its own, and a death
// reports the kill to the play flow for the attacker's player (the kill counts, the combo,
// the score). Then the move clears it. A hit from the other machine comes as a message,
// and the game writes it into a record of these fields alone and runs it the same way
// (FUN_00B550F0): that is the hit Kill enemies as they appear writes - into the enemy's own
// record, at the frame's start (the tick), before the units move.
struct HitLayout {
  uint32_t record = 0;               // uEnemy: its hit record (a pointer)
  uint32_t chit = 0;                 // uEnemy: its cHit
  uint32_t parts = 0, nparts = 0;    // uEnemy: its parts (units whose hits reach it), how many
  uint32_t pos = 0;                  // uCoord: the place (three floats)
  uint32_t place[3] = {};            // the record: the place hit
  uint32_t centre[3] = {};           // ... the part's centre (the enemy's place for no part)
  uint32_t damage = 0, zero = 0;     // ... the damage, and a field the game zeroes
  uint32_t impact = 0, scale = 0;    // ... floats: the attack data's, and 1.0
  uint32_t flags[3] = {};            // ... the attack data's flags
  uint32_t hit = 0, part = 0;        // ... hit (a byte), the part (a byte; 0xFF none)
  uint32_t b94 = 0, f98 = 0;         // ... a byte 0, a float 0.0
  uint32_t attack = 0;               // ... the attack's number
  uint32_t attacker = 0, cls = 0;    // ... the attacker (a unit), its class id
  uint32_t def_impact = 0, def_flags[3] = {};  // an attack's data: what goes into those
  uint32_t whole = 0;                // flags[0]'s bit: the hit takes the whole health
  uintptr_t groups = 0;              // the attacks' data: a pointer and a count for each group
  uintptr_t guns = 0;                // the guns' table: an entry for each item 0x1nn ...
  uint32_t gun_stride = 0, gun_attack = 0;  // ... this many bytes each; its bullet's attack (a word)
  uintptr_t hit_param = 0;           // HIT_PARAM's vtable: every attack's data starts with it
};
HitLayout g_h;
bool g_kill_ok = false;
const mt::Class *g_cls_enemy = nullptr, *g_cls_npc = nullptr, *g_cls_weapon = nullptr;
std::vector<const mt::Class*> g_not_shots;  // weapons whose kill is no shot: knives, the stun rod, ...
int g_melee_seconds = 0;  // a hand-to-hand kill's time (the game's +5 s); 0: its rule not found - kills are shots
const mt::Class* g_weak_cls = nullptr;      // an enemy whose own check takes health only on a weak spot
int g_weak_part = -1;                       // ... its first part there (the Reaper's, from cheats.cpp)
constexpr int kHandgun = 0x102;             // the M92F: the shot when no gun is in hand

// --- the Chainsaw Majini's second life --------------------------------------------------------
// The Chainsaw Majini (uEm22) may be given a second life as it starts (its own rule: a player's
// dynamic difficulty level above 6, or its set's). Until it has used it, its own frame tells the
// hit path every frame that it may not die, so a hit that takes the last of its health leaves it
// 1 - and its brain lies it down instead (its motion, then 8 seconds, its hit shapes off), to get
// up again with 2000 and fight on. Hits written while it lies there do nothing (2026-11-03: the
// user's match stopped at 399 of 400, one standing in its second round). Kill enemies as they
// appear takes that life away as it writes the hit: the death is the game's own, as for a chainsaw
// that never had one.
struct SecondLife {
  uint32_t got_up = 0, may = 0;  // uEm22: it has got up again; it may (a byte each)
  uint32_t no_death = 0;         // uEnemy: it may not die this frame (the hit path's test, set again each frame)
  uint32_t up_health = 0;        // uEm22: the health it gets up with (a word)
  uint32_t lie = 0;              // its brain's lie-down (an action's number), for the log
};
SecondLife g_life;
bool g_life_ok = false;
const mt::Class* g_cls_chainsaw = nullptr;
int g_lives = 0;                    // second lives taken this match
constexpr int kVtableMost = 1024;   // a vtable walked no further than this

// Kill enemies as they appear: how hard it pushes. A hit is written at most this many times
// a tick, again after a while for one that still stands, and after so many only now and then:
// the game may keep one from dying for a while (lying down, a scripted move), and it is hit
// again until it dies.
constexpr int kPerTick = 8;
constexpr uint64_t kRetryMs = 400;
constexpr int kMaxTries = 25;
constexpr uint64_t kSlowMs = 5000;
constexpr int kSettleTicks = 12;    // frames in play before the first hit (after the pause, a load)
constexpr uint64_t kPaceMs = 15000;  // the log's pace line

// --- the stage's areas ----------------------------------------------------------------------
// A stage of the Mercenaries sends its enemies area by area: each area task of its scenario
// (sScenario's tasks of the area type) is a box, and once a player is inside it the task
// starts the area's enemy set, which adds an enemy every few seconds from its stock while
// fewer than its number are about - and waits again while no player is inside. Standing
// still, only the areas around the player send. With Kill enemies as they appear acting,
// each box is widened a thousandfold about its centre and from far below to far above, so
// the game's own test finds every player in every area: they all send, at their own pace
// and within their own numbers. Each box is put back exactly as it was the moment the
// cheat stops acting; one the stage has let go of (a retry loads it anew) is never written.
struct AreaLayout {
  uint32_t lists = 0, type = 0;   // sScenario: its tasks by type (a list each); the area tasks' type
  uint32_t count = 0, items = 0;  // a list: how many, the array
  uint32_t area = 0;              // cScenario (a task): its area (cAreaHit)
  uint32_t shape = 0;             // cAreaHit: its shape
  uint32_t floor = 0, height = 0;  // AHShapeBox: the floor and how high it goes
  uint32_t x[4] = {}, z[4] = {};   // ... its four corners
};
AreaLayout g_a;
bool g_area_ok = false;
const mt::Class *g_cls_scenario_sys = nullptr, *g_cls_task = nullptr, *g_cls_area = nullptr, *g_cls_box = nullptr;
constexpr float kWiden = 1000.0f, kFloor = -1.0e7f, kHeight = 2.0e7f;
constexpr uint32_t kMaxTasks = 256;

struct Box {
  float x[4], z[4], floor, height;
};
struct Widened {
  uintptr_t shape = 0;
  Box was{}, now{};
};
std::vector<Widened> g_widened;

Box read_box(uintptr_t s) {
  Box b;
  for (int i = 0; i < 4; ++i) {
    b.x[i] = mem::read<float>(s + g_a.x[i]);
    b.z[i] = mem::read<float>(s + g_a.z[i]);
  }
  b.floor = mem::read<float>(s + g_a.floor);
  b.height = mem::read<float>(s + g_a.height);
  return b;
}

void write_box(uintptr_t s, const Box& b) {
  for (int i = 0; i < 4; ++i) {
    mem::write<float>(s + g_a.x[i], b.x[i]);
    mem::write<float>(s + g_a.z[i], b.z[i]);
  }
  mem::write<float>(s + g_a.floor, b.floor);
  mem::write<float>(s + g_a.height, b.height);
}

bool same_box(const Box& a, const Box& b) { return std::memcmp(&a, &b, sizeof a) == 0; }

// The area tasks' boxes of the stage in play.
std::vector<uintptr_t> area_boxes() {
  std::vector<uintptr_t> out;
  const uintptr_t sc = game::system("sScenario");
  if (!g_area_ok || !sc || !mt::is_a(sc, g_cls_scenario_sys)) return out;
  const uintptr_t list = mem::read<uintptr_t>(sc + g_a.lists + 4 * g_a.type);
  if (!list || !mem::readable(list, g_a.items + 4)) return out;
  const uint32_t n = u32at(list + g_a.count);
  const uintptr_t items = mem::read<uintptr_t>(list + g_a.items);
  if (!items || n > kMaxTasks || !mem::readable(items, 4 * n)) return out;
  for (uint32_t i = 0; i < n; ++i) {
    const uintptr_t task = mem::read<uintptr_t>(items + 4 * i);
    if (!task || !mt::is_a(task, g_cls_task)) continue;
    const uintptr_t area = mem::read<uintptr_t>(task + g_a.area);
    if (!area || !mt::is_a(area, g_cls_area)) continue;
    const uintptr_t shape = mem::read<uintptr_t>(area + g_a.shape);
    if (shape && mt::is_a(shape, g_cls_box)) out.push_back(shape);
  }
  return out;
}

// Each area opened (on) or put back (off). A box with no extent (a stage's placeholder,
// which the game already takes for inside) is left as it is.
void widen_areas(bool on, Status* s) {
  const std::vector<uintptr_t> boxes = area_boxes();
  auto present = [&](uintptr_t b) {
    for (uintptr_t x : boxes)
      if (x == b) return true;
    return false;
  };
  // Gone from the stage, or no longer as the mod left it: forgotten without a write.
  int lost = 0;
  for (size_t i = 0; i < g_widened.size();) {
    if (!present(g_widened[i].shape) || !same_box(read_box(g_widened[i].shape), g_widened[i].now)) {
      g_widened.erase(g_widened.begin() + static_cast<std::ptrdiff_t>(i));
      ++lost;
    } else {
      ++i;
    }
  }
  if (lost) logf("merc: %d widened area(s) gone from the stage - forgotten, nothing written", lost);
  if (!on) {
    if (!g_widened.empty()) {
      for (const Widened& w : g_widened) write_box(w.shape, w.was);
      logf("merc: kill enemies as they appear: the stage's %d area(s) put back as they were",
           static_cast<int>(g_widened.size()));
      g_widened.clear();
    }
    return;
  }
  int added = 0;
  for (uintptr_t b : boxes) {
    bool known = false;
    for (const Widened& w : g_widened) known = known || w.shape == b;
    if (known) continue;
    Widened w;
    w.shape = b;
    w.was = read_box(b);
    float cx = 0, cz = 0, minx = w.was.x[0], maxx = w.was.x[0], minz = w.was.z[0], maxz = w.was.z[0];
    for (int i = 0; i < 4; ++i) {
      cx += w.was.x[i] / 4;
      cz += w.was.z[i] / 4;
      minx = w.was.x[i] < minx ? w.was.x[i] : minx;
      maxx = w.was.x[i] > maxx ? w.was.x[i] : maxx;
      minz = w.was.z[i] < minz ? w.was.z[i] : minz;
      maxz = w.was.z[i] > maxz ? w.was.z[i] : maxz;
    }
    if (!(maxx - minx > 1.0e-3f) || !(maxz - minz > 1.0e-3f) || !(w.was.height > 0)) continue;  // no extent
    for (int i = 0; i < 4; ++i) {
      w.now.x[i] = cx + (w.was.x[i] - cx) * kWiden;
      w.now.z[i] = cz + (w.was.z[i] - cz) * kWiden;
    }
    w.now.floor = kFloor;
    w.now.height = kHeight;
    write_box(b, w.now);
    g_widened.push_back(w);
    ++added;
  }
  if (added)
    logf("merc: kill enemies as they appear: %d of the stage's %d area(s) opened to wherever you stand - every area "
         "sends its enemies (%d open)",
         added, static_cast<int>(boxes.size()), static_cast<int>(g_widened.size()));
  s->areas = static_cast<int>(g_widened.size());
}

struct Target {
  uintptr_t unit = 0;
  const mt::Class* cls = nullptr;
  int tries = 0;
  uint64_t next = 0;  // not again before
  bool seen = false;
};
std::vector<Target> g_targets;
std::vector<const mt::Class*> g_logged;  // classes whose first hit (or give-up) was logged
int g_settle = 0;
int g_hits = 0, g_slow = 0;  // this match: hits written, enemies still standing after kMaxTries
uint64_t g_pace_at = 0;
int g_pace_kills = 0;
uint64_t (*g_now)() = []() -> uint64_t { return GetTickCount64(); };

bool g_in_match = false;  // a match seen last tick (the count-in or play) ...
int g_last_step = -1;     // ... and its step: one gone back (play to the count-in) is a retry, a match anew
int g_merc_logged = 0;
bool g_bonus_logged = false, g_bank_logged = false;
CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
Status g_status;

void follow(mem::Patch& p, bool on) {
  if (!p.prepared()) return;
  if (on && !p.applied) {
    if (!p.apply()) logf("merc: the %s site changed under us - left alone", p.label);
  } else if (!on && p.applied) {
    p.revert();
  }
}

bool logged(const mt::Class* c) {
  for (const mt::Class* x : g_logged)
    if (x == c) return true;
  g_logged.push_back(c);
  return false;
}

// The shot of the gun in the character's hand (a gun's class and its bullet's attack data),
// or the M92F's when what is in hand is no gun - a knife, the stun rod, a grenade, nothing.
struct Shot {
  int item = 0;
  uint32_t cls = 0;     // the gun's class id (the kill's weapon)
  uint32_t attack = 0;  // its bullet's attack number
  uintptr_t data = 0;   // that attack's data (a HIT_PARAM)
};

uintptr_t attack_data(uint32_t attack) {
  const uintptr_t group = g_h.groups + 8 * (attack >> 16);
  const uintptr_t base = mem::read<uintptr_t>(group);
  const uint32_t count = u32at(group + 4);
  if (!base || (attack & 0xFFFF) >= count) return 0;
  const uintptr_t d = base + (attack & 0xFFFF) * 0x1C;
  return mem::readable(d, 0x1C) && mem::read<uintptr_t>(d) == g_h.hit_param ? d : 0;
}

bool shot_of(int item, Shot* out) {
  if ((item & 0xFF00) != 0x100) return false;
  const mt::Class* c = mt::by_id(((7u << 16) | static_cast<uint32_t>(item)) << 8);
  if (!c || !mt::derives(c->dti, g_cls_weapon->dti)) return false;
  for (const mt::Class* n : g_not_shots)
    if (mt::derives(c->dti, n->dti)) return false;
  const uintptr_t entry = g_h.guns + (item & 0xFF) * g_h.gun_stride;
  if (!mem::readable(entry, g_h.gun_stride)) return false;
  const uint32_t attack = mem::read<uint16_t>(entry + g_h.gun_attack);
  const uintptr_t data = attack_data(attack);
  if (!data) return false;
  *out = Shot{item, c->id, attack, data};
  return true;
}

Shot shot_in_hand(uintptr_t chara) {
  Shot s;
  for (int slot = 0; slot < game::kSlots; ++slot) {
    game::Item it;
    if (game::item_in(chara, slot, &it) && it.equip == 1 && shot_of(it.id, &s)) return s;
  }
  shot_of(kHandgun, &s);
  return s;
}

// A Chainsaw Majini with its second life still to come: the life taken away, and this frame's
// "may not die" with it (its own frame set that a frame ago, and the hit path may run before its
// next one), so the hit's death is the game's own. One that has got up again has no life left,
// and dies at its hit as it is.
void take_second_life(uintptr_t enemy, const mt::Class* c) {
  if (!g_life_ok || !c || !mt::derives(c->dti, g_cls_chainsaw->dti)) return;
  if (!u8at(enemy + g_life.may) || u8at(enemy + g_life.got_up)) return;
  mem::write<uint8_t>(enemy + g_life.may, 0);
  mem::write<uint8_t>(enemy + g_life.no_death, 0);
  if (g_lives++ == 0)
    logf("merc: kill enemies as they appear: a %s with a second life to come (it would lie down and get up again with "
         "%d) - the life taken away before its hit, its death the game's own (first this match)",
         c->name, static_cast<int>(mem::read<int16_t>(enemy + g_life.up_health)));
}

// The hit, written into the enemy's own record as the game writes one from a message: the
// attacker the character, the attack its gun's bullet data with the whole-health bit, no
// part (the hit system's own value for a part it has no index for) - or a weak spot's for an
// enemy that takes health nowhere else. Its class (the kill's weapon) the character's own:
// the game takes the kill for a hand-to-hand one, which earns the clock its 5 seconds (as a
// punch's, a kick's or a finisher's; a knife's earns none) - or, that rule not found, the
// gun's. The place hit a twentieth of the way from the enemy towards the shooter (never the
// enemy's own place: the reactions take a direction from the two). Not over a hit of the
// game's own waiting there.
bool write_hit(uintptr_t enemy, uintptr_t chara, const Shot& shot, uint32_t kill_cls) {
  const uintptr_t rec = mem::read<uintptr_t>(enemy + g_h.record);
  if (!rec || !mem::readable(rec, 0x100) || u8at(rec + g_h.hit)) return false;
  const game::Layout& l = game::layout();
  const mt::Class* c = units::class_of(enemy);
  take_second_life(enemy, c);
  const int hp = mem::read<int16_t>(enemy + l.hp);
  float at[3], from[3];
  for (int i = 0; i < 3; ++i) {
    at[i] = mem::read<float>(enemy + g_h.pos + 4 * i);
    from[i] = mem::read<float>(chara + g_h.pos + 4 * i);
  }
  const bool same = at[0] == from[0] && at[1] == from[1] && at[2] == from[2];
  int part = 0xFF;
  if (g_weak_cls && g_weak_part >= 0 && c && mt::derives(c->dti, g_weak_cls->dti)) part = g_weak_part;
  for (int i = 0; i < 3; ++i) {
    const float p = same ? at[i] + (i == 1 ? 1.0f : 0.0f) : at[i] + (from[i] - at[i]) * 0.05f;
    mem::write<float>(rec + g_h.place[i], p);
    mem::write<float>(rec + g_h.centre[i], at[i]);
  }
  mem::write<int32_t>(rec + g_h.damage, hp > 0 ? hp : 1);
  mem::write<uint32_t>(rec + g_h.zero, 0);
  mem::write<float>(rec + g_h.impact, mem::read<float>(shot.data + g_h.def_impact));
  mem::write<float>(rec + g_h.scale, 1.0f);
  mem::write<uint32_t>(rec + g_h.flags[0], u32at(shot.data + g_h.def_flags[0]) | g_h.whole);
  mem::write<uint32_t>(rec + g_h.flags[1], u32at(shot.data + g_h.def_flags[1]));
  mem::write<uint32_t>(rec + g_h.flags[2], u32at(shot.data + g_h.def_flags[2]));
  mem::write<uint8_t>(rec + g_h.part, static_cast<uint8_t>(part));
  mem::write<uint8_t>(rec + g_h.b94, 0);
  mem::write<float>(rec + g_h.f98, 0.0f);
  mem::write<uint32_t>(rec + g_h.attack, shot.attack);
  mem::write<uintptr_t>(rec + g_h.attacker, chara);
  mem::write<uint32_t>(rec + g_h.cls, kill_cls ? kill_cls : shot.cls);
  mem::write<uint8_t>(rec + g_h.hit, 1);
  return true;
}

// The enemies in play: running units of uEnemy's classes with health left - the animals among
// them: every stage of the Mercenaries counts its chickens among the kills that clear it (its
// start set places them; 2026-11-03: skipped, one left the user's match at 149 of 150) - but not
// the story's companions, nor an enemy's own parts (the units whose hits its damage check takes
// as its own: the Reaper's soft body and the like).
template <typename Fn>
void each_enemy(Fn&& fn) {
  const uintptr_t sunit = game::system("sUnit");
  const game::Layout& l = game::layout();
  std::vector<uintptr_t> enemies, parts;
  units::each(sunit, [&](uintptr_t u, const mt::Class* c) {
    if (!c || !mt::derives(c->dti, g_cls_enemy->dti) || (g_cls_npc && mt::derives(c->dti, g_cls_npc->dti)))
      return true;
    for (uint32_t i = 0; i < g_h.nparts; ++i)
      if (const uintptr_t p = mem::read<uintptr_t>(u + g_h.parts + 4 * i)) parts.push_back(p);
    if ((u32at(u + units::layout().unit_flags) & 7) == 2 && mem::read<int16_t>(u + l.hp) > 0) enemies.push_back(u);
    return true;
  });
  for (uintptr_t u : enemies) {
    bool part = false;
    for (uintptr_t p : parts) part = part || p == u;
    if (!part) fn(u, units::class_of(u));
  }
}

// The kills stopped: what the stage has, for the log (once a stall).
constexpr uint64_t kStallMs = 20000;
uint64_t g_kills_at = 0;  // when the kills last moved
int g_kills_seen = -1;
bool g_stall_logged = false;

void kill_enemies(const game::Run& run, uintptr_t sys, Status* s) {
  // This machine's character (the first with health), the shooter.
  uintptr_t chara = 0;
  for (int i = 0; i < run.nchars && !chara; ++i)
    if (run.chars[i].control == game::Control::kLocal && run.chars[i].hp > 0) chara = run.chars[i].obj;
  if (!chara) return;
  if (g_settle < kSettleTicks) {
    ++g_settle;
    return;
  }
  const uint64_t now = g_now();
  for (Target& t : g_targets) t.seen = false;
  int about = 0;
  each_enemy([&](uintptr_t u, const mt::Class* c) {
    ++about;
    for (Target& t : g_targets)
      if (t.unit == u) {
        t.seen = true;
        return;
      }
    g_targets.push_back(Target{u, c, 0, 0, true});
  });
  // Gone or dead since: dropped.
  for (size_t i = 0; i < g_targets.size();)
    if (!g_targets[i].seen) g_targets.erase(g_targets.begin() + static_cast<std::ptrdiff_t>(i));
    else ++i;
  const Shot shot = shot_in_hand(chara);
  const mt::Class* own = units::class_of(chara);
  const uint32_t kill_cls = g_melee_seconds && own ? own->id : 0;  // a hand-to-hand kill of the character's
  int written = 0;
  for (Target& t : g_targets) {
    if (written >= kPerTick) break;
    if (now < t.next || !shot.data) continue;
    if (!write_hit(t.unit, chara, shot, kill_cls)) continue;
    ++written;
    ++g_hits;
    ++t.tries;
    t.next = now + (t.tries >= kMaxTries ? kSlowMs : kRetryMs);
    if (t.tries == 1 && !logged(t.cls)) {
      if (kill_cls)
        logf("merc: kill enemies as they appear: %s hit (first time) - a hand-to-hand kill for %s (its class "
             "0x%08X: +%d s), the hit a shot of item 0x%X (attack %u)",
             t.cls->name, game::character_name(chara), static_cast<unsigned>(kill_cls), g_melee_seconds,
             static_cast<unsigned>(shot.item), shot.attack);
      else
        logf("merc: kill enemies as they appear: %s hit (first time) - a shot of %s's (item 0x%X, attack %u)",
             t.cls->name, game::character_name(chara), static_cast<unsigned>(shot.item), shot.attack);
    }
    if (t.tries == kMaxTries) {
      ++g_slow;
      logf("merc: kill enemies as they appear: a %s still stands after %d hits - hit again every %llu s while it "
           "stands",
           t.cls->name, kMaxTries, static_cast<unsigned long long>(kSlowMs / 1000));
    }
  }
  s->killing = static_cast<int>(g_targets.size());
  // The pace, for the log: what the stage has about and how the kills go.
  if (!g_pace_at) {
    g_pace_at = now + kPaceMs;
    g_pace_kills = static_cast<int>(u32at(sys + g_m.kills));
  } else if (now >= g_pace_at) {
    const int kills = static_cast<int>(u32at(sys + g_m.kills));
    logf("merc: kill enemies as they appear: %d about, %d hit to date, kills %d of %d (+%d in %llu s)%s", about,
         g_hits, kills, s->target, kills - g_pace_kills, static_cast<unsigned long long>(kPaceMs / 1000),
         g_slow ? " - some hit only every few seconds now (see above)" : "");
    g_pace_at = now + kPaceMs;
    g_pace_kills = kills;
  }
  const int kills = static_cast<int>(u32at(sys + g_m.kills));
  if (kills != g_kills_seen || about) {
    g_kills_seen = kills;
    g_kills_at = now;
    g_stall_logged = false;
  } else if (!g_stall_logged && now - g_kills_at >= kStallMs) {
    g_stall_logged = true;
    logf("merc: kill enemies as they appear: the kills stopped at %d of %d - nothing about for %llu s; the stage's "
         "areas: %d box(es), %d open%s",
         kills, s->target, static_cast<unsigned long long>(kStallMs / 1000), static_cast<int>(area_boxes().size()),
         static_cast<int>(g_widened.size()), g_area_ok ? "" : " (the areas were not understood: see the start)");
  }
}

void forget_kills() {
  g_targets.clear();
  g_settle = 0;
  g_hits = 0;
  g_slow = 0;
  g_lives = 0;
  g_pace_at = 0;
  g_kills_seen = -1;
  g_stall_logged = false;
}

// The stage's areas, found: a stage's own ask for its area tasks (the type it pushes, the
// lists' getter it calls), a walk over a type's tasks (the same getter, then a list's), a
// task's "a player is in its area" (the task's area), an area's frame (its shape) and a
// box's test (its floor, height and corners) - each checked against the classes' sizes,
// and the box's test against AHShapeBox's own vtable.
void discover_areas() {
  const uintptr_t tasks = game::sig_one("kSigAreaTasks"), loop = game::sig_one("kSigTaskLoop");
  const uintptr_t inside = game::sig_one("kSigAreaInside"), frame = game::sig_one("kSigAreaHitFrame");
  const uintptr_t test = game::sig_one("kSigBoxInside");
  g_cls_scenario_sys = mt::find("sScenario");
  g_cls_task = mt::find("cScenario");
  g_cls_area = mt::find("cAreaHit");
  g_cls_box = mt::find("AHShapeBox");
  if (!tasks || !loop || !inside || !frame || !test || !g_cls_scenario_sys || !g_cls_task || !g_cls_area ||
      !g_cls_area->vtable || !g_cls_box || !g_cls_box->vtable) {
    logf("merc: the stages' areas were not found - Kill enemies as they appear takes the enemies the game sends "
         "where you stand");
    return;
  }
  const uintptr_t lists_get = game::call_target(tasks + 8), list_get = game::call_target(loop + 23);
  const bool getters = image::text().contains(lists_get) && game::call_target(loop + 7) == lists_get &&
                       mem::matches(lists_get, mem::parse_pattern("8B 44 24 04 8B 84 81 ?? ?? ?? ?? C2 04 00")) &&
                       image::text().contains(list_get) &&
                       mem::matches(list_get, mem::parse_pattern("8B 44 24 04 39 41 ?? 77 05 33 C0 C2 04 00 8B 49 ?? "
                                                                 "8B 04 81 C2 04 00"));
  AreaLayout a;
  a.type = u8at(tasks + 7);
  a.lists = getters ? u32at(lists_get + 7) : 0;
  a.count = getters ? u8at(list_get + 6) : 0;
  a.items = getters ? u8at(list_get + 16) : 0;
  a.area = u8at(inside + 3);
  a.shape = u8at(frame + 42);
  a.floor = u8at(test + 8);
  a.height = u8at(test + 27);
  const int xs[4] = {45, 64, 151, 74}, zs[4] = {50, 69, 160, 79};
  for (int i = 0; i < 4; ++i) {
    a.x[i] = u8at(test + xs[i]);
    a.z[i] = u8at(test + zs[i]);
  }
  bool corners = u8at(test + 146) == a.x[0];
  for (int i = 0; i < 4; ++i) corners = corners && a.z[i] == a.x[i] + 8 && a.x[i] == a.x[0] + 16 * static_cast<uint32_t>(i);
  // Each check by name, so the log says which one a build or a version fails.
  const struct {
    bool ok;
    const char* what;
  } checks[] = {
      {getters, "the lists' getters"},
      {corners, "the box's corners"},
      {mem::read<uintptr_t>(g_cls_box->vtable + 4 * 9) == test, "AHShapeBox's test in its slot 9"},
      {mem::read<uintptr_t>(g_cls_area->vtable + 4 * 7) == frame, "cAreaHit's frame in its slot 7"},
      {a.lists + 4 * (a.type + 1) <= g_cls_scenario_sys->size, "the lists within sScenario"},
      {a.area + 4 <= g_cls_task->size, "a task's area within cScenario"},
      {u8at(inside + 30) == u8at(frame + 24), "the players found, the same field"},
      {a.shape + 4 <= g_cls_area->size, "the shape within cAreaHit"},
      {a.floor + 4 <= g_cls_box->size && a.height + 4 <= g_cls_box->size && a.z[3] + 4 <= g_cls_box->size,
       "the box's fields within AHShapeBox"},
      {a.count < 0x40 && a.items < 0x40 && a.count != a.items, "a list's count and array"},
  };
  for (const auto& c : checks)
    if (!c.ok) {
      logf("merc: the stages' areas are not as known (%s) - Kill enemies as they appear takes the enemies the game "
           "sends where you stand",
           c.what);
      return;
    }
  g_a = a;
  g_area_ok = true;
  logf("merc: a stage's areas: sScenario +0x%X's tasks of type %u (a list's count +0x%X, its tasks +0x%X); a task's "
       "area +0x%X, its shape +0x%X; a box's floor +0x%X, height +0x%X, corners +0x%X.. (exe+0x%X)",
       a.lists, a.type, a.count, a.items, a.area, a.shape, a.floor, a.height, a.x[0],
       static_cast<unsigned>(test - image::base()));
}

// The vtable slot of a class's own virtual function (its parent has another function there), or
// -1. The vtable is walked while its entries are code.
int own_slot(const mt::Class* c, const mt::Class* parent, uintptr_t fn) {
  if (!c || !c->vtable || !parent || !parent->vtable || !fn) return -1;
  for (int i = 0; i < kVtableMost; ++i) {
    const uintptr_t at = c->vtable + 4 * i;
    if (!mem::readable(at, 4) || !image::text().contains(mem::read<uintptr_t>(at))) return -1;
    if (mem::read<uintptr_t>(at) != fn) continue;
    const uintptr_t theirs = parent->vtable + 4 * i;
    return mem::readable(theirs, 4) && mem::read<uintptr_t>(theirs) != fn ? i : -1;
  }
  return -1;
}

// The Chainsaw Majini's second life, found: its own frame's test (it has got up again, it may -
// then it may not die), its brain's damage reaction that lies it down (the same two bytes, and its
// health), and the hit path's tail, which lets an enemy die only while "may not die" is clear (after
// the whole health's test). The first two each a virtual function of its own class; each field
// within the class that keeps it.
void discover_second_life() {
  const uintptr_t life = game::sig_one("kSigChainsawLife"), down = game::sig_one("kSigChainsawDown");
  const uintptr_t gate = game::sig_one("kSigDeathGate"), whole = game::sig_one("kSigWholeHealth");
  const mt::Class* em22 = mt::find("uEm22");
  const mt::Class* brain = mt::find("uEmIntel22");
  const mt::Class* intel = mt::find("uEmIntel");
  if (!life || !down || !gate || !whole || !em22 || !brain || !intel) {
    logf("merc: the Chainsaw Majini's second life was not found - Kill enemies as they appear kills one once it has "
         "got up again");
    return;
  }
  SecondLife s;
  s.got_up = u32at(life + 73);
  s.may = u32at(life + 89);
  s.no_death = u32at(life + 100);
  s.up_health = u32at(down + 77);
  s.lie = u32at(down + 87);
  const uint32_t own = g_cls_enemy->size;  // uEm22's own fields start where uEnemy's end
  const struct {
    bool ok;
    const char* what;
  } checks[] = {
      {mt::derives(em22->dti, g_cls_enemy->dti), "uEm22 an enemy"},
      {own_slot(em22, g_cls_enemy, life) >= 0, "uEm22's own frame"},
      {own_slot(brain, intel, down) >= 0, "uEmIntel22's own damage reaction"},
      {u32at(down + 45) == s.may && u32at(down + 55) == s.got_up, "its brain's test, the same two bytes"},
      {u32at(down + 65) == game::layout().hp, "its brain's health, the characters'"},
      {u32at(life + 119) == g_h.chit, "its frame's cHit, the hit record's"},
      {gate > whole && gate - whole < 0x100, "the death's test after the whole health's"},
      {u32at(gate + 27) == s.no_death, "may not die, the death's test"},
      {s.no_death < g_cls_enemy->size, "may not die within uEnemy"},
      {s.got_up != s.may && s.got_up >= own && s.may >= own && s.up_health >= own && s.got_up < em22->size &&
           s.may < em22->size && s.up_health + 2 <= em22->size,
       "its own fields within uEm22"},
      {s.lie < 0x100, "the lie-down an action"},
  };
  for (const auto& c : checks)
    if (!c.ok) {
      logf("merc: the Chainsaw Majini's second life is not as known (%s) - Kill enemies as they appear kills one once "
           "it has got up again",
           c.what);
      return;
    }
  g_life = s;
  g_cls_chainsaw = em22;
  g_life_ok = true;
  logf("merc: the Chainsaw Majini's second life: uEm22 +0x%X (it may get up again) and +0x%X (it has); until then its "
       "frame (exe+0x%X) sets uEnemy +0x%X, which the hit path tests before a death (exe+0x%X), and its brain lies it "
       "down (action 0x%X, exe+0x%X) to get up with +0x%X's health - Kill enemies as they appear takes the life away "
       "first",
       s.may, s.got_up, static_cast<unsigned>(life - image::base()), s.no_death,
       static_cast<unsigned>(gate + 25 - image::base()), s.lie, static_cast<unsigned>(down - image::base()),
       s.up_health);
}

}  // namespace

bool match_found() { return g_match_ok; }
bool kills_found() { return g_kill_ok; }

void set_weak_spot(const mt::Class* cls, int part) {
  g_weak_cls = cls;
  g_weak_part = part;
}

bool discover_match() {
  if (!g_lock_ready) {
    InitializeCriticalSection(&g_lock);
    g_lock_ready = true;
  }
  const uintptr_t combo = game::sig_one("kSigMercCombo"), end = game::sig_one("kSigMercComboEnd");
  const uintptr_t bclock = game::sig_one("kSigMercBonusClock"), bstart = game::sig_one("kSigMercBonusStart");
  const uintptr_t clear = game::sig_one("kSigMercClear"), frame = game::sig_one("kSigMercFrame");
  const mt::Class* sys = mt::find("cGameSys");
  const mt::Class* main = mt::find("sBioMain");
  if (!combo || !end || !bclock || !bstart || !clear || !frame || !sys || !main) {
    logf("merc: the Mercenaries' play state was not found - no Endless combo or Combo Bonus always on");
    return false;
  }
  Match m;
  m.sys = u32at(combo + 7);
  // A slot's block: 1 << the shift (the same in each), slot 0's fields from cGameSys.
  const uint32_t shift = u8at(combo + 21);
  m.combo_on = u32at(combo + 25);
  m.combo_clock = u32at(combo + 37);
  m.host_a = u32at(combo + 57);
  m.host_b = u32at(combo + 63);
  m.combo = u32at(end + 24);
  m.best = u32at(end + 30);
  m.waiting = u32at(end + 85);
  m.score = u32at(end + 92);
  m.bonus_on = u32at(bclock + 24);
  m.bonus_clock = u32at(bclock + 43);
  m.bonus_kills = u32at(bstart + 71);
  const uintptr_t seconds = u32at(bstart + 43);
  m.bonus_time = image::data().contains(seconds) || image::rdata().contains(seconds) ? mem::read<float>(seconds) : 0;
  m.online = u32at(clear + 2);
  m.clock = u32at(clear + 13);
  m.mode = u8at(clear + 21);
  m.big = u32at(clear + 34);
  m.extra = static_cast<int>(u32at(clear + 44));
  m.base = static_cast<int>(u32at(clear + 50));
  m.kills = u32at(clear + 56);
  m.step = u32at(frame + 27);
  const uint32_t slots = 4;  // the players: the combo's clock runs for each of the four
  const bool ok =
      game::call_target(combo + 79) == end && shift == 6 && u8at(bclock + 20) == shift && u8at(bstart + 18) == shift &&
      u32at(end + 12) == m.sys && u32at(bclock + 11) == m.sys && u32at(bstart + 8) == m.sys &&
      u32at(bstart + 35) == m.sys && u32at(bstart + 64) == m.sys && u32at(frame + 21) == m.sys &&
      u32at(end + 66) == m.combo && u32at(end + 45) == m.best && u32at(bclock + 32) == m.bonus_on &&
      u32at(bstart + 22) == m.bonus_on && u32at(bstart + 52) == m.bonus_clock && m.bonus_time > 1.0f &&
      m.bonus_time < 600.0f && m.base > 0 && m.base < 10000 && m.extra >= 0 && m.extra < 10000 &&
      m.sys + 4 <= main->size && m.mode + 4 <= sys->size && m.step + 4 <= sys->size && m.online < sys->size &&
      m.host_a + 4 <= sys->size && m.host_b + 4 <= sys->size && m.clock + 4 <= sys->size &&
      m.kills + 4 <= sys->size && m.big < sys->size &&
      // every field of the four slots' blocks inside cGameSys
      m.combo_on + (slots << shift) <= sys->size && m.combo + 4 + ((slots - 1) << shift) <= sys->size &&
      m.best + 4 + ((slots - 1) << shift) <= sys->size && m.combo_clock + 4 + ((slots - 1) << shift) <= sys->size &&
      m.waiting + 4 + ((slots - 1) << shift) <= sys->size && m.score + 4 + ((slots - 1) << shift) <= sys->size &&
      m.bonus_on + ((slots - 1) << shift) < sys->size && m.bonus_clock + 4 + ((slots - 1) << shift) <= sys->size &&
      m.bonus_kills + 4 + ((slots - 1) << shift) <= sys->size &&
      // the two clocks' frame-time `subss`
      g_combo_clock.prepare(combo + 41, "F3 0F 5C 44 24 10", mem::nops(6), "combo clock") &&
      g_bonus_clock.prepare(bclock + 47, "F3 0F 5C 44 24 0C", mem::nops(6), "Combo Bonus clock");
  if (!ok) {
    logf("merc: the Mercenaries' play state is not as known - no Endless combo or Combo Bonus always on");
    return false;
  }
  g_m = m;
  g_cls_sys = sys;
  g_match_ok = true;
  logf("merc: the Mercenaries (modes %u and %u): cGameSys (sBioMain +0x%X) step +0x%X, clock +0x%X, kills +0x%X "
       "(%d to clear, %d more with +0x%X), online +0x%X; the team's slot: combo +0x%X (running +0x%X, its clock "
       "+0x%X exe+0x%X), best +0x%X, earned +0x%X, score +0x%X; Combo Bonus +0x%X (%.0f s, its clock +0x%X "
       "exe+0x%X, kills +0x%X)",
       kModeMercenaries, kModeReunion, m.sys, m.step, m.clock, m.kills, m.base, m.extra, m.big, m.online, m.combo,
       m.combo_on, m.combo_clock, static_cast<unsigned>(combo + 41 - image::base()), m.best, m.waiting, m.score,
       m.bonus_on, static_cast<double>(m.bonus_time), m.bonus_clock,
       static_cast<unsigned>(bclock + 47 - image::base()), m.bonus_kills);
  return true;
}

bool discover_kills() {
  const uintptr_t r = game::sig_one("kSigHitRebuild"), t = game::sig_one("kSigHitTaken");
  const uintptr_t w = game::sig_one("kSigWholeHealth");
  const uintptr_t a = game::sig_one("kSigAttackTable"), g = game::sig_one("kSigShotTable");
  const uintptr_t q = game::sig_one("kSigShotAttack"), parts = game::sig_one("kSigEnemyParts");
  g_cls_enemy = mt::find("uEnemy");
  g_cls_npc = mt::find("uEmNpc");
  g_cls_weapon = mt::find("uWeapon");
  const mt::Class* coord = mt::find("uCoord");
  const mt::Class* chit = mt::find("cHit");
  const mt::Class* hit_param = mt::find("HIT_PARAM");
  g_not_shots.clear();
  for (const char* n : {"uWpKnife", "uWpStunrod", "uWpBomb", "uWpMine", "uWpLauncher", "uWpShell", "uWpShellCommon",
                        "uWpGunEmp"})
    if (const mt::Class* c = mt::find(n)) g_not_shots.push_back(c);
  if (!r || !t || !w || !a || !g || !q || !parts || !g_cls_enemy || !g_cls_weapon || !coord || !chit || !hit_param ||
      !hit_param->vtable || g_not_shots.size() != 8) {
    logf("merc: an enemy's hit record was not found - no Kill enemies as they appear");
    return false;
  }
  HitLayout h;
  h.chit = u32at(r + 13);
  h.place[0] = 0;  // `fstp [esi]`
  h.place[1] = u8at(r + 58);
  h.place[2] = u8at(r + 67);
  h.damage = u8at(r + 76);
  h.zero = u8at(r + 79);
  h.def_impact = u8at(r + 84);
  h.impact = u8at(r + 87);
  h.scale = u8at(r + 96);
  h.def_flags[0] = u8at(r + 99);
  h.flags[0] = u8at(r + 102);
  h.def_flags[1] = u8at(r + 105);
  h.flags[1] = u8at(r + 108);
  h.def_flags[2] = u8at(r + 111);
  h.flags[2] = u8at(r + 114);
  const uintptr_t one = u32at(r + 121);
  h.hit = u8at(r + 146);
  h.part = u8at(r + 156);
  h.b94 = u32at(r + 159);
  h.f98 = u32at(r + 167);
  h.attack = u32at(r + 194);
  h.attacker = u8at(r + 206);
  h.cls = u32at(r + 215);
  for (int i = 0; i < 3; ++i) h.centre[i] = u8at(r + 237 + 6 * i);
  h.pos = u8at(r + 261);
  h.record = u32at(t + 18);
  h.parts = u32at(parts + 2);
  h.nparts = mem::matches(parts + 133, mem::parse_pattern("83 F8 ?? 89 44 24 10 72")) ? u8at(parts + 135) : 0;
  h.whole = u32at(w + 5);
  h.groups = u32at(a + 26);
  h.guns = u32at(g + 46);
  h.gun_stride = 5 << 6;  // the pattern's `lea eax,[eax+eax*4]; shl eax,6`
  h.gun_attack = u8at(q + 17);
  h.hit_param = hit_param->vtable;
  const game::Layout& l = game::layout();
  const bool fields_ok =
      h.place[1] == 4 && h.place[2] == 8 && h.centre[1] == h.centre[0] + 4 && h.centre[2] == h.centre[0] + 8 &&
      u8at(r + 264) == h.centre[0] && u8at(r + 270) == h.centre[1] && u8at(r + 276) == h.centre[2] &&
      u8at(r + 267) == h.pos + 4 && u8at(r + 273) == h.pos + 8 &&
      // the path without attack data: 0.0, 1.0 and three zeros into the same fields
      u8at(r + 129) == h.impact && u8at(r + 134) == h.scale && u8at(r + 137) == h.flags[0] &&
      u8at(r + 140) == h.flags[1] && u8at(r + 143) == h.flags[2] &&
      (image::rdata().contains(one) || image::data().contains(one)) && mem::read<float>(one) == 1.0f &&
      u8at(t + 28) == h.hit && u8at(w + 4) == h.flags[0] && u32at(w + 14) == l.hp_max && h.whole &&
      u32at(parts + 42) == h.record && u8at(parts + 52) == h.hit && h.nparts && h.nparts <= 8 &&
      h.parts + 4 * h.nparts <= g_cls_enemy->size &&
      (h.whole & (h.whole - 1)) == 0 && h.part == h.hit + 1 && h.b94 < 0x100 && h.f98 < 0x100 && h.attack < 0x100 &&
      h.cls < 0x100 && h.attacker < 0x100 &&
      // the record is the cHit's, which the enemy carries
      h.record >= h.chit && h.record + 4 <= h.chit + chit->size && h.chit + chit->size <= g_cls_enemy->size &&
      h.pos + 12 <= coord->size &&
      image::data().contains(h.groups) && image::data().contains(h.guns) && h.gun_attack + 2 <= h.gun_stride &&
      q > g && q - g < 0x300 && image::text().contains(game::call_target(q + 21));
  g_h = h;
  Shot s;
  if (!fields_ok || !shot_of(kHandgun, &s)) {
    logf("merc: an enemy's hit record is not as known - no Kill enemies as they appear");
    return false;
  }
  g_kill_ok = true;
  discover_areas();
  discover_second_life();
  // A kill's weapon and its time: the class id of the player's own class record is a hand-to-hand
  // kill, which (or the stun rod's) gets seconds onto the clock.
  const uintptr_t kind = game::sig_one("kSigMeleeKind"), time = game::sig_one("kSigMeleeTime");
  if (kind && time && u8at(kind + 21) == 0x20 && u32at(kind + 25) == 0x100 && u8at(time + 9) < 8 &&
      u8at(time + 19) > 0 && u8at(time + 19) < 60 && image::text().contains(game::call_target(time + 22))) {
    g_melee_seconds = static_cast<int>(u8at(time + 19));
    logf("merc: a kill whose hit names the player's own class is hand-to-hand (exe+0x%X): %d s onto the clock "
         "(exe+0x%X) - Kill enemies as they appear kills so",
         static_cast<unsigned>(kind - image::base()), g_melee_seconds, static_cast<unsigned>(time - image::base()));
  } else {
    logf("merc: a hand-to-hand kill's rule was not found - Kill enemies as they appear kills with shots (no time)");
  }
  logf("merc: an enemy's hit record (its cHit +0x%X, the record +0x%X): place +0x%X, centre +0x%X, damage +0x%X, "
       "attack data +0x%X/+0x%X/+0x%X/+0x%X (the whole health 0x%X), hit +0x%X, part +0x%X, attack +0x%X, attacker "
       "+0x%X, its class +0x%X; a unit's place +0x%X; the guns' table exe+0x%X (0x%X each, the bullet's attack +0x%X), "
       "the attacks' data exe+0x%X - the M92F's attack %u",
       h.chit, h.record, h.place[0], h.centre[0], h.damage, h.impact, h.flags[0], h.flags[1], h.flags[2], h.whole,
       h.hit, h.part, h.attack, h.attacker, h.cls, h.pos, static_cast<unsigned>(h.guns - image::base()),
       h.gun_stride, h.gun_attack, static_cast<unsigned>(h.groups - image::base()), s.attack);
  return true;
}

bool tick(const game::Run& run, const Wanted& want) {
  Status s;
  bool off = false;
  if (g_match_ok && run.main) {
    const uintptr_t sys = mem::read<uintptr_t>(run.main + g_m.sys);
    if (sys && mt::is_a(sys, g_cls_sys)) {
      const uint32_t mode = u32at(sys + g_m.mode);
      const int step = static_cast<int>(u32at(sys + g_m.step));
      const bool merc = run.mercenaries && (mode == kModeMercenaries || mode == kModeReunion);
      const bool in_match = merc && (step == kStepCountIn || step == kStepPlay);
      const bool offline = merc && !u8at(sys + g_m.online) && u32at(sys + g_m.host_a) == u32at(sys + g_m.host_b);
      const bool acting = in_match && offline;
      const float clock = mem::read<float>(sys + g_m.clock);
      // The clocks' patches, only while a match of this machine's is on.
      follow(g_combo_clock, want.endless_combo && acting);
      follow(g_bonus_clock, want.combo_bonus && acting);
      // Endless combo: what the combo earns onto the score as it is earned (the game would
      // put it on only as the combo ends).
      if (want.endless_combo && acting) {
        const int waiting = static_cast<int>(u32at(sys + g_m.waiting));
        if (waiting > 0) {
          mem::write<int32_t>(sys + g_m.score, static_cast<int32_t>(u32at(sys + g_m.score)) + waiting);
          mem::write<int32_t>(sys + g_m.waiting, 0);
          if (!g_bank_logged) {
            g_bank_logged = true;
            logf("merc: endless combo: the combo's bonus goes onto the score as it is earned (%d now, first time)",
                 waiting);
          }
        }
      }
      // Combo Bonus always on: started as the pickup starts it, from the clock's start.
      if (want.combo_bonus && acting && step == kStepPlay && clock > 0 && !u8at(sys + g_m.bonus_on)) {
        mem::write<uint8_t>(sys + g_m.bonus_on, 1);
        mem::write<float>(sys + g_m.bonus_clock, g_m.bonus_time);
        mem::write<int32_t>(sys + g_m.bonus_kills, 0);
        if (!g_bonus_logged) {
          g_bonus_logged = true;
          logf("merc: Combo Bonus always on: the Combo Bonus started (%.0f s, held)", static_cast<double>(g_m.bonus_time));
        }
      }
      s.match = merc;
      s.offline = offline;
      s.step = step;
      s.time_left = clock;
      s.score = static_cast<int>(u32at(sys + g_m.score));
      s.waiting = static_cast<int>(u32at(sys + g_m.waiting));
      s.combo = u8at(sys + g_m.combo_on) ? static_cast<int>(u32at(sys + g_m.combo)) : 0;
      s.best_combo = static_cast<int>(u32at(sys + g_m.best));
      if (s.combo > s.best_combo) s.best_combo = s.combo;
      s.kills = static_cast<int>(u32at(sys + g_m.kills));
      s.target = g_m.base + (u8at(sys + g_m.big) ? g_m.extra : 0);
      s.bonus = u8at(sys + g_m.bonus_on) != 0;
      s.bonus_left = s.bonus ? mem::read<float>(sys + g_m.bonus_clock) : 0.0f;
      s.bonus_kills = s.bonus ? static_cast<int>(u32at(sys + g_m.bonus_kills)) : 0;
      // Kill enemies as they appear: in play, the clock running with time left - every area
      // of the stage open while it acts (the pause too), put back the moment it stops.
      const bool killing = want.kill && g_kill_ok && acting && step == kStepPlay && clock > 0;
      widen_areas(killing, &s);
      if (killing && run.screen == game::Screen::kPlay)
        kill_enemies(run, sys, &s);
      else if (run.screen != game::Screen::kPlay)
        g_settle = 0;  // the pause, a load: a few frames in play again first
      s.killed = g_hits;
      s.melee_seconds = g_kill_ok ? g_melee_seconds : 0;
      // A match begun or over - or begun again (a retry takes play back to the count-in):
      // Kill enemies as they appear lasts one match.
      const bool again = in_match && g_in_match && step < g_last_step;
      if (in_match != g_in_match || again) {
        if (want.kill) off = true;
        if (g_merc_logged < 40) {
          ++g_merc_logged;
          logf("merc: %s (mode %u, step %d, kills %d of %d, score %d)",
               again ? "the match begins again" : in_match ? "a match of the Mercenaries begins" : "the match is over",
               mode, step, s.kills, s.target, s.score);
        }
        forget_kills();
        g_bonus_logged = g_bank_logged = false;
      }
      g_in_match = in_match;
      g_last_step = in_match ? step : -1;
    } else {
      follow(g_combo_clock, false);
      follow(g_bonus_clock, false);
      widen_areas(false, &s);
      if (g_in_match && want.kill) off = true;
      g_in_match = false;
      g_last_step = -1;
      forget_kills();
    }
  }
  if (g_lock_ready) {
    EnterCriticalSection(&g_lock);
    g_status = s;
    LeaveCriticalSection(&g_lock);
  }
  return off;
}

Status status() {
  if (!g_lock_ready) return Status{};
  EnterCriticalSection(&g_lock);
  const Status s = g_status;
  LeaveCriticalSection(&g_lock);
  return s;
}

void remove_patches() {
  g_combo_clock.revert();
  g_bonus_clock.revert();
}

}  // namespace re5cc::merc
