#pragma once

// The cheat switches: X(enum name, ini key, the ini's comment). The panel flips
// them, the tick applies them (cheats.cpp), and every change is written back to
// RE5CabbyCodes.ini (config.cpp), so they come back on the next start. A key
// that is no longer one goes in sw::kRetired: an ini that still has it is
// rewritten without it (config.cpp).
#define RE5CC_SWITCHES(X)                                                                                        \
  X(kGodMode, "GodMode",                                                                                          \
    "you take no damage, and your health is held at its maximum - and the jeep's, in chapter 2-3")                \
  X(kPartnerGod, "PartnerGodMode", "your AI partner takes no damage either")                                      \
  X(kOneHitKills, "OneHitKills", "any hit that does damage kills an enemy, through the game's own death")         \
  X(kInfiniteAmmo, "InfiniteAmmo",                                                                                \
    "the game's own infinite ammo for every weapon: no rounds used, no reloads - and no mounted gun overheats")    \
  X(kSteadyAim, "SteadyAim", "your aim holds still: no sway while you aim, no kick when you fire")               \
  X(kAutoQte, "AutoQTE",                                                                                          \
    "the quick time events play themselves: their prompts answered, grabs broken at once, cranks wound all the "   \
    "way, the volcano's boulder pushed over")                                                                       \
  X(kOpenMap, "OpenMapAtChapterStart",                                                                            \
    "your map opens by itself as each story chapter begins (its title on screen), as the map key would open it")   \
  X(kInfiniteMoney, "InfiniteMoney", "the shop takes no gold; selling still pays")                                \
  X(kFreezePlaytime, "FreezePlaytime", "the chapter's clock stands still (its rank goes by it)")                  \
  X(kFreezeCountdown, "FreezeCountdown",                                                                          \
    "the countdown on screen stands still: the Mercenaries' clock (time bonuses still add to it), chapter 3-2's "  \
    "escape, Desperate Escape's last wait")                                                                         \
  X(kNoDeathCount, "NoDeathCount", "the results: a death is not counted")                                         \
  X(kNoMissCount, "NoMissCount", "the results: a shot that misses is not counted (the accuracy stays at 100%)")   \
  X(kEndlessCombo, "EndlessCombo",                                                                                \
    "the Mercenaries: a combo never runs out, and its bonus goes onto your score as it is earned")                \
  X(kComboBonus, "ComboBonusAlwaysOn",                                                                            \
    "the Mercenaries: the Combo Bonus runs from the clock's start and never runs out")                            \
  X(kAutoKill, "KillEnemiesAsTheyAppear",                                                                         \
    "the Mercenaries: every enemy dies as it comes, the kill scored as your shot's; it lasts one match")

namespace re5cc::sw {

enum Kind {
#define RE5CC_ENUM(name, key, help) name,
  RE5CC_SWITCHES(RE5CC_ENUM)
#undef RE5CC_ENUM
      kCount
};

// Switches the mod no longer has.
constexpr const char* kRetired[] = {
    "InfinitePoints",  // the bonus features' exchange points (removed 2026-10-29)
};

}  // namespace re5cc::sw
