# RE5CabbyCodes - the play log, the in-game checklist, what is next

Part of the project's notes: `CLAUDE.md` is the guide and lists every section of `docs/`. The addresses are
build 11465250's. A section named in quotes ("The cranks") may be in another of these files:
`grep -n '^## ' docs/*.md`.

## The play log (the user's reports and what came of them)


**Little has been played yet.** The user's first play (2026-10-29, two chapter starts) had the panel up and the
mouse let go all the time - the pause menu's unit is there all through a chapter (above); the pause now goes by its
open byte. Their second (2026-10-29, with `--force-grab-cursor`, all but two switches on): "the one hit kills cheat
is not working" - fixed the same day (subtract-health; installed, seen hooked at the title, not yet played). Their
third (2026-10-29 evening, after the reboot; the log: Item Management, then five Stores at the pause from both
cases): "the item box has checkboxes for actions but they don't stay checked" (the panel let the box's ticks and
typed counts go every frame at the pause; now everything typed or ticked is let go only when the panel goes,
`visible_now`), and they asked for the inventories to list only the slots in use, one time for the chapter (above)
and "Armor" - all built and installed the same evening (not played yet). Later that night: "the item box imgui menu
doesn't have the category tabs like we had for previous RE mod installments" - the box's list now has them (above,
"The box's list"; installed, not played yet). Then: "when trying to take an item out of the item box into chris'
inventory the game crashed" - the game's own fatal error on a model whose archive the area had not loaded ("What
an area has loaded"); Take and Add in play first went by what the area has loaded, and then - the user: "yes make
it work for any item" - have the game's loader load the rest first (built and installed that night; the operands
checked against the dump offline, not yet seen in a log: the next one should have "inventory: sLoadMan's 10 lists
..." at the start, "inventory: N archives loaded in this area; of items (n): uWp02 uIt0201 ..." as each pause opens
in a new area, and for a load "inventory: the game's loader asked for uWp13: 2 new archive(s) (uIt0202 uWp13)",
then "itembox: H&K MP5 (MG) loaded after N frame(s)"). Then: "is there a way to have these items preloaded so we
don't have to do these waits at time of transfer? ... we want this seamless to them" - the box's contents are
preloaded as each pause opens, an Add list's pick as it is picked, and the leftovers with no model are no longer
offered (built and installed that night; the log should show "inventory: the game's loader asked for N item(s)" as a
pause opens with something not loaded in the box).

2026-10-30: "we don't yet seem to have a treasures area for the item box" - the box has a Treasures tab (both
places) and its Add list one ("The treasures"), with the screen's own rebuild of its treasures list beside Item
Management (approved that day). Built, tested offline (the discovery's reads and checks replayed over the dump, all
as expected) and installed; not seen in a log yet: the next should have "itembox: the treasures cSystemInfo +0x3AC8
(54 entries, up to 999 a treasure), each character's sPlayer +0x260 and saved cGameInfo +0x1330 (+0xD8 a character);
Item Management's rebuild of its treasures list exe+0x6048A0" after the box's line.

2026-10-30, the user's play of chapter 2-3 (every switch on but the two clocks; the log has several restarts there):
"the god mode cheat doesn't seem to work for the chapter 2-3 vehicle segment's vehicle health. Also the infinite ammo
doesn't seem to work for the gun overheating in the same section" - the jeep has a damage of its own, and a mounted
gun's overheating is a timer of the weapon's, neither of which the two cheats reached ("The jeep and the mounted
guns"). Built, the new checks replayed over the dump (all as expected) and installed; not seen in a log yet: the next
should have "game: the jeep (uOmJeep, 2 classes) keeps its health as the characters do", "cheats: the jeep's damage
exe+0x6DC3E0 hooked in 2 vtables", "cheats: the weapons' overheating exe+0x79794A (a gun's firing +0x1C41): ..." and,
in 2-3, "cheats: the jeep (uOm1304) in play, its health 3500 of 3500". Then: "it also seems that the one hit kill
cheat didn't work on the gigante boss at the end of the same chapter" - the Ndesu's and its Plagas' hit check takes
every hit into counters of their own ("The Ndesu"). Built the same way (checks replayed over the dump), installed;
the next log should have "cheats: an enemy's hit check (slot 129): the Ndesu's exe+0x4E19E0 (its gauge +0x1F50) and
its Plagas' exe+0x4E14F0 (their count +0x1F50) hooked in 2 vtables" (the 22:28 run had it). Then, at chapter 3-3's end
that night: "the one hit kill on the boss of 3-3 also has this problem" - Irving (Transformed) takes health only
through its core, open while it is down, and goes down only once its body has taken 12000 ("Irving (Transformed)").
Built the same way (the checks replayed over the dump), installed; the next log should have "cheats: Irving's hit
check exe+0x4E82E0 (its body's parts below 60: region 1, left +0x1C1C) and its tentacles' exe+0x556CC0 (region 1,
left +0x1C1C) hooked in 2 vtables".

2026-10-30: "the steady aim cheat doesn't perfectly steady the aim when looking down a rifle scope" - the scope's view
is the gun model's, and the aim's own motions breathe ("The scope"); Steady aim now holds the character's looping
motions while its scope is up. Built, tested offline (`tests/test_cheats.cpp`, the discovery replayed over the dump)
and installed; not seen in a log yet: the next should have "cheats: a scope's flag uPlayer +0x2998; the motion layers
uModel +0x380 (0xE0 each, in use +0x374: speed +0x24, attributes +0xA, plays once 0x4) - Steady aim holds the
breathing through a scope" at the start and, the first times a scope goes up, "cheats: a scope is up - Chris's
looping motions held (layer:motion 0:51A 1:51B 2:519 3:53D 4:53E 5:53C 6:4..)".

2026-10-30, evening: "the game just crashed trying to go through a door in chapter 3-2" - the game ended itself (its
own fatal error or exit, no exception) with nothing of the mod's logged near it; the cause is not known yet ("The
game's own ends"). The mod now logs the game's own error message and any other end (installed); the next such end
should leave "FATAL: the game ends itself with its own error: "..."" and the game's code it came through as the log's
last lines.

2026-10-30, night: "in chapter 3-2 there is a countdown timer. We need to add a freeze countdown timer cheat similar
to how we've done it in prior RE game mods" - the countdown is stage 209's own (the dock at the end of 3-2), shown on
the HUD; Freeze countdown timer now holds it (and Desperate Escape's), the switch is shown in every game, and a "Time
left (s)" editor sets a running one at the pause ("The countdowns"). Built, tested offline and installed; not seen
in a log yet: the next should have "cheats: aS209's countdown +0x650 runs down to 0 in its frame exe+0x345240",
"cheats: aS704's countdown +0x650 runs down to its +0x654 in its frame exe+0x360CE0" and "cheats: the stages'
countdowns: 2, among sArea's areas (+0x382C, 8), shown on the HUD (exe+0x5EC370, up to 5940 seconds)" at the start,
and at the dock "cheats: aS209's countdown is running, m:ss left (frozen)".

The gold editor writes the checkpoint copy too (`kSigCheckpoint`; installed 2026-10-29 after a run to the title
showed "its checkpoint copy 0x4638 on"). The item box window was built the same evening, at the user's word ("you may
add the new hook").

2026-10-31: "when "starting" a game when at the screen to pick inventory items, add an imgui "Back" button ... make
the button instead say "Chapter Select" instead of back" - Item Management before a chapter has no way back in the
game; its window now has Chapter Select: the screen's own quit, then the start-up menu sent to CHAPTER SELECT ("Chapter
Select from Item Management"). Built, the discovery replayed over the dump, tested offline and installed; not seen in
a log yet: the next should have "menus: Chapter Select beside Item Management: the play flow's screen +0x2C (aGame; its
quit from the screen's +0x3E); ..." at the start and, when used, "menus: Chapter Select asked at Item Management ...",
"menus: the start-up menu is up - its return reason 7 (back to the choice made), the story menu's choice parked" and
"menus: the story menu's choice made CHAPTER SELECT ...". Then: "let's attempt to call the game's save feature to save
the inventory before landing back at chapter select" - the button now goes at one click and keeps what was changed: the
screen's own save, then the checkpoint copies the quit takes back brought to it. Installed; its log line "menus:
Chapter Select asked at Item Management - the screen's own save of the cases made, the profile's and the cases'
checkpoint copies brought to what is kept, the screen closing with its quit", and at the start "... the screen's save
exe+0x6079E0 ...; the save data's working block cGameInfo +0x1690 (0x1590 bytes) and its checkpoint copy +0x42D0".
Then (that day's play, 12:09): "using the chapter select option crashed the game" - the save made, the start-up menu
handed over, then the game's own fatal error on the chapter list's layout (top05.lsp), whose archive (MenuResource) the
story menu loads as a choice is made there and the way back never had loaded ("The front end's archives"). The mod now
has the menus' own load bring it in with the title's and hands the menu over only with it loaded (else the game's own
menus come up). And: "also I want the button always visible, it's currently able to be collapsed inside inventory
editor window" - the button is in a window of its own now, beside the item box's, that does not fold. Installed; not
seen in a log yet: the next should have "menus: the menus' own load: aDemo's step +0x4 (1 has the loader choose the
front end's archives, once its area's +0x20 is set) by sLoadMan +0x207D4 - the menus' bit 0x2 (Image\Archive\
MenuResource) added before it ..." at the start and, when used, "menus: the menus' own load asks for
Image\Archive\MenuResource with the title's, as the story menu's choice does" between "asked" and "the start-up menu
is up". Then: "this is working now, however it's a little different from the vanilla game mechanics. The chapter select
screen is opened in front of the main menu for the game. Usually there is no background menu behind it" - the start-up
menu's background reads the reason as it starts and keeps the main menu's look until the menus' choices change it
("The start-up menu's background"); the way back now makes those choices' writes as it starts (under the fade in) and
hands the menu over then. Installed; the next log should have "menus: the start-up menu's background (uIdTopMenu00) as
the story's choice and CHAPTER SELECT's leave it: its list +0x54 (uIdStartUp11: picked +0xD51, go on +0xD64), its
header +0x5C (uIdStartUp14: shown +0xD84), its panel's segment 8 ..." at the start and, when used, "menus: the
start-up menu is up (fading in) - its return reason 7 ...; its background as the story's choice and CHAPTER SELECT's
leave it ...". The user's play that afternoon had it working ("this is working now"). Then: "add the chapter select
button to the pause screen as well, and put a confirmation on it. Also I just ran into an item bug. I put a rifle into
the item box while in game then quit the level, and this duplicated the item, and I think it also destroyed my rocket
launcher" - RESTART after a Take and a Store at the pause brought the case back from the play flow's own copy of the
save data, which the moves never reached ("The saved copies of the cases"); the moves are now made in every saved copy,
and Chapter Select is at the pause too, leaving as QUIT without saving ("Chapter Select from the pause"), both places
asking Yes or No first. What the user's save already holds (the Dragunov in the box and in Chris's case, no rocket
launcher) is not repaired: the box's x and its Add put it right. Before it was installed (the user's game ran on):
"I want the answer to be yes on quit to save status of equipment and item box. I only want the confirmation to exist
while the player is in the game. I also want the chapter select button to exist on the game end screen" - the pause's
Chapter Select now answers yes (the game's own save of the equipment, the box and the treasures), only the pause asks
Yes or No, and the Game Over screen has the button too ("Chapter Select from the Game Over screen"; "the game end
screen" taken as that one). Then: "I also wanted it on the chapter results screen" - the results have it too
("Chapter Select from the chapter's results"). Built, tested offline, the discovery replayed over the dump; installed
as the user's game closed (17:31, sha256 89ce5d2d...). The next log should have "menus: the save data in cGameInfo: the saved state
+0x100, the working block +0x1690 and its checkpoint copy +0x42D0 (0x1590 bytes each), and the play flow's copy of
the profile's cGameInfo as at its last checkpoint (aGame +0x130) ...", "menus: Chapter Select at the Game Over screen
(uIdGameover00): shown +0x194, ..." and "menus: Chapter Select at the pause: the pause menu's panes +0x48 (0x68 each,
4; ...) - QUIT (option 3) from its step 10 ..." at the start; when used at the pause "menus: Chapter Select asked at
the pause - the pause menu's QUIT from its save question answered yes (the equipment, the item box and the treasures
saved) ...", at the Game Over screen "menus: Chapter Select asked at the Game Over screen - its QUIT ...", at the results "menus:
Chapter Select asked at the chapter's results - their own quit ...", then "menus:
the chapter has quit - on to the menus" and the way back's lines as from Item Management.

2026-10-31, evening: "Update the QTE cheat so that it also does the required buttons for turning cranks (A + D). It
doesn't need to automatically leave the door when finished, as sometimes we want to wait to release, but it should
automatically get it fully open" - Automatic QTEs now answers a crank's Turn every frame (the game's top speed, up to
its most) and leaves its Release to the player ("The cranks"). Built, tested offline and installed at 17:50 while the
user's game ran (the check missed it - see "Always install after a change"): the running game kept the build it had
loaded, so the change takes effect from the game's next start. The next log should have, at each crank taken hold of,
"cheats: a crank's Turn answered for character 0 - it winds itself fully open; Release is the player's".

2026-10-31, night (the user in chapter 5, the game running): "I'm not sure the one hit kills are working against the
reaper enemy" - the Reaper (uEm34) takes health only on its weak spot: its own hit check refuses every other hit on its
body, so subtract-health's hook never saw one ("The Reaper"). One hit kills now sets the marks of the game's own instant
kill of one before its check, so a hit with damage anywhere on its body kills it. Built, tested offline, the discovery
replayed over the dump; installed once the user's game had closed. The next log should have "cheats: the Reaper's hit
check exe+0x4A1CF0 (health only from its weak spot, parts 27-29 of its body's 60; its marks +0x1F60 and +0x1F61, both
set by its damage exe+0x4A0350 for its own instant kill) hooked in 1 vtable(s)" after Irving's line.

Then (the same night, 21:48): "when fighting with Jill and Wesker, after damaging wesker the game crashed" - the
game's own fatal error on Chris's cutscene face (evf0000.mod): the mod's item preloads at the pause had gone into the
cutscenes' list, empty since the opening cutscene, so the cutscene that damaging Wesker started was taken for
preloaded and played unloaded ("The game's own ends", "What an area has loaded"). The mod now asks into the area's
sets' list, never the cutscenes'. Built, tested offline (`tests/test_loader.cpp`), the discovery replayed over the
dump; installed at 22:13 (the game closed; sha256 dfb02f06..., with the Reaper's hook). The next log should have
"inventory: the mod asks the loader into the area's sets' list (4: chosen and committed at each stage's load), never
the cutscenes' (7: ...)" at the start, and at a pause "inventory: the game's loader asked for N item(s) into the
area's sets' list (4; the list chosen last, 7, as it was): ...".

2026-11-01, just past midnight (the user's game running): "add a new cheat to auto-open the map on the start of a new
chapter" - Open map at chapter start: as the game clears the profile's mTempBase.mChapterStart (the chapter's title
coming up), the map of each character this machine's pad drives is opened as its key would open it ("The map"). Built,
tested offline (`tests/test_cheats.cpp`, `tests/test_panel.cpp`), the discovery replayed over the dump; installed at
00:51 as the user's game closed (sha256 80dc0a0a..., cmp OK). The next log should have "cheats: a chapter's start: cGameInfo +0x41C4, cleared as
its title (uIdChapSt00) comes up (exe+0x319A40); the play state cGameSys (sBioMain +0x1042C, the story's mode +0x58 0)
keeps each player's map open (+0x554, 5); a character's map uPlayer +0x264C (uIdMap00: open +0x1E, toggled by the key
while +0x1D; its player's number uPlayer +0x2D9C)" at the start and, as a chapter begins with the switch on, "cheats: a
chapter has begun - Chris's map opened (player 0)".

2026-11-01, midday:
- **The report**: "fix 1 hit kill cheat to work on the excella boss". Then, while it was being read: "I'm fine with
  needing to hit this boss, and other bosses, more than once as long as the components to drive the fight forwards
  can also be killed in one shot. That way we drive the fight forward with each piece that needs hit only being hit
  once each to kill them".
- **The cause**: the boss is Uroboros Aheri (uEm42, the end of chapter 6-2). Its four orange cores keep their health
  (7000 each) in its own check's counters, worn only while a core is open, so One hit kills never touched them. Its
  doom attack's escape (2000) is likewise. Its main core already died at one hit once it was out ("Uroboros Aheri").
- **The fix**: a hit with damage breaks a core, open or closed, and breaks the doom attack off. Round one takes two
  hits on cores and one on the main core; Dead's scene number ends the chapter as the game ends it.
- **Checks**: built, tested offline (`tests/test_cheats.cpp`, 97 checks; four broken builds caught), the discovery
  replayed over the dump.
- **Installed** at 12:37 (the game closed; sha256 ff4274c2..., cmp OK).
- **The next log should have**, after the Reaper's line: "cheats: Uroboros Aheri's hit check exe+0x4C0CD0 (its 4
  cores from part 60: each one's health +0x1FC4, 8 apart, worn by exe+0x4BF0B0; its doom attack's escape +0x2154
  past 2000 while +0x18DC has 0x4) hooked in 1 vtable(s)".

2026-11-01, afternoon (the user's game running):
- **The report**: "the automatidc QTE doesn't work for the final level when pushing the boulder".
- **The cause**: the boulder (uOm095e_Kick, stage 508, the volcano of chapter 6-3) goes Push, Push, five Hooks and a
  Ram, all in category 8, which the mod left to the game's own AUTO. AUTO answers the two Pushes itself, 2 s in. It
  makes each Hook and the Ram one plain press, and leaves those presses to the player. With nothing pressed, the Hooks'
  10 s ran out and the push failed ("The boulder").
- **The fix**: Automatic QTEs answers the boulder's Hooks and its Ram, hidden, each once its prompt has been up 0.75 s,
  so each punch's motion plays that long before the next one starts. A press of the player's own still counts. The two
  Pushes stay AUTO's. Taking hold of the boulder (its Push, a context action) is still the player's.
- **Checks**: built, tested offline (`tests/test_cheats.cpp`, 116 checks: the boulder run as the game runs it, keyboard
  and pad at 60, 144 and 20 fps; three broken builds caught, the build before among them), every signature checked
  over the dump (`kSigQteAutoPush` new, 109).
- **Installed** at 14:07 as the user's game closed (sha256 3e7216d4..., cmp OK).
- **The next log should have**, after the action buttons' line: "cheats: the volcano's boulder: AUTO answers its Pushes
  (labels 0xB5, 0xC0) 2 s in, exe+0x3671F1; the mod its Hooks (0xB7) and Ram (0xB6), each up 0.75 s (a request's label
  +0x24, its seconds up +0x74)". At the boulder: "cheats: the boulder's Hook answered for character 0, up 0.77 s" five
  times, then "the boulder's Ram ...".

2026-11-01, 14:27 (the user's game running):
- **The report**: "sometimes on the chapter end screen, the chapter select button remains greyed out".
- **The cause**: the mod's own rule. The last chapter's results (6-3) offer no quit of their own (+0x189), and the
  mod greyed Chapter Select there: "Not after this chapter: it goes on to the ending". But the ending plays before
  them, and their continue is their quit: it sets +0x188 and the game goes back to its menus ("Chapter Select from
  the chapter's results"). The session before (13:16-13:57, the boulder's: 6-3) left two results screens without the
  button, both coming up after the characters were gone, as 6-3's ending leaves them; this session's one results
  screen, another chapter's, went through the button to the chapter list (14:19:11).
- **The fix**: Chapter Select is ready at the last chapter's results too, with the same writes as at any other's. Its
  help there says the chapter list comes instead of the main menu. Whether it can go at the Game Over screen and at
  the results is now logged as it changes, with the screen's step, so a greyed button has its reason in the log.
- **Checks**: built, tested offline (`tests/test_menus.cpp` 93 checks, `tests/test_panel.cpp` 111, the other three
  as before; four broken builds caught: the old rule, no log line for the next screen, a line every tick, the last
  chapter not told to the panel).
- **Installed** at 14:58 as the user's game closed (sha256 7fe43747..., cmp OK).
- **The next log should have**, at each results screen, "menus: Chapter Select at the chapter's results greyed: Wait
  for the results' menu. (their step N, their quit offered; the return reason 1, the top menu's choice 0)" as they
  count up, then "menus: Chapter Select at the chapter's results ready (their step 10, their quit offered; ...)". At
  6-3's: "... ready (their step 10, the last chapter's (no quit of their own); ...)" and, clicked, "menus: Chapter
  Select asked at the chapter's results (the last chapter's: as their continue quits) - their own quit ...".

2026-11-01, 14:39 (the user's game closed at 14:58):
- **The report**: "need to investigate the possible items to add to the inventory / item box. There seems to be extra
  items that aren't real in there. Compare with save data to see what I've found, and question the items I haven't".
- **The save** (decrypted, see "The save file"; `tools/save.py` reads one now): every weapon the shop sells, all 11
  kinds of ammo, both vests, the shop's herbs, the White and Rotten eggs, 49 of the 50 treasures. Not found yet: the
  Brown and Gold eggs, the Sapphire (Marquise) - all three real.
- **The cause**: the Add lists offered every item of the table that has a model. That included each character's fixed
  knife and two knives of neither, and the Mercenaries' Samurai Edges and Px4. It included the scene-only Lantern,
  Flamethrower, L.T.D., RPG-7 NVS and RPG round, and the floor herbs 0x301/0x302 (a pickup makes them the shop's). It
  included the launcher's in-hand id 0x10C (the EXP load again). And the Gatling gun and the Longbow went to either
  character, which Item Management refuses. The evidence for each: "What the Add lists offer".
- **The user's answers** (asked group by group):
  - remove the floor herbs, the bare launcher, the Mercenaries' guns and all four knives;
  - remove the scene-only items;
  - keep the launcher's six loads;
  - "Follow the game" for the Gatling gun and the Longbow.
- **The fix**:
  - `gen_items.py` marks what the story gives Chris and Sheva to carry (`offer`: the shop's price list's items and
    the weapons achievement's, 54).
  - It also marks the two one-character weapons (`only`, read out of Item Management's own check).
  - The Add lists offer only the first. The case's Add and the box's Take grey the Gatling gun for Sheva and the
    Longbow for Chris; the tick refuses both, and says so.
- **Checks**: built, tested offline (`tests/test_panel.cpp` 119 checks, `tests/test_loader.cpp` 70, the other three as
  before; seven broken builds caught, one of them only after the loader test's knife was given a model).
- **Installed** at 15:26 (sha256 9afd8044..., cmp OK).
- **The next log should have**, at a refused Add or Take: "inventory: Longbow not added to Chris's case - the game
  keeps it for Sheva" or "itembox: entry N (Longbow) not taken into Chris's case - the game keeps it for Sheva".

2026-11-01, the user's answer to that report's question ("Should Store refuse those as well?"):
- **The ask**: "yes store should refuse those types of items". A scene's own weapon held at the pause (the Lantern in
  2-2's mine) could be stored, and then sat in the box under Other items.
- **Found on the way**:
  - In play every grenade launcher in a case has the launcher's id 0x10C: the model switch gives each load the
    launcher's model and the item that id, the model keeping the rounds. The game's own save of a case writes the
    load back from them ("What the Add lists offer", Store). The mod's Store put 0x10C itself in the box: under Other
    items since the last change took 0x10C off the Add lists, and with explosive rounds whatever was loaded (the
    switch gives a bare 0x10C those), so a stored acid load came back explosive.
  - The last entry's note "+0x2C 2 for exactly the scene-only weapons" was wrong: the Proximity bomb has it too
    (+0x2C is how an item is used). The Add lists never went by it, so they are unchanged; the docs are corrected.
  - `tests/test_loader.cpp`'s "shotgun" was 0x10E, Sheva's knife in the game's table. Store now refuses it, so the
    test uses the Ithaca M37 (0x104).
- **The fix**: Store takes only what the Add lists offer, by the id the box keeps the item as - a launcher's load
  (`inventory::as_saved`, read as the save's own code reads it: `kSigSaveLauncher`). The panel greys Store beside
  anything else, the hover saying why, with a line under the case; the tick refuses it (a ticked batch's too), says
  so and logs it. A launcher whose load cannot be read stays in the case.
- **Checks**: built, the new discovery's reads and cross-checks replayed over the dump (the six loads as expected),
  `tools/sig.py --check` 110 signatures, 0 bad; tested offline (`tests/test_panel.cpp` 121 checks,
  `tests/test_loader.cpp` 73, the other three as before; seven broken builds caught).
- **Installed** at 18:17 (sha256 7dec6e12..., cmp OK).
- **The next log should have**, at the start: "inventory: the game's save of a case writes the grenade launcher
  (0x10C, a uWpLauncher model) as its load, by the rounds the model keeps at +0x1C80: 0x140->0x125 0x141->0x126
  0x142->0x127 0x14D->0x139 0x14E->0x13A 0x14F->0x13B". At a refused Store: "itembox: Chris's slot N (Lantern) not
  stored - the box takes only what the Add lists offer"; at a launcher's: "itembox: Chris's slot N (G. launcher
  (ACD), 6) stored".

2026-11-02, the user's ask: "it doesn't appear we've added cheats for the mercenaries mode yet. Compare with the RE4
mod to see which ones to make".
- **The comparison**: RE4's (the remake's) Mercenaries switches are Endless combo, Mercenary Mode always on, Kill
  enemies as they appear and Freeze countdown timer; RE4 (2005)'s Endless combo, Kill enemies as they appear and Bonus
  time always on. RE5's Mercenaries is built as the 2005 game's: a combo whose bonus is banked as it ends and a
  30-second Combo Bonus pickup - the counterpart of Mercenary Mode and of Bonus time. Freeze countdown timer covered
  the Mercenaries' clock already (no editor for it: RE4's rule).
- **Built**: Endless combo, Combo Bonus always on and Kill enemies as they appear, in a section of the panel shown
  only in a match, with the match's numbers (`merc.cpp`; "The Mercenaries" in docs/cheats.md). Kill enemies as they
  appear is field writes only - a shot of the gun in hand written into each enemy's own hit record, as the game writes
  a hit from the other machine, and every area task's box of the stage widened while it acts so every area sends
  (standing still, only the areas round the player would) - no new game call. It lasts one match (RE4's rule) and
  acts only offline. A forked agent read the stages' scenario data (their area tasks and enemy-set FSMs).
- **Checks**: built (no warnings); 18 new signatures (`tools/sig.py --check` 128, 0 bad); the discovery's reads and
  cross-checks replayed over the dump; tested offline: `tests/test_merc.cpp` 60 checks (new), `tests/test_panel.cpp`
  127, `tests/test_cheats.cpp` 116, `tests/test_loader.cpp` 73, `tests/test_menus.cpp` 93, `tests/test_crash.cpp` 13.
  The first test run caught a shot's place check failing on x87 precision (the test's, not the mod's).
- **Installed** at 23:36 (sha256 c08ac6d6..., cmp OK).
- **The next log should have**, at the start: "merc: the Mercenaries (modes 2 and 5): cGameSys (sBioMain +0x1042C)
  step +0x67C, clock +0x680, kills +0x58C (150 to clear, 250 more with +0x79E) ...", "merc: a stage's areas:
  sScenario +0xD4's tasks of type 6 ...", "merc: an enemy's hit record (its cHit +0x1290, the record +0x129C) ... - the
  M92F's attack 2", "cheats: 15 of 15 switches can work". In a match: "merc: a match of the Mercenaries begins (mode
  2, step 0, ...)"; "merc: endless combo: the combo's bonus goes onto the score as it is earned (N now, first time)";
  "merc: Combo Bonus always on: the Combo Bonus started (30 s, held)"; "merc: kill enemies as they appear: N of the
  stage's M area(s) opened to wherever you stand ...", "... uEm10 hit (first time) - a shot of Chris's (item 0x102,
  attack 2)" for each kind of enemy, every 15 s "... N about, H hit to date, kills k of 150 (+n in 15 s)"; at the end
  "... put back as they were", "merc: the match is over (...)" and "cheats: kill enemies as they appear: switched off -
  a match began or ended (it lasts one match)". A kind that will not die: "... a uEm34 still stands after 25 hits -
  left to you".

2026-11-03, the user's first Mercenaries match with the new switches (the log, 23:40-23:43: all three on, the kills
~23 every 15 s to 149, then nothing about for 25 s until the time ran out): "1) I don't think the main RE5 game ever
has a countdown timer that I can recall. Maybe this should be a mercenaries only cheat. 2) I tested this and the game
stopped at 149 kills instead of getting to 150 cap. 3) I want to get the animal kills as well, so 150 enemies + all
animals for a higher possible total."
- **1)** The story has two countdowns: chapter 3-2's escape from the oil field (stage 209, the dock - Josh: "We're
  running out of time!") and Desperate Escape's last wait for the helicopter (stage 704). The switch was built for
  them at the user's own ask (2026-10-30: "in chapter 3-2 there is a countdown timer"). Kept for both; answered.
- **2)** The 150th was the stage's chicken. Every stage's 150 includes its chickens - the start set places them (a
  forked agent read every layout of s400-s407: one in most stages, two in the Mines, ten in one 400-enemy variant;
  no other animal anywhere) - and the first build left uEmAnimal out. Besides, the stages' areas had failed their
  discovery at the start: a second signature named `kSigAreaFrame` (sArea's, the countdowns', was found by that name).
  Both fixed ("The Mercenaries" in docs/cheats.md); `tools/sig.py --check` now fails a name defined twice, and
  `tests/test_discover.cpp` (new) runs the real discovery over the dump - it fails on the old name. A stall now logs
  what the stage has.
- **3)** The only animals in the stages are their chickens, and they are part of the 150, so the cheat now kills them
  and counts them as the game does: everything a stage holds dies, the clear at 150. (Not counting them towards the
  clear, built first, would have left most stages unable to clear - taken out.) Only the Village (s402, 156) and
  Reunion's Ancient Ruins (s403, 151) hold more than the 150; the clear comes at 150 there, as in the game.
- **Checks**: built (no warnings); 128 signatures, 0 bad; tested offline: `tests/test_merc.cpp` 61,
  `tests/test_discover.cpp` 14 (new), `tests/test_panel.cpp` 127, `tests/test_cheats.cpp` 116, `tests/test_loader.cpp`
  73, `tests/test_menus.cpp` 93, `tests/test_crash.cpp` 13.
- **Installed** at 00:05 (sha256 356019ab..., cmp OK).
- **The next log should have**, at the start: "merc: a stage's areas: sScenario +0xD4's tasks of type 6 ..." (not
  "are not as known"). In a match with Kill enemies as they appear: "merc: kill enemies as they appear: N of the
  stage's M area(s) opened to wherever you stand ...", "... uEm62 hit (first time) ..." (the chicken), the kills to 150
  and "merc: the match is over (mode 2, step 2, kills 150 of 150, ...)". A stall: "... the kills stopped at k of 150 -
  nothing about for 20 s; the stage's areas: B box(es), A open".

2026-11-03, the user's next ask: "make the kills using knife for the bonus time".
- **Found**: a knife's kill earns no time in RE5's Mercenaries. A kill's weapon kind is FUN_007172A0's - the hit's
  class id equal to the player's own class is hand-to-hand (kind 0), a weapon's class its item's kind (the knives
  0x1A) - and FUN_007243A0 puts 5 s onto the clock only for kind 0 or the stun rod's 0x18.
- **Built**: Kill enemies as they appear's hit now names the shooter's own class, so each kill is that character's
  hand-to-hand kill: +5 s each, its points and combo as before; the reaction stays a bullet's ("Hand-to-hand kills, for
  the time" in docs/cheats.md). The two rules are read and checked at the start (`kSigMeleeKind`, `kSigMeleeTime`);
  without them the kills stay shots. The profile's per-weapon kill records count these as hand-to-hand.
- **Checks**: built (no warnings); 130 signatures, 0 bad; tested offline, every test rebuilt from scratch:
  `tests/test_merc.cpp` 62, `tests/test_discover.cpp` 15, the other five as before.
- **Installed** at 00:27, once the user's game had closed (sha256 454c2703..., cmp OK).
- **The next log should have**, at the start: "merc: a kill whose hit names the player's own class is hand-to-hand
  (exe+0x3172A0): 5 s onto the clock (exe+0x324433) - Kill enemies as they appear kills so"; in a match: "merc: kill
  enemies as they appear: uEm10 hit (first time) - a hand-to-hand kill for Chris (its class 0x...: +5 s), the hit a
  shot of item 0x102 (attack 2)". On screen: the clock's +5 at every kill.

2026-11-03 (evening), after a run of matches (the log, 09:58-19:54: every 150- and 400-kill match cleared but the
last): "I just had a case where the mercenary kill all enemy cheat left a chainsaw guy alive in his second phase
instead of killing him". The log: a 400-kill match from 19:50, "merc: kill enemies as they appear: a uEm22 still
stands after 25 hits - left to you" at 19:51:30, then "1 about, 827 hit to date, kills 399 of 400 (+0 in 15 s)" every
15 s until the user paused at 19:53 and switched it off.
- **Found**: the Chainsaw Majini (uEm22) may start with a second life (its own rule: a player's dynamic difficulty
  level above 6, or its set's), and until it has used it its frame sets "may not die" (uEnemy +0x1E38) every frame,
  which the hit path tests before any death: the mod's first hit left it 1, and its brain laid it down (cEm22ActDieLie:
  its fall, then 8 s) to get up with 2000 for a second round. The hits written while it lay there did nothing; the
  mod's 25 tries (400 ms apart, 10 s) ran out before it got up, and the mod then left it for good. In the earlier
  matches the chainsaws had no second life (the level not yet above 6) and died at once. Details: "The Chainsaw
  Majini's second life" in docs/cheats.md.
- **Built**: the mod takes the second life away (+0x1F5F, and that frame's +0x1E38) as it writes a chainsaw's hit,
  so it dies at that hit - the game's own death, its drop, one kill - as a chainsaw without one does; one that has
  got up already is left as it is (it dies at its hit). And an enemy still standing after 25 tries is now hit every
  5 s while it stands (logged once), never left: whatever keeps one from dying for a while, it dies when that ends.
  Three new signatures (`kSigDeathGate`, `kSigChainsawLife`, `kSigChainsawDown`), cross-checked: each of the two
  chainsaw functions a virtual function of its own class (uEm22's slot 62, uEmIntel22's slot 35), reading the same
  two bytes, "may not die" the hit path's test. The help text and READMEs: "a Chainsaw Majini stays down rather than
  getting up for a second round".
- **Checks**: built (no warnings); 133 signatures, 0 bad; tested offline, every test rebuilt from scratch:
  `tests/test_merc.cpp` 74 (a chainsaw run by the game's rule: dead at its first hit; with the second life not
  understood it lies down, gets up and is killed by the hits every 5 s; built with the life-taking taken out 3 checks
  fail, with the old give-up 2), `tests/test_discover.cpp` 18 (the second life's fields found over the dump),
  `tests/test_panel.cpp` 127, `tests/test_cheats.cpp` 116, `tests/test_loader.cpp` 73, `tests/test_menus.cpp` 93,
  `tests/test_crash.cpp` 13.
- **Installed** at 20:45, once the user's game had closed (a watcher from 20:21; sha256 ad6cb2b9..., cmp OK).
- **The next log should have**, at the start: "merc: the Chainsaw Majini's second life: uEm22 +0x1F5F (it may get up
  again) and +0x1F5E (it has); until then its frame (exe+0x47B280) sets uEnemy +0x1E38 ... - Kill enemies as they
  appear takes the life away first". In a match where one would have got up: "merc: kill enemies as they appear: a
  uEm22 with a second life to come (it would lie down and get up again with 2000) - the life taken away before its
  hit, its death the game's own (first this match)". Any enemy the game keeps alive longer: "... a uEmNN still stands
  after 25 hits - hit again every 5 s while it stands", and the pace lines' kills still moving.

2026-11-03 (night), the user: "the chapter select button shows on the pause menu while in other game modes beyond the
story. Instead of saying the game wasn't launched from chapter select, it should just not show up".
- **Found**: the pause, the Game Over screen and the results offered the button wherever the play flow ran them and the
  Mercenaries' HUD was not up, greyed ("This game was not started from the story's menu.") when the menus' return
  reason or top choice was not a story start's. Lost in Nightmares and Desperate Escape are started from the story's
  menu itself (its choices 8 and 9): they set cGameSys's mode to 3 or 4 and leave the return reason 9. So their pause
  had the greyed button, and so would Versus (mode 1; online, greyed "Not in an online game."). Details: "Chapter
  Select only in a story game" in docs/menus.md.
- **Built**: Chapter Select is offered only in a story game: cGameSys's mode the story's (0, the test the play flow's
  own Item Management step makes, read out of it), the return reason and the top menu's choice a story start's.
  Anywhere else there is no button or window at all, at the pause, the Game Over screen and the results alike, and the
  greyed message is gone. An online story game still shows it greyed. No new signature, no new game call.
- **Checks**: built (no warnings); 133 signatures, 0 bad (none changed); tested offline, both tests that build
  `menus.cpp` rebuilt from scratch: `tests/test_menus.cpp` 103 (the extra episodes at each screen, Versus, the mode
  or the reason alone, the log; a build offering it in every mode fails 10, one not testing the mode 3),
  `tests/test_discover.cpp` 22 (menus.cpp's discovery now run over the dump too: the mode +0x58 through sBioMain
  +0x1042C, the menus' fields as before). The other five tests do not build `menus.cpp` (`test_panel` stubs it) and
  were not rerun. Also corrected docs/cheats.md: FUN_00BF55E0 is the extra episodes' start, not NEW GAME's.
- **Installed** at 21:46 (sha256 f4c90caa..., cmp OK).
- **The next log should have**, at the start: "menus: Chapter Select beside Item Management: ... cGameSys (sBioMain
  +0x1042C, online flags +0x10434): the game's mode +0x58 (0 the story: Chapter Select in no other), ...". At the
  pause in an extra episode, once: "menus: no Chapter Select on this screen - not a story game (the game's mode 3, the
  return reason 9, the top menu's choice 0)" (mode 4 in Desperate Escape); the same once at its Game Over screen and
  its results. In a story chapter the button as before.

2026-11-03 (night, later), the user: "We need a separate instance of the item box for the other game modes such as Lost
in Nightmares. These extra content portions should all have their own item box".
- **Found**: the panel's box at the pause was the story's (cSystemInfo's) in every game but the Mercenaries', so in Lost
  in Nightmares and Desperate Escape it showed the story's box and treasures, Take moved the story's items into the
  episode, Store the episode's into the story. Worse, a move wrote every saved copy of the case, the working block's
  checkpoint copy among them - in an episode the story's (the episodes never commit; their QUIT takes the story back
  from it), so a Store there could take an item out of the story's saved equipment. The game keeps no box in the
  episodes: each starts from a loadout of its own written into the saved state (FUN_00BF55E0), and Lost in
  Nightmares empties both cases at its stage 705 (FUN_0072E6E0). Details: "The extra episodes' item boxes" in
  docs/items.md.
- **Built**: each extra episode has a box of the mod's own, in a file beside it
  (`RE5CabbyCodes.itembox.lost-in-nightmares.ini`, `RE5CabbyCodes.itembox.desperate-escape.ini`): the game's box's 54
  entries and rules, the panel's box at the pause in that episode, named for it, with no treasures; its Add list leaves
  out what the game keeps for a character not in the episode (the Longbow; in Desperate Escape the Gatling gun too).
  Moves there go into the saved state and the play flow's copy of it only. Each change is written at once, a Store
  before the item leaves the case (refused if the file cannot be written); a file that cannot be read is left alone
  and its box unused that run. The game's mode is read each tick (`game::Run::mode`, one new signature,
  `kSigEpisodeCommit`); no box in the Mercenaries or Versus, as before, nor with the mode unknown. The story's box is
  unchanged. No new game call.
- **Checks**: built (no warnings); 134 signatures, 0 bad; tested offline, every test rebuilt: `tests/test_loader.cpp` 90
  (the episodes' boxes: their own, apart from the story's and each other's, the copies an episode restarts from and
  not the story's, the files written and read back with a gun's levels, three unreadable files left alone, a Store
  refused and an Add kept while the file cannot be written, then written; a build showing the story's box in the
  episodes fails 15, one writing the story's copies 2, one not writing the file first 1, one never writing it 6),
  `tests/test_panel.cpp` 133 (the episode's section, no Treasures, its Add list's rule - a build without it fails 2 -,
  an unreadable file), `tests/test_discover.cpp` 23 (the mode's signature agreeing with the menus' own field and mode
  over the dump), `tests/test_menus.cpp` 103, `tests/test_cheats.cpp` 116, `tests/test_merc.cpp` 74,
  `tests/test_crash.cpp` 13.
- **Installed** at 22:14 (sha256 afb748de..., cmp OK).
- **The next log should have**, at the start: "game: the game's mode cGameSys +0x58 (sBioMain +0x1042C; 0 the story, 3
  and 4 the extra episodes - the game's own test of them exe+0x315AE0)", then "itembox: no
  ...\RE5CabbyCodes.itembox.lost-in-nightmares.ini yet - the box of Lost in Nightmares starts empty" (or "... read -
  the box of Lost in Nightmares holds N entries") and the same for Desperate Escape. In an episode: "itembox: a game
  of Lost in Nightmares - the panel's box is the episode's own (RE5CabbyCodes.itembox.lost-in-nightmares.ini)", and
  after each change "itembox: ... written - the box of Lost in Nightmares holds N entries". In a story chapter
  "itembox: a story game - the panel's box is the game's own"; in the Mercenaries "itembox: no item box in this game
  (its mode 2)".

2026-11-04 (evening, the user's game running), the user: "does the open map at chapter start not work for the extras
game modes?"
- **Found**: it did not - by design: Open map at chapter start tested cGameSys's mode for the story's (0) alone. The
  log of that day has Lost in Nightmares begun at 19:16 with `OpenMapAtChapterStart=1` ("itembox: a game of Lost in
  Nightmares ...") and no "cheats: a chapter has begun" line. The episodes begin as a chapter does: their start
  (FUN_00BF55E0) sets mChapterStart, and the same play flow clears it as play begins (their stages 700-706 pass
  FUN_0071DCA0's test); each character's map is set up as in the story. Details: "The extra episodes" under "The map"
  in docs/cheats.md.
- **Built**: the maps open at the start of Lost in Nightmares (mode 3) and Desperate Escape (4) too; the Mercenaries,
  Reunion and Versus are still left alone. A start that opens nothing, with the switch on, is now logged (the first
  ten). The panel's help, README.md and README_BBCode.txt say so. No new signature, no new game call.
- **Checks**: built (no warnings); tested offline: `tests/test_cheats.cpp` 125 (both episodes - nothing before play
  begins, then this machine's character's map opened and not the partner's, shut by the key it stays shut -,
  Reunion, Versus and an unknown mode nothing; the build before fails 2, one opening in every mode 5),
  `tests/test_panel.cpp` 133. The other tests do not build `cheats.cpp` or `overlay.cpp` and were not rerun.
- **The next log should have**, at the start: "cheats: a chapter's start: cGameInfo +0x41C4, ... the play state
  cGameSys (sBioMain +0x1042C; its mode +0x58: 0 the story, 3 and 4 the extra episodes) keeps each player's map open
  (+0x554, 5); ...". As Lost in Nightmares begins (after its opening cutscene): "cheats: a chapter has begun -
  Chris's map opened (player 0)" (Jill's, player 1, when she is picked; Josh's or Jill's in Desperate Escape). A
  Mercenaries round: "cheats: a start in the game's mode 2 - no map opened (only a story chapter's or an extra
  episode's)". If instead "cheats: a chapter's start cleared with no character in play (the menus let it go) - no map
  opened" follows an episode's or a chapter's start, the game cleared it between two areas, and the timing needs
  another look.

2026-11-05, a user's report relayed by the user: "the mod was failing for them", with their log - the mod's first run on
Windows (`C:\WINDOWS\system32\d3d9.dll`, pid 4052, a new ini with the defaults).
- **Found**: Enigma on Windows resolves the game's imports through LdrGetProcedureAddress, as under Proton - the one
  thing the mod rested on - and every line up to Automatic QTEs's matched the user's own run of that morning (the same
  addresses, about 3-4x slower). Then, 1.9 s after the DLL loaded, the log stops: no boulder line, no CRASH line, no "5
  s after start", no "process exiting", no ExitProcess. The boulder's line is where the boot runs its first x87
  instruction and formats its first float. Under Wine that code faults only on a unit with the inexact result
  unmasked, an exception pending, or the invalid operation unmasked with the stack (nearly) full - states that would
  fault the game's own x87 code first, unless made during the boot by the system calls it makes (its log's file
  calls above all: a hook in them). Under Wine each such fault reached the unhandled-exception filter; on Windows a
  filter set since by another DLL, or a handler that ends the program, would leave no line. Not proven: what the
  user saw (closed, frozen, a message) and whether the log they sent was whole are not known. Details: "Windows" and
  "The x87 and SSE units" in docs/engine.md.
- **Built** (no game call, no new signature, nothing the cheats do changed):
  - `src/fpu.h`: a guard around the mod's own float code on the game's threads - the boot, each log line, the tick,
    every ImGui use - with every exception masked and the stack empty, the thread's own state put back after, exactly;
  - each log line formatted before its file is opened; a file call that changes the unit is said once;
  - a first report of any fault in the mod's code (or in a call it made) as Windows raises it, before any handler;
  - the crash filter reasserted as the boot starts (whoever replaced it named); the "N s after start: no tick yet"
    lines with where the main thread is, never held up by the log's lock.
- **Checks**: built (no warnings); `tests/test_fpu.cpp` 69 (new), `tests/test_crash.cpp` 24, every other test rebuilt
  and rerun: cheats 125, loader 90, menus 103, merc 74, discover 23, panel 133. The built DLL, loaded under Wine
  beside a stub `d3d9_orig.dll` with the inexact result unmasked: "boot: the game's thread brings x87 control word
  025F, status 0000, tags FFFF; SSE 1F80 - unmasked: inexact (the mod's own code runs with every exception masked
  and the stack empty, and the game's state is put back after)", the control word 025F again after the call, and at 5
  s "5 s after start: no tick yet; windows: none; the main thread is at ntdll.dll+0xD5F0 (thread 316); up its stack:
  kernelbase.dll+0x624E3, smoke.exe+0x8459, ...".
- **Installed** at 12:29 (sha256 7b71703c..., cmp OK).
- **The next log should have** (the user's, under Proton), after "crash logger installed (the game's own filter is
  chained once it sets one; a fault in the mod's code is logged as it happens, before any handler)": "boot: the
  game's thread brings x87 control word ..., status ..., tags FFFF; SSE ... - nothing in it that would fault a
  float" - the state the game hands the mod, for comparison with a Windows log.
- **Asked of the Windows user**: this build, then `RE5CabbyCodes.log` and `RE5CabbyCodes.prev.log` whatever happens,
  and what they saw. If it stops again: the last lines say why ("exception in the mod's own code, first reported
  ...", "log: the thread's x87/SSE unit changed ...", "crash: the top-level filter had been replaced (..., <module>)",
  or "N s after start: no tick yet; ...; the main thread is at ..."). With none of those, the process was ended from
  outside: Windows' own record (Event Viewer > Windows Logs > Application, "Application Error" for re5dx9.exe: the
  faulting module, exception code and offset) and their overlays and security programs.

2026-11-05 (later, found while building RE6CabbyCodes): **on Windows the panel could never draw.** `adopt.cpp`, copied
from RE4OG, knew the game's window by RE4's class name, "RE4HDWnd"; re5dx9.exe's is "MTFramework" (every log's
"overlay ready" line). Adoption - the device given vtables of its own - is the Windows path (the Steam overlay), and a
device for a window that is not the game's is left alone: on Windows no device was ever hooked, so even a run that
booted would have shown no panel (the log: "overlay: dx9: a device made by CreateDevice for window ... is not the
game's (its window class is 'RE4HDWnd')"). Proton never takes that path, which is why play never showed it.
- **Fixed**: the class is "MTFramework" (the user: "If you found a bug with RE5 for windows I want you to fix that as
  well"). `tests/test_adopt.cpp` checks it (6 checks; 2 of them fail on the old code).
- **Installed** at 13:52 (cmp OK). The v1.0.1 zip in `dist/` was made at 12:34, before this fix.
- **The next Windows log should have**: "dx9: device ... created on thread ... adopted - a vtable of its own with
  Present/..." instead of the "is not the game's" line.

## In-game checklist (not seen in play yet)

- a run on Windows (2026-11-05): the boot past the boulder's line ("cheats: the volcano's boulder: ..."), then the
  tick, the panel at the pause, the device's adoption (`adopt.cpp`, never run on Windows yet: "dx9: the game called
  Direct3DCreate9 ...: IDirect3D9 ... adopted"), the mouse and keys;

- the Mercenaries (2026-11-02), offline, The Mercenaries and Reunion: the panel's "The Mercenaries" section only in a
  match (its score, combo, kills of 150, time left beside the switches), none in a story chapter. Endless combo: kills
  far apart stay one combo, the score climbing at each kill (the HUD's count-up), the combo on screen climbing past
  10, 50; off, the combo ends 10 s after the last kill. Combo Bonus always on: "COMBO BONUS" up as the clock starts
  and staying, a kill's bonus 1,000 from the second in a combo; off, it runs out 30 s later. Kill enemies as they
  appear (switched on at the pause after the count-in): every Majini, dog, Plaga, Big Man, Chainsaw, Giant, Gatling,
  Executioner and Reaper dies as it comes, wherever it is, with its death animation; the kills, combo and score as for
  hand-to-hand kills - the clock +5 s at each; enemies keep coming while you stand still, from every part of the
  stage; the chickens killed too (2,000 points each); the stage clears at 150 (the results); the switch off again
  after the results, after a retry and after a quit;
  the game's own play unchanged after switching off mid-match (the areas send only where you stand again). Online:
  the section says they act only offline, and nothing changes;
- Kill enemies as they appear on a Chainsaw Majini that would get up again (2026-11-03; more likely in later matches
  of a session, as the dynamic difficulty rises): it falls dead at once and stays down, its kill counted (the log's
  "... a uEm22 with a second life to come ... - the life taken away before its hit" line), and no match stops one short
  of its 150 or 400 with "1 about";
- the door in chapter 3-2 where the game ended (2026-10-30): through it, or the log's FATAL line and the text of the
  game's message box, if one showed (the mod's preloads into the cutscenes' list fit it - fixed 2026-10-31);
- chapter 5-3's Wesker and Jill fight: a pause during it (the log: the loader asked into the sets' list, 4), then
  Wesker damaged - with One hit kills his part of the fight ends at the first hit: his exit cutscene (ev309_10) plays,
  then Jill alone; any cutscene after a pause in any chapter plays;
- the countdown at the end of chapter 3-2 (the dock): with Freeze countdown timer the HUD's clock stands still, and
  runs on from there once switched off; "Time left (s)" at the pause sets it, more or less; reaching Josh's boat ends
  the scene as the game does (no game over); with the switch off and no edit the game's own time-out still comes;
- the panel only at the pause menu (never in play or in a cutscene); F7 there and over the game; the mouse on the
  panel and back to the game when the pause closes;
- God mode (a Majini's hit, a fall); Partner (Sheva hit while the AI plays her); One hit kills (a Majini, a
  chainsaw man, a boss between phases); Infinite ammo (no reload, the HUD's count, a grenade thrown);
- chapter 2-3 with God mode: the jeep's gauge full through the bikes' and the trucks' attacks and a ram, and after a
  restart there; the jeep's gun with Infinite ammo firing on without steam or a lock-up (and overheating again with it
  off); an enemy's mounted gun still overheating;
- the Ndesu with One hit kills: a hit on its body letting every Plaga out, each Plaga dying of one hit, the main one's
  death ending the fight (its cutscene, the chapter's end) - and nothing stuck between its rounds;
- Irving (Transformed) with One hit kills (chapter 3-3's end): a hit on its body sending it down (a dive, then its
  mouth on the deck), a hit on the open core killing it (its death, the chapter's end); a tentacle breaking at one hit
  and dropping its item; Sheva's (AI) hit on the core leaving it to yours;
- a Reaper with One hit kills (chapters 5-2 and 6-3): a shot on a leg, its head or its back killing it at once (its
  death as at its weak spot - the body falls, the kill counted); a knife's slash and a kick too; a grenade killing it
  (not leaving it standing on its last point); one hanging from the ceiling not hurt by a shot (the game's rule), and
  killed by one once it is down from there; Sheva's (AI) shot leaving it on its last point, yours killing it; with the
  switch off, its body shrugging shots off again;
- Uroboros Aheri with One hit kills (the end of chapter 6-2):
  - Each orange core breaks at one shot, open or closed. Two broken bring the main core out in round one; one shot
    on the main core kills it, and the chapter ends (its results).
  - A core grown back breaks at one shot again.
  - If it reaches round two, one shot on a core during its doom attack breaks the attack off (no game over), and its four
    cores break at one shot each.
  - Sheva's (AI) shot on the main core leaves it on its last point; yours kills it.
  - With the switch off, the cores take their usual many shots.
- Steady aim: the laser's dot (pad) or the view (mouse) still while aiming and through a scope, a machine gun's
  burst on the dot, the mouse crosshair closed - and whether a pad's laser still moves with the gun's fire;
- Steady aim through a scope (the S75, the PSG-1, the Dragunov, the launchers; pad and mouse): the view perfectly
  still, zoomed in or out; a shot (the S75's bolt) and back on the same point; a reload; turning past the body's
  reach (the feet still step); the character breathing again once the scope is down, and with the switch off;
- Automatic QTEs: a Majini's grab broken at once, the cutscenes' prompts, the enemies' and bosses' dodges; doors,
  pickups, melee moves, Help and the partner's commands still waiting for a press;
- the volcano's boulder (6-3) with Automatic QTEs (keyboard and pad): its Push pressed to take hold of it, then nothing
  else - the two Pushes by themselves (a moment each), five Hooks landing one after another and the Ram, no prompt
  shown, the boulder over; the log's five Hook lines and the Ram's; with the switch off, the game's own;
- a crank with Automatic QTEs (keyboard and pad): taking hold of it with its own prompt, then no Turn prompt and no
  key - the crank winds by itself as fast as the game lets one turn, the gate or door all the way open in a few
  seconds; the character still holding it there until Release (the partner through first), the gate closing after;
  one that stays open by itself letting go on its own; the AI partner told to turn one (Command partner) as before;
  a laser mirror's turning still the player's;
- gold spent in the shop; the gold editor; the ini written again on the first start (`InfinitePoints` gone);
- the results: the counters while playing, Freeze play time, Deaths/Misses don't count, the editors - and that the
  index really is the character (0 Chris, 1 Sheva);
- the inventory: counts, throw away, Add of each tab (a weapon's model in hand when equipped);
- the Add lists (the case's and the box's, 2026-11-01): only the story's items, with no knife, Samurai Edge, Px4,
  Lantern, Flamethrower, L.T.D., RPG-7 NVS, RPG round, Herb [301]/[302] or bare Grenade launcher; the launcher's six
  loads; the herbs by their names alone. The Longbow greyed in Chris's Add and the Gatling gun in Sheva's (the hover
  says whose), each one's own Add making it. The box's Take of either into the other's case greyed; a ticked batch
  with one says "not taken: the game keeps it for the other character";
- Store at the pause (2026-11-01): greyed beside a scene's own weapon (the Lantern in 2-2's mine), the hover saying
  why and "Store is greyed for what the box does not take" under the case; a ticked batch with it says "not stored:
  the box takes only what the Add lists offer". A grenade launcher loaded with acid rounds, stored, is in the box under
  Weapons as G. launcher (ACD) with its rounds, and taken back it fires acid;
- the item box's tabs (both places): each kind's entries under its tab, the counts on the tabs, ticks under two
  tabs taken or thrown away together;
- Max all (both places): every stack in the box and every treasure held at 999 in one click, the guns as they were;
  at the pause kept after a retry; beside Item Management the screen's lists at 999 at once;
- the item box beside Item Management: usable at the screen's menu and while browsing; a count and Apply; x and
  Throw away; Add of each tab (stacking onto what is there, a gun loaded); the screen's lists changing at once;
  nothing made while an item is picked up; the box kept after leaving (the next chapter's start, the save);
- the item box at the pause: Store from each case (a stack onto the box's, a weapon with its upgrades), Take into
  Chris and into Sheva (a weapon with its rounds and upgrades, a stack what one slot holds), the cases' and the
  box's ticks; then a retry from the checkpoint: the case and the box still agree (nothing doubled, nothing lost);
  a gold set in play kept after a retry;
- what an area has loaded: the log's sLoadMan lines; at the pause, Take (and Add) of a weapon nobody carried as the
  area loaded - in the case at once, the box having preloaded it as the pause opened (no stutter, no "Loading ...")
  - equipped, fired, reloaded (its ammo's archive came with it), a launcher's rounds; a retry and the next area
  afterwards (the area's own load keeps or reloads it); the memory with a box full of weapons;
- the treasures (both places): the Treasures tab's kinds and counts as the game's own list shows them; a count set,
  x, Throw away of the ticked with box entries, Add of a few kinds - at the pause, then a retry (still there), the
  next checkpoint and the chapter's end (Item Management lists them, the shop buys them); beside Item Management,
  the screen's treasures list changing at once (no stale count: a sale right after a change); a treasure thrown away
  listed by the game at 0 (its mark of one found) and gone from the panel;
- the Mercenaries' clock frozen, its bonuses still adding;
- Open map at chapter start: a chapter begun from CHAPTER SELECT, from CONTINUE at a chapter's start, from NEW GAME and
  straight after a chapter's results - the map up in its corner as the title comes up (the map's own sound); shut with
  the key, it stays shut through the chapter's areas and opens again at the next chapter's start; a CONTINUE mid-chapter
  leaves it as the game has it; Sheva's (AI) never shown; nothing in the Mercenaries; a chapter whose first area has no
  map (2-3's jeep, if it starts there) - the map open in the next area; with the switch off, the game's own way;
  Lost in Nightmares and Desperate Escape (2026-11-04): the map up as play begins after the opening cutscene, shut with
  the key it stays shut through the episode's areas (Lost in Nightmares' second half, stage 705, too), the partner's
  never shown; nothing in Versus;
- Chapter Select at Item Management - after a chapter's results, after CONTINUE, after CHAPTER SELECT: the chapter list
  each time (no fatal error on its layout), the menus' music on, no main menu behind it (its list gone under the fade
  in, the header and the panel as the story menu's CHAPTER SELECT leaves them); back from it to the story menu and to
  the main menu as the game's own (the list back in); the button in its own window, there with the item box
  window folded; on the chapter list, a difficulty and a character picked and the chapter started; the box, the gold,
  the treasures and both cases as the screen left them (an item
  moved, bought or sold there still so at the next Item Management and in the chapter, nothing doubled or lost), a
  chapter's pickups kept; back from the chapter list to the story menu and CONTINUE (still as left); a chapter picked
  starts as usual (Item Management again, the button with it); the save file has it after the next chapter's results.
- Chapter Select at the pause (a story chapter, the pause menu's first page): Yes or No first (No: nothing happens);
  Yes: the game saves (its saving icon) with no question from it, the screen fades out as with QUIT, then the chapter
  list with no main menu behind it; the equipment, the box and the treasures as they were at the pause - in the
  chapter picked next and after CONTINUE; the panel's cases and box put away while it goes; no button at all in the
  Mercenaries, Lost in Nightmares, Desperate Escape or Versus (2026-11-03: not greyed, gone); greyed on another page of
  the pause menu (the options); Chapter Select beside Item Management going at one click again;
- Chapter Select on the Game Over screen (a death): its window alone at the top right, one click: the screen goes on
  as with QUIT (its own delay, the fade), the game back to the last checkpoint and saved, then the chapter list; with
  CONTINUE picked first, the button gone; no window in the Mercenaries nor in an extra episode;
- Chapter Select at a chapter's results (to the chapter list: seen 2026-11-01): the chapter just finished and its
  pickups kept (the save written at the results); after chapter 6-3 too - ready once their menu is up, its help
  saying the chapter list comes instead of the main menu, one click to the chapter list, the game's clear kept as
  their own continue leaves it; no window in the Mercenaries nor after an extra episode;
- the item box's moves at the pause through every way back: Take an item and Store another, then (a) RESTART, (b) die
  and CONTINUE, (c) QUIT answering No, (d) QUIT answering Yes, (e) Chapter Select at the pause - each item once, in the
  box or the case (as Item Management shows it, and in play); a stack stored partly used comes back as the checkpoint
  had it less what went in the box; a Take into a case full at the last save with nothing used since is refused;
- the extra episodes' boxes (2026-11-03): at the pause in Lost in Nightmares the panel's box named for it, empty at
  first, no Treasures tab, none of the story's items; Store an item from Chris's or Jill's case, Add some ammo - the
  file `RE5CabbyCodes.itembox.lost-in-nightmares.ini` beside the mod has them; then RESTART and a death's CONTINUE -
  each item once (in the box or the case); QUIT, start the episode again - the box as left, and Take puts its items
  in the cases (a weapon with its rounds and upgrades). Desperate Escape: its own box, apart from Lost in Nightmares';
  Take into Josh's and Jill's cases; no Longbow nor Gatling gun in its Add list. Back in a story chapter the story's
  box and treasures as they were, nothing of the episodes' in the story's saved equipment (Item Management shows it);
  no box in the Mercenaries;

## Next to build

Next to build (the user, 2026-10-29: "We will get to item management, key item / treasure lists, and weapon upgrade
editing soon, as well as mercenaries later"): the item box and its treasures are built (see "The item box", "The
treasures"); next the key items list, weapon upgrade editing (the
box's entries carry 8 level bytes, the case's cItems 9; each weapon's level limits are needed: the capacity table at
0x11C06A0 is not indexed by item code). The Mercenaries' cheats are built (2026-11-02, "The Mercenaries"). Not built
either: a Bonus Features window that unlocks without buying (the save's unlock
bits at 0x124..0x160), a records window. **Not wanted** (the user, 2026-10-29): a save file manager, a difficulty
switch, and anything for the exchange points (their switch and editor were removed).
