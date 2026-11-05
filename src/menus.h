#pragma once

#include "game.h"

// Chapter Select from Item Management, from the pause, from the Game Over screen and from
// the chapter's results. Between chapters - after a chapter's results, or after CONTINUE,
// CHAPTER SELECT or NEW GAME in the start-up menu - the play flow (aGame) shows Item
// Management before the chapter loads, and the screen has no way back: it only closes into
// the chapter. The panel's "Chapter Select" leaves it the way the game's own end of a
// co-op session there does - the screen saves the cases and closes with its quit, and the
// play flow takes the profile and the cases back from their checkpoint copies and ends the
// game, back to the start-up menu - then has that menu open its CHAPTER SELECT (its own
// first step, then the chapter list) where it would have opened its top menu. At the pause
// in a story chapter it leaves the chapter the way the pause menu's QUIT does when its
// save question ("Overwrite current equipment and status?") is answered yes: the pause
// menu's own steps from there (the equipment, the item box and the treasures saved, the
// play flow told to quit), then the same way to the chapter list; at the Game Over screen
// as its own QUIT does (the game back to the last checkpoint, then saved), and at the
// chapter's results as their own quit does (the chapter's end already saved) - after the
// last chapter too, where their continue is that quit (back to the menus). The menus'
// archive (the chapter list's layout among its files), which the story menu has loaded as
// a choice is made there, is asked for with the title's in the menus' own load, and the
// menu is handed over only with it loaded. At Item Management what was changed on the
// screen is kept: the screen's own save (the call the user approved, 2026-10-31), then the
// checkpoint copies brought to the profile's data (the box, the gold, the treasures) and
// to the cases, as the game's commit after its own Item Management does, so the quit takes
// back what the screen leaves. Otherwise field writes, all from the tick.
namespace re5cc::menus {

// Where Chapter Select is offered (or was asked, on its way).
enum class Where : uint8_t { kNone, kItemBox, kPause, kGameOver, kResults };

struct View {
  bool found = false;     // discovery understood the way back
  bool offered = false;   // Item Management between chapters; in a story chapter the pause, Game Over, the results
  Where where = Where::kNone;
  bool ready = false;     // ... and can be left now: the screen idle, the game offline
  bool last = false;      // at the results: the last chapter's - the game goes back to its menus from them
  bool going = false;     // on its way to Chapter Select
  char why[160] = "";     // offered but not ready: why it cannot go now
  char note[160] = "";    // what happened last: a refusal, or the way back given up
};

void init();                      // from DllMain: the lock
bool discover();                  // from boot, after game::discover()
void tick(const game::Run& run);  // main thread
View view();                      // any thread

// The panel's button (any thread; at the pause once confirmed): Chapter Select at the next
// tick, if it can go then.
void chapter_select();

// On its way (main thread): asked and not yet handed over or given up - nothing is
// changed in the cases or the box meanwhile.
bool leaving();

// Where the game keeps the cases and the treasures it makes the characters' again from,
// in a cGameInfo (main thread, for the item box's moves at the pause): the save data's
// saved state (play makes the characters from it as a chapter, a checkpoint or a death's
// CONTINUE starts), its working block (Item Management's cases; QUIT's "No" takes the
// saved state back to it) and that one's checkpoint copy (QUIT's "No" takes the working
// block back from it), three blocks of one size; and the play flow's copy of the profile's
// cGameInfo as at the last checkpoint (RESTART and a death's CONTINUE take the saved state
// and the working block back from it), 0 when there is none. false: not understood.
struct Saves {
  uint32_t state = 0, work = 0, copy = 0, size = 0;
  uintptr_t snapshot = 0;
};
bool saves(Saves* out);

}  // namespace re5cc::menus
