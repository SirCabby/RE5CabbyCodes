#include "itembox.h"

#include <windows.h>
#include <io.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "config.h"
#include "files.h"
#include "image.h"
#include "inventory.h"
#include "items.h"
#include "log.h"
#include "mem.h"
#include "menus.h"
#include "mt.h"

namespace re5cc::itembox {
namespace {

CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
View g_view;
std::vector<std::vector<Change>> g_batches;
// A batch waiting for the game's loader: its Takes' items, how long it has waited
// (main thread).
std::vector<Change> g_pending;
std::vector<int> g_pending_ids;
int g_pending_age = 0;
int g_pending_mode = 0;  // the game's mode it was made in: whose box
int g_logged_mode = -2;  // whose box the log last named (-1: none in that game)

// The box in cSystemInfo (kSigBoxFree, kSigBoxRules): where, an entry's size, how many.
uint32_t g_box = 0, g_entry = 0;
int g_count = 0;

// The store's rules (kSigBoxRules): what it never takes, what stacks (up to g_most
// in an entry - the rest go one to an entry) - and what a chapter's start keeps
// (kSigBoxKeep: below g_drop, or one of the two after it).
uint32_t g_never = 0, g_a_lo = 0, g_a_hi = 0, g_b = 0, g_c = 0, g_d_lo = 0, g_d_hi = 0, g_e_lo = 0, g_e_hi = 0;
int g_most = 0;
uint32_t g_drop = 0, g_keep1 = 0, g_keep2 = 0;

// The levels an entry keeps (kSigBoxLevels): byte k of an entry's levels is the
// cItem's level g_lvmap[k] (seven of its nine; the eighth byte stays 0).
constexpr int kKept = 7;
int g_lvmap[kKept] = {};

// The saved cases in cGameInfo (kSigSavedCases): play makes the characters from
// them as it starts or restarts - a character's list of items (a cItem's data
// from its id on, one an entry, the entry its slot), and their counts.
uint32_t g_saved = 0, g_saved_stride = 0, g_saved_item = 0, g_saved_counts = 0;

// Item Management's screen (kSigBoxPanes, kSigBoxPaneUpdate, the panes' state
// switch): its two panes - one a player - each in use while a field of it is not
// negative; a pane's state is its first byte, its step within the state the byte
// at g_sub. Idle: a state that browses (the case, the box; g_browse) at step 1, or
// the opening state at its menu's step.
uint32_t g_pane = 0, g_pane_step = 0, g_in_use = 0, g_sub = 0, g_browse = 0;
uint32_t g_menu_state = 0, g_menu_step = 0;

// The screen's rebuild of a pane's box list from the live box (thiscall, the
// screen; the pane) - what the screen runs after each store and take of its own.
using RebuildFn = uint32_t(__thiscall*)(void* screen, void* pane);
RebuildFn g_rebuild = nullptr;
bool g_found = false;

// The treasures' lists: entries of a 16-bit id and count, an entry left at 0 when it
// was the treasure's only one (the mark of one found, which the game's treasure
// achievement goes by) - cSystemInfo's, right after the box (kSigTreasureCount, the
// checkpoint copy again its offset on); in play each character's in sPlayer
// (kSigTreasureAdd: a pickup goes into every character's), and each one's saved in
// cGameInfo (kSigTreasureLoad: play makes the characters' lists from them).
uint32_t g_tlist = 0, g_tplay = 0, g_tsaved = 0, g_tstride = 0;
int g_tcount = 0, g_tmost = 0;
// Item Management's rebuild of a pane's treasures list from cSystemInfo's (thiscall,
// the screen; the pane) - what the screen runs after a sale of its own.
RebuildFn g_trebuild = nullptr;
bool g_tfound = false;

uint32_t u8at(uintptr_t a) { return mem::read<uint8_t>(a); }
uint32_t u16at(uintptr_t a) { return mem::read<uint16_t>(a); }
uint32_t u32at(uintptr_t a) { return mem::read<uint32_t>(a); }
int s8at(uintptr_t a) { return mem::read<int8_t>(a); }

bool off(const char* why) {
  logf("itembox: %s - no item box", why);
  return false;
}

bool no_treasures(const char* why) {
  logf("itembox: %s - the box without its treasures", why);
  return false;
}

void note(View& v, const char* text) { std::snprintf(v.note, sizeof(v.note), "%s", text); }

// --- the screen ------------------------------------------------------------------------------
uintptr_t pane_at(uintptr_t screen, int k) { return screen + g_pane + static_cast<uint32_t>(k) * g_pane_step; }
bool in_use(uintptr_t pane) { return mem::read<int32_t>(pane + g_in_use) >= 0; }
bool browses(uint32_t state) { return state < 32 && (g_browse & (1u << state)); }

// Every pane in use idle: its menu, or browsing the case or the box, settled -
// nothing picked up, no question or shop list open.
bool idle(uintptr_t screen) {
  int used = 0;
  for (int k = 0; k < 2; ++k) {
    const uintptr_t pane = pane_at(screen, k);
    if (!in_use(pane)) continue;
    ++used;
    const uint32_t state = u8at(pane), step = u8at(pane + g_sub);
    if (!(browses(state) && step == 1) && !(state == g_menu_state && step == g_menu_step)) return false;
  }
  return used > 0;
}

// Each pane in use gets its lists rebuilt - the box's, the treasures', as changed;
// one browsing steps back into its state, as the screen's own changes leave it (step
// 0: the state's entry runs again and shows the item under the cursor afresh).
void rebuild(uintptr_t screen, bool box, bool treasures) {
  for (int k = 0; k < 2; ++k) {
    const uintptr_t pane = pane_at(screen, k);
    if (!in_use(pane)) continue;
    if (box) g_rebuild(reinterpret_cast<void*>(screen), reinterpret_cast<void*>(pane));
    if (treasures && g_trebuild) g_trebuild(reinterpret_cast<void*>(screen), reinterpret_cast<void*>(pane));
    if (browses(u8at(pane))) mem::write<uint8_t>(pane + g_sub, 0);
  }
}

// --- the extra episodes' boxes -----------------------------------------------------------------
// The game keeps no box in an extra episode (no Item Management; each starts from a loadout of
// its own), so each has one of the mod's own: as many entries as the game's box, each kept as the
// game keeps one (an id, a count, the levels), under the box's own rules - the panel's box at the
// pause in that episode. It lives in a file beside the mod, read as the mod starts and written
// whole at every change; a file there that cannot be read is left as it is, and its box unused.
constexpr uint32_t kMaxEntryBytes = 32;  // the most an entry may be (discover)
struct FileBox {
  int mode;          // the game's mode it is for
  const char* name;  // the episode's
  const char* file;  // beside the mod
  bool usable = false;  // read, or no file yet: the box can change
  int retry = 0;        // ticks to wait before writing again, after a write that failed
  uint8_t data[kMaxEntries * kMaxEntryBytes] = {};     // the box, in the game's box's layout
  uint8_t written[kMaxEntries * kMaxEntryBytes] = {};  // ... as its file has it
};
FileBox g_files[] = {
    {game::kModeNightmares, "Lost in Nightmares", "RE5CabbyCodes.itembox.lost-in-nightmares.ini"},
    {game::kModeEscape, "Desperate Escape", "RE5CabbyCodes.itembox.desperate-escape.ini"},
};

FileBox* file_box(int mode) {
  for (FileBox& f : g_files)
    if (f.mode == mode) return &f;
  return nullptr;
}
bool unwritten(const FileBox& f) { return std::memcmp(f.data, f.written, sizeof(f.data)) != 0; }

// --- the box -----------------------------------------------------------------------------------
// The live box, and its checkpoint copy when that is written too (at the pause) - or an extra
// episode's box, the mod's own (`file`; no copy).
struct Box {
  uintptr_t live = 0, copy = 0;
  FileBox* file = nullptr;
  uintptr_t at(int i) const { return live + static_cast<uint32_t>(i) * g_entry; }
  int id(int i) const { return static_cast<int>(u16at(at(i))); }
  int count(int i) const { return static_cast<int>(u16at(at(i) + 2)); }
  void put16(int i, uint32_t off, int v) const {
    mem::write<uint16_t>(at(i) + off, static_cast<uint16_t>(v));
    if (copy) mem::write<uint16_t>(copy + static_cast<uint32_t>(i) * g_entry + off, static_cast<uint16_t>(v));
  }
  void put8(int i, uint32_t off, uint8_t v) const {
    mem::write<uint8_t>(at(i) + off, v);
    if (copy) mem::write<uint8_t>(copy + static_cast<uint32_t>(i) * g_entry + off, v);
  }
  void clear(int i) const {
    for (uint32_t b = 0; b < g_entry; ++b) put8(i, b, 0);
  }
  int empty() const {
    for (int i = 0; i < g_count; ++i)
      if (!id(i)) return i;
    return -1;
  }
};

// How many of a stacking item the box still takes: room in its entries, and empties.
int room_for_stack(const Box& box, int id) {
  int room = 0;
  for (int i = 0; i < g_count; ++i) {
    if (!box.id(i)) room += g_most;
    else if (box.id(i) == id && box.count(i) < g_most) room += g_most - box.count(i);
  }
  return room;
}

// `count` of a stacking item, as the box's own store places it: the entries it has
// topped up first, then empty ones. Returns how many went in.
int place_stack(const Box& box, int id, int count) {
  int left = count;
  for (int i = 0; i < g_count && left > 0; ++i) {
    if (box.id(i) != id || box.count(i) >= g_most) continue;
    const int put = left < g_most - box.count(i) ? left : g_most - box.count(i);
    box.put16(i, 2, box.count(i) + put);
    left -= put;
  }
  for (int i = 0; i < g_count && left > 0; ++i) {
    if (box.id(i)) continue;
    const int n = left < g_most ? left : g_most;
    box.clear(i);
    box.put16(i, 0, id);
    box.put16(i, 2, n);
    left -= n;
  }
  return count - left;
}

// One entry of an item that does not stack: its count (a gun's rounds) and levels.
bool place_one(const Box& box, int id, int count, const uint8_t lv[kLevels]) {
  const int i = box.empty();
  if (i < 0) return false;
  box.clear(i);
  box.put16(i, 0, id);
  box.put16(i, 2, count);
  for (int k = 0; k < kLevels && 4 + static_cast<uint32_t>(k) < g_entry; ++k) box.put8(i, 4 + k, lv[k]);
  return true;
}

const char* name_of(int id) {
  const items::Info* i = items::find(static_cast<uint32_t>(id));
  return i ? i->name : "?";
}

// --- the treasures -----------------------------------------------------------------------------
// The lists a treasure's change is made in, the one shown first (the counts a change
// goes by): beside Item Management cSystemInfo's, which the screen keeps as it
// closes; at the pause each character's in play - this machine's first - then each
// one's saved, in every saved copy of the save data play makes the characters' again
// from (menus::saves), then cSystemInfo's and its checkpoint copy, so neither a
// restart, a death's CONTINUE, QUIT nor the chapter's end undoes it.
struct Lists {
  uintptr_t at[6 * game::kMaxChars + 2] = {};
  int n = 0;
  void add(uintptr_t a) {
    if (a && n < static_cast<int>(sizeof(at) / sizeof(at[0]))) at[n++] = a;
  }
};

Lists treasure_lists(const game::Run& run, uintptr_t sys, bool open) {
  Lists t;
  if (!g_tfound || !sys) return t;
  if (open) {
    t.add(sys + g_tlist);
    return t;
  }
  const uintptr_t sp = game::system("sPlayer"), gi = game::game_info();
  if (!sp || !gi || run.nchars < 1) return t;
  int first = 0;
  for (int i = run.nchars - 1; i >= 0; --i)
    if (run.chars[i].control == game::Control::kLocal) first = i;
  auto step = [&](int i) { return static_cast<uint32_t>(run.chars[i].index) * g_tstride; };
  t.add(sp + g_tplay + step(first));
  for (int i = 0; i < run.nchars; ++i)
    if (i != first) t.add(sp + g_tplay + step(i));
  for (int i = 0; i < run.nchars; ++i) t.add(gi + g_tsaved + step(i));
  menus::Saves sv;
  if (menus::saves(&sv) && g_tsaved >= sv.state && g_tsaved + game::kMaxChars * g_tstride <= sv.state + sv.size) {
    const uint32_t off = g_tsaved - sv.state;
    for (int i = 0; i < run.nchars; ++i) {
      t.add(gi + sv.work + off + step(i));
      t.add(gi + sv.copy + off + step(i));
      if (sv.snapshot) {
        t.add(sv.snapshot + sv.state + off + step(i));
        t.add(sv.snapshot + sv.work + off + step(i));
      }
    }
  }
  t.add(sys + g_tlist);
  t.add(sys + g_tlist + game::layout().checkpoint);
  return t;
}

int treasure_held(uintptr_t list, int id) {
  int n = 0;
  for (int i = 0; i < g_tcount; ++i) {
    const uintptr_t e = list + 4 * static_cast<uint32_t>(i);
    if (static_cast<int>(u16at(e)) == id) n += static_cast<int>(u16at(e + 2));
  }
  return n;
}

// How many of a treasure a list holds made `want`, as the game's own changes leave a
// list: its entries topped up to the most, then empty ones taken; an entry left at 0
// cleared when another holds the treasure, else kept (the mark of one found).
// Returns how many did not fit.
int treasure_put(uintptr_t list, int id, int want) {
  int left = want;
  for (int i = 0; i < g_tcount; ++i) {
    const uintptr_t e = list + 4 * static_cast<uint32_t>(i);
    if (static_cast<int>(u16at(e)) != id) continue;
    const int n = left < g_tmost ? left : g_tmost;
    mem::write<uint16_t>(e + 2, static_cast<uint16_t>(n));
    left -= n;
  }
  for (int i = 0; i < g_tcount && left > 0; ++i) {
    const uintptr_t e = list + 4 * static_cast<uint32_t>(i);
    if (u16at(e)) continue;
    const int n = left < g_tmost ? left : g_tmost;
    mem::write<uint16_t>(e, static_cast<uint16_t>(id));
    mem::write<uint16_t>(e + 2, static_cast<uint16_t>(n));
    left -= n;
  }
  bool marked = want > 0;  // some held: no entry at 0 is kept
  for (int i = 0; i < g_tcount; ++i) {
    const uintptr_t e = list + 4 * static_cast<uint32_t>(i);
    if (static_cast<int>(u16at(e)) != id || u16at(e + 2)) continue;
    if (marked) mem::write<uint16_t>(e, 0);
    marked = true;
  }
  return left;
}

// `d` more (or fewer) of a treasure in every list alike, none below 0: a list behind
// the one shown (a pickup since the checkpoint not in the saved ones yet) keeps its
// difference, as the game's own play would have it. Returns how many did not fit in
// the one shown.
int treasure_change(const Lists& t, int id, int d) {
  int left = 0;
  for (int k = 0; k < t.n; ++k) {
    const int have = treasure_held(t.at[k], id);
    if (!have && d <= 0) continue;
    const int out = treasure_put(t.at[k], id, have + d > 0 ? have + d : 0);
    if (k == 0) left = out;
  }
  return left;
}

// The kinds the list shown holds (an entry at 0 holds none), its entries together.
void list_treasures(uintptr_t list, View& v) {
  for (int i = 0; i < g_tcount; ++i) {
    const uintptr_t e = list + 4 * static_cast<uint32_t>(i);
    const int id = static_cast<int>(u16at(e)), n = static_cast<int>(u16at(e + 2));
    if (!id || !n) continue;
    int k = 0;
    while (k < v.tkinds && v.treasure[k].id != id) ++k;
    if (k == v.tkinds) {
      if (k >= kMaxTreasures) continue;
      v.treasure[v.tkinds++].id = id;
    }
    v.treasure[k].count += n;
  }
}

// --- the cases ---------------------------------------------------------------------------------
const game::Character* character(const game::Run& run, int index) {
  for (int i = 0; i < run.nchars; ++i)
    if (run.chars[i].index == index && run.chars[i].control != game::Control::kNetwork) return &run.chars[i];
  return nullptr;
}

// --- the saved cases ---------------------------------------------------------------------------
// Play makes a character's case again from a saved one: the save data's saved state as a
// chapter, a checkpoint or a death's CONTINUE starts (kSigSavedCases: a character's list of
// items, a cItem's data from its id on, the entry its slot, then their counts); QUIT's "No"
// takes that back to the working block (Item Management's cases), and the working block back
// from its checkpoint copy; RESTART and a death's CONTINUE take both back from the play flow's
// copy of them as at the last checkpoint (menus::saves). The box is in none of them - the game
// never changes it in a chapter - so a move between a case and the box at the pause is made in
// every one of them, as if it had been made before each was saved: else a restart brings the
// case back as it was while the box keeps the move, the item in both or in neither (the user's
// play, 2026-10-31: "I put a rifle into the item box while in game then quit the level, and
// this duplicated the item, and I think it also destroyed my rocket launcher" - a Take of the
// rocket launcher and a Store of the Dragunov, then RESTART).
constexpr int kBulletMax = 3;  // among cItem's nine levels
constexpr int kSavedCopies = 5;
struct SavedCase {
  uintptr_t items = 0, count = 0;  // its entries; how many the save wrote (the slots it keeps)
};
struct SavedCases {
  SavedCase at[kSavedCopies];
  int n = 0;
};

// The saved copies understood (menus::saves), the saved cases in the saved state's block.
bool copies_known(menus::Saves* sv) {
  return menus::saves(sv) && g_saved >= sv->state && g_saved_counts + 4 * game::kMaxChars <= sv->state + sv->size;
}

// A character's case (sPlayer's index) in each saved copy, or none: not understood. An extra
// episode's case is made again only from the saved state and the play flow's copy of it: its
// start fills the saved state with its own loadout, its checkpoints save into it, RESTART and a
// death's CONTINUE take it back from the play flow's copy - while the working block (a story
// area's start makes the saved state from it) and its checkpoint copy stay the story's, which
// the episodes never commit and their QUIT takes back. So an episode's move goes into those two
// only, and never into the story's cases.
SavedCases saved_cases(int index, bool story) {
  SavedCases c;
  menus::Saves sv;
  const uintptr_t gi = game::game_info();
  const uint32_t step = static_cast<uint32_t>(index) * g_saved_stride;
  if (!gi || index < 0 || index >= game::kMaxChars || !copies_known(&sv)) return c;
  auto add = [&](uintptr_t base, uint32_t block) {
    c.at[c.n++] = SavedCase{base + block + (g_saved - sv.state) + step,
                            base + block + (g_saved_counts - sv.state) + 4 * static_cast<uint32_t>(index)};
  };
  add(gi, sv.state);
  if (story) {
    add(gi, sv.work);
    add(gi, sv.copy);
  }
  if (sv.snapshot) {
    add(sv.snapshot, sv.state);
    if (story) add(sv.snapshot, sv.work);
  }
  return c;
}

uintptr_t saved_entry(const SavedCase& c, int k) { return c.items + static_cast<uint32_t>(k) * g_saved_item; }
int saved_id(const SavedCase& c, int k) { return static_cast<int>(u32at(saved_entry(c, k))); }
int saved_slots(const SavedCase& c) {
  const int n = mem::read<int32_t>(c.count);
  return n < 0 ? 0 : n > game::layout().nitems ? game::layout().nitems : n;
}

// How much of an item a case holds, a saved one or the live one: a stack's count, else
// how many of it.
int saved_holds(const SavedCase& c, int id, bool stack) {
  const game::Layout& l = game::layout();
  int n = 0;
  for (int k = 0; k < game::kSlots; ++k)
    if (saved_id(c, k) == id) n += stack ? mem::read<int32_t>(saved_entry(c, k) + (l.it_num - l.it_id)) : 1;
  return n;
}
int live_holds(uintptr_t chara, int id, bool stack) {
  int n = 0;
  game::Item it;
  for (int k = 0; k < game::kSlots; ++k)
    if (game::item_in(chara, k, &it) && it.id == id) n += stack ? it.num : 1;
  return n;
}

// Something in hand in a saved case, as the game's own save of a case leaves one: the one
// in hand before (equip 2), else hand-to-hand (the item the store never takes).
void saved_hand(const SavedCase& c) {
  const game::Layout& l = game::layout();
  const uint32_t equip = l.it_equip - l.it_id;
  int before = -1, bare = -1;
  for (int k = 0; k < saved_slots(c); ++k) {
    const uintptr_t e = saved_entry(c, k);
    if (!u32at(e)) continue;
    if (u32at(e + equip) == 1) return;
    if (u32at(e + equip) == 2 && before < 0) before = k;
    if (u32at(e) == g_never && bare < 0) bare = k;
  }
  const int k = before >= 0 ? before : bare;
  if (k >= 0) mem::write<uint32_t>(saved_entry(c, k) + equip, 1);
}

// A Store in a saved case: the item gone from it as from the live one - the entry of the slot
// it left first, else another of the same item (a launcher's by its model: the case holds its
// rounds' id, the save the launcher's) - a stack by as much as was stored (none below 0),
// anything else let go (one of it, whatever its rounds: an empty gun too). One without it
// (picked up since it was saved) is left as it is: the box keeps the item through a restart
// to it.
void saved_out(const SavedCase& c, int slot, int id, int n, bool stack) {
  const game::Layout& l = game::layout();
  const uint32_t num = l.it_num - l.it_id, equip = l.it_equip - l.it_id;
  const int model = inventory::model_id(id);
  bool hand = false;
  if (!stack) n = 1;
  for (int k = -1; k < game::kSlots && n > 0; ++k) {
    if (k == slot) continue;
    const int at = k < 0 ? slot : k;
    const int was = saved_id(c, at);
    if (!was || (was != id && inventory::model_id(was) != model)) continue;
    const uintptr_t e = saved_entry(c, at);
    const int have = mem::read<int32_t>(e + num);
    if (stack && have > n) {
      mem::write<int32_t>(e + num, have - n);
      break;
    }
    hand = hand || u32at(e + equip) == 1;
    mem::write<uint32_t>(e, 0);
    mem::write<uint32_t>(e + equip, 0);
    n = stack ? n - (have > 0 ? have : 0) : 0;
  }
  if (hand) saved_hand(c);
}

// Where a Take's item (`id`) goes in a saved case: the slot it took in the live one when
// that is free there (-1: not taken yet), else the first free one; one with none free (full
// when it was saved, something used up since) gives up an entry of another stack the live
// case holds less of now - what a restart to it would have handed back. -1: none (only what
// the live case still holds, a gun given away since).
int saved_spot(const SavedCase& c, int slot, uintptr_t chara, int id) {
  if (slot >= 0 && slot < game::kSlots && !saved_id(c, slot)) return slot;
  for (int k = 0; k < game::kSlots; ++k)
    if (!saved_id(c, k)) return k;
  for (int k = -1; k < game::kSlots; ++k) {
    const int at = k < 0 ? slot : k;
    if (at < 0 || at >= game::kSlots || (k >= 0 && k == slot)) continue;
    const int other = saved_id(c, at);
    if (other != id && most_of(other) > 1 && live_holds(chara, other, true) < saved_holds(c, other, true)) return at;
  }
  return -1;
}

// A Take in a saved case, at its spot: the new item as the game's own save of a case writes
// one - a cItem's data from its id on, its slot the entry's, not in hand, its BulletMax's top
// bit cleared (the bonus feature's infinite ammo, which play sets again from its setting as it
// makes the character), the id the box held (a launcher's rounds').
void saved_in(const SavedCase& c, int k, uintptr_t item, int id) {
  const game::Layout& l = game::layout();
  const uintptr_t e = saved_entry(c, k);
  for (uint32_t b = 0; b < g_saved_item; ++b) mem::write<uint8_t>(e + b, mem::read<uint8_t>(item + l.it_id + b));
  mem::write<uint32_t>(e, static_cast<uint32_t>(id));
  mem::write<int32_t>(e + (l.it_slot - l.it_id), k);
  mem::write<uint32_t>(e + (l.it_equip - l.it_id), 0);
  const uintptr_t cap = e + (l.it_lv[kBulletMax] - l.it_id);
  mem::write<uint8_t>(cap, static_cast<uint8_t>(u8at(cap) & 0x7F));
}

struct Tally {
  int added = 0, counted = 0, thrown = 0, short_ = 0, gone = 0, stored = 0, taken = 0, in_hand = 0, no_room = 0;
  int full = 0, refused = 0, not_here = 0, unsaved = 0;
  int theirs = 0;                  // the game keeps it for the other character
  int not_offered = 0;             // a Store of what the Add lists do not offer (a scene's own weapon)
  int at_most = 0, list_full = 0;  // treasures: held to the most an entry holds; no room left in the list
  int unsaved_box = 0;             // a Store into an extra episode's box whose file could not be written
};

// What the Add lists offer (items.h's `offer`: what the story gives Chris and Sheva to
// carry); the box's Add puts nothing else in, nor its Store.
bool offered(int id) {
  const items::Info* i = items::find(static_cast<uint32_t>(id));
  return i && i->offer;
}

// --- an extra episode's box's file -------------------------------------------------------------
// Beside the mod, one line per entry:  <item id>:<count>  - a weapon's count its rounds loaded,
// and  :<levels>  after it when it has any (the eight level bytes the box keeps, in hex) - each
// followed by the item's name after ";", for reading only.
std::string path_of(const FileBox& f) { return std::string(config::dir()) + f.file; }

// One line into an entry: false for a line that is no entry (*blank: a blank one, or only a
// comment) or one the box cannot hold - what the Add lists do not offer, what the box's own
// rules refuse, a count past what an entry holds, levels on what stacks.
bool parse_line(std::string line, int* id, int* count, uint8_t lv[kLevels], bool* blank) {
  *blank = false;
  const size_t cut = line.find(';');
  if (cut != std::string::npos) line.resize(cut);
  const char* s = line.c_str();
  while (*s == ' ' || *s == '\t') ++s;
  const char* e = s + std::strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) --e;
  if (e == s) {
    *blank = true;
    return false;
  }
  const std::string text(s, e);
  char* end = nullptr;
  const long i = std::strtol(text.c_str(), &end, 0);
  if (end == text.c_str() || *end != ':') return false;
  const char* n_at = end + 1;
  const long n = std::strtol(n_at, &end, 10);
  if (end == n_at || n < 0 || n > 0xFFFF) return false;
  std::memset(lv, 0, kLevels);
  bool levels = false;
  if (*end == ':') {
    const char* h = end + 1;
    for (int k = 0; k < kLevels; ++k) {
      char two[3] = {h[0], h[0] ? h[1] : '\0', '\0'};
      char* stop = nullptr;
      if (!std::isxdigit(static_cast<unsigned char>(two[0])) || !std::isxdigit(static_cast<unsigned char>(two[1])))
        return false;
      lv[k] = static_cast<uint8_t>(std::strtoul(two, &stop, 16));
      levels = levels || lv[k];
      h += 2;
    }
    end = const_cast<char*>(h);
  }
  if (*end) return false;
  if (i <= 0 || i > 0xFFFF || !offered(static_cast<int>(i)) || !kept(static_cast<int>(i))) return false;
  const int most = most_of(static_cast<int>(i));
  if (most <= 0 || (most > 1 && (n < 1 || n > most || levels))) return false;
  *id = static_cast<int>(i);
  *count = static_cast<int>(n);
  return true;
}

// The box read from its file: none there yet - an empty box; one that cannot be read, or holds
// more than the box does - left as it is, and the box not used this run (nothing is written
// over it).
void read_file(FileBox& f) {
  std::memset(f.data, 0, sizeof(f.data));
  std::memset(f.written, 0, sizeof(f.written));
  f.usable = false;
  f.retry = 0;
  const std::string path = path_of(f);
  if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    f.usable = true;
    logf("itembox: no %s yet - the box of %s starts empty", path.c_str(), f.name);
    return;
  }
  FILE* in = std::fopen(path.c_str(), "r");
  if (!in) {
    logf("ERROR: itembox: %s is there but could not be opened (error %lu) - the box of %s is left alone this run "
         "(nothing is written over it)",
         path.c_str(), GetLastError(), f.name);
    return;
  }
  int lines = 0, n = 0;
  bool bad = false;
  std::string line;
  for (int c = std::fgetc(in);; c = std::fgetc(in)) {
    if (c != '\n' && c != EOF) {
      line += static_cast<char>(c);
      continue;
    }
    ++lines;
    int id = 0, count = 0;
    uint8_t lv[kLevels] = {};
    bool blank = false;
    if (parse_line(line, &id, &count, lv, &blank)) {
      if (n >= g_count) {
        logf("ERROR: itembox: line %d of %s is past the %d entries a box holds", lines, path.c_str(), g_count);
        bad = true;
        break;
      }
      uint8_t* at = f.data + static_cast<uint32_t>(n++) * g_entry;
      const uint16_t id16 = static_cast<uint16_t>(id), count16 = static_cast<uint16_t>(count);
      std::memcpy(at, &id16, 2);
      std::memcpy(at + 2, &count16, 2);
      std::memcpy(at + 4, lv, kLevels);
    } else if (!blank) {
      logf("ERROR: itembox: line %d of %s is not an entry the box can hold: %s", lines, path.c_str(), line.c_str());
      bad = true;
      break;
    }
    line.clear();
    if (c == EOF) break;
  }
  bad = std::ferror(in) != 0 || bad;
  std::fclose(in);
  if (bad) {
    std::memset(f.data, 0, sizeof(f.data));
    logf("ERROR: itembox: %s could not be read - the box of %s is left alone this run (nothing is written over it)",
         path.c_str(), f.name);
    return;
  }
  std::memcpy(f.written, f.data, sizeof(f.data));
  f.usable = true;
  logf("itembox: %s read - the box of %s holds %d entr%s", path.c_str(), f.name, n, n == 1 ? "y" : "ies");
}

// The box written whole into its file: into one beside it, onto the disk, then moved over it in
// one step (a crash halfway leaves the old file). False: not written (the log says why).
bool write_file(FileBox& f) {
  if (!f.usable) return false;
  const std::string path = path_of(f), tmp = path + ".tmp";
  FILE* out = std::fopen(tmp.c_str(), "w");
  if (!out) {
    logf("ERROR: itembox: cannot write %s (error %lu)", tmp.c_str(), GetLastError());
    return false;
  }
  const char* other = f.mode == game::kModeNightmares ? g_files[1].file : g_files[0].file;
  std::fprintf(out,
               "; RE5CabbyCodes - the item box of %s\n"
               ";\n"
               "; The game has no item box in %s, so the mod keeps one of its own here:\n"
               "; what the panel's item box holds at the pause in that episode. The other extra episode has\n"
               "; one of its own (%s); the story's box is the game's own,\n"
               "; in its save. Nothing the game does changes this box - only the panel's Store, Take, Add,\n"
               "; x and counts do, each written here at once.\n"
               ";\n"
               ";   <item id>:<count>             ammo, recovery items, grenades and the like: how many\n"
               ";   <item id>:<rounds>            a weapon: the rounds it has loaded\n"
               ";   <item id>:<rounds>:<levels>   ... with its upgrade levels as the box keeps them\n"
               ";\n"
               "; The ids are the game's own; what follows \";\" on a line is only the item's name. The mod\n"
               "; reads this file as the game starts and writes it whole at every change, so edit it only\n"
               "; while the game is closed. Deleting it empties the box. A file with a line the mod cannot\n"
               "; read is left as it is, and its box cannot be used until the game starts again.\n",
               f.name, f.name, other);
  int n = 0;
  for (int i = 0; i < g_count; ++i) {
    const uint8_t* at = f.data + static_cast<uint32_t>(i) * g_entry;
    uint16_t id = 0, count = 0;
    std::memcpy(&id, at, 2);
    std::memcpy(&count, at + 2, 2);
    if (!id) continue;
    ++n;
    char data[48], note[96];
    int k = 0;
    while (k < kLevels && !at[4 + k]) ++k;
    int w = std::snprintf(data, sizeof(data), "0x%03X:%u", id, count);
    if (k < kLevels) {
      w += std::snprintf(data + w, sizeof(data) - w, ":");
      for (int b = 0; b < kLevels; ++b) w += std::snprintf(data + w, sizeof(data) - w, "%02X", at[4 + b]);
    }
    const items::Info* info = items::find(id);
    if (info && info->magazine)
      std::snprintf(note, sizeof(note), "%s, %u round%s loaded", info->name, count, count == 1 ? "" : "s");
    else std::snprintf(note, sizeof(note), "%s", info ? info->name : "?");
    std::fprintf(out, "%-32s ; %s\n", data, note);
  }
  // Onto the disk before the move: fclose only hands the bytes to the system's cache.
  bool ok = std::fflush(out) == 0 && !std::ferror(out);
  const HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(out)));
  if (ok && h != INVALID_HANDLE_VALUE) FlushFileBuffers(h);
  ok = std::fclose(out) == 0 && ok;
  DWORD err = 0;
  if (!ok || !files::replace(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH, &err)) {
    logf("ERROR: itembox: cannot write %s (error %lu) - the box of %s is kept, and written again", path.c_str(),
         ok ? err : GetLastError(), f.name);
    DeleteFileA(tmp.c_str());
    return false;
  }
  std::memcpy(f.written, f.data, sizeof(f.data));
  logf("itembox: %s written - the box of %s holds %d entr%s", path.c_str(), f.name, n, n == 1 ? "y" : "ies");
  return true;
}

// The file brought to the box, when they differ: at once after a change, else a second after a
// write that failed.
void keep_file(FileBox& f, bool now) {
  if (!f.usable || !unwritten(f)) return;
  if (!now && f.retry > 0) {
    --f.retry;
    return;
  }
  f.retry = write_file(f) ? 0 : 60;
}

void load_files() {
  for (FileBox& f : g_files) read_file(f);
}

void store(const Box& box, const game::Run& run, const Change& c, Tally& t) {
  const game::Character* ch = character(run, c.chara);
  game::Item it;
  if (!ch || !game::item_in(ch->obj, c.slot, &it) || it.id != c.id) {
    ++t.gone;
    return;
  }
  if (it.equip == 1) {
    ++t.in_hand;
    return;
  }
  // What the box keeps it as: a grenade launcher its load, as the game's own save of a case
  // writes it (in play the case's item has the launcher's id, its model the rounds loaded).
  const int id = inventory::as_saved(it);
  // The box takes only what the Add lists offer - not a scene's own weapon or its rounds (the
  // Lantern, the Flamethrower, the L.T.D., the RPG-7 NVS and its round; the user, 2026-11-01:
  // "store should refuse those types of items").
  if (!offered(id)) {
    ++t.not_offered;
    logf("itembox: %s's slot %d (%s) not stored - the box takes only what the Add lists offer", ch->name, c.slot,
         name_of(id));
    return;
  }
  const int most = most_of(id);
  if (most <= 0 || !kept(id)) {
    ++t.refused;
    return;
  }
  uint8_t before[sizeof(FileBox::data)];  // an extra episode's box as it was
  if (box.file) std::memcpy(before, box.file->data, sizeof(before));
  if (most > 1) {
    if (room_for_stack(box, id) < it.num) {
      ++t.full;
      return;
    }
    place_stack(box, id, it.num);
  } else {
    uint8_t lv[kLevels] = {};
    for (int k = 0; k < kKept; ++k) lv[k] = it.lv[g_lvmap[k]];
    if (!place_one(box, id, it.num, lv)) {
      ++t.full;
      return;
    }
  }
  // An extra episode's box keeps it in its file before it leaves the case - or it stays there.
  if (box.file && !write_file(*box.file)) {
    std::memcpy(box.file->data, before, sizeof(before));
    ++t.unsaved_box;
    logf("itembox: %s's slot %d (%s) not stored - the box of %s could not be written to its file", ch->name, c.slot,
         name_of(id), box.file->name);
    return;
  }
  inventory::clear_item(it.at);
  const SavedCases sc = saved_cases(ch->index, !box.file);
  for (int k = 0; k < sc.n; ++k) saved_out(sc.at[k], c.slot, id, it.num, most > 1);
  ++t.stored;
  logf("itembox: %s's slot %d (%s, %d) stored", ch->name, c.slot, name_of(id), it.num);
}

void take(const Box& box, const game::Run& run, const Change& c, Tally& t) {
  const game::Layout& l = game::layout();
  const game::Character* ch = character(run, c.chara);
  if (c.index < 0 || c.index >= g_count || box.id(c.index) != c.id || !ch) {
    ++t.gone;
    return;
  }
  const items::Info* info = items::find(static_cast<uint32_t>(c.id));
  // A weapon the game keeps for one character (Item Management's own check: the
  // Gatling gun is Chris's, the Longbow Sheva's) goes to no one else.
  if (info && info->only && std::strcmp(info->only, ch->name) != 0) {
    ++t.theirs;
    logf("itembox: entry %d (%s) not taken into %s's case - the game keeps it for %s", c.index, name_of(c.id),
         ch->name, info->only);
    return;
  }
  // Its model's archive not loaded here: the add would make the game quit.
  if (!inventory::in_area(c.id)) {
    ++t.not_here;
    logf("itembox: entry %d (%s) not taken - its model is not loaded in this area", c.index, name_of(c.id));
    return;
  }
  const bool stack = most_of(c.id) > 1;
  const int have = box.count(c.index);
  // A stack: what one slot of the case holds, the rest left in the box.
  const int slot_most = info && info->most > 0 ? info->most : 1;
  const int n = stack ? (have < slot_most ? have : slot_most) : have;
  // Every saved copy of the case takes it too, or a restart to that one would lose it.
  const SavedCases sc = saved_cases(ch->index, !box.file);
  for (int k = 0; k < sc.n; ++k)
    if (saved_spot(sc.at[k], -1, ch->obj, c.id) < 0) {
      ++t.unsaved;
      logf("itembox: entry %d (%s) not taken - %s's case as the game last saved it has no room for it", c.index,
           name_of(c.id), ch->name);
      return;
    }
  uint8_t lv[9] = {};
  for (int k = 0; k < kKept; ++k) lv[g_lvmap[k]] = static_cast<uint8_t>(u8at(box.at(c.index) + 4 + k));
  const uintptr_t made = inventory::add_item(ch->obj, c.id, n, lv);
  if (!made) {
    ++t.no_room;
    return;
  }
  int got = n;
  if (stack) {
    const int num = mem::read<int32_t>(made + l.it_num);
    if (num > slot_most) mem::write<int32_t>(made + l.it_num, slot_most);
    got = num < 1 ? n : num > slot_most ? slot_most : num;
  }
  if (stack && have > got) box.put16(c.index, 2, have - got);
  else box.clear(c.index);
  const int slot = mem::read<int32_t>(made + l.it_slot);
  for (int k = 0; k < sc.n; ++k) {
    const int at = saved_spot(sc.at[k], slot, ch->obj, c.id);
    if (at >= 0) saved_in(sc.at[k], at, made, c.id);
  }
  ++t.taken;
  logf("itembox: entry %d (%s, %d) taken into %s's case (slot %d)", c.index, name_of(c.id), got, ch->name,
       mem::read<int32_t>(made + l.it_slot));
}

// One batch into the box and the treasures' lists; what was done (or not) in the note.
void make(const Box& box, const Lists& tl, const std::vector<Change>& batch, const game::Run& run, bool moves,
          View& v) {
  Tally t;
  // The entries' changes first (their indexes are the box as it was shown), then
  // the takes, the stores and the adds.
  for (const Change& c : batch) {
    if (c.op != Op::kSet) continue;
    if (c.index < 0 || c.index >= g_count || box.id(c.index) != c.id || !c.id) {
      ++t.gone;
      continue;
    }
    if (c.count <= 0) {
      box.clear(c.index);
      ++t.thrown;
      logf("itembox: entry %d (%s) thrown away", c.index, name_of(c.id));
      continue;
    }
    const int most = most_of(c.id);
    if (most <= 1) continue;  // what does not stack keeps its count (a gun's rounds)
    const int n = c.count > most ? most : c.count;
    logf("itembox: entry %d (%s) count %d -> %d", c.index, name_of(c.id), box.count(c.index), n);
    box.put16(c.index, 2, n);
    ++t.counted;
  }
  for (const Change& c : batch)
    if (c.op == Op::kTake && moves) take(box, run, c, t);
  for (const Change& c : batch)
    if (c.op == Op::kStore && moves) store(box, run, c, t);
  for (const Change& c : batch) {
    if (c.op != Op::kAdd || !offered(c.id) || most_of(c.id) <= 0 || !kept(c.id) || c.count <= 0) continue;
    int in = 0;
    if (most_of(c.id) > 1) {
      in = place_stack(box, c.id, c.count);
    } else {
      const items::Info* info = items::find(static_cast<uint32_t>(c.id));
      const int rounds = info && info->cat == 1 && info->num > 0 ? info->num : 1;  // a gun comes loaded
      const uint8_t none[kLevels] = {};
      while (in < c.count && place_one(box, c.id, rounds, none)) ++in;
    }
    t.added += in > 0;
    t.short_ += in < c.count;
    logf("itembox: %s x%d added (%d went in)", name_of(c.id), c.count, in);
  }
  // The treasures: how many are held, as the list shown has them, then the same
  // change in every list.
  for (const Change& c : batch) {
    if ((c.op != Op::kTreasureSet && c.op != Op::kTreasureAdd) || c.id <= 0 || c.count < 0) continue;
    const int have = tl.n ? treasure_held(tl.at[0], c.id) : 0;
    int d = 0;
    if (c.op == Op::kTreasureSet) {
      if (!have) {  // none held any more
        ++t.gone;
        continue;
      }
      d = (c.count < g_tmost ? c.count : g_tmost) - have;
    } else if (tl.n) {
      d = c.count < g_tmost - have ? c.count : g_tmost - have;
      if (d < 0) d = 0;
      t.at_most += d < c.count;
    }
    if (!d) continue;
    const int out = treasure_change(tl, c.id, d);
    t.list_full += out > 0;
    if (c.op == Op::kTreasureAdd) ++t.added;
    else if (have + d == 0) ++t.thrown;
    else ++t.counted;
    logf("itembox: treasure %s %d -> %d (%d list(s))", name_of(c.id), have, have + d - out, tl.n);
  }
  char text[200];
  int at = 0;
  auto say = [&](const char* what, int n) {
    if (n <= 0 || at >= static_cast<int>(sizeof(text)) - 2) return;
    if (at) at += std::snprintf(text + at, sizeof(text) - at, "; ");
    at += std::snprintf(text + at, sizeof(text) - at, "%d %s", n, what);
  };
  say("stored", t.stored);
  say("taken", t.taken);
  say("added", t.added);
  say("changed", t.counted);
  say("thrown away", t.thrown);
  say(t.in_hand == 1 ? "is in hand (change weapons first)" : "are in hand (change weapons first)", t.in_hand);
  say("not stored: the box takes only what the Add lists offer", t.not_offered);
  say("had no room in that case", t.no_room);
  say(t.unsaved == 1 ? "not taken: that case as the game last saved it has no room (a restart would lose it)"
                     : "not taken: that case as the game last saved it has no room (a restart would lose them)",
      t.unsaved);
  say(t.theirs == 1 ? "not taken: the game keeps it for the other character"
                    : "not taken: the game keeps them for the other character",
      t.theirs);
  say("could not be loaded here (see the log)", t.not_here);
  say(t.full + t.short_ == 1 ? "did not fit (the box is full)" : "did not all fit (the box is full)", t.full + t.short_);
  char most[64];
  std::snprintf(most, sizeof(most), "held at %d (the most of a treasure)", g_tmost);
  say(most, t.at_most);
  say("did not fit (the treasures' list is full)", t.list_full);
  say(t.unsaved_box == 1 ? "not stored: the box's file could not be written (see the log)"
                         : "not stored: the box's file could not be written (see the log)",
      t.unsaved_box);
  say("cannot go in the box", t.refused);
  say("had changed - left alone", t.gone);
  if (!at) std::snprintf(text, sizeof(text), "Nothing to change.");
  note(v, text);
}

// The treasures' lists and Item Management's rebuild of its treasures list, once the
// box is understood (`rebuild_fn`: its rebuild of the box list).
bool discover_treasures(uintptr_t rebuild_fn) {
  const game::Layout& l = game::layout();
  const mt::Class* sp = mt::find("sPlayer");
  const mt::Class* scr = mt::find("uIdItemBox00");
  const uintptr_t count = game::sig_one("kSigTreasureCount");
  const uintptr_t add = game::sig_one("kSigTreasureAdd");
  const uintptr_t load = game::sig_one("kSigTreasureLoad");
  const uintptr_t lists = game::sig_one("kSigBoxLists");
  const uintptr_t back = game::sig_one("kSigCheckpoint");
  const uintptr_t saved = game::sig_one("kSigSavedCases");
  if (!sp || !scr || !count || !add || !load || !lists || !back || !saved)
    return no_treasures("the treasures' lists were not found");

  // cSystemInfo's, right after the box: its set-count's reads of it, and the
  // checkpoint's return to its copy, agree.
  g_tlist = u32at(count + 23);
  g_tcount = static_cast<int>(u8at(count + 57));
  if (u32at(count + 15) != g_tlist + 2 || u32at(count + 43) != g_tlist || u32at(count + 69) != g_tlist ||
      u32at(back + 27) != g_tlist || g_tlist != g_box + g_entry * static_cast<uint32_t>(g_count) || g_tcount < 1 ||
      g_tcount > kMaxTreasures || g_tlist + 4 * static_cast<uint32_t>(g_tcount) > 4 + l.checkpoint)
    return no_treasures("the profile's treasures (they disagree)");

  // Each character's in play: sPlayer's add of a treasure, as many entries; up to
  // the most in an entry.
  g_tstride = u32at(add + 2);
  g_tplay = u32at(add + 14);
  g_tmost = static_cast<int>(u16at(add + 63));
  if (u8at(add + 74) != static_cast<uint32_t>(g_tcount) || u8at(add + 95) != static_cast<uint32_t>(g_tcount) ||
      g_tstride != 4 * static_cast<uint32_t>(g_tcount) || g_tmost < 1 ||
      g_tplay + game::kMaxChars * g_tstride > sp->size)
    return no_treasures("the characters' treasures (they disagree)");

  // ... and each one's saved: a character made from the saved state takes its case,
  // then its treasures from them (into the same lists, as many).
  g_tsaved = u32at(load + 32) - l.game_base;
  if (load < saved || load > saved + 0x200 || u32at(load + 2) != g_tstride || u32at(load + 25) != g_tplay ||
      u8at(load + 13) != l.profile_index || u32at(load + 16) != l.game_size ||
      u8at(load + 37) != static_cast<uint32_t>(g_tcount) || g_tsaved < g_saved_counts + 4 * game::kMaxChars ||
      g_tsaved + game::kMaxChars * g_tstride > l.game_size)
    return no_treasures("the characters' saved treasures (they disagree)");

  // The screen's lists of each pane made again: the box's (its rebuild, as found),
  // then the treasures'.
  const uintptr_t trebuild = game::call_target(lists + 35);
  const uint32_t list_field = u8at(rebuild_fn + 17);
  if (game::call_target(lists + 27) != rebuild_fn || u32at(lists + 10) != g_pane || u32at(lists + 45) != g_pane_step ||
      u8at(lists + 21) != list_field || u8at(lists + 42) != list_field || !image::text().contains(trebuild))
    return no_treasures("Item Management's rebuild of its treasures list (its call disagrees)");
  // Its start: a pane's list as the box's rebuild reads it, the same step between
  // the lists and as many cells, its own right after the box's.
  if (!mem::matches(trebuild, mem::parse_pattern("55 8B EC 83 E4 F8 83 EC ?? 53 8B 5D 08 8B 43 ?? 69 C0 ?? ?? ?? ?? "
                                                 "56 57 8B F9 68 ?? ?? ?? ?? 8D B4 38 ?? ?? ?? ??")) ||
      u8at(trebuild + 15) != list_field || u32at(trebuild + 18) != u32at(rebuild_fn + 20) ||
      u32at(trebuild + 27) != u32at(rebuild_fn + 27) ||
      u32at(trebuild + 34) != u32at(rebuild_fn + 34) + u32at(rebuild_fn + 27) ||
      u32at(trebuild + 27) % static_cast<uint32_t>(g_tcount) ||
      u32at(trebuild + 34) + u32at(trebuild + 18) + u32at(trebuild + 27) > scr->size)
    return no_treasures("Item Management's rebuild of its treasures list (its start)");

  g_trebuild = reinterpret_cast<RebuildFn>(trebuild);
  g_tfound = true;
  logf("itembox: the treasures cSystemInfo +0x%X (%d entries, up to %d a treasure), each character's sPlayer +0x%X "
       "and saved cGameInfo +0x%X (+0x%X a character); Item Management's rebuild of its treasures list exe+0x%X",
       g_tlist, g_tcount, g_tmost, g_tplay, g_tsaved, g_tstride, static_cast<unsigned>(trebuild - image::base()));
  return true;
}

}  // namespace

void init() {
  if (g_lock_ready) return;
  InitializeCriticalSection(&g_lock);
  g_lock_ready = true;
}

bool discover() {
  const mt::Class* scr = mt::find("uIdItemBox00");
  const mt::Class* sys = mt::find("cSystemInfo");
  const uintptr_t free_check = game::sig_one("kSigBoxFree");
  const uintptr_t rules = game::sig_one("kSigBoxRules");
  const uintptr_t keep = game::sig_one("kSigBoxKeep");
  const uintptr_t call = game::sig_one("kSigBoxRebuildCall");
  const uintptr_t panes = game::sig_one("kSigBoxPanes");
  const uintptr_t update = game::sig_one("kSigBoxPaneUpdate");
  const uintptr_t levels = game::sig_one("kSigBoxLevels");
  const uintptr_t saved = game::sig_one("kSigSavedCases");
  if (!game::ready() || !scr || !scr->vtable || !sys || !free_check || !rules || !keep || !call || !panes ||
      !update || !levels || !saved)
    return off("the item box or Item Management's screen was not found");
  const game::Layout& l = game::layout();

  // The box: "is there an empty entry" and the store agree on it; it lies in the live data.
  g_box = u32at(free_check + 2);
  g_entry = u8at(free_check + 19);
  g_count = static_cast<int>(u8at(free_check + 22));
  if (u32at(rules + 86) != g_box || g_entry < 4 + kLevels || g_entry > kMaxEntryBytes || g_count < 1 ||
      g_count > kMaxEntries || g_box < 4 || g_box + g_entry * static_cast<uint32_t>(g_count) > 4 + l.checkpoint)
    return off("the item box (its parts disagree)");

  // The store's rules, out of its compares.
  g_never = u32at(rules + 2);
  g_a_lo = u32at(rules + 14);
  g_a_hi = u32at(rules + 27);
  g_b = u32at(rules + 35);
  g_c = u32at(rules + 43);
  g_d_lo = u32at(rules + 51);
  g_d_hi = u32at(rules + 59);
  g_e_lo = 0u - u32at(rules + 67);  // lea edx, [esi - lo]
  g_e_hi = g_e_lo + u32at(rules + 73);
  g_most = static_cast<int>(u32at(rules + 80));
  g_drop = u16at(keep + 6);
  g_keep1 = u16at(keep + 13);
  g_keep2 = u16at(keep + 20);
  if (g_most < 2 || g_most > 0xFFFF || g_a_lo > g_a_hi || g_d_lo > g_d_hi || g_e_lo > g_e_hi || g_e_hi > 0xFFFF ||
      !g_drop || g_keep1 < g_drop || g_keep2 < g_drop)
    return off("the item box's rules (they make no sense)");

  // The levels an entry keeps: a saved item's (a cItem's data from its id on) seven
  // levels, copied one after the other into the cell's, the eighth cleared.
  const uint32_t lv_dst0 = u8at(levels + 6);
  static const int kSrc[kKept] = {3, 10, 17, 24, 31, 38, 45}, kDst[kKept] = {6, 13, 20, 27, 34, 41, 49};
  for (int k = 0; k < kKept; ++k) {
    if (u8at(levels + kDst[k]) != lv_dst0 + static_cast<uint32_t>(k))
      return off("the item box's levels (not one after the other)");
    g_lvmap[k] = -1;
    for (int i = 0; i < 9; ++i)
      if (l.it_lv[i] - l.it_id == u8at(levels + kSrc[k])) g_lvmap[k] = i;
    if (g_lvmap[k] < 0) return off("the item box's levels (not a cItem's)");
  }
  if (u8at(levels + 52) != lv_dst0 + kKept) return off("the item box's levels (no eighth)");

  // The saved cases: four lists of the case's items, then their counts.
  g_saved = u32at(saved + 50) - l.game_base;
  g_saved_counts = u32at(saved + 57) - l.game_base;
  g_saved_stride = u32at(saved + 16);
  g_saved_item = l.item_size - l.it_id;
  if (u32at(saved + 8) != l.game_size || u32at(saved + 2) * 4 != l.game_size || u32at(saved + 35) != u32at(saved + 43) ||
      g_saved_stride != static_cast<uint32_t>(l.nitems) * g_saved_item ||
      g_saved + game::kMaxChars * g_saved_stride != g_saved_counts || g_saved_counts + 4 * game::kMaxChars > l.game_size)
    return off("the saved cases (they disagree)");

  // The screen's sale of a gun's rounds: the store on the profile's cSystemInfo,
  // then the rebuild - the store being the function the rules are in.
  const uintptr_t store_fn = game::call_target(call + 13);
  const uintptr_t rebuild_fn = game::call_target(call + 21);
  if (!store_fn || rules < store_fn || rules > store_fn + 0x40 || u8at(call + 2) != l.profile_index ||
      u32at(call + 5) != l.system_size || u8at(call + 12) != l.system_base || !image::text().contains(rebuild_fn))
    return off("Item Management's rebuild of its box list (its call disagrees)");
  // The rebuild's start: a pane's list (its field), the lists' step, a list's
  // bytes, the first list - two lists (the box's, the treasures') a step, in the screen.
  if (!mem::matches(rebuild_fn, mem::parse_pattern("55 8B EC 83 E4 F8 83 EC ?? 53 56 57 8B 7D 08 8B 47 ?? 69 C0 ?? ?? "
                                                   "?? ?? 8B D9 68 ?? ?? ?? ?? 8D B4 18 ?? ?? ?? ??")) ||
      u32at(rebuild_fn + 20) != 2 * u32at(rebuild_fn + 27) || u32at(rebuild_fn + 27) % static_cast<uint32_t>(g_count) ||
      u32at(rebuild_fn + 34) + 2 * u32at(rebuild_fn + 20) > scr->size)
    return off("Item Management's rebuild of its box list (its start)");

  // The panes: the screen's frame (its vtable slot 6) walks them.
  const uintptr_t frame = mem::read<uintptr_t>(scr->vtable + 4 * 6);
  if (panes < frame || panes > frame + 0x400 || update < panes || update > panes + 0x200)
    return off("Item Management's panes (not in its frame)");
  const int back = s8at(update + 6);  // the pane from the walk's pointer
  g_pane = u32at(panes + 2) + static_cast<uint32_t>(back);
  g_pane_step = u32at(panes + 19);
  g_in_use = static_cast<uint32_t>(s8at(update + 2) - back);
  if (u32at(update + 22) != g_pane_step || g_pane + 2 * g_pane_step > scr->size || g_in_use + 4 > g_pane_step)
    return off("Item Management's panes (they disagree)");

  // A pane's states: its update's switch on the pane's first byte. The states that
  // browse open with the browsing entry (step 0: refresh, to step 1; step 1: run;
  // else wait); the opening state ends its animation on its menu's step.
  const uintptr_t pane_update = game::call_target(update + 12);
  const uintptr_t sw =
      pane_update ? mem::find_pattern(mem::Range{pane_update, pane_update + 0x200},
                                      "0F B6 06 83 F8 ?? 0F 87 ?? ?? ?? ?? FF 24 85 ?? ?? ?? ??")
                  : 0;
  if (!sw) return off("Item Management's pane states (no switch)");
  const uint32_t states = u8at(sw + 5) + 1;
  const uintptr_t table = u32at(sw + 15);
  if (states > 32 || !mem::readable(table, 4 * states)) return off("Item Management's pane states (the table)");
  const mem::Pattern stub = mem::parse_pattern("56 8B CF E8 ?? ?? ?? ??");
  const mem::Pattern entry = mem::parse_pattern(
      "0F B6 C8 83 E9 00 89 74 24 10 74 ?? 83 E9 01 74 ?? 5E 5D 5B C2 04 00 04 01 C6 45 ?? 01 88 45 ??");
  const mem::Pattern opening = mem::parse_pattern("88 5E ?? 5B 5F F3 0F 11 4E ?? C6 46 ?? ?? 5E C2 04 00");
  g_browse = 0;
  g_sub = 0;
  int openings = 0;
  uint32_t opening_sub = 0;
  for (uint32_t s = 0; s < states; ++s) {
    const uintptr_t at = mem::read<uintptr_t>(table + 4 * s);
    if (!image::text().contains(at) || !mem::matches(at, stub)) continue;
    const uintptr_t handler = game::call_target(at + 3);
    if (!handler) continue;
    if (const uintptr_t m = mem::find_pattern(mem::Range{handler, handler + 0x100}, entry)) {
      if (g_sub && u8at(m + 31) != g_sub) return off("Item Management's pane states (two steps)");
      g_sub = u8at(m + 31);
      g_browse |= 1u << s;
    }
    if (const uintptr_t m = mem::find_pattern(mem::Range{handler, handler + 0x100}, opening)) {
      g_menu_state = s;
      g_menu_step = u8at(m + 13);
      opening_sub = u8at(m + 12);
      ++openings;
    }
  }
  int browse = 0;
  for (uint32_t s = 0; s < 32; ++s) browse += (g_browse >> s) & 1;
  if (browse != 2 || !g_sub || g_sub >= g_in_use || openings != 1 || browses(g_menu_state) || opening_sub != g_sub)
    return off("Item Management's pane states (not the two browsing ones and the menu)");

  g_rebuild = reinterpret_cast<RebuildFn>(rebuild_fn);
  g_found = true;
  logf("itembox: the box cSystemInfo +0x%X (%d entries of %u bytes; up to %d of what stacks; levels kept: cItem's "
       "%d %d %d %d %d %d %d); saved cases cGameInfo +0x%X (+0x%X a character, counts +0x%X); Item Management's "
       "rebuild exe+0x%X, panes +0x%X + 0x%X each (in use while +0x%X >= 0; idle in states mask 0x%X at step 1 or "
       "state %u at step %u, the step +0x%X)",
       g_box, g_count, g_entry, g_most, g_lvmap[0], g_lvmap[1], g_lvmap[2], g_lvmap[3], g_lvmap[4], g_lvmap[5],
       g_lvmap[6], g_saved, g_saved_stride, g_saved_counts, static_cast<unsigned>(rebuild_fn - image::base()), g_pane,
       g_pane_step, g_in_use, g_browse, g_menu_state, g_menu_step, g_sub);
  discover_treasures(rebuild_fn);  // the box without them if they are not understood
  load_files();                    // the extra episodes' boxes, from their files
  return true;
}

bool screen_idle(uintptr_t screen) { return g_found && screen && idle(screen); }

int most_of(int id) {
  if (!g_found || id <= 0 || static_cast<uint32_t>(id) == g_never) return 0;
  const uint32_t u = static_cast<uint32_t>(id);
  const bool stacks = (u >= g_a_lo && u <= g_a_hi) || u == g_b || u == g_c || (u >= g_d_lo && u <= g_d_hi) ||
                      (u >= g_e_lo && u <= g_e_hi);
  return stacks ? g_most : 1;
}

bool kept(int id) {
  const uint32_t u = static_cast<uint32_t>(id);
  return g_found && (u < g_drop || u == g_keep1 || u == g_keep2);
}

void tick(const game::Run& run) {
  if (!g_lock_ready) return;
  View v;
  v.found = g_found;
  v.size = g_count;
  std::vector<std::vector<Change>> batches;
  EnterCriticalSection(&g_lock);
  if (g_pending.empty()) batches.swap(g_batches);  // while one waits for the loader, the next ones wait their turn
  const View was = g_view;
  LeaveCriticalSection(&g_lock);
  std::snprintf(v.note, sizeof(v.note), "%s", was.note);

  const uintptr_t screen = g_found ? run.item_screen : 0;
  const uintptr_t sys = g_found ? game::system_info() : 0;
  v.open = screen && sys;
  // In play, whose box by the game's mode: a story chapter's the game's own, an extra
  // episode's the mod's (its file); the Mercenaries and Versus have none.
  const bool in_a_game = g_found && !screen && sys && run.in_game;
  const bool playing = in_a_game && !run.mercenaries;
  FileBox* fb = playing ? file_box(run.mode) : nullptr;
  v.in_play = playing && (run.mode == game::kModeStory || fb);
  if (fb) {
    v.mode = fb->mode;
    v.episode = fb->name;
    v.file = fb->file;
    v.unreadable = !fb->usable;
  }
  if (v.open) v.editable = idle(screen);
  else if (v.in_play) v.editable = run.screen == game::Screen::kPause && !v.unreadable;
  if (menus::leaving()) v.editable = false;  // Chapter Select on its way: nothing more is changed
  // A move at the pause is made in every saved copy of the case too: none without them.
  menus::Saves sv;
  v.can_move = v.in_play && v.editable && inventory::can_change() && copies_known(&sv);
  if (v.open != was.open || v.in_play != was.in_play || v.mode != was.mode)
    v.note[0] = '\0';  // a new place starts with no note
  const int whose = v.in_play ? v.mode : -1;
  if (in_a_game && whose != g_logged_mode) {
    g_logged_mode = whose;
    if (fb)
      logf("itembox: a game of %s - the panel's box is the episode's own (%s)%s", fb->name, fb->file,
           fb->usable ? "" : ", whose file could not be read: none this run");
    else if (v.in_play) logf("itembox: a story game - the panel's box is the game's own");
    else logf("itembox: no item box in this game (its mode %d)", run.mode);
  }
  // The batch waiting for the loader: made once its Takes' items are in (or the
  // wait is over: then the Takes of what is still not loaded are refused), dropped
  // if the pause closes first.
  bool resumed = false;
  if (!g_pending.empty()) {
    char text[200];
    if (!v.can_move || g_pending_mode != v.mode) {
      std::snprintf(text, sizeof(text), "Not taken: the pause closed while %s was loading - try again.",
                    name_of(g_pending_ids[0]));
      note(v, text);
      logf("itembox: the pause closed while %s was loading - its batch dropped", name_of(g_pending_ids[0]));
      g_pending.clear();
    } else {
      inventory::refresh_area();
      bool all = true;
      for (const int id : g_pending_ids) all = all && inventory::ready(id);
      if (all || ++g_pending_age > inventory::kLoadTicks) {
        logf(all ? "itembox: %s loaded after %d frame(s)" : "itembox: %s still not loaded after %d frames",
             name_of(g_pending_ids[0]), g_pending_age);
        batches.insert(batches.begin(), g_pending);
        g_pending.clear();
        resumed = true;
      }
    }
  }
  if (v.open || (v.in_play && !v.unreadable)) {
    // At the pause the checkpoint copy is written too; beside Item Management the
    // screen keeps the live box itself as it closes. An extra episode's box is the
    // mod's own, and the episode has no treasures.
    const Box box = fb ? Box{reinterpret_cast<uintptr_t>(fb->data), 0, fb}
                       : Box{sys + g_box, v.in_play ? sys + g_box + game::layout().checkpoint : 0, nullptr};
    const Lists tl = fb ? Lists{} : treasure_lists(run, sys, v.open);
    bool box_changed = false, treasures_changed = false;
    for (size_t bi = 0; bi < batches.size(); ++bi) {
      const std::vector<Change>& b = batches[bi];
      if (!v.editable) {
        note(v, v.open ? "Not changed: the screen was busy (an item picked up, a shop list open) - try again."
                       : "Not changed: pause the game first.");
        continue;
      }
      if (v.can_move) {
        inventory::refresh_area();  // a Take goes by what is loaded now
        // Takes of what this area has not loaded: the game's loader first, and the
        // batch (and what came after it) waits for it.
        std::vector<int> need;
        for (const Change& c : b)
          if (c.op == Op::kTake && c.index >= 0 && c.index < g_count && box.id(c.index) == c.id &&
              !inventory::in_area(c.id) && std::find(need.begin(), need.end(), c.id) == need.end())
            need.push_back(c.id);
        if (!need.empty() && !(resumed && bi == 0)) {
          bool asked = true;
          for (const int id : need) asked = inventory::load(id) && asked;
          if (asked) {
            g_pending = b;
            g_pending_ids = need;
            g_pending_age = 0;
            g_pending_mode = v.mode;
            char text[200];
            std::snprintf(text, sizeof(text), "Loading %s%s...", name_of(need[0]), need.size() > 1 ? " and the rest" : "");
            note(v, text);
            EnterCriticalSection(&g_lock);
            g_batches.insert(g_batches.begin(), batches.begin() + static_cast<std::ptrdiff_t>(bi) + 1, batches.end());
            LeaveCriticalSection(&g_lock);
            break;
          }
        }
      }
      make(box, tl, b, run, v.can_move, v);
      for (const Change& c : b)
        (c.op == Op::kTreasureSet || c.op == Op::kTreasureAdd ? treasures_changed : box_changed) = true;
    }
    if (v.open && (box_changed || treasures_changed)) rebuild(screen, box_changed, treasures_changed);
    if (fb && box_changed) keep_file(*fb, true);  // an extra episode's box: its file at once
    v.treasures = tl.n > 0;
    v.tmost = g_tmost;
    if (v.treasures) list_treasures(tl.at[0], v);
    for (int i = 0; i < g_count && i < kMaxEntries; ++i) {
      Entry& en = v.entry[i];
      en.id = box.id(i);
      if (!en.id) continue;
      ++v.used;
      en.count = box.count(i);
      for (int k = 0; k < kLevels; ++k) en.lv[k] = static_cast<uint8_t>(u8at(box.at(i) + 4 + k));
      en.avail = v.can_move ? inventory::avail(en.id) : inventory::kNever;
    }
    // At the pause what the box holds is loaded before it is taken - each item once a
    // pause, the weapons first, while the game stands still anyway - so a Take is
    // made at once.
    if (v.can_move) {
      std::vector<int> ids;
      for (int i = 0; i < g_count && i < kMaxEntries; ++i)
        if (v.entry[i].id && v.entry[i].avail == inventory::kLoadable) ids.push_back(v.entry[i].id);
      auto rank = [](int id) {
        const items::Info* info = items::find(static_cast<uint32_t>(id));
        return info ? static_cast<int>(info - items::kItems) : items::kCount;
      };
      std::stable_sort(ids.begin(), ids.end(), [&](int a, int b) { return rank(a) < rank(b); });
      inventory::preload(ids);
    }
  } else if (!batches.empty()) {
    note(v, v.unreadable ? "Not changed: this box's file could not be read (see RE5CabbyCodes.log)."
                         : "Not changed: the box is changed at the pause, or beside Item Management.");
  }
  for (FileBox& f : g_files) keep_file(f, false);  // a write that failed, tried again
  v.unwritten = fb && unwritten(*fb);
  v.loading = !g_pending.empty();
  EnterCriticalSection(&g_lock);
  g_view = v;
  LeaveCriticalSection(&g_lock);
}

View view() {
  if (!g_lock_ready) return View{};
  EnterCriticalSection(&g_lock);
  const View v = g_view;
  LeaveCriticalSection(&g_lock);
  return v;
}

void apply(const std::vector<Change>& batch) {
  if (!g_lock_ready || batch.empty()) return;
  EnterCriticalSection(&g_lock);
  g_batches.push_back(batch);
  LeaveCriticalSection(&g_lock);
}

const char* file_name(int mode) {
  const FileBox* f = file_box(mode);
  return f ? f->file : nullptr;
}

}  // namespace re5cc::itembox
