#include "inventory.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "image.h"
#include "items.h"
#include "log.h"
#include "mem.h"
#include "menus.h"
#include "mt.h"

namespace re5cc::inventory {
namespace {

CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
View g_view;

enum class Op { kCount, kAdd, kThrow, kPreload };
struct Request {
  Op op;
  int chara, slot, count, id;
};
std::vector<Request> g_requests;

// The inventory's own functions: cItemManager's add (18 arguments; thiscall) and
// cItem's clear (thiscall; true: let the weapon model go too).
using AddFn = uintptr_t(__thiscall*)(void* mgr, uint32_t id, int num, int most, int a, int b, int slot, int l0,
                                     int l1, int l2, int l3, int l4, int l5, int l6, int l7, int l8, void* owner,
                                     uint32_t hand, uint32_t z);
using ClearFn = void(__thiscall*)(void* item, int release_model);
AddFn g_add = nullptr;
ClearFn g_clear = nullptr;
uint32_t g_hand_field = 0;  // the character's field the grid hands its add (with the character)
constexpr int kAnySlot = 13;   // "no slot": the add picks one
constexpr int kFullLoad = -2;  // a weapon's count: its whole magazine

void note(View& v, const char* text) { std::snprintf(v.note, sizeof(v.note), "%s", text); }

// --- what the area has loaded --------------------------------------------------------------------
// The add makes an item put in a case a model of its own (cItem's set, when no
// model is handed over - a pickup hands over the one that lay on the floor): a
// unit of the item's model class - uWp13 for the MP5, uIt0201 for handgun ammo -
// whose model, motions (each character's for a weapon), sounds and effects come
// from that class's archive, Image\Archive\uWp13. The game loads an archive only
// for what an area needs: what the characters carry as it loads, what lies in it,
// what its enemies drop (sLoadMan, asked by each class). A model made without its
// archive sends the game to a loose file that is not there: it writes the name to
// re5LoadFails_jim.txt and quits ("Failed open file", the user's Take of an MP5 at
// the pause, 2026-10-29). So in play an item goes into a case only while sLoadMan
// lists its archive as loaded - and one that is not, sLoadMan is asked for first,
// as the game asks it as an area loads (the user's word, 2026-10-29: "yes make it
// work for any item"): its request of the model's class (and of what the class
// needs with it: a gun's ammo, a launcher's rounds), then its commit of the list,
// which starts the loading; its frame runs paused or not.
struct Loader {
  uint32_t count = 0, items = 0, stride = 0;  // list k's count and its items: these + k * stride
  int lists = 0;
  uint32_t current = 0;            // the list it adds to
  uint32_t path = 0, archive = 0;  // an entry's path and archive
  uint32_t flags = 0;              // the archive's flags
  uint32_t state = 0;              // an entry's state
  int sets = -1;       // the area's sets' list: the one the mod asks into (see load)
  int cutscenes = -1;  // the cutscenes' list: never asked into
} g_ld;
constexpr uint32_t kLoaded = 0x1, kFailed = 0x10;  // the archive's flags (kSigLoadReady)
// An entry's state (kSigLoadList's switch, kSigLoadReady): 1 or 4 asked, 2 loading,
// 3 loaded; 0 once its list is chosen again, until it is asked for again - a commit
// then lets it go.
constexpr uint32_t kAgain = 0;
using RequestFn = void(__thiscall*)(void* lm, int kind, int number, int variant, int z);
using CommitFn = int(__thiscall*)(void* lm);
RequestFn g_request = nullptr;
CommitFn g_commit = nullptr;
// cItem's model: the ids the switch gives another's model (g_model_as) - those
// whose case in the jump table is not the default - and the kind of the class key.
uint32_t g_model_lo = 0, g_model_n = 0, g_model_as = 0, g_model_kind = 0;
uintptr_t g_model_bytes = 0, g_model_jumps = 0, g_model_default = 0;
const mt::Class* g_uitem = nullptr;
// The game's own save of a case writes a grenade launcher as its load (kSigSaveLauncher):
// the switch gives every load the launcher's model and the case's item the launcher's id
// (kSigModelMake), the model keeping the rounds loaded; the save reads them back into the
// load's id - g_load_of[rounds - g_rounds_lo] (0: the launcher's id kept).
constexpr uint32_t kMaxRounds = 32;
uint32_t g_launcher = 0, g_rounds = 0, g_rounds_lo = 0, g_rounds_n = 0;
uint32_t g_load_of[kMaxRounds] = {};
const mt::Class* g_launcher_cls = nullptr;  // the model's class (uWpLauncher)
bool g_area_ok = false;  // what is loaded can be read
bool g_load_ok = false;  // ... and the loader asked for more
std::string g_archive_dir;  // sLoadMan's path of a class's archive: this, then its name (kSigLoadPath)

std::vector<std::string> g_loaded;  // the class archives loaded now (their names, sorted); main thread
int g_loaded_age = -1;              // ticks since g_loaded was read (-1: not read this pause)

// Adds waiting for the loader (main thread), made once their archives are in.
struct Waiting {
  int chara = 0, id = 0, count = 0, age = 0;
};
std::vector<Waiting> g_waiting;

bool discover_area() {
  const uintptr_t list = game::sig_one("kSigLoadList");
  const uintptr_t ready = game::sig_one("kSigLoadReady");
  const uintptr_t current = game::sig_one("kSigLoadCurrent");
  const uintptr_t sw = game::sig_one("kSigModelSwitch");
  const uintptr_t make = game::sig_one("kSigModelMake");
  const mt::Class* lm = mt::find("sLoadMan");
  g_uitem = mt::find("uItem");
  if (!list || !ready || !current || !sw || !make || !lm || !g_uitem) return false;
  // The lists (MtArrays: the count +4, the items +0x10 - list k eax * 5 * 4 bytes
  // on), up to the field that says which one is added to.
  g_ld.count = mem::read<uint32_t>(list + 9);
  g_ld.items = mem::read<uint32_t>(list + 23);
  g_ld.stride = 5 * 4;
  const uint32_t first = g_ld.count - 4, cur = mem::read<uint32_t>(current + 6);
  g_ld.lists = cur > first ? static_cast<int>((cur - first) / g_ld.stride) : 0;
  g_ld.current = cur;
  g_ld.path = mem::read<uint8_t>(list + 58);
  g_ld.state = mem::read<uint8_t>(list + 32);
  g_ld.archive = mem::read<uint8_t>(ready + 2);
  g_ld.flags = mem::read<uint8_t>(ready + 9);
  if (g_ld.items != g_ld.count + 0xC || mem::read<uint32_t>(current + 18) != g_ld.count || (cur - first) % g_ld.stride ||
      g_ld.lists < 1 || g_ld.lists > 32 || cur + 4 > lm->size || ready < list || ready > list + 0x100 ||
      mem::read<uint8_t>(ready + 26) != g_ld.path || mem::read<uint8_t>(ready + 30) != g_ld.archive ||
      mem::read<uint8_t>(ready + 55) != g_ld.flags || mem::read<uint8_t>(ready + 61) != mem::read<uint8_t>(list + 32) ||
      g_ld.archive <= g_ld.path || g_ld.archive - g_ld.path > 0x100)
    return false;
  // The model's switch: over [lo, lo + n), through a byte table into a jump table.
  g_model_lo = 0u - mem::read<uint32_t>(sw + 5);
  g_model_n = mem::read<uint8_t>(sw + 15) + 1u;
  g_model_default = sw + 18 + static_cast<int8_t>(mem::read<uint8_t>(sw + 17));
  g_model_bytes = mem::read<uint32_t>(sw + 21);
  g_model_jumps = mem::read<uint32_t>(sw + 28);
  g_model_as = mem::read<uint32_t>(make + 5);
  g_model_kind = mem::read<uint8_t>(make + 24);
  if (make < sw || make > sw + 0x60 || (!image::rdata().contains(g_model_bytes) && !image::text().contains(g_model_bytes)) ||
      !image::text().contains(g_model_default) || g_model_n > 0x400 || !mem::readable(g_model_bytes, g_model_n))
    return false;
  for (uint32_t i = 0; i < g_model_n; ++i) {
    const uint8_t k = mem::read<uint8_t>(g_model_bytes + i);
    if (!mem::readable(g_model_jumps + 4u * k, 4) || !image::text().contains(mem::read<uintptr_t>(g_model_jumps + 4u * k)))
      return false;
  }
  // The archive's path: the request's format, "<folder>\%s".
  const uintptr_t path = game::sig_one("kSigLoadPath");
  const uintptr_t request = game::sig_one("kSigLoadRequest");
  const uintptr_t fmt = path ? mem::read<uintptr_t>(path + 10) : 0;
  const char* f = fmt && mem::readable(fmt, 2) ? reinterpret_cast<const char*>(fmt) : nullptr;
  const size_t flen = f ? strnlen(f, 64) : 0;
  if (!request || path < request || path > request + 0x80 || !f || flen < 3 || flen >= 64 ||
      std::strcmp(f + flen - 2, "%s") != 0 || std::strchr(f, '%') != f + flen - 2)
    return false;
  g_archive_dir.assign(f, flen - 2);
  return true;
}

// The loader's request and commit (the loading of what is not loaded yet): their
// own checks of the lists' fields. Both work on the list the game chose last, which
// is not always one the mod may add to: the cutscenes' list is let go whole once a
// cutscene has played (FUN_007D4BA0), yet stays the list chosen last, and the play
// flow takes any entry loaded in it for its next cutscene, preloaded - and plays that
// one without loading it (FUN_007D51D0 in FUN_0071E600). Item archives asked into it
// at a pause after a cutscene so made the game play the next one unloaded: its first
// file missing, the game's own fatal error (the user's 5-3, 2026-10-31: "Failed open
// file ... pawn\pl\pl00\model\evf0000.mod" - Chris's face in the cutscene that damaging
// Wesker starts). So the mod asks into the area's sets' list, whichever list the game
// chose last: one the area chooses and commits at each stage's load (FUN_0072CF40),
// which lets the mod's leftovers go, as before.
bool discover_load() {
  const uintptr_t request = game::sig_one("kSigLoadRequest");
  const uintptr_t commit = game::sig_one("kSigLoadCommit");
  const uintptr_t choose = game::sig_one("kSigLoadCurrent");
  const uintptr_t preloaded = game::sig_one("kSigCutscenePreloaded");
  const uintptr_t sets = game::sig_one("kSigSetsLoad");
  if (!request || !commit || !choose || !preloaded || !sets || mem::read<uint32_t>(commit + 8) != g_ld.current ||
      mem::read<uint32_t>(commit + 26) != g_ld.count || game::call_target(sets + 11) != choose)
    return false;
  // The cutscenes' list: its count and items where the lists' are, one list apart each,
  // and the state it counts the lists' entries' state; the sets' list: another.
  const uint32_t count = mem::read<uint32_t>(preloaded + 2);
  if (count < g_ld.count || (count - g_ld.count) % g_ld.stride || mem::read<uint32_t>(preloaded + 14) != count + 0xC ||
      mem::read<uint8_t>(preloaded + 23) != g_ld.state)
    return false;
  g_ld.cutscenes = static_cast<int>((count - g_ld.count) / g_ld.stride);
  g_ld.sets = static_cast<int>(mem::read<uint8_t>(sets + 10));
  if (g_ld.cutscenes >= g_ld.lists || g_ld.sets >= g_ld.lists || g_ld.sets == g_ld.cutscenes) return false;
  g_request = reinterpret_cast<RequestFn>(request);
  g_commit = reinterpret_cast<CommitFn>(commit);
  return true;
}

// The model an item is given (the switch's: its own, or another's), and its class -
// one of uItem's, as the game itself checks before making it - or nullptr (no
// model: the add fails).
uint32_t model_of(int id) {
  const uint32_t m = static_cast<uint32_t>(id);
  if (m - g_model_lo < g_model_n &&
      mem::read<uintptr_t>(g_model_jumps + 4u * mem::read<uint8_t>(g_model_bytes + (m - g_model_lo))) != g_model_default)
    return g_model_as;
  return m;
}
const mt::Class* model_class(int id) {
  const mt::Class* c = mt::by_id(((g_model_kind << 16) | (model_of(id) & 0xFFFF)) << 8);
  return c && mt::derives(c->dti, g_uitem->dti) ? c : nullptr;
}

// The game's own save of a case, its grenade launcher (kSigSaveLauncher; after
// discover_area): the launcher's id the switch's, the model's field cItem's, the class
// the model's record; each case of its jump table either the end (the id kept) or `mov
// ebx, <load>` on to the end, each load one the switch gives the launcher's model.
bool discover_launcher() {
  const uintptr_t at = game::sig_one("kSigSaveLauncher");
  const mt::Class* cls = mt::find("uWpLauncher");
  if (!at || !cls) return false;
  const uintptr_t end = at + 45 + static_cast<int8_t>(mem::read<uint8_t>(at + 44));    // past the kinds: kept
  const uintptr_t other = at + 29 + static_cast<int8_t>(mem::read<uint8_t>(at + 28));  // not the class: kept
  const uint32_t id = mem::read<uint32_t>(at + 2), rounds = mem::read<uint32_t>(at + 31);
  const uint32_t lo = 0u - mem::read<uint32_t>(at + 36), n = mem::read<uint8_t>(at + 42) + 1u;
  const uintptr_t bytes = mem::read<uint32_t>(at + 48), jumps = mem::read<uint32_t>(at + 55);
  if (id != g_model_as || mem::read<uint8_t>(at + 10) != game::layout().it_model ||
      mem::read<uint32_t>(at + 16) != cls->dti || rounds < 4 || rounds + 4 > cls->size || other != end ||
      n > kMaxRounds || !image::text().contains(end) || !mem::readable(bytes, n))
    return false;
  uint32_t load_of[kMaxRounds] = {};
  int loads = 0;
  for (uint32_t k = 0; k < n; ++k) {
    const uintptr_t slot = jumps + 4u * mem::read<uint8_t>(bytes + k);
    if (!mem::readable(slot, 4)) return false;
    const uintptr_t to = mem::read<uintptr_t>(slot);
    if (to == end) continue;
    const uintptr_t next = to + 5;  // a jmp rel8 to the end, or the end itself (the last case)
    if (!image::text().contains(to) || mem::read<uint8_t>(to) != 0xBB ||
        (next != end &&
         (mem::read<uint8_t>(next) != 0xEB || next + 2 + static_cast<int8_t>(mem::read<uint8_t>(next + 1)) != end)))
      return false;
    load_of[k] = mem::read<uint32_t>(to + 1);
    if (load_of[k] == id || model_of(static_cast<int>(load_of[k])) != id) return false;
    ++loads;
  }
  if (!loads) return false;
  std::copy(load_of, load_of + kMaxRounds, g_load_of);
  g_launcher = id;
  g_rounds = rounds;
  g_rounds_lo = lo;
  g_rounds_n = n;
  g_launcher_cls = cls;
  return true;
}

// Each entry of the loader's lists - only list `only`'s when it is >= 0 - with its state.
template <typename Each>
void entries(uintptr_t lm, int only, Each&& each) {
  for (int k = 0; k < g_ld.lists; ++k) {
    if (only >= 0 && k != only) continue;
    const uint32_t at = static_cast<uint32_t>(k) * g_ld.stride;
    const int n = mem::read<int32_t>(lm + g_ld.count + at);
    const uintptr_t items = mem::read<uintptr_t>(lm + g_ld.items + at);
    if (n <= 0 || n > 0x4000 || !mem::readable(items, 4u * static_cast<uint32_t>(n))) continue;
    for (int i = 0; i < n; ++i) {
      const uintptr_t e = mem::read<uintptr_t>(items + 4u * static_cast<uint32_t>(i));
      if (e && mem::readable(e, std::max(g_ld.archive, g_ld.state) + 4)) each(e, mem::read<uint32_t>(e + g_ld.state));
    }
  }
}

// The class an entry's archive is for - its path is "<folder>\<class>" - or false.
bool entry_class(uintptr_t e, std::string* out) {
  const size_t room = g_ld.archive - g_ld.path, dir = g_archive_dir.size();
  const char* p = reinterpret_cast<const char*>(e + g_ld.path);
  const size_t len = strnlen(p, room);
  if (len <= dir || len >= room || std::strncmp(p, g_archive_dir.c_str(), dir) != 0) return false;
  out->assign(p + dir, len - dir);
  return true;
}

// The classes whose archives a list holds, whatever their state.
std::vector<std::string> names_in(uintptr_t lm, int list) {
  std::vector<std::string> out;
  std::string s;
  entries(lm, list, [&](uintptr_t e, uint32_t) {
    if (entry_class(e, &s)) out.push_back(s);
  });
  std::sort(out.begin(), out.end());
  return out;
}

// The classes whose archives an item needs to go in a case: its model's, and its own
// too when the switch gives it another's model (the grenade launcher's rounds).
std::vector<const mt::Class*> needs(int id) {
  std::vector<const mt::Class*> out;
  if (const mt::Class* m = model_class(id)) out.push_back(m);
  if (model_of(id) != static_cast<uint32_t>(id))
    if (const mt::Class* own = mt::by_id(((g_model_kind << 16) | (static_cast<uint32_t>(id) & 0xFFFF)) << 8))
      out.push_back(own);
  return out;
}

// Ids already asked for at this pause (preloads are not asked for twice in one).
std::vector<int> g_preloaded;
constexpr int kItemArchivesMax = 64;  // the items' and weapons' archives loaded at once, past which nothing is preloaded

// The archives sLoadMan holds loaded now, by class name.
void read_loaded() {
  g_loaded.clear();
  const uintptr_t lm = game::system("sLoadMan");
  if (!g_area_ok || !lm) return;
  std::string s;
  entries(lm, -1, [&](uintptr_t e, uint32_t) {
    const uintptr_t arc = mem::read<uintptr_t>(e + g_ld.archive);
    if (!arc || !mem::readable(arc + g_ld.flags, 4)) return;
    const uint32_t f = mem::read<uint32_t>(arc + g_ld.flags);
    if ((f & kLoaded) && !(f & kFailed) && entry_class(e, &s)) g_loaded.push_back(s);
  });
  std::sort(g_loaded.begin(), g_loaded.end());
}

bool loaded(const mt::Class* c) {
  if (!c) return false;
  const auto it = std::lower_bound(g_loaded.begin(), g_loaded.end(), c->name,
                                   [](const std::string& a, const char* b) { return a.compare(b) < 0; });
  return it != g_loaded.end() && it->compare(c->name) == 0;
}

const mt::Class* g_item_model[items::kCount] = {};  // items.h's, at discovery

// The items' and weapons' archives loaded here, in the log as a pause opens - when
// they are not the ones logged last.
void log_loaded() {
  static std::string last = "-";
  std::string names;
  int n = 0;
  for (const std::string& s : g_loaded)
    if (s.rfind("uWp", 0) == 0 || s.rfind("uIt", 0) == 0) {
      names += n++ ? " " : "";
      names += s;
    }
  if (names == last) return;
  last = names;
  logf("inventory: %d archives loaded in this area; of items (%d): %.600s", static_cast<int>(g_loaded.size()), n,
       n ? names.c_str() : "none");
}

}  // namespace

bool in_area(int id) { return g_area_ok && loaded(model_class(id)); }

void refresh_area() {
  read_loaded();
  g_loaded_age = 0;
}

Avail avail(int id) {
  if (in_area(id)) return kHere;
  return g_load_ok && model_class(id) ? kLoadable : kNever;
}

bool load(const std::vector<int>& ids) {
  std::vector<int> ask;
  bool all = true;  // every item loaded or asked for
  for (const int id : ids) {
    if (ready(id)) continue;
    if (needs(id).empty()) {  // no model: nothing to load, and never in a case
      all = false;
      continue;
    }
    if (std::find(ask.begin(), ask.end(), id) == ask.end()) ask.push_back(id);
  }
  if (ask.empty()) return all;
  const uintptr_t lm = game::system("sLoadMan");
  if (!g_load_ok || !lm) return false;
  // A commit lets go of what its list holds that was not asked for again (state 0),
  // and a request into the sets' list makes such an entry of the characters' loaded
  // again (FUN_007D4960): only while no list is being chosen.
  const uint32_t cur = mem::read<uint32_t>(lm + g_ld.current);
  bool settled = cur < static_cast<uint32_t>(g_ld.lists);
  if (settled) entries(lm, -1, [&](uintptr_t, uint32_t s) { settled = settled && s != kAgain; });
  if (!settled) {
    logf("inventory: %d item(s) not asked for - the game is choosing an area's archives", static_cast<int>(ask.size()));
    return false;
  }
  // Each item's model's class and what that needs (the game's own request), the
  // model's own too when the item takes another's; one commit for them all - into the
  // area's sets' list, never the cutscenes' (see discover_load); the list chosen last
  // is put back after, as the game left it.
  const std::vector<std::string> before = names_in(lm, g_ld.sets);
  mem::write<uint32_t>(lm + g_ld.current, static_cast<uint32_t>(g_ld.sets));
  for (const int id : ask) {
    g_request(reinterpret_cast<void*>(lm), static_cast<int>(g_model_kind), id, 0, 0);
    if (model_of(id) != static_cast<uint32_t>(id))
      g_request(reinterpret_cast<void*>(lm), static_cast<int>(g_model_kind), static_cast<int>(model_of(id)), 0, 0);
  }
  const int asked = g_commit(reinterpret_cast<void*>(lm));
  mem::write<uint32_t>(lm + g_ld.current, cur);
  std::string list;
  for (const std::string& s : names_in(lm, g_ld.sets))
    if (!std::binary_search(before.begin(), before.end(), s)) {
      list += list.empty() ? "" : " ";
      list += s;
    }
  logf("inventory: the game's loader asked for %d item(s) into the area's sets' list (%d; the list chosen last, %u, "
       "as it was): %d new archive(s) (%s)",
       static_cast<int>(ask.size()), g_ld.sets, cur, asked, list.empty() ? "none - already asked for" : list.c_str());
  return all;
}

bool load(int id) { return load(std::vector<int>{id}); }

bool ready(int id) {
  const std::vector<const mt::Class*> n = needs(id);
  return g_area_ok && !n.empty() && std::all_of(n.begin(), n.end(), [](const mt::Class* c) { return loaded(c); });
}

void preload(std::vector<int> ids) {
  // Not twice at one pause; up to the cap on the items' and weapons' archives loaded at once.
  ids.erase(std::remove_if(ids.begin(), ids.end(),
                           [](int id) {
                             return ready(id) || needs(id).empty() ||
                                    std::find(g_preloaded.begin(), g_preloaded.end(), id) != g_preloaded.end();
                           }),
            ids.end());
  if (ids.empty()) return;
  const int held = static_cast<int>(std::count_if(g_loaded.begin(), g_loaded.end(), [](const std::string& s) {
    return s.rfind("uWp", 0) == 0 || s.rfind("uIt", 0) == 0;
  }));
  const size_t room = static_cast<size_t>(std::max(kItemArchivesMax - held, 0));
  g_preloaded.insert(g_preloaded.end(), ids.begin(), ids.end());  // each considered once a pause, loaded or not
  if (room < ids.size()) {
    logf("inventory: %d item archives loaded here - %d of %d to preload left for when they are taken", held,
         static_cast<int>(ids.size() - room), static_cast<int>(ids.size()));
    ids.resize(room);
  }
  if (!ids.empty()) load(ids);
}

bool carried(int k) { return !g_area_ok || (k >= 0 && k < items::kCount && g_item_model[k] != nullptr); }

int model_id(int id) { return static_cast<int>(model_of(id)); }

int as_saved(const game::Item& it) {
  if (!g_launcher_cls || static_cast<uint32_t>(it.id) != g_launcher || !it.at) return it.id;
  const uintptr_t model = mem::read<uintptr_t>(it.at + game::layout().it_model);
  if (!model || !mt::is_a(model, g_launcher_cls)) return it.id;
  const uint32_t k = mem::read<uint32_t>(model + g_rounds) - g_rounds_lo;
  return k < g_rounds_n && g_load_of[k] ? static_cast<int>(g_load_of[k]) : it.id;
}

bool lists_known() { return g_area_ok; }
const std::string& archive_dir() { return g_archive_dir; }

bool archive_loaded(const char* path) {
  const uintptr_t lm = game::system("sLoadMan");
  if (!g_area_ok || !lm || !path) return false;
  const size_t room = g_ld.archive - g_ld.path;
  bool found = false;
  entries(lm, -1, [&](uintptr_t e, uint32_t) {
    if (found || std::strncmp(reinterpret_cast<const char*>(e + g_ld.path), path, room) != 0) return;
    const uintptr_t arc = mem::read<uintptr_t>(e + g_ld.archive);
    if (!arc || !mem::readable(arc + g_ld.flags, 4)) return;
    const uint32_t f = mem::read<uint32_t>(arc + g_ld.flags);
    found = (f & kLoaded) && !(f & kFailed);
  });
  return found;
}

void init() {
  if (g_lock_ready) return;
  InitializeCriticalSection(&g_lock);
  g_lock_ready = true;
}

bool discover() {
  const uintptr_t clear = game::sig_one("kSigItemClear");
  const uintptr_t tail = game::sig_one("kSigItemAdd");
  const uintptr_t grid = game::sig_one("kSigGridAdd");
  const uintptr_t add = tail ? tail - 0x59 : 0;
  if (!clear || !add || !grid || !mem::matches(add, mem::parse_pattern("8B 44 24 48 8B 54 24 44 50")) ||
      mem::read<uint8_t>(clear + 37) != game::layout().it_alive) {
    logf("inventory: the inventory's own add or clear not found - counts only");
    return false;
  }
  g_add = reinterpret_cast<AddFn>(add);
  g_clear = reinterpret_cast<ClearFn>(clear);
  g_hand_field = mem::read<uint32_t>(grid + 2);
  logf("inventory: the inventory's own add exe+0x%X and clear exe+0x%X (the grid's add hands on the character's +0x%X)",
       static_cast<unsigned>(add - 0x400000), static_cast<unsigned>(clear - 0x400000), g_hand_field);
  g_area_ok = discover_area();
  if (!g_area_ok) {
    logf("inventory: what an area has loaded was not understood - nothing goes into a case in play");
    return true;
  }
  g_load_ok = discover_load();
  int models = 0;
  for (int k = 0; k < items::kCount; ++k) models += (g_item_model[k] = model_class(items::kItems[k].id)) != nullptr;
  const mt::Class* mp5 = model_class(0x113);
  logf("inventory: sLoadMan's %d lists (count +0x%X, items +0x%X, 0x%X apart, the one added to +0x%X; an entry's "
       "path +0x%X, archive +0x%X, state +0x%X; the archive's flags +0x%X; paths \"%s<class>\"); a model's class keyed "
       "by kind %u, ids 0x%X..0x%X switched (to 0x%X's model where not the default); %d of %d items have a model class "
       "(the MP5's: %s); %s",
       g_ld.lists, g_ld.count, g_ld.items, g_ld.stride, g_ld.current, g_ld.path, g_ld.archive, g_ld.state, g_ld.flags,
       g_archive_dir.c_str(), g_model_kind, g_model_lo, g_model_lo + g_model_n - 1, g_model_as, models, items::kCount,
       mp5 ? mp5->name : "none",
       g_load_ok ? "the loader's own request and commit load what is not loaded"
                 : "the loader's request or commit not found - only what is loaded goes into a case in play");
  if (g_load_ok)
    logf("inventory: the mod asks the loader into the area's sets' list (%d: chosen and committed at each stage's "
         "load), never the cutscenes' (%d: any entry loaded there is taken for the next cutscene, preloaded)",
         g_ld.sets, g_ld.cutscenes);
  if (discover_launcher()) {
    char loads[160];
    int at = 0;
    for (uint32_t k = 0; k < g_rounds_n && at < static_cast<int>(sizeof(loads)) - 16; ++k)
      if (g_load_of[k])
        at += std::snprintf(loads + at, sizeof(loads) - at, "%s0x%X->0x%X", at ? " " : "", g_rounds_lo + k, g_load_of[k]);
    logf("inventory: the game's save of a case writes the grenade launcher (0x%X, a %s model) as its load, by the "
         "rounds the model keeps at +0x%X: %s",
         g_launcher, g_launcher_cls->name, g_rounds, loads);
  } else {
    logf("inventory: the save's grenade launcher loads were not understood - a launcher stays out of the item box");
  }
  return true;
}

bool can_change() { return g_add && g_clear; }

uintptr_t add_item(uintptr_t chara, int id, int count, const uint8_t lv[9]) {
  if (!g_add || !chara || !in_area(id)) return 0;
  void* mgr = reinterpret_cast<void*>(chara + game::layout().case_mgr);
  return g_add(mgr, static_cast<uint32_t>(id), count, -1, -1, -1, kAnySlot, lv[0], lv[1], lv[2], lv[3], lv[4], lv[5],
               lv[6], lv[7], lv[8], reinterpret_cast<void*>(chara), mem::read<uint32_t>(chara + g_hand_field), 0);
}

bool clear_item(uintptr_t item) {
  if (!g_clear || !item) return false;
  g_clear(reinterpret_cast<void*>(item), 1);
  return true;
}

void tick(const game::Run& run) {
  if (!g_lock_ready) return;
  View v;
  // Chapter Select on its way from the pause: as if not paused - nothing changed, nothing loaded.
  v.paused = run.screen == game::Screen::kPause && !menus::leaving();
  std::vector<Request> reqs;
  EnterCriticalSection(&g_lock);
  reqs.swap(g_requests);
  std::snprintf(v.note, sizeof(v.note), "%s", g_view.note);
  LeaveCriticalSection(&g_lock);

  const game::Layout& l = game::layout();
  v.can_add = can_change();
  v.area_known = g_area_ok;
  // What the area has loaded, read as the pause opens and now and then while it is
  // up (nothing is loaded while the game stands still), and before a change.
  if (!v.paused) {
    g_loaded_age = -1;
    g_preloaded.clear();
  } else if (g_loaded_age < 0) {
    refresh_area();
    log_loaded();
  } else if (++g_loaded_age >= 30 || !reqs.empty() || !g_waiting.empty() ||
             std::any_of(g_preloaded.begin(), g_preloaded.end(), [](int id) { return !ready(id); })) {
    refresh_area();  // (every frame while something asked for is still loading)
  }
  // Adds waiting for the loader: made once their archives are in, while the pause
  // lasts (in front of this tick's own requests).
  {
    std::vector<Request> now;
    for (auto it = g_waiting.begin(); it != g_waiting.end();) {
      const items::Info* info = items::find(static_cast<uint32_t>(it->id));
      const char* name = info ? info->name : "?";
      char text[160];
      if (!v.paused) {
        std::snprintf(text, sizeof(text), "%s was not added: the pause closed while it was loading.", name);
        note(v, text);
      } else if (ready(it->id)) {
        logf("inventory: %s loaded after %d frame(s)", name, it->age);
        now.push_back(Request{Op::kAdd, it->chara, 0, it->count, it->id});
      } else if (++it->age > kLoadTicks) {
        std::snprintf(text, sizeof(text), "%s was not added: the game did not load it (see the log).", name);
        note(v, text);
        logf("inventory: %s not loaded after %d frames - not added", name, it->age);
      } else {
        ++it;
        continue;
      }
      it = g_waiting.erase(it);
    }
    reqs.insert(reqs.begin(), now.begin(), now.end());
  }
  for (const Request& r : reqs) {
    if (r.op == Op::kPreload) {  // an item picked in an Add list: its archives while it waits to be added
      if (v.paused) preload({r.id});
      continue;
    }
    const game::Character* c = nullptr;
    for (int i = 0; i < run.nchars; ++i)
      if (run.chars[i].index == r.chara) c = &run.chars[i];
    if (!v.paused || !c) {
      note(v, v.paused ? "That character is gone." : "Pause the game to change it.");
      continue;
    }
    if (r.op == Op::kAdd) {
      const items::Info* info = items::find(static_cast<uint32_t>(r.id));
      if (!info || !info->offer || !g_add) continue;  // only what the Add lists offer
      // A weapon the game keeps for one character (Item Management's own check: the
      // Gatling gun is Chris's, the Longbow Sheva's) goes to no one else.
      if (info->only && std::strcmp(info->only, c->name) != 0) {
        char text[160];
        std::snprintf(text, sizeof(text), "Only %s can have the %s.", info->only, info->name);
        note(v, text);
        logf("inventory: %s not added to %s's case - the game keeps it for %s", info->name, c->name, info->only);
        continue;
      }
      if (!in_area(info->id)) {
        // Not loaded in this area: the game's loader first, the add once it is in.
        char text[160];
        if (load(info->id)) {
          g_waiting.push_back(Waiting{r.chara, info->id, r.count, 0});
          std::snprintf(text, sizeof(text), "Loading %s...", info->name);
        } else {
          std::snprintf(text, sizeof(text), "%s cannot be added during play (hover it for why).", info->name);
          logf("inventory: %s not added to %s's case - its model cannot be loaded here", info->name, c->name);
        }
        note(v, text);
        continue;
      }
      const int count = info->cat == 1 ? kFullLoad : r.count < 1 ? 1 : r.count;
      void* mgr = reinterpret_cast<void*>(c->obj + l.case_mgr);
      const uintptr_t made = g_add(mgr, info->id, count, -1, -1, -1, kAnySlot, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   reinterpret_cast<void*>(c->obj), mem::read<uint32_t>(c->obj + g_hand_field), 0);
      if (!made) {
        note(v, "There is no room in that inventory.");
        logf("inventory: %s's case has no room for %s", c->name, info->name);
        continue;
      }
      // A stack's count within what one slot holds.
      const auto most = mem::read<int32_t>(made + l.it_most);
      if (info->cat != 1 && most > 0 && mem::read<int32_t>(made + l.it_num) > most)
        mem::write<int32_t>(made + l.it_num, most);
      logf("inventory: %s %s added to %s's case (slot %d, count %d)", info->name,
           info->cat == 1 ? "(loaded)" : "", c->name, mem::read<int32_t>(made + l.it_slot),
           mem::read<int32_t>(made + l.it_num));
      v.note[0] = '\0';
      continue;
    }
    game::Item it;
    if (!game::item_in(c->obj, r.slot, &it)) {
      note(v, "That slot is empty now.");
      continue;
    }
    if (r.op == Op::kThrow) {
      if (!g_clear) continue;
      if (it.equip == 1) {
        note(v, "That is in their hand: change weapons first.");
        continue;
      }
      const items::Info* info = items::find(static_cast<uint32_t>(it.id));
      g_clear(reinterpret_cast<void*>(it.at), 1);
      logf("inventory: %s thrown away from %s's slot %d", info ? info->name : "?", c->name, r.slot);
      v.note[0] = '\0';
      continue;
    }
    // A count within what the item holds: a gun's rounds up to its capacity, a
    // stack up to its most; never below 1 for what is used up at 0 (ammo, herbs).
    const items::Info* info = items::find(static_cast<uint32_t>(it.id));
    const bool rounds = info && info->magazine;
    int lo = rounds ? 0 : 1, hi = it.most > 0 ? it.most : 1;
    int n = r.count < lo ? lo : r.count > hi ? hi : r.count;
    mem::write<uint32_t>(it.at + l.it_num, static_cast<uint32_t>(n));
    logf("inventory: %s's slot %d (%s) count %d -> %d", c->name, r.slot, info ? info->name : "?", it.num, n);
    v.note[0] = '\0';
  }

  if (v.paused && g_area_ok)
    for (int k = 0; k < items::kCount; ++k)
      v.avail[k] = loaded(g_item_model[k]) ? kHere : g_load_ok && g_item_model[k] ? kLoadable : kNever;
  v.loading = !g_waiting.empty();
  for (int i = 0; i < run.nchars && v.ncases < game::kMaxChars; ++i) {
    const game::Character& c = run.chars[i];
    Case& cs = v.cases[v.ncases++];
    cs.name = c.name;
    cs.control = c.control;
    cs.index = c.index;
    for (int s = 0; s < game::kSlots; ++s) {
      game::Item it;
      if (!game::item_in(c.obj, s, &it)) continue;
      Slot& sl = cs.slot[s];
      sl.used = true;
      sl.id = it.id;
      sl.num = it.num;
      sl.most = it.most;
      sl.equip = it.equip;
      for (int k = 0; k < 9; ++k) sl.lv[k] = it.lv[k];
      const int kept = as_saved(it);
      sl.kept_as = kept != it.id ? kept : 0;
    }
  }
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

void set_count(int chara_index, int slot, int count) {
  if (!g_lock_ready) return;
  EnterCriticalSection(&g_lock);
  g_requests.push_back(Request{Op::kCount, chara_index, slot, count, 0});
  LeaveCriticalSection(&g_lock);
}

void add(int chara_index, int id, int count) {
  if (!g_lock_ready) return;
  EnterCriticalSection(&g_lock);
  g_requests.push_back(Request{Op::kAdd, chara_index, 0, count, id});
  LeaveCriticalSection(&g_lock);
}

void preload_soon(int id) {
  if (!g_lock_ready) return;
  EnterCriticalSection(&g_lock);
  g_requests.push_back(Request{Op::kPreload, 0, 0, 0, id});
  LeaveCriticalSection(&g_lock);
}

void throw_away(int chara_index, int slot) {
  if (!g_lock_ready) return;
  EnterCriticalSection(&g_lock);
  g_requests.push_back(Request{Op::kThrow, chara_index, slot, 0, 0});
  LeaveCriticalSection(&g_lock);
}

}  // namespace re5cc::inventory
