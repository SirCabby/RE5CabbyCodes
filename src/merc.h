#pragma once

#include "game.h"
#include "mt.h"

// The Mercenaries (The Mercenaries and Reunion: the play state's modes 2 and 5): the
// match's numbers the panel shows, and what Endless combo, Combo Bonus always on and Kill
// enemies as they appear do to it. Everything the game keeps for a match is in the play
// state (cGameSys, in sBioMain): its step (the count-in, play, time up, the end), the clock
// the HUD shows, the stage's kills and their target, and a block for each score slot - the
// kills, the combo and its clock, the best combo, the Combo Bonus and its clock, what the
// combo has earned and the score. Both modes keep one slot for the whole team (slot 0).
// Found by the code that runs them (sigs.h); never by address.
namespace re5cc::merc {

// From cheats::install(), after game::discover(): the play state's fields and the hit
// record a shot leaves on an enemy, read out of the game's code and checked against each
// other. Each part says in the log whether it was understood.
bool discover_match();  // the match's fields (Endless combo, Combo Bonus always on, the panel)
bool discover_kills();  // the hit record and the stage's areas (Kill enemies as they appear)

// An enemy class whose own hit check takes health only on a weak spot, and the first part
// there (the Reaper's, read by cheats.cpp): Kill enemies as they appear hits it there.
void set_weak_spot(const mt::Class* cls, int part);

// What the tick does, by switch (the patches go in and out with them).
struct Wanted {
  bool endless_combo = false;
  bool combo_bonus = false;
  bool kill = false;
};

// The tick (main thread): the switches applied; returns true when Kill enemies as they
// appear is to be switched off (a match has ended or begun: it lasts one match).
bool tick(const game::Run& run, const Wanted& want);

// The match's numbers (for the panel): published by the tick.
struct Status {
  bool match = false;    // a match of the Mercenaries is in play (its HUD up, mode 2 or 5)
  bool offline = false;  // not an online game (the cheats act only offline)
  int step = 0;          // the play state's step: 0 the count-in, 1 playing, 2 time up, 3 the end
  float time_left = 0;   // the clock, seconds
  int score = 0;         // the team's score
  int waiting = 0;       // what the combo running has earned, not on the score yet
  int combo = 0, best_combo = 0;
  int kills = 0, target = 0;  // the stage's kills, and how many clear it
  bool bonus = false;         // the Combo Bonus running ...
  float bonus_left = 0;       // ... its seconds left
  int bonus_kills = 0;        // kills in it
  int killing = 0;            // Kill enemies as they appear: enemies it is hitting now
  int killed = 0;             // ... hits it has written this match
  int areas = 0;              // ... the stage's areas it has opened to wherever you stand
  int melee_seconds = 0;      // ... the time each of its kills earns (hand-to-hand), 0: they are shots
};
Status status();

bool match_found();  // discover_match() succeeded
bool kills_found();  // discover_kills() succeeded

void remove_patches();  // unload

}  // namespace re5cc::merc
