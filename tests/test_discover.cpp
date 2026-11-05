// Offline test of the Mercenaries' discovery (src/merc.cpp) and Chapter Select's game modes (src/menus.cpp) over
// the game itself: the dump (ghidra_proj/re5dx9.dumped.exe - the game's sections as they are in memory, raw offsets
// == RVAs) is copied to its own addresses inside this test's image, and the mod's real mt.cpp (the class records and
// their vtables), merc.cpp (discover_match, discover_kills with the stages' areas) and menus.cpp (discover_flow,
// discover_menus) run over it, finding everything by the signatures of src/sigs.h as the mod does in the game. A signature found twice or by another's name, an operand read from the
// wrong place or a cross-check that fails shows here, not only in a play's log (2026-11-02: a second kSigAreaFrame
// made the areas fail in the user's game while tools/sig.py found every pattern once).
//
// The test's image is based at 0x400000 with a section reserving the dump's addresses (0x401000..0x137F000) and its
// own code above them, so a linker script (the default's, with that section put first):
//
//   i686-w64-mingw32-ld --verbose | sed -n '/^=====/,/^=====/p' | sed '1d;$d' | sed 's|^  \.text  __image_base__
//       + ( __section_alignment__ < 0x1000 ? \. : __section_alignment__ ) :|  .dumpspace __image_base__ + 0x1000
//       (NOLOAD) : { *(.dumpspace) }\n  .text  __image_base__ + 0x1000000 :|' > tests/build/discover.ld
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Isrc -static -Wl,--image-base=0x400000
//       -Wl,--disable-dynamicbase -Wl,-T,tests/build/discover.ld tests/test_discover.cpp -o tests/build/test_discover.exe
//   wine tests/build/test_discover.exe ghidra_proj/re5dx9.dumped.exe
//
// - every signature's name is its own (the mod looks one up by name: a second is never found);
// - the class records found (mt::init) and the classes the discovery asks for, with their vtables and sizes;
// - discover_match: the play state's fields and both clocks' patch sites as the dump has them;
// - discover_kills: an enemy's hit record, its parts, its death's report flag, the guns' and the attacks' tables
//   (the M92F's attack 2) - and the stages' areas (sScenario's area tasks, the box's fields) and the Chainsaw
//   Majini's second life (uEm22's own frame and its brain's lie-down agreeing on its two bytes, "may not die" the hit
//   path's test);
// - discover_flow and discover_menus: the play flow's Item Management step and its test of the story's mode (cGameSys
//   +0x58, Chapter Select offered in no other), the start-up menu's return reasons and choices (cGameSys +0x18, +0xC,
//   +0x1C), all through the same sBioMain field;
// - the game's mode for the item box (game.cpp's discover_mode, replayed): kSigEpisodeCommit's cGameSys field and the
//   game's own test of the extra episodes (modes 3 and 4) agreeing with the menus' field and mode.
#include "../src/menus.cpp"
#include "../src/merc.cpp"
#include "../src/mt.cpp"
#include "../src/sigs.h"

#include <cstdarg>
#include <cstdio>
#include <string>

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
namespace image {
uintptr_t base() { return 0x400000; }
mem::Range text() { return {0x401000, 0xF4A000}; }
mem::Range rdata() { return {0xF4A000, 0x10CA000}; }
mem::Range data() { return {0x10CA000, 0x136D000}; }
}  // namespace image
namespace game {
Layout g_layout;
const Layout& layout() { return g_layout; }
struct Entry {
  const char* name;
  const char* pattern;
  int matches;
};
#define RE5CC_TEST_SIG(name, pattern, n) {#name, pattern, n},
constexpr Entry kEntries[] = {RE5CC_SIGS(RE5CC_TEST_SIG)};
std::vector<uintptr_t> sig_all(const char* name) {
  for (const Entry& e : kEntries)
    if (!std::strcmp(e.name, name)) return mem::find_all(image::text(), e.pattern, 8);
  return {};
}
uintptr_t sig_one(const char* name) {
  const std::vector<uintptr_t> v = sig_all(name);
  return v.size() == 1 ? v[0] : 0;
}
uintptr_t call_target(uintptr_t at) {
  return mem::read<uint8_t>(at) == 0xE8 ? at + 5 + mem::read<int32_t>(at + 1) : 0;
}
uintptr_t system(const char*) { return 0; }
bool item_in(uintptr_t, int, Item*) { return false; }
const char* character_name(uintptr_t) { return ""; }
bool ready() { return true; }
uintptr_t system_info() { return 0; }
uintptr_t game_info() { return 0; }
}  // namespace game
namespace inventory {  // what menus.cpp's tick asks for (not run here)
bool lists_known() { return false; }
const std::string& archive_dir() {
  static const std::string none;
  return none;
}
bool archive_loaded(const char*) { return false; }
}  // namespace inventory
namespace itembox {
bool screen_idle(uintptr_t) { return false; }
}  // namespace itembox
namespace units {
Layout g_layout;
const Layout& layout() { return g_layout; }
const mt::Class* class_of(uintptr_t) { return nullptr; }
}  // namespace units
}  // namespace re5cc

namespace {
using namespace re5cc;

// The dump's addresses, reserved in this test's own image (not loaded from its file).
__attribute__((section(".dumpspace"), used)) char g_space[0x137F000 - 0x401000];

int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

bool load(const char* path) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  DWORD was = 0;
  const bool placed = reinterpret_cast<uintptr_t>(g_space) == 0x401000 &&
                      VirtualProtect(g_space, sizeof g_space, PAGE_EXECUTE_READWRITE, &was);
  const bool read = placed && !std::fseek(f, 0x1000, SEEK_SET) && std::fread(g_space, 1, sizeof g_space, f) == sizeof g_space;
  std::fclose(f);
  return read;
}

}  // namespace

int main(int argc, char** argv) {
  g_verbose = argc > 2 && std::string(argv[2]) == "-v";
  const char* dump = argc > 1 ? argv[1] : "ghidra_proj/re5dx9.dumped.exe";
  if (!load(dump)) {
    std::printf("usage: test_discover.exe <re5dx9.dumped.exe> [-v] - the dump could not be placed at 0x401000\n");
    return 2;
  }
  // Every signature's name its own.
  bool unique = true;
  for (size_t i = 0; i < std::size(game::kEntries); ++i)
    for (size_t j = 0; j < i; ++j)
      if (!std::strcmp(game::kEntries[i].name, game::kEntries[j].name)) {
        std::printf("      %s defined twice\n", game::kEntries[i].name);
        unique = false;
      }
  check(unique, "every signature's name is its own");
  // As the game's own discovery gives them (game.cpp: kSigSetLife).
  game::g_layout.hp = 0x1364;
  game::g_layout.hp_max = 0x1366;
  check(mt::init() && mt::class_count() == 4355, "MT's class records: 4355");
  const struct {
    const char* name;
    uintptr_t vtable;
    uint32_t size;
  } classes[] = {{"cGameSys", 0x10413BC, 0x7A8},  {"sScenario", 0xF6771C, 0xF8},  {"cScenario", 0x1049698, 0x28},
                 {"cAreaHit", 0x10391DC, 0x80},   {"AHShapeBox", 0x1039300, 0x70}, {"uEnemy", 0xF9D6A8, 0x1F50},
                 {"HIT_PARAM", 0xF6594C, 0x1C},  {"uEmAnimal", 0xF92538, 0x1FB0}, {"uEm22", 0xF75B20, 0x1F70},
                 {"uEmIntel22", 0xF95AB0, 0x2E0}, {"uEmIntel", 0xF95300, 0x280}};
  bool known = true;
  for (const auto& c : classes) {
    const mt::Class* k = mt::find(c.name);
    known = known && k && k->vtable == c.vtable && k->size == c.size;
  }
  check(known, "the classes the discovery asks for: their vtables and sizes as the dump has them");
  check(merc::discover_match(), "discover_match: the Mercenaries' play state understood");
  const merc::Match& m = merc::g_m;
  check(m.sys == 0x1042C && m.mode == 0x58 && m.step == 0x67C && m.online == 0x56C && m.host_a == 0x574 &&
            m.host_b == 0x578 && m.clock == 0x680 && m.kills == 0x58C && m.big == 0x79E && m.base == 150 &&
            m.extra == 250,
        "the play state: cGameSys +0x1042C, mode, step, online, host, clock, kills (150, 250 more)");
  check(m.combo_on == 0x698 && m.combo == 0x69C && m.best == 0x6A0 && m.combo_clock == 0x6A4 && m.waiting == 0x6B4 &&
            m.score == 0x6B8 && m.bonus_on == 0x6A8 && m.bonus_clock == 0x6AC && m.bonus_kills == 0x6B0 &&
            m.bonus_time == 30.0f,
        "the team's slot: combo, its clock, best, earned, score; the Combo Bonus (30 s)");
  check(merc::g_combo_clock.at == 0x71C379 && merc::g_bonus_clock.at == 0x717C8F,
        "both clocks' patch sites (their frame-time subss)");
  check(merc::discover_kills(), "discover_kills: an enemy's hit record understood");
  const merc::HitLayout& h = merc::g_h;
  check(h.record == 0x129C && h.chit == 0x1290 && h.parts == 0x1BFC && h.nparts == 4 && h.pos == 0x30,
        "an enemy: its record +0x129C (its cHit +0x1290), its 4 parts +0x1BFC; a unit's place");
  check(h.place[0] == 0 && h.centre[0] == 0x10 && h.attacker == 0x20 && h.damage == 0x28 && h.impact == 0x34 &&
            h.scale == 0x38 && h.flags[0] == 0x3C && h.flags[1] == 0x40 && h.flags[2] == 0x44 && h.hit == 0x48 &&
            h.part == 0x49 && h.b94 == 0x94 && h.f98 == 0x98 && h.attack == 0x9C && h.cls == 0xA4 &&
            h.whole == 0x10000000,
        "the record's fields, and the whole-health bit 0x10000000");
  merc::Shot shot;
  check(merc::shot_of(0x102, &shot) && shot.attack == 2 && shot.cls == 0x07010200 &&
            mem::read<float>(shot.data + h.def_impact) == mem::read<float>(0x10FC838 + 8),
        "the M92F's shot: attack 2 (its data from the attacks' table), its class 0x07010200");
  merc::Shot knife;
  check(!merc::shot_of(0x101, &knife), "a knife is no shot");
  check(merc::g_melee_seconds == 5, "a hit naming the player's own class is a hand-to-hand kill: 5 s onto the clock");
  check(merc::g_area_ok, "the stages' areas understood");
  const merc::AreaLayout& a = merc::g_a;
  check(a.lists == 0xD4 && a.type == 6 && a.count == 8 && a.items == 0x14 && a.area == 8 && a.shape == 0x14 &&
            a.floor == 0x60 && a.height == 0x64 && a.x[0] == 0x20 && a.z[3] == 0x58,
        "the areas: sScenario +0xD4's tasks of type 6, a task's area, its box's floor, height and corners");
  check(merc::g_life_ok && merc::g_cls_chainsaw == mt::find("uEm22"), "the Chainsaw Majini's second life understood");
  const merc::SecondLife& s = merc::g_life;
  check(s.may == 0x1F5F && s.got_up == 0x1F5E && s.no_death == 0x1E38 && s.up_health == 0x1F5C && s.lie == 0x20,
        "its second life: uEm22 +0x1F5F (it may get up again), +0x1F5E (it has), uEnemy's +0x1E38 (may not die), up "
        "with +0x1F5C's health, lying down action 0x20");
  check(merc::own_slot(mt::find("uEm22"), mt::find("uEnemy"), 0x87B280) == 62 &&
            merc::own_slot(mt::find("uEmIntel22"), mt::find("uEmIntel"), 0x9848A0) == 35 &&
            merc::own_slot(mt::find("uEm22"), mt::find("uEnemy"), 0x9DA880) == -1,
        "its frame uEm22's own slot 62, the lie-down uEmIntel22's own slot 35 (uEnemy's hit check, slot 129, not "
        "uEm22's own)");
  // Chapter Select (menus.cpp): offered only in the story's mode, as the play flow's Item Management step tests it,
  // and in a game started from the story's menu.
  menus::g_cls_flow = mt::find("aGame");
  menus::g_cls_startup = mt::find("cDemoStartUp");
  menus::g_cls_sys = mt::find("cGameSys");
  check(menus::g_cls_flow && menus::g_cls_startup && menus::g_cls_sys &&
            menus::discover_flow(mt::find("uIdItemBox00")),
        "discover_flow: the play flow's Item Management step understood");
  check(menus::g_mode == 0x58 && menus::g_mode_sys == 0x1042C,
        "... its first step's test of the story: cGameSys's mode +0x58 against 0, cGameSys through sBioMain +0x1042C");
  check(menus::discover_menus(mt::find("sBioMain")), "discover_menus: the start-up menu's reasons and choices understood");
  check(menus::g_sys == 0x1042C && menus::g_reason == 0x18 && menus::g_reason_normal == 1 &&
            menus::g_reason_back == 7 && menus::g_top == 0xC && menus::g_top_story == 0 && menus::g_story == 0x1C &&
            menus::g_chapter_select == 1 && menus::g_story_park == 13,
        "... cGameSys through the same field: the return reason +0x18 (1 a story start's, 7 back to the choice), the "
        "top menu's choice +0xC (0 the story), the story menu's +0x1C (1 CHAPTER SELECT, 13 parked)");
  // The game's mode for the item box (game.cpp's discover_mode, its reads replayed - game.cpp is not built in): the
  // play flow's checkpoint save asks the game's own test of the extra episodes before it commits - cGameSys's field
  // the menus' own, the mode the one their test of the story reads, the test's compares the two episodes' modes.
  {
    const uintptr_t at = game::sig_one("kSigEpisodeCommit");
    const uintptr_t test = at ? game::call_target(at + 19) : 0;
    char pattern[96];
    std::snprintf(pattern, sizeof(pattern), "8B 41 ?? 83 F8 %02X 74 08 83 F8 %02X 74 03 33 C0 C3 B8 01 00 00 00 C3",
                  game::kModeNightmares, game::kModeEscape);
    check(at == 0x7237C6 && test == 0x715AE0 && mem::matches(test, mem::parse_pattern(pattern)) &&
              mem::read<uint32_t>(at + 15) == menus::g_mode_sys && mem::read<uint8_t>(test + 2) == menus::g_mode,
          "the game's mode for the item box (kSigEpisodeCommit): cGameSys through the menus' own field, the mode they "
          "read, and the extra episodes' 3 and 4 the game's own test of them");
  }
  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  return g_fail ? 1 : 0;
}
