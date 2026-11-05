#pragma once

#include "game.h"
#include "switches.h"

// The cheat switches (switches.h), what each does to the game, and the editors
// beside them. The panel (render thread) only flips a switch or posts an edit;
// the tick (main thread) applies it - an edit only while the game is paused.
namespace re5cc::cheats {

void init();     // from DllMain: the switches as the ini has them
bool install();  // from boot::unpacked(), after discovery: the hooks and patches prepared
void tick(const game::Run& run);

bool enabled(sw::Kind k);
void set(sw::Kind k, bool on);  // any thread: takes effect at the next tick, and is written to the ini

// Whether a switch can work in this game (its hooks were found), and why not.
bool available(sw::Kind k);
const char* unavailable_reason(sw::Kind k);

// What the panel shows, published by the tick.
struct Counters {
  int deaths = 0, kills = 0, shots = 0, hits = 0;
};
struct Status {
  bool profile = false;  // the profile and its game info are known
  int gold = 0;
  float clock = 0;    // the chapter's play time, seconds: one clock, which the game adds to every character's alike
  Counters chars[2];  // the chapter's numbers: Chris's and Sheva's
  // A stage's countdown on screen (chapter 3-2's escape, Desperate Escape's last wait):
  bool countdown = false;
  float countdown_left = 0;  // its seconds left
  float countdown_most = 0;  // the most it can be set to: what the HUD can show
};
Status status();

// Edits (any thread); the tick applies them while the game is paused. kTime sets
// the chapter's clock (every character's copy; `chara` is not used); kCountdown the
// seconds left of the stage's countdown running now (nor `chara`).
enum class Edit { kGold, kDeaths, kKills, kShots, kHits, kTime, kCountdown, kCount };
void edit(Edit what, int chara, double value);

void remove_hooks();  // unload

}  // namespace re5cc::cheats
