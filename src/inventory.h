#pragma once

#include <string>
#include <vector>

#include "game.h"
#include "items.h"

// The characters' cases (each uPlayer's cItemManager: the 3 x 3 grid, slots
// 0-8), for the panel: what is in each slot, a count changed, an item added or
// thrown away. The tick publishes the view and makes the changes - only while the
// game is paused. A count is a field write (a gun's: the rounds loaded), within
// what the item holds; adding and throwing away are the inventory's own
// functions (cItemManager's add - which makes a weapon's model in the
// character's hand as a pickup does - and cItem's clear, which lets the model go).
namespace re5cc::inventory {

struct Slot {
  bool used = false;
  int id = 0, num = 0, most = 0, equip = 0;
  uint8_t lv[9] = {};
  int kept_as = 0;  // the id the item box (and the save) keep it as, when not its own: a grenade launcher's load
};
struct Case {
  const char* name = "";  // "Chris", "Sheva", ...
  game::Control control = game::Control::kLocal;
  int index = 0;           // in sPlayer's array
  Slot slot[game::kSlots];
};
// Whether an item can go into a case in play: its model's archive is loaded here
// (kHere), the game's own loader can load it first (kLoadable), or neither - the
// game has no model for it, or its loader was not understood (kNever).
enum Avail : uint8_t { kNever, kLoadable, kHere };

struct View {
  int ncases = 0;
  Case cases[game::kMaxChars];
  bool paused = false;
  bool can_add = false;
  bool area_known = false;  // the game's loader was understood (else only what is loaded goes in a case in play)
  bool loading = false;     // an Add waits for the game's loader
  uint8_t avail[items::kCount] = {};  // at the pause: each item of items.h (Avail)
  char note[160] = "";  // the last change that could not be made
};

void init();                      // from DllMain: the lock
bool discover();                  // from boot, after game::discover(): the inventory's own functions
void tick(const game::Run& run);  // main thread
View view();                      // any thread

// Requests (any thread): made at the next tick while paused. An item is added
// into the first free slot the game picks for it (ammo and healing in the even
// ones, as a pickup); a weapon comes loaded. The item in a character's hand is
// not thrown away (the game itself never takes it from the hand that way).
void set_count(int chara_index, int slot, int count);
void add(int chara_index, int id, int count);
void throw_away(int chara_index, int slot);
bool can_change();  // the inventory's own functions were found

// For the item box's Store and Take (main thread, the tick, the game paused): the
// inventory's own add of an item with its count and nine levels into a
// character's case - the new item, or 0 for no room (or not in_area) - and its
// own clear.
uintptr_t add_item(uintptr_t chara, int id, int count, const uint8_t lv[9]);
bool clear_item(uintptr_t item);

// Whether an item can be put in a case now, in play (main thread, after this
// tick's inventory::tick): the add makes the item a model of its own, from its
// model class's archive, and the game loads an archive only for what an area
// needs - a model made without it sends the game to a file that is not there, and
// it quits. in_area: true only while the game's loader lists that archive as
// loaded, as last read - refresh_area() reads it again (before the adds).
bool in_area(int id);
void refresh_area();
Avail avail(int id);
// The game's own loader asked for the items' archives (its request of each model's
// class - and of what the class needs with it - then one commit of the list): true
// when every one is asked for or loaded already, false when one cannot be (no
// model, or the game choosing an area's archives). ready(): what the item needs to
// go in a case is loaded.
bool load(int id);
bool load(const std::vector<int>& ids);
bool ready(int id);
constexpr int kLoadTicks = 20 * 60;  // how long a change waits for the loader (about 20 seconds)

// What may be put in a case at this pause, loaded before it is asked for (main
// thread): each item once a pause, while the items' and weapons' archives loaded
// stay below a cap - the rest load when they are taken. preload_soon (any thread):
// one item, at the next tick (an Add list's pick).
void preload(std::vector<int> ids);
void preload_soon(int id);

// Whether the game can keep items.h's item k in a case: it has a model class. The
// others are leftovers of its item table (no weapon or recipe uses them; its own
// chapter start could not put one in a case) and are not offered. Any thread, once
// discovery has run; true for all when what an area loads is not understood.
bool carried(int k);

// The item whose model an item is given (any thread, once discovered): its own id, or
// the one the game's switch gives it - the grenade launcher's rounds the launcher's,
// whose id the case's item takes while the save keeps the rounds'.
int model_id(int id);

// The id the game's own save of a case writes for a case's item, and so the one the item
// box keeps it as (main thread): a grenade launcher as its load - in play the case's item
// has the launcher's id once it has its model, which keeps the rounds loaded - else its
// own. A launcher whose load cannot be read keeps the launcher's id, as the save keeps it.
int as_saved(const game::Item& it);

// The loader's lists for archives other than the items' (the menus', menus.cpp):
// whether they were understood, the folder its archives' paths begin with
// ("Image\Archive\"), and - main thread - whether a list holds the archive of this
// path, as it was asked for, loaded now.
bool lists_known();
const std::string& archive_dir();
bool archive_loaded(const char* path);

}  // namespace re5cc::inventory
