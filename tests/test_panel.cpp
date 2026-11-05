// Offline test of the panel (src/overlay.cpp): drawn headless with ImGui over a
// made-up game - paused in a chapter with Chris and Sheva, their cases, the
// profile's gold, the chapter's counters - and driven by clicks, as a
// player drives it. ImGui's test-engine hooks report each widget's label and
// place, so a click lands on the widget it names. overlay.cpp is built into this
// file (its insides are reached by name); everything it asks of the rest of the
// mod is made up below. Build (after `make`, for nothing - this test links no
// object of the mod's):
//
//   i686-w64-mingw32-g++ -std=c++20 -O1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DIMGUI_ENABLE_TEST_ENGINE
//       -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD -Icontrib/imgui -Isrc -static tests/test_panel.cpp
//       contrib/imgui/imgui.cpp contrib/imgui/imgui_draw.cpp contrib/imgui/imgui_tables.cpp
//       contrib/imgui/imgui_widgets.cpp contrib/imgui/backends/imgui_impl_win32.cpp
//       -luser32 -lgdi32 -ldwmapi -o tests/build/test_panel.exe
//   wine tests/build/test_panel.exe 'Z:\some\scratch\dir' [shots]
//
// With "shots" each step's panel is drawn to a picture (.ppm) in the scratch dir.
//
// Each frame runs the panel's own rule for what it keeps (visible_now, as the
// present hook does) before it draws.
//
// - the pause: the cheats (Steady aim and Automatic QTEs among them, and Open map
//   at chapter start, which a click flips and a second click puts back) with the
//   gold's editor and no exchange points, the results (one clock for the chapter,
//   Chris's and Sheva's counters), each character's inventory with its Add - and
//   no ImGui assert;
// - Set beside the gold asks for the gold typed, Set beside the time for the time;
// - a case lists only the slots in use; a slot's count typed and Apply asks for
//   that count; x throws away and Store puts it in the box (neither for the item in
//   hand); ticked items Store together; Store is greyed beside a scene's own weapon (the
//   Lantern) and beside a grenade launcher whose load is not known, and asks for one kept
//   as its load (the box takes only what the Add lists offer);
// - the pause's item box: a tab for each kind (the Add list's, with how many the box
//   holds of it; Other items only while it holds a key item or the like), each
//   listing its own entries; Take into the character picked, x, a count's Apply;
//   its ticks stay ticked - under another tab too - and Take takes the ticked of
//   every tab together; All ticks the tab's; the filter narrows the tab's list; an
//   item the area has not loaded (its model's archive) is asked for all the same
//   (the tick loads it first), one with no model of its own is not, nor anything
//   while a Take waits for the loader;
// - Add: the tabs file a firearm under Weapons, a grenade under Sub-weapons, and
//   spell the vests' Armor; a picked item and Add ask for that character, that
//   item, the count - one the area has not loaded too (picked, it is preloaded);
//   the item list's leftovers with no model are not offered (nor in the box's Add);
// - in play (not paused) the editors are disabled: nothing is asked;
// - the Mercenaries' pause: a section of their own with Endless combo, Combo Bonus always on and Kill enemies as
//   they appear (each click flipping its switch, and back) and the match's numbers - none at a chapter's pause;
// - the countdown switch in a chapter and in the Mercenaries; while a stage's countdown
//   runs, its seconds left beside it, Set asking for them (or those typed) at the pause
//   and nothing in play;
// - Item Management: the item box window instead of the panel - its entries under
//   their tabs with a count for what stacks (ammo and recovery items, as the game's
//   box has it), Apply all asking for the counts typed under any tab, Throw away for
//   the ticked entries, the Add list's tabs and Add - and nothing asked while the
//   screen is busy;
// - Max all (both places): every entry that stacks and every treasure held, under
//   every tab and whatever the filter shows, to its most in one batch - not a gun,
//   nor what is at its most already - the counts typed let go; nothing asked while
//   the screen is busy;
// - Chapter Select beside Item Management between chapters, in a window of its own:
//   in front of the item box's window when that one is focused; one click asks for it
//   once; still there and working with the item box's window folded, and its own
//   window does not fold; disabled while it cannot go (nothing asked); the box's
//   section put away while it goes; not offered beside the main menu's Item
//   Management, nor at the pause; on the Game Over screen and at the chapter's
//   results (the last chapter's too) its window alone, one click.
#include "../src/overlay.cpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// --- what overlay.cpp asks of the rest of the mod, made up ---------------------------------
namespace re5cc {
void logf(const char*, ...) {}
void describe_address(void*, char* out, unsigned n) { std::snprintf(out, n, "?"); }
namespace overlay::dx9 {
void uninstall() {}
}  // namespace overlay::dx9
namespace adopt {
void init() {}
}  // namespace adopt
namespace boot {
bool done() { return true; }
const char* status() { return "ready"; }
}  // namespace boot
namespace config {
Settings g_settings;
const Settings& get() { return g_settings; }
const char* dir() { return ".\\"; }
}  // namespace config
namespace dispatch {
Snapshot g_snap;
Snapshot snapshot() { return g_snap; }
}  // namespace dispatch
namespace cheats {
bool g_on[sw::kCount];
struct Asked {
  Edit what;
  int chara;
  double value;
};
std::vector<Asked> g_asked;
Status g_status;
bool enabled(sw::Kind k) { return g_on[k]; }
void set(sw::Kind k, bool on) { g_on[k] = on; }
bool available(sw::Kind) { return true; }
const char* unavailable_reason(sw::Kind) { return ""; }
Status status() { return g_status; }
void edit(Edit what, int chara, double value) { g_asked.push_back(Asked{what, chara, value}); }
}  // namespace cheats
namespace merc {
Status g_status;
Status status() { return g_status; }
}  // namespace merc
namespace inventory {
View g_view;
struct Asked {
  char op;
  int chara, slot, count, id;
};
std::vector<Asked> g_asked;
View view() { return g_view; }
void set_count(int c, int s, int n) { g_asked.push_back(Asked{'n', c, s, n, 0}); }
void add(int c, int id, int n) { g_asked.push_back(Asked{'a', c, 0, n, id}); }
void throw_away(int c, int s) { g_asked.push_back(Asked{'x', c, s, 0, 0}); }
bool can_change() { return true; }
std::vector<int> g_preloads;
void preload_soon(int id) { g_preloads.push_back(id); }
// The item list's leftovers (no model class): the arrows and the rocket among them.
bool carried(int k) { return items::kItems[k].id != 0x20A && items::kItems[k].id != 0x20B; }
}  // namespace inventory
namespace itembox {
View g_view;
std::vector<std::vector<Change>> g_asked;
View view() { return g_view; }
void apply(const std::vector<Change>& batch) { g_asked.push_back(batch); }
// The box's rules as the game has them: hand-to-hand never; ammo to treasures and
// the grenades stack to 999; the rest one an entry; a chapter keeps below 0x400 and
// the two vests.
int most_of(int id) {
  if (id <= 0 || id == 0x100) return 0;
  const bool stacks = (id >= 0x106 && id <= 0x108) || id == 0x10A || id == 0x136 || (id >= 0x13C && id <= 0x13E) ||
                      (id >= 0x201 && id <= 0x47E);
  return stacks ? 999 : 1;
}
bool kept(int id) { return id < 0x400 || id == 0x601 || id == 0x606; }
}  // namespace itembox
namespace menus {
View g_view;
int g_asked = 0;
View view() { return g_view; }
void chapter_select() { ++g_asked; }
}  // namespace menus
}  // namespace re5cc

// --- ImGui's test-engine hooks: what was drawn where ---------------------------------------
struct Drawn {
  ImGuiID id = 0;
  ImRect bb;
  std::string label;
  ImGuiItemStatusFlags flags = 0;  // a checkbox's ImGuiItemStatusFlags_Checked among them
  bool placed = false, named = false;
};
std::vector<Drawn> g_drawn;

void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID id, const ImRect& bb, const ImGuiLastItemData*) {
  for (auto it = g_drawn.rbegin(); it != g_drawn.rend(); ++it)
    if (it->id == id && !it->placed) {
      it->bb = bb;
      it->placed = true;
      return;
    }
  g_drawn.push_back(Drawn{id, bb, "", 0, true, false});
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id, const char* label, ImGuiItemStatusFlags flags) {
  for (auto it = g_drawn.rbegin(); it != g_drawn.rend(); ++it)
    if (it->id == id && !it->named) {
      it->label = label ? label : "";
      it->flags = flags;
      it->named = true;
      return;
    }
  g_drawn.push_back(Drawn{id, ImRect(), label ? label : "", flags, false, true});
}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }

namespace {

using namespace re5cc;
int g_fail = 0;
std::string g_shots;
int g_shot = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

// --- the made-up game ----------------------------------------------------------------------
void make_game(bool paused, bool merce) {
  dispatch::Snapshot& s = dispatch::g_snap;
  s = dispatch::Snapshot{};
  s.ready = true;
  s.run.in_game = true;
  s.run.mercenaries = merce;
  s.run.screen = paused ? game::Screen::kPause : game::Screen::kPlay;
  s.show_panel = paused;
  cheats::g_status = cheats::Status{};
  merc::g_status = merc::Status{};
  merc::g_status.match = merce;
  merc::g_status.offline = true;
  merc::g_status.step = 1;
  cheats::g_status.profile = true;
  cheats::g_status.gold = 12345;
  cheats::g_status.clock = 1234.5f;
  cheats::g_status.chars[0] = {2, 140, 400, 312};
  cheats::g_status.chars[1] = {0, 98, 0, 0};
  inventory::View& v = inventory::g_view;
  v = inventory::View{};
  v.paused = paused;
  v.can_add = true;
  // What this area has loaded: everything but the MP5 (as the area of the user's
  // crash, 2026-10-29), which the game's loader can load first - and the arrows,
  // which have no model of their own.
  v.area_known = true;
  for (int k = 0; k < items::kCount; ++k) {
    const int id = items::kItems[k].id;
    v.avail[k] = !paused ? inventory::kNever : id == 0x113 ? inventory::kLoadable : id == 0x20B ? inventory::kNever
                                                                                                 : inventory::kHere;
  }
  v.ncases = 2;
  v.cases[0].name = "Chris";
  v.cases[0].index = 0;
  v.cases[0].slot[1] = {true, 0x102, 10, 13, 1, {}};  // M92F (HG), in hand
  v.cases[0].slot[0] = {true, 0x201, 30, 50, 0, {}};  // Handgun ammo
  v.cases[0].slot[3] = {true, 0x106, 1, 5, 0, {}};    // Hand grenade
  v.cases[0].slot[2] = {true, 0x305, 1, 1, 0, {}};    // Herb (Green)
  v.cases[0].slot[6] = {true, 0x304, 1, 1, 0, {}};    // First aid spray (slots 5 and 6 empty)
  v.cases[1].name = "Sheva";
  v.cases[1].control = game::Control::kAi;
  v.cases[1].index = 1;
  v.cases[1].slot[1] = {true, 0x110, 9, 9, 1, {}};  // H&K P8 (HG)
  itembox::View& b = itembox::g_view;
  b = itembox::View{};
  b.found = true;
  b.in_play = !merce;
  b.editable = b.can_move = paused && !merce;
  b.size = 54;
  const uint8_t here = paused ? inventory::kHere : inventory::kNever;
  b.entry[0] = {0x102, 10, {}, here};   // M92F (HG), 10 rounds loaded
  b.entry[5] = {0x201, 120, {}, here};  // Handgun ammo
  b.used = 2;
  // The treasures held, the soul gem first in the game's list (the panel lists them
  // in the item table's order: the gold ring first).
  b.treasures = true;
  b.tmost = 999;
  b.treasure[0] = {0x46F, 1};  // Soul gem
  b.treasure[1] = {0x417, 2};  // Gold ring
  b.tkinds = 2;
}

void picture(const char* name);

void frame() {
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = 1.0f / 60.0f;
  io.DisplaySize = ImVec2(1920, 1080);
  g_drawn.clear();
  overlay::visible_now(dispatch::g_snap);  // what the panel keeps, as wants_draw() decides it each frame
  overlay::g_visible = true;
  ImGui::NewFrame();
  if (!dispatch::g_snap.show_end) {  // the panel's or the box's place (an end screen has neither)
    ImGui::SetNextWindowPos(ImVec2(960, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(940, 1050), ImGuiCond_Always);
  }
  overlay::draw_frame();
  ImGui::Render();
  for (ImTextureData* t : ImGui::GetPlatformIO().Textures)
    if (t->Status == ImTextureStatus_WantCreate || t->Status == ImTextureStatus_WantUpdates) {
      t->SetTexID(static_cast<ImTextureID>(1));
      t->SetStatus(ImTextureStatus_OK);
    }
}

void frames(int n) {
  for (int i = 0; i < n; ++i) frame();
}

const Drawn* find(const char* label, int nth = 0) {
  for (const Drawn& d : g_drawn)
    if (d.label == label && nth-- == 0) return &d;
  return nullptr;
}

// The widget with this label on the same line as `anchor`.
const Drawn* beside(const char* label, const Drawn* anchor) {
  if (!anchor) return nullptr;
  for (const Drawn& d : g_drawn) {
    const float mid = (d.bb.Min.y + d.bb.Max.y) * 0.5f;
    if (d.label == label && mid >= anchor->bb.Min.y && mid <= anchor->bb.Max.y) return &d;
  }
  return nullptr;
}

bool click_at(const Drawn* d) {
  if (!d) return false;
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 at((d->bb.Min.x + d->bb.Max.x) * 0.5f, (d->bb.Min.y + d->bb.Max.y) * 0.5f);
  io.AddMousePosEvent(at.x, at.y);
  frame();
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMouseButtonEvent(0, false);
  frames(2);
  return true;
}
bool click(const char* label, int nth = 0) { return click_at(find(label, nth)); }

// A click on a spot of the display (a window's title bar).
void click_point(float x, float y) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(x, y);
  frame();
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMouseButtonEvent(0, false);
  frames(2);
}

bool any_starting(const char* prefix) {
  for (const Drawn& d : g_drawn)
    if (d.label.rfind(prefix, 0) == 0) return true;
  return false;
}

bool ticked(const char* label) {
  const Drawn* d = find(label);
  return d && (d->flags & ImGuiItemStatusFlags_Checked);
}

// --- pictures (shots): ImGui's triangles filled in, the font's alpha from its atlas -----------
void picture(const char* name) {
  if (g_shots.empty()) return;
  const ImDrawData* dd = ImGui::GetDrawData();
  const int W = 1920, H = 1080;
  std::vector<float> px(static_cast<size_t>(W) * H * 3, 0.06f);
  for (const ImDrawList* dl : dd->CmdLists)
    for (const ImDrawCmd& c : dl->CmdBuffer) {
      if (c.UserCallback) continue;
      const ImTextureData* tex = c.TexRef._TexData;
      const int cx0 = std::max(0, static_cast<int>(c.ClipRect.x)), cy0 = std::max(0, static_cast<int>(c.ClipRect.y));
      const int cx1 = std::min(W, static_cast<int>(c.ClipRect.z)), cy1 = std::min(H, static_cast<int>(c.ClipRect.w));
      for (unsigned i = 0; i + 2 < c.ElemCount; i += 3) {
        const ImDrawVert* v[3];
        for (int k = 0; k < 3; ++k) v[k] = &dl->VtxBuffer[c.VtxOffset + dl->IdxBuffer[c.IdxOffset + i + k]];
        const float x0 = v[0]->pos.x, y0 = v[0]->pos.y, x1 = v[1]->pos.x, y1 = v[1]->pos.y;
        const float x2 = v[2]->pos.x, y2 = v[2]->pos.y;
        const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
        if (std::fabs(area) < 1e-6f) continue;
        const int bx0 = std::max(cx0, static_cast<int>(std::floor(std::min({x0, x1, x2}))));
        const int by0 = std::max(cy0, static_cast<int>(std::floor(std::min({y0, y1, y2}))));
        const int bx1 = std::min(cx1, static_cast<int>(std::ceil(std::max({x0, x1, x2}))));
        const int by1 = std::min(cy1, static_cast<int>(std::ceil(std::max({y0, y1, y2}))));
        for (int y = by0; y < by1; ++y)
          for (int x = bx0; x < bx1; ++x) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            const float w0 = ((x1 - fx) * (y2 - fy) - (x2 - fx) * (y1 - fy)) / area;
            const float w1 = ((x2 - fx) * (y0 - fy) - (x0 - fx) * (y2 - fy)) / area;
            const float w2 = 1.0f - w0 - w1;
            if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
            float col[4];
            for (int k = 0; k < 4; ++k)
              col[k] = (((v[0]->col >> (8 * k)) & 0xFF) * w0 + ((v[1]->col >> (8 * k)) & 0xFF) * w1 +
                        ((v[2]->col >> (8 * k)) & 0xFF) * w2) /
                       255.0f;
            float ta = 1.0f, tc[3] = {1, 1, 1};
            if (tex && tex->Pixels) {
              const float u = v[0]->uv.x * w0 + v[1]->uv.x * w1 + v[2]->uv.x * w2;
              const float t = v[0]->uv.y * w0 + v[1]->uv.y * w1 + v[2]->uv.y * w2;
              const int tx = std::clamp(static_cast<int>(u * tex->Width), 0, tex->Width - 1);
              const int ty = std::clamp(static_cast<int>(t * tex->Height), 0, tex->Height - 1);
              const unsigned char* p = tex->Pixels + (static_cast<size_t>(ty) * tex->Width + tx) * tex->BytesPerPixel;
              if (tex->Format == ImTextureFormat_Alpha8) {
                ta = p[0] / 255.0f;
              } else {
                for (int k = 0; k < 3; ++k) tc[k] = p[k] / 255.0f;
                ta = p[3] / 255.0f;
              }
            }
            const float a = col[3] * ta;
            float* o = &px[(static_cast<size_t>(y) * W + x) * 3];
            for (int k = 0; k < 3; ++k) o[k] = o[k] * (1.0f - a) + col[k] * tc[k] * a;
          }
      }
    }
  char path[600];
  std::snprintf(path, sizeof(path), "%s\\%02d-%s.ppm", g_shots.c_str(), ++g_shot, name);
  if (FILE* f = std::fopen(path, "wb")) {
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (const float c : px) std::fputc(static_cast<int>(std::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f), f);
    std::fclose(f);
  }
}

// --- the checks ------------------------------------------------------------------------------
void check_pause() {
  make_game(true, false);
  frames(3);
  picture("pause");
  check(find("God mode") && find("Partner takes no damage") && find("One hit kills") && find("Infinite ammo") &&
            find("Steady aim") && find("Automatic QTEs"),
        "pause: the characters' cheats");
  {
    const bool was = cheats::g_on[sw::kOpenMap];
    const bool shown = find("Open map at chapter start") != nullptr;
    click("Open map at chapter start");
    const bool flipped = cheats::g_on[sw::kOpenMap] != was;
    click("Open map at chapter start");
    check(shown && flipped && cheats::g_on[sw::kOpenMap] == was,
          "pause: Open map at chapter start, its click flipping the switch (and back)");
  }
  check(find("Infinite money") && find("Set##gold") && !find("Infinite exchange points") && !find("Set##points"),
        "pause: the gold with its editor, no exchange points");
  check(find("Freeze countdown timer") && !find("Set##countdown"),
        "pause: the countdown switch in a chapter too, no countdown's editor while none runs");
  check(find("Freeze play time") && find("Deaths don't count") && find("Misses don't count"), "pause: the results");
  check(find("Set##c00") && find("Set##c13") && find("Set##time") && !find("Set##t0") && !find("Set##t1"),
        "pause: one clock for the chapter, Chris's and Sheva's counters' editors");
  cheats::g_asked.clear();
  click("Set##time");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].what == cheats::Edit::kTime &&
            cheats::g_asked[0].value == 1234,
        "time: Set asks for the chapter's time shown");
  // The gold: typed, then Set.
  cheats::g_asked.clear();
  check(click("Set##gold"), "gold: Set drawn");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].what == cheats::Edit::kGold &&
            cheats::g_asked[0].value == 12345,
        "gold: Set asks for the gold shown");
  // The cases' rows, with the sections above folded (out of view, nothing is drawn).
  overlay::g_cheats_open = overlay::g_results_open = false;
  frames(2);
  // A count: Chris's ammo (slot 1), typed, then Apply.
  check(find("##n00") && !find("Apply##p00"), "count: a field beside Chris's ammo, no Apply until one is typed");
  overlay::g_count[0][0] = {40, true};
  frames(1);
  inventory::g_asked.clear();
  check(click("Apply##p00"), "count: Apply drawn once a count is typed");
  check(inventory::g_asked.size() == 1 && inventory::g_asked[0].op == 'n' && inventory::g_asked[0].chara == 0 &&
            inventory::g_asked[0].slot == 0 && inventory::g_asked[0].count == 40,
        "count: Apply asks for the count typed");
  // Store: beside the ammo, not beside the gun in hand; it asks for that slot.
  check(find("Store##s00") && !find("Store##s01"), "store: Store beside the ammo, none beside the gun in hand");
  itembox::g_asked.clear();
  click("Store##s00");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 1 &&
            itembox::g_asked[0][0].op == itembox::Op::kStore && itembox::g_asked[0][0].chara == 0 &&
            itembox::g_asked[0][0].slot == 0 && itembox::g_asked[0][0].id == 0x201,
        "store: Store asks for Chris's slot 1, the ammo");
  // Two ticked, then Store 2.
  // Only the slots in use: the spray (slot 7) is the row right under the grenade
  // (slot 4), as the grenade's is under the herb's (slot 3).
  {
    const Drawn *herb = find("##ct02"), *grenade = find("##ct03"), *spray = find("##ct06");
    check(herb && grenade && spray && !find("##ct04") &&
              std::fabs((spray->bb.Min.y - grenade->bb.Min.y) - (grenade->bb.Min.y - herb->bb.Min.y)) < 1.0f,
          "case: only the slots in use, no rows for the empty ones");
  }
  click("##ct02");
  click("##ct03");
  itembox::g_asked.clear();
  check(click("Store 2###case store0"), "store: Store 2 for two ticked items");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 2 && itembox::g_asked[0][0].slot == 2 &&
            itembox::g_asked[0][1].slot == 3,
        "store: the ticked herb and grenade, in one batch");
  // x: the ammo has one, the gun in hand none.
  check(find("x##t00") && !find("x##t01"), "throw away: x for the ammo, none for the gun in hand");
  inventory::g_asked.clear();
  click("x##t00");
  check(inventory::g_asked.size() == 1 && inventory::g_asked[0].op == 'x' && inventory::g_asked[0].slot == 0,
        "throw away: x asks for that slot");
  overlay::g_cheats_open = overlay::g_results_open = true;
}

void check_pause_box() {
  make_game(true, false);
  // The sections above folded (out of view, nothing is drawn).
  overlay::g_cheats_open = overlay::g_results_open = false;
  overlay::g_case_open[0] = overlay::g_case_open[1] = false;
  overlay::g_box_open = true;
  overlay::g_take_into = -1;
  overlay::g_box_filter[0] = '\0';
  frames(3);
  picture("pause-box");
  // The tabs: the Add list's, each with how many entries of it the box holds;
  // Other items only while the box holds something filed under none of them.
  check(find("Weapons 1###bank0") && find("Sub-weapons 0###bank1") && find("Ammo 1###bank2") &&
            find("Recovery 0###bank3") && find("Armor 0###bank4") && !any_starting("Other items"),
        "pause box: a tab for each kind, with how many the box holds of it");
  check(find("Take##bk0") && !find("##bt5"), "pause box: Weapons lists the gun, not the ammo");
  check(click("Ammo 1###bank2") && find("Take##bk5") && find("x##bx5") && find("##bn5") && !find("##bt0"),
        "pause box: Ammo lists the ammo, with Take, x and a count");
  picture("pause-box-ammo");
  itembox::g_asked.clear();
  click("Take##bk5");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].op == itembox::Op::kTake &&
            itembox::g_asked[0][0].index == 5 && itembox::g_asked[0][0].id == 0x201 && itembox::g_asked[0][0].chara == 0,
        "pause box: Take puts the ammo in Chris's case (the pad's own character)");
  click("Sheva##take into1");
  click("Weapons 1###bank0");
  itembox::g_asked.clear();
  click("Take##bk0");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].chara == 1 && itembox::g_asked[0][0].index == 0,
        "pause box: Take into Sheva once she is picked");
  itembox::g_asked.clear();
  click("x##bx0");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].op == itembox::Op::kSet &&
            itembox::g_asked[0][0].index == 0 && itembox::g_asked[0][0].count == 0,
        "pause box: x throws the entry away");
  click("Ammo 1###bank2");
  overlay::g_box_row[5] = {0x201, 200, false};
  frames(3);
  itembox::g_asked.clear();
  click("Apply##bp5");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].op == itembox::Op::kSet &&
            itembox::g_asked[0][0].index == 5 && itembox::g_asked[0][0].count == 200,
        "pause box: a typed count stays, and Apply asks for it");
  // Ticks under two tabs: each stays ticked while the other tab is shown, and Take
  // takes both (the list's order: the gun first).
  click("##bt5");
  click("Weapons 1###bank0");
  click("##bt0");
  frames(3);
  check(ticked("##bt0") && overlay::g_box_row[5].tick && find("Take 2###box take"),
        "pause box: an entry ticked under Ammo stays ticked under Weapons, and Take counts both");
  picture("pause-box-ticks");
  click("Ammo 1###bank2");
  check(ticked("##bt5"), "pause box: ... and is still ticked back under Ammo");
  itembox::g_asked.clear();
  check(click("Take 2###box take"), "pause box: Take 2 for the two ticked");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 2 &&
            itembox::g_asked[0][0].op == itembox::Op::kTake && itembox::g_asked[0][0].index == 0 &&
            itembox::g_asked[0][1].index == 5,
        "pause box: Take asks for both ticked entries, in one batch");
  // All ticks what the tab shows, not the other tabs' entries.
  click("All##box");
  check(overlay::g_box_row[5].tick && !overlay::g_box_row[0].tick, "pause box: All ticks the Ammo tab's, not the gun");
  overlay::forget_box_edits();
  // The filter narrows the tab's list.
  std::snprintf(overlay::g_box_filter, sizeof(overlay::g_box_filter), "handgun");
  frames(2);
  check(find("##bt5"), "pause box: the filter \"handgun\" keeps the handgun ammo");
  click("Weapons 1###bank0");
  check(!find("##bt0") && find("Clear##box filter"), "pause box: ... and not the M92F under Weapons");
  click("Clear##box filter");
  check(overlay::g_box_filter[0] == '\0' && find("##bt0"), "pause box: Clear brings the gun back");
  // A key item in the box: Other items comes, and goes with it.
  itembox::g_view.entry[9] = {0x501, 1, {}};
  frames(2);
  check(click("Other items 1###bank5") && find("##bt9") && !find("##bt0"),
        "pause box: Other items lists a key item while the box holds one");
  itembox::g_view.entry[9] = {};
  frames(3);
  check(!any_starting("Other items") && find("##bt0"), "pause box: ... and goes with it, a tab of the kinds shown again");
  // An item this area has not loaded (the MP5 of the user's crash, 2026-10-29): its
  // Take asks as any other (the tick has the game's loader load it first); one with
  // no model of its own (arrows) is greyed; while a Take waits for the loader, no
  // other is asked.
  itembox::g_view.entry[7] = {0x113, 45, {}, inventory::kLoadable};
  itembox::g_view.entry[8] = {0x20B, 30, {}, inventory::kNever};
  frames(2);
  click("Weapons 2###bank0");
  check(find("Take##bk7") && find("Take##bk0"), "pause box: the MP5 listed beside the M92F, each with its Take");
  picture("pause-box-loadable");
  itembox::g_asked.clear();
  click("Take##bk7");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].op == itembox::Op::kTake &&
            itembox::g_asked[0][0].index == 7 && itembox::g_asked[0][0].id == 0x113,
        "pause box: an item this area has not loaded is asked for all the same (loaded first)");
  // (The arrows are offered by no Add list: an entry of them is under Other items.)
  click("Other items 1###bank5");
  itembox::g_asked.clear();
  click("Take##bk8");
  check(find("Take##bk8") && itembox::g_asked.empty(), "pause box: an item with no model of its own is not taken (greyed)");
  click("Ammo 1###bank2");
  itembox::g_view.loading = true;
  frames(2);
  itembox::g_asked.clear();
  click("Take##bk5");
  check(itembox::g_asked.empty(), "pause box: while a Take waits for the game's loader, no other is asked");
  itembox::g_view.loading = false;
  itembox::g_view.entry[7] = itembox::g_view.entry[8] = {};
  frames(2);
  overlay::g_cheats_open = overlay::g_results_open = true;
  overlay::g_case_open[0] = overlay::g_case_open[1] = true;
  overlay::forget_box_edits();
}

void check_add() {
  make_game(true, false);
  frames(2);
  // Chris's Add open, the sections above it folded (out of view, nothing is drawn).
  overlay::g_cheats_open = overlay::g_results_open = false;
  overlay::g_add_open[0] = true;
  frames(2);
  picture("add-open");
  check(find("Weapons") != nullptr || find("M92F (HG)") != nullptr, "add: the tabs and the list drawn");
  check(find("M92F (HG)") && !find("Hand grenade"), "add: Weapons lists a firearm, not a grenade");
  bool armor = false, armour = false;
  for (const Drawn& d : g_drawn) {
    armor = armor || d.label.rfind("Armor", 0) == 0;
    armour = armour || d.label.rfind("Armour", 0) == 0;
  }
  check(armor && !armour, "add: the vests' tab is Armor");
  click("M92F (HG)");
  inventory::g_asked.clear();
  check(click("Add M92F (HG)"), "add: the Add button names the pick");
  check(inventory::g_asked.size() == 1 && inventory::g_asked[0].op == 'a' && inventory::g_asked[0].id == 0x102 &&
            inventory::g_asked[0].chara == 0,
        "add: asks for Chris, the M92F");
  // An item this area has not loaded: picked, it is preloaded (its model and sounds
  // while Add is still to be pressed), and its Add asks as any other. The item
  // list's leftovers with no model (the arrows) are not offered.
  std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "MP5");
  frames(2);
  inventory::g_preloads.clear();
  click("H&K MP5 (MG)");
  check(inventory::g_preloads.size() == 1 && inventory::g_preloads[0] == 0x113,
        "add: picking an item this area has not loaded preloads it");
  inventory::g_asked.clear();
  click("Add H&K MP5 (MG)");
  check(inventory::g_asked.size() == 1 && inventory::g_asked[0].id == 0x113,
        "add: ... and its Add asks for it");
  std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "Arrow");
  frames(2);
  picture("add-leftovers");
  check(!find("Arrow") && !find("Add Arrow"), "add: the item list's leftovers (the arrows) are not offered");
  // Only what the story gives Chris and Sheva to carry (items.h's `offer`, the shop's
  // items and the launcher's loads): no knife, no Mercenaries gun, no scene-only weapon
  // or its rounds, no floor herb, no second launcher.
  {
    const struct {
      const char* query;
      const char* labels[4];
    } kNot[] = {{"Knife", {"Knife [101]", "Knife [10E]", "Knife [123]", "Knife [124]"}},
                {"Samurai", {"Samurai Edge (HG) [112]", "Samurai Edge (HG) [129]"}},
                {"Px4", {"Px4 (HG)"}},
                {"Lantern", {"Lantern"}},
                {"L.T.D.", {"L.T.D."}},
                {"RPG", {"RPG-7 NVS", "RPG round"}},
                {"Flame", {"Flamethrower"}},
                {"Grenade launcher", {"Grenade launcher"}},
                {"Herb (", {"Herb (Green) [301]", "Herb (Red) [302]", "Herb (Green) [305]", "Herb (Red) [306]"}}};
    bool none = true;
    for (const auto& n : kNot) {
      std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "%s", n.query);
      frames(2);
      for (const char* l : n.labels) none = none && (!l || !find(l));
    }
    check(none, "add: no knife, Mercenaries gun, scene-only weapon or rounds, floor herb or bare launcher offered");
    check(find("Herb (Green)") && find("Herb (Red)") && find("Herb (G+G)"),
          "add: ... the shop's herbs by their names alone (no id: the floor's are not listed)");
    std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "G. launcher");
    frames(2);
    check(find("G. launcher (EXP)") && find("G. launcher (ACD)") && find("G. launcher (ICE)") &&
              find("G. launcher (FLM)") && find("G. launcher (FLS)") && find("G. launcher (ELC)"),
          "add: the launcher in each of its six loads");
  }
  // The weapons the game keeps for one character (Item Management's own check): the
  // Longbow is Sheva's - listed in Chris's Add, but its Add asks nothing; his Gatling gun's
  // does. In Sheva's Add, the reverse.
  std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "Longbow");
  frames(2);
  click("Longbow");
  inventory::g_asked.clear();
  click("Add Longbow");
  check(find("Longbow") && find("Add Longbow") && inventory::g_asked.empty(),
        "add: Chris's Add lists the Longbow, but asks nothing for it (only Sheva can have it)");
  std::snprintf(overlay::g_picker[0].query, sizeof(overlay::g_picker[0].query), "Gatling");
  frames(2);
  click("Gatling gun");
  inventory::g_asked.clear();
  click("Add Gatling gun");
  check(inventory::g_asked.size() == 1 && inventory::g_asked[0].id == 0x115 && inventory::g_asked[0].chara == 0,
        "add: ... his Gatling gun's Add asks for it");
  overlay::g_picker[0].query[0] = '\0';
  overlay::g_add_open[0] = false;
  overlay::g_add_open[1] = true;
  std::snprintf(overlay::g_picker[1].query, sizeof(overlay::g_picker[1].query), "Gatling");
  frames(2);
  click("Gatling gun");
  inventory::g_asked.clear();
  click("Add Gatling gun");
  const bool gatling = find("Add Gatling gun") && inventory::g_asked.empty();
  std::snprintf(overlay::g_picker[1].query, sizeof(overlay::g_picker[1].query), "Longbow");
  frames(2);
  picture("add-theirs");
  click("Longbow");
  inventory::g_asked.clear();
  click("Add Longbow");
  check(gatling && inventory::g_asked.size() == 1 && inventory::g_asked[0].id == 0x10F &&
            inventory::g_asked[0].chara == 1,
        "add: Sheva's Add asks nothing for the Gatling gun, and for her Longbow");
  overlay::g_picker[1].query[0] = '\0';
  overlay::g_add_open[1] = false;
  overlay::g_add_open[0] = true;
  overlay::g_picker[0].query[0] = '\0';
  frames(2);
  // The Sub-weapons tab (Chris's Add's, the first drawn - the item box's tabs carry
  // their counts): the grenade.
  frame();
  const bool clicked = click("Sub-weapons");
  frames(2);
  check(clicked && find("Hand grenade") != nullptr, "add: Sub-weapons lists the grenade");
  picture("add-sub");
  overlay::g_add_open[0] = false;
  overlay::g_cheats_open = overlay::g_results_open = true;
}

// The pause's box: a Take of a weapon the game keeps for one character is greyed into
// the other's case - into Chris's the Longbow's, into Sheva's the Gatling gun's.
void check_box_theirs() {
  make_game(true, false);
  overlay::g_cheats_open = overlay::g_results_open = false;
  overlay::g_case_open[0] = overlay::g_case_open[1] = false;
  overlay::g_take_into = -1;
  overlay::forget_box_edits();
  itembox::g_view.entry[10] = {0x10F, 1, {}, inventory::kHere};  // Longbow
  itembox::g_view.entry[11] = {0x115, 1, {}, inventory::kHere};  // Gatling gun
  itembox::g_view.used = 4;
  frames(3);
  click("Weapons 3###bank0");
  picture("pause-box-theirs");
  itembox::g_asked.clear();
  click("Take##bk10");
  const bool longbow = find("Take##bk10") && itembox::g_asked.empty();
  click("Take##bk11");
  check(longbow && itembox::g_asked.size() == 1 && itembox::g_asked[0][0].id == 0x115 &&
            itembox::g_asked[0][0].chara == 0,
        "box: into Chris's case, the Longbow's Take is greyed and the Gatling gun's asked");
  click("Sheva##take into1");
  itembox::g_asked.clear();
  click("Take##bk11");
  const bool gatling = itembox::g_asked.empty();
  click("Take##bk10");
  check(gatling && itembox::g_asked.size() == 1 && itembox::g_asked[0][0].id == 0x10F &&
            itembox::g_asked[0][0].chara == 1,
        "box: ... into Sheva's, the reverse");
  overlay::g_take_into = -1;
  overlay::g_cheats_open = overlay::g_results_open = true;
  overlay::g_case_open[0] = overlay::g_case_open[1] = true;
}

// An extra episode's pause (the user, 2026-11-03: "These extra content portions should all have
// their own item box"): the box is the episode's own - its section named for it, no Treasures tab
// in it nor in its Add list (the episodes have none), and its Add list without the weapons the game
// keeps for a character who is not in the episode (the Longbow with Chris and Jill; the Longbow and
// the Gatling gun with Josh and Jill). Store and Take as the story's. A file that could not be read:
// the section alone, nothing of the box, nothing asked.
bool add_lists(const char* name) {
  char prefix[64];
  std::snprintf(prefix, sizeof(prefix), "%s##ba", name);
  return any_starting(prefix);
}
void episode(int mode, const char* name, const char* file, const char* partner, const char* first) {
  make_game(true, false);
  overlay::g_cheats_open = overlay::g_results_open = false;
  overlay::g_case_open[0] = overlay::g_case_open[1] = true;
  overlay::g_add_open[0] = overlay::g_add_open[1] = false;
  overlay::g_box_open = overlay::g_box_add_open = true;
  overlay::g_box_filter[0] = '\0';
  overlay::g_box_pick = overlay::BoxPicker{};
  overlay::g_take_into = -1;
  overlay::forget_box_edits();
  inventory::g_view.cases[0].name = first;
  inventory::g_view.cases[1].name = partner;
  itembox::View& b = itembox::g_view;
  b.mode = mode;
  b.episode = name;
  b.file = file;
  b.treasures = false;
  b.tkinds = 0;
}
void check_episode_box() {
  episode(game::kModeNightmares, "Lost in Nightmares", "RE5CabbyCodes.itembox.lost-in-nightmares.ini", "Jill", "Chris");
  frames(3);
  click("Weapons");  // the Add list's tab of guns
  picture("episode-box");
  check(find("Item box - Lost in Nightmares  (2/54)###box") && find("Weapons 1###bank0") && find("Ammo 1###bank2") &&
            !any_starting("Treasures") && find("Store##s00"),
        "episode: the pause's box named for Lost in Nightmares - its tabs, no Treasures (nor in its Add list), Store "
        "beside the case");
  check(add_lists("Gatling gun") && !add_lists("Longbow") && add_lists("M92F (HG)"),
        "episode: its Add list offers Chris's Gatling gun, not Sheva's Longbow (she is not in it)");
  itembox::g_asked.clear();
  click("Take##bk0");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].op == itembox::Op::kTake &&
            itembox::g_asked[0][0].index == 0 && itembox::g_asked[0][0].chara == 0,
        "episode: Take puts the gun in Chris's case, as in the story");
  episode(game::kModeEscape, "Desperate Escape", "RE5CabbyCodes.itembox.desperate-escape.ini", "Jill", "Josh");
  frames(3);
  click("Weapons");
  check(find("Item box - Desperate Escape  (2/54)###box") && !add_lists("Gatling gun") && !add_lists("Longbow") &&
            add_lists("M92F (HG)") && find("Josh##take into0") && find("Jill##take into1"),
        "episode: Desperate Escape's box - Take into Josh or Jill, and neither the Gatling gun nor the Longbow in its "
        "Add list");
  // Its file there but not read: the section says so, and nothing of the box is drawn or asked.
  itembox::g_view.unreadable = true;
  itembox::g_view.editable = itembox::g_view.can_move = false;
  itembox::g_view.used = 0;
  itembox::g_view.entry[0] = itembox::g_view.entry[5] = {};
  frames(3);
  picture("episode-box-unreadable");
  itembox::g_asked.clear();
  click("Store##s00");
  check(find("Item box - Desperate Escape  (0/54)###box") && !find("Weapons 0###bank0") &&
            !find("Add to the item box###box add list") && itembox::g_asked.empty(),
        "episode: a box whose file could not be read - its section alone, no Store asked");
  // The story again: its own box, both weapons offered.
  make_game(true, false);
  frames(3);
  click("Weapons");
  check(find("Item box  (2/54)###box") && add_lists("Gatling gun") && add_lists("Longbow") && find("Treasures 2###bank6"),
        "episode: back in the story - the game's box, its treasures, both characters' weapons in its Add list");
  overlay::g_cheats_open = overlay::g_results_open = true;
  overlay::g_box_add_open = false;
}

// Store takes only what the Add lists offer (the user, 2026-11-01: "store should refuse
// those types of items"): beside a scene's own weapon - the Lantern - it is greyed; a
// grenade launcher (in play the case's item has the launcher's id) is kept as its load and
// asked for; one whose load is not known is greyed too.
void check_store_kept_out() {
  make_game(true, false);
  overlay::g_cheats_open = overlay::g_results_open = false;
  overlay::g_case_open[0] = true;
  overlay::g_case_open[1] = overlay::g_box_open = false;
  inventory::g_view.cases[0].slot[7] = {true, 0x130, 1, 1, 0, {}};         // Lantern (2-2's mine)
  inventory::g_view.cases[0].slot[8] = {true, 0x10C, 6, 12, 0, {}, 0x126};  // the launcher, acid rounds loaded
  ImGui::GetIO().AddMousePosEvent(5.0f, 5.0f);  // no tooltip over the picture
  frames(3);
  picture("store-kept-out");
  itembox::g_asked.clear();
  click("Store##s07");
  const bool lantern = find("Store##s07") && itembox::g_asked.empty();
  click("Store##s08");
  check(lantern && itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 1 &&
            itembox::g_asked[0][0].op == itembox::Op::kStore && itembox::g_asked[0][0].slot == 8 &&
            itembox::g_asked[0][0].id == 0x10C,
        "store: greyed beside the Lantern (a scene's own); the grenade launcher's (kept as its load) asks for it");
  inventory::g_view.cases[0].slot[8].kept_as = 0;  // its load not known: the bare launcher
  frames(2);
  itembox::g_asked.clear();
  click("Store##s08");
  check(find("Store##s08") && itembox::g_asked.empty(), "store: ... greyed beside a launcher whose load is not known");
  overlay::g_cheats_open = overlay::g_results_open = overlay::g_box_open = true;
  overlay::g_case_open[1] = true;
}

void check_play() {
  make_game(false, true);
  frames(3);
  picture("play-mercenaries");
  check(find("Freeze countdown timer") != nullptr, "Mercenaries: the countdown switch");
  cheats::g_asked.clear();
  inventory::g_asked.clear();
  click("Set##gold");
  click("Set##n00");
  check(cheats::g_asked.empty() && inventory::g_asked.empty(), "in play: the editors ask nothing (disabled)");
}

// The Mercenaries' own switches: a section of their own in a match (its numbers beside them), none in a chapter.
void check_mercenaries() {
  make_game(true, false);
  frames(3);
  check(!find("Endless combo") && !find("Combo Bonus always on") && !find("Kill enemies as they appear"),
        "Mercenaries' switches: none at a chapter's pause");
  make_game(true, true);
  merc::g_status.score = 123456;
  merc::g_status.combo = 12;
  merc::g_status.best_combo = 40;
  merc::g_status.kills = 87;
  merc::g_status.target = 150;
  merc::g_status.time_left = 252.5f;
  merc::g_status.bonus = true;
  merc::g_status.bonus_left = 30.0f;
  merc::g_status.bonus_kills = 9;
  merc::g_status.killing = 6;
  merc::g_status.areas = 2;
  cheats::g_on[sw::kAutoKill] = true;
  frames(3);
  picture("pause-mercenaries");
  check(find("Endless combo") && find("Combo Bonus always on") && find("Kill enemies as they appear") &&
            find("Freeze countdown timer"),
        "Mercenaries' pause: their three switches, and the countdown's");
  check(!find("Freeze play time") && !find("Set##time"), "Mercenaries' pause: no chapter results");
  for (sw::Kind k : {sw::kEndlessCombo, sw::kComboBonus, sw::kAutoKill}) {
    const char* label = k == sw::kEndlessCombo ? "Endless combo"
                        : k == sw::kComboBonus ? "Combo Bonus always on"
                                               : "Kill enemies as they appear";
    const bool was = cheats::g_on[k];
    click(label);
    const bool flipped = cheats::g_on[k] != was;
    click(label);
    check(flipped && cheats::g_on[k] == was, "Mercenaries' pause: a click flips the switch (and back)");
  }
  overlay::g_merc_open = true;
  cheats::g_on[sw::kAutoKill] = false;
}

// A stage's countdown running (chapter 3-2's escape): its seconds left beside the
// switch, Set asking for them at the pause - and nothing in play.
void check_countdown() {
  make_game(true, false);
  cheats::g_status.countdown = true;
  cheats::g_status.countdown_left = 151.7f;
  cheats::g_status.countdown_most = 5940.0f;
  frames(3);
  picture("pause-countdown");
  check(find("Freeze countdown timer") && find("##countdown") && find("Set##countdown"),
        "countdown: running - its seconds left and Set beside the switch");
  cheats::g_asked.clear();
  click("Set##countdown");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].what == cheats::Edit::kCountdown &&
            cheats::g_asked[0].value == 151,
        "countdown: Set asks for the seconds shown");
  overlay::g_countdown = {75, true};
  frames(1);
  cheats::g_asked.clear();
  click("Set##countdown");
  check(cheats::g_asked.size() == 1 && cheats::g_asked[0].value == 75, "countdown: ... or for the seconds typed");
  make_game(false, false);
  cheats::g_status.countdown = true;
  cheats::g_status.countdown_left = 151.7f;
  cheats::g_status.countdown_most = 5940.0f;
  frames(3);
  cheats::g_asked.clear();
  click("Set##countdown");
  check(find("Set##countdown") && cheats::g_asked.empty(), "countdown: in play, Set asks nothing (disabled)");
}

void make_itembox(bool editable) {
  dispatch::Snapshot& s = dispatch::g_snap;
  s = dispatch::Snapshot{};
  s.ready = true;
  s.run.screen = game::Screen::kItemBox;
  s.show_itembox = true;
  itembox::View& v = itembox::g_view;
  v = itembox::View{};
  v.found = v.open = true;
  v.editable = editable;
  v.size = 54;
  v.entry[0] = {0x102, 10, {}};   // M92F (HG), 10 rounds loaded
  v.entry[1] = {0x304, 1, {}};    // First aid spray
  v.entry[5] = {0x201, 120, {}};  // Handgun ammo
  v.used = 3;
}

int item_index(int id) {
  for (int k = 0; k < items::kCount; ++k)
    if (items::kItems[k].id == id) return k;
  return -1;
}

void check_itembox() {
  make_itembox(true);
  overlay::g_box_open = overlay::g_box_add_open = true;
  overlay::g_box_filter[0] = '\0';
  frames(3);
  click("Weapons 1###bank0");
  picture("itembox");
  check(find("All##box") && find("##bt0") && !find("##bn0") && !find("##bt1") && !find("##bt5"),
        "item box: Weapons lists the gun alone, with no count");
  check(click("Recovery 1###bank3") && find("##bt1") && find("##bn1") && !find("##bt0"),
        "item box: Recovery lists the spray, with a count (it stacks in the box)");
  check(click("Ammo 1###bank2") && find("##bt5") && find("##bn5"), "item box: Ammo lists the ammo, with a count");
  check(!find("God mode") && !find("Set##gold"), "item box: the cheats panel is not drawn beside Item Management");
  // A count typed, then Apply all - from another tab.
  overlay::g_box_row[5] = {0x201, 300, false};
  click("Weapons 1###bank0");
  itembox::g_asked.clear();
  check(click("Apply all###box apply"), "item box: Apply all drawn");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 1 &&
            itembox::g_asked[0][0].op == itembox::Op::kSet && itembox::g_asked[0][0].index == 5 &&
            itembox::g_asked[0][0].id == 0x201 && itembox::g_asked[0][0].count == 300,
        "item box: Apply all asks for the count typed under Ammo");
  check(!find("Take##bk0") && find("x##bx0"), "item box: x beside each entry, no Take beside Item Management");
  // An entry ticked, then Throw away.
  click("##bt0");
  itembox::g_asked.clear();
  check(click("Throw away###box throw"), "item box: Throw away drawn");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 1 && itembox::g_asked[0][0].index == 0 &&
            itembox::g_asked[0][0].id == 0x102 && itembox::g_asked[0][0].count == 0,
        "item box: Throw away asks for the ticked entry");
  // Add: its Ammo tab (the box's own carries its count), the handgun ammo ticked
  // (10, as it comes), Add.
  frame();
  const bool tab = click("Ammo");
  frames(2);
  char ammo[64];
  std::snprintf(ammo, sizeof(ammo), "Handgun ammo##ba%d", item_index(0x201));
  const std::string grenade = "Hand grenade##ba" + std::to_string(item_index(0x106));
  check(tab && find(ammo) && !find(grenade.c_str()), "item box: Ammo lists the ammo, not the grenade");
  const std::string arrow = "Arrow##ba" + std::to_string(item_index(0x20B));
  check(!find(arrow.c_str()), "item box: the item list's leftovers (the arrows) are not offered");
  click(ammo);
  picture("itembox-add");
  itembox::g_asked.clear();
  check(click("Add###box add"), "item box: Add drawn");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 1 &&
            itembox::g_asked[0][0].op == itembox::Op::kAdd && itembox::g_asked[0][0].id == 0x201 &&
            itembox::g_asked[0][0].count == 10,
        "item box: Add asks for the ticked ammo, 10 of it");
  // The screen busy: nothing is asked.
  make_itembox(false);
  frames(2);
  overlay::g_box_row[5] = {0x201, 300, false};
  overlay::g_box_row[1] = {0x304, 0, true};
  frames(1);
  itembox::g_asked.clear();
  click("Apply all###box apply");
  click("Throw away###box throw");
  check(itembox::g_asked.empty(), "item box: nothing asked while the screen is busy");
  overlay::forget_box_edits();
}

// Chapter Select's button clicked, and a frame for its window to fit what it shows next (it
// sizes itself by the last frame's contents).
bool ask() {
  const bool clicked = click("Chapter Select");
  frames(1);
  return clicked;
}

// Chapter Select beside Item Management between chapters, beside the panel at the pause, on
// the Game Over screen and at the chapter's results: a window of its own, in front of the item
// box's (the panel's), that does not fold - asking Yes or No first only at the pause, in the
// game.
void check_chapter_select() {
  make_itembox(true);
  overlay::g_box_open = true;
  menus::g_view = menus::View{};
  menus::g_view.found = menus::g_view.offered = menus::g_view.ready = true;
  menus::g_view.where = menus::Where::kItemBox;
  menus::g_asked = 0;
  frames(3);
  picture("chapter-select");
  ImGuiWindow* box = ImGui::FindWindowByName(overlay::kBoxWindow);
  ImGuiWindow* own = ImGui::FindWindowByName(overlay::kChapterWindow);
  check(find("Chapter Select") && find("Apply all###box apply") && box && own && own != box && own->Active,
        "chapter select: its button in a window of its own beside Item Management between chapters, the box in its");
  if (!box || !own) return;
  // The item box's window focused (its title bar clicked): Chapter Select's stays in front.
  click_point(box->Pos.x + box->Size.x - 60.0f, box->Pos.y + 6.0f);
  check(ImGui::GetCurrentContext()->NavWindow == box &&
            ImGui::FindWindowDisplayIndex(own) > ImGui::FindWindowDisplayIndex(box),
        "chapter select: in front of the item box's window, that one focused");
  check(ask() && menus::g_asked == 1 && !find("Yes") && find("Chapter Select"),
        "chapter select: beside Item Management one click asks for it, once - no question");
  // The item box's window folded: the button stays, and works; its own window does not fold.
  ImGui::SetWindowCollapsed(overlay::kBoxWindow, true);
  frames(2);
  picture("chapter-select-box-folded");
  menus::g_asked = 0;
  check(box->Collapsed && !find("Apply all###box apply") && ask() && menus::g_asked == 1,
        "chapter select: the item box's window folded, the button still there and working");
  ImGui::SetWindowCollapsed(overlay::kChapterWindow, true);
  frames(2);
  check(!own->Collapsed && find("Chapter Select"), "chapter select: its own window does not fold");
  ImGui::SetWindowCollapsed(overlay::kBoxWindow, false);
  frames(2);
  // Not ready: drawn, nothing asked.
  menus::g_view.ready = false;
  std::snprintf(menus::g_view.why, sizeof(menus::g_view.why), "Wait for the screen to settle.");
  frames(2);
  menus::g_asked = 0;
  ask();
  check(find("Chapter Select") && !find("Yes") && menus::g_asked == 0,
        "chapter select: disabled while it cannot go - nothing asked");
  // On its way: said so, the box put away.
  menus::g_view.ready = menus::g_view.offered = false;
  menus::g_view.going = true;
  frames(2);
  picture("chapter-select-going");
  check(!find("Chapter Select") && !find("Apply all###box apply") && !find("Throw away###box throw"),
        "chapter select: while it goes, no button and the box's section put away");
  // Not offered (the main menu's Item Management): no button, the box as before.
  menus::g_view = menus::View{};
  menus::g_view.found = true;
  frames(2);
  check(!find("Chapter Select") && find("Apply all###box apply"),
        "chapter select: not offered beside the main menu's Item Management");
  // At the pause in a story chapter: beside the panel, in a window of its own, Yes or No first.
  make_game(true, false);
  frames(2);
  check(find("God mode") && !find("Chapter Select"), "chapter select: not at the pause unless offered there");
  menus::g_view.offered = menus::g_view.ready = true;
  menus::g_view.where = menus::Where::kPause;
  frames(3);
  picture("chapter-select-pause");
  ImGuiWindow* panel = ImGui::FindWindowByName(overlay::kPanelWindow);
  ImGuiWindow* at_pause = ImGui::FindWindowByName(overlay::kChapterPauseWindow);
  check(find("God mode") && find("Chapter Select") && panel && at_pause && at_pause != panel && at_pause->Active &&
            ImGui::FindWindowDisplayIndex(at_pause) > ImGui::FindWindowDisplayIndex(panel),
        "chapter select: at the pause, in a window of its own in front of the panel");
  menus::g_asked = 0;
  check(ask() && menus::g_asked == 0 && find("Yes") && find("No") && !find("Chapter Select"),
        "chapter select: at the pause a click asks Yes or No first - nothing asked yet");
  frames(1);
  picture("chapter-select-pause-confirm");
  check(click("No") && menus::g_asked == 0 && find("Chapter Select") && !find("Yes"),
        "chapter select: No - nothing asked, the button back");
  check(ask() && click("Yes") && menus::g_asked == 1 && find("Chapter Select") && !find("Yes"),
        "chapter select: Yes - asked for, once");
  // Asking, then it can no longer go: the question let go.
  ask();
  menus::g_view.ready = false;
  frames(2);
  check(!find("Yes") && find("Chapter Select"), "chapter select: the question let go once it cannot go");
  menus::g_view.ready = true;
  // The pause closed while asking: asked again next time.
  ask();
  make_game(false, false);
  frames(2);
  make_game(true, false);
  frames(2);
  check(find("Chapter Select") && !find("Yes"), "chapter select: the pause closed while it asked - asked afresh");
  // On its way from the pause: the cases and the box put away, the cheats still there.
  const bool shown = any_starting("Chris's inventory") && any_starting("Item box");
  menus::g_view.offered = menus::g_view.ready = false;
  menus::g_view.going = true;
  frames(2);
  picture("chapter-select-pause-going");
  check(shown && find("God mode") && !find("Chapter Select") && !any_starting("Chris's inventory") &&
            !any_starting("Item box"),
        "chapter select: on its way from the pause - the cases and the box put away, the cheats still there");
  // The Game Over screen: its window alone (no panel), one click.
  make_game(false, false);
  dispatch::g_snap.show_end = true;
  menus::g_view = menus::View{};
  menus::g_view.found = menus::g_view.offered = menus::g_view.ready = true;
  menus::g_view.where = menus::Where::kGameOver;
  menus::g_asked = 0;
  frames(3);
  picture("chapter-select-game-over");
  ImGuiWindow* at_over = ImGui::FindWindowByName(overlay::kChapterEndWindow);
  check(find("Chapter Select") && !find("God mode") && at_over && at_over->Active,
        "chapter select: on the Game Over screen, its window alone (no panel)");
  check(ask() && menus::g_asked == 1 && !find("Yes"), "chapter select: there one click asks for it, once - no question");
  // The chapter's results: the same, their window alone, one click.
  menus::g_view.where = menus::Where::kResults;
  menus::g_asked = 0;
  frames(2);
  picture("chapter-select-results");
  check(find("Chapter Select") && !find("God mode") && at_over && at_over->Active && ask() && menus::g_asked == 1 &&
            !find("Yes"),
        "chapter select: at the chapter's results, its window alone (no panel), one click asks for it - no question");
  // The last chapter's results (no quit of their own): the same button, ready, one click.
  menus::g_view.last = true;
  menus::g_asked = 0;
  frames(2);
  check(find("Chapter Select") && !find("God mode") && ask() && menus::g_asked == 1 && !find("Yes"),
        "chapter select: at the last chapter's results the same - one click asks for it");
  dispatch::g_snap.show_end = false;
  menus::g_view = menus::View{};
}

// The treasures: a tab of the box's own, at the pause and beside Item Management.
void check_treasures() {
  make_game(true, false);
  overlay::g_cheats_open = overlay::g_results_open = false;
  overlay::g_case_open[0] = overlay::g_case_open[1] = false;
  overlay::g_add_open[0] = overlay::g_add_open[1] = false;
  overlay::g_box_open = overlay::g_box_add_open = true;
  overlay::g_box_filter[0] = '\0';
  overlay::g_box_pick = overlay::BoxPicker{};
  frames(3);
  check(find("Treasures 2###bank6") && find("Armor 0###bank4") && !any_starting("Other items"),
        "treasures: a tab of the box's, with how many kinds are held");
  check(click("Treasures 2###bank6") && find("##tt1047") && find("##tt1135") && find("##tn1047") &&
            find("x##tx1047") && !any_starting("Take##bk") && !find("##bt0"),
        "treasures: the tab lists the gold ring and the soul gem with their counts and x - no Take, no box entry");
  picture("pause-treasures");
  {
    const Drawn *ring = find("##tt1047"), *gem = find("##tt1135");
    check(ring && gem && ring->bb.Min.y < gem->bb.Min.y, "treasures: in the item table's order (the ring first)");
  }
  itembox::g_asked.clear();
  click("x##tx1047");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].op == itembox::Op::kTreasureSet &&
            itembox::g_asked[0][0].id == 0x417 && itembox::g_asked[0][0].count == 0,
        "treasures: x throws the gold rings away (how many held: 0)");
  overlay::treasure_row(0x417) = {0x417, 7, false};
  frames(2);
  itembox::g_asked.clear();
  click("Apply##tp1047");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].op == itembox::Op::kTreasureSet &&
            itembox::g_asked[0][0].id == 0x417 && itembox::g_asked[0][0].count == 7,
        "treasures: a typed count stays, and Apply asks for it");
  // A treasure and a box entry ticked: Take counts the entry alone, Throw away both.
  click("##tt1047");
  click("Weapons 1###bank0");
  click("##bt0");
  frames(2);
  check(find("Take###box take") && find("Throw away 2###box throw"),
        "treasures: a treasure ticked with a box entry - Take for the entry alone, Throw away for both");
  itembox::g_asked.clear();
  click("Throw away 2###box throw");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 2 &&
            itembox::g_asked[0][0].op == itembox::Op::kSet && itembox::g_asked[0][0].index == 0 &&
            itembox::g_asked[0][1].op == itembox::Op::kTreasureSet && itembox::g_asked[0][1].id == 0x417 &&
            itembox::g_asked[0][1].count == 0,
        "treasures: ... and Throw away asks for both in one batch");
  click("Treasures 2###bank6");
  click("All##box");
  check(overlay::treasure_row(0x417).tick && overlay::treasure_row(0x46F).tick && !overlay::g_box_row[0].tick,
        "treasures: All ticks the treasures, not the box's entries");
  overlay::forget_box_edits();
  std::snprintf(overlay::g_box_filter, sizeof(overlay::g_box_filter), "ring");
  frames(2);
  check(find("##tt1047") && !find("##tt1135"), "treasures: the filter \"ring\" keeps the gold ring alone");
  overlay::g_box_filter[0] = '\0';
  frames(2);
  // The Add list's Treasures tab: the game's treasures, not the gold pickups, the
  // Mercenaries' score stars nor the leftovers outside the game's table of them.
  frame();
  const bool tab = click("Treasures");
  frames(2);
  const std::string ring = "Gold ring##ba" + std::to_string(item_index(0x417));
  check(tab && find(ring.c_str()) && !find(("Gold (Large)##ba" + std::to_string(item_index(0x401))).c_str()) &&
            !find(("Score star (Small)##ba" + std::to_string(item_index(0x427))).c_str()) &&
            !find(("Beetle (Emerald green)##ba" + std::to_string(item_index(0x426))).c_str()) &&
            !find(("Handgun ammo##ba" + std::to_string(item_index(0x201))).c_str()),
        "treasures: the Add list's Treasures tab - the game's treasures, not gold, score stars or leftovers");
  picture("pause-treasures-add");
  click(ring.c_str());
  frames(2);
  check(find(("##bc" + std::to_string(item_index(0x417))).c_str()) != nullptr,
        "treasures: a ticked treasure has its count beside it");
  itembox::g_asked.clear();
  click("Add###box add");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0].size() == 1 &&
            itembox::g_asked[0][0].op == itembox::Op::kTreasureAdd && itembox::g_asked[0][0].id == 0x417 &&
            itembox::g_asked[0][0].count == 1,
        "treasures: Add asks for one gold ring among the treasures");
  // The treasures not understood: no tab of them, in the list or its Add.
  itembox::g_view.treasures = false;
  frames(3);
  check(!any_starting("Treasures"), "treasures: no tab of them when their list was not understood");
  // Beside Item Management.
  make_itembox(true);
  itembox::g_view.treasures = true;
  itembox::g_view.tmost = 999;
  itembox::g_view.treasure[0] = {0x470, 3};  // Heart of Africa
  itembox::g_view.tkinds = 1;
  frames(3);
  check(click("Treasures 1###bank6") && find("##tt1136") && find("x##tx1136"),
        "treasures: beside Item Management too, with x");
  picture("itembox-treasures");
  itembox::g_asked.clear();
  click("x##tx1136");
  check(itembox::g_asked.size() == 1 && itembox::g_asked[0][0].op == itembox::Op::kTreasureSet &&
            itembox::g_asked[0][0].id == 0x470 && itembox::g_asked[0][0].count == 0,
        "treasures: ... where x asks as at the pause");
  click("Weapons 1###bank0");
  overlay::forget_box_edits();
  overlay::g_cheats_open = overlay::g_results_open = true;
  overlay::g_case_open[0] = overlay::g_case_open[1] = true;
}

// Max all: every count the box's list has - each entry that stacks and each treasure
// held, under every tab - to its most, in one batch; a gun, and what is at its most
// already, left out.
void check_max_all() {
  make_game(true, false);
  overlay::g_cheats_open = overlay::g_results_open = false;
  overlay::g_case_open[0] = overlay::g_case_open[1] = false;
  overlay::g_add_open[0] = overlay::g_add_open[1] = false;
  overlay::g_box_open = true;
  overlay::g_box_filter[0] = '\0';
  overlay::forget_box_edits();
  itembox::g_view.entry[9] = {0x106, 999, {}, inventory::kHere};  // Hand grenades, at the most already
  itembox::g_view.used = 3;
  frames(3);
  click("Weapons 1###bank0");
  overlay::g_box_row[5] = {0x201, 200, false};       // a count typed under Ammo
  overlay::treasure_row(0x417) = {0x417, 7, false};  // and one under Treasures
  std::snprintf(overlay::g_box_filter, sizeof(overlay::g_box_filter), "M92");
  frames(2);
  picture("pause-box-max");
  check(find("Max all 3###box max") != nullptr,
        "max all: drawn with how many counts it fills - the ammo and both treasures, under the other tabs too");
  itembox::g_asked.clear();
  click("Max all 3###box max");
  {
    const std::vector<std::vector<itembox::Change>>& a = itembox::g_asked;
    check(a.size() == 1 && a[0].size() == 3 && a[0][0].op == itembox::Op::kSet && a[0][0].index == 5 &&
              a[0][0].id == 0x201 && a[0][0].count == 999 && a[0][1].op == itembox::Op::kTreasureSet &&
              a[0][1].id == 0x417 && a[0][1].count == 999 && a[0][2].op == itembox::Op::kTreasureSet &&
              a[0][2].id == 0x46F && a[0][2].count == 999,
          "max all: one batch from the Weapons tab, the filter on - the ammo and both treasures to 999, not the gun "
          "nor the grenades");
  }
  check(overlay::g_box_row[5].count == 0 && overlay::treasure_row(0x417).count == 0,
        "max all: ... and the counts typed are let go");
  // Everything at its most: nothing to fill.
  overlay::g_box_filter[0] = '\0';
  itembox::g_view.entry[5].count = 999;
  itembox::g_view.treasure[0].count = itembox::g_view.treasure[1].count = 999;
  frames(2);
  itembox::g_asked.clear();
  click("Max all###box max");
  check(find("Max all###box max") && itembox::g_asked.empty(), "max all: everything at its most - nothing asked");
  // Beside Item Management: the spray and the ammo (both stack in the box), not the gun.
  make_itembox(true);
  frames(3);
  picture("itembox-max");
  itembox::g_asked.clear();
  check(click("Max all 2###box max"), "max all: beside Item Management too");
  {
    const std::vector<std::vector<itembox::Change>>& a = itembox::g_asked;
    check(a.size() == 1 && a[0].size() == 2 && a[0][0].op == itembox::Op::kSet && a[0][0].index == 5 &&
              a[0][0].count == 999 && a[0][1].op == itembox::Op::kSet && a[0][1].index == 1 &&
              a[0][1].id == 0x304 && a[0][1].count == 999,
          "max all: ... the ammo and the spray to 999, in the list's order");
  }
  make_itembox(false);
  frames(2);
  itembox::g_asked.clear();
  click("Max all 2###box max");
  check(itembox::g_asked.empty(), "max all: nothing asked while the screen is busy");
  overlay::forget_box_edits();
  overlay::g_cheats_open = overlay::g_results_open = true;
  overlay::g_case_open[0] = overlay::g_case_open[1] = true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: test_panel.exe <scratch dir> [shots]\n");
    return 2;
  }
  if (argc > 2 && std::strcmp(argv[2], "shots") == 0) g_shots = argv[1];
  ImGui::CreateContext();
  GImGui->TestEngineHookItems = true;
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(overlay::kScale);
  style.FontScaleMain = overlay::kScale;
  check_pause();
  check_pause_box();
  check_add();
  check_box_theirs();
  check_episode_box();
  check_store_kept_out();
  check_play();
  check_mercenaries();
  check_countdown();
  check_itembox();
  check_chapter_select();
  check_treasures();
  check_max_all();
  ImGui::DestroyContext();
  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  return g_fail ? 1 : 0;
}
