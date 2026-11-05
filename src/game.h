#pragma once

#include <cstdint>
#include <vector>

// What the mod knows about the game: its systems (MT Framework's singletons,
// the s* classes, found by their class names in the main system's own list of
// them), the units in play, the screen that is up, the characters, their
// inventories and the profile's and the chapter's numbers. Everything is found by
// name (mt.h) or by the code patterns in sigs.h, read out of the matched
// instructions and checked against each other - never by address.
namespace re5cc::game {

enum class Screen {
  kNone,     // nothing the mod knows (start-up, loading, the title and the menus)
  kPlay,     // a chapter or the Mercenaries, playing
  kPause,    // the pause menu
  kItemBox,  // Item Management (between chapters): the cases, the item box, the shop
};

// Who drives a character (uPlayer's own word for it): this machine's pad, the
// AI, or the other player over the network.
enum class Control { kLocal = 0, kAi = 1, kNetwork = 2 };

constexpr int kMaxChars = 4;  // sPlayer's characters
constexpr int kMaxVehicles = 4;
struct Character {
  uintptr_t obj = 0;
  int index = 0;  // in sPlayer's array
  Control control = Control::kLocal;
  const char* name = "";  // "Chris", "Sheva", ...
  int hp = 0, hp_max = 0;
};

// The game's modes (its play state's, cGameSys +0x58): the story's, and the two extra
// episodes that the game's own test of them names (kSigEpisodeCommit) - Lost in Nightmares
// (Chris and Jill) and Desperate Escape (Josh and Jill), each started from the story's menu
// with a loadout of its own. Versus is 1, the Mercenaries 2 and Reunion 5.
constexpr int kModeStory = 0, kModeNightmares = 3, kModeEscape = 4;

struct Run {
  uintptr_t main = 0;    // the main system (sBioMain) whose frame is running
  bool in_game = false;  // a game is in play (a chapter, the Mercenaries)
  int mode = -1;         // the game's mode (kModeStory ...), -1 when it cannot be read
  Screen screen = Screen::kNone;
  bool mercenaries = false;  // the Mercenaries' clock is running (its HUD is up)
  uintptr_t item_screen = 0;  // Item Management's unit (uIdItemBox00) while it is up
  uintptr_t pause_menu = 0;   // the pause menu's unit (uIdPause00) while it is open
  uintptr_t game_over = 0;    // the Game Over screen's unit (uIdGameover00) while it is up
  uintptr_t results = 0;      // the chapter's results' unit (uIdChapEd00) while it is up
  int nchars = 0;
  Character chars[kMaxChars];
  // The jeep of chapter 2-3 (uOmJeep) while it is in play: a pawn whose health is
  // kept in the characters' fields (Layout::hp).
  int nvehicles = 0;
  uintptr_t vehicles[kMaxVehicles] = {};
};

// From boot::unpacked(): the class records and what the mod needs of them.
bool discover();
bool ready();
const char* status_text();  // why not ready, for the panel

// The tick: `main` is the sBioMain whose frame is running.
void tick(void* main, Run* out);
const char* screen_name(Screen s);

// The matches of a signature (sigs.h) in the game's code - logged when their
// number is not the one sigs.h expects - and the one match of a unique one (0
// otherwise). call_target: where an `E8 rel32` at `at` goes (0 for no call).
std::vector<uintptr_t> sig_all(const char* name);
uintptr_t sig_one(const char* name);
uintptr_t call_target(uintptr_t at);

// A system by its class name ("sUnit", "sPlayer", ...), once the first tick has
// seen the main system's list: the instance, or 0.
uintptr_t system(const char* name);

// --- the characters ----------------------------------------------------------------------
struct Layout {
  uint32_t hp = 0, hp_max = 0;  // uCharacter, 16-bit
  uint32_t control = 0;         // uPlayer, 32-bit (Control)
  uint32_t case_mgr = 0;        // uPlayer: its cItemManager
  uint32_t players = 0;         // sPlayer: the characters' array
  int nplayers = 0;
  // cItemManager and cItem
  uint32_t items = 0;   // the manager's first item
  int nitems = 0;
  uint32_t item_size = 0;
  uint32_t it_id = 0, it_num = 0, it_most = 0, it_slot = 0, it_equip = 0, it_lv[9] = {}, it_alive = 0, it_model = 0;
  uint32_t model_owner = 0;  // a weapon model (uItem): the character cItem's set gives it to
  // sInfoMan's profiles and their game info
  uint32_t profile_index = 0;  // sInfoMan: the profile in use
  uint32_t system_base = 0, system_size = 0;  // cSystemInfo[]
  uint32_t game_base = 0, game_size = 0;      // cGameInfo[]
  uint32_t gold = 0;                          // cSystemInfo
  // cSystemInfo keeps a checkpoint copy of its data: each field again this far on.
  // The game copies the data over it at points in play and as the item management
  // closes, and takes it back - the gold and treasures when play goes back to it
  // (the pause menu's own options among others), all of it as a chapter starts.
  uint32_t checkpoint = 0;
  uint32_t counters = 0;  // cGameInfo: the first character's deaths; kills +4, shots +8, hits +12, time +16
  uint32_t counter_stride = 0;  // to the next character's
};
const Layout& layout();

// The characters as sPlayer has them (main thread).
int characters(Character out[kMaxChars]);
// The character at an index of sPlayer's array (a player's number), or 0.
uintptr_t character_at(int index);
const char* character_name(uintptr_t obj);
void set_health(uintptr_t obj, int hp);

// The profile in use (cSystemInfo) and its game info (cGameInfo), or 0.
uintptr_t system_info();
uintptr_t game_info();

// --- the inventory (cItemManager in each character) -------------------------------------
constexpr int kSlots = 9;  // the case's 3 x 3 grid
struct Item {
  uintptr_t at = 0;  // the cItem
  int id = 0, num = 0, most = 0, slot = 0, equip = 0;
  uint8_t lv[9] = {};
};
// The item in a slot (0-8) of the character's case, or false.
bool item_in(uintptr_t chara, int slot, Item* out);

}  // namespace re5cc::game
