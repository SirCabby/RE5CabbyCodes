#include "overlay.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

#include "adopt.h"
#include "boot.h"
#include "cheats.h"
#include "config.h"
#include "dispatch.h"
#include "game.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "inventory.h"
#include "itembox.h"
#include "items.h"
#include "backends/imgui_impl_win32.h"
#include "log.h"
#include "menus.h"
#include "merc.h"
#include "version.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace re5cc::overlay {
namespace {

HWND g_hwnd = nullptr;
WNDPROC g_orig_wndproc = nullptr;
bool g_context_ready = false;
bool g_visible = false;
bool g_user_hidden = false;  // the toggle key, while the panel's screen is up
bool g_user_shown = false;   // the toggle key, in play: the cheats over the game

// The panel is drawn half as big again as ImGui's own sizes: the game draws at
// the display's resolution, where ImGui's 13-pixel font is small (the size the
// user settled on for RE3-1999CabbyCodes, 2026-10-07). Every size below that
// is written in pixels is at ImGui's own scale and goes through scaled().
constexpr int kScalePercent = 150;
constexpr float kScale = kScalePercent / 100.0f;
float scaled(float px) { return px * kScale; }

// Kept in the ImGui layout file ([RE5CabbyCodes][Panel]) beside the windows'
// places and sizes: which sections are folded away.
bool g_cheats_open = true;
bool g_results_open = true;
bool g_merc_open = true;
bool g_case_open[game::kMaxChars] = {true, true, false, false};
bool g_box_open = true;      // the item box window: what is in the box ...
bool g_box_add_open = true;  // ... and its Add

const char* const kPanelWindow = "Resident Evil 5 - Cabby Codes  v" RE5CC_VERSION "###RE5CC panel";
const char* const kBoxWindow = "Resident Evil 5 - Cabby Codes: item box  v" RE5CC_VERSION "###RE5CC item box";
// No title bar: the name is the id - one beside Item Management's item box window, one beside the panel at the
// pause, one on the Game Over screen and the chapter's results.
const char* const kChapterWindow = "Chapter Select###RE5CC chapter select";
const char* const kChapterPauseWindow = "Chapter Select###RE5CC chapter select at the pause";
const char* const kChapterEndWindow = "Chapter Select###RE5CC chapter select at an end";

CRITICAL_SECTION g_imgui_cs;
bool g_imgui_cs_ready = false;

// What the panel is taking this frame, published for the input guard.
volatile LONG g_capture_mouse = 0;
volatile LONG g_capture_keyboard = 0;
volatile LONG g_visible_flag = 0;

const ImVec4 kOrange(1.0f, 0.6f, 0.3f, 1.0f);

LRESULT CALLBACK hk_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  const dispatch::Snapshot s = dispatch::snapshot();
  // F-keys above F9 arrive as WM_SYSKEYDOWN; accept both, ignore auto-repeat.
  if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !(lp & (1 << 30)) &&
      static_cast<int>(wp) == config::get().toggle_key) {
    ImGuiLock guard;  // wants_draw reads and clears these
    if (s.show_panel || s.show_itembox || s.show_end || config::get().always_show) {
      g_user_hidden = !g_user_hidden;
      logf("panel %s by the toggle key", g_user_hidden ? "hidden" : "shown");
      return 0;
    }
    // In play the key opens the cheats over the game; out of a game (the title,
    // the menus) it does nothing - unless the mod cannot see the game, when the
    // panel says why.
    if (s.run.screen == game::Screen::kPlay || !s.ready) {
      g_user_shown = !g_user_shown;
      logf("panel %s over the game by the toggle key", g_user_shown ? "opened" : "closed");
      return 0;
    }
  }
  // ImGui sees every message, visible or not: a button release that arrives
  // after the panel is hidden would otherwise never be delivered, leaving it
  // convinced the button is still held (and the window stuck to the pointer).
  if (g_context_ready) {
    bool swallow = false;
    {
      ImGuiLock guard;
      if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
      if (g_visible) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) swallow = true;
        if (io.WantTextInput && msg >= WM_KEYFIRST && msg <= WM_KEYLAST && wp != VK_ESCAPE) swallow = true;
      }
    }
    if (swallow) return 0;
  }
  // The game's own window procedure runs outside the lock.
  return CallWindowProcW(g_orig_wndproc, hwnd, msg, wp, lp);
}

// Keep the current window inside the display: moved back whenever any part of
// it has ended up outside (a saved position from a bigger window, a panel that
// grew).
void keep_in_reach(const ImVec2& display) {
  const ImVec2 pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
  ImVec2 want = pos;
  if (want.x + size.x > display.x) want.x = display.x - size.x;
  if (want.y + size.y > display.y) want.y = display.y - size.y;
  if (want.x < 0) want.x = 0;
  if (want.y < 0) want.y = 0;
  if (want.x != pos.x || want.y != pos.y) ImGui::SetWindowPos(want);
}

// The help for the item just drawn, on hover (a disabled one too).
void item_help(const char* help) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip()) {
    ImGui::PushTextWrapPos(scaled(380.0f));
    ImGui::TextUnformatted(help);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

// A section heading that folds, remembered in the layout file.
bool section(const char* label, bool* open) {
  ImGui::SetNextItemOpen(*open, ImGuiCond_Always);
  const bool now = ImGui::CollapsingHeader(label);
  if (now != *open) {
    *open = now;
    ImGui::MarkIniSettingsDirty();
  }
  return now;
}

// A section within a section (a list's Add): its heading and what it holds set
// in under the parent's, folding on its own.
template <typename Draw>
void subsection(const char* label, bool* open, Draw&& draw) {
  const float indent = ImGui::GetStyle().IndentSpacing;
  ImGui::Spacing();
  ImGui::Indent(indent);
  if (section(label, open)) draw();
  ImGui::Unindent(indent);
}

void cheat_row(sw::Kind k, const char* label, const char* help) {
  bool on = cheats::enabled(k);
  const bool can = cheats::available(k);
  ImGui::BeginDisabled(!can);
  if (ImGui::Checkbox(label, &on)) cheats::set(k, on);
  ImGui::EndDisabled();
  item_help(can ? help : cheats::unavailable_reason(k));
}

// Edits the panel is typing: kept until Set, then let go so the field follows the game again.
struct Edit {
  int v = 0;
  bool typing = false;
};

// "<label> [   n] Set": Set sends the typed value; until then the field follows the game.
bool int_editor(const char* label, const char* id, Edit& e, int live, int lo, int hi, float width, bool enabled) {
  if (!e.typing) e.v = live;
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine();
  ImGui::BeginDisabled(!enabled);
  ImGui::SetNextItemWidth(scaled(width));
  char field[48];
  std::snprintf(field, sizeof(field), "##%s", id);
  if (ImGui::InputInt(field, &e.v, 0, 0)) e.typing = true;
  if (e.v < lo) e.v = lo;
  if (e.v > hi) e.v = hi;
  ImGui::SameLine();
  char button[48];
  std::snprintf(button, sizeof(button), "Set##%s", id);
  const bool set = ImGui::SmallButton(button);
  ImGui::EndDisabled();
  if (set) e.typing = false;
  return set;
}

Edit g_gold, g_clock, g_countdown, g_counter[2][4];
Edit g_count[game::kMaxChars][game::kSlots];

void forget_edits() {
  g_gold.typing = g_clock.typing = g_countdown.typing = false;
  for (auto& row : g_counter)
    for (Edit& e : row) e.typing = false;
  for (auto& row : g_count)
    for (Edit& e : row) e.typing = false;
}

void hms(float seconds, char* out, size_t n) {
  const int s = seconds < 0 ? 0 : static_cast<int>(seconds);
  std::snprintf(out, n, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
}

void draw_cheats(const dispatch::Snapshot& snap, const cheats::Status& st) {
  const bool paused = snap.run.screen == game::Screen::kPause;
  ImGui::SeparatorText("Characters");
  cheat_row(sw::kGodMode, "God mode",
            "No hit takes any health off you: the characters' damage routine turns the hit away, the way the game "
            "itself turns away a hit on the other player's character in a network game (no damage, not dying). Your "
            "health is held at its maximum too. You still flinch, and grabs still play out; a death the game scripts "
            "without a hit (a fall, a missed QTE) is left to it. The jeep you ride in chapter 2-3 is covered as well: "
            "no hit takes any of its health, and its health is held at its maximum.");
  cheat_row(sw::kPartnerGod, "Partner takes no damage",
            "The same for your partner while the computer plays them (Sheva, or Chris when you play Sheva): no damage, "
            "health held at its maximum. A second player's character, here or over the network, is not covered.");
  cheat_row(sw::kOneHitKills, "One hit kills",
            "Any hit that does damage takes all of an enemy's health, so it dies through the game's own death. An "
            "enemy the game keeps alive for now (a boss between phases) keeps its last point. The story's companions "
            "(Josh and the like) are left alone. The Ndesu (the giant ending chapter 2-3) goes by counters of its own: "
            "there a hit with damage takes all its strength for the round and lets out every Plaga still in it, and a "
            "Plaga dies of one hit - the last one's death ends the fight. Irving's transformed shape (the boss ending "
            "chapter 3-3) takes damage only in its core, which opens while it is down: a hit with damage on its body "
            "sends it down (then, or at a later hit if it is busy attacking), a hit on the open core kills it, and a "
            "tentacle breaks at one hit.");
  cheat_row(sw::kInfiniteAmmo, "Infinite ammo",
            "Every weapon gets the game's own infinite ammo - the bonus feature's: firing uses no rounds, so there is "
            "nothing to reload, and a thrown grenade or egg stays in the inventory. A mounted gun (the jeep's in "
            "chapter 2-3, a gun turret) never overheats in your hands or your partner's; the enemies' still do.");
  cheat_row(sw::kSteadyAim, "Steady aim",
            "Your aim holds still. The game makes an aim you hold sway a little (it breathes: the laser sight's dot, "
            "or the view through a scope, drifts about where you point it) and kicks it up at every shot. With this "
            "on there is no sway and no kick, with a pad or the mouse, and the mouse crosshair no longer opens at a "
            "shot. A scope looks down the gun itself, which moved with your character's breathing: while a scope is "
            "up, that is held still too. Your shots go straight to where you aim, as they always do (a shotgun keeps "
            "its spread of pellets).");
  cheat_row(sw::kAutoQte, "Automatic QTEs",
            "The quick time events play themselves. This turns on the game's own automatic mode (Action Assist: "
            "AUTO, whatever the option is set to), which answers the cutscenes' prompts and some dodges by itself and "
            "picks the easiest buttons. On top of that, a grab is broken free of at once, the enemies' and bosses' "
            "dodges, holds and blows are taken as they come up, and a crank you take hold of winds itself all the "
            "way, as fast as the game allows (its gate or door fully open) - you let go (Release) when you choose, "
            "unless it is one that stays open and lets go of you by itself. Once you take hold of the boulder in "
            "the volcano (chapter 6-3), the pushing and the punches play out by themselves. "
            "Prompts you choose to act on (doors, items, melee moves, helping or commanding your partner, turning a "
            "mirror) are left to you.");
  ImGui::SeparatorText("Map");
  cheat_row(sw::kOpenMap, "Open map at chapter start",
            "Your map opens by itself as each story chapter begins - as its title comes up - and as Lost in "
            "Nightmares or Desperate Escape begins, just as if you had pressed the map key. The key closes it as "
            "usual, and the game then keeps it closed from one area to the next, until the next chapter begins. This "
            "takes effect from the next chapter start; the Mercenaries and Versus are left alone.");
  ImGui::SeparatorText("Gold");
  cheat_row(sw::kInfiniteMoney, "Infinite money",
            "The shop takes no gold for what you buy or upgrade; selling treasures still pays.");
  if (st.profile &&
      int_editor("Gold:", "gold", g_gold, st.gold, 0, 9999999, 110, paused))
    cheats::edit(cheats::Edit::kGold, 0, g_gold.v);
  ImGui::SeparatorText("Countdown");
  cheat_row(sw::kFreezeCountdown, "Freeze countdown timer",
            "The countdown on screen stands still where it is, so time never runs out: the Mercenaries' clock (its "
            "time bonuses still add to it), the escape from the oil field at the end of chapter 3-2, and the wait for "
            "the helicopter at the end of Desperate Escape. It runs on from there the moment you switch this off.");
  if (st.countdown) {
    const int left = static_cast<int>(st.countdown_left);
    if (int_editor("Time left (s)", "countdown", g_countdown, left, 1, static_cast<int>(st.countdown_most), 90, paused))
      cheats::edit(cheats::Edit::kCountdown, 0, g_countdown.v);
    item_help("The seconds left of the countdown on screen. It runs down from what you set once you leave the pause "
              "(or stays there while Freeze countdown timer is on).");
    ImGui::SameLine();
    ImGui::TextDisabled("%d:%02d", left / 60, left % 60);
  }
  if (!paused && st.profile) ImGui::TextDisabled("Pause the game to change the numbers.");
}

// The Mercenaries' own switches and the match's numbers (shown only in a match).
void draw_mercenaries(const merc::Status& m) {
  cheat_row(sw::kEndlessCombo, "Endless combo",
            "A combo never runs out of time: every kill counts as the next one in it however long since the last, so "
            "its bonus keeps climbing to its most and stays there. The game puts what a combo has earned onto your "
            "score only once the combo is over; with this on it goes on as it is earned instead. Switched off, the "
            "combo runs out as usual, 10 seconds after the last kill.");
  cheat_row(sw::kComboBonus, "Combo Bonus always on",
            "The Combo Bonus - the 30 seconds a Combo Bonus pickup gives, in which the bonus of every kill from the "
            "second in a combo is at least 1,000 - runs from the moment the clock starts and never runs out. Switched "
            "off, 30 seconds of it are left, as a pickup would leave.");
  cheat_row(sw::kAutoKill, "Kill enemies as they appear",
            "Every enemy dies the moment it is in play, wherever it is (those already about when you switch it on "
            "too): the game's own death, and the kill scores as one of your hand-to-hand kills (a punch, a kick, a "
            "finisher) - its points, the combo, the kills and the 5 seconds a hand-to-hand kill puts on the clock (a "
            "knife's kill earns none). The big ones (the Chainsaw Majini, the Giant Majini, the Reapers and the "
            "like) as well - a Chainsaw Majini stays down rather than getting up for a second round -, and the "
            "chickens: every stage counts its chickens among the 150 that clear it (a chicken "
            "is worth 2,000 points). The game sends a stage's enemies area by area, only to the areas you are in; with "
            "this on every area sends its own wherever you stand, at the game's own pace, so a match is over in a few "
            "minutes. It waits for the clock to start and stops when time "
            "is up or you are down. It lasts one match: it switches itself off as the match ends (cleared, time up, "
            "retried or quit) - switch it on again in the pause menu of the next.");
  if (!m.offline) ImGui::TextColored(kOrange, "Online: these act only in a game of your own (offline).");
  ImGui::SeparatorText("This match");
  const int left = static_cast<int>(m.time_left);
  ImGui::Text("Score %d", m.score);
  if (m.waiting > 0) {
    ImGui::SameLine();
    ImGui::TextDisabled("(+%d as the combo ends)", m.waiting);
  }
  ImGui::Text("Combo %d  (best %d)", m.combo, m.best_combo);
  ImGui::Text("Kills %d of %d", m.kills, m.target);
  ImGui::Text("Time left %d:%02d", left / 60, left % 60);
  if (m.bonus) {
    const int b = static_cast<int>(m.bonus_left);
    ImGui::Text("Combo Bonus %d:%02d left, %d kill%s in it", b / 60, b % 60, m.bonus_kills,
                m.bonus_kills == 1 ? "" : "s");
  }
  if (cheats::enabled(sw::kAutoKill) && m.step == 1) {
    ImGui::TextDisabled("Hitting %d enem%s; %d area%s of the stage open", m.killing, m.killing == 1 ? "y" : "ies",
                        m.areas, m.areas == 1 ? "" : "s");
    if (m.melee_seconds) ImGui::TextDisabled("Each kill a hand-to-hand one: +%d s on the clock", m.melee_seconds);
  }
}

void draw_results(const dispatch::Snapshot& snap, const cheats::Status& st) {
  const bool paused = snap.run.screen == game::Screen::kPause;
  cheat_row(sw::kFreezePlaytime, "Freeze play time",
            "The chapter's clock - the time on the results screen, which its rank goes by - stands still.");
  cheat_row(sw::kNoDeathCount, "Deaths don't count", "A death is not counted in the chapter's results.");
  cheat_row(sw::kNoMissCount, "Misses don't count",
            "A shot that misses is not counted as a shot fired, so the chapter's accuracy stays at 100%.");
  if (!st.profile) return;
  // One clock for the chapter: the game adds the same time to both characters'.
  char time[32];
  hms(st.clock, time, sizeof(time));
  ImGui::SeparatorText("Chapter");
  if (int_editor("Time (s)", "time", g_clock, static_cast<int>(st.clock), 0, 359999, 90, paused))
    cheats::edit(cheats::Edit::kTime, 0, g_clock.v);
  item_help("The chapter's play time in seconds, as the results screen shows it. The game keeps one clock for the "
            "chapter (it counts the same time for Chris and Sheva), and Set changes it for both.");
  ImGui::SameLine();
  ImGui::TextDisabled("%s", time);
  static const char* const kWho[2] = {"Chris", "Sheva"};
  static const char* const kWhat[4] = {"Deaths", "Kills", "Shots", "Hits"};
  static const cheats::Edit kEdit[4] = {cheats::Edit::kDeaths, cheats::Edit::kKills, cheats::Edit::kShots,
                                        cheats::Edit::kHits};
  for (int c = 0; c < 2; ++c) {
    const cheats::Counters& n = st.chars[c];
    ImGui::SeparatorText(kWho[c]);
    const int live[4] = {n.deaths, n.kills, n.shots, n.hits};
    for (int k = 0; k < 4; ++k) {
      char id[24];
      std::snprintf(id, sizeof(id), "c%d%d", c, k);
      char label[24];
      std::snprintf(label, sizeof(label), "%-7s", kWhat[k]);
      if (int_editor(label, id, g_counter[c][k], live[k], 0, 9999999, 90, paused)) cheats::edit(kEdit[k], c, g_counter[c][k].v);
      if (k % 2 == 0) ImGui::SameLine(0, scaled(24));
    }
    if (n.shots > 0) {
      ImGui::SameLine();
      ImGui::TextDisabled(" accuracy %d%%", n.hits * 100 / n.shots);
    }
  }
}

const char* item_name(int id, char* buf, size_t n) {
  if (const items::Info* i = items::find(static_cast<uint32_t>(id))) return i->name;
  std::snprintf(buf, n, "item %#x", id);
  return buf;
}

// The Add picker's tabs: what can be added to a case - what the story gives Chris
// and Sheva to carry (items.h's `offer`: the shop's items and the launcher's loads) -
// filed by the item table's own data: a magazine-fed firearm is a weapon, every
// other weapon (grenades, eggs, the stun rod, the rocket launcher) a sub-weapon.
enum Tab { kTabWeapons, kTabSub, kTabAmmo, kTabRecovery, kTabArmor, kTabCount };
const char* const kTabNames[kTabCount] = {"Weapons", "Sub-weapons", "Ammo", "Recovery", "Armor"};
int tab_of(const items::Info& i) {
  if (!i.offer) return -1;  // knives, the Mercenaries' guns, scene-only weapons, floor herbs, leftovers
  switch (i.cat) {
    case 1: return i.magazine ? kTabWeapons : kTabSub;
    case 2: return kTabAmmo;
    case 3: return kTabRecovery;
    case 6: return kTabArmor;  // the vests
    default: return -1;        // key items and treasures are kept off the grid's add
  }
}

// The game keeps two weapons for one character each (Item Management's own check:
// the Gatling gun is Chris's, the Longbow Sheva's): why `who` may not have `i`, or
// nullptr when they may.
const char* not_theirs(const items::Info& i, const char* who, char* buf, size_t n) {
  if (!i.only || (who && !std::strcmp(who, i.only))) return nullptr;
  std::snprintf(buf, n, "Only %s can have the %s: the game itself gives it to no one else.", i.only, i.name);
  return buf;
}

// The box takes only what the Add lists offer, as it keeps the item (a grenade launcher
// as its load: the case's item has the launcher's id) - which leaves out a scene's own
// weapons and their rounds (the user, 2026-11-01: "store should refuse those types of
// items"): why a case's item stays out of it, or nullptr when it goes in.
const char* not_for_box(const inventory::Slot& sl, char* buf, size_t n) {
  const items::Info* i = items::find(static_cast<uint32_t>(sl.kept_as ? sl.kept_as : sl.id));
  if (i && i->offer) return nullptr;
  std::snprintf(buf, n,
                "The %s cannot go in the item box: it takes only what the Add lists offer - the story's items to "
                "keep, not a scene's own weapons.",
                i ? i->name : "item");
  return buf;
}

// The item box's tabs: the Add list's, one for what is filed under none of them
// (key items, an item the table does not have) shown while the box holds some, and
// the treasures - which the game keeps in a list of their own beside the box.
constexpr int kTabOther = kTabCount;
constexpr int kTabTreasures = kTabCount + 1;
constexpr int kTabBanks = kTabCount + 2;
const char* tab_name(int t) {
  return t >= 0 && t < kTabCount ? kTabNames[t] : t == kTabTreasures ? "Treasures" : "Other items";
}
// An entry of the box (its item list's: a treasure there is an Other item).
int box_tab_of(int id) {
  const items::Info* i = items::find(static_cast<uint32_t>(id));
  const int t = i ? tab_of(*i) : -1;
  return t >= 0 ? t : kTabOther;
}

// An item's place in the lists: the item table's order (the Add list's).
int rank_of(int id) {
  for (int k = 0; k < items::kCount; ++k)
    if (items::kItems[k].id == id) return k;
  return items::kCount + id;
}

// An item's name in the lists: a name the table has twice gets its id - among what
// the Add lists offer for an item they offer (the herbs and eggs the floor and the
// leftovers share a name with go by their names), among all of them for the rest
// (the four knives, the two Samurai Edges).
const char* list_name(const items::Info& i, char* buf, size_t n) {
  int same = 0;
  for (const items::Info& o : items::kItems) same += (o.offer || !i.offer) && !std::strcmp(o.name, i.name);
  if (same < 2) return i.name;
  std::snprintf(buf, n, "%s [%03X]", i.name, i.id);
  return buf;
}

bool matches_query(const char* name, const char* q) {
  if (!q[0]) return true;
  for (const char* h = name; *h; ++h) {
    const char *a = h, *b = q;
    while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) == std::tolower(static_cast<unsigned char>(*b))) ++a, ++b;
    if (!*b) return true;
  }
  return false;
}

// Why an item cannot go into an inventory during play (inventory::Avail), or nullptr
// when it can - loaded, or loading (it goes in as soon as it is in).
const char* avail_help(uint8_t avail, bool known) {
  if (avail != inventory::kNever) return nullptr;
  return known ? "The game has no model for this item, so it cannot keep it in an inventory - it is a leftover of "
                 "the game's item list that no weapon or recipe uses."
               : "The mod could not read the game's loader (see RE5CabbyCodes.log), so during play only what this "
                 "area has loaded can go into an inventory: an item without its model makes the game quit. Item "
                 "Management, between chapters, takes anything.";
}

struct Picker {
  char query[48] = "";
  int tab = 0;
  int picked = 0;
  int count = 1;
};
Picker g_picker[game::kMaxChars];
bool g_add_open[game::kMaxChars] = {};

void draw_add(const inventory::View& v, const inventory::Case& cs) {
  Picker& p = g_picker[cs.index & 3];
  ImGui::SetNextItemWidth(scaled(220));
  ImGui::InputTextWithHint("##q", "search", p.query, sizeof(p.query));
  if (ImGui::BeginTabBar("tabs")) {
    for (int t = 0; t < kTabCount; ++t)
      if (ImGui::BeginTabItem(kTabNames[t])) {
        p.tab = t;
        ImGui::EndTabItem();
      }
    ImGui::EndTabBar();
  }
  int greyed = 0, theirs = 0;  // greyed: cannot go into an inventory now; theirs: another character's
  if (ImGui::BeginChild("list", ImVec2(0, scaled(150)), ImGuiChildFlags_Borders)) {
    for (const items::Info& i : items::kItems) {
      if (tab_of(i) != p.tab && !(p.query[0] && tab_of(i) >= 0)) continue;
      if (!inventory::carried(static_cast<int>(&i - items::kItems))) continue;  // the item list's leftovers
      char buf[64];
      const char* name = list_name(i, buf, sizeof(buf));
      if (!matches_query(name, p.query)) continue;
      // At the pause, what cannot go into an inventory now is greyed, and always what
      // the game keeps for the other character (it can be picked, to see why).
      char whose[128];
      const char* other = not_theirs(i, cs.name, whose, sizeof(whose));
      const bool grey = other || (v.paused && v.avail[&i - items::kItems] == inventory::kNever);
      if (grey) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      if (ImGui::Selectable(name, p.picked == i.id)) {
        p.picked = i.id;
        // Its model and sounds loaded while the Add is still to be pressed.
        if (!other && v.paused && v.avail[&i - items::kItems] == inventory::kLoadable) inventory::preload_soon(i.id);
      }
      if (grey) {
        ImGui::PopStyleColor();
        item_help(other ? other : avail_help(inventory::kNever, v.area_known));
        ++(other ? theirs : greyed);
      }
    }
  }
  ImGui::EndChild();
  if (greyed) ImGui::TextDisabled("Greyed: cannot go into an inventory during play - hover one for why.");
  if (theirs) ImGui::TextDisabled("Greyed: the other character's weapon - hover it for why.");
  const items::Info* pick = items::find(static_cast<uint32_t>(p.picked));
  if (pick && tab_of(*pick) < 0) pick = nullptr;
  const uint8_t pick_avail = pick ? v.avail[pick - items::kItems] : static_cast<uint8_t>(inventory::kNever);
  char whose[128];
  const char* pick_other = pick ? not_theirs(*pick, cs.name, whose, sizeof(whose)) : nullptr;
  ImGui::BeginDisabled(!pick || !v.paused || pick_avail == inventory::kNever || pick_other);
  if (pick && pick->cat != 1) {
    ImGui::SetNextItemWidth(scaled(60));
    ImGui::InputInt("##count", &p.count, 0, 0);
    if (p.count < 1) p.count = 1;
    if (pick->most > 0 && p.count > pick->most) p.count = pick->most;
    ImGui::SameLine();
  }
  char label[96];
  std::snprintf(label, sizeof(label), "Add %s", pick ? pick->name : "");
  if (ImGui::Button(label) && pick) inventory::add(cs.index, pick->id, pick->cat == 1 ? 1 : p.count);
  ImGui::EndDisabled();
  const char* why = pick_other ? pick_other : pick && v.paused ? avail_help(pick_avail, v.area_known) : nullptr;
  item_help(why ? why
                : "Into the first free slot the game picks for it, as a pickup goes in (ammo and healing in the even "
                  "ones); a weapon comes loaded. Up to what one slot holds.");
}

// "All": ticked when every row is, mixed when some are; a click ticks them all
// (or, all ticked, unticks them).
bool tick_all(const char* label, int on, int of, bool* all) {
  bool v = of > 0 && on == of;
  const bool some = on > 0 && on < of;
  ImGui::BeginDisabled(of == 0);
  if (some) ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
  const bool clicked = ImGui::Checkbox(label, &v);
  if (some) ImGui::PopItemFlag();
  ImGui::EndDisabled();
  *all = v;
  return clicked;
}

// A button's label with how many it works on ("Store 3"), and its fixed id.
void numbered(char* out, size_t n, const char* text, int count, const char* id) {
  if (count > 1) std::snprintf(out, n, "%s %d###%s", text, count, id);
  else std::snprintf(out, n, "%s###%s", text, id);
}

// Ticks on the cases' rows, each with the id it was made on: an item that changed
// under it drops it.
struct CaseTick {
  int id = 0;
  bool on = false;
};
CaseTick g_case_tick[game::kMaxChars][game::kSlots];

// A character's case: each item with its tick, its slot, its count (Apply / Undo
// when one is typed), Store (into the item box, beside it below) and x - the empty
// slots left out; above, what works on the ticked rows and the typed counts together.
void draw_case(const inventory::View& v, int ci, const itembox::View& box) {
  const inventory::Case& cs = v.cases[ci];
  const bool can_store = box.found && box.in_play;
  int on = 0, of = 0, typed = 0, movable = 0;
  for (int s = 0; s < game::kSlots; ++s) {
    const inventory::Slot& sl = cs.slot[s];
    CaseTick& t = g_case_tick[ci][s];
    Edit& e = g_count[ci][s];
    if (t.id && (!sl.used || t.id != sl.id)) t = CaseTick{};
    if (!sl.used) {
      e.typing = false;
      continue;
    }
    ++of;
    on += t.on;
    movable += t.on && sl.equip != 1;
    typed += e.typing && e.v != sl.num;
  }
  if (!of) {
    ImGui::TextDisabled("The inventory is empty.");
    return;
  }
  char label[64], id[32];
  ImGui::BeginDisabled(!v.paused);
  bool all = false;
  std::snprintf(label, sizeof(label), "All##case%d", ci);
  if (tick_all(label, on, of, &all))
    for (int s = 0; s < game::kSlots; ++s)
      if (cs.slot[s].used) g_case_tick[ci][s] = CaseTick{cs.slot[s].id, all};
  item_help("Tick every item in the case - or, all ticked, untick them. Ticked items can be stored or thrown away "
            "together (not the one in hand).");
  ImGui::SameLine();
  ImGui::BeginDisabled(!typed);
  std::snprintf(id, sizeof(id), "case apply%d", ci);
  numbered(label, sizeof(label), "Apply all", typed, id);
  if (ImGui::SmallButton(label)) {
    for (int s = 0; s < game::kSlots; ++s) {
      Edit& e = g_count[ci][s];
      if (cs.slot[s].used && e.typing && e.v != cs.slot[s].num) inventory::set_count(cs.index, s, e.v);
      e.typing = false;
    }
  }
  ImGui::EndDisabled();
  item_help("Make every count typed in the list below, all at once.");
  if (can_store) {
    ImGui::SameLine();
    ImGui::BeginDisabled(!movable || !box.can_move);
    std::snprintf(id, sizeof(id), "case store%d", ci);
    numbered(label, sizeof(label), "Store", movable, id);
    if (ImGui::SmallButton(label)) {
      std::vector<itembox::Change> batch;
      for (int s = 0; s < game::kSlots; ++s)
        if (g_case_tick[ci][s].on && cs.slot[s].used && cs.slot[s].equip != 1)
          batch.push_back(itembox::Change{itembox::Op::kStore, -1, cs.slot[s].id, 0, cs.index, s});
      itembox::apply(batch);
      for (CaseTick& t : g_case_tick[ci]) t = CaseTick{};
    }
    ImGui::EndDisabled();
    item_help("Put every ticked item in the item box (below), as Store beside each one does - not the one in "
              "hand, nor what the box does not take (a scene's own weapon).");
  }
  if (v.can_add) {
    ImGui::SameLine();
    ImGui::BeginDisabled(!movable);
    std::snprintf(id, sizeof(id), "case throw%d", ci);
    numbered(label, sizeof(label), "Throw away", movable, id);
    if (ImGui::SmallButton(label)) {
      for (int s = 0; s < game::kSlots; ++s)
        if (g_case_tick[ci][s].on && cs.slot[s].used && cs.slot[s].equip != 1) inventory::throw_away(cs.index, s);
      for (CaseTick& t : g_case_tick[ci]) t = CaseTick{};
    }
    ImGui::EndDisabled();
    item_help("Throw every ticked item away (not the one in hand).");
  }
  ImGui::EndDisabled();

  if (!ImGui::BeginTable("case", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) return;
  ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(24));
  ImGui::TableSetupColumn("slot", ImGuiTableColumnFlags_WidthFixed, scaled(16));
  ImGui::TableSetupColumn("item", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("count", ImGuiTableColumnFlags_WidthFixed, scaled(100));
  ImGui::TableSetupColumn("do", ImGuiTableColumnFlags_WidthFixed, scaled(150));
  int kept_out = 0;  // rows whose Store is greyed: what the box does not take
  for (int s = 0; s < game::kSlots; ++s) {
    const inventory::Slot& sl = cs.slot[s];
    if (!sl.used) continue;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    CaseTick& t = g_case_tick[ci][s];
    bool tick = t.on;
    std::snprintf(id, sizeof(id), "##ct%d%d", ci, s);
    ImGui::BeginDisabled(!v.paused);
    if (ImGui::Checkbox(id, &tick)) t = CaseTick{sl.id, tick};
    ImGui::EndDisabled();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%d", s + 1);
    item_help("Its slot in the inventory's 3 x 3 grid.");
    ImGui::TableNextColumn();
    char buf[32];
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item_name(sl.id, buf, sizeof(buf)));
    if (sl.equip == 1) {
      ImGui::SameLine();
      ImGui::TextColored(kOrange, "(in hand)");
    }
    ImGui::TableNextColumn();
    const items::Info* info = items::find(static_cast<uint32_t>(sl.id));
    Edit& e = g_count[ci][s];
    if (!e.typing) e.v = sl.num;
    if (info && (info->magazine || sl.most > 1)) {
      ImGui::BeginDisabled(!v.paused);
      ImGui::SetNextItemWidth(scaled(50));
      std::snprintf(id, sizeof(id), "##n%d%d", ci, s);
      if (ImGui::InputInt(id, &e.v, 0, 0)) e.typing = true;
      ImGui::EndDisabled();
      item_help(info->magazine ? "The rounds loaded, up to the gun's capacity - made with Apply."
                               : "How many, up to the most one slot holds - made with Apply.");
      ImGui::SameLine();
      ImGui::TextDisabled("/%d", sl.most);
    }
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(!v.paused);
    if (e.typing && e.v != sl.num) {
      std::snprintf(id, sizeof(id), "Apply##p%d%d", ci, s);
      if (ImGui::SmallButton(id)) {
        inventory::set_count(cs.index, s, e.v);
        e.typing = false;
      }
      ImGui::SameLine();
      std::snprintf(id, sizeof(id), "Undo##u%d%d", ci, s);
      if (ImGui::SmallButton(id)) e.typing = false;
      ImGui::SameLine();
    } else if (e.typing) {
      e.typing = false;  // typed back to what it is
    }
    if (can_store && sl.equip != 1) {
      char why[192];
      const char* out = not_for_box(sl, why, sizeof(why));
      kept_out += out != nullptr;
      ImGui::BeginDisabled(!box.can_move || out != nullptr);
      std::snprintf(id, sizeof(id), "Store##s%d%d", ci, s);
      if (ImGui::SmallButton(id))
        itembox::apply({itembox::Change{itembox::Op::kStore, -1, sl.id, 0, cs.index, s}});
      ImGui::EndDisabled();
      item_help(out ? out
                : info && info->cat == 1 && itembox::most_of(sl.id) <= 1
                    ? "Put it in the item box (below), with its rounds and upgrades."
                    : "Put all of it in the item box (below), onto what the box has of it first.");
      ImGui::SameLine();
    }
    if (v.can_add && sl.equip != 1) {
      std::snprintf(id, sizeof(id), "x##t%d%d", ci, s);
      if (ImGui::SmallButton(id)) inventory::throw_away(cs.index, s);
      item_help("Throw it away.");
    }
    ImGui::EndDisabled();
  }
  ImGui::EndTable();
  if (kept_out) ImGui::TextDisabled("Store is greyed for what the box does not take - hover it for why.");
}

// The item box's section (below, with the window beside Item Management).
void draw_box_section(const itembox::View& v, const inventory::View* cases);

void draw_panel() {
  const dispatch::Snapshot s = dispatch::snapshot();
  const ImGuiIO& io = ImGui::GetIO();
  // NewFrame has run, so what ImGui wants this frame is settled; the input
  // guard reads these from the game's thread.
  InterlockedExchange(&g_capture_mouse, io.WantCaptureMouse ? 1 : 0);
  InterlockedExchange(&g_capture_keyboard, io.WantTextInput ? 1 : 0);
  // Resizable, not auto-fitted (an auto-fitting window whose wrapped text
  // measures itself against its own width shrinks a little every frame once a
  // scrollbar takes some of it), and never bigger than the display.
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - scaled(600), scaled(40)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(scaled(580), scaled(640)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(scaled(360), scaled(200)), io.DisplaySize);
  if (!ImGui::Begin(kPanelWindow, nullptr)) {
    ImGui::End();
    return;
  }
  keep_in_reach(io.DisplaySize);
  if (!s.ready) {
    ImGui::TextColored(kOrange, "Game hooks: %s", boot::done() ? boot::status() : "waiting for the game to start");
    ImGui::End();
    return;
  }
  if (!s.run.in_game) ImGui::TextDisabled("Waiting for a game (start a chapter or the Mercenaries)...");
  const cheats::Status st = cheats::status();
  int on = 0, results_on = 0, merc_on = 0;
  for (int k = 0; k < sw::kCount; ++k) {
    if (!cheats::enabled(static_cast<sw::Kind>(k))) continue;
    if (k == sw::kFreezePlaytime || k == sw::kNoDeathCount || k == sw::kNoMissCount) ++results_on;
    else if (k == sw::kEndlessCombo || k == sw::kComboBonus || k == sw::kAutoKill) ++merc_on;
    else ++on;
  }
  char head[64];
  std::snprintf(head, sizeof(head), "Cheats  (%d on)###cheats", on);
  if (section(head, &g_cheats_open)) draw_cheats(s, st);
  const merc::Status ms = merc::status();
  if (s.run.in_game && ms.match) {
    if (merc_on) std::snprintf(head, sizeof(head), "The Mercenaries  (%d on)###mercenaries", merc_on);
    else std::snprintf(head, sizeof(head), "The Mercenaries###mercenaries");
    if (section(head, &g_merc_open)) draw_mercenaries(ms);
  }
  if (s.run.in_game && !s.run.mercenaries) {
    if (results_on) std::snprintf(head, sizeof(head), "Results  (%d on)###results", results_on);
    else std::snprintf(head, sizeof(head), "Results###results");
    if (section(head, &g_results_open)) draw_results(s, st);
  }
  if (s.run.in_game && !menus::view().going) {  // leaving for Chapter Select: nothing more is changed
    const inventory::View v = inventory::view();
    const itembox::View box = itembox::view();
    for (int c = 0; c < v.ncases; ++c) {
      int used = 0;
      for (const inventory::Slot& sl : v.cases[c].slot) used += sl.used;
      std::snprintf(head, sizeof(head), "%s's inventory  (%d/9)###case%d", v.cases[c].name, used, v.cases[c].index);
      if (section(head, &g_case_open[v.cases[c].index & 3])) {
        ImGui::PushID(c);
        draw_case(v, c, box);
        if (v.can_add) {
          char sub[64];
          std::snprintf(sub, sizeof(sub), "Add to %s's inventory###add%d", v.cases[c].name, v.cases[c].index);
          subsection(sub, &g_add_open[v.cases[c].index & 3], [&] { draw_add(v, v.cases[c]); });
        }
        ImGui::PopID();
      }
    }
    if (v.note[0]) ImGui::TextColored(kOrange, "%s", v.note);
    if (box.found && box.in_play) {
      draw_box_section(box, &v);
      if (box.note[0]) ImGui::TextColored(kOrange, "%s", box.note);
    }
  }
  ImGui::Separator();
  ImGui::TextDisabled("F%d hides this panel", config::get().toggle_key - 0x6F);
  ImGui::End();
}

// --- the item box: the panel's at the pause, the window beside Item Management ---------------
// A count typed or a tick made on an entry keeps the id the entry had then: an
// entry changed since (moved, used, a new item in its place) drops them rather
// than change the wrong item.
struct BoxRow {
  int id = 0;
  int count = 0;  // typed (0: none)
  bool tick = false;
};
BoxRow g_box_row[itembox::kMaxEntries];
// A treasure's, by its id (the kinds held come and go): one no longer held drops them.
BoxRow g_treasure_row[itembox::kMaxTreasures];
int g_take_into = -1;  // the character Take puts an entry in (sPlayer's index)
int g_box_tab = kTabWeapons;  // the box's tab shown
char g_box_filter[48] = "";   // what narrows its list

struct BoxPicker {
  char query[48] = "";
  int tab = 0;
  int num[items::kCount] = {};  // ticked: how many (0: not ticked)
};
BoxPicker g_box_pick;

void forget_box_edits() {
  for (BoxRow& r : g_box_row) r = BoxRow{};
  for (BoxRow& r : g_treasure_row) r = BoxRow{};
}

// A treasure's row: its own, or a free one.
BoxRow& treasure_row(int id) {
  BoxRow* free_row = nullptr;
  for (BoxRow& r : g_treasure_row) {
    if (r.id == id) return r;
    if (!r.id && !free_row) free_row = &r;
  }
  static BoxRow spare;
  BoxRow& r = free_row ? *free_row : spare;
  r = BoxRow{id, 0, false};
  return r;
}

// Whether one of the characters in play is this one (by name), at the pause.
bool has_character(const inventory::View* cases, const char* who) {
  for (int c = 0; cases && c < cases->ncases; ++c)
    if (!std::strcmp(cases->cases[c].name, who)) return true;
  return false;
}

// Where the box's Add files an item, or -1: what the case's Add offers, as far as
// the box's own store takes it and a chapter's start leaves it there - and the
// game's treasures (its own table of them) under Treasures, for the treasures held.
// An extra episode's box leaves out what the game keeps for a character who is not
// in that episode (Chris and Sheva's own weapons): it could never be taken there.
int box_add_tab(const items::Info& i, const itembox::View& v, const inventory::View* cases) {
  if (items::treasure(i.id)) return v.treasures ? kTabTreasures : -1;
  if (v.episode && i.only && cases && !has_character(cases, i.only)) return -1;
  const bool offered = tab_of(i) >= 0 && itembox::most_of(i.id) > 0 && itembox::kept(i.id) &&
                       inventory::carried(static_cast<int>(&i - items::kItems));
  return offered ? tab_of(i) : -1;
}

// The most of an item one tick of the Add list puts in: what an entry of the box
// holds of it - of a treasure, what an entry of the treasures' list holds.
int box_add_most(const items::Info& i, const itembox::View& v) {
  return items::treasure(i.id) ? v.tmost : itembox::most_of(i.id);
}

// A ticked item's count to start from: what it comes with, within an entry (one
// treasure).
int box_default(const items::Info& i, const itembox::View& v) {
  const int most = box_add_most(i, v);
  if (most <= 1 || items::treasure(i.id)) return 1;
  return std::clamp(i.num > 0 ? i.num : 1, 1, most);
}

// The treasures' rows (`order`: the kinds in the list's order; `keeps`: what the
// filter keeps), in a table `height` tall: each with its tick, how many are held
// (Apply / Undo when one is typed) and x.
void draw_treasure_rows(const itembox::View& v, const int* order, int n, const bool* keeps, float height) {
  if (!ImGui::BeginTable("treasures", 4,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                         ImVec2(0.0f, height)))
    return;
  ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(24));
  ImGui::TableSetupColumn("item", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("count", ImGuiTableColumnFlags_WidthFixed, scaled(110));
  ImGui::TableSetupColumn("do", ImGuiTableColumnFlags_WidthFixed, scaled(110));
  for (int k = 0; k < n; ++k) {
    if (!keeps[order[k]]) continue;
    const itembox::Treasure& tr = v.treasure[order[k]];
    BoxRow& r = treasure_row(tr.id);
    char id[32], buf[48], help[96];
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(!v.editable);
    bool t = r.tick;
    std::snprintf(id, sizeof(id), "##tt%d", tr.id);
    if (ImGui::Checkbox(id, &t)) r.tick = t;
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item_name(tr.id, buf, sizeof(buf)));
    if (const items::Treasure* worth = items::treasure(static_cast<uint32_t>(tr.id))) {
      std::snprintf(help, sizeof(help), "The shop pays %d gold for one.", worth->price);
      item_help(help);
    }
    ImGui::TableNextColumn();
    int num = r.count > 0 ? r.count : tr.count;
    ImGui::SetNextItemWidth(scaled(56));
    std::snprintf(id, sizeof(id), "##tn%d", tr.id);
    if (ImGui::InputInt(id, &num, 0, 0)) {
      num = std::clamp(num, 1, v.tmost);
      r.count = num == tr.count ? 0 : num;
    }
    std::snprintf(help, sizeof(help), "How many you hold, up to %d - made with Apply.", v.tmost);
    item_help(help);
    ImGui::SameLine();
    ImGui::TextDisabled("/%d", v.tmost);
    ImGui::TableNextColumn();
    if (r.count > 0) {
      std::snprintf(id, sizeof(id), "Apply##tp%d", tr.id);
      if (ImGui::SmallButton(id)) {
        itembox::apply({itembox::Change{itembox::Op::kTreasureSet, -1, tr.id, r.count}});
        r.count = 0;
      }
      ImGui::SameLine();
      std::snprintf(id, sizeof(id), "Undo##tu%d", tr.id);
      if (ImGui::SmallButton(id)) r.count = 0;
      ImGui::SameLine();
    }
    std::snprintf(id, sizeof(id), "x##tx%d", tr.id);
    if (ImGui::SmallButton(id)) itembox::apply({itembox::Change{itembox::Op::kTreasureSet, -1, tr.id, 0}});
    item_help("Throw them away, all you hold.");
    ImGui::EndDisabled();
  }
  ImGui::EndTable();
}

// The box's entries under tabs for their kinds (the Add list's), in the Add list's
// order, a filter narrowing them: each with its tick, its count (Apply / Undo when
// one is typed), Take into a character's case (at the pause) and x; the treasures
// held under a tab of their own, alike but with no Take (they go in no inventory);
// above, what works on the ticked rows - under any tab - and the typed counts
// together. `cases`: the characters a Take can go to (at the pause), or none.
void draw_box_rows(const itembox::View& v, const inventory::View* cases) {
  const bool can_take = cases && cases->ncases > 0;
  if (can_take) {
    bool known = false;
    for (int c = 0; c < cases->ncases; ++c) known = known || cases->cases[c].index == g_take_into;
    if (!known) {  // this machine's own character first
      g_take_into = cases->cases[0].index;
      for (int c = cases->ncases - 1; c >= 0; --c)
        if (cases->cases[c].control == game::Control::kLocal) g_take_into = cases->cases[c].index;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Take into:");
    for (int c = 0; c < cases->ncases; ++c) {
      ImGui::SameLine();
      char lb[48];
      std::snprintf(lb, sizeof(lb), "%s##take into%d", cases->cases[c].name, cases->cases[c].index);
      if (ImGui::RadioButton(lb, g_take_into == cases->cases[c].index)) g_take_into = cases->cases[c].index;
    }
    item_help("Whose inventory Take puts an item in.");
  }
  const char* take_who = nullptr;  // that character's name
  for (int c = 0; can_take && c < cases->ncases; ++c)
    if (cases->cases[c].index == g_take_into) take_who = cases->cases[c].name;
  // The entries in the Add list's order (one item's by their place in the box), how
  // many each tab holds and how many of those the filter keeps.
  int order[itembox::kMaxEntries], n = 0;
  int held[kTabBanks] = {}, matched[kTabBanks] = {};
  int rank[itembox::kMaxEntries] = {}, kind[itembox::kMaxEntries] = {};
  bool keeps[itembox::kMaxEntries] = {};
  char buf[48];
  for (int i = 0; i < v.size && i < itembox::kMaxEntries; ++i) {
    BoxRow& r = g_box_row[i];
    if (r.id && r.id != v.entry[i].id) r = BoxRow{};
    if (!v.entry[i].id) continue;
    order[n++] = i;
    rank[i] = rank_of(v.entry[i].id);
    kind[i] = box_tab_of(v.entry[i].id);
    keeps[i] = matches_query(item_name(v.entry[i].id, buf, sizeof(buf)), g_box_filter);
    ++held[kind[i]];
    matched[kind[i]] += keeps[i];
  }
  std::stable_sort(order, order + n, [&](int a, int b) { return rank[a] < rank[b]; });
  // The treasures held, likewise; a row of a kind no longer held is let go.
  int torder[itembox::kMaxTreasures], tn = 0;
  bool tkeeps[itembox::kMaxTreasures] = {};
  for (BoxRow& r : g_treasure_row) {
    bool now = false;
    for (int k = 0; k < v.tkinds; ++k) now = now || v.treasure[k].id == r.id;
    if (r.id && !now) r = BoxRow{};
  }
  for (int k = 0; v.treasures && k < v.tkinds && k < itembox::kMaxTreasures; ++k) {
    torder[tn++] = k;
    tkeeps[k] = matches_query(item_name(v.treasure[k].id, buf, sizeof(buf)), g_box_filter);
    ++held[kTabTreasures];
    matched[kTabTreasures] += tkeeps[k];
  }
  std::stable_sort(torder, torder + tn,
                   [&](int a, int b) { return rank_of(v.treasure[a].id) < rank_of(v.treasure[b].id); });
  if (ImGui::BeginTabBar("box banks")) {
    for (int t = 0; t < kTabBanks; ++t) {
      if ((t == kTabOther && !held[t]) || (t == kTabTreasures && !v.treasures)) continue;
      char tab[48];
      std::snprintf(tab, sizeof(tab), "%s %d###bank%d", tab_name(t), held[t], t);
      if (ImGui::BeginTabItem(tab)) {
        g_box_tab = t;
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }
  // Its tab gone (the box holds nothing of Other any more): the tab bar brings back
  // the tab shown before it from the next frame on.
  if ((g_box_tab == kTabOther && !held[kTabOther]) || (g_box_tab == kTabTreasures && !v.treasures))
    g_box_tab = kTabWeapons;
  const int tab = g_box_tab;
  if (!n && !tn) {
    ImGui::TextDisabled(v.treasures ? "The box is empty, and no treasures are held." : "The box is empty.");
    return;
  }
  ImGui::SetNextItemWidth(scaled(220));
  ImGui::InputTextWithHint("##box filter", "type to narrow the list", g_box_filter, sizeof(g_box_filter));
  if (g_box_filter[0]) {
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##box filter")) g_box_filter[0] = '\0';
    ImGui::SameLine();
    ImGui::TextDisabled("%d of %d shown", matched[tab], held[tab]);
  }
  const bool treasures = tab == kTabTreasures;
  auto shown = [&](int i) { return kind[i] == tab && keeps[i]; };
  int on = 0, of = 0, on_shown = 0, typed = 0, takes = 0;  // takes: the box's entries ticked
  int below = 0;  // the counts below their most, under every tab (Max all)
  for (int k = 0; k < n; ++k) {
    const BoxRow& r = g_box_row[order[k]];
    on += r.tick;
    takes += r.tick;
    typed += r.count > 0;
    const int most = itembox::most_of(v.entry[order[k]].id);
    below += most > 1 && v.entry[order[k]].count < most;
    if (shown(order[k])) {
      ++of;
      on_shown += r.tick;
    }
  }
  for (int k = 0; k < tn; ++k) {
    const BoxRow& r = treasure_row(v.treasure[torder[k]].id);
    on += r.tick;
    typed += r.count > 0;
    below += v.treasure[torder[k]].count < v.tmost;
    if (treasures && tkeeps[torder[k]]) {
      ++of;
      on_shown += r.tick;
    }
  }
  char label[64];
  ImGui::BeginDisabled(!v.editable);
  bool all = false;
  if (tick_all("All##box", on_shown, of, &all)) {
    for (int k = 0; k < n; ++k)
      if (shown(order[k])) g_box_row[order[k]] = BoxRow{v.entry[order[k]].id, g_box_row[order[k]].count, all};
    for (int k = 0; treasures && k < tn; ++k)
      if (tkeeps[torder[k]]) treasure_row(v.treasure[torder[k]].id).tick = all;
  }
  item_help(can_take ? "Tick every item the list shows (the tab's, and what the filter keeps) - or, all ticked, "
                       "untick them. Ticked items can be taken or thrown away together, from every tab at once."
                     : "Tick every item the list shows (the tab's, and what the filter keeps) - or, all ticked, "
                       "untick them. Ticked items can be thrown away together, from every tab at once.");
  ImGui::SameLine();
  ImGui::BeginDisabled(!typed);
  numbered(label, sizeof(label), "Apply all", typed, "box apply");
  if (ImGui::SmallButton(label)) {
    std::vector<itembox::Change> batch;
    for (int k = 0; k < n; ++k) {
      const int i = order[k];
      if (g_box_row[i].count > 0) batch.push_back(itembox::Change{itembox::Op::kSet, i, v.entry[i].id, g_box_row[i].count});
    }
    for (int k = 0; k < tn; ++k) {
      const BoxRow& r = treasure_row(v.treasure[torder[k]].id);
      if (r.count > 0) batch.push_back(itembox::Change{itembox::Op::kTreasureSet, -1, r.id, r.count});
    }
    itembox::apply(batch);
    for (BoxRow& r : g_box_row) r.count = 0;
    for (BoxRow& r : g_treasure_row) r.count = 0;
  }
  ImGui::EndDisabled();
  item_help("Make every count typed in the list, under any tab, all at once.");
  // Every count the list has to its most, under every tab: each entry that stacks,
  // each treasure held - the changes Apply makes, in one batch.
  ImGui::SameLine();
  ImGui::BeginDisabled(!below);
  numbered(label, sizeof(label), "Max all", below, "box max");
  if (ImGui::SmallButton(label)) {
    std::vector<itembox::Change> batch;
    for (int k = 0; k < n; ++k) {
      const itembox::Entry& en = v.entry[order[k]];
      const int most = itembox::most_of(en.id);
      if (most > 1 && en.count < most) batch.push_back(itembox::Change{itembox::Op::kSet, order[k], en.id, most});
    }
    for (int k = 0; k < tn; ++k) {
      const itembox::Treasure& tr = v.treasure[torder[k]];
      if (tr.count < v.tmost) batch.push_back(itembox::Change{itembox::Op::kTreasureSet, -1, tr.id, v.tmost});
    }
    itembox::apply(batch);
    for (BoxRow& r : g_box_row) r.count = 0;
    for (BoxRow& r : g_treasure_row) r.count = 0;
  }
  ImGui::EndDisabled();
  item_help(v.treasures ? "Fill every count in the box to the most beside it, under every tab at once (whatever the "
                          "filter shows): each entry of what stacks - ammo, recovery items, grenades and eggs - and "
                          "each treasure you hold. A weapon keeps the rounds it has loaded; nothing new is added."
                        : "Fill every count in the box to the most beside it, under every tab at once (whatever the "
                          "filter shows): each entry of what stacks - ammo, recovery items, grenades and eggs. A "
                          "weapon keeps the rounds it has loaded; nothing new is added.");
  if (can_take) {
    ImGui::SameLine();
    ImGui::BeginDisabled(!takes || !v.can_move || v.loading);
    numbered(label, sizeof(label), "Take", takes, "box take");
    if (ImGui::SmallButton(label)) {
      std::vector<itembox::Change> batch;
      for (int k = 0; k < n; ++k) {
        const int i = order[k];
        if (g_box_row[i].tick) batch.push_back(itembox::Change{itembox::Op::kTake, i, v.entry[i].id, 0, g_take_into});
      }
      itembox::apply(batch);
      for (BoxRow& r : g_box_row) r.tick = false;
    }
    ImGui::EndDisabled();
    item_help(v.treasures ? "Put every ticked item, under any tab, in the inventory chosen above, as Take beside each "
                            "one does - in the list's order, as long as there is room. Treasures stay where they are: "
                            "they go in no inventory; nor does a weapon the game keeps for the other character."
                          : "Put every ticked item, under any tab, in the inventory chosen above, as Take beside each "
                            "one does - in the list's order, as long as there is room - but a weapon the game keeps "
                            "for the other character.");
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!on);
  numbered(label, sizeof(label), "Throw away", on, "box throw");
  if (ImGui::SmallButton(label)) {
    std::vector<itembox::Change> batch;
    for (int k = 0; k < n; ++k) {
      const int i = order[k];
      if (g_box_row[i].tick) batch.push_back(itembox::Change{itembox::Op::kSet, i, v.entry[i].id, 0});
    }
    for (int k = 0; k < tn; ++k) {
      const BoxRow& r = treasure_row(v.treasure[torder[k]].id);
      if (r.tick) batch.push_back(itembox::Change{itembox::Op::kTreasureSet, -1, r.id, 0});
    }
    itembox::apply(batch);
    for (BoxRow& r : g_box_row) r.tick = false;
    for (BoxRow& r : g_treasure_row) r.tick = false;
  }
  ImGui::EndDisabled();
  item_help("Take every ticked item, under any tab, out of the box for good (a treasure: all you hold of it).");
  ImGui::EndDisabled();
  if (on > on_shown) {  // beside the buttons where it fits, else under them
    char more[48];
    std::snprintf(more, sizeof(more), "(%d ticked %s)", on - on_shown, g_box_filter[0] ? "not listed" : "under other tabs");
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(more).x) ImGui::NewLine();
    ImGui::TextDisabled("%s", more);
  }

  if (!held[tab]) {
    if (treasures) ImGui::TextDisabled("No treasures are held.");
    else ImGui::TextDisabled("Nothing in the box is filed under %s.", tab_name(tab));
    return;
  }
  if (!matched[tab]) {
    ImGui::TextDisabled("Nothing under %s matches \"%s\".", tab_name(tab), g_box_filter);
    return;
  }
  // As tall as its rows, up to ten, and half a row more: past ten, the half row and
  // the scroll bar say there is more. Each tab keeps its own scroll.
  const float height = (std::min(matched[tab], 10) + 0.5f) * ImGui::GetFrameHeightWithSpacing();
  if (treasures) {
    draw_treasure_rows(v, torder, tn, tkeeps, height);
    return;
  }
  char table_id[16];
  std::snprintf(table_id, sizeof(table_id), "box%d", tab);
  if (!ImGui::BeginTable(table_id, 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                         ImVec2(0.0f, height)))
    return;
  ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthFixed, scaled(24));
  ImGui::TableSetupColumn("item", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("count", ImGuiTableColumnFlags_WidthFixed, scaled(110));
  ImGui::TableSetupColumn("do", ImGuiTableColumnFlags_WidthFixed, scaled(can_take ? 150 : 110));
  int away = 0;    // rows that cannot go into an inventory during play
  int others = 0;  // rows the game keeps for the other character
  for (int k = 0; k < n; ++k) {
    const int i = order[k];
    if (!shown(i)) continue;
    const items::Info* info = items::find(static_cast<uint32_t>(v.entry[i].id));
    char whose[128];
    const char* other = can_take && info ? not_theirs(*info, take_who, whose, sizeof(whose)) : nullptr;
    others += other != nullptr;
    away += can_take && v.can_move && !other && v.entry[i].avail == inventory::kNever;
    const itembox::Entry& en = v.entry[i];
    BoxRow& r = g_box_row[i];
    char id[32];
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(!v.editable);
    bool t = r.tick;
    std::snprintf(id, sizeof(id), "##bt%d", i);
    if (ImGui::Checkbox(id, &t)) r = BoxRow{en.id, r.count, t};
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item_name(en.id, buf, sizeof(buf)));
    ImGui::TableNextColumn();
    const int most = itembox::most_of(en.id);
    if (most > 1) {
      int num = r.count > 0 ? r.count : en.count;
      ImGui::SetNextItemWidth(scaled(56));
      std::snprintf(id, sizeof(id), "##bn%d", i);
      if (ImGui::InputInt(id, &num, 0, 0)) {
        num = std::clamp(num, 1, most);
        r = BoxRow{en.id, num == en.count ? 0 : num, r.tick};
      }
      item_help("How many, up to what one entry of the box holds - made with Apply.");
      ImGui::SameLine();
      ImGui::TextDisabled("/%d", most);
    } else if (info && info->magazine) {
      ImGui::AlignTextToFramePadding();
      ImGui::TextDisabled("%d loaded", en.count);
    }
    ImGui::TableNextColumn();
    if (r.count > 0) {
      std::snprintf(id, sizeof(id), "Apply##bp%d", i);
      if (ImGui::SmallButton(id)) {
        itembox::apply({itembox::Change{itembox::Op::kSet, i, en.id, r.count}});
        r.count = 0;
      }
      ImGui::SameLine();
      std::snprintf(id, sizeof(id), "Undo##bu%d", i);
      if (ImGui::SmallButton(id)) r.count = 0;
      ImGui::SameLine();
    }
    if (can_take) {
      ImGui::BeginDisabled(!v.can_move || v.loading || en.avail == inventory::kNever || other);
      std::snprintf(id, sizeof(id), "Take##bk%d", i);
      if (ImGui::SmallButton(id)) itembox::apply({itembox::Change{itembox::Op::kTake, i, en.id, 0, g_take_into}});
      ImGui::EndDisabled();
      const char* why = other ? other : v.can_move ? avail_help(en.avail, cases->area_known) : nullptr;
      item_help(why        ? why
                : most > 1 ? "Put what one slot holds in the inventory chosen above (the rest stays in the box)."
                           : "Put it in the inventory chosen above, with its rounds and upgrades.");
      ImGui::SameLine();
    }
    std::snprintf(id, sizeof(id), "x##bx%d", i);
    if (ImGui::SmallButton(id)) itembox::apply({itembox::Change{itembox::Op::kSet, i, en.id, 0}});
    item_help("Throw it out of the box.");
    ImGui::EndDisabled();
  }
  ImGui::EndTable();
  if (away) ImGui::TextDisabled("Take is greyed for what cannot go into an inventory during play - hover it for why.");
  if (others) ImGui::TextDisabled("Take is greyed for the other character's weapon - hover it for why.");
}

void draw_box_add(const itembox::View& v, const inventory::View* cases) {
  BoxPicker& p = g_box_pick;
  ImGui::SetNextItemWidth(scaled(220));
  ImGui::InputTextWithHint("##box q", "search", p.query, sizeof(p.query));
  if (ImGui::BeginTabBar("box tabs")) {
    for (int t = 0; t < kTabCount; ++t)
      if (ImGui::BeginTabItem(kTabNames[t])) {
        p.tab = t;
        ImGui::EndTabItem();
      }
    if (v.treasures && ImGui::BeginTabItem(tab_name(kTabTreasures))) {
      p.tab = kTabTreasures;
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  // The list: a tab's items, or with a search every tab's; a ticked item that
  // stacks (and a treasure) has its count beside it.
  std::vector<int> listed;
  if (ImGui::BeginChild("box list", ImVec2(0, scaled(190)), ImGuiChildFlags_Borders)) {
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0.0f));
    for (int k = 0; k < items::kCount; ++k) {
      const items::Info& i = items::kItems[k];
      const int t = box_add_tab(i, v, cases);
      if (t < 0 || (t != p.tab && !p.query[0])) continue;
      char buf[64];
      const char* name = list_name(i, buf, sizeof(buf));
      if (!matches_query(name, p.query)) continue;
      listed.push_back(k);
      char lb[96];
      std::snprintf(lb, sizeof(lb), "%s##ba%d", name, k);
      bool on = p.num[k] > 0;
      if (ImGui::Checkbox(lb, &on)) p.num[k] = on ? box_default(i, v) : 0;
      const int most = box_add_most(i, v);
      if (on && most > 1) {
        ImGui::SameLine(std::max(scaled(260), ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + style.ItemSpacing.x));
        ImGui::SetNextItemWidth(scaled(70));
        std::snprintf(lb, sizeof(lb), "##bc%d", k);
        ImGui::InputInt(lb, &p.num[k], 0, 0);
        p.num[k] = std::clamp(p.num[k], 1, most);
        char h[96];
        if (t == kTabTreasures) std::snprintf(h, sizeof(h), "How many: up to %d of a treasure in all.", most);
        else std::snprintf(h, sizeof(h), "How many: up to %d fill one entry; more take the next.", most);
        item_help(h);
      }
    }
    ImGui::PopStyleVar();
    if (listed.empty()) ImGui::TextDisabled(p.query[0] ? "Nothing matches \"%s\"." : "Nothing here.", p.query);
  }
  ImGui::EndChild();
  int on = 0, listed_on = 0;
  for (int k = 0; k < items::kCount; ++k) on += p.num[k] > 0;
  for (const int k : listed) listed_on += p.num[k] > 0;
  char label[48];
  ImGui::BeginDisabled(listed.empty() || listed_on == static_cast<int>(listed.size()));
  std::snprintf(label, sizeof(label), "Tick all %d listed###box tick all", static_cast<int>(listed.size()));
  if (ImGui::SmallButton(label))
    for (const int k : listed)
      if (p.num[k] <= 0) p.num[k] = box_default(items::kItems[k], v);
  ImGui::EndDisabled();
  item_help("Tick every item the list shows: the tab's, or what the search keeps.");
  if (on) {
    ImGui::SameLine();
    if (ImGui::SmallButton("Untick all###box untick")) std::fill(std::begin(p.num), std::end(p.num), 0);
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%d ticked", on);
  ImGui::SameLine();
  ImGui::BeginDisabled(!v.editable || !on);
  numbered(label, sizeof(label), "Add", on, "box add");
  if (ImGui::SmallButton(label)) {
    std::vector<itembox::Change> batch;
    for (int k = 0; k < items::kCount; ++k) {
      if (p.num[k] <= 0) continue;
      const int id = items::kItems[k].id;
      batch.push_back(itembox::Change{items::treasure(id) ? itembox::Op::kTreasureAdd : itembox::Op::kAdd, -1, id,
                                      p.num[k]});
    }
    itembox::apply(batch);
    std::fill(std::begin(p.num), std::end(p.num), 0);
  }
  ImGui::EndDisabled();
  item_help("Put every ticked item in the box, as the box's own store does: what stacks fills the entries it "
            "has first, then empty ones; a weapon takes an entry each and comes loaded, with no upgrades. A "
            "treasure goes with the treasures you hold.");
}

// The box's section: its rows and its Add, and why it cannot be changed now. In an
// extra episode it is that episode's own box (the mod's, kept in a file beside it).
void draw_box_section(const itembox::View& v, const inventory::View* cases) {
  char head[96];
  if (v.episode) std::snprintf(head, sizeof(head), "Item box - %s  (%d/%d)###box", v.episode, v.used, v.size);
  else std::snprintf(head, sizeof(head), "Item box  (%d/%d)###box", v.used, v.size);
  if (!section(head, &g_box_open)) return;
  if (v.episode) {
    ImGui::PushTextWrapPos(0.0f);
    if (v.unreadable) {
      ImGui::TextColored(kOrange,
                         "This box's file, %s, is there but could not be read: it is left as it is, and the box "
                         "cannot be used until the game starts again (see RE5CabbyCodes.log).",
                         v.file);
      ImGui::PopTextWrapPos();
      return;
    }
    ImGui::TextDisabled("This episode's own box: the game has none in %s, so the mod keeps one in %s, beside "
                        "the mod. The story's box is not touched here.",
                        v.episode, v.file);
    ImGui::PopTextWrapPos();
  }
  draw_box_rows(v, cases);
  subsection("Add to the item box###box add list", &g_box_add_open, [&] { draw_box_add(v, cases); });
  if (v.unwritten) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(kOrange, "Not in %s yet: it could not be written (see RE5CabbyCodes.log) - tried again "
                                "every second.", v.file);
    ImGui::PopTextWrapPos();
  }
  if (!v.editable)
    ImGui::TextDisabled(v.open ? "Changes wait while the screen is busy - go back to its menu, your inventory or the box."
                               : "Pause the game to change the box.");
}

// --- Chapter Select (beside Item Management, beside the panel at the pause, Game Over, the results) ---
// The way back to the chapter list, in a window of its own that does not fold and stays in
// front of the item box's or the panel's, so the button is there whatever is done with that
// window (the user, 2026-10-31: "I want the button always visible"). Beside Item Management
// what was changed on the screen is saved first; at the pause it goes as the pause menu's QUIT
// does when it saves, once confirmed - the only place it asks (the user, 2026-10-31: "I only
// want the confirmation to exist while the player is in the game"); on the Game Over screen as
// its own QUIT does, at the chapter's results as their own quit does (the last chapter's too).
enum class CsPlace { kNone, kBox, kPause, kEnd };
CsPlace g_cs_confirm = CsPlace::kNone;  // the pause's button clicked: Yes or No asked

void draw_chapter_select() {
  const dispatch::Snapshot s = dispatch::snapshot();
  const menus::View m = menus::view();
  const CsPlace place = s.show_itembox  ? CsPlace::kBox
                        : s.show_panel  ? CsPlace::kPause
                        : s.show_end ? CsPlace::kEnd
                                        : CsPlace::kNone;
  if (g_cs_confirm != place || !m.ready) g_cs_confirm = CsPlace::kNone;  // asked elsewhere, or it cannot go now
  if (!s.ready || !m.found || place == CsPlace::kNone || !(m.offered || m.going)) return;
  const ImGuiIO& io = ImGui::GetIO();
  const ImGuiStyle& style = ImGui::GetStyle();
  // One width whatever it shows - the button, the question, "Going back to Chapter
  // Select..." - so it never grows over the window beside it.
  const float w = std::max(ImGui::CalcTextSize("Chapter Select...").x + 2 * style.FramePadding.x, scaled(170));
  const float outer = w + 2 * style.WindowPadding.x;
  const float right = place == CsPlace::kPause ? scaled(610) : place == CsPlace::kBox ? scaled(590) : scaled(20);
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - right, scaled(40)), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(outer, 0.0f), ImVec2(outer, FLT_MAX));
  const char* id = place == CsPlace::kPause ? kChapterPauseWindow : place == CsPlace::kBox ? kChapterWindow
                                                                                           : kChapterEndWindow;
  if (ImGui::Begin(id, nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    keep_in_reach(io.DisplaySize);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    if (m.going) {
      ImGui::TextColored(kOrange, "Going back to Chapter Select...");
    } else if (g_cs_confirm == CsPlace::kPause) {
      ImGui::TextUnformatted("Leave this chapter for the chapter list?");
      ImGui::TextDisabled("As QUIT, saving: your equipment and the item box are saved as they are now.");
      const float half = (w - style.ItemSpacing.x) / 2;
      if (ImGui::Button("Yes", ImVec2(half, 0.0f))) {
        menus::chapter_select();
        g_cs_confirm = CsPlace::kNone;
      }
      ImGui::SameLine();
      if (ImGui::Button("No", ImVec2(half, 0.0f))) g_cs_confirm = CsPlace::kNone;
    } else {
      ImGui::BeginDisabled(!m.ready);
      if (ImGui::Button("Chapter Select", ImVec2(w, 0.0f))) {
        if (place == CsPlace::kPause) g_cs_confirm = place;
        else menus::chapter_select();
      }
      ImGui::EndDisabled();
      item_help(!m.ready ? m.why
                : place == CsPlace::kPause
                    ? "Leave this chapter for the chapter list, as the pause menu's QUIT does when you save: your "
                      "equipment, the item box and the treasures are saved as they are now (the game's own save), "
                      "then the chapter list."
                : place == CsPlace::kEnd && m.where == menus::Where::kResults && m.last
                    ? "Leave for the chapter list instead of the main menu, where the game goes after the last "
                      "chapter's results: the chapter's end is saved already."
                : place == CsPlace::kEnd && m.where == menus::Where::kResults
                    ? "Leave for the chapter list instead of the next chapter, as the results' own quit does: the "
                      "chapter's end is saved already."
                : place == CsPlace::kEnd
                    ? "Leave for the chapter list instead of continuing, as this screen's QUIT does: the game goes "
                      "back to the last checkpoint and is saved; the item box stays as it is."
                    : "Leave Item Management for the chapter list instead of starting the chapter. What was "
                      "changed on this screen (items moved, bought, sold or upgraded) is saved first, as the "
                      "screen saves it when it closes.");
      if (m.note[0]) ImGui::TextColored(kOrange, "%s", m.note);
    }
    ImGui::PopTextWrapPos();
  }
  ImGui::End();
}

void draw_itembox() {
  const dispatch::Snapshot s = dispatch::snapshot();
  const ImGuiIO& io = ImGui::GetIO();
  InterlockedExchange(&g_capture_mouse, io.WantCaptureMouse ? 1 : 0);
  InterlockedExchange(&g_capture_keyboard, io.WantTextInput ? 1 : 0);
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - scaled(580), scaled(40)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(scaled(560), scaled(620)), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(scaled(360), scaled(200)), io.DisplaySize);
  if (!ImGui::Begin(kBoxWindow, nullptr)) {
    ImGui::End();
    return;
  }
  keep_in_reach(io.DisplaySize);
  const menus::View m = menus::view();
  const itembox::View v = itembox::view();
  if (!s.ready || !v.found) ImGui::TextColored(kOrange, "The item box was not understood (see RE5CabbyCodes.log).");
  else if (!v.open) ImGui::TextDisabled("Waiting for Item Management...");
  else if (!m.going) draw_box_section(v, nullptr);  // leaving: nothing more is changed
  if (v.note[0] && !m.going) ImGui::TextColored(kOrange, "%s", v.note);
  ImGui::Separator();
  ImGui::TextDisabled("F%d hides this window", config::get().toggle_key - 0x6F);
  ImGui::End();
}

// The visibility rule for this frame. What was typed or ticked belongs to the
// showing it was made in: it is let go once neither the panel's screen (the pause
// menu, Item Management) nor the panel over the game is up.
bool visible_now(const dispatch::Snapshot& s) {
  const bool at_screen = s.show_panel || s.show_itembox || s.show_end || config::get().always_show;
  const bool over_play = g_user_shown && (s.run.screen == game::Screen::kPlay || !s.ready);
  if (!at_screen) {
    g_user_hidden = false;  // the next time the screen comes up the panel is back
    g_cs_confirm = CsPlace::kNone;  // ... and Chapter Select asks again
    if (!over_play) {
      forget_edits();
      forget_box_edits();
      for (auto& row : g_case_tick)
        for (CaseTick& t : row) t = CaseTick{};
    }
  }
  if (s.ready && s.run.screen != game::Screen::kPlay) g_user_shown = false;
  return g_context_ready && ((at_screen && !g_user_hidden) || over_play);
}

}  // namespace

bool ensure_context(HWND hwnd) {
  if (g_context_ready) return true;
  if (!hwnd) return false;
  g_hwnd = hwnd;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  // Window positions persist beside the DLL, under the mod's own name rather
  // than a stray imgui.ini in the game folder.
  static char ini_path[MAX_PATH] = {};
  std::snprintf(ini_path, sizeof(ini_path), "%sRE5CabbyCodes.imgui.ini", config::dir());
  io.IniFilename = ini_path;
  // ImGui writes the file a moment after a change (wants_draw writes what is
  // still waiting when the panel goes): half a second, not its usual five, so
  // a game closed straight after a click loses nothing.
  io.IniSavingRate = 0.5f;
  // The panel's own lines in that file, registered before the first frame,
  // which is when ImGui reads it.
  ImGuiSettingsHandler panel{};
  panel.TypeName = "RE5CabbyCodes";
  panel.TypeHash = ImHashStr("RE5CabbyCodes");
  panel.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char* name) -> void* {
    return std::strcmp(name, "Panel") == 0 ? &g_cheats_open : nullptr;
  };
  panel.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
    int v = 0, i = 0;
    if (std::sscanf(line, "CheatsOpen=%d", &v) == 1) g_cheats_open = v != 0;
    else if (std::sscanf(line, "ResultsOpen=%d", &v) == 1) g_results_open = v != 0;
    else if (std::sscanf(line, "MercenariesOpen=%d", &v) == 1) g_merc_open = v != 0;
    else if (std::sscanf(line, "CaseOpen%d=%d", &i, &v) == 2 && i >= 0 && i < game::kMaxChars) g_case_open[i] = v != 0;
    else if (std::sscanf(line, "AddOpen%d=%d", &i, &v) == 2 && i >= 0 && i < game::kMaxChars) g_add_open[i] = v != 0;
    else if (std::sscanf(line, "BoxOpen=%d", &v) == 1) g_box_open = v != 0;
    else if (std::sscanf(line, "BoxAddOpen=%d", &v) == 1) g_box_add_open = v != 0;
  };
  panel.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* out) {
    out->appendf("[%s][Panel]\nCheatsOpen=%d\nResultsOpen=%d\nMercenariesOpen=%d\n", h->TypeName,
                 g_cheats_open ? 1 : 0, g_results_open ? 1 : 0, g_merc_open ? 1 : 0);
    for (int i = 0; i < game::kMaxChars; ++i)
      out->appendf("CaseOpen%d=%d\nAddOpen%d=%d\n", i, g_case_open[i] ? 1 : 0, i, g_add_open[i] ? 1 : 0);
    out->appendf("BoxOpen=%d\nBoxAddOpen=%d\n", g_box_open ? 1 : 0, g_box_add_open ? 1 : 0);
    out->append("\n");
  };
  ImGui::AddSettingsHandler(&panel);
  // No keyboard navigation: the game's menus keep their keys. Dragging is
  // limited to the title bar so a click on the panel's background cannot pick
  // the window up by accident.
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.ScaleAllSizes(kScale);  // the padding, spacing and the rest; the font has its own factor
  style.FontScaleMain = kScale;

  if (!ImGui_ImplWin32_Init(hwnd)) {
    logf("ERROR: ImGui Win32 backend init failed");
    ImGui::DestroyContext();
    return false;
  }
  // The game's window is a Unicode one (RegisterClassExW, "MTFramework").
  g_orig_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hk_wndproc)));
  g_context_ready = true;
  char cls[64] = "?";
  GetClassNameA(hwnd, cls, sizeof(cls));
  logf("overlay ready (hwnd=%p class '%s', scale %d%%, present thread %lu, window thread %lu)",
       static_cast<void*>(hwnd), cls, kScalePercent, GetCurrentThreadId(), GetWindowThreadProcessId(hwnd, nullptr));
  return true;
}

HWND window() { return g_context_ready ? g_hwnd : nullptr; }

bool visible() { return g_visible_flag != 0; }
bool capturing_mouse() { return g_capture_mouse != 0; }
bool capturing_keyboard() { return g_capture_keyboard != 0; }

void lock_imgui() {
  if (g_imgui_cs_ready) EnterCriticalSection(&g_imgui_cs);
}

void unlock_imgui() {
  if (g_imgui_cs_ready) LeaveCriticalSection(&g_imgui_cs);
}

// Called from the present hook with the context lock held.
bool wants_draw() {
  const bool was_visible = g_visible;
  g_visible = visible_now(dispatch::snapshot());
  InterlockedExchange(&g_visible_flag, g_visible ? 1 : 0);
  // ImGui's delay before it writes the layout file (the sections folded, the
  // panel's place) counts down only in frames, and the panel draws none once
  // it is gone - nor does the mod touch ImGui as the game exits. A change made
  // just before the pause closed would wait for the panel's next showing, and be
  // lost if the game ended first: so whatever is waiting is written now.
  if (was_visible && !g_visible && g_context_ready && GImGui->SettingsDirtyTimer > 0.0f && GImGui->IO.IniFilename)
    ImGui::SaveIniSettingsToDisk(GImGui->IO.IniFilename);
  if (!g_visible) {  // nothing is being taken from the game until it draws again
    InterlockedExchange(&g_capture_mouse, 0);
    InterlockedExchange(&g_capture_keyboard, 0);
  }
  if (!g_context_ready) return false;
  ImGuiIO& io = ImGui::GetIO();
  // The game hides the system pointer (and holds the mouse while it plays):
  // the panel draws one of its own.
  io.MouseDrawCursor = g_visible;
  if (g_visible) {
    // If a release was ever missed the panel would follow the pointer for ever;
    // the physical button is the truth. GetAsyncKeyState reads the physical
    // buttons and the messages the logical ones: with the buttons swapped in
    // Windows' mouse settings (left-handed) the panel's left button is the
    // physical right one (RE3CabbyCodes, 2026-10-13).
    static const int kVk[3] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON};
    const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
    for (int b = 0; b < 3; ++b) {
      const int vk = swapped && b < 2 ? kVk[1 - b] : kVk[b];
      if (io.MouseDown[b] && !(GetAsyncKeyState(vk) & 0x8000)) io.AddMouseButtonEvent(b, false);
    }
  } else {
    // Input made while the panel is away is not the panel's: ImGui only queues
    // events and drains them in NewFrame, which does not run while the panel is
    // hidden, so they would be replayed onto it the moment it came back (RE0).
    io.ClearEventsQueue();
    io.ClearInputMouse();
    io.ClearInputKeys();
  }
  return g_visible;
}

// Each screen its window: beside Item Management the item box, on the Game Over screen and the
// chapter's results none (unless the panel was opened over the game), elsewhere the panel - and
// Chapter Select's own beside either, where it is offered.
void draw_frame() {
  if (!g_visible) return;
  const dispatch::Snapshot s = dispatch::snapshot();
  if (s.show_itembox) {
    draw_itembox();
  } else if (!s.show_end || g_user_shown || config::get().always_show) {
    draw_panel();
  } else {
    const ImGuiIO& io = ImGui::GetIO();
    InterlockedExchange(&g_capture_mouse, io.WantCaptureMouse ? 1 : 0);
    InterlockedExchange(&g_capture_keyboard, io.WantTextInput ? 1 : 0);
  }
  draw_chapter_select();
}

void shutdown_imgui() {
  if (!g_context_ready) return;
  ImGui_ImplWin32_Shutdown();
  if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
  g_context_ready = false;
  g_visible = false;
  InterlockedExchange(&g_visible_flag, 0);
  InterlockedExchange(&g_capture_mouse, 0);
  InterlockedExchange(&g_capture_keyboard, 0);
}

void init() {
  adopt::init();
  if (!g_imgui_cs_ready) {  // before the window procedure or the present hook exists
    InitializeCriticalSection(&g_imgui_cs);
    g_imgui_cs_ready = true;
  }
}

void uninstall() {
  // Window procedure first: a message arriving after our image is gone would
  // jump into freed memory.
  if (g_orig_wndproc && g_hwnd && IsWindow(g_hwnd)) {
    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_orig_wndproc));
    g_orig_wndproc = nullptr;
  }
  dx9::uninstall();
  shutdown_imgui();
  logf("overlay hooks removed");
}

}  // namespace re5cc::overlay
