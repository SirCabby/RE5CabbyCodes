#pragma once

// Every byte signature the mod finds the game's code by: SIG(name, "pattern", matches).
// `matches` is how many times the pattern occurs in build 11465250's code - most
// are unique; a few are searched only inside a function found by class name
// first, and say so. tools/sig.py --check runs them all over a dump
// (ghidra_proj/re5dx9.dumped.exe; DumpImage = 1 writes one). Each operand the
// mod reads out of a match is named where it is used (game.cpp, cheats.cpp).
#define RE5CC_SIGS(SIG)                                                                                              \
  /* uCharacter's set-health (its vtable slot 67): health +11, its most +29 (16-bit fields).                     */ \
  SIG(kSigSetLife,                                                                                                  \
      "66 8B 44 24 04 66 85 C0 66 89 81 ?? ?? ?? ?? 7D ?? 66 C7 81 ?? ?? ?? ?? 00 00 0F B7 81 ?? ?? ?? ?? 66 39 81 " \
      "?? ?? ?? ?? 7E ?? 66 89 81 ?? ?? ?? ?? C2 04 00",                                                             \
      1)                                                                                                             \
  /* uPlayer's damage (slot 66): who drives the character, +5 (2: the network partner - no damage here).        */ \
  SIG(kSigPlayerDamage, "57 8B F9 83 BF ?? ?? ?? ?? 02 75 06 32 C0 5F C2 08 00", 1)                                \
  /* uEnemy's damage (slot 66): the hit's value kept at +28, set-health's slot and health checked at +37.        */ \
  SIG(kSigEnemyDamage,                                                                                              \
      "F3 0F 10 44 24 08 56 8B F1 8B 4C 24 08 8B 06 8B 90 ?? ?? ?? ?? 51 8B CE F3 0F 11 86 ?? ?? ?? ?? FF D2 66 83 " \
      "BE ?? ?? ?? ?? 00 7F",                                                                                        \
      1)                                                                                                             \
  /* uEnemy's subtract-health (slot 69), which every hit on an enemy comes to: health +8 +15 +27 +43 +52, its    */ \
  /* most +36.                                                                                                   */ \
  SIG(kSigEnemySubtract,                                                                                            \
      "66 8B 44 24 04 66 29 81 ?? ?? ?? ?? 0F B7 81 ?? ?? ?? ?? 66 85 C0 7D ?? 66 C7 81 ?? ?? ?? ?? 00 00 0F B7 81 " \
      "?? ?? ?? ?? 66 39 81 ?? ?? ?? ?? 7E ?? 66 89 81 ?? ?? ?? ?? C2 04 00",                                        \
      1)                                                                                                             \
  /* The jeep's damage (uOmJeep's slot 66, chapter 2-3), its tail: the hit's damage and value (+0, +4) handed on  */ \
  /* to the pawns' damage (called +15), whose answer it returns after its own note of its health (called +24).   */ \
  SIG(kSigJeepDamage,                                                                                               \
      "8B 44 24 0C D9 44 24 10 51 D9 1C 24 50 8B CD E8 ?? ?? ?? ?? 8B CD 8A D8 E8 ?? ?? ?? ?? 5D 8A C3 5B C2 08 00", \
      1)                                                                                                             \
  /* The hit path's ask of an enemy whether a hit takes its health (thiscall (part, damage) -> yes, ret 8;       */ \
  /* uEnemy's says yes): the check's place in the vtable +4, and again +44 (its two calls).                      */ \
  SIG(kSigHitCheck,                                                                                                 \
      "8B 16 8B 92 ?? ?? ?? ?? 8B F8 66 0F B6 45 ?? 57 8B CE 50 FF D2 84 C0 0F 84 ?? ?? ?? ?? EB ?? 66 0F B6 4D "    \
      "?? 8B 7C 24 ?? 8B 06 8B 90 ?? ?? ?? ?? 57 51 8B CE FF D2",                                                    \
      1)                                                                                                             \
  /* The Ndesu's check (uEm45, the giant ending chapter 2-3), which takes the hit itself and says no - its tail: */ \
  /* its flags +2 (bits 9-10 the round; bit 11 the round's floor held), its gauge +60 and +116, the rounds'      */ \
  /* floors +69.                                                                                                 */ \
  SIG(kSigNdesuHit,                                                                                                 \
      "8B 8F ?? ?? ?? ?? 8B C1 C1 E8 09 A8 01 F3 0F 2A C5 F3 0F 59 44 24 ?? 5E 5D 74 ?? 8B D1 C1 EA 0A F6 D2 0F "    \
      "B6 C2 83 E0 01 83 C8 02 EB ?? 8B C1 C1 E8 0A 0F B6 C0 83 E0 01 F3 0F 10 8F ?? ?? ?? ?? F3 0F 10 14 85 ?? "    \
      "?? ?? ?? 0F 28 E1 F3 0F 5C E0 0F 2F D4 76 ?? C1 E9 0B F6 C1 01 74 ?? 0F 28 C1 F3 0F 5C C2 0F 2F C3 77 ?? "    \
      "0F 28 C3 F3 0F 5C C8 F3 0F 11 8F ?? ?? ?? ?? 32 C0 5F 83 C4 18 C2 08 00",                                     \
      1)                                                                                                             \
  /* A Plaga's check (uEm4502, the Ndesu's), which says no as well: while it is out (flags +2, bit 0) its count  */ \
  /* +15 takes the hit, below 0 its death (bit 3, the flags again +32 and +99), and the Ndesu (its parent, +39)  */ \
  /* loses as much off its gauge (+61, +73).                                                                     */ \
  SIG(kSigPlagaHit,                                                                                                 \
      "F6 81 ?? ?? ?? ?? 01 74 ?? 8B 54 24 08 29 91 ?? ?? ?? ?? 79 ?? 83 B9 ?? ?? ?? ?? 02 74 ?? 83 89 ?? ?? ?? "    \
      "?? 08 8B 41 ?? 85 C0 74 ?? 80 B9 ?? ?? ?? ?? 00 F3 0F 2A C2 75 ?? F3 0F 10 88 ?? ?? ?? ?? F3 0F 5C C8 F3 "    \
      "0F 11 88 ?? ?? ?? ?? F3 0F 10 88 ?? ?? ?? ?? F3 0F 5C C8 F3 0F 11 88 ?? ?? ?? ?? 83 89 ?? ?? ?? ?? 20 32 "    \
      "C0 C2 08 00",                                                                                                 \
      1)                                                                                                             \
  /* An enemy's region wear (thiscall (region, change), ret 8; worn through it answers 1): the regions' last +9  */ \
  /* (31), 12 bytes each - what is left +19 (+60, +82, +108; an int), worn through +39 (+76; a byte), its most   */ \
  /* +99 (an index, x 4). Worn through only while the enemy may be hurt (+47): else 1 is left.                   */ \
  SIG(kSigRegionWear,                                                                                                \
      "57 8B 7C 24 08 32 C0 83 FF ?? 77 ?? 53 8D 14 7F 8B 9C 91 ?? ?? ?? ?? 56 8D 34 91 8B 54 24 14 03 D3 85 D2 "    \
      "7F ?? 38 86 ?? ?? ?? ?? 75 ?? 38 81 ?? ?? ?? ?? 74 ?? BA 01 00 00 00 89 96 ?? ?? ?? ?? 5E 5B 5F C2 08 00 "    \
      "B0 01 33 D2 88 86 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? 5E 5B 5F C2 08 00 3B D3 7E ?? 8D 94 7F ?? ?? ?? ?? 8B 14 "    \
      "91 89 96 ?? ?? ?? ?? 5E 5B 5F C2 08 00",                                                                      \
      1)                                                                                                             \
  /* Irving's check (uEm46, Irving (Transformed), the boss ending chapter 3-3), which says no to its body: a     */ \
  /* part below +9 (a word) is its body's and wears region +60 by the damage (the wear called +63); the others   */ \
  /* are its core's (the part less -(+90); the core +84, +108), which take health while the core may be hurt     */ \
  /* (+114). The parts' kinds +22 and +96 (looked up +26, +101).                                                 */ \
  SIG(kSigIrvingHit,                                                                                                 \
      "8B 44 24 04 53 32 DB 66 3D ?? ?? 56 8B F1 73 ?? 0F B7 C0 50 8D 8E ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 F8 0B 74 "    \
      "?? 83 F8 02 75 ?? 38 9E ?? ?? ?? ?? 74 ?? 57 8B 7C 24 14 8B CF F7 D9 51 6A ?? 8B CE E8 ?? ?? ?? ?? 01 BE "    \
      "?? ?? ?? ?? 5F 5E 8A C3 5B C2 08 00 8B 8E ?? ?? ?? ?? 83 C0 ?? 0F B7 D0 81 C1 ?? ?? ?? ?? 52 E8 ?? ?? ?? "    \
      "?? 8B 8E ?? ?? ?? ?? 80 B9 ?? ?? ?? ?? 00 75 ?? 83 F8 0B 75 ?? 8B 54 24 10 01 96 ?? ?? ?? ?? B0 01 88 86 "    \
      "?? ?? ?? ?? 5E 5B C2 08 00",                                                                                  \
      1)                                                                                                             \
  /* A tentacle's check (uEmc0, four of Irving's), which says no: a hit on its weak spot (kind 11), or any while */ \
  /* more than a tenth of region +69 is left (+55; its most +27), wears it by the damage (the wear called +72).  */ \
  /* The parts' kinds +11 (looked up +15).                                                                       */ \
  SIG(kSigTentacleHit,                                                                                               \
      "0F B7 44 24 04 56 8B F1 50 8D 8E ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 F8 0B 74 ?? 8B 86 ?? ?? ?? ?? 8D 0C 80 03 "    \
      "C9 B8 1F 85 EB 51 F7 E9 C1 FA 05 8B CA C1 E9 1F 03 CA 39 8E ?? ?? ?? ?? 7E ?? 8B 54 24 0C F7 DA 52 6A ?? "    \
      "8B CE E8 ?? ?? ?? ?? 32 C0 5E C2 08 00",                                                                      \
      1)                                                                                                             \
  /* The Reaper's check (uEm34, chapters 5 and 6), which takes health only from its weak spot: a part from +8    */ \
  /* on is not its body's (no); in two of its actions (+59: +65, then +70 more - hanging, holding a player)      */ \
  /* none; the weak spot's parts (from -(+78), +81 more) take it. Any other part only past its marks: the        */ \
  /* first +42 (+88, +107, +121) alone takes health to the last point at most (health +130; subtract-health      */ \
  /* through the vtable called +147), with the second +48 (+94, +115) the hit's. It clears both, either way.     */ \
  SIG(kSigReaperHit,                                                                                                 \
      "66 8B 54 24 04 66 83 FA ?? 72 ?? 8B 81 ?? ?? ?? ?? 8B D0 C1 EA 15 F6 C2 01 75 ?? 0D 00 00 10 00 89 81 "       \
      "?? ?? ?? ?? 32 C0 88 81 ?? ?? ?? ?? 88 81 ?? ?? ?? ?? 32 C0 C2 08 00 8B 81 ?? ?? ?? ?? 83 E8 ?? 74 ?? "       \
      "83 E8 ?? 74 ?? 0F B7 C2 83 C0 ?? 83 F8 ?? 77 ?? 32 C0 88 81 ?? ?? ?? ?? 88 81 ?? ?? ?? ?? B0 01 C2 08 "       \
      "00 32 C0 38 81 ?? ?? ?? ?? 74 ?? 38 81 ?? ?? ?? ?? 88 81 ?? ?? ?? ?? 75 ?? 0F BF 81 ?? ?? ?? ?? 8B 54 "       \
      "24 08 83 C0 FF 3B D0 0F 4D D0 52 E8 ?? ?? ?? ?? 32 C0 C2 08 00",                                              \
      1)                                                                                                             \
  /* ... and its damage (uEm34's, two places on): its own instant kill of one (a hit of the kind +10 at +6)      */ \
  /* sets both marks (+20, +26) and the hit's whole-health bit (its flags +32), as the second mark alone does    */ \
  /* (+47); any other hit has uEnemy's damage (jumped to +58).                                                   */ \
  SIG(kSigReaperKill,                                                                                                \
      "8B 44 24 04 81 B8 ?? ?? ?? ?? ?? ?? ?? ?? 75 ?? B2 01 88 91 ?? ?? ?? ?? 88 91 ?? ?? ?? ?? 81 48 ?? 00 "       \
      "00 00 10 B8 ?? ?? ?? ?? C2 04 00 80 B9 ?? ?? ?? ?? 00 75 ?? 89 44 24 04 E9 ?? ?? ?? ??",                      \
      1)                                                                                                             \
  /* Uroboros Aheri's check (uEm42, Excella's, the boss ending chapter 6-2): its body's parts below +10 (a word;  */ \
  /* the part less it, +34) take health; from there on a part is one of its cores' (the core = the part / it - 1,  */ \
  /* the last +155), which say no: during its doom attack (its flags +164, bit +168) the hit goes into its       */ \
  /* escape (+177, +183), past +187 broken off; else into the core's flinch and - the core open - its health     */ \
  /* (the core wear called +330), then the wear asked (+341) whether the core is worn through (it breaks).       */ \
  SIG(kSigAheriHit,                                                                                                  \
      "56 8B F1 8B 4C 24 08 66 83 F9 ?? 72 ?? 0F B7 D1 B8 89 88 88 88 F7 E2 C1 EA 05 0F B7 C2 8B D0 90 81 C1 ?? ?? "  \
      "00 00 83 E8 01 75 F5 85 D2 75 ?? 8B 44 24 0C 01 86 ?? ?? ?? ?? 01 86 ?? ?? ?? ?? 81 BE ?? ?? ?? ?? ?? ?? ?? "  \
      "?? 8B 8E ?? ?? ?? ?? 7E ?? 81 8E ?? ?? ?? ?? ?? ?? ?? ?? C7 86 ?? ?? ?? ?? 00 00 00 00 B0 01 5E C2 08 00 81 "  \
      "F9 ?? ?? ?? ?? 7E ?? 81 8E ?? ?? ?? ?? ?? ?? ?? ?? C7 86 ?? ?? ?? ?? 00 00 00 00 B0 01 5E C2 08 00 57 8D 7A "  \
      "FF 85 FF 0F 8C ?? ?? ?? ?? 83 FF ?? 0F 8F ?? ?? ?? ?? F6 86 ?? ?? ?? ?? ?? 74 ?? 8B 44 24 10 01 86 ?? ?? ?? "  \
      "?? 81 BE ?? ?? ?? ?? ?? ?? ?? ?? 0F 8E ?? ?? ?? ?? 81 8E ?? ?? ?? ?? ?? ?? ?? ?? 5F 32 C0 5E C2 08 00 0F B7 "  \
      "C9 53 8B 9C BE ?? ?? ?? ?? 51 8D 8B ?? ?? ?? ?? E8 ?? ?? ?? ?? 8A 8B ?? ?? ?? ?? F6 C1 ?? BA ?? ?? ?? ?? 74 "  \
      "?? F6 C1 ?? 75 ?? BA ?? ?? ?? ?? 83 F8 0B F3 0F 2A C2 F3 0F 59 84 BE ?? ?? ?? ?? 55 F3 0F 2C E8 74 ?? 8B 0D "  \
      "?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 8B 44 24 18 01 84 BE ?? ?? ?? ?? 8A 9B ?? ?? ?? ?? F6 C3 ?? 74 ?? F6 C3 ?? "  \
      "74 ?? 50 57 8B CE E8 ?? ?? ?? ?? 33 DB 53 57 8B CE E8 ?? ?? ?? ?? 84 C0 74 ?? 8B CF 89 9C BE ?? ?? ?? ?? BA "  \
      "01 00 00 00 5D D3 E2 5B 5F 09 96 ?? ?? ?? ?? 32 C0 5E C2 08 00",                                              \
      1)                                                                                                             \
  /* ... and its cores' wear (thiscall (core, change), ret 8; worn through it answers 1): on this machine only    */ \
  /* while it hosts the game, the core's health +43 (+51, +64, +82, +92, +100; a word, the cores +42's scale     */ \
  /* apart) less the change, kept within 0 and its most (+74).                                                   */ \
  SIG(kSigCoreWear,                                                                                                  \
      "A1 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 3B 90 ?? ?? ?? ?? 74 05 32 C0 C2 08 00 8B 54 24 04 66 8B "  \
      "44 24 08 66 29 84 D1 ?? ?? ?? ?? 0F B7 84 D1 ?? ?? ?? ?? 66 85 C0 7D 0A 66 C7 84 D1 ?? ?? ?? ?? 00 00 0F B7 "  \
      "84 D1 ?? ?? ?? ?? 66 39 84 D1 ?? ?? ?? ?? 7E 08 66 89 84 D1 ?? ?? ?? ?? 66 83 BC D1 ?? ?? ?? ?? 00 0F 9E C0 "  \
      "C2 08 00",                                                                                                    \
      1)                                                                                                             \
  /* cSystemInfo's return to its checkpoint copy (gold and treasures back from it, then all of it copied over    */ \
  /* again): the copy's gold +4, gold +12, the copy's treasures +21, treasures +27, the size +32, the copy +38.  */ \
  SIG(kSigCheckpoint,                                                                                               \
      "8B C1 8B 88 ?? ?? ?? ?? 56 57 89 88 ?? ?? ?? ?? 8D 50 04 8D B0 ?? ?? ?? ?? 8D B8 ?? ?? ?? ?? 68 ?? ?? ?? ?? " \
      "52 05 ?? ?? ?? ?? B9 36 00 00 00 50",                                                                         \
      1)                                                                                                             \
  /* sPlayer's "is this one of the characters": the characters' array +12, how many +28.                         */ \
  SIG(kSigIsPlayer,                                                                                                 \
      "8B 54 24 04 85 D2 74 17 33 C0 83 C1 ?? 8D 49 00 39 11 74 10 83 C0 01 83 C1 04 83 F8 ?? 7C F1 32 C0 C2 04 00", \
      1)                                                                                                             \
  /* cItem's set: each field's offset (id +2, count +9, the levels, its most +74, slot +82, equipped +89 ...).    */ \
  SIG(kSigItemSet,                                                                                                  \
      "89 56 ?? 8B 54 24 10 89 4E ?? 8B 4C 24 14 89 56 ?? 8A 55 20 89 4E ?? 8A 4D 24 88 56 ?? 8A 55 28 88 4E ?? 8A " \
      "4D 34 88 46 ?? 8A 45 30 88 56 ?? 8A 55 38 88 46 ?? 8A 45 3C 88 4E ?? 8A 4D 40 88 56 ?? 8B 54 24 18 89 7E ?? " \
      "8B 7D 44 C7 46 ?? 0D 00 00 00 C7 46 ?? 00 00 00 00 88 46 ?? 88 4E ??",                                      \
      1)                                                                                                             \
  /* ... the weapon model it was handed +5 (and the slot again +8) ...                                         */  \
  SIG(kSigItemModel, "8B 55 4C 89 56 ?? 89 46 ?? 8B 4E ?? 85 C9 74", 1)                                               \
  /* ... and its end: alive +16.                                                                                 */ \
  SIG(kSigItemAlive, "89 B8 ?? ?? ?? ?? C6 80 ?? ?? ?? ?? 01 5F C6 46 ?? 01 5E B0 01 5B 8B E5 5D C2 48 00", 1)       \
  /* cItemManager's add, the call that loads a character's case at a chapter's start: the manager in the        */ \
  /* character at +2 (lea ecx,[ebx+21A0h]; call add).                                                            */ \
  SIG(kSigCaseLoad, "8D 8B ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F8 B8 01 00 00 00 39 46", 1)                              \
  /* cItemManager's constructor (and its destructor, alike): its items from +8, how many (less one) +10.         */ \
  SIG(kSigItemMgrCtor, "C7 01 ?? ?? ?? ?? 8D 71 ?? BD ?? 00 00 00 33 DB", 2)                                       \
  /* cItem's clear (the inventory's own removal; its argument: release the weapon model too): the alive flag +37. */ \
  SIG(kSigItemClear,                                                                                                \
      "8B 44 24 04 53 56 57 50 8B F1 E8 ?? ?? ?? ?? 6A 2C 33 DB 8D 7E 04 53 57 E8 ?? ?? ?? ?? 83 C4 0C 89 1F 5F 88 " \
      "5E ?? 89 5E",                                                                                                 \
      1)                                                                                                             \
  /* The tail of cItemManager's add (18 arguments; under sItem's lock): the function starts 0x59 before.         */ \
  SIG(kSigItemAdd, "52 51 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? C2 48 00", 1)                                           \
  /* The case grid's own add of an item: the character's field it hands on, +2.                                  */ \
  SIG(kSigGridAdd, "8B 87 ?? ?? ?? ?? 8B 4C 24 1C 8B 54 24 18 6A 00 50 8B 44 24 28 57", 1)                       \
  /* The weapons' "is it infinite": the game's own debug switch (a stub that answers 0) called at +19.           */ \
  SIG(kSigInfinite,                                                                                                 \
      "80 7F 23 ?? 76 07 5F B0 01 5E C2 04 00 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0 75 EA", 1)                      \
  /* The weapons' update (uWeapon's slot 98), its overheating: the gun's owner +2 (a weapon model's, as cItem's  */ \
  /* set gives it) - a uPlayer (its record pushed +15)? Driven by this machine's pad (the driver +36; jne +41    */ \
  /* when not): the heat's index by the level (+76, +120). Else index 0 (+135) and, for the jeep's two guns       */ \
  /* (the first one's id: minus +154), the firing forgotten ("Add" +165; ja +161 past it). Then the firing's     */ \
  /* test (Add +175): the heat goes up while it is set.                                                          */ \
  SIG(kSigOverheat,                                                                                                 \
      "8B 8E ?? ?? ?? ?? 85 C9 0F 84 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F8 85 FF 0F 84 ?? ?? ?? ?? 83 BF " \
      "?? ?? ?? ?? 00 75 ?? 0F B6 87 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 83 F8 07 " \
      "7D ?? C6 86 ?? ?? ?? ?? 00 EB ?? 0F B6 8F ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? " \
      "?? 83 F8 0A 0F 9D C2 80 C2 01 88 96 ?? ?? ?? ?? EB ?? 8B 06 8B 50 10 8B CE C6 86 ?? ?? ?? ?? 00 FF D2 8B 40 " \
      "20 C1 E8 08 25 FF FF 00 00 05 ?? ?? ?? ?? 83 F8 01 77 ?? C6 86 ?? ?? ?? ?? 00 0F 57 C0 80 BE ?? ?? ?? ?? 01 " \
      "75 ??",                                                                                                       \
      1)                                                                                                             \
  /* cSystemInfo's take-gold and take-points (gold's the lower field): the field +2; the clamp's xor +14, the     */ \
  /* sub +25 (2 bytes each). Only take-gold is patched; take-points cross-checks it (the next field).            */ \
  SIG(kSigTake,                                                                                                     \
      "8B 81 ?? ?? ?? ?? 8B 54 24 04 3B C2 73 0B 33 C0 89 81 ?? ?? ?? ?? C2 04 00 2B C2 89 81 ?? ?? ?? ?? C2 04 00", \
      2)                                                                                                             \
  /* The shop's read of the gold: sInfoMan's profile index +7, the profile's stride +10, the gold +17.          */  \
  SIG(kSigShopGold, "A1 ?? ?? ?? ?? 8B 48 ?? 69 C9 ?? ?? ?? ?? 8B 94 01 ?? ?? ?? ?? 89 56 78", 1)                   \
  /* A death counted (call add-deaths): sInfoMan's profile index +2, the game info's stride +5, its base +15.     */ \
  SIG(kSigDeathCounted, "8B 56 20 69 D2 ?? ?? ?? ?? 6A 01 55 8D 8C 32 ?? ?? ?? ?? E8", 1)                          \
  /* cGameInfo's four counters (deaths, kills, shots, hits - in that order in memory): each one's field +10.     */ \
  SIG(kSigCounter,                                                                                                  \
      "8B 44 24 04 8D 04 40 8D 8C C1 ?? ?? ?? ?? 8B 44 24 08 03 01 3D 7F 96 98 00 76 05 B8 7F 96 98 00 89 01 C2 08 " \
      "00",                                                                                                          \
      4)                                                                                                             \
  /* cGameInfo's add-play-time: the field's (index * 8) +15.                                                     */ \
  SIG(kSigPlayTime, "8B 44 24 04 F3 0F 10 0D ?? ?? ?? ?? 8D 84 40 ?? ?? ?? ?? F3 0F 10 04 C1 F3 0F 58 44 24 08", 1) \
  /* The Mercenaries' clock running down: the time read +4, the store of the new time +39 (8 bytes).            */ \
  SIG(kSigCountdown,                                                                                                \
      "F3 0F 10 87 ?? ?? ?? ?? F3 0F 10 54 24 08 0F 57 C9 F3 0F 5C C2 0F 2F C8 72 0D 0F 28 C1 C7 87 ?? ?? ?? ?? 01 " \
      "00 00 00 F3 0F 11 87 ?? ?? ?? ??",                                                                            \
      1)                                                                                                             \
  /* A stage's countdown running down in its frame - chapter 3-2's escape from the oil field (stage 209) and the */ \
  /* wait for the helicopter ending Desperate Escape (stage 704): the seconds left +9 +25 +35 (a float of the    */ \
  /* stage's), the frame time taken off them +13 (5 bytes), the floor kept in xmm1.                             */ \
  SIG(kSigStageCountdown,                                                                                           \
      "A1 ?? ?? ?? ?? F3 0F 10 83 ?? ?? ?? ?? F3 0F 5C 40 28 0F 2F C8 F3 0F 11 83 ?? ?? ?? ?? 72 08 F3 0F 11 8B ?? " \
      "?? ?? ??",                                                                                                    \
      2)                                                                                                             \
  /* The HUD's countdown (uIdCockpit00's; thiscall (seconds), ret 4): the most it shows +10 (a float's address),  */ \
  /* the seconds it shows +21 and +38, its "shown" flag +27.                                                     */ \
  SIG(kSigHudCountdown,                                                                                             \
      "F3 0F 10 44 24 04 F3 0F 10 0D ?? ?? ?? ?? 0F 2F C1 F3 0F 11 81 ?? ?? ?? ?? C6 81 ?? ?? ?? ?? 01 72 08 F3 0F " \
      "11 89 ?? ?? ?? ?? C2 04 00",                                                                                  \
      1)                                                                                                             \
  /* sArea's frame (its vtable slot 6): its areas +9 (a pointer each, as many as +73, the first null ending      */ \
  /* them), each one's "started" byte +24 and +54, then each one's frame called through its vtable, +62.         */ \
  SIG(kSigAreaFrame,                                                                                                \
      "55 56 8B E9 57 33 FF 8D B5 ?? ?? ?? ?? 8D 49 00 8B 06 85 C0 74 ?? 80 78 ?? 00 75 ?? 8B C8 8B 01 8B 50 ?? FF " \
      "D2 84 C0 74 ?? 8B 0E 8B 01 8B 50 ?? FF D2 8B 06 C6 40 ?? 01 8B 0E 8B 11 8B 42 ?? FF D0 83 C7 01 83 C6 04 83 " \
      "FF ?? 7C",                                                                                                    \
      1)                                                                                                             \
  /* The action buttons' check (sActionButton; thiscall (unit, pad, player) -> pressed): the unit's request +6,  */ \
  /* the unit's "drawn" flag +15; the request's "answered" flag +20.                                             */ \
  SIG(kSigQteCheck, "53 56 8B F1 8B 46 ?? 32 DB 33 C9 3B C1 88 5E ?? 74 ?? 80 78 ?? 01 74 ?? 80 78 ?? 01", 1)        \
  /* ... its read of the request's button type, +5 (the request +2, the drawn flag +12) ...                      */ \
  SIG(kSigQteType, "8B 56 ?? 8B 42 ?? 3B C1 75 ?? C6 46 ?? 01", 1)                                                  \
  /* ... of the unit's kind of input, +2 (1 a press, 3 two at once, 4 and 6 mashing, 5 a hold, 7-9 the stick) ... */ \
  SIG(kSigQteKind, "8B 46 ?? 83 C0 FF 83 F8 08 0F 87", 1)                                                            \
  /* ... and its AUTO tail: the "AUTO?" test called at +6, the character's driver +21, the request +34, its      */ \
  /* category +37, its label +40.                                                                                */ \
  SIG(kSigQteAuto,                                                                                                  \
      "8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0 0F 84 ?? ?? ?? ?? 83 BF ?? ?? ?? ?? 00 0F 85 ?? ?? ?? ?? 8B 46 ?? 8B 48 " \
      "?? 8B 50 ??",                                                                                                 \
      1)                                                                                                             \
  /* ... and in it AUTO's answer to the volcano's boulder's two Pushes (category 8): their labels +2 (the first, */ \
  /* a u32) and +10 (the second, past the first), the unit's drawn flag +15, the request's seconds up +21.       */ \
  SIG(kSigQteAutoPush, "81 EA ?? ?? ?? ?? 74 ?? 83 EA ?? 75 ?? C6 46 ?? 00 F3 0F 10 40 ?? 0F 2F 05", 1)              \
  /* A player's button manager calling the check: the call at +10.                                              */ \
  SIG(kSigQteCall, "8B 53 08 52 8D 43 10 50 8B CE E8 ?? ?? ?? ?? 3C 01 75", 1)                                     \
  /* The profile's Action Assist tests, "AUTO?" (== 0) and "not OFF?" (!= 2): sInfoMan's profile index +2, the   */ \
  /* profile's stride +5, the setting (a word in cSystemInfo, from sInfoMan) +13.                                */ \
  SIG(kSigAssistAuto, "8B 41 ?? 69 C0 ?? ?? ?? ?? 66 83 7C 08 ?? 00 0F 94 C0 C3", 1)                                 \
  SIG(kSigAssistOn, "8B 41 ?? 69 C0 ?? ?? ?? ?? 66 83 7C 08 ?? 02 0F 95 C0 C3", 1)                                   \
  /* The player's aim (cPadEx's, each frame of an aim): the breathing sway's pitch and yaw, each the sine of its */ \
  /* phase (+4, +68) times its sizes - the last multiplies at +50 and +126 (6 bytes each), then the stores of    */ \
  /* the two (+56, +132).                                                                                        */ \
  SIG(kSigAimSway,                                                                                                  \
      "F3 0F 10 86 ?? ?? ?? ?? F3 0F 59 05 ?? ?? ?? ?? 0F 5A C0 E8 ?? ?? ?? ?? F2 0F 5A C0 F3 0F 59 83 ?? ?? ?? ?? " \
      "F3 0F 59 83 ?? ?? ?? ?? F3 0F 59 44 24 ?? F3 0F 59 44 24 ?? F3 0F 11 86 ?? ?? ?? ?? F3 0F 10 86 ?? ?? ?? ?? " \
      "F3 0F 59 05 ?? ?? ?? ?? 0F 5A C0 E8 ?? ?? ?? ?? 80 BE ?? ?? ?? ?? 00 A1 ?? ?? ?? ?? F2 0F 5A C0 F3 0F 59 80 " \
      "?? ?? ?? ?? F3 0F 59 80 ?? ?? ?? ?? F3 0F 59 44 24 ?? F3 0F 59 44 24 ?? F3 0F 11 86",                         \
      1)                                                                                                             \
  /* ... a shot's kick of it: the pitch's (push the weapon; mov ecx; call - 8 bytes, the result on the FPU) ...  */ \
  SIG(kSigAimKickPitch, "53 8B CF E8 ?? ?? ?? ?? D9 54 24 ?? F3 0F 10 96 ?? ?? ?? ?? D9 9E", 1)                     \
  /* ... and the yaw's (push the last kick, the weapon; mov ecx; call - 16 bytes).                               */ \
  SIG(kSigAimKickYaw, "D9 44 24 ?? 51 D9 1C 24 53 8B CF E8 ?? ?? ?? ?? D9 96", 1)                                   \
  /* The mouse crosshair's opening at a shot (the HUD's; thiscall, 3 arguments).                                 */ \
  SIG(kSigCrosshairOpen,                                                                                            \
      "8B 81 ?? ?? ?? ?? 85 C0 74 ?? F3 0F 10 44 24 04 8A 4C 24 0C F3 0F 11 80 ?? ?? ?? ?? F3 0F 10 44 24 08 F3 0F " \
      "11 80 ?? ?? ?? ?? 88 88 ?? ?? ?? ?? C2 0C 00",                                                                \
      1)                                                                                                             \
  /* A scope going up (an aim through a scoped weapon): the scope camera asked for (called +10), the camera's    */ \
  /* "scope" flag +17 set, then the character's +24 (a byte) ...                                                 */ \
  SIG(kSigScopeOpen, "0F B6 86 ?? ?? ?? ?? 50 8B CF E8 ?? ?? ?? ?? C6 87 ?? ?? ?? ?? 01 C6 86 ?? ?? ?? ?? 01", 1)  \
  /* ... and coming down: the camera's flags cleared (its "scope" +16), then the character's +29.                */ \
  SIG(kSigScopeClose,                                                                                               \
      "C6 87 ?? ?? ?? ?? 00 C6 87 ?? ?? ?? ?? 00 C6 87 ?? ?? ?? ?? 00 8B 86 ?? ?? ?? ?? C6 86 ?? ?? ?? ?? 00 81 60 " \
      "04 FF F7 FF FF",                                                                                              \
      1)                                                                                                             \
  /* uModel's set of a motion on one of its layers (thiscall (layer, motion, blend-in, frame, speed, attributes)): */ \
  /* the layers in use +9; the first layer's index +40 and a layer's size +43 (the layers from index x size on);  */ \
  /* a layer's blend-in count +72 (and +84), its motion's number +63 (and +116), its speed +121, attributes +125. */ \
  SIG(kSigMotionSet,                                                                                                \
      "8B 54 24 04 53 8B D9 3B 93 ?? ?? ?? ?? 72 06 32 C0 5B C2 18 00 F3 0F 10 4C 24 10 F3 0F 10 15 ?? ?? ?? ?? 0F " \
      "57 C0 8D 42 ?? 69 C0 ?? ?? ?? ?? 03 C3 33 C9 3B D1 F3 0F 11 48 ?? 75 14 66 81 78 ?? FF FF 75 0C F3 0F 11 40 " \
      "?? F3 0F 11 40 ?? EB 0A F3 0F 11 48 ?? F3 0F 11 50 ?? 8B 54 24 0C 81 FA FF FF 00 00 F3 0F 10 5C 24 18 55 56 " \
      "57 8B 7C 24 28 66 89 50 ?? F3 0F 11 58 ?? 66 89 78 ??",                                                      \
      1)                                                                                                             \
  /* ... a motion that does not loop (its loop frame, +2, below 0) gets the attribute bit +7 (attributes +11);   */ \
  /* the model's frame time +16.                                                                                 */ \
  SIG(kSigMotionLoop, "39 4A ?? 7D 07 83 CF ?? 66 89 78 ?? F3 0F 10 43 ?? 8D 72 ?? 83 C2 ??", 1)                    \
  /* uModel's motion clock, each layer in use (+2) from the first (index +55, size +58): its speed (+67) times   */ \
  /* the model's frame time (+52) is what moves its frame on; its blend-in count (+73, stored +102) runs down by */ \
  /* the frame time alone.                                                                                       */ \
  SIG(kSigMotionClock,                                                                                              \
      "39 B0 ?? ?? ?? ?? 57 89 74 24 1C 0F 86 ?? ?? ?? ?? 0F 57 DB F3 0F 10 25 ?? ?? ?? ?? BB ?? 00 00 00 BF FE FF " \
      "00 00 EB 08 8D A4 24 00 00 00 00 90 F3 0F 10 48 ?? 83 C6 ?? 69 F6 ?? ?? ?? ?? F3 0F 10 74 06 ?? F3 0F 10 44 " \
      "06 ?? 03 F0 0F 2F C3 F3 0F 59 F1 F3 0F 11 74 24 18 76 ?? F3 0F 5C C1 0F 2F D8 F3 0F 11 46 ??",               \
      1)                                                                                                             \
  /* The item box (cSystemInfo's): "is there an empty entry" - the box +2, an entry's size +19, how many +22.    */ \
  SIG(kSigBoxFree, "81 C1 ?? ?? ?? ?? 33 C0 66 83 39 00 74 ?? 83 C0 01 83 C1 ?? 83 F8 ?? 7C ?? 32 C0 C3 B0 01 C3",  \
      1)                                                                                                             \
  /* ... its store's rules (on the id in esi): never this one +2; up to the most +80 for +14..+27, +35, +43,     */ \
  /* +51..+59 and from minus +67 the span +73; one an entry for the rest - and the box again +86.                */ \
  SIG(kSigBoxRules,                                                                                                 \
      "81 FE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 81 FE ?? ?? ?? ?? B8 01 00 00 00 72 ?? 81 FE ?? ?? ?? ?? 76 ?? 81 FE ?? " \
      "?? ?? ?? 74 ?? 81 FE ?? ?? ?? ?? 74 ?? 81 FE ?? ?? ?? ?? 72 ?? 81 FE ?? ?? ?? ?? 76 ?? 8D 96 ?? ?? ?? ?? 81 " \
      "FA ?? ?? ?? ?? 77 ?? B8 ?? ?? ?? ?? 81 C1 ?? ?? ?? ??",                                                        \
      1)                                                                                                             \
  /* ... a chapter's start, dropping from it the ids from +6 on but two, +13 and +20 (the vests).               */ \
  SIG(kSigBoxKeep, "0F B7 08 66 81 F9 ?? ?? 72 ?? 66 81 F9 ?? ?? 74 ?? 66 81 F9 ?? ?? 74 ?? 66 0F EF C0", 1)       \
  /* Item Management's sale of a gun's rounds: the box's store on the profile's cSystemInfo (the profile +2, the */ \
  /* size +5, the array +12), the store called +13, then the screen's rebuild of a pane's box list +21.          */ \
  SIG(kSigBoxRebuildCall, "8B 48 ?? 69 C9 ?? ?? ?? ?? 8D 4C 01 ?? E8 ?? ?? ?? ?? 57 8B CE E8 ?? ?? ?? ??", 1)        \
  /* Item Management's frame, its two panes: the first's +5 at +2, the step to the next +19 ...                  */ \
  SIG(kSigBoxPanes, "8D B5 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B D3 83 F2 01 69 D2 ?? ?? ?? ?? 89 99 ?? ?? ?? ?? 80 3E 00", \
      1)                                                                                                             \
  /* ... and the update of each one in use: in use while its field (+2, from the +5) is not negative; the pane   */ \
  /* from the +5 (+6, a negative byte); its update called +12; the step +22.                                     */ \
  SIG(kSigBoxPaneUpdate, "83 7E ?? 00 8D 46 ?? 7C ?? 50 8B CD E8 ?? ?? ?? ?? 83 C3 01 81 C6 ?? ?? ?? ?? 83 FB 02", 1) \
  /* Item Management's cell of a saved item: the seven levels a box entry keeps, from the saved item's offsets   */ \
  /* +3 +10 +17 +24 +31 +38 +45 to the cell's +6 +13 +20 +27 +34 +41 +49, and the eighth cleared (+52).          */ \
  SIG(kSigBoxLevels,                                                                                                \
      "0F B6 50 ?? 88 56 ?? 0F B6 48 ?? 88 4E ?? 0F B6 50 ?? 88 56 ?? 0F B6 48 ?? 88 4E ?? 0F B6 50 ?? 88 56 ?? 0F " \
      "B6 48 ?? 88 4E ?? 0F B6 50 ?? 57 88 56 ?? C6 46 ?? 00",                                                       \
      1)                                                                                                             \
  /* A character made from the saved state (as play starts or restarts): cGameInfo's step +8 (and a quarter of  */ \
  /* it +2), a character's case list +16, its record +35 (and +43), the case lists +50 and their counts +57     */ \
  /* (both from sInfoMan).                                                                                       */ \
  SIG(kSigSavedCases,                                                                                               \
      "69 C0 ?? ?? ?? ?? 69 C9 ?? ?? ?? ?? 8B DF 69 DB ?? ?? ?? ?? 8D 34 BF C1 E6 04 03 F1 03 D9 03 C7 80 BC 16 ?? ?? " \
      "?? ?? 00 8D B4 16 ?? ?? ?? ?? 8D 8C 13 ?? ?? ?? ?? 8B 94 82 ?? ?? ?? ??",                                     \
      1)                                                                                                             \
  /* ... and its treasures from its saved ones (a list of an id and a count an entry): a character's step +2,    */ \
  /* sInfoMan's profile index +13, cGameInfo's size +16, sPlayer's lists +25, the saved lists +32 (from          */ \
  /* sInfoMan), a list's entries +37.                                                                            */ \
  SIG(kSigTreasureLoad,                                                                                             \
      "69 FF ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 50 ?? 69 D2 ?? ?? ?? ?? 03 D7 8D BC 2F ?? ?? ?? ?? 8D B4 02 ?? ?? ?? ?? " \
      "B9 ?? 00 00 00 F3 A5",                                                                                        \
      1)                                                                                                             \
  /* sPlayer's add of a treasure to a character's list: a character's step +2, the lists +14; an entry of it     */ \
  /* below the most (+63, 16-bit) topped up first, else an empty one taken - the entries +74 and +95.            */ \
  SIG(kSigTreasureAdd,                                                                                              \
      "69 C0 ?? ?? ?? ?? 80 7E 1C 00 57 8D BC 30 ?? ?? ?? ?? 75 09 80 3D ?? ?? ?? ?? 00 74 0A 8D 4E 04 51 FF 15 ?? " \
      "?? ?? ?? 33 C9 8B C7 BA 01 00 00 00 55 90 0F B7 28 39 6C 24 18 75 08 66 81 78 02 ?? ?? 72 ?? 03 CA 83 C0 04 " \
      "83 F9 ?? 7C E5 33 C9 8B C7 90 66 83 38 00 74 ?? 03 CA 83 C0 04 83 F9 ?? 7C F0",                              \
      1)                                                                                                             \
  /* cSystemInfo's count of a treasure set (Item Management's sale): the count +15; at 0 the id is cleared only  */ \
  /* if another entry holds the treasure (else kept: one found) - the list +23, +43 and +69, its entries +57.    */ \
  SIG(kSigTreasureCount,                                                                                            \
      "8B 44 24 08 85 C0 56 8B 74 24 08 66 89 84 B1 ?? ?? ?? ?? 75 ?? 8D 91 ?? ?? ?? ?? 57 8D 64 24 00 3B C6 74 ?? " \
      "66 8B 3A 66 3B BC B1 ?? ?? ?? ?? 74 ?? 83 C0 01 83 C2 04 83 F8 ?? 7C ?? 5F 5E C2 08 00 66 C7 84 B1 ?? ?? ?? " \
      "?? 00 00",                                                                                                    \
      1)                                                                                                             \
  /* Item Management's lists of each pane made again: the panes +10, a pane's list +21 (and +42), the box's list */ \
  /* rebuilt +27 and the treasures' +35, the step to the next pane +45.                                         */ \
  SIG(kSigBoxLists,                                                                                                 \
      "53 56 57 8B F9 83 C8 FF 8D B7 ?? ?? ?? ?? BB 02 00 00 00 3B 46 ?? 74 ?? 56 8B CF E8 ?? ?? ?? ?? 56 8B CF E8 " \
      "?? ?? ?? ?? 8B 46 ?? 81 C6 ?? ?? ?? ?? 83 EB 01 75 ??",                                                        \
      1)                                                                                                             \
  /* The play flow's readiness (aGame's vtable slot 5), past its steps before a chapter loads - Item             */ \
  /* Management's the one called +2: a quit one of them asked for (+18) takes the profile back from its copy     */ \
  /* (called +31) and ends the game (its step +38, again +45, made +46).                                         */ \
  SIG(kSigFlowQuit,                                                                                                 \
      "8B CE E8 ?? ?? ?? ?? EB ?? 8B CE E8 ?? ?? ?? ?? 80 BE ?? ?? ?? ?? 00 74 ?? 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? "   \
      "?? C7 46 ?? 00 00 00 00 C6 46 ?? ?? B0 01",                                                                  \
      1)                                                                                                            \
  /* ... that step making Item Management by the screen's class record (+1) into a field of the flow's (+14,     */ \
  /* again +30), and ...                                                                                         */ \
  SIG(kSigFlowItemBox,                                                                                              \
      "B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 3B C3 89 46 ?? 0F 84 ?? ?? ?? ?? 81 48 04 00 20 00 00 8B 4E ?? 51 8B 0D ?? "   \
      "?? ?? ?? 6A 1A",                                                                                             \
      1)                                                                                                            \
  /* ... and, once it is done, copying its quit byte (+2) into the flow's quit (+5) as it lets it go (the field  */ \
  /* +11, +25, +35).                                                                                             */ \
  SIG(kSigFlowItemBoxDone,                                                                                          \
      "8A 57 ?? 88 96 ?? ?? ?? ?? 8B 46 ?? 3B C3 74 ?? 81 60 04 FF DF FF FF 8B 4E ?? 8B 01 8B 50 30 FF D2 89 5E "   \
      "??",                                                                                                         \
      1)                                                                                                            \
  /* Item Management's own quit, at the end of a co-op session there: the panes' cases saved (called +2), then   */ \
  /* the quit byte +9, the message shown +13, a partner's joining and leaving flags +19 and +26, its frame's     */ \
  /* step +36 made its fade out (+37), the fade's timer +42.                                                     */ \
  SIG(kSigBoxQuit,                                                                                                  \
      "8B CE E8 ?? ?? ?? ?? C6 46 ?? 01 89 BE ?? ?? ?? ?? C6 86 ?? ?? ?? ?? 00 C6 86 ?? ?? ?? ?? 00 0F 57 C0 C6 "   \
      "46 ?? ?? F3 0F 11 86 ?? ?? ?? ?? 5F 5E 5D C3",                                                               \
      1)                                                                                                            \
  /* Its close once both panes are done (its frame's main step): the same save (+2), the step on by one (+17),   */ \
  /* "ok" +21, not the quit +25, the timer +31; the fade out's step (+37: the timer +62, +81; the step on, +89), */ \
  /* then the end's (+93: done +95).                                                                             */ \
  SIG(kSigBoxClose,                                                                                                 \
      "8B CD E8 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? 80 45 ?? 01 C6 45 ?? 01 C6 45 ?? 00 F3 0F 11 85 ?? ?? ?? ?? "   \
      "EB ?? 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0 75 ?? 8B 15 ?? ?? ?? ?? F3 0F 10 85 ?? ?? ?? ?? F3 0F 5C 42 "   \
      "28 0F 57 C9 0F 2F C8 F3 0F 11 85 ?? ?? ?? ?? 72 ?? 80 45 ?? 01 EB ?? C6 45 ?? 01",                           \
      1)                                                                                                            \
  /* Item Management's working copy of a character's case entry ((character, entry, the partner's)): sInfoMan's  */ \
  /* profile index +7, cGameInfo's size +10, the save data +22 (from sInfoMan), an entry's bytes +44 and +67,    */ \
  /* the partner's copy +48 and its own +71 (24 entries a character).                                            */ \
  SIG(kSigBoxCases,                                                                                                 \
      "A1 ?? ?? ?? ?? 8B 48 ?? 69 C9 ?? ?? ?? ?? 83 7C 24 0C 00 8D 8C 01 ?? ?? ?? ?? 8B 44 24 04 74 ?? 8D 14 40 "   \
      "8B 44 24 08 8D 14 D0 6B D2 ?? 8D 84 0A ?? ?? ?? ?? C2 0C 00 8B 54 24 08 8D 04 40 8D 04 C2 6B C0 ?? 8D 84 "   \
      "08 ?? ?? ?? ?? C2 0C 00",                                                                                    \
      1)                                                                                                            \
  /* cGameInfo's commit (its fields over their checkpoint copies), its last: the save data's working block       */ \
  /* into its copy (+21) by the save data's own copy (+28, called +32) - what play takes back ...                */ \
  SIG(kSigCasesCommit,                                                                                              \
      "F3 A5 50 8D 4B 04 66 0F D6 83 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 8B ?? ?? ?? ?? 51 8D 8B ?? ?? ?? ?? "            \
      "E8 ?? ?? ?? ?? 5F 5E 5B C3",                                                                                 \
      1)                                                                                                            \
  /* ... in cGameInfo's take-back (the play flow's quit, the story menu's start): the working block back from    */ \
  /* that copy (+4) by the save data's (+10, called +17).                                                        */ \
  SIG(kSigCasesRestore, "FF D2 8D 83 ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? 50 8B CE E8 ?? ?? ?? ??", 1)                     \
  /* The start-up menu's frame (cDemoStartUp's, aDemo's): its step +2, the last +10, the steps' table +16 (a     */ \
  /* stub calling each).                                                                                         */ \
  SIG(kSigMenuStep,                                                                                                 \
      "8B 47 ?? 25 FF 00 00 00 83 F8 ?? 77 ?? FF 24 85 ?? ?? ?? ?? 8B CF E8 ?? ?? ?? ?? EB ?? 8B CF E8 ?? ?? ?? "   \
      "?? EB",                                                                                                      \
      1)                                                                                                            \
  /* ... its step taking the return reason the game left: cGameSys in sBioMain +8, the reason +14 (from 1, the   */ \
  /* last less 1 +20), unknown ones on as the normal one (ja +22), the reasons' table +26 ...                    */ \
  SIG(kSigMenuReason,                                                                                               \
      "8B 0D ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? 8B 48 ?? 83 C1 FF 83 F9 ?? 77 ?? FF 24 8D ?? ?? ?? ??",                  \
      1)                                                                                                            \
  /* ... the reason back to the choice made: cleared (+4); in an online game (sBioMain's flags +12, bit 0) or    */ \
  /* not, its step's next (+19, made +20), the story flow's step (+23, made +24, the choice) and CHAPTER         */ \
  /* SELECT's (+29 and +35, made +30 and +36); the menus' music (called +67), then the top menu's choice (jmp    */ \
  /* +72) ...                                                                                                    */ \
  SIG(kSigMenuReasonBack,                                                                                           \
      "33 DB 89 58 ?? A1 ?? ?? ?? ?? F6 80 ?? ?? ?? ?? 01 C6 46 ?? ?? C6 46 ?? ?? 74 ?? C6 46 ?? ?? EB ?? C6 46 "   \
      "?? ?? 53 88 5E ?? 8B 0D ?? ?? ?? ?? 6A 02 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? "   \
      "?? ?? E9",                                                                                                   \
      1)                                                                                                            \
  /* ... going on by the top menu's choice: cGameSys +8, the choice +14, the last +17, the table +27 ...         */ \
  SIG(kSigMenuTop,                                                                                                  \
      "8B 0D ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B 42 ?? 83 F8 ?? 0F 87 ?? ?? ?? ?? FF 24 85 ?? ?? ?? ??",               \
      1)                                                                                                            \
  /* ... and the story flow by the story menu's: online (the flags +7) its own few, else cGameSys +65, the       */ \
  /* choice +71 (+22 online), the last +74, the table +80; past the last it is parked (the story flow's step +44 */ \
  /* made the flow's own bound, ja +76).                                                                         */ \
  SIG(kSigMenuStory,                                                                                                \
      "A1 ?? ?? ?? ?? F6 80 ?? ?? ?? ?? 01 74 ?? 8B 80 ?? ?? ?? ?? 8B 40 ?? 83 E8 00 74 ?? 83 E8 01 74 ?? 83 E8 "   \
      "01 0F 84 ?? ?? ?? ?? 88 4E ?? 5E C3 8B CE 5E E9 ?? ?? ?? ?? 8B CE 5E E9 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 8B "   \
      "42 ?? 83 F8 ?? 77 ?? FF 24 85 ?? ?? ?? ??",                                                                  \
      1)                                                                                                            \
  /* The menus' own area (aDemo): its frame's step +2 (the last +10, the table +16) - the first step's           */ \
  /* handler (+20) makes it the next (+23), whose handler (+26) has sLoadMan (+28, again +45) choose the         */ \
  /* front end's archives (called +32) and makes it the next (+40), whose handler (+43) waits for the            */ \
  /* loader before the start-up menu is made ...                                                                 */ \
  SIG(kSigFrontLoad,                                                                                                \
      "8B 46 ?? 25 FF 00 00 00 83 F8 ?? 77 ?? FF 24 85 ?? ?? ?? ?? C6 46 ?? ?? 5E C3 8B 0D ?? ?? ?? ?? E8 ?? ?? "   \
      "?? ?? C6 46 ?? ?? 5E C3 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0 75",                                          \
      1)                                                                                                            \
  /* ... that choice's menus: by a bit (+6) of sLoadMan's field (+2) - the story menu's choices set it - their   */ \
  /* archive asked for (called +72, as +35): the trial's (+60) when sBioMain's flags (+47) have bit 0 (+51),     */ \
  /* else the menus' own (+67: the path +68).                                                                    */ \
  SIG(kSigMenusArchive,                                                                                             \
      "F6 86 ?? ?? ?? ?? ?? 74 ?? 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0 74 ?? 6A 00 6A 00 68 ?? ?? ?? ?? 8B CE "   \
      "E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? F6 80 ?? ?? ?? ?? ?? 6A 00 8B CE 6A 00 74 ?? 68 ?? ?? ?? ?? EB ?? 68 ?? ?? "   \
      "?? ?? E8",                                                                                                   \
      1)                                                                                                            \
  /* The start-up menu's background (uIdTopMenu00) told a top menu choice was made (thiscall (bits)): its        */ \
  /* main menu's list (+5) by the list's setter (called +15: bit 1 picked, bit 2 go on); while cGameSys          */ \
  /* (sBioMain's +28, the global +22) has the top menu's choice (+34) the story's (+35), its header (+40)        */ \
  /* shown by the header's setter (jmp +54) with +50 ...                                                         */ \
  SIG(kSigMenuChosen,                                                                                               \
      "56 8B F1 8B 4E ?? 85 C9 74 ?? 8B 44 24 08 50 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 83 7A ?? "   \
      "?? 75 ?? 8B 4E ?? 85 C9 74 ?? 5E C7 44 24 04 ?? ?? ?? ?? E9 ?? ?? ?? ?? 5E C2 04 00",                        \
      1)                                                                                                            \
  /* ... its change of screens (thiscall (from, to)): the table +12 (from, to, the panel's segment: 3 bytes,     */ \
  /* +41 rows); found, its segment asked for (+58 set), its number (+66), played forward (+69 cleared) ...       */ \
  SIG(kSigMenuTransition,                                                                                           \
      "53 56 8B 74 24 10 57 8B 7C 24 10 B8 ?? ?? ?? ?? 33 D2 0F BE 18 3B FB 75 ?? 0F BE 58 01 3B F3 74 ?? 83 C2 "   \
      "01 83 C0 03 83 FA ?? 7C ?? 5F 5E 5B C2 08 00 80 78 02 00 7C ?? C6 41 ?? 01 0F BE 40 02 89 41 ?? C6 41 ?? "   \
      "00 56 57 E8",                                                                                                \
      1)                                                                                                            \
  /* ... and the story menu's close on to its choice's screen: its own screen +13 (offline: sBioMain's flags     */ \
  /* +8, bit 0), the choice's from the table +39 by the choice (+33), the change of screens called +81.          */ \
  SIG(kSigStoryClose,                                                                                               \
      "8B 15 ?? ?? ?? ?? 8B 92 ?? ?? ?? ?? BF ?? ?? ?? ?? 23 D7 74 ?? BF ?? ?? ?? ?? 38 5E ?? 74 ?? 8B 46 ?? 3B "   \
      "C3 0F BE 88 ?? ?? ?? ?? 75 ?? 39 9E ?? ?? ?? ?? 74 ?? B9 ?? ?? ?? ?? 3B D3 74 ?? 83 E8 01 F7 D8 1B C0 83 "   \
      "E0 ?? 83 C0 ?? 8B C8 51 57 8B CE E8",                                                                        \
      1)                                                                                                            \
  /* The play flow's (aGame's) copy of a cGameInfo as at its last checkpoint, made at its first and copied over  */ \
  /* there at each (the area's start, a checkpoint, the pause's save): its field +2 (again +39, +54), made of    */ \
  /* cGameInfo's size (+10, again +60; constructed, called +28) and copied from the profile's cGameInfo          */ \
  /* (sInfoMan +45: the profile index +51, cGameInfo[] +68) by its copy (called +72) - the chapter's fields,     */ \
  /* then the save data's blocks. RESTART and a death's CONTINUE copy it back over the profile's.                */ \
  SIG(kSigFlowSnapshot,                                                                                             \
      "83 BF ?? ?? ?? ?? 00 75 ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 04 85 C0 74 ?? 8B C8 E8 ?? ?? ?? ?? EB ?? "   \
      "33 C0 89 87 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 8B 4E ?? 8B 87 ?? ?? ?? ?? 69 C9 ?? ?? ?? ?? 50 8D 8C 31 ?? ?? "   \
      "?? ?? E8",                                                                                                   \
      1)                                                                                                            \
  /* The play flow's save at a checkpoint (FUN_00723660), after that copy: it commits the profile's data over    */ \
  /* their checkpoint copies only outside the extra episodes, asking cGameSys (sBioMain +15) the game's own      */ \
  /* test of them (called +19, FUN_00715AE0: its mode +2 is 3 Lost in Nightmares or 4 Desperate Escape).         */ \
  SIG(kSigEpisodeCommit,                                                                                            \
      "85 ED 0F 84 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0 75 ?? 33 FF BE ?? ?? ?? ??",   \
      1)                                                                                                            \
  /* The pause menu's run of its panes (one a local player, uIdPause00's): the walk from +2 (the pane itself +15 */ \
  /* back, s8), a pane's in-use byte its first, its done byte +34 (s8, from the walk) ...                        */ \
  SIG(kSigPausePanes,                                                                                               \
      "8D B7 ?? ?? ?? ?? 90 8B 0D ?? ?? ?? ?? 8D 6E ?? 89 81 ?? ?? ?? ?? 80 7D 00 00 0F 84 ?? ?? ?? ?? 80 7E ?? "   \
      "00 0F 85",                                                                                                   \
      1)                                                                                                            \
  /* ... while the menu's no-cancel byte (+2) is clear, a pane's cancel (its bit +8, s8, of the menu's +11) ends */ \
  /* the option it runs (+28, s8) ...                                                                            */ \
  SIG(kSigPauseCancel,                                                                                              \
      "80 7F ?? 00 75 ?? 8B 4E ?? 8A 87 ?? ?? ?? ?? BA 01 00 00 00 D3 E2 84 D0 74 ?? 39 5E ?? 74 ?? 8B 06",         \
      1)                                                                                                            \
  /* ... and each pane's option runs (+2, s8; the last +5, the table +15): 0 the menu itself (called +22), 1     */ \
  /* RESUME (the pane's close byte +34, s8), RESTART and QUIT their handler (called +44).                        */ \
  SIG(kSigPauseOption,                                                                                              \
      "8B 46 ?? 83 F8 ?? 0F 87 ?? ?? ?? ?? FF 24 85 ?? ?? ?? ?? 55 8B CF E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? C6 46 ?? "   \
      "01 E9 ?? ?? ?? ?? 55 8B CF E8",                                                                              \
      1)                                                                                                            \
  /* That handler's QUIT once asked: the step (+25, again +55) waiting for the save manager (+2, busy +8), the   */ \
  /* next running the save question (called +34; done: the step +56), that one quitting - RESTART (+82) and QUIT */ \
  /* (one more, +87) by the option (+79); offline (sBioMain +96: +102 bit 2 clear) the play flow (its getter     */ \
  /* called +115, +149) told to quit (called +156) ...                                                           */ \
  SIG(kSigPauseQuit,                                                                                                \
      "8B 0D ?? ?? ?? ?? 80 B9 ?? ?? ?? ?? 00 0F 85 ?? ?? ?? ?? 5B 04 01 5F 88 46 ?? 5E 5D C2 04 00 56 8B CD E8 "   \
      "?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 5B 5F C6 46 ?? 01 C6 46 ?? ?? 5E 5D C2 04 00 33 DB 38 1F 74 ?? 38 5F "   \
      "?? 0F 85 ?? ?? ?? ?? 8B 46 ?? 83 E8 ?? 74 ?? 83 E8 01 0F 85 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? F6 81 ?? ?? ?? "   \
      "?? 02 74 ?? 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? C6 46 01 01 EB ?? 68 ?? ?? ?? ?? E8 ?? "   \
      "?? ?? ?? 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B C8 E8",                                                         \
      1)                                                                                                            \
  /* ... and as RESTART or QUIT is answered yes: the menu's +2 set (its network events let go), the play flow's  */ \
  /* (its getter called +13, again +28) +35 set (no game over while it goes).                                    */ \
  SIG(kSigPauseLeaving,                                                                                             \
      "C6 85 ?? ?? ?? ?? 01 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 ?? 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? C6 80 "   \
      "?? ?? ?? ?? 01",                                                                                             \
      1)                                                                                                            \
  /* The pause's save question ("Overwrite current equipment and status?") answered: its timer (+10, again +26)  */ \
  /* run out, the answer (+31) yes - its next step (+40) - or no: unless RESTART (the option +57, +58) the       */ \
  /* profile's taken back from its checkpoint copies (called +82), the profile chosen around it.                 */ \
  SIG(kSigPauseNoSave,                                                                                              \
      "8B 0D ?? ?? ?? ?? F3 0F 10 46 ?? F3 0F 5C 41 28 0F 57 C9 0F 2F C8 F3 0F 11 46 ?? 72 ?? 83 7E ?? 00 75 ?? "   \
      "02 C3 5F 88 46 ?? 88 5E ?? C6 46 ?? 00 5E 33 C0 5B C2 04 00 83 7E ?? ?? 74 ?? 8B 56 ?? 8B 0D ?? ?? ?? ?? "   \
      "52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? E8",                                                                     \
      1)                                                                                                            \
  /* The Game Over screen (uIdGameover00's frame, offline): its opening's end - shown (+2) - then by cGameSys    */ \
  /* (sBioMain +7: +13, its +19 and +25 differing for a network partner) its menu (the step +33 one on) or the   */ \
  /* step waiting for the choice made elsewhere (+41 made +42), the choice (+45) none ...                        */ \
  SIG(kSigGameOverChoice,                                                                                           \
      "88 9E ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? 3B 88 ?? ?? ?? ?? 75 ?? 00 5E ?? E9 "   \
      "?? ?? ?? ?? C6 46 ?? ?? C7 86 ?? ?? ?? ?? FF FF FF FF",                                                      \
      1)                                                                                                            \
  /* ... that step: the choice from sBioMain's flags (+2: +8) - CONTINUE's bit (+13) 0, QUIT's (+32) 1 (the      */ \
  /* choice +21, +40, +46) - then decided (+83), its timer (+91) made the delay (+74), the step (+80) one on.    */ \
  SIG(kSigGameOverDecided,                                                                                          \
      "8B 0D ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? A9 ?? ?? ?? ?? 74 ?? C7 86 ?? ?? ?? ?? 00 00 00 00 EB ?? A9 ?? ?? ?? "   \
      "?? 74 ?? 89 9E ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 85 C0 7C ?? 8B CE 75 ?? E8 ?? ?? ?? ?? EB ?? E8 ?? ?? ?? ?? "   \
      "F3 0F 10 05 ?? ?? ?? ?? 00 5E ?? 88 9E ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ??",                                 \
      1)                                                                                                            \
  /* The chapter's results (uIdChapEd00's frame), its menu: with the quit offered (+2 clear: not the last        */ \
  /* chapter, not the Mercenaries, not online) a quit pressed asks first - the prompts (+56) made 3 (+60), +72   */ \
  /* made 0x13, the wait (+82) -1, the step (+92) its question's (+93) ...                                       */ \
  SIG(kSigResultsAsk,                                                                                               \
      "38 9E ?? ?? ?? ?? 75 ?? F6 44 24 ?? 01 75 ?? 8B 0D ?? ?? ?? ?? 6A FF E8 ?? ?? ?? ?? 84 C0 75 ?? 8B 0D ?? "   \
      "?? ?? ?? 53 53 53 E8 ?? ?? ?? ?? 84 C0 74 ?? 6A ?? 8B CE C7 86 ?? ?? ?? ?? 03 00 00 00 E8 ?? ?? ?? ?? 53 "   \
      "C7 86 ?? ?? ?? ?? ?? 00 00 00 C7 86 ?? ?? ?? ?? FF FF FF FF C6 46 ?? ??",                                    \
      1)                                                                                                            \
  /* ... the question answered: no - +8 cleared, the step (+14) back to the menu's (+15) - or yes: the step      */ \
  /* (+54) one on, the quit (+58) set, the wait (+65) 0, its timer (+73) the delay (+48), then the session let   */ \
  /* go and the save manager waited for, the screen done.                                                        */ \
  SIG(kSigResultsQuit,                                                                                              \
      "E8 ?? ?? ?? ?? 53 89 9E ?? ?? ?? ?? C6 46 ?? ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? E9 ?? ?? "   \
      "?? ?? 8B CE E8 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? 80 46 ?? 01 C6 86 ?? ?? ?? ?? 01 89 9E ?? ?? ?? ?? F3 "   \
      "0F 11 86 ?? ?? ?? ??",                                                                                       \
      1)                                                                                                            \
  /* The play flow's chapter end once its results (+2, the flow's field) are done: their quit (+8) set, the      */ \
  /* flow's step (+17, again +24) made the quit after a game's end (+25, as the Game Over's QUIT), else on to    */ \
  /* the next chapter.                                                                                           */ \
  SIG(kSigResultsDone,                                                                                              \
      "8B 86 ?? ?? ?? ?? 80 B8 ?? ?? ?? ?? 00 74 ?? C7 46 ?? 00 00 00 00 C6 46 ?? ?? EB ?? 80 46 ?? 01 85 C0 74",   \
      1)                                                                                                            \
  /* sLoadMan's step through a list of the archives it was asked for (list k = eax * 5 * 4 bytes on): the list's */ \
  /* count +9 and its items +23 (an MtArray: count +4, items +0x10); an entry's state +32, its path +58.         */ \
  SIG(kSigLoadList,                                                                                                 \
      "8D 04 80 57 33 ED 39 AC 81 ?? ?? ?? ?? 8D 3C 81 B2 01 7E ?? 56 8B 8F ?? ?? ?? ?? 8B 34 A9 8B 46 ?? 83 E8 01 " \
      "74 ?? 83 E8 01 74 ?? 83 E8 02 75 ?? 8B 0D ?? ?? ?? ?? 6A 02 8D 56 ??",                                        \
      1)                                                                                                             \
  /* ... an entry's archive (+2, again +30) and the archive's flags (+9, again +55): bit 4 failed (asked for     */ \
  /* again), bit 0 loaded (the entry's state +61 made 3); the entry's path again +26.                            */ \
  SIG(kSigLoadReady,                                                                                                \
      "8B 4E ?? 85 C9 74 ?? 8B 41 ?? C1 E8 04 A8 01 74 ?? E8 ?? ?? ?? ?? 6A 02 8D 4E ?? 51 C7 46 ?? 00 00 00 00 8B " \
      "0D ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? EB ?? F6 41 ?? 01 74 ?? C7 46 ?? 03 00 00 00",                    \
      1)                                                                                                             \
  /* sLoadMan's choice of the list it adds to: that field +6 (right after the lists), the first count +18.       */ \
  SIG(kSigLoadCurrent, "8B 54 24 04 89 91 ?? ?? ?? ?? 33 C0 8D 14 92 39 84 91 ?? ?? ?? ??", 1)                      \
  /* sLoadMan's request of a class's archive (thiscall (kind, number, variant, 0), ret 10h - and of what the     */ \
  /* class needs with it): its start, the class's key made (kind << 16 | number) << 8 | variant ...             */ \
  SIG(kSigLoadRequest,                                                                                              \
      "81 EC 08 01 00 00 8B 84 24 ?? ?? ?? ?? 0F B6 94 24 ?? ?? ?? ?? 53 8B 9C 24 ?? ?? ?? ?? 55 8B E9 C1 E0 10 0F " \
      "B7 CB 0B C1 C1 E0 08 0B C2 50 E8",                                                                            \
      1)                                                                                                             \
  /* ... and the archive's path made from the class's name (the format pushed +10).                             */ \
  SIG(kSigLoadPath, "8B 46 04 57 50 8D 4C 24 ?? 68 ?? ?? ?? ?? 51 E8", 1)                                          \
  /* sLoadMan's commit of the list it adds to (thiscall, no arguments): what was not asked for again let go,     */ \
  /* the list's loading started for what is new - its start: the list field +8, the first count +26.            */ \
  SIG(kSigLoadCommit,                                                                                               \
      "51 53 55 57 8B F9 8B 87 ?? ?? ?? ?? 8D 04 80 33 ED 33 C9 8D 04 87 33 DB 39 A8 ?? ?? ?? ?? 89 4C 24 0C 0F 8E", \
      1)                                                                                                             \
  /* The play flow's test whether a cutscene is preloaded (sLoadMan, fastcall): any entry of the cutscenes' list */ \
  /* (its count +2, its items +14) in the state +23 loaded (3) - the event step then plays it without loading    */ \
  /* it (FUN_0071E600), and the play step lets the whole list go after (FUN_007D4BA0).                           */ \
  SIG(kSigCutscenePreloaded,                                                                                         \
      "8B 91 ?? ?? ?? ?? 33 C0 85 D2 7E ?? 8B 89 ?? ?? ?? ?? 57 8B 39 83 7F ?? 03 75 03 83 C0 01 83 C1 04 83 "       \
      "EA 01 75 ?? 5F C3",                                                                                           \
      1)                                                                                                             \
  /* The area's sets loaded (FUN_0072CF40, at each stage's load and after a cutscene changing them): the list    */ \
  /* +10 chosen (the choice called +11), its requests, then the commit, which lets go of what was not asked for. */ \
  SIG(kSigSetsLoad,                                                                                                  \
      "53 8B D9 8B 0D ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 48 ?? 69 C9 40 59 00 00 80 7C 24 08 "       \
      "00",                                                                                                          \
      1)                                                                                                             \
  /* cItem's model, made for an item put in a case with no model handed over: the item's id (+2, the cItem's)   */ \
  /* less the first of a switch (minus +5) over +15 + 1 ids; its byte table +21 into the jump table +28; past it */ \
  /* (ja, rel8 +17) and the table's default case the id keeps its own model ...                                  */ \
  SIG(kSigModelSwitch,                                                                                              \
      "8B 45 04 53 05 ?? ?? ?? ?? 33 FF 32 DB 83 F8 ?? 77 ?? 0F B6 88 ?? ?? ?? ?? FF 24 8D ?? ?? ?? ??", 1)          \
  /* ... the others are given another's (+5) - the grenade launcher's rounds - then the model's class is made    */ \
  /* from the item's id and the kind pushed at +24 (an MT class record keyed (kind << 16 | id) << 8).            */ \
  SIG(kSigModelMake, "B3 01 C7 45 04 ?? ?? ?? ?? 8B 45 04 8B 0D ?? ?? ?? ?? 6A 00 6A 00 50 6A ?? E8", 1)          \
  /* The game's own save of a case (FUN_00C932D0), its grenade launcher: an item of the launcher's id (+2: the   */ \
  /* one the switch gives the loads' model) with a model (cItem +10) of the launcher's class (its record pushed  */ \
  /* +16) is written as its load - the rounds the model keeps (+31), less the first (minus +36), over +42 + 1:   */ \
  /* the byte table +48 into the jump table +55, each load's case `mov ebx, <load>`; past it (ja, rel8 +44) the  */ \
  /* id is kept.                                                                                                 */ \
  SIG(kSigSaveLauncher,                                                                                             \
      "81 FB ?? ?? ?? ?? 75 ?? 8B 48 ?? 85 C9 74 ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 ?? 8B 80 ?? ?? ?? ?? 05 " \
      "?? ?? ?? ?? 83 F8 ?? 77 ?? 0F B6 80 ?? ?? ?? ?? FF 24 85 ?? ?? ?? ??",                                       \
      1)                                                                                                            \
  /* The play flow's chapter start (FUN_00719A40, as play begins): while the profile's mTempBase.mChapterStart   */ \
  /* is set (sInfoMan: the profile +10, cGameInfo's size +13, the field +20 - set as a chapter is set to begin   */ \
  /* from its beginning), the chapter's title for each player (sBioMain's flags +39, bit +43 none; cGameSys in   */ \
  /* sBioMain +53, its mode +62: 1 four players) - its class record moved in at +147 (mov ecx, then the maker   */ \
  /* called +151) - then the field cleared.                                                                      */ \
  SIG(kSigChapterStart,                                                                                             \
      "A1 ?? ?? ?? ?? 56 8B F1 8B 48 ?? 69 C9 ?? ?? ?? ?? 83 BC 01 ?? ?? ?? ?? 00 0F 84 ?? ?? ?? ?? 8B 15 ?? ?? ?? "  \
      "?? F6 82 ?? ?? ?? ?? ?? 57 0F 85 ?? ?? ?? ?? 8B 8A ?? ?? ?? ?? 53 33 DB 83 79 ?? 01 0F 94 C3",                 \
      1)                                                                                                            \
  /* A character's map (uIdMap00), as its setup ends: while the map key toggles it (+2), its "open" flag (+33)  */ \
  /* taken from the play state's byte of its player (+10): cGameSys (sBioMain +21) asked through its getter     */ \
  /* (called +26: (player) -> open, ret 4).                                                                      */ \
  SIG(kSigMapRestore,                                                                                               \
      "80 7E ?? 00 5F 5D 74 ?? 8B 96 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 88 46 ?? 5B B0 "  \
      "01 5E 83 C4 18 C3",                                                                                          \
      1)                                                                                                            \
  /* ... and its frame's toggle: in the toggle mode (+2), the map key pressed (the pad's +11, bit 0x200) shuts a */ \
  /* map the scenario forced open (+20, +25) or - unless the scenario keeps it shut (+33) - flips "open" (+28,   */ \
  /* +38, +44), then keeps it (+48) in the play state's byte of its player (+51): cGameSys (sBioMain +63) set    */ \
  /* through its setter (called +68: (player, open), ret 8).                                                     */ \
  SIG(kSigMapToggle,                                                                                                \
      "38 5E ?? 74 ?? 3B C3 74 ?? F7 40 ?? 00 02 00 00 74 ?? 38 5E ?? 74 ?? 88 5E ?? 88 5E ?? EB ?? 38 5E ?? 75 ?? "  \
      "38 5E ?? 0F 94 C0 88 46 ?? 0F B6 4E ?? 8B 96 ?? ?? ?? ?? A1 ?? ?? ?? ?? 51 8B 88 ?? ?? ?? ?? 52 E8",           \
      1)                                                                                                            \
  /* A character's start making its map (the maker called +0): the map's player (+14) its player's number (+8, */ \
  /* a byte), the map's character (+20) the character, and the character's map (+50).                         */ \
  SIG(kSigMapMake,                                                                                                  \
      "E8 ?? ?? ?? ?? 0F B6 96 ?? ?? ?? ?? 89 90 ?? ?? ?? ?? 89 B0 ?? ?? ?? ?? 80 BE ?? ?? ?? ?? 01 0F 94 C1 88 88 "  \
      "?? ?? ?? ?? 8B 8E ?? ?? ?? ?? 85 C9 89 86 ?? ?? ?? ??",                                                       \
      1)                                                                                                            \
  /* The Mercenaries' combo clock (FUN_0071C350, each frame for each player, `(slot, frame time)`): cGameSys in  */ \
  /* sBioMain +7; a score slot's block 1 << +21 apart (a byte of the `shl`); while the slot's combo runs (+25,   */ \
  /* a byte) its clock (+37, a float) less the frame time (the `subss` +41, 6 bytes); at 0 - where this machine  */ \
  /* runs the match (cGameSys +57 == +63) - the combo's end (called +79).                                        */ \
  SIG(kSigMercCombo,                                                                                                \
      "A1 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 56 57 8B 7C 24 0C 8B F7 C1 E6 06 80 BC 06 ?? ?? ?? ?? 00 74 ?? F3 0F 10 84 "  \
      "06 ?? ?? ?? ?? F3 0F 5C 44 24 10 0F 57 C9 0F 2F C8 72 ?? 8B 90 ?? ?? ?? ?? 3B 90 ?? ?? ?? ?? 0F 28 C1 F3 0F " \
      "11 44 24 0C 75 ?? 57 E8",                                                                                    \
      1)                                                                                                            \
  /* ... the combo's end (FUN_00717BD0, `(slot)`): cGameSys +12; the combo (+24, +66) kept as the best (+30,     */ \
  /* +45) when above it, then 0; what it earned (+85) onto the score (+92).                                      */ \
  SIG(kSigMercComboEnd,                                                                                             \
      "8B 15 ?? ?? ?? ?? 8B 44 24 04 8B 8A ?? ?? ?? ?? C1 E0 06 03 C8 56 8B B1 ?? ?? ?? ?? 3B B1 ?? ?? ?? ?? 7E ?? "  \
      "8B 92 ?? ?? ?? ?? 89 B4 10 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A ?? ?? ?? ?? 33 D2 89 94 01 ?? ?? ?? ?? 8B 0D " \
      "?? ?? ?? ?? 8B 89 ?? ?? ?? ?? 8B B4 01 ?? ?? ?? ?? 01 B4 01 ?? ?? ?? ??",                                      \
      1)                                                                                                            \
  /* ... the Combo Bonus' clock (FUN_00717C60, `(slot, frame time)`): cGameSys +11; while it runs (+24, +32, a   */ \
  /* byte) its clock (+43, a float) less the frame time (the `subss` +47, 6 bytes); at 0 it stops.              */ \
  SIG(kSigMercBonusClock,                                                                                           \
      "8B 54 24 04 A1 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 56 8B F2 C1 E6 06 80 BC 06 ?? ?? ?? ?? 00 8D 8C 06 ?? ?? ?? ?? "  \
      "74 ?? F3 0F 10 84 06 ?? ?? ?? ?? F3 0F 5C 44 24 0C 0F 57 C9 0F 2F C8 72",                                     \
      1)                                                                                                            \
  /* ... and its start, a Combo Bonus picked up (FUN_00718100, `(slot)`): cGameSys +8 (+35, +64); it runs (+22) */ \
  /* for the seconds at the address +43 (+52, its clock) with no kills in it yet (+71).                          */ \
  SIG(kSigMercBonusStart,                                                                                           \
      "8B 0D ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B 44 24 04 C1 E0 06 C6 84 02 ?? ?? ?? ?? 01 8B 0D ?? ?? ?? ?? 8B 91 ?? "  \
      "?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 84 02 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? C7 84 02 ?? " \
      "?? ?? ?? 00 00 00 00 C2 04 00",                                                                              \
      1)                                                                                                            \
  /* The Mercenaries' clock (FUN_00717850) after its count down: offline (cGameSys +2, a byte, 0) the HUD's copy */ \
  /* (+13); in modes 2 and 5 (+21, a byte) the stage cleared (the play flow's +64 set) once its kills (+56) reach */ \
  /* +50 - or +44 more with the flag +34 (a byte).                                                               */ \
  SIG(kSigMercClear,                                                                                                \
      "80 B9 ?? ?? ?? ?? 00 75 08 F3 0F 11 81 ?? ?? ?? ?? 8B 00 8B 48 ?? 83 F9 02 74 05 83 F9 05 75 ?? 8A 90 ?? ?? "  \
      "?? ?? F6 DA 1B D2 81 E2 ?? ?? ?? ?? 81 C2 ?? ?? ?? ?? 39 90 ?? ?? ?? ?? 7C ?? C7 87 ?? ?? ?? ?? 01 00 00 00",  \
      1)                                                                                                            \
  /* The play flow's Mercenaries frame (FUN_007245D0): cGameSys +21, its step (+27: 0 the count-in, then 1).    */ \
  SIG(kSigMercFrame,                                                                                                \
      "51 A1 ?? ?? ?? ?? F3 0F 10 40 ?? A1 ?? ?? ?? ?? 57 8B F9 8B 88 ?? ?? ?? ?? 83 B9 ?? ?? ?? ?? 00 F3 0F 11 44 " \
      "24 04 75",                                                                                                   \
      1)                                                                                                            \
  /* An enemy's hit made again from the other machine's message (FUN_00B550F0, `(record)` on the enemy): its   */ \
  /* cHit +13; into the record the place hit (+48 .. +67, three floats), the damage (+76) and 0 (+79); the attack */ \
  /* data's +84 into +87, the message's float (1.0 without data: the address +121) into +96, its +99, +105, +111 */ \
  /* into +102, +108, +114; hit (+146) = 1, the part (+156), 0 (+159), 0.0 (+167), the attack's number (+194),  */ \
  /* the attacker (+206), the attacker's class id (+215); the part's centre - or the enemy's place (+261, +267, */ \
  /* +273) - into +237, +243, +249.                                                                               */ \
  SIG(kSigHitRebuild,                                                                                               \
      "0F B6 97 ?? ?? ?? ?? 83 C4 04 52 8D 8F ?? ?? ?? ?? 8B D8 E8 ?? ?? ?? ?? 8B 75 08 33 D2 3B F2 0F 84 ?? ?? ?? "  \
      "?? 3B DA D9 87 ?? ?? ?? ?? 0F 57 C0 D9 1E D9 87 ?? ?? ?? ?? D9 5E ?? D9 87 ?? ?? ?? ?? D9 5E ?? 8B 8F ?? ?? " \
      "?? ?? 89 4E ?? 89 56 ?? 74 ?? D9 43 ?? D9 5E ?? D9 87 ?? ?? ?? ?? D9 5E ?? 8B 4B ?? 89 4E ?? 8B 4B ?? 89 4E " \
      "?? 8B 4B ?? 89 4E ?? EB ?? F3 0F 10 0D ?? ?? ?? ?? F3 0F 11 46 ?? F3 0F 11 4E ?? 89 56 ?? 89 56 ?? 89 56 ?? " \
      "C6 46 ?? 01 8A 8F ?? ?? ?? ?? 88 4E ?? 88 96 ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? 0F B6 8F ?? ?? ?? ?? 0F B7 " \
      "9F ?? ?? ?? ?? C1 E1 10 0B CB 3B C2 89 8E ?? ?? ?? ?? 8B 8F ?? ?? ?? ?? 89 4E ?? 8B 8F ?? ?? ?? ?? 89 8E ?? " \
      "?? ?? ?? 74 ?? 8D 54 24 10 52 8B C8 E8 ?? ?? ?? ?? D9 00 D9 5E ?? D9 40 04 D9 5E ?? D9 40 08 D9 5E ?? 5F 5E " \
      "5B 8B E5 5D C2 04 00 D9 47 ?? D9 5E ?? D9 47 ?? D9 5E ?? D9 47 ?? D9 5E ??",                                   \
      1)                                                                                                            \
  /* An enemy's damage check (FUN_00963C90) over its parts (+2: units whose hits reach it - the Reaper's soft    */ \
  /* body and the like; a pointer each, the loop's bound at +135), each part's hit record (+42) and hit (+52).   */ \
  SIG(kSigEnemyParts,                                                                                               \
      "8D 8E ?? ?? ?? ?? C7 44 24 10 00 00 00 00 89 4C 24 14 8B 54 24 14 8B 1A 85 DB 74 ?? 53 8B CE E8 ?? ?? ?? ?? " \
      "84 C0 74 ?? 8B BB ?? ?? ?? ?? 85 FF 74 06 80 7F ?? 00 75 04",                                                \
      1)                                                                                                            \
  /* A kill's weapon (FUN_007172A0, `(player, the hit's class id)`): the class id its own class record's (+21,   */ \
  /* the record's id; the class by its getDTI) is a hand-to-hand kill, the kind of the code +25 (0x100); a       */ \
  /* weapon's (0x07cccc00) the kind of its item cccc.                                                              */ \
  SIG(kSigMeleeKind,                                                                                                \
      "8B 4C 24 04 85 C9 74 ?? 8B 01 8B 50 10 FF D2 8B 4C 24 08 39 48 ?? 75 ?? B9 00 01 00 00 51 8B 0D ?? ?? ?? ?? E8", \
      1)                                                                                                            \
  /* ... and a kill's time (FUN_007243A0): the kill's first player only (the record's kind +9: hand-to-hand 0, */ \
  /* the stun rod 0x18) gets seconds (+19) onto the clock (called +22).                                          */ \
  SIG(kSigMeleeTime, "83 7C 24 1C 00 75 ?? 8A 45 ?? 84 C0 74 04 3C 18 75 ?? 6A ?? 8B CE E8", 1)                    \
  /* Whether an enemy takes a hit this frame (FUN_009E0700): its hit record (a pointer, +18), hit (+28).         */ \
  SIG(kSigHitTaken, "55 8B 6C 24 08 56 85 ED 8B F1 8B C5 0F 44 C6 57 8B B8 ?? ?? ?? ?? 85 FF 74 06 80 7F ?? 00 75 08",  \
      1)                                                                                                            \
  /* The hit path (FUN_009DEA50): a hit whose flags (+4, a byte) have the bit +5 takes the enemy's whole health */ \
  /* (its most +14, a word) - the game's own instant kill.                                                        */ \
  SIG(kSigWholeHealth,                                                                                              \
      "8B F8 F7 45 ?? ?? ?? ?? ?? 74 07 0F BF BE ?? ?? ?? ?? 81 FF FF 7F 00 00 7E 07 BF FF 7F 00 00", 1)            \
  /* ... and its tail: a hit that took the last of the health kills only while the game lets the enemy die - the */ \
  /* network's owner (called +7), it may be hurt this frame (+18), it may die (+27: the Chainsaw Majini's frame  */ \
  /* sets that until it has got up again); then dead (+48) and set-health (its slot +38) 0.                      */ \
  SIG(kSigDeathGate,                                                                                                \
      "8B 0D ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? 84 C0 74 ?? 80 BE ?? ?? ?? ?? 00 75 ?? 80 BE ?? ?? ?? ?? 00 75 ?? 8B 06 " \
      "8B 90 ?? ?? ?? ?? 6A 00 8B CE C6 86 ?? ?? ?? ?? 01 FF D2",                                                   \
      1)                                                                                                            \
  /* The Chainsaw Majini's frame (uEm22, a virtual function of its own): until it has got up again (+73, a byte) */ \
  /* and while it may (its second life, +89) it may not die (+100, the death's test above); its cHit +119.       */ \
  SIG(kSigChainsawLife,                                                                                             \
      "55 8B EC 83 E4 F0 51 53 56 57 8B F9 E8 ?? ?? ?? ?? 8B 87 ?? ?? ?? ?? 83 E8 ?? F3 0F 10 05 ?? ?? ?? ?? 74 ?? " \
      "83 E8 ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? 8B CF F3 0F 11 47 ?? F3 0F 11 47 ?? F3 0F 11 47 ?? E8 ?? ?? ?? ?? 80 " \
      "BF ?? ?? ?? ?? 00 74 ?? C6 44 24 0C 00 EB ?? 80 BF ?? ?? ?? ?? 00 B0 01 74 ?? 88 87 ?? ?? ?? ?? 88 44 24 0C " \
      "8B 5C 24 0C 6A 00 6A FF 53 8D B7 ?? ?? ?? ??",                                                               \
      1)                                                                                                            \
  /* ... and its brain's damage reaction (uEmIntel22, a virtual function of its own): with its second life (+45) */ \
  /* and not got up yet (+55), its health (+65) below 2 or at most +77 lies it down (the action +87) - to get up */ \
  /* with that much.                                                                                             */ \
  SIG(kSigChainsawDown,                                                                                             \
      "51 53 55 8B E9 8B 95 ?? ?? ?? ?? 56 8B B2 ?? ?? ?? ?? 8B CE 83 C8 FF C1 E9 08 BB 01 00 00 00 84 CB 89 44 24 " \
      "0C 0F 85 ?? ?? ?? ?? 80 BA ?? ?? ?? ?? 00 57 74 ?? 80 BA ?? ?? ?? ?? 00 75 ?? 0F B7 BA ?? ?? ?? ?? 66 3B FB " \
      "7E ?? 66 3B BA ?? ?? ?? ?? 7F ?? 5F 5E 5D B8 ?? ?? ?? ??",                                                   \
      1)                                                                                                            \
  /* An attack's data by its number (FUN_008237D0): the groups (+26: a pointer and a count each, 8 bytes) by the */ \
  /* number's high word, 0x1C bytes an entry by its low word.                                                   */ \
  SIG(kSigAttackTable,                                                                                              \
      "8B 44 24 04 8B C8 25 FF FF 00 00 8D 14 C5 00 00 00 00 C1 E9 10 2B D0 8B 04 CD ?? ?? ?? ?? 8D 04 90 C3", 1)    \
  /* A gun's shot (uWeapon slot 111, FUN_00B97BA0): an item 0x1nn's entry of the guns' table (+46), 0x140 bytes  */ \
  /* each by the low byte ...                                                                                    */ \
  SIG(kSigShotTable,                                                                                                \
      "8B 45 08 8B 40 04 83 EC 34 53 8B D9 8B C8 81 E1 00 FF 00 00 81 F9 00 01 00 00 56 57 0F 85 ?? ?? ?? ?? 25 FF " \
      "00 00 00 8D 04 80 C1 E0 06 05 ?? ?? ?? ??",                                                                   \
      1)                                                                                                            \
  /* ... and its bullet's attack: the number at the entry's +17 (a word), set as the attack (called +21).        */ \
  SIG(kSigShotAttack,                                                                                               \
      "33 C0 89 86 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 0F B7 51 ?? 52 8B CE E8 ?? ?? ?? ?? 8B 45 08 0F B6 48 ?? 8B 50 04 " \
      "51 52 E8",                                                                                                   \
      1)                                                                                                            \
  /* A stage's ask for its area tasks (FUN_007604D0): sScenario's tasks of the type +7 (a byte) - each a box of  */ \
  /* the stage that starts one of its enemy sets once a player is in it - from its lists' getter (called +8).    */ \
  SIG(kSigAreaTasks, "8B 0D ?? ?? ?? ?? 6A 06 E8 ?? ?? ?? ?? 8B E8 85 ED 0F 84", 1)                                 \
  /* A walk over a type's tasks (FUN_00759770): the lists' getter (called +7), then a list's (called +23).      */ \
  SIG(kSigTaskLoop,                                                                                                 \
      "8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 85 C0 74 ?? 8B 54 3C 14 52 8B C8 E8 ?? ?? ?? ?? 85 C0 74 ??", 1)          \
  /* A task's "a player is in its area" (FUN_00CBFD50): its area (cAreaHit, +3); the players found in it (+30,  */ \
  /* +50) against its own (+35, +47).                                                                            */ \
  SIG(kSigAreaInside,                                                                                               \
      "56 8B 71 ?? 85 F6 75 04 32 C0 5E C3 8B 51 ?? 8B 42 ?? 83 E8 00 74 16 83 E8 01 75 EC 8B 4E ?? 8B C2 8B 40 ?? " \
      "23 C8 3B C8 0F 94 C0 5E C3 8B 52 ?? 85 56 ?? 5E 0F 95 C0 C3",                                               \
      1)                                                                                                            \
  /* An area's frame (cAreaHit, FUN_00BEAB70): the players found cleared (+24); while it is on (+20) its shape  */ \
  /* (+42) is tested for each player.                                                                            */ \
  SIG(kSigAreaHitFrame,                                                                                             \
      "55 8B EC 83 E4 F0 81 EC ?? ?? ?? ?? 53 56 8B F1 33 DB 38 5E ?? 57 89 5E ?? 0F 84 ?? ?? ?? ?? 39 5E ?? 0F 84 " \
      "?? ?? ?? ?? 8B 46 ?? 3B C3 0F 84",                                                                            \
      1)                                                                                                            \
  /* A box's test of a place (AHShapeBox slot 9, FUN_00BE6560): above its floor (+8) by no more than its height */ \
  /* (+27), and inside its four corners' quad - x of each +45, +64, +151, +74, z +50, +69, +160, +79.            */ \
  SIG(kSigBoxInside,                                                                                                \
      "8B 44 24 04 F3 0F 10 41 ?? F3 0F 10 48 04 0F 2F C1 0F 87 ?? ?? ?? ?? F3 0F 10 51 ?? F3 0F 58 D0 0F 2F CA 0F " \
      "87 ?? ?? ?? ?? F3 0F 10 61 ?? F3 0F 10 69 ?? F3 0F 10 10 F3 0F 10 58 08 F3 0F 10 41 ?? F3 0F 10 49 ?? F3 0F " \
      "10 71 ?? F3 0F 10 79 ?? F3 0F 5C D4 F3 0F 5C DD F3 0F 5C C4 F3 0F 5C CD F3 0F 5C F4 F3 0F 5C FD 0F 28 E0 0F " \
      "28 E9 F3 0F 59 E3 F3 0F 59 EA 0F 2F E5 77 ?? 0F 28 E6 0F 28 EF F3 0F 59 E3 F3 0F 59 EA 0F 2F EC 77 ?? F3 0F " \
      "10 69 ?? F3 0F 10 61 ?? F3 0F 5C E5 F3 0F 10 69 ??",                                                          \
      1)
