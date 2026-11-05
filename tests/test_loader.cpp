// Offline test of what puts an item in a case during play (src/inventory.cpp and
// src/itembox.cpp, built in): what the area has loaded, the game's loader asked
// for what it has not, and the changes that wait for it - over a made-up game: an
// sLoadMan with its lists and their entries (the real offsets), its request and
// commit (stand-ins that do what the game's do to the lists), a character's case,
// cSystemInfo's item box and cGameInfo's saved cases. The discovery is not run:
// its results are set as the dump gives them (checked there by tools/sig.py and the
// operand reads). Build (no object of the mod's linked):
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Isrc -static tests/test_loader.cpp
//       -o tests/build/test_loader.exe
//   wine tests/build/test_loader.exe
//
// - an item whose model's archive is loaded is here; one with a model class but no
//   archive loaded can be loaded; one with no model class never;
// - load asks the game's request for the model's class (and the model's own for a
//   grenade launcher's rounds), commits once, and waits for what was new - into the
//   area's sets' list whichever list the game chose last, which it puts back; nothing is
//   asked while any list holds an entry not asked for again (a commit would let it go);
// - after a cutscene (its list let go, yet the list chosen last) the items go into the
//   sets' list, none into the cutscenes', so the play flow still finds no cutscene
//   preloaded and loads its next one first (the user's 5-3 crash, 2026-10-31);
// - an Add of what is not loaded waits, is made once its archives are in, and is
//   dropped when the pause closes first;
// - a Take of what is not loaded waits with its batch, the next batch waits its
//   turn, both are made once the archives are in; a Take is dropped when the pause
//   closes first; with nothing loaded in time the batch is made without it;
// - at a pause what the box holds is preloaded (once a pause, in one commit, under a
//   cap on the item archives loaded), so its Take is made at once; an Add list's
//   pick is preloaded; the item list's leftovers (no model class) are neither
//   carried nor loaded;
// - an archive by its path (the menus', for Chapter Select): loaded only with its
//   archive in, not while asked for, failed, or a path it only begins.
//
// And the item box's treasures (src/itembox.cpp), over made-up lists of them - each
// character's in sPlayer, each one's saved in cGameInfo, cSystemInfo's and its
// checkpoint copy (the real offsets) - and a made-up Item Management screen:
// - at the pause the view lists the kinds this machine's character holds (an entry
//   at 0, the mark of one found and sold, holds none); a count set, a treasure
//   thrown away and one added change every list by as much (a list behind the one
//   shown keeps its difference), an entry at 0 kept only while no other holds the
//   treasure, an added one filling that mark first and held at the most an entry
//   holds; nothing while the game is not paused;
// - beside Item Management only cSystemInfo's list changes, and the screen
//   rebuilds its treasures list (only) for each pane in use, which steps back to
//   show it afresh; at the pause a change is made in every saved copy of the save data
//   (the working block, its checkpoint copy, the play flow's copy) too.
//
// And the moves between a case and the box at the pause in every saved copy of the case
// the game makes it again from - the save data's saved state, its working block and that
// one's checkpoint copy, the play flow's copy of the saved state and the working block
// (stand-ins for menus.cpp's): the user's play of 2026-10-31 (a Take of the rocket
// launcher, a Store of the Dragunov, then RESTART: the Dragunov in the case and the box,
// the rocket launcher in neither) played through RESTART, a death's CONTINUE and QUIT
// without saving - each item once; a stack stored lessens each copy's by as much; the
// copy that had the stored item in hand puts the one before in hand; a grenade launcher
// (the case's item has the launcher's id) goes in the box as its load - its model's rounds,
// as the game's own save writes it - and from the copies that keep another load; one whose
// load cannot be read, and a scene's own weapon (the Lantern), are not stored: the box
// takes only what the Add lists offer (said so, the case and copies kept); a copy full when saved
// gives up a stack the case holds less of now, and with none such the Take is refused
// (nothing changed, said so); nothing moved while Chapter Select goes or without the
// copies understood.
#include "../src/inventory.cpp"
#include "../src/itembox.cpp"

#include <cstdarg>
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

// --- the made-up game ------------------------------------------------------------------------
namespace {
using namespace re5cc;

// sLoadMan's lists, the field naming the list chosen last (which the game's request and
// commit work on): the characters' (chosen last as the test starts), the area's sets'
// (where the mod asks) and the cutscenes'.
constexpr uint32_t kLists = 0x20708, kCurrent = 0x207D0;
constexpr int kCur = 3, kSets = 4, kCutscenes = 7;
std::vector<uint8_t> g_lm(0x21000);
uintptr_t lm() { return reinterpret_cast<uintptr_t>(g_lm.data()); }
uintptr_t g_list_items[10][256];

struct Entry {
  uint8_t b[0x54] = {};
};
struct Archive {
  uint8_t b[0x60] = {};
};
std::vector<Entry*> g_entries;

int count_of(int k) { return *reinterpret_cast<int*>(lm() + kLists + 4 + k * 0x14); }
void set_count(int k, int n) { *reinterpret_cast<int*>(lm() + kLists + 4 + k * 0x14) = n; }
uint32_t& state_of(Entry* e) { return *reinterpret_cast<uint32_t*>(e->b + 0x48); }
Archive*& archive_of(Entry* e) { return *reinterpret_cast<Archive**>(e->b + 0x44); }
uint32_t& flags_of(Archive* a) { return *reinterpret_cast<uint32_t*>(a->b + 0x4C); }

Entry* find_entry(int k, const std::string& name) {
  for (int i = 0; i < count_of(k); ++i) {
    auto* e = reinterpret_cast<Entry*>(g_list_items[k][i]);
    if (std::string(reinterpret_cast<const char*>(e->b + 4)) == "Image\\Archive\\" + name) return e;
  }
  return nullptr;
}

Entry* add_entry(int k, const std::string& name, uint32_t state, bool loaded) {
  auto* e = new Entry;
  std::snprintf(reinterpret_cast<char*>(e->b + 4), 0x40, "Image\\Archive\\%s", name.c_str());
  state_of(e) = state;
  if (loaded) {
    archive_of(e) = new Archive;
    flags_of(archive_of(e)) = 1;
  }
  const int n = count_of(k);
  g_list_items[k][n] = reinterpret_cast<uintptr_t>(e);
  set_count(k, n + 1);
  g_entries.push_back(e);
  return e;
}

// The loader's frame, one step: asked (1) -> loading (2, the archive made) -> loaded (3).
void advance() {
  for (Entry* e : g_entries) {
    if (state_of(e) == 1) {
      state_of(e) = 2;
      archive_of(e) = new Archive;
    } else if (state_of(e) == 2) {
      state_of(e) = 3;
      flags_of(archive_of(e)) |= 1;
    }
  }
}

// The game's classes: an item's model's class by its key, (7 << 16 | item) << 8.
std::vector<mt::Class> g_classes;
uint32_t key(int id) { return ((7u << 16) | static_cast<uint32_t>(id)) << 8; }
const char* class_name(int id) {
  for (const mt::Class& c : g_classes)
    if (c.id == key(id)) return c.name;
  return nullptr;
}

// The request, into the list chosen last: an entry for the class (unless the list has
// it - one not asked for again is asked again; into the sets' list, unless the
// characters' has it, FUN_007D4960), and what the class needs (a gun's ammo, the rounds).
struct Asked {
  int kind, id, list;
};
std::vector<Asked> g_requests;
std::vector<int> g_committed;  // the list of each commit
int g_commits = 0;
int current() { return static_cast<int>(*reinterpret_cast<uint32_t*>(lm() + kCurrent)); }
void __thiscall fake_request(void* self, int kind, int id, int, int) {
  const int cur = current();
  g_requests.push_back(Asked{kind, id, cur});
  auto ask = [&](int what) {
    const char* name = class_name(what);
    if (!name) return;
    Entry* e = cur == kSets ? find_entry(kCur, name) : nullptr;
    if (!e) e = find_entry(cur, name);
    if (e) {
      if (state_of(e) == 0) state_of(e) = 3;
      return;
    }
    add_entry(cur, name, 1, false);
  };
  (void)self;
  ask(id);
  if (id == 0x113) ask(0x202);
  if (id == 0x125) ask(0x206);
}
int __thiscall fake_commit(void*) {
  ++g_commits;
  const int cur = current();
  g_committed.push_back(cur);
  int n = 0;
  for (int i = 0; i < count_of(cur); ++i) n += state_of(reinterpret_cast<Entry*>(g_list_items[cur][i])) == 1;
  return n;
}
// The play flow's test whether a cutscene is preloaded (FUN_007D51D0): the entries of
// the cutscenes' list loaded.
int cutscene_preloaded() {
  int n = 0;
  for (int i = 0; i < count_of(kCutscenes); ++i)
    n += state_of(reinterpret_cast<Entry*>(g_list_items[kCutscenes][i])) == 3;
  return n;
}

// A character and its case (the real layout's offsets).
std::vector<uint8_t> g_chara(0x3000);
uintptr_t chara() { return reinterpret_cast<uintptr_t>(g_chara.data()); }
constexpr uint32_t kMgr = 0x21A0;
uintptr_t item_at(int i) { return chara() + kMgr + 4 + static_cast<uint32_t>(i) * 0x30; }
std::vector<int> g_adds;  // the ids the inventory's add was asked for
uintptr_t __thiscall fake_add(void*, uint32_t id, int num, int, int, int, int, int, int, int, int, int, int, int, int,
                              int, void*, uint32_t, uint32_t) {
  g_adds.push_back(static_cast<int>(id));
  bool used[9] = {};
  for (int i = 0; i < 24; ++i)
    if (mem::read<uint8_t>(item_at(i) + 0x29)) {
      const int s = static_cast<int>(mem::read<uint32_t>(item_at(i) + 0x18));
      if (s >= 0 && s < 9) used[s] = true;
    }
  for (int i = 0; i < 24; ++i) {
    if (mem::read<uint8_t>(item_at(i) + 0x29)) continue;
    int slot = 0;
    while (slot < 9 && used[slot]) ++slot;
    if (slot == 9) return 0;
    mem::write<uint32_t>(item_at(i) + 0x04, id);
    mem::write<uint32_t>(item_at(i) + 0x08, static_cast<uint32_t>(num < 0 ? 45 : num));
    mem::write<uint32_t>(item_at(i) + 0x0C, 45);
    mem::write<uint32_t>(item_at(i) + 0x18, static_cast<uint32_t>(slot));
    mem::write<uint32_t>(item_at(i) + 0x1C, 0);
    mem::write<uint8_t>(item_at(i) + 0x29, 1);
    return item_at(i);
  }
  return 0;
}
void __thiscall fake_clear(void* item, int) { mem::write<uint8_t>(reinterpret_cast<uintptr_t>(item) + 0x29, 0); }

std::vector<uint8_t> g_sys(0x10000), g_info(0x8000), g_splayer(0x1000);
game::Layout g_layout;
// A grenade launcher's model in a case (a uWpLauncher: its vtable, the rounds loaded at +0x1C80).
constexpr uintptr_t kLauncherVtable = 0x7778;
std::vector<uint8_t> g_launcher_model(0x1CA0);
uintptr_t launcher_model(uint32_t rounds) {
  const uintptr_t m = reinterpret_cast<uintptr_t>(g_launcher_model.data());
  mem::write<uintptr_t>(m, kLauncherVtable);
  mem::write<uint32_t>(m + 0x1C80, rounds);
  return m;
}
uint8_t g_switch_bytes[0x30];
uintptr_t g_switch_jumps[2] = {0xAAAA, 0xBBBB};  // 0: another's model (0x10C's); 1: the default (its own)

int g_run_mode = game::kModeStory;  // the game's mode the made-up runs are in

game::Run run(bool paused) {
  game::Run r;
  r.in_game = true;
  r.mode = g_run_mode;
  r.screen = paused ? game::Screen::kPause : game::Screen::kPlay;
  r.nchars = 1;
  r.chars[0].obj = chara();
  r.chars[0].index = 0;
  r.chars[0].name = "Chris";
  return r;
}

}  // namespace

// --- what the two modules ask of the rest of the mod, made up --------------------------------
namespace re5cc::game {
std::vector<uintptr_t> sig_all(const char*) { return {}; }
uintptr_t sig_one(const char*) { return 0; }
uintptr_t call_target(uintptr_t) { return 0; }
bool ready() { return true; }
uintptr_t system(const char* name) {
  if (std::strcmp(name, "sLoadMan") == 0) return lm();
  if (std::strcmp(name, "sPlayer") == 0) return reinterpret_cast<uintptr_t>(g_splayer.data());
  return 0;
}
const Layout& layout() { return g_layout; }
uintptr_t system_info() { return reinterpret_cast<uintptr_t>(g_sys.data()); }
uintptr_t game_info() { return reinterpret_cast<uintptr_t>(g_info.data()); }
bool item_in(uintptr_t c, int slot, Item* out) {
  const Layout& l = g_layout;
  for (int i = 0; i < l.nitems; ++i) {
    const uintptr_t it = c + l.case_mgr + l.items + static_cast<uint32_t>(i) * l.item_size;
    if (!mem::read<uint8_t>(it + l.it_alive) || static_cast<int>(mem::read<uint32_t>(it + l.it_slot)) != slot) continue;
    out->at = it;
    out->id = static_cast<int>(mem::read<uint32_t>(it + l.it_id));
    out->num = static_cast<int>(mem::read<uint32_t>(it + l.it_num));
    out->most = static_cast<int>(mem::read<uint32_t>(it + l.it_most));
    out->slot = slot;
    out->equip = static_cast<int>(mem::read<uint32_t>(it + l.it_equip));
    for (int k = 0; k < 9; ++k) out->lv[k] = mem::read<uint8_t>(it + l.it_lv[k]);
    return true;
  }
  return false;
}
}  // namespace re5cc::game
namespace re5cc::mt {
const Class* find(const char*) { return nullptr; }
const Class* by_id(uint32_t id) {
  for (const Class& c : g_classes)
    if (c.id == id) return &c;
  return nullptr;
}
bool derives(uintptr_t, uintptr_t) { return true; }
// An object's class, as the made-up models have it: their first word, the class's vtable.
bool is_a(uintptr_t obj, const Class* cls) { return cls && obj && mem::read<uintptr_t>(obj) == cls->vtable; }
}  // namespace re5cc::mt
// The folder beside the mod (the extra episodes' boxes' files): a scratch one of the test's own.
namespace {
std::string g_dir;
}  // namespace
namespace re5cc::config {
const char* dir() { return g_dir.c_str(); }
}  // namespace re5cc::config
namespace re5cc::image {
uintptr_t base() { return 0x400000; }
mem::Range text() { return {}; }
mem::Range rdata() { return {}; }
}  // namespace re5cc::image
// menus.cpp's: Chapter Select on its way, and the saved copies of the save data - in the
// profile's cGameInfo and the play flow's copy of it (the real offsets).
namespace {
std::vector<uint8_t> g_snap(0x5940);  // the play flow's copy of cGameInfo
bool g_leaving = false, g_saves_known = true, g_snap_on = true;
constexpr uint32_t kState = 0x100, kWork = 0x1690, kCopy = 0x42D0, kBlockSize = 0x1590;
}  // namespace
namespace re5cc::menus {
bool leaving() { return g_leaving; }
bool saves(Saves* out) {
  if (!g_saves_known) return false;
  *out = Saves{kState, kWork, kCopy, kBlockSize, g_snap_on ? reinterpret_cast<uintptr_t>(g_snap.data()) : 0};
  return true;
}
}  // namespace re5cc::menus

// --- the checks ------------------------------------------------------------------------------
namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

void setup() {
  // The game as the dump has it (discovery's results).
  g_layout.case_mgr = kMgr;
  g_layout.items = 4;
  g_layout.nitems = 24;
  g_layout.item_size = 0x30;
  g_layout.it_id = 4;
  g_layout.it_num = 8;
  g_layout.it_most = 0xC;
  g_layout.it_slot = 0x18;
  g_layout.it_equip = 0x1C;
  for (int k = 0; k < 9; ++k) g_layout.it_lv[k] = 0x20 + static_cast<uint32_t>(k);
  g_layout.it_alive = 0x29;
  g_layout.it_model = 0x2C;
  g_layout.checkpoint = 0x4638;
  g_classes.reserve(64);  // the modules keep pointers to these
  for (const auto& [id, name] : std::vector<std::pair<int, const char*>>{
           {0x102, "uWp02"}, {0x10C, "uWp0c"}, {0x113, "uWp13"}, {0x125, "uWp25"},
           {0x201, "uIt0201"}, {0x202, "uIt0202"}, {0x206, "uIt0206"}}) {
    mt::Class c;
    c.dti = 0x1000 + static_cast<uintptr_t>(id);
    c.name = name;
    c.id = key(id);
    g_classes.push_back(c);
  }
  static mt::Class uitem;
  uitem.dti = 0x9999;
  uitem.name = "uItem";
  namespace inv = inventory;
  inv::g_ld = inv::Loader{0x2070C, 0x20718, 0x14, 10, kCurrent, 4, 0x44, 0x4C, 0x48, kSets, kCutscenes};
  inv::g_model_lo = 0x10C;
  inv::g_model_n = 0x30;
  for (uint8_t& b : g_switch_bytes) b = 1;
  for (const int id : {0x10C, 0x125, 0x126, 0x127, 0x139, 0x13A, 0x13B}) g_switch_bytes[id - 0x10C] = 0;
  inv::g_model_bytes = reinterpret_cast<uintptr_t>(g_switch_bytes);
  inv::g_model_jumps = reinterpret_cast<uintptr_t>(g_switch_jumps);
  inv::g_model_default = g_switch_jumps[1];
  inv::g_model_as = 0x10C;
  inv::g_model_kind = 7;
  inv::g_uitem = &uitem;
  // The game's save of a case, its grenade launcher (kSigSaveLauncher, as the dump has it):
  // the rounds a uWpLauncher model keeps at +0x1C80, 0x140 on, into the loads EXP, ACD, ICE,
  // then (0x14D on) FLM, FLS, ELC.
  static mt::Class launcher;
  launcher.dti = 0x7770;
  launcher.vtable = kLauncherVtable;
  launcher.name = "uWpLauncher";
  launcher.size = 0x1CA0;
  inv::g_launcher_cls = &launcher;
  inv::g_launcher = 0x10C;
  inv::g_rounds = 0x1C80;
  inv::g_rounds_lo = 0x140;
  inv::g_rounds_n = 16;
  const uint32_t loads[16] = {0x125, 0x126, 0x127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x139, 0x13A, 0x13B};
  std::copy(loads, loads + 16, inv::g_load_of);
  inv::g_area_ok = inv::g_load_ok = true;
  inv::g_archive_dir = "Image\\Archive\\";
  inv::g_request = fake_request;
  inv::g_commit = fake_commit;
  inv::g_add = reinterpret_cast<inv::AddFn>(fake_add);
  inv::g_clear = fake_clear;
  inv::g_hand_field = 0x262C;
  for (int k = 0; k < items::kCount; ++k) inv::g_item_model[k] = inv::model_class(items::kItems[k].id);
  inventory::init();
  // sLoadMan: its ten lists, the fourth the one added to: the M92F and its ammo loaded.
  for (int k = 0; k < 10; ++k) *reinterpret_cast<uintptr_t*>(lm() + kLists + 0x10 + k * 0x14) =
                                   reinterpret_cast<uintptr_t>(g_list_items[k]);
  *reinterpret_cast<uint32_t*>(lm() + kCurrent) = kCur;
  add_entry(kCur, "uWp02", 3, true);
  add_entry(kCur, "uIt0201", 3, true);
  add_entry(0, "uSomethingElse", 1, false);  // another list's, asked and never loaded
  // The item box (cSystemInfo's), as discovery finds it.
  namespace box = itembox;
  box::g_found = true;
  box::g_box = 0x3840;
  box::g_entry = 12;
  box::g_count = 54;
  box::g_never = 0x100;
  box::g_a_lo = 0x106;
  box::g_a_hi = 0x108;
  box::g_b = 0x10A;
  box::g_c = 0x136;
  box::g_d_lo = 0x13C;
  box::g_d_hi = 0x13E;
  box::g_e_lo = 0x201;
  box::g_e_hi = 0x47E;
  box::g_most = 999;
  box::g_drop = 0x400;
  box::g_keep1 = 0x601;
  box::g_keep2 = 0x606;
  const int lvmap[7] = {0, 2, 3, 5, 6, 7, 8};
  for (int k = 0; k < 7; ++k) box::g_lvmap[k] = lvmap[k];
  box::g_saved = 0x240;
  box::g_saved_item = 0x2C;
  box::g_saved_stride = 24 * 0x2C;
  box::g_saved_counts = 0x240 + 4 * 24 * 0x2C;
  // The treasures' lists.
  box::g_tfound = true;
  box::g_tlist = 0x3AC8;
  box::g_tcount = 54;
  box::g_tmost = 999;
  box::g_tplay = 0x260;
  box::g_tsaved = 0x1330;
  box::g_tstride = 0xD8;
  itembox::init();
}

void put_in_box(int index, int id, int count) {
  const uintptr_t at = reinterpret_cast<uintptr_t>(g_sys.data()) + 0x3840 + static_cast<uint32_t>(index) * 12;
  mem::write<uint16_t>(at, static_cast<uint16_t>(id));
  mem::write<uint16_t>(at + 2, static_cast<uint16_t>(count));
}
int box_id(int index) {
  return mem::read<uint16_t>(reinterpret_cast<uintptr_t>(g_sys.data()) + 0x3840 + static_cast<uint32_t>(index) * 12);
}

void tick(bool paused) {
  const game::Run r = run(paused);
  inventory::tick(r);
  itembox::tick(r);
}

void check_area() {
  tick(true);
  check(inventory::avail(0x102) == inventory::kHere && inventory::avail(0x201) == inventory::kHere,
        "area: the M92F and its ammo are here (their archives loaded)");
  check(inventory::avail(0x113) == inventory::kLoadable, "area: the MP5 can be loaded (a model class, no archive)");
  check(inventory::avail(0x20B) == inventory::kNever, "area: the arrows never (no model class)");
  const inventory::View v = inventory::view();
  int k102 = -1, k113 = -1;
  for (int k = 0; k < items::kCount; ++k) {
    if (items::kItems[k].id == 0x102) k102 = k;
    if (items::kItems[k].id == 0x113) k113 = k;
  }
  check(v.avail[k102] == inventory::kHere && v.avail[k113] == inventory::kLoadable, "area: the view says so too");
}

// The front end's archives in the title's list (the menus', for menus.cpp): known by
// their path as asked for, loaded only once their archive is in.
void check_archives() {
  add_entry(1, "MenuResource", 3, true);
  add_entry(1, "TitleResource", 1, false);  // asked for, not loading yet
  Entry* shop = add_entry(1, "ShopResource", 3, true);
  flags_of(archive_of(shop)) = 0x11;  // failed
  check(inventory::lists_known() && inventory::archive_dir() == "Image\\Archive\\" &&
            inventory::archive_loaded("Image\\Archive\\MenuResource"),
        "archives: the menus' archive loaded, found by its path in another list");
  check(!inventory::archive_loaded("Image\\Archive\\TitleResource") &&
            !inventory::archive_loaded("Image\\Archive\\ShopResource") &&
            !inventory::archive_loaded("Image\\Archive\\Menu") &&
            !inventory::archive_loaded("Image\\Archive\\Menu2Resource"),
        "archives: not one asked for and not in yet, one failed, a path it only begins, one not asked for");
  set_count(1, 0);
}

void check_load() {
  tick(true);
  // The list holds an entry not asked for again: nothing is asked (a commit would let it go).
  Entry* again = add_entry(kCur, "uSomeArea", 0, true);
  g_requests.clear();
  g_commits = 0;
  check(!inventory::load(0x113) && g_requests.empty() && g_commits == 0,
        "load: nothing asked while the game chooses an area's archives (an entry in state 0)");
  state_of(again) = 3;
  // The grenade launcher's rounds: its own class and the launcher's model.
  g_committed.clear();
  check(inventory::load(0x125) && g_requests.size() == 2 && g_requests[0].id == 0x125 && g_requests[1].id == 0x10C &&
            g_requests[0].kind == 7 && g_commits == 1,
        "load: G. launcher (EXP) asks for its class and the launcher's model (kind 7), commits once");
  check(g_requests[0].list == kSets && g_requests[1].list == kSets && g_committed == std::vector<int>{kSets} &&
            current() == kCur,
        "load: ... into the area's sets' list, committed there; the list the game chose last put back");
  check(!inventory::ready(0x125), "load: ... not ready while its archives load");
  for (int i = 0; i < 2; ++i) advance();
  inventory::refresh_area();
  check(inventory::ready(0x125) && inventory::avail(0x125) == inventory::kHere, "load: ... ready once they are in");
}

// The user's crash (chapter 5-3, 2026-10-31): a cutscene played lets the cutscenes' list go
// whole, and it stays the list chosen last; items asked into it at the next pause made the
// play flow take them for its next cutscene, preloaded, and play that one unloaded.
void check_cutscenes() {
  tick(false);
  g_classes.push_back(mt::Class{0x2006, 0, "uWp16", 0, 0, key(0x116)});
  *reinterpret_cast<uint32_t*>(lm() + kCurrent) = kCutscenes;  // a cutscene was loaded ...
  set_count(kCutscenes, 0);                                     // ... played, and its list let go
  g_requests.clear();
  g_committed.clear();
  inventory::load(0x116);
  check(g_requests.size() == 1 && g_requests[0].list == kSets && find_entry(kSets, "uWp16") &&
            count_of(kCutscenes) == 0 && g_committed == std::vector<int>{kSets} && current() == kCutscenes,
        "after a cutscene: asked into the area's sets' list and committed there, nothing into the cutscenes' list, "
        "the list chosen last (the cutscenes') put back");
  advance();
  advance();
  inventory::refresh_area();
  check(cutscene_preloaded() == 0 && inventory::ready(0x116),
        "after a cutscene: ... loaded, and the play flow still finds no cutscene preloaded (it loads the next first)");
  // The game choosing a list's archives (an entry not asked for again in the characters',
  // not the list chosen last): nothing is asked, whichever list that is.
  Entry* choosing = add_entry(kCur, "uSomeChara", 0, true);
  g_classes.push_back(mt::Class{0x2007, 0, "uWp17", 0, 0, key(0x117)});
  g_requests.clear();
  g_commits = 0;
  check(!inventory::load(0x117) && g_requests.empty() && g_commits == 0,
        "load: nothing asked while another list is being chosen (an entry in state 0 there)");
  state_of(choosing) = 3;
  set_count(kCutscenes, 0);
  *reinterpret_cast<uint32_t*>(lm() + kCurrent) = kCur;
}

void check_add_waits() {
  inventory::add(0, 0x113, 1);
  g_adds.clear();
  tick(true);
  inventory::View v = inventory::view();
  check(g_adds.empty() && v.loading && std::strstr(v.note, "Loading") != nullptr,
        "add: the MP5 waits for the loader (nothing added, Loading...)");
  check(find_entry(kSets, "uWp13") && find_entry(kSets, "uIt0202"),
        "add: ... its class and its ammo's asked for (into the area's sets' list)");
  tick(true);
  check(g_adds.empty(), "add: ... still waiting while they load");
  advance();
  tick(true);
  check(g_adds.empty(), "add: ... loading");
  advance();
  tick(true);
  v = inventory::view();
  check(g_adds.size() == 1 && g_adds[0] == 0x113 && !v.loading, "add: ... added once its archives are in");
  // Another, then the pause closes before it loads: dropped.
  g_classes.push_back(mt::Class{0x2000, 0, "uWp1b", 0, 0, key(0x11B)});
  inventory::add(0, 0x11B, 1);
  g_adds.clear();
  tick(true);
  check(inventory::view().loading, "add: the S&W M500 waits for the loader");
  tick(false);
  v = inventory::view();
  check(g_adds.empty() && !v.loading && std::strstr(v.note, "pause closed") != nullptr,
        "add: ... dropped when the pause closes first");
  advance();
  advance();
}

void check_take_waits() {
  // Empty the case, the MP5's archives let go; the box holds an MP5 and handgun ammo.
  for (int i = 0; i < 24; ++i) mem::write<uint8_t>(item_at(i) + 0x29, 0);
  for (const char* n : {"uWp13", "uIt0202"})
    if (Entry* e = find_entry(kSets, n)) {
      state_of(e) = 3;
      flags_of(archive_of(e)) = 0;  // as if never loaded
      std::snprintf(reinterpret_cast<char*>(e->b + 4), 0x40, "Image\\Archive\\gone");
    }
  put_in_box(3, 0x113, 45);
  put_in_box(4, 0x201, 100);
  tick(true);
  itembox::View v = itembox::view();
  check(v.entry[3].avail == inventory::kLoadable && v.entry[4].avail == inventory::kHere,
        "take: the box's MP5 can be loaded, its handgun ammo is here");
  g_adds.clear();
  g_requests.clear();
  itembox::apply({itembox::Change{itembox::Op::kTake, 3, 0x113, 0, 0}});
  itembox::apply({itembox::Change{itembox::Op::kSet, 4, 0x201, 0}});  // the ammo thrown away, after it
  tick(true);
  v = itembox::view();
  check(g_adds.empty() && v.loading && box_id(3) == 0x113 && box_id(4) == 0x201 && !g_requests.empty(),
        "take: the MP5 waits for the loader with its batch; the next batch waits its turn");
  advance();
  tick(true);
  check(g_adds.empty() && box_id(4) == 0x201, "take: ... still waiting");
  advance();
  tick(true);
  v = itembox::view();
  check(g_adds.size() == 1 && g_adds[0] == 0x113 && box_id(3) == 0 && !v.loading,
        "take: ... taken into Chris's case once its archives are in, the entry emptied");
  tick(true);
  check(box_id(4) == 0, "take: ... then the batch after it made");
  // A Take, then the pause closes before it loads: dropped, the entry kept.
  g_classes.push_back(mt::Class{0x2001, 0, "uWp1c", 0, 0, key(0x11C)});
  put_in_box(6, 0x11C, 5);
  g_adds.clear();
  itembox::apply({itembox::Change{itembox::Op::kTake, 6, 0x11C, 0, 0}});
  tick(true);
  tick(false);
  v = itembox::view();
  check(g_adds.empty() && box_id(6) == 0x11C && !v.loading && std::strstr(v.note, "pause closed") != nullptr,
        "take: dropped when the pause closes first, the entry kept");
  // Not loaded in time: the batch is made without it (the Take refused, nothing made of it).
  advance();
  advance();
  if (Entry* e = find_entry(kSets, "uWp1c")) flags_of(archive_of(e)) = 0;  // the archive never loads
  itembox::apply({itembox::Change{itembox::Op::kTake, 6, 0x11C, 0, 0}});
  for (int i = 0; i < inventory::kLoadTicks + 5; ++i) tick(true);
  v = itembox::view();
  check(g_adds.empty() && box_id(6) == 0x11C && !v.loading && std::strstr(v.note, "could not be loaded") != nullptr,
        "take: not loaded in time - the batch made without it, the entry kept");
}


void check_preload() {
  // A new pause: what the box holds is preloaded, once, and its Take is then made at once.
  tick(false);
  put_in_box(6, 0, 0);  // the earlier check's item that never loads
  g_classes.push_back(mt::Class{0x2002, 0, "uWp1d", 0, 0, key(0x11D)});
  put_in_box(10, 0x11D, 30);
  g_requests.clear();
  g_commits = 0;
  tick(true);
  check(g_requests.size() == 1 && g_requests[0].id == 0x11D && g_commits == 1,
        "preload: the box's AK-74 asked for as the pause opens, in one commit");
  tick(true);
  check(g_requests.size() == 1, "preload: ... once a pause");
  advance();
  advance();
  tick(true);
  check(itembox::view().entry[10].avail == inventory::kHere, "preload: ... loaded while the game stands still");
  g_adds.clear();
  itembox::apply({itembox::Change{itembox::Op::kTake, 10, 0x11D, 0, 0}});
  tick(true);
  check(g_adds.size() == 1 && g_adds[0] == 0x11D && box_id(10) == 0 && !itembox::view().loading,
        "preload: ... and its Take made at once (nothing to wait for)");
  // An Add list's pick: preloaded at the next tick.
  g_classes.push_back(mt::Class{0x2003, 0, "uWp1e", 0, 0, key(0x11E)});
  g_requests.clear();
  inventory::preload_soon(0x11E);
  tick(true);
  check(g_requests.size() == 1 && g_requests[0].id == 0x11E, "preload: an Add list's pick asked for at the next tick");
  advance();
  advance();
  // The cap: with the items' and weapons' archives at it, nothing more is preloaded.
  tick(false);
  std::vector<Entry*> filler;
  for (int i = 0; i < inventory::kItemArchivesMax; ++i)
    filler.push_back(add_entry(5, "uWpF" + std::to_string(i), 3, true));
  g_classes.push_back(mt::Class{0x2004, 0, "uWp1f", 0, 0, key(0x11F)});
  put_in_box(11, 0x11F, 30);
  g_requests.clear();
  tick(true);
  check(g_requests.empty(), "preload: nothing past the cap on loaded item archives (left for when it is taken)");
  g_adds.clear();
  itembox::apply({itembox::Change{itembox::Op::kTake, 11, 0x11F, 0, 0}});
  tick(true);
  check(!g_requests.empty() && itembox::view().loading, "preload: ... its Take still loads it first");
  advance();
  advance();
  tick(true);
  check(g_adds.size() == 1 && g_adds[0] == 0x11F, "preload: ... and takes it");
  set_count(5, 0);
  // The item list's leftovers: no model class - not carried, never loaded.
  int k20b = -1, k113 = -1;
  for (int k = 0; k < items::kCount; ++k) {
    if (items::kItems[k].id == 0x20B) k20b = k;
    if (items::kItems[k].id == 0x113) k113 = k;
  }
  check(!inventory::carried(k20b) && inventory::carried(k113), "leftovers: the arrows are not carried, the MP5 is");
  g_requests.clear();
  check(!inventory::load(0x20B) && g_requests.empty(), "leftovers: the arrows cannot be loaded (nothing asked)");
}

// The two weapons the game keeps for one character each (Item Management's own check,
// items.h's `only`): the Longbow is Sheva's - Chris's Add of one and a Take of one into his
// case are refused (the entry kept), a Take into hers is made. What the Add lists do not
// offer (a knife) goes into neither a case nor the box.
game::Run run_two(bool paused);
void check_theirs() {
  tick(false);
  g_classes.push_back(mt::Class{0x2005, 0, "uWp0f", 0, 0, key(0x10F)});
  put_in_box(12, 0x10F, 1);
  tick(true);  // the pause opens: the box's Longbow preloaded
  advance();
  advance();
  tick(true);
  check(itembox::view().entry[12].avail == inventory::kHere, "theirs: the box's Longbow loaded here");
  g_adds.clear();
  inventory::add(0, 0x10F, 1);
  tick(true);
  check(g_adds.empty() && std::strstr(inventory::view().note, "Only Sheva can have the Longbow") != nullptr,
        "theirs: Chris's Add of the Longbow refused - only Sheva can have it");
  itembox::apply({itembox::Change{itembox::Op::kTake, 12, 0x10F, 0, 0}});
  tick(true);
  check(g_adds.empty() && box_id(12) == 0x10F &&
            std::strstr(itembox::view().note, "the game keeps it for the other character") != nullptr,
        "theirs: ... and its Take into his case, the entry kept");
  itembox::apply({itembox::Change{itembox::Op::kTake, 12, 0x10F, 0, 1}});
  const game::Run two = run_two(true);
  inventory::tick(two);
  itembox::tick(two);
  check(g_adds.size() == 1 && g_adds[0] == 0x10F && box_id(12) == 0, "theirs: ... taken into Sheva's case");
  // A knife (each character's own, never added): offered by no Add list, made by neither -
  // its model loaded here, so the rule alone stops it.
  g_classes.push_back(mt::Class{0x2006, 0, "uWp23", 0, 0, key(0x123)});
  add_entry(kSets, "uWp23", 3, true);
  inventory::refresh_area();
  check(inventory::in_area(0x123), "theirs: a knife's model loaded here");
  g_adds.clear();
  inventory::add(0, 0x123, 1);
  itembox::apply({itembox::Change{itembox::Op::kAdd, -1, 0x123, 1}});
  tick(true);
  bool boxed = false;
  for (int i = 0; i < 54; ++i) boxed = boxed || box_id(i) == 0x123;
  check(g_adds.empty() && !boxed, "theirs: a knife is added to neither a case nor the box (no Add list offers it)");
}

// --- the treasures -------------------------------------------------------------------------------
// A character's saved list in the other saved copies: the working block's, its copy's, the
// play flow's copy's saved state and working block.
std::vector<uintptr_t> copy_lists(int c) {
  const uintptr_t gi = reinterpret_cast<uintptr_t>(g_info.data()), sn = reinterpret_cast<uintptr_t>(g_snap.data());
  const uint32_t off = 0x1330 - kState + static_cast<uint32_t>(c) * 0xD8;
  return {gi + kWork + off, gi + kCopy + off, sn + kState + off, sn + kWork + off};
}
// Chris's (0) and Sheva's (1) lists in play, their saved ones, cSystemInfo's and its copy.
uintptr_t live_list(int c) { return reinterpret_cast<uintptr_t>(g_splayer.data()) + 0x260 + c * 0xD8; }
uintptr_t saved_list(int c) { return reinterpret_cast<uintptr_t>(g_info.data()) + 0x1330 + c * 0xD8; }
uintptr_t profile_list(bool copy) { return reinterpret_cast<uintptr_t>(g_sys.data()) + 0x3AC8 + (copy ? 0x4638 : 0); }
uintptr_t all_lists(int k) {
  return k < 2 ? live_list(k) : k < 4 ? saved_list(k - 2) : profile_list(k == 5);
}
void put_treasure(uintptr_t list, int i, int id, int n) {
  mem::write<uint16_t>(list + 4 * i, static_cast<uint16_t>(id));
  mem::write<uint16_t>(list + 4 * i + 2, static_cast<uint16_t>(n));
}
int entry_id(uintptr_t list, int i) { return mem::read<uint16_t>(list + 4 * i); }
int entry_count(uintptr_t list, int i) { return mem::read<uint16_t>(list + 4 * i + 2); }
int held(uintptr_t list, int id) {
  int n = 0;
  for (int i = 0; i < 54; ++i) n += entry_id(list, i) == id ? entry_count(list, i) : 0;
  return n;
}
int entries_of(uintptr_t list, int id) {
  int n = 0;
  for (int i = 0; i < 54; ++i) n += entry_id(list, i) == id;
  return n;
}

// Sheva (the AI) first in the game's list, Chris (this machine's) after her.
game::Run run_two(bool paused) {
  game::Run r = run(paused);
  r.nchars = 2;
  r.chars[1] = r.chars[0];
  r.chars[0].index = 1;
  r.chars[0].name = "Sheva";
  r.chars[0].control = game::Control::kAi;
  return r;
}

const itembox::Treasure* kind_of(const itembox::View& v, int id) {
  for (int k = 0; k < v.tkinds; ++k)
    if (v.treasure[k].id == id) return &v.treasure[k];
  return nullptr;
}

// Item Management's screen: two panes, the first browsing the box (state 5, step 1),
// the second not in use; its rebuilds of a pane's lists, counted.
std::vector<uint8_t> g_screen(0x5B88);
int g_box_rebuilds = 0, g_treasure_rebuilds = 0;
uint32_t __thiscall fake_box_rebuild(void*, void*) { return ++g_box_rebuilds; }
uint32_t __thiscall fake_treasure_rebuild(void*, void*) { return ++g_treasure_rebuilds; }

void check_treasures() {
  constexpr int kRing = 0x417, kGem = 0x46F, kRelief = 0x420, kFang = 0x419, kHeart = 0x470;
  // In play: the gold ring and the soul gem saved at the checkpoint, the ivory relief
  // found and sold (its entry at 0); the venom fang and a second ring picked up since.
  // Sheva's list has a soul gem more than Chris's.
  for (int c = 0; c < 2; ++c) {
    put_treasure(live_list(c), 0, kRing, 2);
    put_treasure(live_list(c), 1, kGem, 1 + c);
    put_treasure(live_list(c), 2, kRelief, 0);
    put_treasure(live_list(c), 3, kFang, 1);
  }
  for (int k = 2; k < 6; ++k) {
    put_treasure(all_lists(k), 0, kRing, 1);
    put_treasure(all_lists(k), 1, kGem, 1);
    put_treasure(all_lists(k), 2, kRelief, 0);
  }
  auto tick_two = [](bool paused) {
    const game::Run r = run_two(paused);
    inventory::tick(r);
    itembox::tick(r);
  };
  tick_two(true);
  itembox::View v = itembox::view();
  check(v.treasures && v.tmost == 999 && v.tkinds == 3 && kind_of(v, kRing) && kind_of(v, kRing)->count == 2 &&
            kind_of(v, kGem) && kind_of(v, kGem)->count == 1 && kind_of(v, kFang) && !kind_of(v, kRelief),
        "treasures: the view lists the kinds Chris holds (this machine's), not the one sold (its entry at 0)");
  itembox::apply({itembox::Change{itembox::Op::kTreasureSet, -1, kRing, 5}});
  tick_two(true);
  bool ok = held(live_list(0), kRing) == 5 && held(live_list(1), kRing) == 5;
  for (int k = 2; k < 6; ++k) ok = ok && held(all_lists(k), kRing) == 4;
  check(ok, "treasures: a count set (the ring 2 -> 5) in both characters' lists, the saved ones and the profile's "
            "(and its copy) given as many more (1 -> 4)");
  ok = true;
  for (int c = 0; c < 2; ++c)
    for (const uintptr_t l : copy_lists(c)) ok = ok && held(l, kRing) == 3;
  check(ok, "treasures: ... and each saved copy's (the working block, its copy, the play flow's) by as many (0 -> 3)");
  itembox::apply({itembox::Change{itembox::Op::kTreasureSet, -1, kFang, 0}});
  tick_two(true);
  ok = true;
  for (int c = 0; c < 2; ++c) ok = ok && entry_id(live_list(c), 3) == kFang && entry_count(live_list(c), 3) == 0;
  for (int k = 2; k < 6; ++k) ok = ok && entries_of(all_lists(k), kFang) == 0;
  check(ok && !kind_of(itembox::view(), kFang),
        "treasures: thrown away (the venom fang) - its only entry kept at 0, the mark of one found; the saved lists, "
        "which never had it, left alone");
  itembox::apply({itembox::Change{itembox::Op::kTreasureAdd, -1, kRelief, 2}});
  tick_two(true);
  ok = true;
  for (int k = 0; k < 6; ++k) ok = ok && entry_id(all_lists(k), 2) == kRelief && entry_count(all_lists(k), 2) == 2 &&
                                   entries_of(all_lists(k), kRelief) == 1;
  check(ok, "treasures: an add (2 ivory reliefs) fills the mark of one found first, in every list");
  itembox::apply({itembox::Change{itembox::Op::kTreasureAdd, -1, kHeart, 3}});
  tick_two(true);
  ok = entry_id(live_list(0), 4) == kHeart && entry_id(saved_list(0), 3) == kHeart;
  for (int k = 0; k < 6; ++k) ok = ok && held(all_lists(k), kHeart) == 3;
  check(ok, "treasures: a new kind (3 Hearts of Africa) takes each list's first empty entry");
  itembox::apply({itembox::Change{itembox::Op::kTreasureAdd, -1, kRing, 2000}});
  tick_two(true);
  ok = held(live_list(0), kRing) == 999 && held(live_list(1), kRing) == 999;
  for (int k = 2; k < 6; ++k) ok = ok && held(all_lists(k), kRing) == 998;
  check(ok && std::strstr(itembox::view().note, "held at 999") != nullptr,
        "treasures: an add held at the most an entry holds (999), the others given as many (994), and said so");
  put_treasure(live_list(0), 6, kGem, 0);  // a second entry of the soul gem, at 0
  itembox::apply({itembox::Change{itembox::Op::kTreasureSet, -1, kGem, 4}});
  tick_two(true);
  check(held(live_list(0), kGem) == 4 && entry_id(live_list(0), 6) == 0 && held(saved_list(1), kGem) == 4,
        "treasures: an entry at 0 cleared once another holds the treasure");
  check(held(live_list(1), kGem) == 5, "treasures: ... the count set by Chris's list (1 -> 4), Sheva's given as many "
                                       "more (2 -> 5)");
  itembox::apply({itembox::Change{itembox::Op::kTreasureSet, -1, kGem, 9}});
  tick_two(false);
  check(held(live_list(0), kGem) == 4 && held(profile_list(false), kGem) == 4 &&
            std::strstr(itembox::view().note, "pause") != nullptr,
        "treasures: nothing changed while the game is not paused");
  // Beside Item Management: the profile's list alone, and the screen's treasures list
  // rebuilt for the pane in use.
  namespace box = itembox;
  box::g_pane = 0x1B0;
  box::g_pane_step = 0xA64;
  box::g_in_use = 0x54;
  box::g_sub = 1;
  box::g_browse = (1u << 1) | (1u << 5);
  box::g_menu_state = 0;
  box::g_menu_step = 10;
  box::g_rebuild = fake_box_rebuild;
  box::g_trebuild = fake_treasure_rebuild;
  const uintptr_t screen = reinterpret_cast<uintptr_t>(g_screen.data());
  mem::write<uint8_t>(screen + 0x1B0, 5);      // browsing the box
  mem::write<uint8_t>(screen + 0x1B0 + 1, 1);  // at its step 1
  mem::write<int32_t>(screen + 0x1B0 + 0x54, 0);
  mem::write<int32_t>(screen + 0x1B0 + 0xA64 + 0x54, -1);
  game::Run r;
  r.screen = game::Screen::kItemBox;
  r.item_screen = screen;
  itembox::apply({itembox::Change{itembox::Op::kTreasureSet, -1, kGem, 7}});
  inventory::tick(r);
  itembox::tick(r);
  v = itembox::view();
  check(held(profile_list(false), kGem) == 7 && held(profile_list(true), kGem) == 4 && held(live_list(0), kGem) == 4 &&
            v.open && kind_of(v, kGem) && kind_of(v, kGem)->count == 7,
        "treasures: beside Item Management the profile's list alone (the soul gem 4 -> 7), as the view shows");
  check(g_treasure_rebuilds == 1 && g_box_rebuilds == 0 && mem::read<uint8_t>(screen + 0x1B0 + 1) == 0,
        "treasures: ... the screen's treasures list rebuilt for the pane in use (not the box's), which steps back to "
        "show it afresh");
}
// --- the moves in the saved copies of the case ---------------------------------------------------
constexpr uint32_t kSavedAt = 0x240, kSavedStride = 0x420, kSavedEntry = 0x2C, kSavedCounts = 0x12C0;
constexpr int kCopies = 5;
// Copy k of Chris's saved case: the profile's cGameInfo's saved state, working block and its
// copy, then the play flow's copy's saved state and working block.
uintptr_t copy_base(int k) {
  return reinterpret_cast<uintptr_t>(k < 3 ? g_info.data() : g_snap.data()) +
         (k == 0 || k == 3 ? kState : k == 1 || k == 4 ? kWork : kCopy);
}
uintptr_t saved_at(int k, int slot) {
  return copy_base(k) + (kSavedAt - kState) + static_cast<uint32_t>(slot) * kSavedEntry;
}
void put_saved(int k, int slot, uint32_t id, int num, int equip) {
  const uintptr_t e = saved_at(k, slot);
  mem::write<uint32_t>(e, id);
  mem::write<int32_t>(e + 4, num);
  mem::write<int32_t>(e + 0x14, slot);
  mem::write<uint32_t>(e + 0x18, static_cast<uint32_t>(equip));
}
uint32_t sid(int k, int slot) { return mem::read<uint32_t>(saved_at(k, slot)); }
int snum(int k, int slot) { return mem::read<int32_t>(saved_at(k, slot) + 4); }
int sslot(int k, int slot) { return mem::read<int32_t>(saved_at(k, slot) + 0x14); }
uint32_t sequip(int k, int slot) { return mem::read<uint32_t>(saved_at(k, slot) + 0x18); }
// Every copy as the chapter's start left it (the saved cases, the counts 12).
void clear_copies() {
  std::fill(g_info.begin() + kState, g_info.begin() + kCopy + kBlockSize, 0);
  std::fill(g_snap.begin(), g_snap.end(), 0);
  for (int k = 0; k < kCopies; ++k)
    mem::write<int32_t>(copy_base(k) + (kSavedCounts - kState), 12);
}
void set_live(int i, int slot, uint32_t id, int num, int equip) {
  mem::write<uint32_t>(item_at(i) + 0x04, id);
  mem::write<uint32_t>(item_at(i) + 0x08, static_cast<uint32_t>(num));
  mem::write<uint32_t>(item_at(i) + 0x0C, 50);
  mem::write<uint32_t>(item_at(i) + 0x18, static_cast<uint32_t>(slot));
  mem::write<uint32_t>(item_at(i) + 0x1C, static_cast<uint32_t>(equip));
  mem::write<uint8_t>(item_at(i) + 0x29, 1);
}
int live_slot_of(uint32_t id) {
  for (int i = 0; i < 24; ++i)
    if (mem::read<uint8_t>(item_at(i) + 0x29) && mem::read<uint32_t>(item_at(i) + 0x04) == id)
      return static_cast<int>(mem::read<uint32_t>(item_at(i) + 0x18));
  return -1;
}
// How many of an item the box holds (a stack's count, else its entries) and a saved state's case.
int in_box(uint32_t id, bool stack) {
  int n = 0;
  for (int i = 0; i < 54; ++i)
    if (static_cast<uint32_t>(box_id(i)) == id)
      n += stack ? mem::read<uint16_t>(reinterpret_cast<uintptr_t>(g_sys.data()) + 0x3840 + i * 12 + 2) : 1;
  return n;
}
int in_state(uint32_t id, bool stack) {
  int n = 0;
  for (int s = 0; s < 9; ++s)
    if (sid(0, s) == id) n += stack ? snum(0, s) : 1;
  return n;
}
// The game's reverts: RESTART and a death's CONTINUE take the saved state and the working
// block back from the play flow's copy; QUIT's "No" the working block from its copy, then
// the saved state from it. The case is made from the saved state.
void restart() {
  std::memcpy(g_info.data() + kState, g_snap.data() + kState, kBlockSize);
  std::memcpy(g_info.data() + kWork, g_snap.data() + kWork, kBlockSize);
}
void quit_no() {
  std::memcpy(g_info.data() + kWork, g_info.data() + kCopy, kBlockSize);
  std::memcpy(g_info.data() + kState, g_info.data() + kWork, kBlockSize);
}

void check_moves() {
  constexpr uint32_t kM92 = 0x102, kRocket = 0x10D, kDragunov = 0x120, kAmmo = 0x201, kSpray = 0x304;
  constexpr uint32_t kLauncher = 0x10C, kLauncherExp = 0x125, kShotgun = 0x104, kMagnum = 0x111;
  constexpr uint32_t kLauncherAcd = 0x126, kLantern = 0x130;
  for (const auto& [id, name] : std::vector<std::pair<int, const char*>>{
           {kRocket, "uWp0d"}, {kDragunov, "uWp20"}, {kShotgun, "uWp04"}, {kMagnum, "uWp11"}}) {
    g_classes.push_back(mt::Class{static_cast<uintptr_t>(0x3000 + id), 0, name, 0, 0, key(id)});
    add_entry(kCur, name, 3, true);
  }
  // The chapter's start, as every copy has it: the M92F in hand, the Dragunov the one
  // before, 30 rounds of handgun ammo. Since: 20 rounds used. The box: the rocket launcher.
  tick(false);
  for (int i = 0; i < 24; ++i) mem::write<uint8_t>(item_at(i) + 0x29, 0);
  for (int i = 0; i < 54; ++i) put_in_box(i, 0, 0);
  clear_copies();
  for (int k = 0; k < kCopies; ++k) {
    put_saved(k, 0, kM92, 10, 1);
    put_saved(k, 1, kDragunov, 18, 2);
    put_saved(k, 3, kAmmo, 30, 0);
  }
  put_saved(1, 2, 0x105, 1, 0);  // the working block: a gun in slot 2, given away since
  set_live(0, 0, kM92, 10, 1);
  set_live(1, 1, kDragunov, 18, 2);
  set_live(2, 3, kAmmo, 10, 0);
  put_in_box(23, kRocket, 1);
  tick(true);
  check(itembox::view().can_move, "moves: offered at the pause with the saved copies understood");
  // The user's play: the rocket launcher taken, the Dragunov stored.
  itembox::apply({itembox::Change{itembox::Op::kTake, 23, static_cast<int>(kRocket), 0, 0}});
  tick(true);
  const int rocket = live_slot_of(kRocket);
  bool ok = rocket == 2 && box_id(23) == 0;
  for (int k = 0; k < kCopies; ++k) {
    const int at = k == 1 ? 4 : 2;  // the working block's slot 2 holds a gun: its first free one
    ok = ok && sid(k, at) == kRocket && snum(k, at) == 1 && sslot(k, at) == at && sequip(k, at) == 0;
  }
  check(ok && sid(1, 2) == 0x105, "moves: a Take - the rocket launcher in the case (slot 2) and in every saved copy "
                                  "of it, in that slot or the first free one (its slot that one), not in hand; gone "
                                  "from the box");
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, static_cast<int>(kDragunov), 0, 0, 1}});
  tick(true);
  ok = live_slot_of(kDragunov) < 0 && in_box(kDragunov, false) == 1;
  for (int k = 0; k < kCopies; ++k) ok = ok && sid(k, 1) == 0 && sid(k, 0) == kM92 && sequip(k, 0) == 1;
  check(ok, "moves: a Store - the Dragunov in the box, gone from the case and from every saved copy");
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, static_cast<int>(kAmmo), 0, 0, 3}});
  tick(true);
  ok = in_box(kAmmo, true) == 10;
  for (int k = 0; k < kCopies; ++k) ok = ok && sid(k, 3) == kAmmo && snum(k, 3) == 20;
  check(ok, "moves: a stack stored (10 rounds) - each copy's lessened by as much (30 -> 20)");
  // The game's reverts: each item once, the ammo as a revert hands it back with the box's.
  const std::vector<uint8_t> info = g_info;
  restart();
  check(in_state(kDragunov, false) + in_box(kDragunov, false) == 1 && in_state(kRocket, false) == 1 &&
            in_box(kRocket, false) == 0 && in_state(kAmmo, true) + in_box(kAmmo, true) == 30,
        "moves: RESTART (or a death's CONTINUE) - the Dragunov once (the box), the rocket launcher once (the case), "
        "the ammo as the checkpoint had it (20 + the box's 10)");
  std::copy(info.begin(), info.end(), g_info.begin());
  quit_no();
  check(in_state(kDragunov, false) + in_box(kDragunov, false) == 1 && in_state(kRocket, false) == 1 &&
            in_box(kRocket, false) == 0,
        "moves: QUIT without saving - the same");
  std::copy(info.begin(), info.end(), g_info.begin());

  // A copy that had the stored item in hand: the one before put in hand there.
  clear_copies();
  for (int k = 0; k < kCopies; ++k) put_saved(k, 0, kM92, 10, 1);
  put_saved(0, 0, kM92, 10, 2);
  put_saved(0, 4, kShotgun, 6, 1);
  set_live(3, 4, kShotgun, 6, 0);
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, static_cast<int>(kShotgun), 0, 0, 4}});
  tick(true);
  check(sid(0, 4) == 0 && sequip(0, 0) == 1 && sequip(1, 0) == 1,
        "moves: the copy that had the stored shotgun in hand puts the one before in hand (the M92F)");
  // An empty gun (no rounds loaded): gone from the copies all the same.
  clear_copies();
  for (int k = 0; k < kCopies; ++k) put_saved(k, 7, kRocket, 1, 0);
  set_live(5, 7, kRocket, 0, 0);
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, static_cast<int>(kRocket), 0, 0, 7}});
  tick(true);
  ok = live_slot_of(kRocket) < 0;
  for (int k = 0; k < kCopies; ++k) ok = ok && sid(k, 7) == 0;
  check(ok, "moves: an empty gun stored (the rocket launcher, fired) goes from every copy too");
  // A grenade launcher: the case's item has the launcher's id (0x10C), its model the rounds
  // loaded - acid ones since the checkpoint, whose copies keep it explosive (0x125). The box
  // keeps it as its load, as the game's own save of a case writes it: G. launcher (ACD).
  clear_copies();
  for (int k = 0; k < kCopies; ++k) put_saved(k, 5, kLauncherExp, 6, 0);
  set_live(4, 5, kLauncher, 6, 0);
  mem::write<uintptr_t>(item_at(4) + 0x2C, launcher_model(0x141));
  set_live(6, 7, kAmmo, 15, 0);
  tick(true);
  check(inventory::view().cases[0].slot[5].kept_as == static_cast<int>(kLauncherAcd) &&
            inventory::view().cases[0].slot[7].used && !inventory::view().cases[0].slot[7].kept_as,
        "moves: the view keeps the launcher as G. launcher (ACD) (its model's rounds), the ammo as itself");
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, static_cast<int>(kLauncher), 0, 0, 5}});
  tick(true);
  ok = live_slot_of(kLauncher) < 0 && in_box(kLauncherAcd, false) == 1 && !in_box(kLauncher, false) &&
       !in_box(kLauncherExp, false);
  for (int k = 0; k < kCopies; ++k) ok = ok && sid(k, 5) == 0;
  check(ok, "moves: a grenade launcher stored goes in the box as its load, G. launcher (ACD) - not as the case's "
            "0x10C - and from the copies keeping another (0x125)");
  // One whose load cannot be read (no model): not stored - the box would keep the bare
  // launcher, which the switch loads with explosive rounds.
  clear_copies();
  for (int k = 0; k < kCopies; ++k) put_saved(k, 5, kLauncherExp, 6, 0);
  set_live(4, 5, kLauncher, 6, 0);
  mem::write<uintptr_t>(item_at(4) + 0x2C, 0);
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, static_cast<int>(kLauncher), 0, 0, 5}});
  tick(true);
  ok = live_slot_of(kLauncher) == 5 && !in_box(kLauncher, false) &&
       std::strstr(itembox::view().note, "only what the Add lists offer") != nullptr;
  for (int k = 0; k < kCopies; ++k) ok = ok && sid(k, 5) == kLauncherExp;
  check(ok, "moves: a launcher whose load cannot be read stays in the case and every copy, said so");
  // A scene's own weapon (the Lantern, 2-2's mine): the box takes only what the Add lists
  // offer (the user, 2026-11-01: "store should refuse those types of items").
  clear_copies();
  for (int k = 0; k < kCopies; ++k) put_saved(k, 5, kLantern, 1, 0);
  set_live(4, 5, kLantern, 1, 0);
  set_live(6, 7, kAmmo, 15, 0);
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, static_cast<int>(kLantern), 0, 0, 5},
                  itembox::Change{itembox::Op::kStore, -1, static_cast<int>(kAmmo), 0, 0, 7}});
  tick(true);
  ok = live_slot_of(kLantern) == 5 && !in_box(kLantern, false) && live_slot_of(kAmmo) < 0 &&
       std::strstr(itembox::view().note, "1 stored; 1 not stored: the box takes only what the Add lists offer");
  for (int k = 0; k < kCopies; ++k) ok = ok && sid(k, 5) == kLantern;
  check(ok, "moves: the Lantern (a scene's own) not stored - kept in the case and every copy, the box without it, "
            "said so; the ammo ticked with it stored");
  mem::write<uint8_t>(item_at(4) + 0x29, 0);

  // A copy full when it was saved: the Take takes the place of a stack the case holds less
  // of now (the spray used since); with none such, it is refused.
  for (int i = 0; i < 24; ++i) mem::write<uint8_t>(item_at(i) + 0x29, 0);
  clear_copies();
  for (int s = 0; s < 9; ++s) {
    const uint32_t id = s == 4 ? kSpray : 0x103 + static_cast<uint32_t>(s);
    for (int k = 0; k < kCopies; ++k) put_saved(k, s, id, 1, s == 0);
    if (s != 4) set_live(s, s, id, 1, s == 0);
  }
  put_in_box(30, kMagnum, 6);
  itembox::apply({itembox::Change{itembox::Op::kTake, 30, static_cast<int>(kMagnum), 0, 0}});
  tick(true);
  ok = live_slot_of(kMagnum) == 4;
  for (int k = 0; k < kCopies; ++k) ok = ok && sid(k, 4) == kMagnum && sslot(k, 4) == 4;
  check(ok, "moves: a copy full when saved - the Take in place of the spray used since");
  restart();
  check(in_state(kMagnum, false) == 1 && in_box(kMagnum, false) == 0, "moves: ... and a restart keeps it once");
  // Full again, of guns the case still holds - one given away since: refused.
  mem::write<uint8_t>(item_at(4) + 0x29, 0);  // the magnum out of the case again
  clear_copies();
  for (int s = 0; s < 9; ++s)
    for (int k = 0; k < kCopies; ++k) put_saved(k, s, s == 4 ? 0x112 : 0x103 + static_cast<uint32_t>(s), 1, s == 0);
  put_in_box(31, kRocket, 1);
  g_adds.clear();
  itembox::apply({itembox::Change{itembox::Op::kTake, 31, static_cast<int>(kRocket), 0, 0}});
  tick(true);
  check(g_adds.empty() && box_id(31) == static_cast<int>(kRocket) && live_slot_of(kRocket) < 0 &&
            std::strstr(itembox::view().note, "no room") != nullptr,
        "moves: a copy with no room for it, nothing the case holds less of - the Take refused, nothing changed, "
        "said so");
  // Nothing moved while Chapter Select goes, nor without the saved copies.
  clear_copies();
  g_leaving = true;
  itembox::apply({itembox::Change{itembox::Op::kTake, 31, static_cast<int>(kRocket), 0, 0}});
  tick(true);
  check(g_adds.empty() && box_id(31) == static_cast<int>(kRocket) && !itembox::view().can_move,
        "moves: none while Chapter Select goes");
  g_leaving = false;
  g_saves_known = false;
  itembox::apply({itembox::Change{itembox::Op::kTake, 31, static_cast<int>(kRocket), 0, 0}});
  tick(true);
  check(g_adds.empty() && box_id(31) == static_cast<int>(kRocket) && !itembox::view().can_move,
        "moves: none without the saved copies understood");
  g_saves_known = true;
}

// --- the extra episodes' boxes ---------------------------------------------------------------------
std::string path_in_dir(const char* name) { return g_dir + name; }
bool file_there(const char* name) { return GetFileAttributesA(path_in_dir(name).c_str()) != INVALID_FILE_ATTRIBUTES; }
std::string file_text(const char* name) {
  std::string t;
  if (FILE* f = std::fopen(path_in_dir(name).c_str(), "rb")) {
    for (int c = std::fgetc(f); c != EOF; c = std::fgetc(f)) t += static_cast<char>(c);
    std::fclose(f);
  }
  return t;
}
void write_text(const char* name, const char* text) {
  if (FILE* f = std::fopen(path_in_dir(name).c_str(), "wb")) {
    std::fputs(text, f);
    std::fclose(f);
  }
}
// The view's box: how much of an item it holds (a stack's count, else its entries).
int in_view(const itembox::View& v, int id, bool stack) {
  int n = 0;
  for (int i = 0; i < v.size; ++i)
    if (v.entry[i].id == id) n += stack ? v.entry[i].count : 1;
  return n;
}
int index_in_view(const itembox::View& v, int id) {
  for (int i = 0; i < v.size; ++i)
    if (v.entry[i].id == id) return i;
  return -1;
}
void tick_in(int mode) {
  g_run_mode = mode;
  tick(true);
}

void check_episodes() {
  constexpr const char* kNightmaresFile = "RE5CabbyCodes.itembox.lost-in-nightmares.ini";
  constexpr const char* kEscapeFile = "RE5CabbyCodes.itembox.desperate-escape.ini";
  constexpr int kM92 = 0x102, kAmmo = 0x201, kMgAmmo = 0x202, kDragunov = 0x120, kRing = 0x417;
  DeleteFileA(path_in_dir(kNightmaresFile).c_str());
  DeleteFileA(path_in_dir(kEscapeFile).c_str());
  itembox::load_files();  // as the mod starts: no files yet
  check(itembox::g_files[0].usable && itembox::g_files[1].usable && !file_there(kNightmaresFile) &&
            !file_there(kEscapeFile) && !std::strcmp(itembox::file_name(game::kModeNightmares), kNightmaresFile) &&
            !std::strcmp(itembox::file_name(game::kModeEscape), kEscapeFile) && !itembox::file_name(game::kModeStory),
        "episodes: no files yet - both boxes start empty, and nothing is written until they change");
  // The story's box holds 77 rounds of handgun ammo; Chris's case the M92F (in hand), machine
  // gun ammo and the Dragunov; every saved copy of it the same.
  for (int i = 0; i < 24; ++i) mem::write<uint8_t>(item_at(i) + 0x29, 0);
  for (int i = 0; i < 54; ++i) put_in_box(i, 0, 0);
  put_in_box(0, kAmmo, 77);
  clear_copies();
  for (int k = 0; k < kCopies; ++k) {
    put_saved(k, 0, kM92, 10, 1);
    put_saved(k, 1, kMgAmmo, 30, 0);
    put_saved(k, 2, kDragunov, 7, 0);
  }
  set_live(0, 0, kM92, 10, 1);
  set_live(1, 1, kMgAmmo, 30, 0);
  set_live(2, 2, kDragunov, 7, 0);
  for (int k = 0; k < 9; ++k) mem::write<uint8_t>(item_at(2) + 0x20 + k, 0);
  mem::write<uint8_t>(item_at(2) + 0x20, 3);  // the Dragunov's Power level ...
  mem::write<uint8_t>(item_at(2) + 0x28, 2);  // ... and its Scope's
  for (int i = 0; i < 54; ++i) put_treasure(profile_list(false), i, 0, 0);
  put_treasure(profile_list(false), 0, kRing, 2);

  tick_in(game::kModeNightmares);
  itembox::View v = itembox::view();
  check(v.in_play && v.mode == game::kModeNightmares && v.episode && !std::strcmp(v.episode, "Lost in Nightmares") &&
            v.file && !std::strcmp(v.file, kNightmaresFile) && v.used == 0 && v.size == 54 && !v.treasures &&
            v.editable && v.can_move && !v.unreadable && !v.unwritten,
        "episodes: at the pause in Lost in Nightmares the panel's box is its own - empty, not the story's (77 rounds), "
        "no treasures - and can change");
  itembox::apply({itembox::Change{itembox::Op::kAdd, -1, kAmmo, 25}});
  tick_in(game::kModeNightmares);
  v = itembox::view();
  check(in_view(v, kAmmo, true) == 25 && box_id(0) == kAmmo && in_box(kAmmo, true) == 77 &&
            file_text(kNightmaresFile).find("\n0x201:25 ") != std::string::npos && !v.unwritten,
        "episodes: an Add (25 rounds) goes into Lost in Nightmares' box and its file at once; the story's box keeps "
        "its 77");
  // A Store: the Dragunov into the episode's box, out of the case - and out of the copies the
  // episode makes the case again from (its saved state, the play flow's copy of it), never the
  // story's (the working block, its checkpoint copy, the play flow's copy of the working block).
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, kDragunov, 0, 0, 2}});
  tick_in(game::kModeNightmares);
  v = itembox::view();
  check(live_slot_of(kDragunov) < 0 && in_view(v, kDragunov, false) == 1 && in_box(kDragunov, false) == 0 &&
            sid(0, 2) == 0 && sid(3, 2) == 0 && sid(1, 2) == static_cast<uint32_t>(kDragunov) &&
            sid(2, 2) == static_cast<uint32_t>(kDragunov) && sid(4, 2) == static_cast<uint32_t>(kDragunov) &&
            file_text(kNightmaresFile).find("\n0x120:7:0300000000000200 ") != std::string::npos,
        "episodes: a Store - the Dragunov in the episode's box and its file (its rounds and levels), gone from the "
        "case, its saved state and the play flow's copy of it; the story's working block and checkpoint copy keep it");
  // A Take: the 25 rounds into the case (the slot the Dragunov left), and into the same two copies.
  const int at = index_in_view(v, kAmmo);
  itembox::apply({itembox::Change{itembox::Op::kTake, at, kAmmo, 0, 0}});
  tick_in(game::kModeNightmares);
  v = itembox::view();
  check(at >= 0 && in_view(v, kAmmo, true) == 0 && live_slot_of(kAmmo) == 2 && sid(0, 2) == kAmmo &&
            snum(0, 2) == 25 && sid(3, 2) == kAmmo && sid(1, 2) == static_cast<uint32_t>(kDragunov) &&
            sid(2, 2) == static_cast<uint32_t>(kDragunov) && in_box(kAmmo, true) == 77 &&
            file_text(kNightmaresFile).find("0x201") == std::string::npos,
        "episodes: a Take - the rounds in the case, its saved state and the play flow's copy of it, gone from the "
        "episode's box and its file; the story's copies and box untouched");
  // A treasure asked for in an episode changes nothing: the episodes have none.
  itembox::apply({itembox::Change{itembox::Op::kTreasureAdd, -1, kRing, 3}});
  tick_in(game::kModeNightmares);
  check(held(profile_list(false), kRing) == 2 && !itembox::view().treasures,
        "episodes: no treasures in an episode - a treasure's change asked there leaves the profile's alone");

  // Desperate Escape: a box of its own, apart from Lost in Nightmares'.
  const std::string nightmares = file_text(kNightmaresFile);
  tick_in(game::kModeEscape);
  v = itembox::view();
  check(v.in_play && v.episode && !std::strcmp(v.episode, "Desperate Escape") && v.used == 0 && !v.treasures,
        "episodes: at the pause in Desperate Escape its own box - empty, apart from Lost in Nightmares'");
  itembox::apply({itembox::Change{itembox::Op::kAdd, -1, kMgAmmo, 40}});
  tick_in(game::kModeEscape);
  check(in_view(itembox::view(), kMgAmmo, true) == 40 && file_text(kEscapeFile).find("\n0x202:40 ") != std::string::npos &&
            file_text(kNightmaresFile) == nightmares,
        "episodes: an Add there goes into Desperate Escape's file alone");

  // The mod's next start: each box read back from its file.
  itembox::load_files();
  tick_in(game::kModeNightmares);
  v = itembox::view();
  const int gun = index_in_view(v, kDragunov);
  const bool nightmares_back = gun >= 0 && v.entry[gun].count == 7 && v.entry[gun].lv[0] == 3 &&
                               v.entry[gun].lv[6] == 2 && v.used == 1;
  tick_in(game::kModeEscape);
  check(nightmares_back && in_view(itembox::view(), kMgAmmo, true) == 40 && itembox::view().used == 1,
        "episodes: read back as the mod starts again - the Dragunov (7 rounds loaded, its levels) in Lost in "
        "Nightmares' box, 40 machine gun rounds in Desperate Escape's");

  // A file the mod cannot read (a line it does not know, an item no Add list offers, a stack
  // past 999): left as it is, its box not used, nothing written over it.
  for (const char* bad : {"0x202:40\nnot an entry\n", "0x130:1 ; Lantern\n", "0x202:1000\n"}) {
    write_text(kEscapeFile, bad);
    itembox::load_files();
    tick_in(game::kModeEscape);
    v = itembox::view();
    const bool shown = v.in_play && v.unreadable && !v.editable && !v.can_move && v.used == 0;
    itembox::apply({itembox::Change{itembox::Op::kAdd, -1, kMgAmmo, 5}});
    tick_in(game::kModeEscape);
    std::string shown_as = bad;
    for (size_t k; (k = shown_as.find('\n')) != std::string::npos;) shown_as.replace(k, 1, "\\n");
    char what[160];
    std::snprintf(what, sizeof(what), "episodes: a file that cannot be read (\"%s\") is left as it is - the box not "
                  "used, said so",
                  shown_as.c_str());
    check(shown && file_text(kEscapeFile) == bad && std::strstr(itembox::view().note, "could not be read") != nullptr,
          what);
  }
  tick_in(game::kModeNightmares);
  check(itembox::g_files[0].usable && in_view(itembox::view(), kDragunov, false) == 1,
        "episodes: ... the other episode's box unaffected");

  // A box whose file cannot be written: a Store is refused (the item stays in the case and
  // every copy); an Add is kept and written again once it can be.
  const std::string blocker = path_in_dir(kNightmaresFile) + ".tmp";
  CreateDirectoryA(blocker.c_str(), nullptr);  // the file written beside it cannot be made
  tick_in(game::kModeNightmares);
  const std::string unblocked = file_text(kNightmaresFile);
  itembox::apply({itembox::Change{itembox::Op::kStore, -1, kAmmo, 0, 0, 2}});
  tick_in(game::kModeNightmares);
  v = itembox::view();
  check(live_slot_of(kAmmo) == 2 && sid(0, 2) == kAmmo && in_view(v, kAmmo, true) == 0 &&
            file_text(kNightmaresFile) == unblocked && !v.unwritten &&
            std::strstr(v.note, "not stored: the box's file could not be written") != nullptr,
        "episodes: a Store whose file cannot be written is refused - the rounds stay in the case and its copies, "
        "the box without them, said so");
  const std::string before = file_text(kNightmaresFile);
  itembox::apply({itembox::Change{itembox::Op::kAdd, -1, kMgAmmo, 9}});
  tick_in(game::kModeNightmares);
  check(itembox::view().unwritten && in_view(itembox::view(), kMgAmmo, true) == 9 && file_text(kNightmaresFile) == before,
        "episodes: an Add whose file cannot be written stays in the box, said not to be in the file yet");
  RemoveDirectoryA(blocker.c_str());
  for (int i = 0; i < 61; ++i) tick_in(game::kModeNightmares);
  check(!itembox::view().unwritten && file_text(kNightmaresFile).find("\n0x202:9 ") != std::string::npos,
        "episodes: ... written once it can be (tried again every second)");

  // No box in the Mercenaries, nor while the game's mode cannot be read; the story's again.
  tick_in(2);
  const bool merc = !itembox::view().in_play;
  tick_in(-1);
  const bool unknown = !itembox::view().in_play;
  tick_in(game::kModeStory);
  v = itembox::view();
  check(merc && unknown && v.in_play && !v.episode && v.mode == game::kModeStory && v.treasures &&
            in_view(v, kAmmo, true) == 77,
        "episodes: no box in the Mercenaries nor with the mode unknown; back in the story, the game's own box (77 "
        "rounds) and its treasures");
  for (int i = 0; i < 24; ++i) mem::write<uint8_t>(item_at(i) + 0x29, 0);
  DeleteFileA(path_in_dir(kNightmaresFile).c_str());
  DeleteFileA(path_in_dir(kEscapeFile).c_str());
}
}  // namespace

int main(int argc, char** argv) {
  g_verbose = argc > 1 && std::strcmp(argv[1], "-v") == 0;
  char tmp[MAX_PATH] = {};
  GetTempPathA(MAX_PATH, tmp);
  g_dir = std::string(tmp) + "re5cc_test_loader\\";
  CreateDirectoryA(g_dir.c_str(), nullptr);
  setup();
  check_area();
  check_archives();
  check_load();
  check_cutscenes();
  check_add_waits();
  check_take_waits();
  check_preload();
  check_theirs();
  check_treasures();
  check_moves();
  check_episodes();
  RemoveDirectoryA(g_dir.c_str());
  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  return g_fail ? 1 : 0;
}
