#include "game.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "image.h"
#include "log.h"
#include "mem.h"
#include "mt.h"
#include "sigs.h"
#include "units.h"

namespace re5cc::game {
namespace {

bool g_ready = false;
char g_status[160] = "waiting for the game's code to be unpacked";
Layout g_lay;

const mt::Class* g_cls_system = nullptr;  // cSystem: every s* singleton derives from it
const mt::Class* g_cls_main = nullptr;    // sBioMain
const mt::Class* g_cls_player = nullptr;  // uPlayer: Chris, Sheva and the Mercenaries' others
const mt::Class* g_cls_pause = nullptr;   // uIdPause00: the pause menu
const mt::Class* g_cls_over = nullptr;    // uIdGameover00: the Game Over screen (menus.cpp says if it is understood)
const mt::Class* g_cls_results = nullptr; // uIdChapEd00: the chapter's results (menus.cpp says if they are understood)
const mt::Class* g_cls_merce = nullptr;   // uIdMerce00: the Mercenaries' HUD (its clock)
const mt::Class* g_cls_items = nullptr;   // uIdItemBox00: Item Management (made as it opens, gone as it closes)
std::vector<const mt::Class*> g_cls_vehicles;  // uOmJeep and the classes under it (the game makes uOm1304)
uint32_t g_pause_open = 0;                // uIdPause00: its "open" byte
uintptr_t g_sunit = 0, g_splayer = 0, g_sinfo = 0;
// The play state (cGameSys, which sBioMain holds) and its mode: 0 when not understood.
const mt::Class* g_cls_gamesys = nullptr;
uint32_t g_gamesys = 0, g_mode = 0;

// The screens up last tick (uId* classes), for the trace of what comes and goes.
std::vector<const mt::Class*> g_screens_prev;

// The systems the main system holds, by class name (from the first tick).
std::map<std::string, uintptr_t> g_systems;
bool g_systems_seen = false;

// --- signatures ---------------------------------------------------------------------------
struct Sig {
  const char* name;
  const char* pattern;
  int matches;
};
#define RE5CC_SIG_ENTRY(n, p, m) {#n, p, m},
constexpr Sig kSigs[] = {RE5CC_SIGS(RE5CC_SIG_ENTRY)};
#undef RE5CC_SIG_ENTRY

const Sig* sig(const char* name) {
  for (const Sig& s : kSigs)
    if (!std::strcmp(s.name, name)) return &s;
  return nullptr;
}

uint32_t u8at(uintptr_t a) { return mem::read<uint8_t>(a); }
uint32_t u32at(uintptr_t a) { return mem::read<uint32_t>(a); }
uintptr_t vslot(const mt::Class* c, int slot) { return c && c->vtable ? mem::read<uintptr_t>(c->vtable + 4 * slot) : 0; }

// What failed, for the panel and the log.
bool fail(const char* what) {
  std::snprintf(g_status, sizeof(g_status), "%s was not found in the game (see the log)", what);
  logf("ERROR: game: %s not found or not as known - the mod stays off", what);
  return false;
}

// Every pointer in the main system's instance to an object of a cSystem class:
// MT Framework's singletons, each named by its class record.
void survey_systems(uintptr_t main) {
  g_systems_seen = true;
  const uint32_t size = g_cls_main ? g_cls_main->size : 0;
  if (!size || !mem::readable(main, size)) {
    logf("game: the main system %08X is not readable over its %u bytes - no systems", static_cast<unsigned>(main), size);
    return;
  }
  const mem::Range img = image::whole();
  int found = 0;
  for (uint32_t off = 0; off + 4 <= size; off += 4) {
    const auto v = mem::read<uintptr_t>(main + off);
    if (!v || (v & 3) || img.contains(v) || v == main) continue;
    uintptr_t vt = 0;
    if (!mem::read_safe(v, &vt) || !image::rdata().contains(vt)) continue;
    const uintptr_t dti = mt::dti_of_object(v);
    if (!dti || !g_cls_system || !mt::derives(dti, g_cls_system->dti)) continue;
    const char* name = mt::class_name_of(v);
    if (!name || g_systems.count(name)) continue;
    g_systems[name] = v;
    ++found;
    if (config::get().trace) logf("game: system %-24s %08X (sBioMain+0x%X)", name, static_cast<unsigned>(v), off);
  }
  std::string names;
  for (const auto& kv : g_systems) {
    if (!names.empty()) names += ", ";
    names += kv.first;
  }
  logf("game: the main system %08X holds %d systems: %s", static_cast<unsigned>(main), found, names.c_str());
  g_sunit = system("sUnit");
  g_splayer = system("sPlayer");
  g_sinfo = system("sInfoMan");
  if (!g_sunit || !g_splayer || !g_sinfo)
    logf("ERROR: game: sUnit %s, sPlayer %s, sInfoMan %s", g_sunit ? "found" : "missing",
         g_splayer ? "found" : "missing", g_sinfo ? "found" : "missing");
}

// --- discovery of the layout ------------------------------------------------------------
// The pause menu is a unit the chapter makes at its start and keeps all through
// it (the area's start, 0x720A50); the menu is open while a byte of it is set.
// Its draw (vtable slot 9) does nothing while that byte is 0
//   sub esp,8; push esi; mov esi,ecx; cmp byte [esi+open],0; je
// and its frame (slot 6) ends by closing it - a call to a function that tests the
// same byte first - and going back to waiting
//   mov ecx,esi; call close; mov byte [esi+step],0; pop esi; ret
//   close: push edi; mov edi,ecx; cmp byte [edi+open],0; je
bool discover_pause() {
  const uintptr_t draw = vslot(g_cls_pause, 9), frame = vslot(g_cls_pause, 6);
  if (!image::text().contains(draw) || !image::text().contains(frame) ||
      !mem::matches(draw, mem::parse_pattern("83 EC 08 56 8B F1 80 7E ?? 00 0F 84")))
    return fail("the pause menu's open state");
  g_pause_open = u8at(draw + 8);
  const uintptr_t tail = mem::find_pattern(mem::Range{frame, frame + 0x90}, "8B CE E8 ?? ?? ?? ?? C6 46 ?? 00 5E C3");
  const uintptr_t close = tail ? call_target(tail + 2) : 0;
  if (!image::text().contains(close) || !mem::matches(close, mem::parse_pattern("57 8B F9 80 7F ?? 00 0F 84")) ||
      u8at(close + 5) != g_pause_open || g_pause_open >= g_cls_pause->size)
    return fail("the pause menu's open state (its draw and its close disagree)");
  logf("game: the pause menu (uIdPause00, made with the chapter) is open while its +0x%X is set", g_pause_open);
  return true;
}

bool discover_characters() {
  const mt::Class* chara = mt::find("uCharacter");
  const mt::Class* enemy = mt::find("uEnemy");
  // Health: uCharacter's set-health, its vtable slot 67.
  const uintptr_t set_life = sig_one("kSigSetLife");
  if (!set_life || set_life != vslot(chara, 67)) return fail("the characters' health");
  g_lay.hp = u32at(set_life + 11);
  g_lay.hp_max = u32at(set_life + 29);
  if (u32at(set_life + 20) != g_lay.hp || u32at(set_life + 36) != g_lay.hp || u32at(set_life + 45) != g_lay.hp ||
      g_lay.hp_max != g_lay.hp + 2)
    return fail("the characters' health (its fields disagree)");
  // Who drives a player: uPlayer's damage, slot 66, turns the network partner away.
  const uintptr_t pdmg = sig_one("kSigPlayerDamage");
  if (!pdmg || pdmg != vslot(g_cls_player, 66)) return fail("the players' damage");
  g_lay.control = u32at(pdmg + 5);
  // The enemies' damage (slot 66) reads the same health and goes on through set-health's
  // sibling, subtract-health (slot 69).
  const uintptr_t edmg = sig_one("kSigEnemyDamage");
  if (!edmg || edmg != vslot(enemy, 66) || u32at(edmg + 37) != g_lay.hp || u32at(edmg + 17) != 69 * 4)
    return fail("the enemies' damage");
  // sPlayer's characters.
  const uintptr_t is_player = sig_one("kSigIsPlayer");
  const mt::Class* sp = mt::find("sPlayer");
  if (!is_player || !sp) return fail("sPlayer's characters");
  g_lay.players = u8at(is_player + 12);
  g_lay.nplayers = static_cast<int>(u8at(is_player + 28));
  if (g_lay.nplayers < 1 || g_lay.nplayers > kMaxChars || g_lay.players + 4 * g_lay.nplayers > sp->size)
    return fail("sPlayer's characters (they do not fit it)");
  logf("game: health +0x%X (most +0x%X), who drives a player +0x%X, sPlayer's %d characters +0x%X", g_lay.hp,
       g_lay.hp_max, g_lay.control, g_lay.nplayers, g_lay.players);
  return true;
}

// The jeep of chapter 2-3 (uOmJeep < uObjModel) is a pawn, as the characters are:
// its health (3500) is kept in their fields when its set-health (slot 67) is
// theirs. Its classes are noted for the tick, which finds it among the units. Not
// a failure when it is not so: God mode then leaves the jeep alone.
void discover_vehicles() {
  const mt::Class* jeep = mt::find("uOmJeep");
  if (!jeep || vslot(jeep, 67) != vslot(mt::find("uCharacter"), 67)) {
    logf("game: the jeep (uOmJeep) %s - God mode leaves its health alone",
         jeep ? "does not keep its health as the characters do" : "was not found");
    return;
  }
  int count = 0;
  const mt::Class* all = mt::classes(&count);
  for (int i = 0; i < count; ++i)
    if (mt::derives(all[i].dti, jeep->dti)) g_cls_vehicles.push_back(&all[i]);
  logf("game: the jeep (uOmJeep, %d classes) keeps its health as the characters do",
       static_cast<int>(g_cls_vehicles.size()));
}

// The game's mode, in its play state (cGameSys, a field of sBioMain): the play flow's save at
// a checkpoint asks the game's own test of the extra episodes - the mode 3 or 4 - before it
// commits the profile's data over their checkpoint copies, which it never does in them. The
// field comes out of that call's setup, the mode's offset out of the test, whose compares are
// the two episodes' modes; cGameSys's record holds both, and the tick reads the mode only from
// an object of that class. Without it the mode stays -1 (the item box's at the pause goes by it).
void discover_mode() {
  const uintptr_t at = sig_one("kSigEpisodeCommit");
  g_cls_gamesys = mt::find("cGameSys");
  const uintptr_t test = at ? call_target(at + 19) : 0;
  char pattern[96];
  std::snprintf(pattern, sizeof(pattern), "8B 41 ?? 83 F8 %02X 74 08 83 F8 %02X 74 03 33 C0 C3 B8 01 00 00 00 C3",
                kModeNightmares, kModeEscape);
  if (!at || !g_cls_gamesys || !image::text().contains(test) || !mem::matches(test, mem::parse_pattern(pattern)) ||
      u32at(at + 15) + 4 > g_cls_main->size || u8at(test + 2) + 4 > g_cls_gamesys->size) {
    g_cls_gamesys = nullptr;
    logf("game: the game's mode (its play state's) was not found - no item box at the pause");
    return;
  }
  g_gamesys = u32at(at + 15);
  g_mode = u8at(test + 2);
  logf("game: the game's mode cGameSys +0x%X (sBioMain +0x%X; %d the story, %d and %d the extra episodes - the game's "
       "own test of them exe+0x%X)",
       g_mode, g_gamesys, kModeStory, kModeNightmares, kModeEscape, static_cast<unsigned>(test - image::base()));
}

bool discover_items() {
  const mt::Class* item = mt::find("cItem");
  const mt::Class* mgr = mt::find("cItemManager");
  const uintptr_t set = sig_one("kSigItemSet");
  const uintptr_t model = sig_one("kSigItemModel");
  const uintptr_t alive = sig_one("kSigItemAlive");
  const uintptr_t load = sig_one("kSigCaseLoad");
  const std::vector<uintptr_t> ctor = sig_all("kSigItemMgrCtor");
  if (!item || !mgr || !set || !model || !alive || !load || ctor.empty()) return fail("the inventory");
  g_lay.it_id = u8at(set + 2);
  g_lay.it_num = u8at(set + 9);
  // The nine levels in memory order (the cItem's own names for them: Power,
  // FireSpeed, ReloadSpeed, BulletMax, BulrRate, Critical, Through, Range, Scope),
  // each where cItem's set stores it.
  const uint32_t lv_at[9] = {28, 34, 46, 40, 52, 58, 64, 91, 94};
  for (int i = 0; i < 9; ++i) g_lay.it_lv[i] = u8at(set + lv_at[i]);
  g_lay.it_most = u8at(set + 71);
  g_lay.it_slot = u8at(set + 77);
  g_lay.it_equip = u8at(set + 84);
  g_lay.it_model = u8at(model + 5);
  g_lay.it_alive = u8at(alive + 16);
  g_lay.model_owner = u32at(alive + 2);
  g_lay.item_size = item->size;
  g_lay.items = u8at(ctor[0] + 8);
  g_lay.nitems = static_cast<int>(u32at(ctor[0] + 10)) + 1;
  g_lay.case_mgr = u32at(load + 2);
  bool levels_ok = true;
  for (int i = 0; i < 9; ++i) levels_ok = levels_ok && g_lay.it_lv[i] == g_lay.it_lv[0] + static_cast<uint32_t>(i);
  if (u8at(model + 8) != g_lay.it_slot || !levels_ok || g_lay.it_alive >= g_lay.item_size ||
      g_lay.it_model + 4 > g_lay.item_size || g_lay.items + g_lay.nitems * g_lay.item_size > mgr->size ||
      g_lay.case_mgr + mgr->size > g_cls_player->size)
    return fail("the inventory (its fields disagree)");
  logf("game: a character's case at +0x%X: %d items of %u bytes from +0x%X (id +0x%X, count +0x%X, most +0x%X, slot "
       "+0x%X, equipped +0x%X, levels +0x%X.., alive +0x%X, model +0x%X; the model's owner +0x%X)",
       g_lay.case_mgr, g_lay.nitems, g_lay.item_size, g_lay.items, g_lay.it_id, g_lay.it_num, g_lay.it_most,
       g_lay.it_slot, g_lay.it_equip, g_lay.it_lv[0], g_lay.it_alive, g_lay.it_model, g_lay.model_owner);
  return true;
}

bool discover_info() {
  const mt::Class* info = mt::find("sInfoMan");
  const mt::Class* sys = mt::find("cSystemInfo");
  const mt::Class* gi = mt::find("cGameInfo");
  const std::vector<uintptr_t> take = sig_all("kSigTake");
  const uintptr_t shop = sig_one("kSigShopGold");
  const uintptr_t death = sig_one("kSigDeathCounted");
  const std::vector<uintptr_t> counter = sig_all("kSigCounter");
  const uintptr_t play = sig_one("kSigPlayTime");
  const uintptr_t back = sig_one("kSigCheckpoint");
  if (!info || !sys || !gi || take.size() != 2 || !shop || !death || counter.size() != 4 || !play || !back)
    return fail("the profile and the chapter's numbers");
  g_lay.gold = u32at(take[0] + 2);
  const uint32_t points = u32at(take[1] + 2);  // take-points': the exchange points, the next field
  g_lay.profile_index = u8at(shop + 7);
  g_lay.system_size = u32at(shop + 10);
  g_lay.system_base = u32at(shop + 17) - g_lay.gold;
  g_lay.game_size = u32at(death + 5);
  g_lay.game_base = u32at(death + 15);
  g_lay.counters = u32at(counter[0] + 10);
  // add-play-time's [ecx + (index * 3 + k) * 8]: a character's numbers are three
  // pairs of dwords - one for each of sPlayer's characters (the chapter's clock is
  // added to all of them alike).
  g_lay.counter_stride = 24;
  bool counters_ok = call_target(death + 19) == counter[0];
  for (int i = 0; i < 4; ++i) counters_ok = counters_ok && u32at(counter[i] + 10) == g_lay.counters + 4 * i;
  // The checkpoint copy: its data from +4, then the copy - the two fill the class.
  g_lay.checkpoint = u32at(back + 32);
  const bool checkpoint_ok = u32at(back + 12) == g_lay.gold && u32at(back + 4) == g_lay.gold + g_lay.checkpoint &&
                             u32at(back + 38) == 4 + g_lay.checkpoint &&
                             u32at(back + 21) == u32at(back + 27) + g_lay.checkpoint &&
                             4 + 2 * g_lay.checkpoint == sys->size;
  if (points != g_lay.gold + 4 || u8at(death + 2) != g_lay.profile_index || g_lay.system_size != sys->size ||
      g_lay.game_size != gi->size || !counters_ok || u32at(play + 15) * 8 != g_lay.counters + 16 || !checkpoint_ok ||
      g_lay.counters + kMaxChars * g_lay.counter_stride > g_lay.game_size ||
      g_lay.system_base + 2 * g_lay.system_size > info->size || g_lay.game_base + 2 * g_lay.game_size > info->size)
    return fail("the profile and the chapter's numbers (they disagree)");
  logf("game: sInfoMan's profile index +0x%X; cSystemInfo[] +0x%X (%u bytes: gold +0x%X, its checkpoint copy 0x%X "
       "on); cGameInfo[] +0x%X (%u bytes: the chapter's counters +0x%X, %u a character)",
       g_lay.profile_index, g_lay.system_base, g_lay.system_size, g_lay.gold, g_lay.checkpoint, g_lay.game_base,
       g_lay.game_size, g_lay.counters, g_lay.counter_stride);
  return true;
}

// "uPl01ShebaArmorA" -> "Sheva": the class right under uPlayer names the
// character (the costumes derive from it).
const char* name_of_class(const mt::Class* c) {
  static const struct {
    const char* cls;
    const char* name;
  } kNames[] = {{"uPl00Chris", "Chris"},   {"uPl01Sheba", "Sheva"},     {"uPl02Jill", "Jill"},
                {"uPl03Wesker", "Wesker"}, {"uPl04Josh", "Josh"},       {"uPl05Excella", "Excella"},
                {"uPl06Barry", "Barry"},   {"uPl07Rebecca", "Rebecca"}, {"uPl86Irving", "Irving"}};
  for (int depth = 0; c && depth < 8; ++depth) {
    for (const auto& n : kNames)
      if (!std::strcmp(c->name, n.cls)) return n.name;
    c = mt::by_dti(c->parent);
  }
  return "?";
}

}  // namespace

std::vector<uintptr_t> sig_all(const char* name) {
  const Sig* s = sig(name);
  if (!s) return {};
  std::vector<uintptr_t> hits = mem::find_all(image::text(), s->pattern, 8);
  if (static_cast<int>(hits.size()) != s->matches)
    logf("game: %s: %d match(es), expected %d", name, static_cast<int>(hits.size()), s->matches);
  return hits;
}

uintptr_t sig_one(const char* name) {
  const std::vector<uintptr_t> h = sig_all(name);
  return h.size() == 1 ? h[0] : 0;
}

uintptr_t call_target(uintptr_t at) {
  return mem::read<uint8_t>(at) == 0xE8 ? at + 5 + mem::read<int32_t>(at + 1) : 0;
}

bool discover() {
  if (!mt::init()) return fail("MT Framework's class records");
  g_cls_system = mt::find("cSystem");
  g_cls_main = mt::find("sBioMain");
  g_cls_player = mt::find("uPlayer");
  g_cls_pause = mt::find("uIdPause00");
  g_cls_merce = mt::find("uIdMerce00");
  g_cls_items = mt::find("uIdItemBox00");  // (itembox.cpp says whether it is understood)
  g_cls_over = mt::find("uIdGameover00");
  g_cls_results = mt::find("uIdChapEd00");
  if (!g_cls_system || !g_cls_main || !g_cls_main->vtable || !g_cls_player || !g_cls_player->vtable || !g_cls_pause ||
      !g_cls_merce)
    return fail("the game's main classes");
  logf("game: sBioMain's record %08X, vtable %08X, %u bytes", static_cast<unsigned>(g_cls_main->dti),
       static_cast<unsigned>(g_cls_main->vtable), g_cls_main->size);
  if (!units::discover()) return fail("the game's units");
  if (!discover_pause() || !discover_characters() || !discover_items() || !discover_info()) return false;
  discover_vehicles();
  discover_mode();
  g_ready = true;
  std::snprintf(g_status, sizeof(g_status), "ready");
  return true;
}

bool ready() { return g_ready; }
const char* status_text() { return g_status; }
const Layout& layout() { return g_lay; }

void tick(void* main, Run* out) {
  if (!g_systems_seen) survey_systems(reinterpret_cast<uintptr_t>(main));
  *out = Run{};
  out->main = reinterpret_cast<uintptr_t>(main);
  if (!g_sunit) return;
  bool merce = false;
  uintptr_t items = 0, pause = 0, over = 0, results = 0;
  std::vector<const mt::Class*> screens;
  const bool trace = config::get().trace;
  units::each(g_sunit, [&](uintptr_t u, const mt::Class* c) {
    if (!c) return true;
    // The pause menu's unit is there all through a chapter; only open is it the pause.
    if (c == g_cls_pause) {
      if (!pause && mem::read<uint8_t>(u + g_pause_open)) pause = u;
    } else if (c == g_cls_merce) {
      merce = true;
    } else if (c == g_cls_items && g_cls_items) {
      items = u;
    } else if (c == g_cls_over && g_cls_over) {
      over = u;
    } else if (c == g_cls_results && g_cls_results) {
      results = u;
    } else if (out->nvehicles < kMaxVehicles &&
               std::find(g_cls_vehicles.begin(), g_cls_vehicles.end(), c) != g_cls_vehicles.end()) {
      out->vehicles[out->nvehicles++] = u;
    }
    // Every uId* class is a screen, a menu or a part of the HUD ("cockpit").
    if (trace && !std::strncmp(c->name, "uId", 3)) screens.push_back(c);
    return true;
  });
  out->nchars = characters(out->chars);
  out->in_game = out->nchars > 0;
  if (g_cls_gamesys) {
    const uintptr_t gs = mem::read<uintptr_t>(out->main + g_gamesys);
    if (gs && !(gs & 3) && mt::is_a(gs, g_cls_gamesys)) out->mode = static_cast<int>(mem::read<uint32_t>(gs + g_mode));
  }
  out->mercenaries = merce;
  out->item_screen = items;
  out->pause_menu = out->in_game && !items ? pause : 0;
  out->game_over = out->in_game ? over : 0;
  out->results = results;
  out->screen = items           ? Screen::kItemBox
                : !out->in_game ? Screen::kNone
                : pause         ? Screen::kPause
                                : Screen::kPlay;
  if (trace) {
    // What screens came and went (each a uId* unit), for the log.
    for (const mt::Class* c : screens)
      if (std::find(g_screens_prev.begin(), g_screens_prev.end(), c) == g_screens_prev.end())
        logf("trace: %s up", c->name);
    for (const mt::Class* c : g_screens_prev)
      if (std::find(screens.begin(), screens.end(), c) == screens.end()) logf("trace: %s gone", c->name);
    g_screens_prev.swap(screens);
  }
}

const char* screen_name(Screen s) {
  switch (s) {
    case Screen::kNone: return "none";
    case Screen::kPlay: return "play";
    case Screen::kPause: return "pause";
    case Screen::kItemBox: return "item management";
  }
  return "?";
}

uintptr_t system(const char* name) {
  const auto it = g_systems.find(name);
  return it == g_systems.end() ? 0 : it->second;
}

int characters(Character out[kMaxChars]) {
  if (!g_ready || !g_splayer) return 0;
  int n = 0;
  for (int i = 0; i < g_lay.nplayers; ++i) {
    const uintptr_t c = mem::read<uintptr_t>(g_splayer + g_lay.players + 4 * i);
    if (!c) continue;
    // A character the game is making or running (state 1 or 2), as sPlayer's own check takes it.
    const uint32_t state = mem::read<uint32_t>(c + units::layout().unit_flags) & 7;
    if (state != 1 && state != 2) continue;
    Character& ch = out[n++];
    ch.obj = c;
    ch.index = i;
    ch.control = static_cast<Control>(mem::read<uint32_t>(c + g_lay.control));
    ch.name = character_name(c);
    ch.hp = mem::read<int16_t>(c + g_lay.hp);
    ch.hp_max = mem::read<int16_t>(c + g_lay.hp_max);
  }
  return n;
}

uintptr_t character_at(int index) {
  if (!g_ready || !g_splayer || index < 0 || index >= g_lay.nplayers) return 0;
  return mem::read<uintptr_t>(g_splayer + g_lay.players + 4 * index);
}

const char* character_name(uintptr_t obj) { return name_of_class(units::class_of(obj)); }

void set_health(uintptr_t obj, int hp) {
  if (!obj) return;
  const int most = mem::read<int16_t>(obj + g_lay.hp_max);
  mem::write<int16_t>(obj + g_lay.hp, static_cast<int16_t>(hp < 0 ? 0 : hp > most ? most : hp));
}

uintptr_t system_info() {
  if (!g_ready || !g_sinfo) return 0;
  const uint32_t i = mem::read<uint32_t>(g_sinfo + g_lay.profile_index);
  return i < 2 ? g_sinfo + g_lay.system_base + i * g_lay.system_size : 0;
}

uintptr_t game_info() {
  if (!g_ready || !g_sinfo) return 0;
  const uint32_t i = mem::read<uint32_t>(g_sinfo + g_lay.profile_index);
  return i < 2 ? g_sinfo + g_lay.game_base + i * g_lay.game_size : 0;
}

bool item_in(uintptr_t chara, int slot, Item* out) {
  if (!g_ready || !chara) return false;
  const uintptr_t mgr = chara + g_lay.case_mgr;
  for (int i = 0; i < g_lay.nitems; ++i) {
    const uintptr_t it = mgr + g_lay.items + i * g_lay.item_size;
    if (!mem::read<uint8_t>(it + g_lay.it_alive)) continue;
    if (static_cast<int>(mem::read<uint32_t>(it + g_lay.it_slot)) != slot) continue;
    out->at = it;
    out->id = static_cast<int>(mem::read<uint32_t>(it + g_lay.it_id));
    out->num = static_cast<int>(mem::read<uint32_t>(it + g_lay.it_num));
    out->most = static_cast<int>(mem::read<uint32_t>(it + g_lay.it_most));
    out->slot = slot;
    out->equip = static_cast<int>(mem::read<uint32_t>(it + g_lay.it_equip));
    for (int k = 0; k < 9; ++k) out->lv[k] = mem::read<uint8_t>(it + g_lay.it_lv[k]);
    return true;
  }
  return false;
}

}  // namespace re5cc::game
