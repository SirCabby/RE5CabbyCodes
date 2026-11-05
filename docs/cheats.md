# RE5CabbyCodes - the cheats

Part of the project's notes: `CLAUDE.md` is the guide and lists every section of `docs/`. The addresses are
build 11465250's. A section named in quotes ("The cranks") may be in another of these files:
`grep -n '^## ' docs/*.md`.

## The cheats (`cheats.cpp`)

- **God mode / Partner takes no damage**: the player damage slot in every player vtable re-pointed at
  `hk_player_damage`, which returns `false` (no damage, not dead) for a character driven by this machine's pad
  (God mode) or by the AI (Partner) - the game's own answer for the network partner - and calls on otherwise. The
  tick also holds a protected character's health at its most (anything that got past the hook). God mode covers the
  jeep of chapter 2-3 the same way: its own damage slot hooked, its health held - see "The jeep and the mounted
  guns".
- **One hit kills**: subtract-health (slot 69, `kSigEnemySubtract`, checked to be uEnemy's slot 69 on the same
  health fields) re-pointed at `hk_enemy_subtract` in every enemy vtable but the companions'. It raises a positive
  amount to the enemy's whole health, then calls on. Every way health leaves an enemy comes through it: the hit
  handler, the damage slot, the actions' fixed blows. The caller goes on as the game: the death, or the one point
  kept for an enemy that may not die yet. Not the Ndesu's or its Plagas', nor Irving's body and tentacles (their hit
  checks take the hit into counters of their own), nor a Reaper's body (its check refuses all but its weak spot),
  nor Uroboros Aheri's cores (its check keeps their health itself): those are hooked there - see "The Ndesu", "Irving
  (Transformed)", "The Reaper" and "Uroboros Aheri".
- **Infinite ammo**: the weapons' "is it infinite" (0xC4C200; called by the shot's ammo use 0xC89F30, the grenade
  throw 0x7E83E0, the HUD and the reload checks) answers yes for a weapon whose capacity level is 14+ (the bonus
  feature's infinite ammo sets that) or when `sPlayer::isInfiniteAmmo` (0x7E6A60, a stub that answers 0: a debug
  switch compiled out) says so. The mod patches that call (`E8 rel32` -> `mov al,1` + 3 nops, `kSigInfinite`): every
  weapon is the game's own infinite. With it, no gun a player holds overheats (`kSigOverheat`) - see "The jeep and
  the mounted guns".
- **Infinite money**: cSystemInfo's take-gold (0xCEEAB0): `if (have < n) have = 0; else have -= n` - both the
  clamp's `xor eax,eax` and the `sub eax,edx` become nops (`kSigTake`, two matches, gold's field the lower; the
  other is take-points, 0xCEEB00, only a cross-check now). Add-gold caps at 9,999,999. Infinite exchange points and
  their editor were removed at the user's word (2026-10-29); the ini's `InfinitePoints` is dropped (`sw::kRetired`:
  an ini with a retired key, or without a current one, is written again with its values).
- **Steady aim** (`kSigAimSway`, `kSigAimKickPitch`, `kSigAimKickYaw`, `kSigCrosshairOpen`): the player's aim is
  cPadEx's FUN_00B86CD0 (called by 0xB8C3F5, 0xCAD60B, 0xCAF075; `this` the pad, the character the first argument),
  every frame of an aim. The **sway** ("breathing"): two phases (+0x490 pitch, +0x494 yaw) wander by random steps,
  and +0x4A0 = sin(phase) x sizes (sParameter +0x2B8, +0x298 / +0x2A8, the weapon's, ...), +0x4A4 the yaw's; with a
  pad they are added to the aim's angles (+0x458 pitch = +0x4B8 + +0x4A0, +0x450 yaw = +0x4B4 + +0x4A4), with the
  mouse (FUN_00B5C4F0, "a local player on keyboard and mouse") to the character's aim pitch (+0x2908 = +0x290C +
  the pad's +0x4A0, FUN_00B66DE0 at 0xB67FBF/0xB68468) and the aim camera's yaw (the AI brain +0x6C4 = the pad's
  +0x4A4, 0xB7528A; the pad is the brain's +0x220), and the size is damped by sParameter+0x454 (RE3Controls'
  "Breathing"). (0x84A28E, once taken for the mouse's read, is a camera copying its own +0x3F0.. state.) The
  **kick** at a shot (character +0x2D8C bit 2):
  FUN_00B6E760 (thiscall, the weapon, `ret 4`: a random pitch from the weapon's table) into +0x4BC/+0x4B8 (and the
  aim camera's +0x290C with the mouse), FUN_00B6E7E0 (`(weapon, last), ret 8`: a yaw either way) into +0x4C0/+0x4B4;
  +0x4BC/+0x4C8 bring it back. The shot itself (uWeapon slot 111, 0xB97BA0, for every gun but the shotguns
  0xBB61F0, launchers, the bow, the stun rod, the turrets) flies from the muzzle straight at the aim's point - no
  random spread. The mod: the sway's last multiplies (`mulss xmm0,[esp+38h]`, 0xB8809A, 0xB880E6) become `xorps
  xmm0,xmm0` (+0x4A0/+0x4A4 = 0); the kick's calls with their pushes (0xB882A0, 8 bytes; 0xB8834C, 16 bytes) become
  `fldz` + nops (both kicks 0, the stack and the FPU as after the call); the mouse crosshair's opening at a shot
  (FUN_00B5B160, the HUD's +0x388/+0x394/+0x398, drawing only) returns at once (`ret 0Ch`). Not seen in play: whether
  the gun's own fire animation still moves the laser line with a pad (RE4OG's needed a held pose). Through a scope
  the view follows the gun model, which the aim's own motions breathe: Steady aim holds them still while a scope is
  up - see "The scope".
- **Automatic QTEs** - see "The action buttons" below.
- **Open map at chapter start** - see "The map" below.
- **Freeze play time**: cGameInfo's add-play-time (0xC41930, `(character, seconds)`) returns at once (`ret 8`).
- **Deaths don't count**: cGameInfo's add-deaths (0xC41870) returns at once.
- **Misses don't count**: a hold - each character's shots follow its hits (the counters, below).
- **Endless combo**, **Combo Bonus always on**, **Kill enemies as they appear** (the Mercenaries; `merc.cpp`) - see
  "The Mercenaries" below.
- **Freeze countdown timer**: the Mercenaries' clock - its frame (0x717850, on aGame, the play flow) stores `time -
  dt` at +0x4DC; the store (8 bytes) becomes nops (`kSigCountdown`). The time bonuses add elsewhere (0x71CC3D). The
  HUD's copy is computed from the same read, so it holds too. And the stages' countdowns (chapter 3-2's escape,
  Desperate Escape's last wait): their frames' subtraction of the frame time becomes nops - see "The countdowns".

## The jeep and the mounted guns (chapter 2-3) - built 2026-10-30

The user (2026-10-30): "the god mode cheat doesn't seem to work for the chapter 2-3 vehicle segment's vehicle health.
Also the infinite ammo doesn't seem to work for the gun overheating in the same section".

- **The jeep** is a `uOmJeep` (< uObjModel < uPawn, 0x1A20 bytes; the game makes `uOm1304`), in stages 109 and 111
  (aS109's and aS111's jeep events; the stage is cGameInfo's mTempBase.StageNo, +0x41C8). A pawn, as the characters:
  its health is uPawn's +0x1364/+0x1366 (3500, its constructor 0xADBB50) through the same set/add/subtract-health
  (slots 67-69; uPawn's own, which uCharacter keeps). Its attached parts (`uOmJeepRail`/`Roof`/`Cover`/`Antenna`,
  `uOmJeepDriver`) are pawns with the pawns' damage and decide nothing.
- **Its damage** (slot 66, 0xADC3E0 in uOmJeep's and uOm1304's vtables; the pawns' is 0xB56FB0): in stage 111 the
  damage is scaled by the lowest dynamic-difficulty level of the players in play (FUN_0071AF90), then the pawns'
  damage (the hit's value at +0x1368, subtract-health, `return health < 1`), then FUN_00ADB120 - in stage 109, flags
  0x39-0x3D for each fraction of its health it has fallen below. Every hit comes there: the hit handler FUN_00ADAE00
  (the pawn's hit record +0x129C: +0x48 hit, +0x28 damage, +0x34 value; afterwards, with cGameSys +0x79C set, its
  health back to its most; health below 1: FUN_00721840(player, 3) for each player, the jeep's action 5 (destroyed),
  sBioMain+0x10438 |= 0x20000000; else FUN_00721840(player, 0) and a reaction, action 3, 4 or 0x1F), the scripted
  blows FUN_00ADAC70 (states 0x15-0x17: 200, or the most -
  never the last point), the truck's ram FUN_00ADE3D0 (500, never below 100). A restart in stage 109 (FUN_00ADAFD0)
  sets its health to its most times the fraction of the lowest flag reached.
- **The airboat** (`uOmBoatAir`/`uOm1306`, chapter 3) passes every hit on as 0 damage (FUN_00A89CC0); its health
  (1000) never changes.
- **The mounted guns** are `uWpGunEmp` weapons: `uWpGunEmpJeep` (uWp2d, uWp2e - items 0x12D/0x12E, the jeep's) and
  `uWpGunEmpRide` (uWp2c, uWp2f "Gun turret", uWp31). A player holds one as the item in case slot 14 (0xE:
  getItemBySlot 0xC4B700, its model FUN_00C4D9B0); the model's owner +0xFDC is the character (cItem's set,
  `kSigItemAlive` +2). Enemies man them too (`cEm10ActAtkGunEmpRide`; the AI reads the heat at 0x9EB3E0, 0x9ECC40,
  0x9ECF20, 0x9FA1D0).
- **Overheating** is uWeapon's `OverHeat` block (createProperty names it): Enable +0x1C3F, Now +0x1C40 (overheated),
  Add +0x1C41 (fired this frame), TimerUp[3] +0x1C44, TimerDownOvh[3] +0x1C50, TimerDownNml[3] +0x1C5C, TimerNow
  +0x1C68 (0..1), SeType +0x1C6D; +0x1C6C picks of the three (the dynamic difficulty's level: below 7 -> 0, 7-9 -> 1,
  10 -> 2). A shot sets Add (FUN_00B97EB0, FUN_00B980E0, the jeep gun's FUN_00BA44A0; the player's fire actions
  FUN_00B81090, FUN_00B81223, FUN_00B84300, FUN_00B89EE0 refuse while Enable and Now). The weapons' update (uWeapon's
  slot 98, FUN_00B97280; every mounted gun's own slot 98 runs it first, then FUN_00BA1AE0's steam and its sound): when
  Enable, an owner that is a uPlayer driven by this machine's pad gets the index by its level; any other player's
  gun index 0 and - for the jeep's guns (0x12D-0x12E) - Add cleared: the AI's jeep gun never heats. Then with Add,
  TimerNow += dt / TimerUp[k] and at 1 Now is set; without, it falls by dt / TimerDownOvh[k] (Now) or
  TimerDownNml[k], and at 0 Now is cleared. Add is cleared at the end. FUN_00B5A820 (from a network message: the
  other machine's gun) sets Now and TimerNow 1.
- **The mod**. God mode: `kSigJeepDamage` (the damage's tail, checked to call on to uPawn's slot 66 and to lie in
  uOmJeep's slot 66) - that slot hooked in both vtables by `hk_jeep_damage`: `false` (no damage, not destroyed) while
  God mode is on, as for the characters. `game::discover_vehicles` notes uOmJeep's classes when its set-health is
  uCharacter's; the tick finds the jeep among the units (`Run::vehicles`), logs it as it comes into play ("the jeep
  (uOm1304) in play, its health 3500 of 3500"), and holds its health at its most under God mode (the restart's
  fraction). Infinite ammo: two jumps of the update's owner test (`kSigOverheat`: the owner field checked against
  cItem's set's, the record pushed uPlayer's, the driver field the characters', the id that of a uWpGunEmpJeep model)
  - the driver's `jne` (0xB97973) becomes `jmp` (every player taken for one this machine does not drive) and the
  jeep guns' `ja` (0xB979EB) nops (every gun taken for one of the jeep's): every gun a player holds forgets its
  firing, so it never heats (one that has still cools); the enemies' guns are no player's and heat as before. Not
  seen in play.

## The Ndesu (chapter 2-3's boss) - built 2026-10-30

The user (2026-10-30): "it also seems that the one hit kill cheat didn't work on the gigante boss at the end of the
same chapter". The game's text calls it the Ndesu (mes_system_e 1483, figure No.41; "Giant Majini", No.22, is the
big ordinary Majini).

- **The hit check.** uEnemy's hit path FUN_009DEA50 (from its damage handler FUN_009E0840, with slot 131's answer:
  the hit's +0x28, its damage) asks the enemy's **slot 129** (thiscall `(part, damage) -> yes`, `ret 8`; the part
  is the hit's +0x49) before any health goes - before working the damage out while +0x1DA0 is 0 (the default),
  after it otherwise. uEnemy's says yes (0x9DA880). 16 classes have their own (uEm34, uEm40/41, uEm41Arm, uEm42,
  uEm43, uEm45, uEm4502, uEm46, uEm80/81/a2, uEm82, uEmc0/c2/c3/c5), read on 2026-10-30 no further than this:
  uEm80/81/a2 (FUN_009086C0) and uEmc5 (FUN_0095F1E0) always say yes; uEm40 and uEm41 mostly yes, wearing regions
  of their own besides; the Ndesu's, Irving's and the Reaper's (uEm34; below) no at least to their bodies; uEm41Arm,
  uEm42, uEm43 and uEm82 keep counters of their own and say no at least at times (uEm42, Uroboros Aheri: below). The classes' names are the game's
  kill records' (the table 0x10CB3E8, 55 entries of 10 bytes: the record +0, its name's line in mes_system_e +4 (a
  word, 1464-1503), the category +6 (3 an enemy), the class's number +7 (uEm<number>), a sub-kind +8 (a word, 0 any);
  FUN_007B8D70 finds an enemy's, counting uEm11/15/1d-1f/21/92 as uEm10, uEm31 as uEm30, uEm52 as uEm50, uEm81/82/a2
  as uEm80): uEm0b Bui kichwa, uEm0c Kipepeo, uEm0d Cephalo, uEm0e Duvalia, uEm10 Majini (Town; its sub-kind 0x100
  Wetlands), uEm12 Majini (Wetlands; 0x100 Base), uEm14 Majini (Base), uEm13 Big man Majini, uEm20 Executioner Majini,
  uEm26 the red one, uEm22 Chainsaw Majini, uEm23 Gatling gun Majini, uEm24 Giant Majini, uEm50 Motorcycle Majini,
  uEm30 Adjule, uEm32 Licker β (line 1477; its β a glyph gen_items does not read), **uEm34 the Reaper**, uEm40
  Uroboros, uEm41 Uroboros Mkono, uEm42 Uroboros Aheri, uEm43 U-8, uEm44 Popokarimu, uEm45 the Ndesu, uEm46 Irving,
  uEm80 Wesker, uEm83 Jill, uEm61-70 the animals. The stages' archives agree (s103 uEm40, s108 uEm44, s111 uEm45,
  s205 uEm46, s304 uEm43, s308 uEm41, s511/s512 uEm42; s307 and s505 uEm34).
- **The Ndesu** is `uEm45` (0x1FB0 bytes; health 8500, never touched). Its start (slot 5, FUN_008E42F0) makes a
  `uEm4501` (+0x1F60, at joint 0x11; it only waits) and **five Plagas** (`uEm4502`, +0x1F64..+0x1F74, joints 80, 83,
  86, 89, 62; each one's index +0x1F58; the fifth flagged the main one, its +0x1BB4 bit 4), gives each of four body
  regions (4, 1, 2, 3: uEnemy's region table +0x1C0C + region x 0xC, FUN_009D9AA0) 8000, and sets its **gauge**
  (+0x1F50, a float) from the round's start (0xF82BE8: 20000, 45000, 35000, 40000; the round from +0x1BB4 bits 9-10).
  The gauge is only ever set from that table (FUN_008E1C30; PlagaWait's start FUN_008E60E0, which moves the round
  on), never added to. Its slot 112 hands
  itself to every `uWpGunEmpJeep` as the target (+0x1DA8).
- **Its check** (FUN_008E19E0) always says no: by the part hit (FUN_00C44150 -> a row of 0xF83A68) it wears each
  region whose Plaga is still in (bit 1 clear) by damage x 0.3..1.0 (FUN_009DA890; worn through -> that Plaga's bit
  2, "come out", unless +0x1E24), takes damage x the part's factor (0xF82C08: 1.0 or 0.8; 1.0 with +0x1E28) off a
  stagger (+0x1F54; while it is down (bit 1) a weak spot (+0x448 bit 13) emptying it takes 5000 more off the gauge)
  and off the gauge - which stays at the round's floor (0xF82BF8: 10000, 15000, 15000, 20000) while bit 11 is set.
- **A Plaga** (`uEm4502`, 0x1F70 bytes; its count +0x1F50 an int, 8000, the main one's 26000 - FUN_008E6720) comes
  out by its appear action (FUN_008E3CB0: bit 2 -> bit 0). Its check (FUN_008E14F0) says no as well: while it is out
  (bit 0) the damage comes off its count - below 0 its death (bit 3, unless its action is 2) - and, through its
  parent (+0x18, the Ndesu; not with +0x1E28), off the Ndesu's gauge and stagger alike; bit 5 "hit". Its slot 130
  (FUN_008E3AE0), right after: the main one dead -> the kill counted (FUN_00721C50) and sBioMain+0x10438 |=
  0x80000000 (FUN_00726120) - the fight's end.
- **Its brain** (an FSM, around 0x9AB000; the Ndesu at its +0x280): the main Plaga dead -> state 1 (FUN_009AB160);
  some Plaga dead and none of the first four still out -> 5 (FUN_009AB270); a Plaga hit while the gauge is empty
  -> 6 (FUN_009AB210); the gauge empty -> reaction 0x17 (FUN_009AB2F0), whose start sets bit 11 (FUN_009ABF80;
  0x12/0x13 clear it). Its actions (cEm45Act*): Wait, Move, Turn, Attack, Tackle, PoleAtk, Pickup, Pillar, Quake,
  Damage, PlagaWait (the round moved on: bits 9-10, the gauge set again), PlagaDie, DeathAtk.
- **The mod** (`kSigHitCheck`: both calls' slot 0x204, agreed; `kSigNdesuHit`: the check's tail, within 0x200 of
  uEm45's slot 129, its gauge twice; `kSigPlagaHit`: uEm4502's slot 129, its count, the Ndesu's gauge twice and the
  flags thrice, as the Ndesu's): slot 129 of both classes hooked. With One hit kills a hit with damage on the Ndesu
  goes on as 1,000,000 (past any gauge or region at the least share, 0.3) - the gauge to its floor, or to nothing
  (put back to 0 if the hit took it below: the brain only asks whether any is left), every region through (each
  Plaga still in comes out), the stagger gone; on a Plaga as its count + 1 (its death; the Ndesu's gauge loses that
  much, as when it is shot dead). So the fight goes the game's own way, a hit a step. Not seen in play.

## Irving (Transformed) (chapter 3-3's boss) - built 2026-10-30

The user (2026-10-30): "the one hit kill on the boss of 3-3 also has this problem". The game's text calls it Irving
(Transformed) (mes_system_e 1484, figure No.42); it is stage 205's, the last of chapter 3-3 (`s205.arc` names `em46`).

- **Irving** is `uEm46` (0x21F0 bytes; health 2000, its constructor 0x8EB9C0). Its start (slot 5, FUN_008EBD40)
  copies its parameters (0xF84198, 100 bytes) to +0x1FF0 - its body's region 12000 (+0x1FF0), a tentacle's 2000
  (+0x1FF2), the brain's thresholds 200, 8000, 8000, 7000, 3000, 1500 (+0x2038..+0x204C) - gives region 1 its 12000
  (the only time it is set) and makes its brain (FUN_009AD930: a unit on line 5, vtable 0xF96E48, Irving at +0x280).
  Its parts (FUN_008E78C0): four **tentacles** `uEmc0` (+0x2090, +0x209C, +0x20A8, +0x20B4), four **grabbing
  tentacles** `uEmc2` (+0x20C0..+0x20CC; each with 20 segments, `uEmc4`, whose check is uEnemy's: health) and the
  **core** `uEmc3` (+0x2058, and +0x1BFC; at joint 0xDF).
- **The regions** (uEnemy's, 32 of 12 bytes from +0x1C0C: worn through (a byte), what is left +4 (an int), its most
  +8): FUN_009D9AA0 `(region, n)` sets one (left and most n, not worn); FUN_009DA890 `(region, change)` adds the
  change and at 0 or below marks it worn through (left 0), answering 1 - unless the enemy may not be hurt this frame
  (+0x1E24: then 1 is left). Once worn, a hit takes what is left below 0.
- **Its check** (slot 129, FUN_008E82E0): a part below 0x3C is its body's - unless its kind (FUN_00C44150 on
  +0x1290) is 2 with +0x20FA clear, region 1 is worn by the damage and +0x2100 gets it; no. From 0x3C on the part is
  its core's (the brain's frame FUN_00963C90 hands Irving the hits on the parts it keeps from +0x1BFC, each part +
  0x3C x (its place + 1)): on the core's weak spot (kind 11) while the core may be hurt (its +0x1E24 clear) +0x2104
  gets the damage and +0x20D0 = 1 - yes: health. The core's own check (FUN_0095DD10) always says no; its actions Off,
  DownEnd and DownEnd2 set its +0x1E24 every frame (uEnemy's frame, 0x9E0A5B, clears it), so it is open in Wait,
  Down, Damage and Die. Irving never sets its own +0x1E24/+0x1E38: it may die whenever its health is gone.
- **Its brain**: on a hit frame (+0x1E25) with health left, its damage reaction (slot 35, FUN_009ACF80): region 1
  worn through and an action that allows it (the waits, shifts, dives and tentacle attacks: the table 0x1119DF8,
  byte 1 of 4) -> **DownDive** (action 0x1D; the actions' table 0x1119C08, 8 bytes each: 0x12 Die, 0x1E Down, 0x36
  a bite, 0x37 DownEnd); its end -> **Down** (0x1E: +0x2104 = 0, the core told down, FUN_0095DD20(1)); in Down
  +0x2104 above 5000 -> DownEnd (0x37); Down's end -> the bite 0x36, whose end -> Down again. With no health left
  its death reaction (slot 36, FUN_009AD040) -> Die (0x12), unless a player is busy (slot 87). Body damage of 200
  (+0x2100) and region 1 below 8000 pick its patterns (FUN_009AE630); a grab (a uEmc2 with bit 0) moves Irving to it.
  The game's tail of a hit (FUN_009DEA50) keeps 1 point when the AI partner's hit would kill it (+0x1E3D clear).
- **A tentacle** (`uEmc0`, 0x2080 bytes): its start (FUN_009573F0) gives region 1 Irving's +0x1FF2 (2000); its check
  (FUN_00956CC0) wears it by the damage on its weak spot (kind 11), or on any part while more than a tenth is left;
  no. Worn through, its brain's hit reaction (FUN_009D60E0) breaks it (FUN_00958FA0: +0x1BB4 bit 4, which Irving's
  brain counts; an item dropped, FUN_007BF050; +0x2074 counted); its Repair sets the 2000 again (0x958BA1,
  0x95918C). A grabbing tentacle's check (FUN_0095B390) marks the segment hit (its +0x1BB4 = 2) and wears the region
  of the part's kind, which nothing reads.
- **The mod** (`kSigRegionWear`: the regions' fields, agreed four times over, the most's index x 4; `kSigIrvingHit`:
  the body's parts below its word +9, agreed by the core's `add eax, -0x3C`, region +60, the wear called;
  `kSigTentacleHit`: its region's left and most where the wear's put them, the wear called; the kinds' table and
  lookup the same in both; the core's "may be hurt" the wear's): slot 129 of uEm46 and uEmc0 hooked. With One hit
  kills a hit with damage on Irving's body (a part below 0x3C) or on a tentacle goes on as all that is left of
  region 1 (or the damage, if more; unchanged once it is worn): the body worn through at once - so that hit, or the
  next in an action that allows it, sends Irving down, where a hit on the open core takes all its health
  (subtract-health's hook) and kills it (the AI partner's leaves 1: this machine's next kills); a tentacle breaks at
  one hit. The core's hits are left as they come (raised, +0x2104 would pass 5000 and end the Down). Not seen in
  play.

## The Reaper (One hit kills) - built 2026-10-31

The user (2026-10-31, playing chapter 5): "I'm not sure the one hit kills are working against the reaper enemy".

- **The Reaper** is `uEm34` (0x1FB0 bytes; health 10000, its constructor FUN_008A49A0; the kill records' 1503, figure
  No.26), in stages 307 (5-2) and 505 (6-3) and 401 (no chapter's: one of 400-407, the Mercenaries' it seems). Its
  start (slot 5, FUN_008A4BB0) makes its brain (FUN_00997610, vtable 0xF95D98), a `uEm3402` (+0x1F6C) and a `uEm3403`
  (+0x1F70) - their actions Blow and Erase: its slot 130 (FUN_008A0390) takes a hit's damage off the counter of the
  part's group (part 0, and four legs' among parts 11-26: +0x1F8C + 4 x group, 20 each) and, once it runs out, marks
  the group (+0x1F2D) with a uEm3403 for part 0, a uEm3402 for a leg, until a timer of the group's runs out (+0x1F78,
  the brain's slot 37) - and, unless its +0x1BB4 bit 22, a `uEm3401` (+0x1F74, a soft body; kept at +0x1BFC, so a hit
  on it reaches the Reaper as a part from 60; bit 27 set). Its actions (the table 0x1116E40, 8 bytes each; the current
  one +0x10E0): 0 Wait, 1 Ramp, **2 Hang**, 3 and 0xE Fall, 4 Move, 6 Crawl, 8-0xD and 0x12 Jump, 0x10-0x11 Turn, 0x13
  Smoke, 0x14-0x16 Attack, **0x17 AtkCatch** (its grab: the player's death is uEm34::cEm34ActAtkDeath_PL), 0x18-0x1A
  Damage, 0x1B CoreWait, 0x1C CoreMove, 0x1D FinalEnd, 0x1E-0x20 Abort, 0x21 Blown, **0x22 Die**, 0x23 Disrupted, 0x24
  Erase. Its 30 parts' kinds (the table [0x1116CE4]): 2 2 1 1 8 10 10 7 9 9 1, legs 4 and 3 (11-26), 11 6 5 - parts
  27-29 its weak spot. Its constructor sets no damage factors by kind (+0x12C8, which other enemies' set through
  FUN_00C44180): every part's the default, -1, which FUN_00C46050 takes for x1 - none is 0.
- **Its damage as the hit is made** (slot 70, FUN_008A1D90, from FUN_00C47560 after the base FUN_00C46050), by the
  weapon's class (the weapon table +0xB8, FUN_007E5FA0): 0x400 (hand grenade, proximity bomb) and 0x100 (the grenade
  launcher and its rounds) set its mark **+0x1F60**; 0x200 (rocket launcher, L.T.D., RPG-7 NVS) too, and with the
  attack's +0x40 bit 0x200 its mark **+0x1F61**; 0x2000 (the stun rod) and 0x80000 their own factors; any other: part
  27 the damage, 28-29 half, any other part a tenth (0.1, 0xF79B04).
- **Its damage** (slot 131, FUN_008A0350): a hit of kind 0x1E0018 (+0x9C - what a stage hands its `uOmHitCheck`
  objects, a trap: FUN_0074BB60, FUN_007535B0) sets both marks; with +0x1F61 set the hit gets its whole-health bit
  (+0x3C 0x10000000) and 30000 - the game's own instant kill; else uEnemy's (the hit's +0x28).
- **Its check** (slot 129, FUN_008A1CF0, `(part, damage)`): a part from 60 (the uEm3401's) no - +0x1BB4 bit 20 set
  (unless bit 21), which its brain takes for a wake-up (FUN_00996FF0: state 3); in Hang or AtkCatch no; parts 27-29
  yes; any other part no without +0x1F60, yes with both marks, and with +0x1F60 alone the damage capped at its health
  - 1 taken through subtract-health's slot (FUN_009DABA0, `jmp [vtbl+0x114]`), then no. Every path clears both marks
  (but a "no" without +0x1F60, which leaves +0x1F61 as it was). So shots on its body take nothing, a grenade's blast
  leaves it 1 at least, and only its weak spot's hits take health (27 the hit's damage, 28-29 half).
- **Its brain** (vtable 0xF95D98, the Reaper at +0x280): the hit notice (slot 39, FUN_00997FC0, before the damage)
  in Hang notes a hit for the wake-up (+0x28C), in AtkCatch nothing; otherwise picks the reaction (+0x2A0): Damage
  0x18-0x1A on the weak spot's parts (+0x1BB4 bits 8, 6, 7), Blown (0x21) for a blast, 0x23 for the 0x200 class with
  the hit's +0x3C bit 0x200; on any other part +0x28C. Its damage reaction (slot 35) answers it; its death reaction
  (slot 36) Die (0x22), Disrupted (0x23) after a 0x23 - the generic frame FUN_00963C90 asks it on a hit frame with no
  health left (FUN_009636C0).
- **The mod** (`kSigReaperHit`: the check, its marks agreed four and three times, its health the characters', the
  capped path's call the subtract-health slot's stub; `kSigReaperKill`: its damage, two places on in its vtable, the
  same marks, then a jump to uEnemy's damage): uEm34's slot 129 hooked. With One hit kills a hit with damage on its
  body (a part below 60) sets both marks - the game's own instant kill's - before the game's check: its body then says
  yes as the weak spot does (a blast too, past its cap), subtract-health's hook takes all its health and the death is
  the game's (FUN_009DEA50's tail, the brain's Die). Hanging or holding a player it takes nothing, as the game has
  it; a hit on its uEm3401 is left as it comes (the wake-up). Before the fix One hit kills reached only the weak spot
  (and a blast, whose capped subtraction its hook raised to the whole health - a death by the brain alone, without
  FUN_009DEA50's tail). Built, tested offline (`tests/test_cheats.cpp`), the discovery replayed over the dump; not seen
  in play.

## Uroboros Aheri (One hit kills; Excella's, the boss ending chapter 6-2) - built 2026-11-01

The user (2026-11-01): "fix 1 hit kill cheat to work on the excella boss"; then, while it was being read: "I'm fine with
needing to hit this boss, and other bosses, more than once as long as the components to drive the fight forwards can
also be killed in one shot. That way we drive the fight forward with each piece that needs hit only being hit once
each to kill them".

- **Uroboros Aheri** is `uEm42` (0x21E0 bytes; health 28000, its start FUN_008C0450), in stage 511 (chapter 6-2's last
  area, scenes from 10600, and 6-3's first, from 11000) and 512 (6-3). Its start makes:
  - its brain (`uEmIntel42`, FUN_0099FB30, vtable 0xF96470; uEm42 at +0x280);
  - two `uOmS511_UroExtra` (+0x2114, +0x2118: extra masses, no damage of their own);
  - its four **cores** (`uEm42Core`, +0x2104..+0x2110). They are also kept at +0x1BFC.., so the brain hands their hits
    to uEm42 as parts from 60 x (core + 1) (FUN_00963C90).
  - eight `uOmS511_SlimeTarget` (+0x20D4.., no damage of their own).

  A variant (+0xD78 == 1) starts in DeadLock (13): the corpse of chapter 6-3. Its `uEm42Slime` and `uEm42Tentacle`
  parts (FUN_00788B60 makes them too, from a network message) are plain uEnemy health, through uEnemy's check.
- **Its actions** (the table 0x11181F0, 8 bytes each; +0x10E0): 0 Dead, 1/2/5 Wait, 3 and 12 Enter, 4 CoreEnter,
  6 CoreHide, 7 CoreDamage, 8 Round2Enter, 9 DeathBeat, 10 DeathBeatOk, 11 DeathBeatEscape, 13 DeadLock.
- **Its flags** (+0x18DC):
  - bit 0: the main core is out (set by CoreEnter's start, cleared by CoreHide's end);
  - bit 1: round two (Round2Enter);
  - bit 2: the doom attack (DeathBeat);
  - bit 3: round one's core has been out (CoreHide then leads to Round2Enter).
- **Its check** (slot 129, FUN_008C0CD0, `(part, damage)`):
  - **A part below 60 is its body's**: its main core, hittable only while it is out (FUN_00C46BB0(1) at CoreEnter,
    (0) at CoreHide and Round2Enter). The answer is yes (health). Two counters take the hit too: +0x212C (past 3000,
    +0x1E2C |= 0x200, a flinch) and +0x2130 (past 12000, 0x10000: the core hides).
  - **From 60 on a part is a core's** (core = part / 60 - 1), and the answer is no.
  - During the doom attack (bit 2) the hit goes into its escape, +0x2154 (past 2000: 0x8000).
  - Otherwise it goes into the core's flinch, +0x211C + 4 x core: past 1500 x its factor (+0x1FE4 + 4 x core), or
    800 x it while the core is closed, gives 0x20 << core.
  - The core's health is worn only while the core is open (its +0x18DC bit 1 clear) or flinching (0x40). The wear is
    FUN_008BF0B0, `(core, change)`, and acts only on the network host. Each core's health is a word at +0x1FC4 +
    8 x core, its most at +0x1FC6; 7000, from the constructor FUN_008BFEB0. Worn through (below 1), the core breaks:
    1 << core.
- **Its slot 130** (FUN_008BD660): a hit from the satellite laser marks 0x400 << core, or 0x4000 on its body. The hit
  is the laser's when its +0xA4 is the class id of `uWp3f` (item 0x13F): the strike the L.T.D. (`uWp34`, item 0x134)
  makes once it locks on (FUN_00BBDE20).
- **Its brain's hit notice** (slot 39, FUN_009A0360) marks 0x20000 x 2^(part / 60) for a hit with +0x3C bit 0x2000
  and +0x40 bit 0x40000000. The attack data's commands 0x19, 0x1A, 0x2E and 0x2F set the latter (FUN_007DA250).
- **Its brain's damage reaction** (slot 35, FUN_0099FEE0; uEnemy's frame clears +0x1E2C at 0x9E0AD2):
  - none in Dead or Enter;
  - main core in: a core broken (1 << core), or hit by the laser or the special (0x400 / 0x40000 << core), gets
    Break (5); a flinch gets the core's Damage (4);
  - main core out: 0x20000 (the special, on its body) gives Dead, the kill credited to the rocket launcher
    (FUN_007B7630(0x10D)); 0x200 gives CoreDamage (7); 0x10000 gives CoreHide (6);
  - in the doom attack: 0x8000, or any core's special, gives DeathBeatEscape (11).

  Its death reaction (slot 36) is always Dead (0). With its health gone on a hit frame (FUN_00963C90 -> FUN_009636C0)
  it dies. Nothing of its own sets +0x1E24 or +0x1E38, but a grab's frame (slot 141).
- **Its brain's think** (slot 37, FUN_009A2510):
  - main core in, and round one's core has been out (bit 3): Round2Enter;
  - a broken core (its +0x18DC bit 4, set as its Break ends) whose regrowth timer (+0x1F84) has run out regrows
    (Regene, 6). Its health comes back at 70% of its most (at least 1500), its factor likewise (FUN_008C9CA0).
  - cores 0 and 1 broken in round one, or all four in round two: CoreEnter (4);
  - main core out, and a broken core's timer has run out: CoreHide (6).
- **A core** (`uEm42Core`, 0x1FC0 bytes; its actions the table 0x11183F0: 0 Enter, 1 DeadLock, 2/3 Wait, 4 Damage,
  5 Break, 6 Regene, 7 Blow, 8 DeathBeat, 9-12 2nd/3rd Blow, 13 DeathBeatOk, 14 DeathBeatCancel, 15 Damage) opens and
  closes on timers of its own (FUN_008C2CE0: +0x1F88 and +0x1F98; bit 2 closing, bit 1 closed). Its Damage opens it
  (bits 1-2 cleared, 0x40 while it plays). Break turns its hit shape off (FUN_00C46C70(0)) and starts its regrowth
  timer; Regene turns it back on.
- **The doom attack** (DeathBeat, 9) follows Round2Enter's motion. Its start sets bit 2 and the escape to 0. After
  its motion and a timer, DeathBeatOk (10) sets every player's health to 0 (slot 67), unless the escape was passed
  first (DeathBeatEscape, then Wait 5).
- **The fight**:
  - Round one: cores 0 and 1 broken, then the main core is out. 12000 on it (or a broken core's timer) hides it
    again: Round2Enter, then the doom attack.
  - Round two: all four cores broken, then the main core is out.
  - Its death (Dead) comes by its health, or by the special hit while the main core is out.
- **The chapter's end**:
  - Dead's start sets the profile's SceNo to 0x2990 and tells its FSM 0x72. Its frame sets 0x299A partway through
    its motion.
  - The play flow's frame (FUN_00724A20 -> FUN_007165F0) takes 0x299A for the chapter's end: aGame step 7
    (FUN_007165D0).
  - aS511's frame (FUN_0075AD00) sends its FSM 0xF at the stage's start, and 0x10 once SceNo is 0x2990 or more - but
    only after some player has held the L.T.D. in slot 14 (+0x639). The chapter's end does not wait for that.
  - The AI partner's brain (FUN_00CAF090, case 0x42) targets the main core while it is out, unless the L.T.D. is
    charging (uWp34 +0x1C9C).
- **Text** (mes_stage5_e 412-413, 432-433, 440-443, 455-458, 470-475): "This thing is gigantic! We have to take it out
  before it destroys the ship!", "It's a satellite laser! Maybe we could use it to destroy that thing!", "The
  targeting device should be on the roof! Hurry!", "Persistent son of a bitch."
- **Before the fix**: One hit kills took all of the main core's health at one hit once it was out (the subtract-health
  hook; its death the game's). But a core's health is the check's own counter, so each core took as many hits as ever
  (7000), and so did the doom attack's escape (2000).
- **The mod**: uEm42's slot 129 is hooked. Its signatures and checks:
  - `kSigAheriHit`: the body's parts agreed by what the check takes off a core's part, the last core, the flags and
    the doom bit, the escape twice and its most, the wear called twice;
  - `kSigCoreWear`: the core's health six times, its most, and the scale of the cores' step;
  - uEm42's slot 129 checked to be the check, and the fields within the class.

  With One hit kills, a hit with damage on a core:
  - in the doom attack, takes all that is left of the escape: the attack is broken off, the game's own
    DeathBeatEscape;
  - otherwise, the core's health is made 0 before the check. The check finds it worn through, as at its own last
    point, and breaks it - open, closed or flinching, grown back or not.

  So round one takes two hits on cores and one on the main core, and the chapter ends as the game ends it. Field
  writes only.

Built, tested offline (`tests/test_cheats.cpp`: 16 checks, four broken builds caught) and installed; the discovery
replayed over the dump. Not seen in play.

## The scope (Steady aim through one) - built 2026-10-30

The user (2026-10-30): "the steady aim cheat doesn't perfectly steady the aim when looking down a rifle scope".

- **The scoped weapons**: the weapon table (0x11B5170 + (id & 0xFF) x 0x140) +0x98 not 0 - the S75, the PSG-1, the
  Dragunov SVD, the rocket launcher, the L.T.D., the RPG-7 NVS and 0x138; all of table type 0 (+0x10), so their fire
  motion plays once (the fire action FUN_00C89CC0 gives types 2 and 4 - the jeep's guns, the turret, the flamethrower
  - a looping one).
- **Up and down**: FUN_00B5AF10 (thiscall on the character, `(on)`; from the aim loop's start FUN_00C8C770, the fire
  action's FUN_00C89CC0/FUN_00C89F30, and the aim's reset FUN_00B7A240 - 48 callers, every other action's start among
  them) opens it for a scoped weapon: the character's per-viewport draw mask without this viewport (uPlayer slot 51,
  0xB6FF50: the model and the 12 case slots' models - the character is not drawn), the scope camera asked for
  (FUN_00B59880), uCameraCtrl +0x30B7 = 1, **character +0x2998 = 1** (a byte), the zoom's FOV (camera +0x2804/+0x2808),
  the scope's HUD unit (`uIdScope00`, character +0x2644) shown. Down: the reverse, +0x2998 = 0.
- **The scope camera**: uCameraCtrl (0x3100 bytes) keeps sub-cameras by name (createProperty 0x838D20): mLERP +0x1C0,
  mLAG1 +0x380, mQuake +0x610, mQFPS +0xE00 (`uCameraQFPS`), mAnim +0x1480, **mWep +0x2650** (`uCameraWep`), mMot
  +0x2870, and the request list +0x2DE0 (a callback camera: FUN_008433A0 `(callback, arg, type)`, the request's +4
  type, +8 arg, +0xC callback; its update FUN_00842EA0 calls the callback with a 0x40-byte out: eye, target, roll,
  FOV). FUN_00B59880 asks for type 7 with FUN_0084B590 and mWep; +0x30B8 = "type 7 is current" (the HUD's test). The
  callback: FOV from mZoomMin/mZoomMax by the zoom (character +0x2DCC = the pad's +0x45C), **eye = character
  +0x26F0, target = character +0x2720**, a roll.
- **Eye and target** (FUN_00B66DE0, the character's, each frame): from the gun model in hand (FUN_00C4D9B0(14)):
  eye = its joint 103 (0x67; joint 100 when it has none), target = its joint 100's matrix (17 for a model of class id
  0x130) x (-d, 0, 0) - or (0, 0, d), (0, 0, -d) for the table's +0x94 1, 2 - d = the table's +0xC (x a stage
  factor); with a pad, not scoped and +0x98 0, the target is the aim's point (+0x2850) instead. While scoped (+0x2998,
  not attached to another model: +0x18 0) and 0.2 s after it went up (+0x276C, +0x2764), each is eased: new = 0.2 x
  this frame's + 0.8 x the last (+0x2768 = 0.2, set once at 0xB6F4DC). For the
  Dragunov (0x120, 60) and 0x10D/0x134/0x135/0x138 (70) the eye moves along joint 103's x by the pitch (+0x2DC4)
  when it is above 0. The aim's point (+0x2840.., the shots' end) is the ray from uCameraCtrl's camera (+0x30 eye,
  +0x50 target) - through a scope, the scope's own centre.
- **The aim's pose** (`cPlAction_pl_act_weapon_ready_loop`, vtable 0x10444B8: start 0xC8C770, frame 0xC899F0):
  layers 0, 1, 2 play motions 0x51A, 0x51B, 0x519 (bank 5: the weapon's set, `pawn\pl\pl00\motion\pl00wpNN` in the
  weapon class's archive), blended by the pitch (FUN_00C771A0 -> each layer's +0x14); FUN_00C850E0 plays the stance
  on 3, 4, 5 (0x53D, 0x53E, 0x53C; turning, 0x542-0x547 at the turn's rate set each frame); a play on layer 0
  (FUN_00B7C9D0) also sets layer 6 (the weapon's grip, bank 4) and layer 7 (once). **The keyframes breathe** (the
  S75's set, Chris, read out of `uWp05.arc`): 0x19/0x1A/0x1B are 80-frame loops turning bones 1-13 (spine to hands)
  by up to 4.5 degrees (bone 11), 0x3C/0x3D/0x3E 80-frame loops (bones 15, 16, 20 and bone 0's translation). So the
  gun - and through a scope the view - rises and falls with the breath whatever the aim's angles do.
- **uModel's motion layers** (`uModel::Motion`, 0xE0 bytes, eight from uModel +0x380 - its constructor makes eight,
  createProperty's mMotion says 8, no model is given more; +0x374 how many its clock runs, FUN_005CCE40 sets it):
  +4 the motion's number (bank << 8 | index; 0xFFFF none), +8 state, +0xA attributes (4: plays once - the set adds it
  when the motion's loop frame is below 0; 0x10: the clock stopped), +0xC blend-in frames, +0x10 the blend-in's count,
  +0x14 blend, +0x18 frame, +0x1C frames, +0x20 loop frame, +0x24 speed. The set (FUN_0059E880, thiscall `(layer,
  motion, blend-in, frame, speed, attributes)`, through uPlayer slot 91 -> 122 -> FUN_0084CFF0) writes all of them.
  The clock (FUN_005A1C00, from uModel slot 23): `frame += speed x the model's frame time (+0x10)` unless 0x10, the
  blend-in count down by the frame time alone. The game holds the aim's layers itself on the jeep's scripted moment
  (FUN_00ADF430 sets each character's +0x2D98 bit 0; the aim loop then keeps layers 0-2's speed 0, saving layer 0's
  in the action +0x20, and puts it back when the bit clears).
- **The mod** (`kSigScopeOpen`, `kSigScopeClose`: the flag set and cleared, the camera's +0x30B7 both times, the
  scope camera's request called; `kSigMotionSet`, `kSigMotionLoop`, `kSigMotionClock`: every field read out of the
  set, agreed by the clock - count, first layer x size, speed, the blend-in, the frame time - and the layer's size
  by `uModel::Motion`'s record): each tick, for a character this machine's pad drives whose +0x2998 is set, Steady
  aim on, every layer the model runs whose motion loops (not 0xFFFF, not attribute 4) and runs (speed not 0) gets
  speed 0, its motion and speed noted; once the scope is down or the switch off, a noted layer still on that motion
  at speed 0 gets its speed back. A shot's or a reload's motion plays once and is never held; the turning steps get
  their speed from the game each frame after the tick, so they still step; a loop started over (the aim loop after a
  shot) is held again at once - at its start, so the pose after a shot is the one before it. A character gone from
  play is forgotten without a write. Built and tested offline (`tests/test_cheats.cpp`; the discovery's reads and
  checks replayed over the dump); not seen in play.

## The countdowns (chapter 3-2, Desperate Escape) - built 2026-10-30

The user (2026-10-30): "in chapter 3-2 there is a countdown timer. We need to add a freeze countdown timer cheat
similar to how we've done it in prior RE game mods".

- **The HUD's countdown** (uIdCockpit00, each player's: the character's +0x2640): FUN_009EC370 (thiscall
  `(seconds)`, `ret 4`; `kSigHudCountdown`) shows it - +0x34C the seconds (capped at the float at 0xF9E47C, 5940:
  99:00), +0x344 = 1; FUN_009EC3A0 hides it (+0x344 = 0). Its draw (FUN_009F0390) prints `mm:ss:hh` from +0x34C
  unless sBioMain+0x10438 has 0x70000400 or +0x1043C has 0x8000. The HUD's frame (FUN_009F8FC0) sets it itself from
  cGameSys +0x680 (while +0x67C >= 0) in modes (cGameSys +0x58) 1, 2 and 5 - the Mercenaries' among them. Only two
  callers of the setter, both stages: aS209's frame and aS704's.
- **The areas**: sArea (sBioArea, 0x384C bytes) runs its areas from +0x382C (8 pointers, the first null ending them;
  `kSigAreaFrame`, sArea's slot 6, FUN_00434490): each one's slot 5 until it answers ready, its slot 6 (its start)
  once (+0x20 = 1), then its **slot 7, its frame**, every frame. The areas are cArea classes: aGame (the play flow,
  0x510 bytes; FUN_00725E80 its frame, the play's steps by its +4 - so the Mercenaries' clock +0x4DC is aGame's) and
  the stage in play, `aS<stage>` (aStageBase < cArea: 0x650 bytes, the stage's own fields after; 93 of them).
- **The chapters' stages** (the table at 0x10CAFB0, 78 entries of 8 bytes: u16 stage, u8 position, u8 chapter, ...;
  walked at 0xC41A81 and 0xC41AE0): chapter 6 (3-2) is stages 201, 207, 203, 209; 5 (3-1) 606, 200, 202; 7 (3-3) 204,
  607, 205. The user's game was in stage 201 (cGameInfo +0x41C8; ChapterNo 6) at 19:14 that day (read live).
- **Stage 209** (the oil field's dock, the end of 3-2 - Josh: "Hurry up you two! We're running out of time!", "Shit!
  It's gonna blow!", mes_stage2_e 989-990): aS209 (0x670 bytes; its constructor zeroes +0x650..+0x661). Its slot 17
  (FUN_00745610, `ret 14h`, the float the 4th argument - the scenario's FSM's) sets **+0x650, the seconds left** (and
  hides the HUD's for 0 or less; then three cues through FUN_00772200); slot 18 (FUN_007450E0) answers **+0x654, "run
  out"** (while cGameSys +0x574 == +0x578): the scene's time-out. Its frame (slot 7, FUN_00745240), while +0x650 > 0:
  0 if sBioMain+0x10438 has 0x10000000 (the scene ending it); unless paused (0x800) `time -= dt` (the frame time,
  [0x11B209C] +0x28; `kSigStageCountdown`), clamped at 0; then for each player 0-1 with a HUD: time left -> show it
  (the character running, its +4 & 0x400, sBioMain+0x1043C's bit 31 and 0x2000000 clear, FUN_00427C90), none -> hide
  it and set +0x654.
- **Stage 704** (Desperate Escape's last wait - "Just hold out a bit longer for me, Ms. Valentine!", mes_stage7_e 515):
  aS704 (0x660 bytes) the same with **a floor**: slot 17 (FUN_00760E40) sets +0x650 (the time), +0x654 (what it runs
  down to) and +0x658 ("run out") 0; its frame (FUN_00760CE0) runs while +0x650 > +0x654, unless 0xC00, and clamps at
  the floor.
- Aside: the scenario's FSM has a "Timer" order of its own (cFSMOrder slot 19: `FuncParamTimer`'s mSetType 0 start, 1
  stop, 2 reset; mFreeRealNo 16..47 -> FreeReal[n - 16] at cFSMOrder +0xC8; the one running +0x158, which its frame
  FUN_00C2D4D0 counts **up**): nothing shown, not the HUD's countdown.
- **The mod** (`kSigStageCountdown`: 2 matches; `kSigHudCountdown`, `kSigAreaFrame`): the areas' list, count and
  frame slot out of sArea's frame (checked to be sArea's slot 6); for each match the area class whose frame starts
  nearest before it (within 0x400) and calls the HUD's countdown within 0x100 after it; what it runs down to out of
  the frame's test before it (`movss xmm1,[ebx+floor]; movss xmm0,[ebx+time]; comiss; jbe`, or `movss
  xmm0,[ebx+time]; xorps xmm1,xmm1; comiss; jbe` for 0); the HUD's most out of its setter. **Freeze countdown timer**
  makes each frame's `subss xmm0,[eax+28h]` (5 bytes) nops: the seconds stay, the clamp and the time-out are the
  game's (a scene that zeroes it still ends it). The tick finds the stage among sArea's areas by its frame (the
  area's vtable's slot), running while its seconds are above what it runs down to: the panel's "Time left (s)"
  beside the switch, and its edit at the pause - written to the stage's field, kept a second above the floor (at it
  the game takes it for run out) and within the HUD's most. The switch is shown in every game now (it was under "The
  Mercenaries", shown only there). Logged: each stage's countdown found at the start, "cheats: aS209's countdown is
  running, m:ss left (frozen)" as one starts, "cheats: the countdown is over" as it goes, each edit. The Mercenaries'
  clock has no editor (aGame +0x4DC: not built). Built, tested offline (`tests/test_cheats.cpp`, the discovery's
  reads and checks replayed over the dump) and installed; not seen in play.

## The action buttons (Automatic QTEs)

- Every prompt on screen - a door's Open, a melee move, a QTE's buttons - is a **request** (`cActionButton`, 0x7C
  bytes) made by FUN_00764480 (19 arguments: the owner +4, a byte +8, the player +0x10 (4: any, the owner's callback
  +0x5C picks), the **category** +0x1C, a word +0x14, kind << 8 | **type** (+0x18, +0x20), the **label** +0x24 (a
  line of `mes_system_e`: 24/26/30/35 Dodge, 29 Duck, 13/28/190 Break free, 43 Help ...), a count/time +0x28, the
  callbacks +0x3C.. called on success; +0x6C strict (a wrong button fails the player: +0x70 bit), +0x74 seconds up;
  280 call sites; the event player's QTEs go through 0x764500, a struct of the same). sActionButton's frame
  (0x7676E0) updates each request (0xBC30E0), then each player's **ButtonMgr** (0x2D0 bytes, 4 of them) takes up to
  two of the requests aimed at it into its two units (+0x230, 0x48 each: +4 in use, +5 drawn, +0xC the request,
  +0x10 the kind) and **checks** them, the lower category first (FUN_00767390 calls FUN_00766F00, thiscall `(unit,
  pad, player) -> pressed`, `ret 8`, at 0x767576); "pressed" sends it over the network and calls the request's
  success (0xBC3360: +0xB/+0xC set, the callbacks).
- **Types** (+0x20, the mask table 0xF58E50, the icons 0xF590B8): 1-4 X Y A B, 5-8 LT RT LB RB (the plain icons of
  the context prompts), 9-0xC X Y A B and 0xD-0x13 two at once (the QTE icons), 0x14/0x15/0x1A-0x1C the stick, 0x16-
  0x19 X Y A B to mash or hold, 0x1D BACK+X. **Kinds** (+0x18; the check's switch): 1 a press (0x763720), 2 a
  release, 3 two at once in a window (0x763940), 4 a mash to a count (0x766320), 5 a hold (0x763B60), 6 the stick
  shaken and buttons mashed to a count (0x7664A0: the grabs), 7 the stick turned round (0x763D00: the cranks - see
  "The cranks" - and a kick's Push), 8/9 the stick held up or down / to one side (0x763EE0: 8 uOmSwitch3's, category
  9; 9 the laser mirrors' turning, uOmLaserMirror's cActPlCatch - Turn, category 8 - and cActPlTurn, category 2). A
  dodge's buttons come from FUN_00763200 by the player's control type: 0x313/0x30F/0x30E (kind 3), or 0x109/0x10B/
  0x10C (kind 1, QTE icons).
- **Categories** (+0x1C, from the 280 sites): 1 the cutscenes' QTEs (the event player, 0x7B2D60: its buttons from the
  table 0x10E86C0 by level) and "Climb up"; 0, 3, 4, 0x3C Break free (kind 6/5: Majini grabs, 20-50 shakes); 6, 0xB,
  0xC, 0x12, 0x13, 0x14, 0x16 dodges, ducks, Run, Land; 0xE and 0x19 the bosses' and enemies' QTEs (Dodge, Hold, Tear
  off, Cut, Counterattack, Left hook, Restrain, Inject, Uppercut - and, in 0x19, the melee moves with plain icons); 0x10
  dodges, Duck and Grab - and a vehicle's Switch spots (plain B); 8 the volcano boulder's Push, Push, Hook and Ram
  (0xB2B620, uOm095e_Kick::cActPush - see "The boulder") and a laser mirror's Turn (kind 9); 2 Cancel, a crank's Release
  and stuck doors; 5 Start, Play (FUN_00B2E150, uOm0201_Piano's) and a crank's Turn (the category's only kind 7); 0x33 a
  crank's own prompts to take hold of it; 0x11 Help (the partner grabbed; QTE icon B), 0x17 Heal partner, 0x18/0x3F
  Resuscitate; 0x21 the context actions (Investigate, Open, Pick lock, Push, Command partner ... and one Duck); 0x15,
  0x1B, 0x1F, 0x20, 0x29.. other context actions.
- **The game's Action Assist** (options page `uIdOption04`, its four settings words of cSystemInfo +0x30..+0x36;
  `mes_system_e` 2501 ACTION ASSIST, "Adjust the action button event playability.", 2328-2330 AUTO, ASSIST, OFF):
  **cSystemInfo +0x32**, 0 AUTO, 1 ASSIST, 2 OFF (written by 0xA2CC80; cSystemInfo's constructor zeroes it).
  Read only through FUN_007B6B60 ("AUTO?") and FUN_007B6B40 ("not OFF?"). AUTO: the check's tail (0x76714F) answers
  "pressed" for a local player's (+0x2DA8 0) categories 4, 6, 7, 0xB, 0xC, 0x14 at once, 0x21 only for label 0x1D,
  1 a quarter second in (5 s for label 7 in stage 0x1FC), 8 for labels 0xB5/0xC0 after 2 s - and draws none of
  them; kind 1's categories 0x26/0x27 take a held button; FUN_00B2B620 and FUN_00B2E150 make their prompts one plain
  press, not strict; a gauge (FUN_00B05E10) halves. Not OFF: the event QTEs' easiest buttons (0x7B0FA0), a grab's
  steady buttons and doubled time (0x900C50), the dying player's timers 0 (0xCB02F0, 0xCB0350, 0xCA6C50/90), the
  dodges' windows (0xB05F80).
- **The mod** (`kSigQteCheck`, `kSigQteType`, `kSigQteKind`, `kSigQteAuto`, `kSigQteCall`, `kSigAssistAuto`,
  `kSigAssistOn`): both tests become `mov al,1; ret` - the game's AUTO, the saved setting untouched (the options
  page reads the word itself) - and the managers' call of the check goes to `hk_qte_check`, which answers "pressed"
  (and not drawn, as AUTO) every frame for a request aimed at a character this machine's pad drives whose type is a
  QTE icon (9-0x1C), whose kind is not the stick held or turned (7-9) and whose category is 0, 3, 4, 6, 7, 0xB, 0xC,
  0xE, 0x10, 0x12, 0x13, 0x14, 0x16, 0x19 or 0x3C - or a crank's Turn, kind 7 in category 5 (2026-10-31, "The
  cranks"); the volcano boulder's Hooks and Ram (category 8, labels 0xB7 and 0xB6) it answers, hidden, once each
  has been up 0.75 s, a press of the player's own still counting (2026-11-01, "The boulder"); anything else goes to
  the game's check. Not seen in play.

## The cranks (Automatic QTEs) - built 2026-10-31

The user (2026-10-31): "Update the QTE cheat so that it also does the required buttons for turning cranks (A + D). It
doesn't need to automatically leave the door when finished, as sometimes we want to wait to release, but it should
automatically get it fully open".

- **uOmCrank** (< uObjModel, 0x1A60 bytes; its constructor FUN_00AB0E60, its start FUN_00AB3A60): a gate's or a door's
  crank, open as far as it is wound. Its classes: uOm0872, uOm021b, uOm021e, uOm021f, uOm0220, uOm0221 (a ring of 24,
  flags 0: wound back when let go), uOm08a6 (32, flags 2: kept as far as it got), uOm08df and uOm08eb (32, flags 4:
  locked open once wound; top speed 2). Its fields: +0x1970 how far it is wound (a float; +0x17FC a copy), +0x1974
  its most (FUN_00AB28F0 from +0x199C, 6 by default, 3 for uOm08eb: a Havok one's x 50, a model's x half its winding
  motion's frames x 0.5), +0x197C the share x 100; **the ring of turns** - its size +0x1984, its bytes +0x1988, the slot
  +0x198C, the clock +0x1990 - and **+0x1994 "turned"** (a byte); +0x1998 the top speed (1); +0x19A4 flags (bit 0 left
  open, bit 1 never wound back, **bit 2 locked open once wound**, bit 12 being let go); +0x19F5 locked; +0x1935 the
  player at it (4 none).
- **Its own actions** (uOmCrank::cAct*): cActWait (FUN_00AB4020: the prompts to take hold of it - category 0x33, a
  press: Turn (0x29) and Command partner (0x72), Ready trap (0xF9) in stage 705, none in 201 - the ring cleared);
  cActWind (FUN_00AB41C0: step 0 gives the player cActPlWind and clears the ring; then each frame its speed
  (FUN_00AB2750) into its own motion's (+0x3A4) and the winding FUN_00AB11A0 - wound += its time factor (+0xFD4) x
  the frame time x speed x 30, up to the most - and at the most its sounds, done); cActRewind (FUN_00AB2AB0, let go:
  wound back to 0, the door closing - unless bit 1, or bit 2 with the crank at its most: then bit 0, left open).
- **The player's** (uOmCrank::cActPlWind, 0xB0 bytes, its frame FUN_00AB4790): step 0 makes **Turn** (category 5,
  kind 7, button 0x1C, label 0x29; its success FUN_00AB20F0 sets +0x1994; offered (FUN_00AB2130) while not being let
  go, not locked and below the most) and **Release** (category 2, kind 1, button 1, label 0x93 'Release'; its success
  FUN_00AB21B0 lets go - FUN_00A83370(player, 1), bit 12; offered (FUN_00AB2230) while not being let go and, for a
  bit-2 crank, below the most) - and in stage 705 'Deploy trap' (0xF8, category 0x2E) for the character
  FUN_00B5BED0(1) answers (the partner's, it seems); steps 1-3 walk
  into place and take hold; **step 4** each frame: the player's motion (layer 0, +0x3A4) at the crank's speed; at the
  most (FUN_00AB1200) the motion stopped and, for a bit-2 crank, the player let go by the game (FUN_00A83370(player,
  2): locked, cActPlLeave), else held; its prompts kept up (+0x34 = 1). cActPlLeave (FUN_00AB4FD0) lets go.
- **The ring** (the crank's frame, FUN_00AB1840 from its slot 44): every 1/30 s of frame time (one slot a frame at
  most) the slot moves on, takes +0x1994, which is cleared. **The speed** (FUN_00AB2750; a network partner's its synced
  +0x1800): the share of the ring marked x the top speed (FUN_00AB12D0: +0x1998, x 0.35 for a human in stage 201, x
  1.5 while this machine plays on the keyboard). So a crank winds as fast as it is turned, and a Turn answered every
  frame fills the ring: the game's own top speed.
- **The Turn's check** (kind 7, FUN_00763D00 `(pad, ai, character)`): with a pad, the stick's eight directions (the
  table 0xF59154 of the pad's +0x34 bits), pressed for each step of one or two eighths in the same sense as the last
  (the unit's +0x3C the last, +0x1C the sense); with the keyboard (FUN_00799AF0: no pad) the game's keys 2 and 3
  (FUN_0079A4D0 - A and D, the user's "A + D"), pressed whenever the pair held changes (+0x40); the AI (ai 1) a press
  every [0x11B1794]+0xE08 seconds.
- **The managers** ask their two units' checks the lower category first and answer the first pressed (FUN_00767390):
  Release (2) before Turn (5), so a press of Release always wins. A request not offered is not in the manager's list
  (FUN_00764690) and its check is not asked. Before the success, a pressed answer for a character in play sets its
  +0x2658 unit's +0x19 and, for the AI's, tells its brain (FUN_00B5CB20) - what a player's own fast turning does too.
- **The mod**: `is_qte` takes kind 7 in category 5, the cranks' Turn (the only other kind 7, the volcano boulder's Push
  in category 8, AUTO answers itself after 2 s - "The boulder"; kinds 8 and 9 stay the player's); `hk_qte_check` answers
  it pressed and not drawn every frame for a character this machine's pad drives. The ring fills in about a second, the
  crank winds at the game's top speed to its most (2.5 s for a most of 90 at the keyboard's top speed, in the test's
  model of it), where the Turn is no longer offered and the player holds it - Release theirs, as the user asked (a bit-2
  crank lets go by itself, as the game always has it). The first ten Turns answered are logged: "cheats: a crank's Turn
  answered for character 0 - it winds itself fully open; Release is the player's". Built, tested offline
  (`tests/test_cheats.cpp`) and installed; not seen in play.

## The boulder (Automatic QTEs) - built 2026-11-01

The user (2026-11-01): "the automatidc QTE doesn't work for the final level when pushing the boulder".

- **uOm095e_Kick** (< uOmS508_Break < uObjModel, 0x19C0 bytes; model om_rockA01_02, `Image\Archive\uOm095e_Kick.arc`)
  is the boulder of stage 508 - the volcano, chapter 6-3's last stage but one (the chapters' stages, "The countdowns":
  chapter 15 is 511, 512, 505, 612, 506, 610, 508, 611). Its stage's other uOmS508_Break pieces are uOm095a_Jump,
  uOm095b_Cliff, uOm095c_Break and uOm095d_Stampede.
- **Its actions** (by the pawn's action number +0x10E0, set through slot 55; the pawn's action update FUN_00B56480 ends
  and starts the action afresh whenever one is set, the same one too): 0 **cActWait** (FUN_00B2B470: the context
  prompt that starts it - category 0x21, a press, label 0xC0 Push: the player's, as a door's Open); 2-6 **cActPush**
  (its start FUN_00B2B620 makes the step's prompt, all category 8 for player 0, its success FUN_00B294F0 -> the
  boulder's slot 105 FUN_00B29470 -> its events FUN_00B28B70); 1 and 7 **cActMotion** (1 the push failed: the
  player's motion, then back to 0; 7 the boulder over: FUN_007723E0's scenario cue for stage 0x1FC). The player's
  **cActPlAct** (its frame FUN_00B2BE70) plays the step's motions from `om095e_pl` (in the same archive; 30 fps): 2 ->
  0 (96 frames) then 1 (313, looped), 3 -> 2 (101) then 3 (194, looped), 4 -> 4 (78) then 1, 5 -> 5 (66) then 1, 6 -> 8
  (153), 7 -> 6 (621), 1 -> 7 (146). A Hook's own motion runs 2.2-2.6 s; the last mark of its event timeline (the
  0x100000 bit) is at frame 36 (43 for the first) - five whole Hooks would not fit their 10 s.
- **Its steps** (the boulder's properties, the defaults of its start FUN_00B299A0 - no archive sets them: none names
  `mABPoint`): **mABPoint** +0x1994 the points left and **mLimitTimer** +0x1990 the seconds left, taken at each step's
  start from mABPoint_PushA +0x1998 (30), _PushB +0x199C (90; 60 when character 0 is the AI's), _Punch +0x19A0 (5) and
  mLimitTimer_PushA/B/Punch +0x19A4/+0x19A8/+0x19AC (10 s), _Upper +0x19B0 (2 s):
  - 2 **Push** - a button held (kind 5, button 0x16, label 0xC0 Push; the hold's check FUN_00763B60 answers each new
    press while it is held): each success takes a point - five while a pad plays (FUN_00799AF0 no), one on the keyboard;
  - 3 **Push** - the stick turned round (kind 7, 0x1C, label 0xB5 Push): the same;
  - 4, then 5 for each next - a **Hook** (label 0xB7): a mash of X, A or B at random (kind 4, 0x416/0x418/0x419) to 4,
    5, ... 8 presses; with AUTO (and a local player) one plain press (kind 1, button 1, 0x101). Each success takes a
    point and sets action 5 again - the next Hook's prompt made afresh (its clock at 0), the player's punch motion
    started again; the limit is set at step 4 only (10 s for the five);
  - 6 the **Ram** (label 0xB6): two at once (kind 3, 0x30F); with AUTO one plain press (0x101). Its success: 7.
  - Its frame (slot 88, FUN_00B28A30; for character 0, not the network partner's): while points are left the limit
    runs down (the time factor +0xFD4 x the frame time); none left -> the next step (2 -> 3 -> 4 -> 6); the limit out
    -> 1, failed.
- **AUTO** (FUN_00766F00's tail; the mod makes "AUTO?" yes): category 8 with label 0xB5 or 0xC0 - the two Pushes -
  hidden, and pressed every frame once the request has been up 2 s (its +0x74, counted by the request's frame
  FUN_00BC30E0 while it is offered, zeroed as it is made by FUN_00BC2DA0). The Hooks and the Ram it does not answer:
  their one plain press is the player's. So the build before this one pushed by itself, then showed "Hook" and waited;
  with nothing pressed the Hooks' 10 s ran out and the push failed (its motion, then the boulder to take hold of
  again) - the user's report. The labels are never remapped (FUN_00BC2DA0's tables at 0x113C158 and 0x113C110 remap
  some melee labels by the character's class, none of these).
- **Category 8's prompts**: the 280 sites have two - this one and a laser mirror's Turn (0xB1627B, kind 9, label 0x46).
  The data-driven ones: the scenario's FuncParamActBtn order (FUN at 0xC32064, its category from +0x24) is used by
  stage 100's Executioner scripts only (`mBtnType` in s100's `s100_butch_all`, `s100_butcher`, `s100_butcher_True`; no
  other stage's FSM has it); the event player's (0x764500's callers) are categories 1, 0x1E and 0x3F.
- **The mod** (`kSigQteAutoPush`: AUTO's branch for the two Pushes - their labels 0xB5 and 0xB5 + 0xB, read and checked,
  the unit's drawn flag, the request's seconds up +0x74; within 0x100 of `kSigQteAuto`, whose +40 gives the request's
  label +0x24): for category 8 with label 0xB7 (Hook) or 0xB6 (Ram), aimed at a character this machine's pad drives,
  `hk_qte_check` asks the game's check (a press of the player's own still counts), hides the prompt, and answers pressed
  once it has been up **0.75 s** - each punch's motion runs that long before the next starts it again, the five Hooks
  and the Ram in about 4.5 s (the Hooks' limit 10 s, the Ram's 2 s). The two Pushes stay AUTO's, as the game paces them
  (2 s in, then every frame: about 6 s for both on the keyboard at 60 fps). The whole push, from taking hold, ~10 s. The
  first twelve answers are logged: "cheats: the boulder's Hook answered for character 0, up 0.77 s" (Ram the same).
  Built, tested offline (`tests/test_cheats.cpp`: the boulder run as the game runs it, keyboard and pad at 60, 144 and
  20 fps) and installed; not seen in play.

## The map (Open map at chapter start) - built 2026-11-01

The user (2026-11-01): "add a new cheat to auto-open the map on the start of a new chapter".

- **A character's map** is a unit of its own, `uIdMap00` (0x1B00 bytes, < cUnit; no code names its record - the
  maker calls its constructor directly), made by FUN_00A149F0 (its constructor FUN_00A134C0; line 27, flag 0x2000)
  from a character's start (FUN_00B7E4B0, in every area: the characters are made anew) into **character +0x264C** (the
  HUD +0x2640, the scope's +0x2644 beside it): its player +0x6D4 = the character's **+0x2D9C** (a byte, the player's
  number), its character +0x6D0, +0x6DC = (+0x1AF9 == 1). Every character has one; the frame returns for one this
  machine's pad does not drive (+0x2DA8), so the AI's and the network partner's are never shown.
- **Its flags**: +0x14 its step (0 waiting for its layouts - two `id` resources, +0x20/+0x24 from the player's number
  + 0x2D/0x31 - then set up by FUN_00A13A50; 1 running), **+0x1E open** (the player's toggle), **+0x1D the toggle
  mode** (its constructor sets 1 and nothing clears it; without it +0x1E follows the key held), +0x19 forced open (the
  scenario's: the key's next press shuts it), +0x1A kept shut (the scenario's: the key does nothing and the map is
  hidden), +0x18 shown this frame, +0x1B hidden (a change plays the map's sound, 0x54/0x55 - 0x11C/0x11D with +0x6DC).
- **Its frame** (slot 6, FUN_00A16E20, its body laid out before it): unless sBioMain+0x1043C has 0x8000, for a
  character in play this machine's pad drives - shown when FUN_00A13240 lets it (the unit running with its +4 bits 11
  and 14 - the game sets and clears 0x800 of it around some scenes: 0x916B09, 0x91D7AC, 0xACC98A, 0xB2193C, ... -,
  sBioMain+0x10438 none of 0x400/0x200/0x20, +0x1043C not 0x1000, the character not scoped (+0x2998) nor busy (+0x10ED,
  +0x2D7C bit 8, its +0x2654's +0x68 1), two tests by the player: FUN_007162D0 no, FUN_0076C740 yes) and what the
  character's slot 53 answers has its +0xFB4 set, or forced, or open and not kept shut. The key (the pad's pressed
  buttons +0x3C, bit 0x200; held +0x34): in the toggle mode a press shuts a forced map (both cleared) or else flips
  open unless kept shut, then **keeps it in the play state: FUN_00C42D70** (thiscall on cGameSys, `(player, open)`,
  ret 8: `cGameSys +0x554 + player` = open, players 0-4).
- **The play state keeps it**: cGameSys (sBioMain +0x1042C, 0x7A8 bytes) +0x554, a byte a player (five). The map's
  setup ends by taking its open flag from there (FUN_00C42D50, `(player) -> open`) while in the toggle mode, so a map
  opened stays open in the next area (a new unit, the same byte) and one shut stays shut. Cleared only by cGameSys's
  reset (FUN_00C43A00: aDemo's start for the reasons 0, 1 and 8 - back to the title or the start-up menu -, the
  start-up menu's step 5, sBioMain's start), the Mercenaries' load (FUN_007173B0, modes 1, 2 and 5, every player) and
  No Mercy's start (FUN_00769610, players 0-1). Nothing between a chapter's results and the next chapter: a chapter
  begins with the map as the last one left it, shut when it comes from the menus.
- **The scenario's Map order** (cFSMOrder::FuncParamMap, 0xC bytes, at 0xC2BF80): +8 the player (4 all), +9 the kind
  (0 forced open, 1 kept shut), +0xA the value; another order (0xC307A0) sets the map's points (+0x1AEC, three).
- **Its setup** (FUN_00A13A50) gives no map in stages 109 and 111 (the jeep's; mTempBase.StageNo 0x6D, 0x6F: the unit
  never runs there), special ones in 200 (the airboat), 204, 107, 500 and 508; in mode 1 four players' markers.
- **A chapter's start** is cGameInfo's **mTempBase.mChapterStart (+0x41C4)**: set as a chapter is set to begin from
  its beginning - CHAPTER SELECT's chapter list (uIdStartUp05's frame FUN_00A3CCC0 -> sInfoMan's FUN_007B9140:
  ChapterNo from cGameSys +0x5C, StageNo, StartPosIdx and SceNo from the chapter table 0x10CABD0 (8 bytes a chapter),
  then 1), the extra episodes' start (FUN_00BF55E0: chapter 0, 1 - called only from FUN_00BF9560, their flow; this
  note first had it NEW GAME's, corrected 2026-11-03), the next chapter after the results (FUN_00C41A60: ChapterNo + 1 and
  its first stage, 1) and cGameInfo's take-back (FUN_00C42610: the play flow's quit, the story flow's start, a load -
  by the stage table 0x10CAFB0's +5 for the saved stage and chapter: CONTINUE from a chapter's start); the Mercenaries'
  rounds by its setter (FUN_00726990, from 0x71F94F, 0xBF6264, 0xBF6BE3); three other menu setups clear it
  (FUN_00BF4BD0, FUN_00BF5050, FUN_00BF6520), and No Mercy's start. The play flow's copy of cGameInfo (aGame +0x130)
  carries it, so a RESTART or a death's CONTINUE back to a copy made while it was set would start the chapter over,
  title and all (whether one is made while it is set was not traced). **Cleared** by the play flow's FUN_00719A40 as play begins
  (FUN_0071DCA0 - from aGame's step 2, FUN_00722A50, not in stages 600-699 nor 803 -; after an opening cutscene,
  FUN_00722D70's step 2; the Mercenaries' FUN_0071F840): unless sBioMain+0x10438 has 0x20, the chapter's title
  (`uIdChapSt00`, FUN_009EAF80, line 27) for each player in play (FUN_00C42B90; four in mode 1) into aGame +0x108..,
  then the field 0.
- **The mod** (`kSigChapterStart`, `kSigMapRestore`, `kSigMapToggle`, `kSigMapMake`; inline: the getter's and the
  setter's bodies at their calls - the same bytes, the same last player -, the maker's size and its constructor's
  store of uIdMap00's vtable, the title's class record moved in before the maker's call, the field's clear in the same
  function; the toggle's flag four times, its player and cGameSys the restore's and the maker's): the tick notes the
  profile's mChapterStart set; when it next sees it cleared, with characters in play, cGameSys's mode (+0x58) 0, 3 or
  4 (the story, Lost in Nightmares, Desperate Escape - below) and the switch on, each character this machine's pad
  drives with its player's number below five gets
  `cGameSys +0x554 + player` = 1 and, when its map (character +0x264C) is a uIdMap00 in the toggle mode, its +0x1E = 1
  - what one press of the key leaves. A map not set up yet takes it from the play state as it sets up. Cleared out of
  a game (the menus) it only lets the start go. The scenario's forced and kept-shut flags stay the game's (a map kept
  shut shows once let go). Field writes only. Built, tested offline (`tests/test_cheats.cpp`, `tests/test_panel.cpp`),
  the discovery replayed over the dump (cGameInfo +0x41C4; cGameSys sBioMain +0x1042C, mode +0x58, the bytes +0x554,
  five; uPlayer +0x264C and +0x2D9C; uIdMap00 +0x1E, +0x1D, +0x6D4); not seen in play.
- **The extra episodes** (2026-11-04; the user: "does the open map at chapter start not work for the extras game
  modes?" - their log of that day: Lost in Nightmares begun with the switch on, and nothing). The mod tested the
  story's mode alone. The episodes begin as a chapter does: the story menu's choices 8 and 9 (FUN_00BF9560, mode 3 or
  4) end in FUN_00BF55E0, which sets mChapterStart (and ChapterNo 0); their stages are 700-706, which FUN_0071DCA0's
  test lets through (it leaves out 600-699 and 803), so FUN_00719A40 clears the field as play begins - after the
  opening cutscene, as in a story chapter. Each character's map is made and set up as in the story: FUN_00A13A50 tests
  no mode but Versus' (1, four players' markers), and 700-706 are none of its special stages; its end takes the open
  flag from cGameSys +0x554 the same way. So the mod now opens the maps for modes 0, 3 and 4 (`chapter_mode`), the
  Mercenaries (2), Reunion (5) and Versus (1) still left alone. A start that opens nothing (the switch on) is logged,
  the first ten: "cheats: a start in the game's mode 2 - no map opened (only a story chapter's or an extra episode's)",
  and one cleared with no character in play "cheats: a chapter's start cleared with no character in play (the menus let
  it go) - no map opened" - after an episode's start, that line would mean the clear came between two areas (in its
  log, the episode's opening cutscene ran with characters in play, then an area changed: "game ended", "game started"
  1.2 s later). Built, tested offline (`tests/test_cheats.cpp`: both episodes, Reunion, Versus, an unknown mode); not
  seen in play.

## The Mercenaries (Endless combo, Combo Bonus always on, Kill enemies as they appear) - built 2026-11-02

The user (2026-11-02): "it doesn't appear we've added cheats for the mercenaries mode yet. Compare with the RE4 mod to
see which ones to make". RE4's (the remake's) are Endless combo, Mercenary Mode always on, Kill enemies as they appear
and Freeze countdown timer; RE4 (2005)'s Endless combo, Kill enemies as they appear and Bonus time always on. RE5's
Mercenaries is built as the 2005 game's was: a combo whose bonus is banked at its end, and a 30-second Combo Bonus
pickup (mes_item_e 461 "Combo Bonus") - the counterpart of Mercenary Mode and of Bonus time. Freeze countdown timer
was there already (no editor for the Mercenaries' clock: RE4's rule, aGame +0x4DC).

- **The modes** (cGameSys +0x58): 2 The Mercenaries, 5 Reunion (FUN_00C42FD0 maps every player to score slot 0 in
  both: one score and one combo for the team), 1 Versus (+0x5CC its kind: 0 Slayers, 1 Survivors, 2-3 the teams'), 3
  and 4 the DLC episodes (3 Lost in Nightmares, 4 Desperate Escape; each with an item box of the mod's own - "The
  extra episodes' item boxes" in items.md). cGameSys +0x56C an online game; +0x574 == +0x578 where this machine runs
  the match (the host, or offline) - the game's own scoring and clocks act only there.
- **The play flow's Mercenaries frame** (FUN_007245D0, aGame): its step cGameSys **+0x67C** - 0 the count-in (aGame
  +0x4BC), 1 play, 2 time up or the stage cleared (FUN_00792830, a message: +0x680 the clock, +0x690 the clear's time
  bonus), 3 the end (the same message's second form: each running combo ended through FUN_00717BD0, the Combo Bonus
  off; then FUN_00722780 the final score: slot 0's +0x6B8 plus +0x690, never what a running combo was still waiting
  for, but the end above banked it). Each frame not paused (sBioMain+0x10438 0x800): the clock (FUN_00717850, below),
  the HUD's copy **+0x680**, and for each player of +0x478 the combo's clock (FUN_0071C350), the Combo Bonus' clock
  (FUN_00717C60) and Versus' respawn (FUN_007225E0).
- **The clock** (FUN_00717850): aGame +0x4DC less the frame time (`kSigCountdown`'s store), copied to +0x680 offline;
  in modes 2/5 the stage is cleared (+0x4D4 = 1) once the stage's kills **+0x58C** reach **150**, or 400 with the byte
  +0x79E (`kSigMercClear`: `(-(+0x79E != 0) & 250) + 150`). Clear and time up alike: +0x690 = time left x 1000 (300 in
  mode 5) / 100 x 100 + 1..9.
- **A score slot** (0x40 bytes from cGameSys +0x694, slot n at n << 6): +0x694 kills, **+0x698** the combo running
  (a byte), **+0x69C** the combo, +0x6A0 the best combo, **+0x6A4** its clock (seconds), **+0x6A8** the Combo Bonus
  running (a byte), **+0x6AC** its clock, +0x6B0 kills in it, **+0x6B4** what the combo has earned (not on the score
  yet), **+0x6B8** the score, +0x6CC/+0x6D0 Versus' points shown. The HUD (uIdMerce00) reads them (FUN_00A1AAE0 the
  score's count-up, FUN_00A1AD10 the combo's +0x6B4 at each kill, its "+N" at the combo's end, FUN_00A1B040 the Combo
  Bonus' display while +0x6A8).
- **A kill's scoring** (FUN_0071CD10, `(slot, points, column)`): score += the enemy's points; kills, combo +1; the
  combo's clock = **10 s** (0x10CBB4C), running; its bonus by the combo (mode 2's table 0x10CBBC0: 0 for one kill,
  then 20, 50, 100, 200, 250, 300, 350, 400, 500 from 10, 600 from 20, 700, 850, 1000 from 50 - the column 1 values,
  500-7000, for the big ones; Reunion's table 0x10CBC90 goes on to 1200, 1400, 1700 ... from 60, 70, 80); while the
  Combo Bonus runs a bonus above 0 is at least **1000** and +0x6B0 counts it; the bonus onto +0x6B4. The points
  (FUN_007228F0, the table 0x10CBB58 by the enemy's class number): 300 for a Majini, 400-1000 for the others, the
  column 1 ones 5000 (uEm20, uEm24, uEm32) to 15000 (uEm26 the red Executioner), 2000 for a chicken (uEm62).
- **A kill's report**: an enemy's damage handler (FUN_009E0840) reports its death through FUN_00721C50 - once (the
  enemy's +0x191C): the record {its points' class number, its kill record (FUN_007B8D70), +0x1F41, the attacker's
  player bit (the hit's attacker resolved by FUN_00C49530: the unit +0x20 or its owners, its +0x2D9C), the weapon's
  kind (FUN_007172A0: the hit's class id +0xA4 - the attacker's own class: hand-to-hand, 0; a weapon's 0x07cccc00: the
  item cccc's kind, the table 0x10CB220), ...} to FUN_007916A0 (offline, or the host) -> FUN_00786010 -> for each
  player of the record's bits FUN_007243A0: +0x58C +1, the score through FUN_007228F0 and FUN_0071CD10, **+5 s** for a
  hand-to-hand or stun rod kill (kinds 0 and 0x18; FUN_0071CBF0: aGame +0x4DC, then FUN_00718050's +0x680 and the
  HUD's +0x68C/+0x688), the records. Scripted deaths report through FUN_00722070 (the enemy's +0x136C: the last
  player to hit it). Animals count too (uEmAnimal: the crows, the chicken ...): their brains run the same damage
  check (uEmIntel62's slot 27 FUN_009B3230, uEmIntelAnimal's slot 30 FUN_009D4540), so their deaths report and count
  towards the stage's kills like an enemy's (FUN_00717AA0, the report's gate, asks only the mode and the play flow).
- **The combo's clock** (FUN_0071C350, `kSigMercCombo`): while +0x698, `clock - frame time`; at 0 (where this machine
  runs the match) FUN_00717BD0 (`kSigMercComboEnd`): the best kept, the combo 0, **+0x6B4 onto +0x6B8**, not running.
- **The Combo Bonus' clock** (FUN_00717C60, `kSigMercBonusClock`) the same on +0x6A8/+0x6AC; at 0 it stops. A pickup
  (FUN_0071CC90 -> FUN_00718100, `kSigMercBonusStart`): +0x6A8 1, +0x6AC **30 s** (0x10CBB50), +0x6B0 0 - and a
  network message (FUN_00791130 0x1E).
- **Endless combo**: the combo clock's `subss` (6 bytes, `kSigMercCombo` +41) becomes nops - the clock stays where
  the last kill set it (10 s) - and the tick moves +0x6B4 onto +0x6B8 each frame (the game would bank it only at the
  combo's end). The HUD's "+N" at a combo's end then shows only the last kill's bonus (already on the score).
  Switched off, the clock runs again and the combo ends 10 s after the last kill with nothing left to bank.
- **Combo Bonus always on**: the Combo Bonus clock's `subss` (`kSigMercBonusClock` +47) nops; at the step 1 with time
  left, a slot 0 without it running gets the pickup's three writes (field writes; no message - offline only).
  Switched off, 30 s of it run down. The end (step 3) turns it off; the tick does not start it again.
- **Both act** only in a match (the HUD uIdMerce00 up, mode 2 or 5, the step 0 or 1), offline and where this machine
  runs it; the patches go in and out with that.
- **Kill enemies as they appear - the hit.** Every pawn carries a `cHit` (+0x1290; 0x74 bytes) whose record (+0xC, so
  pawn **+0x129C**; 0xE0 bytes, made with the pawn by FUN_00C43E80) the hit system fills (FUN_00C460E0 from a damage
  shape's callback FUN_00C47560): place +0x0, the part's centre +0x10, the attacker unit **+0x20** (+0x24 its +0xFD0),
  damage **+0x28** (summed; +0x2C the most of one), the attack data's +0x34/+0x38/**+0x3C** (flags; bit **0x10000000**
  the whole health, the game's instant kill - FUN_009DEA50 at 0x9DEAD6)/+0x40/+0x44, hit **+0x48**, the part **+0x49**
  (its index in the cHit's list, 0xFF for none), +0x94, +0x98, the attack's number **+0x9C**, +0xA0 its priority, the
  attacker's class id **+0xA4** (a pawn's +0x1920; a bullet - uWpShell/uWpShellCommon - carries its gun's, written at
  0xBA333A). The enemy's move (uEnemy's slot 44, FUN_009D9D90: uCharacter's move, then FUN_00C485A0 clears +0x48 and
  the attackers) runs its action's damage check (FUN_00963C90, ~40 callers by class: the record, merged with its four
  parts' at +0x1BFC - the Reaper's soft body and the like - the most damage kept) and, when FUN_009E0700 lets it (a
  hit, the attacker's no-hit timer +0x1DFC, not +0x1E24, the network's owner), FUN_009E0840: the brain's hit (+0x1B68
  slot 39), slot 131's damage, FUN_009DEA50 (slot 129's check by part, FUN_0071B010's dynamic difficulty, the
  whole-health bit, subtract-health, the death: +0x1910, set-health 0), the kill's report, slot 130, the effects, slot
  128's reaction (uEm10's: slot 127, FUN_00861160 - a sound by FUN_007E5FA0(+0xA4)'s weapon kind; its part lookups
  FUN_00C44150 bounded: kind 0 out of range). **A hit from the other machine** (FUN_0078D590, a message) is written by
  FUN_00B550F0 into a record of these fields alone - place, damage, 0, the attack data (FUN_008237D0(number): +8, the
  message's float, +0x10, +0x14, +0x18), hit, part, 0, 0.0, the number, the attacker (the player's character,
  FUN_00786D80 by handle), the class id, the part's centre or the enemy's place (+0x30) - and run through FUN_009E0840
  as a local one.
- **The mod** (`merc.cpp`, field writes only): each frame in play (the clock running with time left, step 1, not
  paused, 12 frames after a pause or a load), this machine's first character with health is the shooter; the gun in
  its hand (the case's item with EquipType 1, its class (7 << 16 | item) << 8 a uWeapon but no knife, stun rod,
  grenade, mine, launcher, shell or mounted gun) or else the M92F (0x102); its bullet's attack number from the guns'
  table (0x11B5170, 0x140 bytes an entry by the item's low byte, the number at +8: `kSigShotTable`/`kSigShotAttack`)
  and its data from the attacks' table (0x1115280: a pointer and a count a group, 0x1C bytes an entry; the data's
  vtable HIT_PARAM's - M92F: attack 2, 0.1, 1.0, 0x80000, 0x200). For each enemy - a running uEnemy with health (the
  chickens among them since 2026-11-03), not uEmNpc, not in another's parts (+0x1BFC, four) - whose record holds no
  hit of the game's own, at most
  8 a frame, again after 400 ms while it stands, after 25 tries every 5 s (logged; until 2026-11-03 it was left
  there for good - see the Chainsaw Majini below): the network path's fields
  (`kSigHitRebuild`): the place a twentieth of the way from the enemy (+0x30) towards the shooter, the centre the
  enemy's place, damage its health, 0, the attack data with the whole-health bit, 1.0, part 0xFF - a Reaper's first
  weak-spot part (27, `kSigReaperHit`) -, 0, 0.0, the number, the shooter, the shooter's own class (below; the gun's
  before 2026-11-03), then hit. Its move takes it in its damage check: the game's death, the kill reported for the
  shooter's player. An enemy in a state whose action runs no damage check keeps standing until the next try.
- **Hand-to-hand kills, for the time** (the user, 2026-11-03: "make the kills using knife for the bonus time"). A
  kill's weapon kind is FUN_007172A0's (`kSigMeleeKind`): the hit's class id (+0xA4) equal to the attacking player's
  own class record's id (+0x20, its getDTI's record) is **hand-to-hand** (code 0x100, kind 0); a weapon's 0x07cccc00
  the kind of item cccc (the table 0x10CB220: the M92F kind 1, the stun rod 0x122 kind 0x18, the knives 0x101,
  0x10E, 0x123, 0x124 kind 0x1A). FUN_007243A0 (`kSigMeleeTime`, 0x724433) puts **5 s** onto the clock
  (FUN_0071CBF0) for the kill's first player only when the kind is 0 or 0x18 - so a knife's kill earns no time; a
  punch's, a kick's, a finisher's (and the stun rod's) do. The mod's hit names the shooter's class (units::class_of,
  the id its vtable's getDTI answers - what the game compares): every kill is that character's hand-to-hand one,
  +5 s each, its points and combo as before; the reaction stays the gun's bullet's (the attack data is the bullet's).
  The kind also picks the profile's per-weapon kill record (FUN_0071C130(3, player, kind)): these count as
  hand-to-hand there. With either rule not found the hit keeps the gun's class (shots: no time), and says so.
- **Every area sends.** (Read with a scratch XFS reader over the stages' archives.) The Mercenaries' maps are stages
  400-407 (aS400-aS407); each mode's folder (`soft/merce`, `merceex` - the 400-enemy one -, `mercedc` in s40xSub.arc,
  taken for Reunion) holds `s40x_fsm` (an rScenario) with 1-7 **area tasks** (type 6, each an `AHShapeBox`) that boot
  the FSM `EmSetAreaN` the first time a player is inside; its `AreaCheck` (the AreaHit order, "a player is in task
  6/n") and `EmAdd` (EnemyAdd, FUN_00C2E480: while sEnemy's count +0x24 is below its keep, 9-12 - 30-40 in merceex -
  one enemy from its stock every 0.5-4 s) states alternate, and leaving the box pauses the adding. `StartEmSet` sends
  the first 9-12, `BossEmSet`/`TorchEmSet` go by the kills (+0x58C). A spawn: FUN_007AF6E0(group, id) - offline or the
  host, each once. No camera, distance or cooldown rule. Standing still, only the boxes around the player send.
  The mod: while it acts (the pause too), each area task's box - sScenario +0xD4 + 4 x 6 (`kSigAreaTasks`: a stage's
  own `push 6` to the lists' getter), a list's count +8 and array +0x14 (`kSigTaskLoop`), a cScenario's cAreaHit +8
  (`kSigAreaInside`), its shape +0x14 (`kSigAreaHitFrame`, cAreaHit slot 7) - an AHShapeBox (slot 9, `kSigBoxInside`:
  floor +0x60, height +0x64, corners +0x20/+0x30/+0x40/+0x50, x and z +8) is widened a thousandfold about its centre,
  floor -1e7, height 2e7: the game's own test finds every player in every area (a box with no extent - s405's
  "dummy" - and other shapes left). Each is put back exactly as it stops acting (switched off, time up, out of the
  match), only while the stage still lists it with the mod's values; one a retry or a new stage let go of is never
  written. Scene zones (type 0, s405's traps) and item zones (type 7) are not touched.
- **The animals** (the user, 2026-11-03: "I want to get the animal kills as well, so 150 enemies + all animals for
  a higher possible total"). Read from every layout of s400-s407 (and their Sub archives, every folder): **each
  stage's 150 includes its chickens** - uEm62 (a uEmAnimal; 2000 points), placed only by the start set (`StartEmSet`,
  group 0, `s40x_em00`), each once, never again. Units by stage (non-animal + chickens), merce: s400 149+1, s401
  148+2, s402 155+1 (156), s403-s407 149+1; mercedc: s401 149+1, s402 156+1, s403 150+1, the others 149+1; merceex
  (400): 399+1, but s401 398+2 and s407 390+10. No other animal (crow, perch, snake, crocodile, rat, bat, vulture,
  spider) and no uEmNpc is in any set or layout; the rest of the layouts are hourglasses, barrels, boxes, ladders and
  scenery. A chicken's brain (uEmIntel62) runs the shared damage check, so its death reports and counts like an
  enemy's (one falling below its height limit, FUN_009B22F0, too, with no attacker). So the first build, which left
  uEmAnimal out, left each stage's chicken(s) standing - the user's 149 of 150. Since 2026-11-03 the animals are hit
  as the enemies are and counted as the game counts them: everything a stage holds is killed, and the clear comes at
  its 150. Only s402 (all three folders) and s403's Reunion hold more than the clear needs (the rest stay at the
  clear). Taking a chicken's kill off the count (tried first) would have made most stages' clear impossible.
- **The Chainsaw Majini's second life** (2026-11-03, below). `uEm22` (0x1F70 bytes; health 6500, its constructor
  FUN_0087B010 also sets **+0x1F5C, 2000** - the health it gets up with - and +0x1E3D 0, so the AI partner's hits
  leave it 1). Its start (slot 5, FUN_0087CB40) clears **+0x1F5E** (it has got up again) and gives it a second life,
  **+0x1F5F**, by a rule of its own: with +0xD78 1 when a present player's dynamic difficulty level (FUN_0071AEF0)
  is above 6, always with +0xD78 2 or its set's +0x1F62, never with sBioMain+0x10434 bit 1. So in the user's earlier
  matches the chainsaws died at the mod's hit, and in a later one (the level risen) one got up.
  - Its frame (slot 62, FUN_0087B280; `kSigChainsawLife`) sets uEnemy's **+0x1E38** - "may not die" - every frame
    while +0x1F5F is set and +0x1F5E clear (uEnemy's frame clears it again first, 0x9E0A61). The hit path's tail
    (FUN_009DEA50 at 0x9DEB5A, `kSigDeathGate`) kills a hit's target with no health left only while the network's
    owner says so (FUN_007F2930), +0x1E24 and +0x1E38 are clear (+0x1910 = 1, set-health 0); else set-health(1).
    Only three classes set +0x1E38: uEm22 here, uEm80/uEm81/uEma2 (Wesker; slot 113, always) and uEm83 (Jill; slot
    113, when not the host or FUN_0077E460(10)) - no other enemy of the Mercenaries.
  - Its brain's damage reaction (uEmIntel22 slot 35, FUN_009848A0; `kSigChainsawDown`), with +0x1F5F set and
    +0x1F5E clear and its health below 2 or at most +0x1F5C, picks action **0x20, cEm22ActDieLie** (uEm22's actions:
    the table 0x1116838, 8 bytes each - 0x1F Die, 0x20 DieLie, 0x21 DieBomb, 0x22 DieIce, 0x23 Getup). DieLie's
    start (FUN_0087C1C0) sets +0x1F5E, **set-health(+0x1F5C)** and +0x18D8 bit 2; its frame (FUN_00878700) turns its
    cHit's shapes of kind 1 off every frame (FUN_00C46C70(0): their +0xBD), plays the fall, then waits +0x1F58 (8.0
    s) and ends; its end turns them on again. The brain then picks Getup (FUN_00985BB0: 0x20 -> its plan 7 -> 0x23;
    Getup's frame sets +0x1B5C bit 8, under which the brain's damage and death reactions answer none), and after it
    the chainsaw fights on (its damage reactions now lead to AtkRanbu, 0x16). The mod's hits written while it lay
    there and got up did nothing (in the user's match none killed it in 10 s); only its second round dies as any
    enemy does.
  - **The mod** (field writes): as it writes a hit on a uEm22 with +0x1F5F set and +0x1F5E clear, it clears
    +0x1F5F and this frame's +0x1E38 (set by its frame a frame ago - the brains' damage check may run before the
    enemy's next frame clears it). Its frame then never sets +0x1E38 again, so the hit takes its whole health and
    its death is the game's own (Die, its drop, the kill reported once), as for a chainsaw given no second life. One
    that has got up already (+0x1F5E set) is not touched: it dies at its hit as it is. And since a hit can still find
    one lying down (the player's own shots), an enemy standing after 25 tries is now hit every 5 s (not left), so it
    dies once it is up. The discovery: `kSigChainsawLife` must be a virtual function of uEm22's own (its vtable, not
    uEnemy's at that slot: 62), `kSigChainsawDown` of uEmIntel22's own (35, not uEmIntel's); both read the same two
    bytes; the frame's cHit is the hit record's (+0x1290); the brain's health the characters' (+0x1364); the death's
    test within 0x100 after `kSigWholeHealth` and its "may not die" the frame's; uEm22's fields from uEnemy's end
    (0x1F50) on and inside its size. One check failing names itself in the log, and the second life is left to the
    game (then the hits every 5 s kill it in its second round). One hit kills (`cheats.cpp`) leaves the second life
    to the game: a shot each life.
- **One match** (RE4's rule, the user's there): the switch goes off (and the ini with it) as a match begins, begins
  again (a retry: the step from 1 back to 0) or ends (step 2/3, the HUD gone, out of modes 2/5).
- **The panel**: "The Mercenaries" (a section, folded state kept) only in a match: the three switches, the score (and
  what the running combo waits for), the combo and the best, the kills of the target, the time left, the Combo Bonus'
  time and kills, and with Kill enemies as they appear on the enemies it hits and the areas it has opened.
- **Log**: "merc: the Mercenaries (modes 2 and 5): cGameSys ... ", "merc: an enemy's hit record ...", "merc: a stage's
  areas ...", "merc: the Chainsaw Majini's second life: uEm22 +0x1F5F (it may get up again) and +0x1F5E (it has) ...
  - Kill enemies as they appear takes the life away first", "merc: a kill whose hit names the player's own class is
  hand-to-hand (exe+0x3172A0): 5 s onto the clock (exe+0x324433) ..." at boot; "merc: a match of the Mercenaries
  begins / the match begins again / is over (mode,
  step, kills, score)"; "merc: endless combo: ... as it is earned (first time)"; "merc: Combo Bonus always on: ...
  started (30 s, held)"; "merc: kill enemies as they appear: uEm10 hit (first time) - a hand-to-hand kill for Chris
  (its class 0x1E9D3E0B: +5 s), the hit a shot of item 0x102 (attack 2)", "...
  a uEm34 still stands after 25 hits - hit again every 5 s while it stands" (before 2026-11-03: "... - left to you"),
  "... a uEm22 with a second life to come (it would lie down and get up again with 2000) - the life taken away before
  its hit, its death the game's own (first this match)", every 15 s "... N about, H hit to date, kills k of 150 (+n
  in 15 s)", "... N of the stage's M area(s) opened ...", "... put back as they were", and once a stall "... the kills
  stopped at k of 150 - nothing about for 20 s; the stage's areas: B box(es), A open"; "cheats: kill enemies as they
  appear: switched off - a match began or ended (it lasts one match)". A part of the discovery not understood names
  its check: "merc: the stages' areas are not as known (cAreaHit's frame in its slot 7) - ...", "merc: the Chainsaw
  Majini's second life is not as known (uEm22's own frame) - Kill enemies as they appear kills one once it has got up
  again".
- Built, tested offline (`tests/test_merc.cpp`, `tests/test_panel.cpp`, and since 2026-11-03 `tests/test_discover.cpp`:
  the real mt.cpp and merc.cpp discovery over the dump at its own addresses).
- **2026-11-03, the user's first match** ("the game stopped at 149 kills instead of getting to 150"; the log: the
  kills at ~23 every 15 s to 149, then nothing about for 25 s until the time ran out). The 150th was the stage's
  chicken, left out as an animal (above). Besides, the stages' areas had not been understood at the start ("merc: the
  stages' areas are not as known"), so they sent only where the user stood. Its cause: the new cAreaHit frame
  signature was named `kSigAreaFrame`, the name of sArea's (the countdowns'); `game::sig_one` finds a name's first
  entry, so the areas got sArea's frame and failed their check (cAreaHit's slot 7). `tools/sig.py --check` found both
  patterns once each and the scratch replay read the right address directly, so neither saw it. Renamed
  `kSigAreaHitFrame`; `tools/sig.py --check` now fails a name defined twice; `tests/test_discover.cpp` runs the
  discovery itself (it fails on the old name: three checks).
- **2026-11-03 (evening), a Chainsaw Majini left standing** (the user: "the mercenary kill all enemy cheat left a
  chainsaw guy alive in his second phase instead of killing him"). The log, a 400-kill match (19:50-19:53): "a uEm22
  still stands after 25 hits - left to you" at 19:51:30, then "1 about ... kills 399 of 400 (+0 in 15 s)" until the
  user paused and switched it off. Its first hit left it 1 (its second life, above) and it lay down for its fall and
  8 s; the 24 hits after (400 ms apart) did nothing while it lay there, and once they were spent the mod left it for
  good - so it got up for its second round and was never hit again. Fixed: the mod takes the second life away with
  the hit (it dies at once, the game's own death), and an enemy still standing after 25 tries is hit every 5 s
  instead of left. Tested offline (`tests/test_merc.cpp`: a chainsaw run by the game's rule, both fixes taken out in
  turn - 3 and 2 checks fail; `tests/test_discover.cpp`: the discovery over the dump). Not seen in play.
