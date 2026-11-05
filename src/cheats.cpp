#include "cheats.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "config.h"
#include "image.h"
#include "log.h"
#include "mem.h"
#include "merc.h"
#include "mt.h"

namespace re5cc::cheats {
namespace {

volatile LONG g_on[sw::kCount] = {};
volatile LONG g_dirty = 0;  // a switch changed: the tick writes the ini
bool g_ok[sw::kCount] = {};
const char* g_why[sw::kCount] = {};

// --- the damage hooks ---------------------------------------------------------------------
// Every player class's vtable has uPlayer's damage in slot 66 (32 vtables, one
// function), every enemy class's uEnemy's subtract-health in slot 69 (93 vtables,
// one function): the hooks go into each of those vtables' slot, and call on to the
// function that was there. The story's companions (uEmNpc: Josh, Excella and the
// rest) keep theirs.
constexpr int kDamageSlot = 66;
constexpr int kSubtractSlot = 69;
using PlayerDamageFn = bool(__thiscall*)(void*, int, int);
using EnemySubtractFn = void(__thiscall*)(void*, int);
PlayerDamageFn g_player_damage = nullptr;
EnemySubtractFn g_enemy_subtract = nullptr;
std::vector<uintptr_t> g_player_entries, g_enemy_entries;

// A hit on a player: the protected ones take nothing, the way the game itself
// turns away a hit on the other player's character in a network game (no
// damage, not dead) - the hit's reaction is the caller's and plays out.
bool __thiscall hk_player_damage(void* self, int damage, int info) {
  const auto control = mem::read<uint32_t>(reinterpret_cast<uintptr_t>(self) + game::layout().control);
  if ((control == static_cast<uint32_t>(game::Control::kLocal) && g_on[sw::kGodMode]) ||
      (control == static_cast<uint32_t>(game::Control::kAi) && g_on[sw::kPartnerGod]))
    return false;
  return g_player_damage(self, damage, info);
}

// Health taken from an enemy - what every hit on one comes to: a shot's or a
// blow's damage worked out from the hit (FUN_009DEA50, which never goes through
// the damage slot), the damage slot itself, an action's fixed blow. With One hit
// kills any of it that does damage takes all the health there is, and the caller
// goes on as the game: its death, or the one point it keeps for an enemy that may
// not die yet (a boss between phases).
void __thiscall hk_enemy_subtract(void* self, int amount) {
  if (g_on[sw::kOneHitKills] && static_cast<int16_t>(amount) > 0) {
    const int hp = mem::read<int16_t>(reinterpret_cast<uintptr_t>(self) + game::layout().hp);
    if (hp > static_cast<int16_t>(amount)) amount = hp;
  }
  g_enemy_subtract(self, amount);
}

// The Ndesu (the giant ending chapter 2-3, uEm45) and its five Plagas (uEm4502)
// take no health from a hit. The hit path asks an enemy's check (its vtable's,
// read out of the path) before any health goes, and theirs take the hit into
// counters of their own and say no. The Ndesu's: a gauge (its strength for the
// round, kept at the round's floor while a flag says so) and four regions of its
// body, each one worn through letting out the Plaga under it. A Plaga's, while it
// is out: its count - below 0 its death, the fifth's the end of the fight - and
// as much off the Ndesu's gauge. One hit kills makes a hit with damage take all of
// it: the gauge to its floor or to nothing (no further: the brain only asks
// whether any is left) and every region through, so each Plaga still in comes
// out; a Plaga's count past 0.
using HitCheckFn = bool(__thiscall*)(void* self, int part, int damage);
HitCheckFn g_ndesu_hit = nullptr, g_plaga_hit = nullptr;
std::vector<uintptr_t> g_ndesu_entries, g_plaga_entries;
uint32_t g_ndesu_gauge = 0, g_plaga_count = 0;
constexpr int kWholeHit = 1000000;  // past any gauge or region the Ndesu has, at the least share (0.3) a region gets

bool __thiscall hk_ndesu_hit(void* self, int part, int damage) {
  if (!g_on[sw::kOneHitKills] || damage <= 0) return g_ndesu_hit(self, part, damage);
  float* gauge = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(self) + g_ndesu_gauge);
  const bool some_left = *gauge > 0;
  const bool yes = g_ndesu_hit(self, part, kWholeHit);
  if (some_left && *gauge < 0) *gauge = 0;
  return yes;
}

bool __thiscall hk_plaga_hit(void* self, int part, int damage) {
  if (g_on[sw::kOneHitKills] && damage > 0) {
    const int left = mem::read<int32_t>(reinterpret_cast<uintptr_t>(self) + g_plaga_count);
    if (left >= damage) damage = left + 1;
  }
  return g_plaga_hit(self, part, damage);
}

// Irving (Transformed) (the boss ending chapter 3-3, uEm46) takes health only through its
// core (uEmc3), and only while the core is open - Irving down, its mouth on the deck. Its
// check says no to a hit on its body and wears a region of it (12000) by the damage
// instead; worn through (for good: nothing sets it again), its next hit sends Irving down
// (its brain's reaction: a dive, then down). Its four tentacles (uEmc0) say no as well: a
// hit wears their region (2000; its last tenth only on the weak spot), worn through they
// break - and drop what they carry, and grow back. One hit kills makes a hit with damage
// on the body or a tentacle take all that is left of the region; the core's hit then takes
// all the health (the subtract-health hook).
struct Region {
  uint32_t worn = 0;  // worn through (a byte)
  uint32_t left = 0;  // what is left (an int)
};
HitCheckFn g_irving_hit = nullptr, g_tentacle_hit = nullptr;
std::vector<uintptr_t> g_irving_entries, g_tentacle_entries;
Region g_irving_body, g_tentacle_body;
uint32_t g_irving_parts = 0;  // the parts below this are its body's, the rest its core's

// The damage that wears `r` through: all that is left of it, or the damage when that is
// more or the region is through already.
int wear_through(void* self, const Region& r, int damage) {
  const uintptr_t p = reinterpret_cast<uintptr_t>(self);
  if (mem::read<uint8_t>(p + r.worn)) return damage;
  const int left = mem::read<int32_t>(p + r.left);
  return left > damage ? left : damage;
}

bool __thiscall hk_irving_hit(void* self, int part, int damage) {
  if (g_on[sw::kOneHitKills] && damage > 0 && static_cast<uint16_t>(part) < g_irving_parts)
    damage = wear_through(self, g_irving_body, damage);
  return g_irving_hit(self, part, damage);
}

bool __thiscall hk_tentacle_hit(void* self, int part, int damage) {
  if (g_on[sw::kOneHitKills] && damage > 0) damage = wear_through(self, g_tentacle_body, damage);
  return g_tentacle_hit(self, part, damage);
}

// The Reaper (uEm34, chapters 5 and 6) takes health only from a hit on its weak spot: its
// check says no to a hit anywhere else on its body - whose damage the Reaper has already
// cut to a tenth as the hit was made - and to every hit while it hangs from the ceiling or
// holds a player. Two marks of its own let a hit on its body through: the game sets both
// for its own instant kills of one (a rocket launcher's hit of one kind, a stage's trap:
// the hit then takes all its health), the first alone for a grenade's or a proximity
// bomb's blast (which takes it to its last point at most); the check clears both, whatever
// it answers. One hit kills sets both for a hit with damage on its body and lets the
// game's check answer: the body takes the hit as the weak spot does, the subtract-health
// hook takes all the health, and the death is the game's own. Hanging or holding a player
// it takes nothing, as before.
HitCheckFn g_reaper_hit = nullptr;
std::vector<uintptr_t> g_reaper_entries;
uint32_t g_reaper_parts = 0;                        // the parts below this are its body's
uint32_t g_reaper_through = 0, g_reaper_whole = 0;  // its two marks (bytes)

bool __thiscall hk_reaper_hit(void* self, int part, int damage) {
  if (g_on[sw::kOneHitKills] && damage > 0 && static_cast<uint16_t>(part) < g_reaper_parts) {
    const uintptr_t p = reinterpret_cast<uintptr_t>(self);
    *reinterpret_cast<uint8_t*>(p + g_reaper_through) = 1;
    *reinterpret_cast<uint8_t*>(p + g_reaper_whole) = 1;
  }
  return g_reaper_hit(self, part, damage);
}

// Uroboros Aheri (uEm42, Excella's, the boss ending chapter 6-2) takes health only in its
// main core, which comes out once its orange cores (uEm42Core, four; their hits come to its
// check as parts from 60 on) are broken - two in the first round, all four in the second.
// Its check keeps each core's health itself (7000, less at each regrowth) and wears a core
// only while it is open - each opens and closes on timers of its own, and a hit on a closed
// one only counts towards its flinching open. During its doom attack the cores' hits go into
// an escape instead, past 2000 the attack broken off (otherwise it kills the players). One
// hit kills breaks a core at a hit with damage, open or closed: its health 0 before the
// check, which finds it worn through as at the game's own last point; and in the doom attack
// it takes all that is left of the escape. The main core takes all its health at a hit as
// any enemy does (the subtract-health hook): its death is the game's, and so is the
// chapter's end.
struct AheriLayout {
  uint32_t parts = 0;        // the parts below this are its body's; each core's take this many more
  uint32_t cores = 0;        // how many cores
  uint32_t flags = 0;        // its flags (a byte) ...
  uint32_t beat = 0;         // ... and the doom attack's bit there
  uint32_t escape = 0;       // the doom attack's escape (an int) ...
  uint32_t escape_most = 0;  // ... broken off past this
  uint32_t core_hp = 0;      // the first core's health (a word) ...
  uint32_t core_step = 0;    // ... and the next core's this much on
};
HitCheckFn g_aheri_hit = nullptr;
std::vector<uintptr_t> g_aheri_entries;
AheriLayout g_aheri;

bool __thiscall hk_aheri_hit(void* self, int part, int damage) {
  const uint32_t at = static_cast<uint16_t>(part);
  if (g_on[sw::kOneHitKills] && damage > 0 && g_aheri.parts && at >= g_aheri.parts &&
      at / g_aheri.parts - 1 < g_aheri.cores) {
    const uintptr_t p = reinterpret_cast<uintptr_t>(self);
    if (mem::read<uint8_t>(p + g_aheri.flags) & g_aheri.beat) {
      const int64_t left = static_cast<int64_t>(g_aheri.escape_most) + 1 - mem::read<int32_t>(p + g_aheri.escape);
      if (left > damage) damage = static_cast<int>(left);
    } else {
      auto* hp = reinterpret_cast<int16_t*>(p + g_aheri.core_hp + g_aheri.core_step * (at / g_aheri.parts - 1));
      if (*hp > 0) *hp = 0;
    }
  }
  return g_aheri_hit(self, part, damage);
}

// The jeep of chapter 2-3 (uOmJeep) has a damage of its own in slot 66 - every hit,
// scripted blow and truck's ram it takes comes there: it scales the damage in one
// stage, calls on to the pawns' damage (the health off, "destroyed?") and notes
// how low its health has gone. God mode turns the hit away there, as for the
// character: no damage, not destroyed - the jeep's shake plays out.
PlayerDamageFn g_jeep_damage = nullptr;
std::vector<uintptr_t> g_jeep_entries;
uintptr_t g_jeep_seen = 0;  // the jeep in play last tick, for the log

bool __thiscall hk_jeep_damage(void* self, int damage, int info) {
  if (g_on[sw::kGodMode]) return false;
  return g_jeep_damage(self, damage, info);
}

// Each vtable of a class deriving from `base` (but not from `except`) whose `slot`
// holds `fn` gets `hook` there.
int hook_vtables(const mt::Class* base, const mt::Class* except, int slot, uintptr_t fn, void* hook,
                 std::vector<uintptr_t>* out) {
  int count = 0;
  const mt::Class* all = mt::classes(&count);
  int n = 0;
  for (int i = 0; i < count; ++i) {
    const mt::Class& c = all[i];
    if (!c.vtable || !mt::derives(c.dti, base->dti)) continue;
    if (except && mt::derives(c.dti, except->dti)) continue;
    for (uintptr_t vt : mt::vtables_of(&c)) {
      const uintptr_t entry = vt + 4 * slot;
      if (mem::read<uintptr_t>(entry) != fn) continue;
      if (!mem::write<uintptr_t>(entry, reinterpret_cast<uintptr_t>(hook))) continue;
      out->push_back(entry);
      ++n;
    }
  }
  return n;
}

// --- byte patches ---------------------------------------------------------------------------
mem::Patch g_infinite;              // the weapons' "is it infinite" asks the game's debug switch: now "yes"
mem::Patch g_overheat_driver;       // a gun's overheating: every player's taken for one this machine does not drive
mem::Patch g_overheat_jeep;         // ... and the firing of every gun of such a player forgotten, not the jeep's alone
mem::Patch g_gold_clamp, g_gold_take;  // cSystemInfo's take-gold: neither the clamp to 0 nor the subtraction
mem::Patch g_playtime;              // cGameInfo's add-play-time returns at once
mem::Patch g_deaths;                // cGameInfo's add-deaths returns at once
mem::Patch g_countdown;             // the Mercenaries' clock: its new time not stored

const std::vector<uint8_t> kRet8 = {0xC2, 0x08, 0x00};

uint32_t u8at(uintptr_t a) { return mem::read<uint8_t>(a); }
uint32_t u32at(uintptr_t a) { return mem::read<uint32_t>(a); }
// Where a `jmp rel32` (E9) at `at` goes, or 0.
uintptr_t jmp_target(uintptr_t at) { return u8at(at) == 0xE9 ? at + 5 + mem::read<int32_t>(at + 1) : 0; }

// --- the stages' countdowns -----------------------------------------------------------------
// A stage whose scene is timed keeps a countdown of its own - chapter 3-2's escape from
// the oil field (stage 209), the wait for the helicopter ending Desperate Escape (stage
// 704): the seconds left, a float of the stage's, which its frame runs down by the frame's
// time while the game is not paused (to 0, or to a floor of the stage's) and shows on
// each player's HUD. Once it is out the HUD's countdown goes and the stage notes it, which
// is the scene's time-out (a game over); a scene that ends in time zeroes it. Freeze
// countdown timer takes the frame's time out of the subtraction. The stages are among
// sArea's areas (beside the play flow, aGame, which keeps the Mercenaries' clock:
// g_countdown above); the panel's countdown is the one running there.
struct StageClock {
  uintptr_t frame = 0;  // the stage's frame: an area whose vtable holds it is that stage
  const char* name = "";
  uint32_t time = 0;   // its seconds left (a float)
  uint32_t floor = 0;  // the float it runs down to (0: none - it runs down to 0)
  mem::Patch frozen;   // its frame's subtraction of the frame time -> nops
};
std::vector<StageClock> g_clocks;
const mt::Class* g_cls_areas = nullptr;      // sArea
uint32_t g_area_list = 0, g_area_count = 0;  // sArea: its areas, a pointer each (the first null ends them)
uint32_t g_area_frame = 0;                   // an area's frame: its place in the area's vtable
uintptr_t g_area_sys = 0;                    // sArea's instance (sBioArea), once the main system is surveyed
float g_clock_most = 0;                      // the most the HUD's countdown shows, in seconds
uintptr_t g_clock_seen = 0;                  // the stage whose countdown ran last tick, for the log

// The countdown running now: an area of a countdown's stage class whose seconds are
// above what it runs down to (its frame's own test).
struct Running {
  uintptr_t stage = 0;
  const StageClock* clock = nullptr;
  float left = 0, floor = 0;
};
bool running_clock(Running* out) {
  if (g_clocks.empty()) return false;
  if (!g_area_sys) {
    const uintptr_t s = game::system("sBioArea");
    if (!s || !mt::is_a(s, g_cls_areas)) return false;
    g_area_sys = s;
  }
  for (uint32_t i = 0; i < g_area_count; ++i) {
    const uintptr_t a = mem::read<uintptr_t>(g_area_sys + g_area_list + 4 * i);
    if (!a) break;
    const uintptr_t frame = mem::read<uintptr_t>(mem::read<uintptr_t>(a) + g_area_frame);
    for (const StageClock& c : g_clocks) {
      if (frame != c.frame) continue;
      const float left = mem::read<float>(a + c.time), floor = c.floor ? mem::read<float>(a + c.floor) : 0.0f;
      if (left <= floor) continue;
      *out = Running{a, &c, left, floor};
      return true;
    }
  }
  return false;
}

// Each match of the countdowns' subtraction lies in the frame of an area class - the
// nearest frame starting before it (sArea runs each of its areas' frames through one
// place in their vtables, read out of its own frame) - that shows the countdown on the
// HUD right after; what it runs down to is read out of the frame's test before it: a
// field of the stage's, or 0.
void install_stage_clocks() {
  const uintptr_t frame = game::sig_one("kSigAreaFrame"), hud = game::sig_one("kSigHudCountdown");
  const std::vector<uintptr_t> subs = game::sig_all("kSigStageCountdown");
  const mt::Class* area = mt::find("cArea");
  const mt::Class* areas = mt::find("sArea");
  const mt::Class* cockpit = mt::find("uIdCockpit00");
  const uint32_t slot = frame ? u8at(frame + 62) : 0, list = frame ? u32at(frame + 9) : 0;
  const uint32_t n = frame ? u8at(frame + 73) : 0;
  const uintptr_t most_at = hud ? u32at(hud + 10) : 0;
  const float most = image::rdata().contains(most_at) ? mem::read<float>(most_at) : 0.0f;
  if (!frame || !hud || subs.empty() || !area || !areas || !areas->vtable || !cockpit ||
      mem::read<uintptr_t>(areas->vtable + 4 * 6) != frame || !slot || slot % 4 || u8at(frame + 24) != u8at(frame + 54) ||
      !n || list + 4 * n > areas->size || u32at(hud + 21) != u32at(hud + 38) || u32at(hud + 21) + 4 > cockpit->size ||
      u32at(hud + 27) >= cockpit->size || !(most >= 60.0f && most <= 6000.0f)) {
    logf("cheats: the stages' countdowns are not as known - Freeze countdown timer leaves them alone");
    return;
  }
  int count = 0;
  const mt::Class* all = mt::classes(&count);
  for (const uintptr_t sub : subs) {
    const uint32_t time = u32at(sub + 9);
    const mt::Class* owner = nullptr;  // the area class whose frame this is
    uintptr_t fn = 0;
    for (int i = 0; i < count; ++i) {
      const mt::Class& c = all[i];
      if (!c.vtable || !mt::derives(c.dti, area->dti)) continue;
      const uintptr_t f = mem::read<uintptr_t>(c.vtable + slot);
      if (image::text().contains(f) && f <= sub && sub - f < 0x400 && f > fn) {
        owner = &c;
        fn = f;
      }
    }
    bool shown = false;
    for (uintptr_t p = sub; p < sub + 0x100 && !shown; ++p) shown = game::call_target(p) == hud;
    const unsigned b0 = time & 0xFF, b1 = (time >> 8) & 0xFF, b2 = (time >> 16) & 0xFF, b3 = time >> 24;
    char to_field[96], to_zero[96];  // movss xmm1,[ebx+floor]; movss xmm0,[ebx+time] / xorps xmm1,xmm1; comiss; jbe
    std::snprintf(to_field, sizeof to_field, "F3 0F 10 8B ?? ?? ?? ?? F3 0F 10 83 %02X %02X %02X %02X 0F 2F C1 0F 86",
                  b0, b1, b2, b3);
    std::snprintf(to_zero, sizeof to_zero, "F3 0F 10 83 %02X %02X %02X %02X 0F 57 C9 0F 2F C1 0F 86", b0, b1, b2, b3);
    const uintptr_t test_field = fn ? mem::find_pattern(mem::Range{fn, sub}, to_field) : 0;
    const uintptr_t test_zero = fn ? mem::find_pattern(mem::Range{fn, sub}, to_zero) : 0;
    StageClock c;
    c.floor = test_field ? u32at(test_field + 4) : 0;
    if (!owner || !shown || (!test_field && !test_zero) || u32at(sub + 25) != time || u32at(sub + 35) != time ||
        time + 4 > owner->size || c.floor + 4 > owner->size || c.floor == time ||
        !c.frozen.prepare(sub + 13, "F3 0F 5C 40 28", mem::nops(5), owner->name)) {
      logf("cheats: a stage's countdown at exe+0x%X is not as known - left alone",
           static_cast<unsigned>(sub - image::base()));
      continue;
    }
    c.frame = fn;
    c.name = owner->name;
    c.time = time;
    g_clocks.push_back(c);
    char down[32];
    if (c.floor) std::snprintf(down, sizeof down, "its +0x%X", c.floor);
    else std::snprintf(down, sizeof down, "0");
    logf("cheats: %s's countdown +0x%X runs down to %s in its frame exe+0x%X", c.name, c.time, down,
         static_cast<unsigned>(fn - image::base()));
  }
  if (g_clocks.empty()) return;
  g_cls_areas = areas;
  g_area_list = list;
  g_area_count = n;
  g_area_frame = slot;
  g_clock_most = most;
  logf("cheats: the stages' countdowns: %d, among sArea's areas (+0x%X, %u), shown on the HUD (exe+0x%X, up to %.0f "
       "seconds)",
       static_cast<int>(g_clocks.size()), list, n, static_cast<unsigned>(hud - image::base()), most);
}

// `call fn` at `site` (E8 rel32).
std::vector<uint8_t> call_to(uintptr_t site, const void* fn) {
  const auto rel = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(fn) - (site + 5));
  return {0xE8, static_cast<uint8_t>(rel), static_cast<uint8_t>(rel >> 8), static_cast<uint8_t>(rel >> 16),
          static_cast<uint8_t>(rel >> 24)};
}

// --- Steady aim -----------------------------------------------------------------------------
// The player's aim (cPadEx's, every frame of an aim) adds a "breathing" sway - a
// pitch and a yaw, each the sine of a phase that wanders at random, times its
// sizes - to the aim's angles with a pad, and to the aim camera with the mouse;
// and a shot kicks the aim (a random pitch up and yaw either way, from the
// weapon's numbers) and lets it back after. The shots fly straight to where the
// aim points (only a shotgun's pellets spread). Steady aim makes the sway nothing
// (each one's last multiply gives 0), the kick nothing (its two calls give 0), and
// keeps the mouse crosshair from opening at a shot - it is only drawn wider.
mem::Patch g_sway_pitch, g_sway_yaw;  // mulss xmm0,[esp+n] -> xorps xmm0,xmm0
mem::Patch g_kick_pitch, g_kick_yaw;  // the kick's calls (their arguments too) -> fldz
mem::Patch g_crosshair_open;          // the crosshair's opening returns at once

// ... and through a scope. A scope's view is the gun's own: the scope camera sits at
// the gun model's sight and looks down its barrel (eased toward it each frame). The
// gun is in the character's hands, posed by the aim's motions - loops on the model's
// layers (the aim's three, blended by its height, and the stance's three) whose
// keyframes breathe: 80 frames in which the spine, the shoulders and the arms turn by
// up to a few degrees. With the sway gone, the view through a scope still rose and
// fell with that breath. So while a scope is up for a character this machine's pad
// drives, its looping motions are held (their speed 0): the pose stays where the aim
// puts it - a layer's blend-in still runs, it does not go by the speed - and the
// character is not drawn meanwhile. A motion that plays once (a shot, a reload) is
// never held. A held loop gets its speed back as the scope comes down or the switch
// goes off - unless the game has put another motion on the layer since.
struct Stance {
  uint32_t scope = 0;                        // uPlayer: its scope is up (a byte)
  uint32_t layers = 0, layer_size = 0;       // uModel: its motion layers, one's size
  uint32_t in_use = 0;                       // uModel: how many of them its clock runs
  uint32_t motion = 0, speed = 0, attr = 0;  // a layer: its motion's number (0xFFFF none), speed, attributes
  uint32_t once = 0;                         // the attribute bit of a motion that plays once
};
Stance g_stance;
bool g_stance_ok = false;
constexpr int kLayers = 8;  // uModel makes eight (and no model is given more)
struct Held {
  uintptr_t obj = 0;
  uint32_t mask = 0;  // the layers held
  uint16_t motion[kLayers] = {};
  float speed[kLayers] = {};
};
Held g_held[game::kMaxChars];
int g_holds_logged = 0;

void hold_stance(const game::Run& run) {
  // A character no longer in play is forgotten without a write: it may be gone.
  for (Held& h : g_held) {
    bool here = false;
    for (int i = 0; i < run.nchars; ++i) here = here || run.chars[i].obj == h.obj;
    if (!here) h = Held{};
  }
  for (int i = 0; i < run.nchars; ++i) {
    const game::Character& c = run.chars[i];
    const bool scoped = g_on[sw::kSteadyAim] && g_ok[sw::kSteadyAim] && c.control == game::Control::kLocal &&
                        mem::read<uint8_t>(c.obj + g_stance.scope) != 0;
    Held* h = nullptr;
    for (Held& x : g_held)
      if (x.obj == c.obj) h = &x;
    for (Held& x : g_held)
      if (!h && scoped && !x.obj) {
        h = &x;
        h->obj = c.obj;
      }
    if (!h) continue;
    const uint32_t was = h->mask;
    const uint32_t in_use = mem::read<uint32_t>(c.obj + g_stance.in_use);
    for (uint32_t k = 0; k < kLayers; ++k) {  // every model has all eight
      const uintptr_t m = c.obj + g_stance.layers + k * g_stance.layer_size;
      const uint16_t motion = mem::read<uint16_t>(m + g_stance.motion);
      const float speed = mem::read<float>(m + g_stance.speed);
      const bool loops = k < in_use && motion != 0xFFFF && !(mem::read<uint16_t>(m + g_stance.attr) & g_stance.once);
      const uint32_t bit = 1u << k;
      if (scoped && loops) {
        if (speed != 0) {  // a loop running, or started over: held where it is
          h->motion[k] = motion;
          h->speed[k] = speed;
          h->mask |= bit;
          mem::write<float>(m + g_stance.speed, 0.0f);
        }
      } else if (h->mask & bit) {
        if (motion == h->motion[k] && speed == 0) mem::write<float>(m + g_stance.speed, h->speed[k]);
        h->mask &= ~bit;
      }
    }
    if (!was && h->mask && g_holds_logged < 3) {
      ++g_holds_logged;
      char held[96] = "";
      for (int k = 0, n = 0; k < kLayers; ++k)
        if (h->mask & (1u << k))
          n += snprintf(held + n, sizeof held - n, " %d:%X", k, static_cast<unsigned>(h->motion[k]));
      logf("cheats: a scope is up - %s's looping motions held (layer:motion%s)", c.name, held);
    }
    if (!scoped && !h->mask) *h = Held{};
  }
}

// --- Automatic QTEs -------------------------------------------------------------------------
// Every prompt on screen is a request (cActionButton) that a player's button
// manager (sActionButton) checks every frame through one function, the unit's
// check: "pressed" lets the request's owner act. The game has an automatic mode
// of its own, the AUTO of its Action Assist option: two tests of the profile's
// setting, "AUTO?" and "not OFF?", make the check answer the cutscenes' QTEs (a
// quarter second in) and the dodges and grabs of a few categories at once, and
// pick the easiest buttons and the longest windows. Automatic QTEs makes both
// tests answer yes - the setting itself is left as it is - and answers, at the
// managers' call of the check, the QTEs that AUTO leaves to the player: the grabs'
// Break free, the enemies' and the bosses' dodges, holds and blows - and a crank's
// Turn, and the volcano boulder's punches.
struct QteLayout {
  uint32_t request = 0, drawn = 0, kind = 0;      // the unit's request, "draw the prompt", its kind of input
  uint32_t answered = 0, type = 0, category = 0;  // the request's "answered" flag, its button, its category
  uint32_t label = 0, seconds = 0;                // its label (a line of mes_system_e), the seconds it has been up
};
QteLayout g_qte;
using QteCheckFn = bool(__thiscall*)(void* unit, void* pad, int player);
QteCheckFn g_qte_check = nullptr;
mem::Patch g_qte_call;                  // the managers' call of the check, to hk_qte_check
mem::Patch g_assist_auto, g_assist_on;  // the Action Assist tests: mov al,1; ret

// A crank (uOmCrank: a gate's or a door's, open as far as it is wound) puts up two
// prompts while a player holds it: Turn - the stick turned round (kind 7; on the
// keyboard A and D in turn), the cranks' category (5) - and Release (category 2, a
// press). Each Turn answered marks the crank's slot of the moment in a ring of its
// last 24 or 32 thirtieths of a second, and it winds as fast as the ring is full:
// answered every frame, at the game's own top speed, up to its most - where the
// Turn is no longer offered and the player holds it open until Release (a crank
// that stays open lets go by itself). The only other prompt of the stick turned
// round, a kick's Push (category 8), AUTO answers itself after two seconds.
constexpr uint32_t kStickTurned = 7, kCrankCategory = 0x05;
uintptr_t g_crank_turn = 0;  // the last crank's Turn answered, logged once
int g_cranks_logged = 0;

// The volcano's boulder (uOm095e_Kick, stage 508 of chapter 6-3), once the player has
// taken hold of it (its Push, a context action): Push (a button held), Push (the stick
// turned round), five Hooks and a Ram - category 8's prompts, whose only other is a
// laser mirror's Turn (the stick held to one side). AUTO answers the two Pushes itself
// (labels 0xB5 and 0xC0, read out of its code): hidden, and every frame once up two
// seconds, each answer a step of the push. Each Hook (0xB7) and the Ram (0xB6) it makes
// one plain press, which it leaves to the player: each a prompt of its own, the next
// punch's made as one is answered (the action starts again - the punch's motion with
// it), against a time limit (10 s for the five Hooks, 2 s for the Ram) that fails the
// push when it runs out. The mod answers those, hidden, once each has been up
// kBlowWait - the punch before it given that long, the five well inside their limit.
constexpr uint32_t kBoulderCategory = 0x08, kPushTurned = 0xB5, kRam = 0xB6, kHook = 0xB7, kPushHeld = 0xC0;
constexpr float kBlowWait = 0.75f;
bool g_boulder_ok = false;  // the request's label and clock found, AUTO's two Pushes as known
int g_blows_logged = 0;     // the first answers are logged (each Hook's, the Ram's)

bool is_boulder_blow(uint32_t category, uint32_t label) {
  return category == kBoulderCategory && (label == kHook || label == kRam);
}

// A QTE: its button one of the QTE icons (types 9-0x1C: a button, two at once, a
// mash, a hold, the stick), in a category that holds QTEs - dodges, grabs, the
// bosses' prompts - and not the stick held to one side (kinds 8-9: the laser
// mirrors' turning, a switch) nor turned round (7) but for a crank's Turn. The
// other categories hold the context actions (doors, items, melee, the partner's
// help and commands, a vehicle's switch of places, a crank's Release); AUTO
// answers the cutscenes' (category 1) and the boulder's Pushes (8) itself, a moment in.
bool is_qte(uint32_t category, uint32_t type, uint32_t kind) {
  if (type < 0x09 || type > 0x1C) return false;
  if (kind >= 7 && kind <= 9) return kind == kStickTurned && category == kCrankCategory;
  switch (category) {
    case 0x00: case 0x03: case 0x04: case 0x06: case 0x07: case 0x0B: case 0x0C: case 0x0E:
    case 0x10: case 0x12: case 0x13: case 0x14: case 0x16: case 0x19: case 0x3C:
      return true;
    default:
      return false;
  }
}

// The check for a QTE of a character this machine's pad drives: "pressed", every
// frame its prompt is up (a hold held, a mash done, a crank turning), and not drawn
// - as AUTO does. The boulder's Hooks and its Ram: not drawn, the game's check asked
// (a press of the player's own still counts), and "pressed" once the prompt has been
// up kBlowWait.
bool __thiscall hk_qte_check(void* self, void* pad, int player) {
  const auto unit = reinterpret_cast<uintptr_t>(self);
  if (g_on[sw::kAutoQte]) {
    const uintptr_t req = mem::read<uintptr_t>(unit + g_qte.request);
    const uintptr_t chara = game::character_at(player);
    if (req && !mem::read<uint8_t>(req + g_qte.answered) && chara &&
        mem::read<uint32_t>(chara + game::layout().control) == static_cast<uint32_t>(game::Control::kLocal)) {
      const uint32_t kind = mem::read<uint32_t>(unit + g_qte.kind);
      const uint32_t category = mem::read<uint32_t>(req + g_qte.category);
      if (is_qte(category, mem::read<uint32_t>(req + g_qte.type), kind)) {
        *reinterpret_cast<uint8_t*>(unit + g_qte.drawn) = 0;
        if (kind == kStickTurned && req != g_crank_turn && g_cranks_logged < 10) {
          g_crank_turn = req;
          ++g_cranks_logged;
          logf("cheats: a crank's Turn answered for character %d - it winds itself fully open; Release is the "
               "player's", player);
        }
        return true;
      }
      const uint32_t label = g_boulder_ok ? mem::read<uint32_t>(req + g_qte.label) : 0;
      if (g_boulder_ok && is_boulder_blow(category, label)) {
        const bool pressed = g_qte_check(self, pad, player);
        *reinterpret_cast<uint8_t*>(unit + g_qte.drawn) = 0;
        if (pressed || mem::read<float>(req + g_qte.seconds) < kBlowWait) return pressed;
        if (g_blows_logged < 12) {
          ++g_blows_logged;
          logf("cheats: the boulder's %s answered for character %d, up %.2f s", label == kRam ? "Ram" : "Hook",
               player, static_cast<double>(mem::read<float>(req + g_qte.seconds)));
        }
        return true;
      }
    }
  }
  return g_qte_check(self, pad, player);
}

// --- Open map at chapter start --------------------------------------------------------------
// Each character has a map of its own (uIdMap00), made with the character as it starts -
// in every area - and shown in a corner of the screen while its "open" flag is set. The
// map key flips the flag (the toggle mode its constructor sets) and keeps it in the play
// state (cGameSys: a byte for each player), from which the next area's map takes it as it
// sets up. The scenario may force a map open for a moment (the key then shuts it) or keep
// it shut for a while (the flag stays as it is, the map hidden). The game clears those
// bytes only as it goes back to the menus (cGameSys's reset) or starts a round of the
// Mercenaries or of No Mercy: a story chapter begins with the map as the last one left it -
// shut, coming from the menus. A chapter's start is the game's own: the profile's
// mTempBase.mChapterStart, set as a chapter is set to begin from its beginning (CHAPTER
// SELECT, NEW GAME, the next chapter after the results, CONTINUE or a retry from a point the
// game takes for one) and cleared by the play flow as play begins and the chapter's title
// comes up. The extra episodes (Lost in Nightmares, Desperate Escape) begin the same way:
// their own start sets the field (FUN_00BF55E0) and the same play flow clears it. As the
// tick sees it cleared - a game in play, a story chapter or an extra episode - each
// character this machine's pad drives has its map opened as the key would open it: the
// byte in the play state and the map's own flag. The key shuts it again as ever, and the
// game keeps it so until the next chapter begins. The Mercenaries' rounds set and clear
// the field too; they, Reunion's and Versus' are left alone.
struct MapLayout {
  uint32_t chapter_start = 0;   // cGameInfo: mTempBase.mChapterStart (32-bit)
  uint32_t sys = 0;             // sBioMain: cGameSys, the play state
  uint32_t mode = 0;            // cGameSys: the game's mode (game::kModeStory ...)
  uint32_t kept = 0;            // cGameSys: each player's map open, a byte each
  uint32_t players = 0;         // ... how many
  uint32_t chara_map = 0;       // uPlayer: its map (uIdMap00)
  uint32_t chara_player = 0;    // uPlayer: its player's number (a byte), the map's player
  uint32_t open = 0;            // uIdMap00: open (a byte)
  uint32_t toggles = 0;         // uIdMap00: the map key toggles "open" (a byte; else it is held)
};
MapLayout g_map;
const mt::Class* g_cls_map = nullptr;      // uIdMap00
const mt::Class* g_cls_gamesys = nullptr;  // cGameSys
bool g_chapter_set = false;                // the chapter's start seen set: a chapter is to begin
int g_maps_logged = 0, g_starts_left_logged = 0;

// The games whose start opens the map: a story chapter's and an extra episode's.
bool chapter_mode(uint32_t mode) {
  return mode == game::kModeStory || mode == game::kModeNightmares || mode == game::kModeEscape;
}

// The tick's part: a chapter's start seen set, then cleared.
void open_maps(const game::Run& run) {
  if (!g_ok[sw::kOpenMap]) return;
  const uintptr_t gi = game::game_info();
  if (!gi) return;
  if (mem::read<uint32_t>(gi + g_map.chapter_start)) {
    g_chapter_set = true;
    return;
  }
  if (!g_chapter_set) return;
  g_chapter_set = false;  // the chapter has begun - or, out of a game, the menus let the start go
  if (!g_on[sw::kOpenMap] || !run.main) return;
  // A start that opens nothing is logged (the first ten), so that a report can say why.
  if (!run.in_game) {
    if (g_starts_left_logged < 10) {
      ++g_starts_left_logged;
      logf("cheats: a chapter's start cleared with no character in play (the menus let it go) - no map opened");
    }
    return;
  }
  const uintptr_t sys = mem::read<uintptr_t>(run.main + g_map.sys);
  if (!sys || !mt::is_a(sys, g_cls_gamesys)) return;
  const uint32_t mode = mem::read<uint32_t>(sys + g_map.mode);
  if (!chapter_mode(mode)) {
    if (g_starts_left_logged < 10) {
      ++g_starts_left_logged;
      logf("cheats: a start in the game's mode %u - no map opened (only a story chapter's or an extra episode's)", mode);
    }
    return;
  }
  for (int i = 0; i < run.nchars; ++i) {
    const game::Character& c = run.chars[i];
    if (c.control != game::Control::kLocal) continue;
    const uint32_t player = mem::read<uint8_t>(c.obj + g_map.chara_player);
    if (player >= g_map.players) continue;
    mem::write<uint8_t>(sys + g_map.kept + player, 1);
    const uintptr_t map = mem::read<uintptr_t>(c.obj + g_map.chara_map);
    const bool unit = map && mt::is_a(map, g_cls_map) && mem::read<uint8_t>(map + g_map.toggles);
    if (unit) mem::write<uint8_t>(map + g_map.open, 1);
    if (g_maps_logged < 10) {
      ++g_maps_logged;
      logf("cheats: a chapter has begun - %s's map opened (player %u%s)", c.name, player,
           unit ? "" : "; its map takes it as it sets up");
    }
  }
}

// Open map at chapter start, found: the play flow's chapter start (the profile's field it
// tests and clears, cGameSys and its mode; the chapter's title it makes, by its class
// record), the map's restore of its flag as it sets up and its toggle (the same flag,
// player and play state; the getter's and the setter's array of bytes, the same), and a
// character's start making its map (the maker making a uIdMap00) into the character's
// field.
void install_open_map() {
  const game::Layout& l = game::layout();
  const uintptr_t start = game::sig_one("kSigChapterStart"), restore = game::sig_one("kSigMapRestore");
  const uintptr_t toggle = game::sig_one("kSigMapToggle"), make = game::sig_one("kSigMapMake");
  const mt::Class* map = mt::find("uIdMap00");
  const mt::Class* sys = mt::find("cGameSys");
  const mt::Class* title = mt::find("uIdChapSt00");
  const mt::Class* main = mt::find("sBioMain");
  const mt::Class* player = mt::find("uPlayer");
  if (!start || !restore || !toggle || !make || !map || !map->vtable || !sys || !title || !main || !player) {
    logf("cheats: the map or a chapter's start was not found - no Open map at chapter start");
    return;
  }
  MapLayout m;
  const uint32_t field = u32at(start + 20);  // sInfoMan: the profiles' cGameInfo from game_base
  m.chapter_start = field - l.game_base;
  m.sys = u32at(start + 53);
  m.mode = u8at(start + 62);
  m.toggles = u8at(restore + 2);
  m.open = u8at(restore + 33);
  const uint32_t map_player = u32at(restore + 10);
  m.chara_player = u32at(make + 8);
  m.chara_map = u32at(make + 50);
  // The getter's and the setter's bodies: (player) -> open / (player, open) on the bytes from
  // +kept, the player checked against their last.
  const uintptr_t get = game::call_target(restore + 26), put = game::call_target(toggle + 68);
  const bool get_ok = image::text().contains(get) &&
                      mem::matches(get, mem::parse_pattern("8B 44 24 04 83 F8 ?? 77 0A 8A 84 08 ?? ?? ?? ?? C2 04 00 32 "
                                                           "C0 C2 04 00"));
  const bool put_ok = image::text().contains(put) &&
                      mem::matches(put, mem::parse_pattern("8B 44 24 04 83 F8 ?? 77 0B 8A 54 24 08 88 94 08 ?? ?? ?? ?? "
                                                           "C2 08 00"));
  m.kept = get_ok ? u32at(get + 12) : 0;
  m.players = get_ok ? u8at(get + 6) + 1 : 0;
  // The maker: a unit of uIdMap00's size, its constructor giving it uIdMap00's vtable.
  const uintptr_t maker = game::call_target(make);
  const bool maker_ok = image::text().contains(maker) &&
                        mem::matches(maker, mem::parse_pattern("8B 0D ?? ?? ?? ?? 8B 01 8B 50 14 56 6A ?? 68 ?? ?? ?? ?? "
                                                               "FF D2 85 C0 74 ?? 8B C8 E8")) &&
                        u32at(maker + 15) == map->size;
  const uintptr_t ctor = maker_ok ? game::call_target(maker + 27) : 0;
  char vt[40];
  std::snprintf(vt, sizeof vt, "C7 06 %02X %02X %02X %02X", static_cast<unsigned>(map->vtable & 0xFF),
                static_cast<unsigned>((map->vtable >> 8) & 0xFF), static_cast<unsigned>((map->vtable >> 16) & 0xFF),
                static_cast<unsigned>(map->vtable >> 24));
  const bool ctor_ok = image::text().contains(ctor) && mem::find_pattern(mem::Range{ctor, ctor + 0x20}, vt) != 0;
  // The chapter's start clears the field it tests, and makes the chapter's title.
  char clear[48];
  std::snprintf(clear, sizeof clear, "C7 84 38 %02X %02X %02X %02X 00 00 00 00", field & 0xFF, (field >> 8) & 0xFF,
                (field >> 16) & 0xFF, field >> 24);
  const bool cleared = mem::find_pattern(mem::Range{start, start + 0x140}, clear) != 0;
  const bool titled = u8at(start + 146) == 0xB9 && u32at(start + 147) == title->dti && u8at(start + 151) == 0xE8;
  if (!get_ok || !put_ok || !maker_ok || !ctor_ok || !cleared || !titled || u8at(start + 10) != l.profile_index ||
      u32at(start + 13) != l.game_size || field <= l.game_base || m.chapter_start + 4 > l.game_size ||
      u8at(toggle + 2) != m.toggles || u8at(toggle + 28) != m.open || u8at(toggle + 38) != m.open ||
      u8at(toggle + 44) != m.open || u8at(toggle + 48) != m.open || u32at(toggle + 51) != map_player ||
      u32at(make + 14) != map_player || u32at(restore + 21) != m.sys || u32at(toggle + 63) != m.sys ||
      u32at(put + 16) != m.kept || u8at(put + 6) + 1 != m.players || m.players > 8 || m.toggles == m.open ||
      m.open >= map->size || m.toggles >= map->size || map_player + 4 > map->size || m.sys + 4 > main->size ||
      m.mode + 4 > sys->size || m.kept + m.players > sys->size || m.chara_map + 4 > player->size ||
      m.chara_player >= player->size) {
    logf("cheats: the map or a chapter's start is not as known - no Open map at chapter start");
    return;
  }
  g_map = m;
  g_cls_map = map;
  g_cls_gamesys = sys;
  g_ok[sw::kOpenMap] = true;
  logf("cheats: a chapter's start: cGameInfo +0x%X, cleared as its title (uIdChapSt00) comes up (exe+0x%X); the play "
       "state cGameSys (sBioMain +0x%X; its mode +0x%X: %d the story, %d and %d the extra episodes) keeps each "
       "player's map open (+0x%X, %u); a character's map uPlayer +0x%X (uIdMap00: open +0x%X, toggled by the key "
       "while +0x%X; its player's number uPlayer +0x%X)",
       m.chapter_start, static_cast<unsigned>(start - image::base()), m.sys, m.mode, game::kModeStory,
       game::kModeNightmares, game::kModeEscape, m.kept, m.players, m.chara_map, m.open, m.toggles, m.chara_player);
}

void follow(mem::Patch& p, bool on) {
  if (!p.prepared()) return;
  if (on && !p.applied) {
    if (!p.apply()) logf("cheats: the %s site changed under us - left alone", p.label);
  } else if (!on && p.applied) {
    p.revert();
  }
}

void unavailable(sw::Kind k, const char* why) {
  g_ok[k] = false;
  g_why[k] = why;
}

// --- what the panel shows, and its edits -------------------------------------------------
CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
Status g_status;
struct Pending {
  bool set = false;
  int chara = 0;
  double value = 0;
};
constexpr int kEdits = static_cast<int>(Edit::kCount);
Pending g_edit[kEdits];

}  // namespace

void init() {
  for (int i = 0; i < sw::kCount; ++i) g_on[i] = config::get().on[i] ? 1 : 0;
  for (int i = 0; i < sw::kCount; ++i) {
    g_ok[i] = false;
    g_why[i] = "the game was not understood (see the log)";
  }
  InitializeCriticalSection(&g_lock);
  g_lock_ready = true;
}

bool install() {
  if (!game::ready()) return false;
  const mt::Class* player = mt::find("uPlayer");
  const mt::Class* enemy = mt::find("uEnemy");
  const mt::Class* npc = mt::find("uEmNpc");
  // The damage hooks (in before any character exists).
  if (player && player->vtable) {
    g_player_damage = reinterpret_cast<PlayerDamageFn>(mem::read<uintptr_t>(player->vtable + 4 * kDamageSlot));
    const int n = hook_vtables(player, nullptr, kDamageSlot, reinterpret_cast<uintptr_t>(g_player_damage),
                               reinterpret_cast<void*>(&hk_player_damage), &g_player_entries);
    logf("cheats: the players' damage hooked in %d vtables", n);
    if (n) g_ok[sw::kGodMode] = g_ok[sw::kPartnerGod] = true;
  }
  // The enemies' subtract-health: the slot uEnemy's damage calls on through (game.cpp
  // checks that), holding the function the signature finds, on the same health.
  const uintptr_t sub = game::sig_one("kSigEnemySubtract");
  const game::Layout& l = game::layout();
  if (enemy && enemy->vtable && sub && mem::read<uintptr_t>(enemy->vtable + 4 * kSubtractSlot) == sub &&
      u32at(sub + 8) == l.hp && u32at(sub + 15) == l.hp && u32at(sub + 27) == l.hp && u32at(sub + 36) == l.hp_max &&
      u32at(sub + 43) == l.hp && u32at(sub + 52) == l.hp) {
    g_enemy_subtract = reinterpret_cast<EnemySubtractFn>(sub);
    const int n = hook_vtables(enemy, npc, kSubtractSlot, sub, reinterpret_cast<void*>(&hk_enemy_subtract),
                               &g_enemy_entries);
    logf("cheats: the enemies' subtract-health exe+0x%X hooked in %d vtables (the story's companions left alone)",
         static_cast<unsigned>(sub - image::base()), n);
    if (n) g_ok[sw::kOneHitKills] = true;
  } else {
    logf("cheats: the enemies' subtract-health was not found - One hit kills is off");
  }
  // ... and on the Ndesu and its Plagas: the hit path's check (its two calls, one
  // place in the vtable), theirs there; the field each takes the hit into, and the
  // Ndesu's gauge the Plaga's takes it off as well - the same flags field in both.
  const uintptr_t ask = game::sig_one("kSigHitCheck");
  const uint32_t check_at = ask ? u32at(ask + 4) : 0;
  const mt::Class* ndesu = mt::find("uEm45");
  const mt::Class* plaga = mt::find("uEm4502");
  const uintptr_t ndesu_tail = game::sig_one("kSigNdesuHit"), plaga_check = game::sig_one("kSigPlagaHit");
  const uintptr_t ndesu_check = ndesu && ndesu->vtable && check_at ? mem::read<uintptr_t>(ndesu->vtable + check_at) : 0;
  const uint32_t gauge = ndesu_tail ? u32at(ndesu_tail + 60) : 0, flags = ndesu_tail ? u32at(ndesu_tail + 2) : 0;
  if (ask && check_at && check_at % 4 == 0 && u32at(ask + 44) == check_at && enemy && ndesu && plaga &&
      plaga->vtable && mt::derives(ndesu->dti, enemy->dti) && mt::derives(plaga->dti, enemy->dti) && ndesu_tail &&
      plaga_check && image::text().contains(ndesu_check) && ndesu_check < ndesu_tail &&
      ndesu_tail - ndesu_check < 0x200 && mem::read<uintptr_t>(plaga->vtable + check_at) == plaga_check &&
      u32at(ndesu_tail + 116) == gauge && u32at(plaga_check + 61) == gauge && u32at(plaga_check + 73) == gauge &&
      u32at(plaga_check + 2) == flags && u32at(plaga_check + 32) == flags && u32at(plaga_check + 99) == flags &&
      gauge + 4 <= ndesu->size && u32at(plaga_check + 15) + 4 <= plaga->size) {
    g_ndesu_hit = reinterpret_cast<HitCheckFn>(ndesu_check);
    g_plaga_hit = reinterpret_cast<HitCheckFn>(plaga_check);
    g_ndesu_gauge = gauge;
    g_plaga_count = u32at(plaga_check + 15);
    const int slot = static_cast<int>(check_at / 4);
    const int n = hook_vtables(ndesu, nullptr, slot, ndesu_check, reinterpret_cast<void*>(&hk_ndesu_hit),
                               &g_ndesu_entries) +
                  hook_vtables(plaga, nullptr, slot, plaga_check, reinterpret_cast<void*>(&hk_plaga_hit),
                               &g_plaga_entries);
    logf("cheats: an enemy's hit check (slot %d): the Ndesu's exe+0x%X (its gauge +0x%X) and its Plagas' exe+0x%X "
         "(their count +0x%X) hooked in %d vtables",
         slot, static_cast<unsigned>(ndesu_check - image::base()), gauge,
         static_cast<unsigned>(plaga_check - image::base()), g_plaga_count, n);
  } else {
    logf("cheats: the Ndesu's and its Plagas' hit checks are not as known - One hit kills leaves them to the game");
  }
  // ... and on Irving and its tentacles: each check wears a region of its own through the
  // enemies' region wear, whose fields (12 bytes a region) are read out of the wear.
  const mt::Class* irving = mt::find("uEm46");
  const mt::Class* tentacle = mt::find("uEmc0");
  const uintptr_t wear = game::sig_one("kSigRegionWear"), irving_check = game::sig_one("kSigIrvingHit"),
                  tentacle_check = game::sig_one("kSigTentacleHit");
  const uint32_t left0 = wear ? u32at(wear + 19) : 0, worn0 = wear ? u32at(wear + 39) : 0;
  const uint32_t ri = irving_check ? u8at(irving_check + 60) : 0, rt = tentacle_check ? u8at(tentacle_check + 69) : 0;
  const uint32_t body = irving_check ? mem::read<uint16_t>(irving_check + 9) : 0;
  if (ask && check_at && check_at % 4 == 0 && u32at(ask + 44) == check_at && enemy && irving && tentacle &&
      irving->vtable && tentacle->vtable && mt::derives(irving->dti, enemy->dti) &&
      mt::derives(tentacle->dti, enemy->dti) && wear && irving_check && tentacle_check &&
      mem::read<uintptr_t>(irving->vtable + check_at) == irving_check &&
      mem::read<uintptr_t>(tentacle->vtable + check_at) == tentacle_check &&
      game::call_target(irving_check + 63) == wear && game::call_target(tentacle_check + 72) == wear &&
      u32at(wear + 60) == left0 && u32at(wear + 82) == left0 && u32at(wear + 108) == left0 &&
      u32at(wear + 76) == worn0 && worn0 + 4 == left0 && 4 * u32at(wear + 99) == left0 + 4 &&
      ri <= u8at(wear + 9) && rt <= u8at(wear + 9) && u32at(tentacle_check + 55) == left0 + 12 * rt &&
      u32at(tentacle_check + 27) == left0 + 4 + 12 * rt && body &&
      static_cast<uint32_t>(-static_cast<int8_t>(u8at(irving_check + 90))) == body &&
      u32at(irving_check + 22) == u32at(irving_check + 96) && u32at(irving_check + 22) == u32at(tentacle_check + 11) &&
      game::call_target(irving_check + 26) == game::call_target(irving_check + 101) &&
      game::call_target(irving_check + 26) == game::call_target(tentacle_check + 15) &&
      u32at(irving_check + 84) == u32at(irving_check + 108) && u32at(irving_check + 114) == u32at(wear + 47) &&
      left0 + 12 * ri + 4 <= irving->size && left0 + 12 * rt + 4 <= tentacle->size) {
    g_irving_hit = reinterpret_cast<HitCheckFn>(irving_check);
    g_tentacle_hit = reinterpret_cast<HitCheckFn>(tentacle_check);
    g_irving_body = {worn0 + 12 * ri, left0 + 12 * ri};
    g_tentacle_body = {worn0 + 12 * rt, left0 + 12 * rt};
    g_irving_parts = body;
    const int slot = static_cast<int>(check_at / 4);
    const int n = hook_vtables(irving, nullptr, slot, irving_check, reinterpret_cast<void*>(&hk_irving_hit),
                               &g_irving_entries) +
                  hook_vtables(tentacle, nullptr, slot, tentacle_check, reinterpret_cast<void*>(&hk_tentacle_hit),
                               &g_tentacle_entries);
    logf("cheats: Irving's hit check exe+0x%X (its body's parts below %u: region %u, left +0x%X) and its tentacles' "
         "exe+0x%X (region %u, left +0x%X) hooked in %d vtables",
         static_cast<unsigned>(irving_check - image::base()), body, ri, g_irving_body.left,
         static_cast<unsigned>(tentacle_check - image::base()), rt, g_tentacle_body.left, n);
  } else {
    logf("cheats: Irving's and its tentacles' hit checks are not as known - One hit kills leaves them to the game");
  }
  // ... and on the Reaper: its check, and its damage two places on in the vtable, which sets
  // the check's two marks for the game's own instant kill (any other hit: uEnemy's damage).
  // The check's capped path takes health through subtract-health's slot.
  const mt::Class* reaper = mt::find("uEm34");
  const uintptr_t reaper_check = game::sig_one("kSigReaperHit"), reaper_kill = game::sig_one("kSigReaperKill");
  const uint32_t through = reaper_check ? u32at(reaper_check + 42) : 0;
  const uint32_t whole = reaper_check ? u32at(reaper_check + 48) : 0;
  const uintptr_t capped = reaper_check ? game::call_target(reaper_check + 147) : 0;
  if (ask && check_at && check_at % 4 == 0 && u32at(ask + 44) == check_at && enemy && enemy->vtable && reaper &&
      reaper->vtable && mt::derives(reaper->dti, enemy->dti) && reaper_check && reaper_kill &&
      mem::read<uintptr_t>(reaper->vtable + check_at) == reaper_check &&
      mem::read<uintptr_t>(reaper->vtable + check_at + 8) == reaper_kill &&
      jmp_target(reaper_kill + 58) == mem::read<uintptr_t>(enemy->vtable + check_at + 8) && through != whole &&
      u32at(reaper_check + 88) == through && u32at(reaper_check + 107) == through &&
      u32at(reaper_check + 121) == through && u32at(reaper_check + 94) == whole &&
      u32at(reaper_check + 115) == whole && u32at(reaper_kill + 20) == through && u32at(reaper_kill + 26) == whole &&
      u32at(reaper_kill + 47) == whole && u32at(reaper_check + 130) == l.hp && image::text().contains(capped) &&
      mem::matches(capped, mem::parse_pattern("8B 01 8B 80 ?? ?? ?? ?? FF E0")) &&
      u32at(capped + 4) == 4 * kSubtractSlot && u8at(reaper_check + 8) && through < reaper->size &&
      whole < reaper->size) {
    g_reaper_hit = reinterpret_cast<HitCheckFn>(reaper_check);
    g_reaper_parts = u8at(reaper_check + 8);
    g_reaper_through = through;
    g_reaper_whole = whole;
    const int slot = static_cast<int>(check_at / 4);
    const int n = hook_vtables(reaper, nullptr, slot, reaper_check, reinterpret_cast<void*>(&hk_reaper_hit),
                               &g_reaper_entries);
    const unsigned weak = static_cast<unsigned>(-static_cast<int8_t>(u8at(reaper_check + 78)));
    merc::set_weak_spot(reaper, static_cast<int>(weak));  // where Kill enemies as they appear hits one
    logf("cheats: the Reaper's hit check exe+0x%X (health only from its weak spot, parts %u-%u of its body's %u; its "
         "marks +0x%X and +0x%X, both set by its damage exe+0x%X for its own instant kill) hooked in %d vtable(s)",
         static_cast<unsigned>(reaper_check - image::base()), weak, weak + u8at(reaper_check + 81), g_reaper_parts,
         through, whole, static_cast<unsigned>(reaper_kill - image::base()), n);
  } else {
    logf("cheats: the Reaper's hit check is not as known - One hit kills leaves it to the game");
  }
  // ... and on Uroboros Aheri: its check, its cores' wear (both calls of it the check's), the
  // part where the cores start agreed by what the check takes off a core's part, and every
  // field it touches within the class.
  const mt::Class* aheri = mt::find("uEm42");
  const uintptr_t aheri_check = game::sig_one("kSigAheriHit"), core_wear = game::sig_one("kSigCoreWear");
  if (aheri_check && core_wear) {
    AheriLayout a;
    a.parts = u8at(aheri_check + 10);
    a.cores = u8at(aheri_check + 155) + 1;
    a.flags = u32at(aheri_check + 164);
    a.beat = u8at(aheri_check + 168);
    a.escape = u32at(aheri_check + 177);
    a.escape_most = u32at(aheri_check + 187);
    a.core_hp = u32at(core_wear + 43);
    a.core_step = 1u << (u8at(core_wear + 42) >> 6);  // [ecx+edx*n+health]: its scale
    const uint32_t hp_at[] = {51, 64, 82, 92, 100};
    bool hp_agreed = (u8at(core_wear + 42) & 0x3F) == 0x11 && u32at(core_wear + 74) == a.core_hp + 2;
    for (uint32_t o : hp_at) hp_agreed = hp_agreed && u32at(core_wear + o) == a.core_hp;
    if (ask && check_at && check_at % 4 == 0 && u32at(ask + 44) == check_at && enemy && aheri && aheri->vtable &&
        mt::derives(aheri->dti, enemy->dti) && mem::read<uintptr_t>(aheri->vtable + check_at) == aheri_check &&
        game::call_target(aheri_check + 330) == core_wear && game::call_target(aheri_check + 341) == core_wear &&
        a.parts && 0x10000u - mem::read<uint16_t>(aheri_check + 34) == a.parts && a.cores <= 8 && a.beat &&
        !(a.beat & (a.beat - 1)) && u32at(aheri_check + 183) == a.escape && a.escape_most && hp_agreed &&
        a.flags < aheri->size && a.escape + 4 <= aheri->size &&
        a.core_hp + a.core_step * a.cores <= aheri->size) {
      g_aheri = a;
      g_aheri_hit = reinterpret_cast<HitCheckFn>(aheri_check);
      const int n = hook_vtables(aheri, nullptr, static_cast<int>(check_at / 4), aheri_check,
                                 reinterpret_cast<void*>(&hk_aheri_hit), &g_aheri_entries);
      logf("cheats: Uroboros Aheri's hit check exe+0x%X (its %u cores from part %u: each one's health +0x%X, %u "
           "apart, worn by exe+0x%X; its doom attack's escape +0x%X past %u while +0x%X has 0x%X) hooked in %d "
           "vtable(s)",
           static_cast<unsigned>(aheri_check - image::base()), a.cores, a.parts, a.core_hp, a.core_step,
           static_cast<unsigned>(core_wear - image::base()), a.escape, a.escape_most, a.flags, a.beat, n);
    } else {
      logf("cheats: Uroboros Aheri's hit check is not as known - One hit kills leaves its cores to the game");
    }
  } else {
    logf("cheats: Uroboros Aheri's hit check is not as known - One hit kills leaves its cores to the game");
  }
  // God mode for the jeep too: its damage (slot 66) ends by calling on to the pawns'
  // damage - the one uPawn's slot 66 holds - and answering what it does.
  const mt::Class* jeep = mt::find("uOmJeep");
  const mt::Class* pawn = mt::find("uPawn");
  const uintptr_t jeep_tail = game::sig_one("kSigJeepDamage");
  const uintptr_t jeep_damage = jeep && jeep->vtable ? mem::read<uintptr_t>(jeep->vtable + 4 * kDamageSlot) : 0;
  if (jeep_tail && image::text().contains(jeep_damage) && jeep_damage < jeep_tail && jeep_tail - jeep_damage < 0x100 &&
      pawn && pawn->vtable && game::call_target(jeep_tail + 15) == mem::read<uintptr_t>(pawn->vtable + 4 * kDamageSlot)) {
    g_jeep_damage = reinterpret_cast<PlayerDamageFn>(jeep_damage);
    const int n = hook_vtables(jeep, nullptr, kDamageSlot, jeep_damage, reinterpret_cast<void*>(&hk_jeep_damage),
                               &g_jeep_entries);
    logf("cheats: the jeep's damage exe+0x%X hooked in %d vtables", static_cast<unsigned>(jeep_damage - image::base()),
         n);
  } else {
    logf("cheats: the jeep's damage was not found - God mode leaves the jeep alone");
  }
  // Infinite ammo: the call of the debug switch (a stub that answers 0).
  if (const uintptr_t s = game::sig_one("kSigInfinite")) {
    const uintptr_t stub = game::call_target(s + 19);
    if (stub && mem::matches(stub, mem::parse_pattern("32 C0 C3")) &&
        g_infinite.prepare(s + 19, "E8 ?? ?? ?? ??", {0xB0, 0x01, 0x90, 0x90, 0x90}, "infinite-ammo switch"))
      g_ok[sw::kInfiniteAmmo] = true;
  }
  // ... and no gun a player holds overheats (the jeep's, a gun turret). The weapons'
  // update heats a gun while it fires, but forgets the firing of the AI partner's
  // jeep gun: one whose owner is a player this machine's pad does not drive, and one
  // of the jeep's two guns. Both tests are made to pass for every player and every
  // gun (the driver's jne a jmp, the jeep guns' ja nops): a player's gun never heats
  // up (one that has still cools down), while the enemies' are no player's and heat
  // as before. The heat's index (by the level) is then the AI's, 0.
  if (const uintptr_t s = game::sig_one("kSigOverheat")) {
    const mt::Class* weapon = mt::find("uWeapon");
    const mt::Class* jeep_gun = mt::find("uWpGunEmpJeep");
    const mt::Class* first = mt::by_id(((7u << 16) | (0u - u32at(s + 154))) << 8);  // the first jeep gun's model
    if (weapon && player && jeep_gun && first && mt::derives(first->dti, jeep_gun->dti) &&
        u32at(s + 2) == l.model_owner && u32at(s + 15) == player->dti && u32at(s + 36) == l.control &&
        u8at(s + 42) == 126 - 43 && u8at(s + 162) == 170 - 163 && u32at(s + 165) == u32at(s + 175) &&
        u32at(s + 76) == u32at(s + 120) && u32at(s + 120) == u32at(s + 135) && u32at(s + 175) < weapon->size &&
        g_overheat_driver.prepare(s + 41, "75 ??", {0xEB, static_cast<uint8_t>(u8at(s + 42))},
                                  "overheat: this machine's player?") &&
        g_overheat_jeep.prepare(s + 161, "77 ??", {0x90, 0x90}, "overheat: one of the jeep's guns?"))
      logf("cheats: the weapons' overheating exe+0x%X (a gun's firing +0x%X): with Infinite ammo no player's gun heats",
           static_cast<unsigned>(s - image::base()), u32at(s + 175));
    else
      logf("cheats: the weapons' overheating is not as known - Infinite ammo leaves it alone");
  }
  // Gold: the take function of the lower field (the other is the exchange points').
  const std::vector<uintptr_t> take = game::sig_all("kSigTake");
  if (take.size() == 2 && g_gold_clamp.prepare(take[0] + 14, "33 C0", {0x90, 0x90}, "take-gold clamp") &&
      g_gold_take.prepare(take[0] + 25, "2B C2", {0x90, 0x90}, "take-gold"))
    g_ok[sw::kInfiniteMoney] = true;
  // Steady aim: the sway and the kick in the player's aim - one function: the sway's
  // two stores (the pitch, the yaw beside it) of its two phases (beside each other),
  // then the kick's pitch and its yaw, two calls into the code - and the crosshair.
  const uintptr_t sway = game::sig_one("kSigAimSway"), kick_pitch = game::sig_one("kSigAimKickPitch");
  const uintptr_t kick_yaw = game::sig_one("kSigAimKickYaw"), opening = game::sig_one("kSigCrosshairOpen");
  if (sway && kick_pitch && kick_yaw && opening && kick_pitch > sway && kick_pitch - sway < 0x400 &&
      kick_yaw > kick_pitch && kick_yaw - kick_pitch < 0x100 && u32at(sway + 68) == u32at(sway + 4) + 4 &&
      u32at(sway + 136) == u32at(sway + 60) + 4 && image::text().contains(game::call_target(kick_pitch + 3)) &&
      image::text().contains(game::call_target(kick_yaw + 11)) &&
      game::call_target(kick_pitch + 3) != game::call_target(kick_yaw + 11)) {
    const std::vector<uint8_t> zero = {0x0F, 0x57, 0xC0, 0x90, 0x90, 0x90};  // xorps xmm0,xmm0
    std::vector<uint8_t> fldz_pitch = mem::nops(8), fldz_yaw = mem::nops(16);
    fldz_pitch[0] = fldz_yaw[0] = 0xD9;
    fldz_pitch[1] = fldz_yaw[1] = 0xEE;
    if (g_sway_pitch.prepare(sway + 50, "F3 0F 59 44 24 ??", zero, "aim sway (pitch)") &&
        g_sway_yaw.prepare(sway + 126, "F3 0F 59 44 24 ??", zero, "aim sway (yaw)") &&
        g_kick_pitch.prepare(kick_pitch, "53 8B CF E8 ?? ?? ?? ??", fldz_pitch, "shot kick (pitch)") &&
        g_kick_yaw.prepare(kick_yaw, "D9 44 24 ?? 51 D9 1C 24 53 8B CF E8 ?? ?? ?? ??", fldz_yaw, "shot kick (yaw)") &&
        g_crosshair_open.prepare(opening, "8B 81 ??", {0xC2, 0x0C, 0x00}, "crosshair opening")) {
      g_ok[sw::kSteadyAim] = true;
      logf("cheats: the aim's sway exe+0x%X, its kick exe+0x%X and exe+0x%X, the crosshair's opening exe+0x%X",
           static_cast<unsigned>(sway - image::base()), static_cast<unsigned>(kick_pitch - image::base()),
           static_cast<unsigned>(kick_yaw - image::base()), static_cast<unsigned>(opening - image::base()));
    }
  }
  // ... and through a scope: the character's scope flag (set as a scope goes up,
  // cleared as it comes down), and the model's motion layers - from the set of a
  // motion on a layer, which writes every field the hold reads, agreed by the clock
  // that runs them: a layer's speed is what moves its frame on, its blend-in runs
  // down by the frame time alone.
  const uintptr_t up = game::sig_one("kSigScopeOpen"), down = game::sig_one("kSigScopeClose");
  const uintptr_t set = game::sig_one("kSigMotionSet"), loop = game::sig_one("kSigMotionLoop");
  const uintptr_t clock = game::sig_one("kSigMotionClock");
  const mt::Class* model = mt::find("uModel");
  const mt::Class* layer = mt::find("uModel::Motion");
  if (up && down && set && loop && clock && player && model && layer && mt::derives(player->dti, model->dti)) {
    Stance s;
    s.scope = u32at(up + 24);
    s.layer_size = u32at(set + 43);
    s.layers = u8at(set + 40) * s.layer_size;
    s.in_use = u32at(set + 9);
    s.motion = u8at(set + 63);
    s.speed = u8at(set + 121);
    s.attr = u8at(set + 125);
    s.once = u8at(loop + 7);
    const uint32_t blend = u8at(set + 72);
    if (u32at(down + 29) == s.scope && u32at(down + 16) == u32at(up + 17) && s.scope < player->size &&
        image::text().contains(game::call_target(up + 10)) && s.layer_size == layer->size &&
        u8at(set + 116) == s.motion && u8at(set + 84) == blend && loop > set && loop - set < 0x200 &&
        u8at(loop + 11) == s.attr && s.once && !(s.once & (s.once - 1)) && u32at(clock + 2) == s.in_use &&
        u8at(clock + 55) == u8at(set + 40) && u32at(clock + 58) == s.layer_size && u8at(clock + 67) == s.speed &&
        u8at(clock + 73) == blend && u8at(clock + 102) == blend && u8at(clock + 52) == u8at(loop + 16) &&
        s.motion + 2 <= s.layer_size && s.attr + 2 <= s.layer_size && s.speed + 4 <= s.layer_size &&
        s.in_use + 4 <= s.layers && s.layers + kLayers * s.layer_size <= model->size) {
      g_stance = s;
      g_stance_ok = true;
      logf("cheats: a scope's flag uPlayer +0x%X; the motion layers uModel +0x%X (0x%X each, in use +0x%X: speed "
           "+0x%X, attributes +0x%X, plays once 0x%X) - Steady aim holds the breathing through a scope",
           s.scope, s.layers, s.layer_size, s.in_use, s.speed, s.attr, s.once);
    }
  }
  if (!g_stance_ok) logf("cheats: the scope or the motion layers are not as known - the breathing through a scope stays");
  // Automatic QTEs: the check (its start, its read of the button type, the switch on
  // the kind, the AUTO tail - one function), the managers' call of it, and the two
  // Action Assist tests - both of the profile's same setting, a word in cSystemInfo.
  const uintptr_t check = game::sig_one("kSigQteCheck"), type = game::sig_one("kSigQteType");
  const uintptr_t kind = game::sig_one("kSigQteKind"), tail = game::sig_one("kSigQteAuto");
  const uintptr_t call = game::sig_one("kSigQteCall");
  const uintptr_t is_auto = game::sig_one("kSigAssistAuto"), is_on = game::sig_one("kSigAssistOn");
  if (check && type && kind && tail && call && is_auto && is_on) {
    g_qte.request = u8at(check + 6);
    g_qte.drawn = u8at(check + 15);
    g_qte.answered = u8at(check + 20);
    g_qte.type = u8at(type + 5);
    g_qte.kind = u8at(kind + 2);
    g_qte.category = u8at(tail + 37);
    const uint32_t setting = u8at(is_auto + 13);
    if (type > check && kind > type && tail > kind && tail - check < 0x400 && u8at(type + 2) == g_qte.request &&
        u8at(type + 12) == g_qte.drawn && u8at(tail + 34) == g_qte.request && game::call_target(tail + 6) == is_auto &&
        u32at(tail + 21) == l.control && game::call_target(call + 10) == check && u8at(is_auto + 2) == l.profile_index &&
        u8at(is_on + 2) == l.profile_index && u32at(is_auto + 5) == l.system_size && u32at(is_on + 5) == l.system_size &&
        u8at(is_on + 13) == setting && setting >= l.system_base && setting + 2 <= l.system_base + l.system_size &&
        g_qte_call.prepare(call + 10, "E8 ?? ?? ?? ??", call_to(call + 10, reinterpret_cast<void*>(&hk_qte_check)),
                           "the action buttons' check") &&
        g_assist_auto.prepare(is_auto, "8B 41 ??", {0xB0, 0x01, 0xC3}, "Action Assist: AUTO?") &&
        g_assist_on.prepare(is_on, "8B 41 ??", {0xB0, 0x01, 0xC3}, "Action Assist: not OFF?")) {
      g_qte_check = reinterpret_cast<QteCheckFn>(check);
      g_ok[sw::kAutoQte] = true;
      logf("cheats: the action buttons' check exe+0x%X (a unit's request +0x%X, kind +0x%X; a request's category "
           "+0x%X, button +0x%X), its call exe+0x%X; Action Assist (cSystemInfo +0x%X) tests exe+0x%X, exe+0x%X",
           static_cast<unsigned>(check - image::base()), g_qte.request, g_qte.kind, g_qte.category, g_qte.type,
           static_cast<unsigned>(call + 10 - image::base()), setting - l.system_base,
           static_cast<unsigned>(is_auto - image::base()), static_cast<unsigned>(is_on - image::base()));
      // The boulder's punches: a request's label (the tail reads it beside the category)
      // and its seconds up (AUTO's answer to the boulder's two Pushes, further on in the
      // same tail: their labels, the unit's drawn flag).
      const uintptr_t push = game::sig_one("kSigQteAutoPush");
      const uint32_t label = u8at(tail + 40), seconds = push ? u8at(push + 21) : 0;
      if (push > tail && push - tail < 0x100 && u8at(push + 15) == g_qte.drawn && u32at(push + 2) == kPushTurned &&
          u32at(push + 2) + u8at(push + 10) == kPushHeld && label > g_qte.category && label > g_qte.type &&
          seconds > label) {
        g_qte.label = label;
        g_qte.seconds = seconds;
        g_boulder_ok = true;
        logf("cheats: the volcano's boulder: AUTO answers its Pushes (labels 0x%X, 0x%X) 2 s in, exe+0x%X; the mod "
             "its Hooks (0x%X) and Ram (0x%X), each up %.2f s (a request's label +0x%X, its seconds up +0x%X)",
             kPushTurned, kPushHeld, static_cast<unsigned>(push - image::base()), kHook, kRam,
             static_cast<double>(kBlowWait), g_qte.label, g_qte.seconds);
      } else {
        logf("cheats: AUTO's answer to the boulder's Pushes is not as known - its Hooks and Ram stay the player's");
      }
    }
  }
  install_open_map();
  if (const uintptr_t s = game::sig_one("kSigPlayTime"))
    if (g_playtime.prepare(s, "8B 44 24", kRet8, "add-play-time")) g_ok[sw::kFreezePlaytime] = true;
  const std::vector<uintptr_t> counter = game::sig_all("kSigCounter");
  if (counter.size() == 4 && g_deaths.prepare(counter[0], "8B 44 24", kRet8, "add-deaths"))
    g_ok[sw::kNoDeathCount] = true;
  g_ok[sw::kNoMissCount] = counter.size() == 4;  // a hold on the counters
  // Freeze countdown timer: the Mercenaries' clock, and the stages' countdowns.
  if (const uintptr_t s = game::sig_one("kSigCountdown"))
    if (g_countdown.prepare(s + 39, "F3 0F 11 87 ?? ?? ?? ??", mem::nops(8), "Mercenaries clock"))
      g_ok[sw::kFreezeCountdown] = true;
  install_stage_clocks();
  if (!g_clocks.empty()) g_ok[sw::kFreezeCountdown] = true;
  // The Mercenaries: the match's play state (Endless combo, Combo Bonus always on) and an
  // enemy's hit record (Kill enemies as they appear) - merc.cpp.
  if (merc::discover_match()) g_ok[sw::kEndlessCombo] = g_ok[sw::kComboBonus] = true;
  if (merc::discover_kills() && merc::match_found()) g_ok[sw::kAutoKill] = true;
  for (int i = 0; i < sw::kCount; ++i)
    if (!g_ok[i]) unavailable(static_cast<sw::Kind>(i), "its place in the game was not found (see the log)");
  int ok = 0;
  for (bool b : g_ok) ok += b;
  logf("cheats: %d of %d switches can work", ok, static_cast<int>(sw::kCount));
  return ok > 0;
}

bool enabled(sw::Kind k) { return g_on[k] != 0; }

void set(sw::Kind k, bool on) {
  if ((g_on[k] != 0) == on) return;
  InterlockedExchange(&g_on[k], on ? 1 : 0);
  InterlockedExchange(&g_dirty, 1);
}

bool available(sw::Kind k) { return g_ok[k]; }
const char* unavailable_reason(sw::Kind k) { return g_why[k] ? g_why[k] : ""; }

Status status() {
  if (!g_lock_ready) return Status{};
  EnterCriticalSection(&g_lock);
  const Status s = g_status;
  LeaveCriticalSection(&g_lock);
  return s;
}

void edit(Edit what, int chara, double value) {
  if (!g_lock_ready) return;
  EnterCriticalSection(&g_lock);
  Pending& p = g_edit[static_cast<int>(what)];
  p.set = true;
  p.chara = chara;
  p.value = value;
  LeaveCriticalSection(&g_lock);
}

void tick(const game::Run& run) {
  if (InterlockedExchange(&g_dirty, 0)) {
    bool on[sw::kCount];
    for (int i = 0; i < sw::kCount; ++i) on[i] = g_on[i] != 0;
    config::save_switches(on);
    logf("cheats: switches now:%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", on[sw::kGodMode] ? " god" : "",
         on[sw::kPartnerGod] ? " partner" : "", on[sw::kOneHitKills] ? " one-hit" : "",
         on[sw::kInfiniteAmmo] ? " ammo" : "", on[sw::kSteadyAim] ? " steady-aim" : "",
         on[sw::kAutoQte] ? " qte" : "", on[sw::kOpenMap] ? " map" : "", on[sw::kInfiniteMoney] ? " money" : "",
         on[sw::kFreezePlaytime] ? " playtime" : "", on[sw::kFreezeCountdown] ? " countdown" : "",
         on[sw::kNoDeathCount] ? " deaths" : "", on[sw::kNoMissCount] ? " misses" : "",
         on[sw::kEndlessCombo] ? " endless-combo" : "", on[sw::kComboBonus] ? " combo-bonus" : "",
         on[sw::kAutoKill] ? " kill-all" : "");
  }
  // The patches follow their switches.
  for (mem::Patch* p : {&g_infinite, &g_overheat_driver, &g_overheat_jeep}) follow(*p, g_on[sw::kInfiniteAmmo] != 0);
  for (mem::Patch* p : {&g_sway_pitch, &g_sway_yaw, &g_kick_pitch, &g_kick_yaw, &g_crosshair_open})
    follow(*p, g_on[sw::kSteadyAim] != 0);
  if (g_stance_ok) hold_stance(run);
  for (mem::Patch* p : {&g_qte_call, &g_assist_auto, &g_assist_on}) follow(*p, g_on[sw::kAutoQte] != 0);
  open_maps(run);
  // The Mercenaries (merc.cpp). Kill enemies as they appear lasts one match: switched off as
  // a match begins or ends (written to the ini as the panel's switches are).
  merc::Wanted want;
  want.endless_combo = g_on[sw::kEndlessCombo] && g_ok[sw::kEndlessCombo];
  want.combo_bonus = g_on[sw::kComboBonus] && g_ok[sw::kComboBonus];
  want.kill = g_on[sw::kAutoKill] && g_ok[sw::kAutoKill];
  if (merc::tick(run, want) && g_on[sw::kAutoKill]) {
    set(sw::kAutoKill, false);
    logf("cheats: kill enemies as they appear: switched off - a match began or ended (it lasts one match)");
  }
  follow(g_gold_clamp, g_on[sw::kInfiniteMoney] != 0);
  follow(g_gold_take, g_on[sw::kInfiniteMoney] != 0);
  follow(g_playtime, g_on[sw::kFreezePlaytime] != 0);
  follow(g_deaths, g_on[sw::kNoDeathCount] != 0);
  follow(g_countdown, g_on[sw::kFreezeCountdown] != 0);
  for (StageClock& c : g_clocks) follow(c.frozen, g_on[sw::kFreezeCountdown] != 0);

  // God mode's hold: a protected character's health at its most (what got past
  // the damage hook - the game's own scripted losses are left to it).
  for (int i = 0; i < run.nchars; ++i) {
    const game::Character& c = run.chars[i];
    const bool prot = (c.control == game::Control::kLocal && g_on[sw::kGodMode]) ||
                      (c.control == game::Control::kAi && g_on[sw::kPartnerGod]);
    if (prot && c.hp > 0 && c.hp < c.hp_max) game::set_health(c.obj, c.hp_max);
  }
  // ... and, under God mode, the jeep's (chapter 2-3): a restart there gives it back
  // only as much as it had left.
  const game::Layout& l = game::layout();
  for (int i = 0; i < run.nvehicles; ++i) {
    const uintptr_t v = run.vehicles[i];
    const int hp = mem::read<int16_t>(v + l.hp), most = mem::read<int16_t>(v + l.hp_max);
    if (i == 0 && v != g_jeep_seen) {
      const char* name = mt::class_name_of(v);
      logf("cheats: the jeep (%s) in play, its health %d of %d", name ? name : "?", hp, most);
    }
    if (g_on[sw::kGodMode] && hp > 0 && hp < most) game::set_health(v, most);
  }
  g_jeep_seen = run.nvehicles ? run.vehicles[0] : 0;

  // A stage's countdown running (the panel's), logged as one starts and as it goes.
  Running clock;
  const bool ticking = running_clock(&clock);
  if ((ticking ? clock.stage : 0) != g_clock_seen) {
    const int left = static_cast<int>(clock.left);
    if (ticking)
      logf("cheats: %s's countdown is running, %d:%02d left%s", clock.clock->name, left / 60, left % 60,
           g_on[sw::kFreezeCountdown] ? " (frozen)" : "");
    else
      logf("cheats: the countdown is over");
    g_clock_seen = ticking ? clock.stage : 0;
  }

  // The panel's edits, only while the game is paused.
  Pending pend[kEdits];
  EnterCriticalSection(&g_lock);
  for (int e = 0; e < kEdits; ++e) {
    pend[e] = g_edit[e];
    g_edit[e] = Pending{};
  }
  LeaveCriticalSection(&g_lock);
  for (int e = 0; e < kEdits; ++e) {
    if (pend[e].set && run.screen != game::Screen::kPause) {
      logf("cheats: an edit came while the game was not paused - not made");
      pend[e].set = false;
    }
  }
  // The countdown's: the seconds left of the one running, kept above what it runs down
  // to (or the game would take it for run out) and within what the HUD shows.
  if (Pending& p = pend[static_cast<int>(Edit::kCountdown)]; p.set) {
    if (ticking) {
      const float lo = clock.floor + 1.0f, want = static_cast<float>(p.value);
      const float t = want < lo ? lo : want > g_clock_most ? g_clock_most : want;
      mem::write<float>(clock.stage + clock.clock->time, t);
      clock.left = t;
      const int left = static_cast<int>(t);
      logf("cheats: %s's countdown set to %d:%02d", clock.clock->name, left / 60, left % 60);
    } else {
      logf("cheats: the countdown's edit came with no countdown running - not made");
    }
    p.set = false;
  }

  // The profile's numbers and the chapter's counters.
  const uintptr_t sys = game::system_info(), gi = game::game_info();
  Status s;
  s.profile = sys && gi;
  s.countdown = ticking;
  s.countdown_left = ticking ? clock.left : 0.0f;
  s.countdown_most = g_clock_most;
  if (s.profile) {
    // Misses don't count: a character's shots follow its hits.
    if (g_on[sw::kNoMissCount] && g_ok[sw::kNoMissCount])
      for (int c = 0; c < 2; ++c) {
        const uintptr_t base = gi + l.counters + c * l.counter_stride;
        const auto shots = mem::read<uint32_t>(base + 8), hits = mem::read<uint32_t>(base + 12);
        if (shots != hits) mem::write<uint32_t>(base + 8, hits);
      }
    for (int e = 0; e < kEdits; ++e) {
      if (!pend[e].set) continue;
      const double v = pend[e].value;
      const int c = pend[e].chara < 0 ? 0 : pend[e].chara > 1 ? 1 : pend[e].chara;
      const uintptr_t base = gi + l.counters + c * l.counter_stride;
      const auto n = static_cast<uint32_t>(v < 0 ? 0 : v > 9999999 ? 9999999 : v);
      switch (static_cast<Edit>(e)) {
        case Edit::kGold:  // and the checkpoint's, or a retry or the chapter's end takes the gold back
          mem::write<uint32_t>(sys + l.gold, n);
          mem::write<uint32_t>(sys + l.gold + l.checkpoint, n);
          break;
        case Edit::kDeaths: mem::write<uint32_t>(base + 0, n); break;
        case Edit::kKills: mem::write<uint32_t>(base + 4, n); break;
        case Edit::kShots: mem::write<uint32_t>(base + 8, n); break;
        case Edit::kHits: mem::write<uint32_t>(base + 12, n); break;
        case Edit::kTime:  // one clock: the game adds the same time to every character's
          for (int k = 0; k < game::kMaxChars; ++k)
            mem::write<float>(gi + l.counters + k * l.counter_stride + 16, static_cast<float>(v < 0 ? 0 : v));
          break;
        case Edit::kCountdown:  // made above
        case Edit::kCount: break;
      }
      logf("cheats: edit %d (character %d) set to %.0f", e, c, v);
    }
    s.gold = static_cast<int>(mem::read<uint32_t>(sys + l.gold));
    s.clock = mem::read<float>(gi + l.counters + 16);  // the first character's copy: they run together
    for (int c = 0; c < 2; ++c) {
      const uintptr_t base = gi + l.counters + c * l.counter_stride;
      s.chars[c].deaths = static_cast<int>(mem::read<uint32_t>(base + 0));
      s.chars[c].kills = static_cast<int>(mem::read<uint32_t>(base + 4));
      s.chars[c].shots = static_cast<int>(mem::read<uint32_t>(base + 8));
      s.chars[c].hits = static_cast<int>(mem::read<uint32_t>(base + 12));
    }
  } else {
    for (int e = 0; e < kEdits; ++e)
      if (pend[e].set) logf("cheats: an edit came while the profile was not known - not made");
  }
  EnterCriticalSection(&g_lock);
  g_status = s;
  LeaveCriticalSection(&g_lock);
}

void remove_hooks() {
  for (uintptr_t e : g_player_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_player_damage))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_player_damage));
  for (uintptr_t e : g_enemy_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_enemy_subtract))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_enemy_subtract));
  for (uintptr_t e : g_jeep_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_jeep_damage))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_jeep_damage));
  for (uintptr_t e : g_ndesu_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_ndesu_hit))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_ndesu_hit));
  for (uintptr_t e : g_plaga_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_plaga_hit))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_plaga_hit));
  for (uintptr_t e : g_irving_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_irving_hit))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_irving_hit));
  for (uintptr_t e : g_tentacle_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_tentacle_hit))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_tentacle_hit));
  for (uintptr_t e : g_reaper_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_reaper_hit))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_reaper_hit));
  for (uintptr_t e : g_aheri_entries)
    if (mem::read<uintptr_t>(e) == reinterpret_cast<uintptr_t>(&hk_aheri_hit))
      mem::write<uintptr_t>(e, reinterpret_cast<uintptr_t>(g_aheri_hit));
  for (mem::Patch* p : {&g_infinite, &g_overheat_driver, &g_overheat_jeep, &g_gold_clamp, &g_gold_take, &g_playtime, &g_deaths, &g_countdown, &g_sway_pitch,
                        &g_sway_yaw, &g_kick_pitch, &g_kick_yaw, &g_crosshair_open, &g_qte_call, &g_assist_auto,
                        &g_assist_on})
    p->revert();
  for (StageClock& c : g_clocks) c.frozen.revert();
  merc::remove_patches();
}

}  // namespace re5cc::cheats
