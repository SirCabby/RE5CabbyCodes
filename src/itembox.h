#pragma once

#include <cstdint>
#include <vector>

#include "game.h"

// The item box - the story's storage, cSystemInfo's 54 entries - for the panel at
// the pause menu and for its window beside Item Management: what is in it, a count
// changed, an entry thrown away, items added, and at the pause the cases' Store
// and the box's Take. The tick publishes the view and makes the changes:
// - beside Item Management only while the screen's panes are idle (its menu, or
//   browsing the case or the box - nothing picked up): the live box is written,
//   then the screen rebuilds each pane's box list through its own function (the
//   call the user approved on 2026-10-29); the screen keeps the box as it closes;
// - at the pause in a chapter: the live box and its checkpoint copy alike, so a
//   retry or the chapter's end keeps the change. Store and Take go through the
//   inventory's own clear and add, and the character's saved case (the one play
//   restarts from) is brought to the slot's new state too: a move between the case
//   and the box is never undone on one side only. A Take of what the area has not
//   loaded has the game's loader load it first (inventory::load): its batch waits
//   for it, and the ones after wait their turn.
// The treasures beside it: the game keeps them in a list of their own (the
// profile's, which Item Management lists and the shop buys from; in play each
// character's, and each one's saved copy that play restarts from). Beside Item
// Management the profile's list is changed and the screen rebuilds its treasures
// list through its own function (the call the user approved on 2026-10-30); at the
// pause the change is made in every copy alike.
// The extra episodes (Lost in Nightmares, Desperate Escape) are played with no box
// of the game's - each starts from a loadout of its own - so each has a box of the
// mod's own, the panel's at the pause in that episode (the user, 2026-11-03: "These
// extra content portions should all have their own item box"): as many entries as
// the game's, each kept as the game keeps one, under the same rules, in a file
// beside the mod (file_name) - read as the mod starts, written at every change. A
// Store is written there before the item leaves the case. Its moves at the pause go
// into the cases the episode restarts from (its saved state, the play flow's copy of
// it) and never into the story's (the working block's checkpoint copy, which the
// episodes never commit). No treasures: the episodes have no shop.
namespace re5cc::itembox {

constexpr int kMaxEntries = 64;    // room for the box as found (54 entries)
constexpr int kLevels = 8;         // the level bytes an entry keeps
constexpr int kMaxTreasures = 64;  // room for the treasures' list as found (54 entries)

struct Entry {
  int id = 0;     // 0: empty
  int count = 0;  // a gun's: the rounds loaded
  uint8_t lv[kLevels] = {};
  uint8_t avail = 0;  // at the pause: whether it can be taken into a case (inventory::Avail)
};

// A treasure held: how many of it (all its entries' counts together).
struct Treasure {
  int id = 0;
  int count = 0;
};

struct View {
  bool found = false;     // discovery understood the box and Item Management
  bool open = false;      // Item Management is up (its window)
  bool in_play = false;   // a story chapter or an extra episode is being played: its box (the panel's)
  int mode = game::kModeStory;    // whose box: the story's (the game's own) or an extra episode's (the mod's)
  const char* episode = nullptr;  // an extra episode's box: the episode's name (nullptr: the story's)
  const char* file = nullptr;     // ... the file beside the mod it is kept in
  bool unreadable = false;        // ... that file is there but could not be read: left alone, no box this run
  bool unwritten = false;         // ... a change is not in that file yet (it could not be written; tried again)
  bool editable = false;  // changes can be made now
  bool can_move = false;  // Store and Take can be made now (the pause in a chapter or an episode)
  bool loading = false;   // a batch waits for the game's loader (its Takes' items); the next ones after it
  int size = 0;           // the box's entries (54)
  int used = 0;
  Entry entry[kMaxEntries];
  bool treasures = false;  // the treasures' list was understood (and is listed here)
  int tmost = 0;           // the most of a treasure an entry of the list holds (999)
  int tkinds = 0;          // the kinds of treasure held
  Treasure treasure[kMaxTreasures];  // in the list's order
  char note[200] = "";  // what the last changes did, or why they were not made
};

enum class Op {
  kSet,          // an entry's new count (`count`; 0 throws it away)
  kAdd,          // `count` of item `id` into the box
  kStore,        // the item in a character's case slot into the box
  kTake,         // an entry into a character's case
  kTreasureSet,  // how many of treasure `id` are held (`count`; 0 throws them away)
  kTreasureAdd,  // `count` more of treasure `id`
};
struct Change {
  Op op = Op::kSet;
  int index = -1;  // kSet, kTake: the entry - with the id it showed, so a box changed since is left alone
  int id = 0;      // the entry's, the item's or the treasure's id
  int count = 0;
  int chara = -1;  // kStore, kTake: the character (sPlayer's index)
  int slot = -1;   // kStore: the slot the item is in
};

void init();                      // from DllMain: the lock
bool discover();                  // from boot, after game::discover() and inventory::discover()
void tick(const game::Run& run);  // main thread
View view();                      // any thread

// A batch (any thread): made together at the next tick, if it can be made then.
void apply(const std::vector<Change>& batch);

// Item Management's panes idle (main thread): each one in use at its menu, or
// browsing the case or the box - nothing picked up, no question or shop list open.
bool screen_idle(uintptr_t screen);

// The box's own rules (any thread, once discover() has run): what one entry of an
// item holds (the most a stack takes; 1 for what does not stack; 0 for what the
// box never takes), and whether a chapter's start keeps it in the box.
int most_of(int id);
bool kept(int id);

// An extra episode's box (game::kModeNightmares, kModeEscape): its file's name beside
// the mod ("RE5CabbyCodes.itembox.lost-in-nightmares.ini"), or nullptr for another mode.
const char* file_name(int mode);

}  // namespace re5cc::itembox
