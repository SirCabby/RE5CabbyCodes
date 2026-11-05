# RE5CabbyCodes - the tests and the test bed

Part of the project's notes: `CLAUDE.md` is the guide and lists every section of `docs/`. The addresses are
build 11465250's. A section named in quotes ("The cranks") may be in another of these files:
`grep -n '^## ' docs/*.md`.

## Tests (offline, under Wine)

Run them in a scratch prefix (`WINEPREFIX=<scratchpad>/wpfx`, `env -u DISPLAY -u WAYLAND_DISPLAY`), never the
user's; `wineserver -k` afterwards. `tests/test_panel.cpp` (its build line in its header) draws the real panel
(`overlay.cpp` built in) headless with ImGui's test-engine hooks over a made-up game paused in a chapter - the
cheats and their editors, the results, both cases, the Add list's tabs - clicks Set, x and Add and checks the
requests, and (with `shots`) writes a picture of each step (with Steady aim
and Automatic QTEs, Open map at chapter start - a click flipping it and a second putting it back -, without the
exchange points; the Mercenaries' section (Endless combo, Combo Bonus always on, Kill enemies as they appear - each
click flipping its switch, and back - with the match's numbers) only at a match's pause, none at a chapter's, and no
chapter results there; the countdown switch in a chapter and in the Mercenaries, a stage's
countdown's seconds and their Set while one runs - nothing asked in play; one chapter clock and its Set; the cases'
rows (the sections above folded: the panel is taller than the test's window) only for the slots in
use, their Apply, Store and Store of the ticked, Store greyed beside the Lantern (a scene's own) and beside a
launcher whose load is not known, and asked for one kept as its load; the pause's item box under its tabs - each kind's count, each tab
its own entries, Other items coming and going with a key item - with Take into the character picked, x, a typed
count kept and Apply, its ticks kept under another tab and Take of the ticked of two tabs, All the tab's, the filter,
Take of what the area has not loaded asked all the same, none of what has no model nor while a Take waits for the
loader; the Armor tab; Add likewise, a pick preloaded, the item list's leftovers not offered (nor in the box's Add);
only the story's items offered (no knife, Mercenaries gun, scene-only weapon or rounds, floor herb or bare launcher;
the launcher's six loads; a herb by its name alone), the Longbow greyed in Chris's Add and the Gatling gun in Sheva's
(each one's own Add asking for it), and the box's Take of either greyed into the other's case;
the window beside Item Management, its tabs, no Take, its Apply all of a count typed under another tab, Throw away,
Add, nothing asked while the screen is busy; the Treasures tab at the pause and beside Item Management - the kinds
held in the item table's order, x, a typed count and Apply, a treasure ticked with a box entry (Take for the entry
alone, Throw away for both in one batch), All the tab's, the filter, the Add list's Treasures tab offering the game's
50 and not the gold, the score stars nor the leftovers, its Add, no tab when the list was not understood; Max all at
the pause and beside Item Management - every entry that stacks and every treasure held, under every tab and whatever
the filter shows, to its most in one batch, not a gun nor what is at its most already, the typed counts let go, nothing
while the screen is busy; Chapter Select beside Item Management between chapters, in a window of its own - in front
of the item box's when that one is focused, one click asking once (no question), still there and working with the
item box's window folded, not folding itself, disabled while it cannot go, the box's section put away while it goes,
not beside the main menu's Item Management; at the pause only where offered, in a window of its own in front of the
panel, a click asking Yes or No first (No: nothing asked; Yes: asked once), the question let go once it cannot go and
asked afresh after the pause closed, the cases and the box put away while it goes, the cheats still there; on the
Game Over screen and at the chapter's results its window alone (no panel), one click asking once - at the last
chapter's results too; an extra episode's pause - its own box named for it, no Treasures tab nor its Add list's, Take
into Chris or Josh and Jill, its Add list without what the game keeps for a character not in the episode (the Longbow,
in Desperate Escape the Gatling gun too), a box whose file could not be read shown alone with nothing asked, and back
in the story its box, its treasures and both weapons offered): 133 checks, all
passing on 2026-11-03 (the pictures of the question and of the Game Over window checked, and of the episode's box and
its unreadable file; the pause's picture with the
map's switch under its own heading; three broken builds of the Add lists caught: everything offered, nothing greyed for
the other character, a name's id counted over every item; three of Store's: never greyed, greyed by the case's id
(the launcher's too), its reason shown but the button live; and the episode's Add list offering the other
characters' weapons, 2 checks).
`tests/test_loader.cpp` builds
`inventory.cpp` and `itembox.cpp` over a made-up sLoadMan (the real offsets; stand-ins for its request and commit
that do to the lists what the game's do), a case and the box: what is here, loadable or never; the request (the
launcher's rounds' two classes) and one commit, nothing asked while the list holds an entry in state 0; an Add and
a Take that wait for the loader, the batch after the Take waiting its turn, both made once the archives are in;
dropped when the pause closes first; made without it when nothing loads in time; at a pause the box's contents
preloaded once, in one commit, its Take then made at once; an Add list's pick preloaded; nothing preloaded past the
cap (the Take then loads it); the leftovers neither carried nor loaded; the Longbow's Add and Take into Chris's case
refused (the entry kept, said so) and its Take into Sheva's made, a knife added to neither a case nor the box; and
the treasures over made-up lists of them
(each character's in sPlayer, each one's saved, cSystemInfo's and its copy) and a made-up Item Management screen - the
view the kinds this machine's character holds, a count set, a throw-away and adds made in every list by as much, the
mark of one found kept or cleared as the game's own rule has it and filled first by an add, the most of 999, nothing
while not paused; beside Item Management the profile's list alone and the screen's treasures list rebuilt; an archive
by its path (the menus', for Chapter Select) loaded only once its archive is in - not while asked for, failed, nor a
path it only begins; the moves between a case and the box at the pause in every saved copy of the case (stand-ins
for `menus::saves` and `menus::leaving`): the user's play of 2026-10-31 (a Take of the rocket launcher, a Store of the
Dragunov) through RESTART, a death's CONTINUE and QUIT without saving - each item once, the Take in each copy's same
slot or its first free one (the entry's slot made that one), a stack lessened by as much in each, the copy that had
the stored item in hand putting the one before in hand, a grenade launcher stored as its load (its model's rounds; the
view saying so) and matched in the copies by its model, one whose load cannot be read and the Lantern (a scene's own)
not stored (said so; the ammo ticked with it stored), a copy full when
saved giving up a stack used since and with none such the Take refused (nothing changed, said so), nothing moved while
Chapter Select goes or without the copies, an empty gun stored gone from the copies too; and the treasures' changes in
every saved copy too; and the loader asked into the area's sets' list whichever list the game chose last (put back
after): after a cutscene (its list let go, yet the list chosen last) nothing into the cutscenes' list, so the play
flow still finds no cutscene preloaded; nothing asked while any list is being chosen; and the extra episodes' boxes
(their files in a scratch folder of the test's): each its own, apart from the story's and from each other, no
treasures (a treasure's change asked there leaves the profile's alone); an Add, a Store and a Take written to its file
at once, the Store and the Take made in the saved state and the play flow's copy of it but not in the story's working
block or its checkpoint copy; both files read back as the mod starts again (a gun's rounds and levels); three
unreadable files (a line it does not know, the Lantern, a stack past 999) left as they are and the box unused; a
Store whose file cannot be written refused, an Add kept and written a second after it can be; no box in the
Mercenaries nor with the mode unknown - 90 checks, all passing on 2026-11-03 (four broken builds of the episodes'
boxes caught: the story's box in the episodes - the bug the user met - fails 15, the story's copies written from an
episode 2, a Store not writing the file first 1, the file never written 6; four broken builds of the Add lists'
rules caught: the tick's Add and the box's Take of the other
character's weapon, the tick's and the box's Add of what no list offers; four of Store's: what no list offers stored,
the case's 0x10C kept, every launcher kept as EXP, the view without its load; fourteen broken builds tried, each caught: A alone - the bug the user met -, without the play flow's copy
or the working block's checkpoint copy, without the hand, the model match, the stack's lessening, the victims or the
refusal, moving while Chapter Select goes or without the copies, the treasures not in the copies, a Take's entry not
its slot or always the live slot, an empty gun left in the copies; and three of the loader's: asking into the list
chosen last - the user's 5-3 crash - fails 8, not putting it back 2, looking only at that list for one being chosen
1). `tests/test_cheats.cpp` builds `cheats.cpp` and runs its
tick over a made-up character (its eight motion layers and its scope flag at the real offsets): Steady aim's hold
through a scope - this
machine's character's loops held while its scope is up, not a motion that plays once, an empty layer, one past those
the model runs, the AI's or the network partner's; a loop started over or sped up again held again; each speed back
as the scope comes down or the switch goes off, not one the game has replaced since; a character gone from play
forgotten without a write; and the stages' countdowns over a made-up sArea and its areas (the play flow, a stage laid
out as 209's - down to 0 - and one as 704's - down to its floor): the panel's countdown only while a stage's seconds
are above what it runs down to, never the play flow's field, nor an area past the list's first null; both stages'
patches in and out with the switch and on unload; the edit only at the pause with one running, kept at least a
second above the floor and within the HUD's most; and Automatic QTEs' answer at the managers' call over made-up
prompts (a unit and its request at the real offsets, the game's check stood in for): a crank's Turn (kind 7, category
5) pressed every frame for this machine's character and not drawn - its Release, the AI's and the network partner's,
the switch off, a request answered already, a laser mirror's Turn (kind 9) and the boulder's Push (kind 7, category 8)
left to the game's check, a grab's Break free still pressed - and a crank run as the game's code runs one (its ring of
32, its top speed, its most of 90) at 60, 144 and 20 fps: wound to its most in 2.5-2.8 s with its ring full, the Turn
never drawn, then held - not let go - until Release is pressed; and the volcano boulder's prompts (category 8, labels at
their offset): its Hooks and its Ram hidden and pressed once up 0.75 s (not at 0.74), a press of the player's own at
once, its Pushes AUTO's (hidden, not before 2 s), the AI's, the network partner's, the switch off, not found and a
Hook's label in another category the game's check - and the boulder run as the game runs it (its steps, points and
limits, each Hook's prompt made afresh, AUTO's answer to the Pushes) with the keyboard and a pad at 60, 144 and 20 fps:
pushed over (every step begun, five Hooks), nothing drawn, each punch's prompt up 0.75 s at least, in 8.7-14.5 s; with
the switch off (AUTO alone) the Hooks drawn and waiting, the push failed at their limit; and One hit kills on a Reaper
through its hit check's
hook (its check stood in for as the dump has it, the hit path's tail and subtract-health with the mod's hooks in their
places): the switch off, a shot on its body nothing, on its weak spot its damage, a grenade's blast to its last point;
on, a shot anywhere on its body (and on its weak spot, and a blast) all its health by the game's check's own yes, both
marks cleared after; hanging or holding a player nothing (the next shot on its feet kills); a hit past its body's parts
or without damage the game's; and One hit kills on Uroboros Aheri through its hit check's hook (its check and its cores'
wear stood in for as the dump has them): the switch off, a shot on an open core wears it, on a closed one only its
flinch, in its doom attack into the escape, on its main core its damage; on, a hit with damage breaks a core - open,
closed, flinching, the fourth, grown back, broken already - and nothing else, in the doom attack one hit breaks the
attack off and no core, its main core all its health, a hit without damage or past its cores' parts the game's; and Open
map at chapter start over a made-up profile (its chapter's start at the real
offset), play state (cGameSys in sBioMain: its mode, each player's map byte) and characters with their maps (open, the
toggle mode): nothing while no chapter starts nor as one is set to begin and loads; as its start is cleared (its title
up) in the story, this machine's character's map opened - its byte and its own flag - not the AI's nor the network
partner's; one the player shut stays shut until the next chapter's start, which opens it again; the switch off
nothing, and switched on mid-chapter it waits for the next start; a start let go in the menus, then a game without
one - nothing; the extra episodes' starts (Lost in Nightmares, Desperate Escape) as a story chapter's - nothing
before play begins, then this machine's character's map opened, and one the player shut staying shut -; the
Mercenaries', Reunion's, Versus' and an unknown mode's nothing; a map not set up, not in the toggle mode or none only
the byte; a player's number past the bytes nothing; unavailable nothing - 125 checks, all passing on 2026-11-04 (the
build before, without cranks, fails 8; one answering every kind 7 fails 1; one pressing Release fails 7; the Reaper:
one without its hook - the bug the user met - fails 4, one setting only its first mark 4, one answering yes itself 4, one marking any
part or any hit 1 each; the map: nine broken builds, each caught - opening as the start is set 1, without the mode's
test 5, the story's mode alone (the build before 2026-11-04 - the user's report from Lost in Nightmares) 2, the AI's
too 2, only the byte 3, the flag whatever the mode 1, no bound 1, kept armed out of a game 3, every
frame while on 7; Uroboros Aheri: four broken builds, each caught - no core broken 6, no doom attack's branch 2, the
core off by one 2, no damage test 2; the boulder: three broken builds, each caught - the build before (the user's
report: left at step 1 after 16 s) 11, no pace 8, the Pushes taken from AUTO 6). `tests/test_crash.cpp` builds
`crash.cpp` (with
the mod's own -O2) over a made-up fatal error that formats its text into 1 KB on its stack, as the game's does: only
FatalAppExitA and ExitProcess taken, never without the real one; the fatal error's text logged (its newline dropped)
with the code it came through - past the text, up to the function that failed - then the real FatalAppExitA with the
game's arguments; ExitProcess(0) one line, any other code with where it came from; an exception logged as before -
13 checks, all passing on 2026-10-30. `tests/test_menus.cpp` builds `menus.cpp` over a made-up game (sBioMain and its
cGameSys, sArea with the play flow and later the menus' aDemo - its start, its load by sLoadMan's field - and its
cDemoStartUp, Item Management's screen and a
stand-in for its save, cGameInfo's working block and its checkpoint copy, cSystemInfo and its copy at the real
offsets), the game's side played as its code does: offered only for the play flow's Item Management (one first seen past
its fade in too) in a story game (its mode, from the story's menu), ready only at its main step with idle panes,
offline; asked, the screen's own
save once, then the profile's and the working block's checkpoint copies made what the screen and the save left (the
rest of cGameInfo's copies untouched, nothing the screen changed put back), then the screen's own quit - nothing
called or written when it cannot go; the menus' archive asked for with the title's once aDemo has started, before its
load (the loader's field left alone before its start and past its load); the start-up menu's reason and its parked
choice as soon as its background (uIdTopMenu00) has started - not before - with the background as the story's choice
and CHAPTER SELECT's leave it (the main menu's list picked and on its way, the header shown, the panel's segment asked
for), the fade waited for, then CHAPTER SELECT from its first step; the background not understood: handed over at the
step that takes the reason, the background as it is; given up without a write on a screen not closing, a game
starting, another reason, the menus' archive not loaded (their load gone by before the mod's tick), a story flow not
parked; at the pause in a story chapter (the pause menu and its panes, the play flow's step and its copy of cGameInfo,
made up): offered, ready with this machine's pane at the pause menu's first page - not on another page, with two
players on this machine, online (greyed, saying why); not offered at all (no button, no why) in Lost in Nightmares or
Desperate Escape (their own modes, the return reason 9), in Versus, by the mode or the reason alone, from another top
menu choice, in the Mercenaries, nor with the pause's QUIT not understood - the screen left without it logged once;
asked: the pane's QUIT from its save question answered no (the option, the question's step, timer and
answer, then the step waiting for the save manager), the no-cancel byte, the menu's and the play flow's "on its way" -
nothing of the menus yet; the moves let go meanwhile; the play flow quitting waited for (the menus' area in while the
characters linger too), then the way back as from Item Management; given up, said so, when the QUIT does not go
through; the pause's QUIT answered yes (from the question's first step for a player the save manager does not let
save), the save waited for while the pane runs QUIT; at the Game Over screen: offered once it is shown, until a choice
is made, not at the step waiting for a partner's, nor in the Mercenaries or an extra episode - asked: QUIT's bit of
sBioMain's flags (under
the game's lock while it uses one), the screen at the step taking the choice made elsewhere, the choice none, waited
for while the bit is set, then the menus as from Item Management; at the chapter's results: offered, ready at their
menu - the last chapter's too (no quit of their own: their continue quits), said to be the last - not in the
Mercenaries nor an extra episode - asked: their quit set, its wait and timer 0, the prompts off, the step after their question's yes, waited
for while their quit is set and while the play flow is at its step after a game's end, then the menus; from the last
chapter's, with the characters gone, the same way to the chapter list; at the Game Over screen and the results,
whether it can go logged once a change (greyed and why, or ready, with the screen's steps), again for the next screen;
the saved copies for the item box (the play flow's copy only while it is a cGameInfo) - 103 checks, all
passing on 2026-11-03 (on 2026-11-03 a build offering it in every mode fails 10, one not testing the mode 3; a build without the copies, or with them before the save, fails them; so does one without the
bit, without the archive's check, or writing the bit before aDemo's start or past its load; or one leaving the
background's list, header or panel alone, handing over only at the reason's step or before the background has
started, or not waiting through the fade; and thirty-eight broken builds of the pause, Game Over and results paths
tried, each caught; on 2026-11-01 four more: the old rule greying the last chapter's results, no line for the next
screen, a line every tick, the last chapter not told to the panel). Each frame runs the panel's own rule
for what it keeps (`visible_now`, which `wants_draw` runs in the game) before drawing: the test used to call
`draw_frame` alone, so it never saw the rule that let the pause's box drop its ticks every frame (the user's play,
2026-10-29: "the item box has checkboxes for actions but they don't stay checked"); with that rule put back four of
its checks fail. The discovery is
proven in the real game by a headless run to the title (the log's discovery lines; `tools/sig.py --check` over the
dump for the signatures).

`tests/test_merc.cpp` builds `merc.cpp` (the Mercenaries) over a made-up play state (cGameSys at the real offsets,
slot 0's block), made-up enemies with their hit records, a character with a gun in hand, the guns' table and the
attacks' data, and a scenario with its area tasks' boxes, and runs the game's own code it leans on as the dump has it
- a kill's scoring with mode 2's bonus table, the combo's clock and its end, the Combo Bonus' clock and a pickup, each
clock reading its own `subss` site (so the patches' bytes decide), and a box's test of a place: out of a match (the
story's mode, no HUD), online or in another machine's game nothing written and no patch in; Endless combo off - a
combo over 10 s after its last kill, its bonus banked then - and on - kills 15 s apart one combo of 6, each bonus on
the score as earned, the clock held, and off again over 10 s on with nothing more to bank; Combo Bonus always on -
nothing in the count-in, started at the clock's start and held through 100 s, a kill's bonus 1,000 from a combo's
second, a pickup's held, off 30 s left, not again after the match's end or with no time left; Kill enemies as they
appear - nothing in the first 12 frames, then a shot of the M92F in a Majini's record and on a Reaper's weak spot
(every field: hit, its health, the data's flags with the whole-health bit, the part, the number, the attacker, the
gun's class, the place between it and the shooter, the centre), not the Reaper's soft body (its part), a chicken or
Josh, not again before 400 ms, a hit of the game's own not written over, after 25 only every 5 s, the dead dropped, eight a
frame, the M92F's shot with a knife in hand, nothing at the pause (and 12 frames again after it), with the clock out or
the shooter down, the switch to go off as a match begins, begins again (a retry) and ends; the stage's areas - two
boxes widened a thousandfold about their centres from far below to far above (far places inside by the game's own
test), a box with no extent and a cylinder untouched, each once, put back exactly when switched off and at time up, a
new stage's box widened and the old ones never written again, a box the game changed forgotten; since 2026-11-03 a
chicken hit too (every stage counts its chickens among its 150), and the hit's class the shooter's own (the game's
hand-to-hand kill, +5 s), the gun's when that rule is not found; since 2026-11-03 (evening) the Chainsaw Majini's
second life, run by the game's rule as the dump has it (the hit path lets it die only while "may not die" is clear,
its frame sets that until it has got up again, its brain lays it down at 1 to get up with 2000, hits doing nothing
while it lies there): one with a second life to come dead at its first hit (the life and that frame's "may not die"
taken with the hit), one without or already up dead at its hit with its bytes as they were, another class's bytes
untouched, and with the second life not understood the hits every 5 s after the 25 killing it once it is up - 74
checks, all passing on 2026-11-03. Its first run caught the shot's place check failing on the test's own x87 compare
of 0.1f. Built against merc.cpp with the life-taking taken out, three checks fail; with the old give-up after 25, two.

`tests/test_discover.cpp` runs the Mercenaries' discovery, and since 2026-11-03 Chapter Select's game modes, over the
game itself: the dump copied to its own addresses
in the test's image (based at 0x400000, a section reserving 0x401000..0x137F000 and the test's code above it: the
default linker script with that section put first - the header's command writes it to `tests/build/discover.ld`), and
the mod's real mt.cpp, merc.cpp and menus.cpp run over it by the signatures of src/sigs.h: every signature's name its own, MT's
4355 class records and the classes asked for (vtables, sizes), discover_match (the play state, both clocks' patch
sites), discover_kills (the hit record, the parts, the M92F's shot, a knife no shot, the hand-to-hand kill's rule
and its 5 s), the stages' areas and the Chainsaw Majini's second life (its two bytes, "may not die", the health it gets
up with, the lie-down action; its frame uEm22's own slot 62, the lie-down uEmIntel22's own slot 35), and menus.cpp's
discover_flow and discover_menus (the story's mode +0x58 that Item Management's step tests, through sBioMain +0x1042C as
the menus' own field; the return reason +0x18, the top menu's choice +0xC, the story menu's +0x1C), and the game's mode
for the item box (game.cpp's discover_mode, its reads replayed: kSigEpisodeCommit's cGameSys field and the game's own
test of the extra episodes, 3 and 4, agreeing with the menus' field and mode) - 23 checks, all passing on 2026-11-03. Built against the sources with the
second `kSigAreaFrame` put back - the cause of the user's failed areas - three of them fail (the names, the areas
understood, their layout); tools/sig.py had found each pattern once.

**The dump.** `DumpImage = 1` in the ini makes the mod write `re5dx9.dumped.exe` (the game's five sections as they
are in memory, raw offsets == RVAs) and `re5dx9.imports.txt` (each import slot's `module!export`) beside the DLL
once the game is unpacked. A headless run can also read it out of `/proc/<pid>/mem` (the game runs in pressure
vessel's user namespace, which a process of the same user may read). `ghidra_proj/` (gitignored) holds the dump of
2026-10-28, the fixed PE, its Ghidra project `RE5`, `re5dx9.asm` (a full objdump: "who touches this field" is a
grep) and `classes.txt` (every MT class record).

**Always install after a change.** Once a change builds and its checks pass, run `make install` yourself (check the
game is closed first - under Proton its process is named `-windowed`, so `pgrep -x re5dx9.exe`, which the Makefile's
own guard runs too, does not see it: `pgrep -f 'steamapps/common/Resident Evil 5/(Launcher|re5dx9)\.exe'` does; if
the game is running, say so and install as soon as it closes - the install's rename would leave the running game on
the build it loaded, its file `d3d9.dll (deleted)` in `/proc/<pid>/maps`, as on 2026-10-31), confirm with
`cmp build/d3d9.dll "$GAME_DIR/d3d9.dll"`, and say in the reply that it is installed. Installing is not a release:
the version stays as it is.

The VERSION file is the user's: never create or change it, never run `make version`/`make rev`, never suggest a
bump.

The log (`RE5CabbyCodes.log`, the run before it in `.prev.log`), the ini (`RE5CabbyCodes.ini`) and the ImGui
layout (`RE5CabbyCodes.imgui.ini`: the window `###RE5CC panel` and which sections are folded) sit beside the DLL.
`Trace = 1` adds diagnostics (every name Enigma's loader resolves, every system, every `uId*` screen unit that
comes and goes); `AlwaysShow = 1` draws the panel everywhere; `Watch = 0x...:4` logs values; `Disable =
overlay,tick,game,input` bisects a fault. Keep them on only for a diagnostic run and reset them after.

## Testing without playing

The user play-tests: never drive the game in their session, never send it input or take screenshots there. A
headless run is only ever in a gamescope of our own with its own prefix, in `~/.cache/re5cc-test/` (outside the
repo): `run_slr.sh SECONDS` runs a copy of the game (`game/` holds `re5dx9.exe`, `Launcher.exe`, the `.CFG`, the
mod's `d3d9.dll` and ini; `nativePC_MT` is a symlink to the real install) the way the user's Steam does - Steam
Linux Runtime 4 + Proton Experimental, `SteamAppId=21690`, their launch options (below) in a `gamescope --backend
headless`, prefix `pfx/` (its `config.ini` as theirs). `RE5CC_GS_ARGS`, `RE5CC_EXE` (`re5dx9.exe` to skip the
launcher), `RE5CC_EXE_ARGS` change those; `RE5CC_DXVK=1` turns DXVK's log on (`game/re5dx9_d3d9.log`: every
`ResetSwapChain`, every display mode set - Proton sets `DXVK_LOG_LEVEL=none`); `RE5CC_WINEDEBUG=+loaddll` passes a
WINEDEBUG. `shot_run.sh SECONDS NAME...` takes screenshots of that display only (`guard.sh`'s `shot`, which refuses
unless the headless gamescope is ours and up).

**The user's launch** (Steam launch options, 2026-10-28): `__NV_DISABLE_EXPLICIT_SYNC=1 GAMESCOPE_ARGS="-w 1920 -h
1080 -W 1600 -H 900 -r 120 --force-windows-fullscreen -S fit" ~/.local/bin/gamescope-launch %command% -windowed -w
1920 -h 1080` - their wrapper runs the game in a nested gamescope (its log: `~/.cache/gamescope-launch/`). Steam
starts **`Launcher.exe`** (a console program): `CreateProcessA("re5dx9.exe", its arguments)`, and when
`GetLastError()` is then nonzero (and not 0x715) it prints `res = %d, err = 0x%08x, %s` and waits for a key - under
Proton a successful start leaves 0xCB, so its console always reads "res = 1, err = 0x000000cb, Environment
variable not found." (harmless: res 1 = started). The game ignores `-windowed -w -h` (no such strings in it); its
display comes from `Documents/CAPCOM/RESIDENT EVIL 5/config.ini` in the prefix (`[DISPLAY] Resolution`,
`FullScreen`; first written 1280x720 fullscreen).

**The reset storm** (the user's first launch, 2026-10-28: a black screen, then a crash): the game's fullscreen at
1280x720 under gamescope's `--force-windows-fullscreen` with `-w 1920 -h 1080` resets the device over and over
(~20 a second; DXVK sets 1280x720 twice each time), **with or without the mod** (without: 311 resets in 45 s,
DXVK's log). After ~40 s it crashed at 0x5F32D9 (a draw command reading a null target's +0x1C, called from
0x43F097 with `[sRender+0x17530]` - the "LightMask" target, which sRender's reset (0x452E80) releases). At
1920x1080 - windowed or fullscreen - the game resets once and holds. The user's `config.ini` was set to **1920x1080,
FullScreen=OFF** on 2026-10-28 (what their `-windowed -w 1920 -h 1080` asks for; the original is
`~/.cache/re5cc-test/user-config.ini.orig`). The mod's log now says "the game keeps resetting the display" once and
counts every 100th (`log_reset`, `overlay_dx9.cpp`). sRender's reset runs when any bit of `sRender+0x196CC` is set
(Present answering D3DERR_DEVICELOST sets 0x10; `0x458D60` sets 2 in fullscreen); which of them the storm sets
was not traced.

**A headless run's end has broken the GPU twice** (2026-10-29, driver 615.78.08). The run's `timeout` stopped
gamescope at 16:03:23, and the kernel logged `Xid 51 ... name=gamescope-pw, BAD_TSG encountered` and `Xid 154 ... GPU
recovery action changed ... to 0x1 (PF FLR)`. From then on every new GPU device failed (`NV_ERR_RESET_REQUIRED`;
`vkCreateDevice` -3 in gamescope and vulkaninfo) until a reboot, for the user's own games too. So `run_slr.sh` now
refuses to start with an Xid in the kernel log since boot, ends the game with the test prefix's own `wineserver -k`
(Proton's) when the time is up and lets gamescope exit by itself (a signal only if it hangs 40 s), and checks the
kernel log and `vulkaninfo --summary` afterwards. Four runs after the reboot ended clean; the fifth (17:46:47, 30 s)
did not: the game ended by `wineserver -k`, gamescope exited with status 0 - and at its exit `Xid 51 ...
name=gamescope-wl, BAD_TSG` and `Xid 154` again, the GPU needing a reboot. So the clean ending does not prevent it:
**a headless gamescope on the NVIDIA GPU can wedge it at any run's end** - and the user's own does too: on 2026-10-31
at 20:13:07 their gamescope (their launch's, nested) exited at the end of their session with `Xid 51 ...
name=gamescope-wl, BAD_TSG` and `Xid 154`, and every start after failed in gamescope itself (`vkCreateDevice` -3, its
log in `~/.cache/gamescope-launch/`) until a reboot, with nothing of ours running (the session's last command at
17:54): the driver's fault at a gamescope's exit, whoever's. When the game will not start, look for an Xid first
(`journalctl -k -b -o short-iso | grep -i xid`). Run one only with the user's word, or once
the test bed renders on the CPU (lavapipe: `vulkan-swrast` and `lib32-vulkan-swrast`, not installed on 2026-10-29;
`VK_ICD_FILENAMES` to its icd for gamescope and the game - untried). `guard.sh`'s `user_game_running` looked for
`re5dx9.exe` in command lines, which the user's game never shows (it is `-windowed`; Steam's lines name `.../Resident
Evil 5/Launcher.exe`): it now looks for `steamapps/common/Resident Evil 5/(Launcher|re5dx9).exe`.

**The user's gamepad reached a headless run** (2026-10-29, 17:42): the title's `uIdTitle01` came and went a dozen
times in 35 s - presses - while the user had their wired Xbox 360 pad (045e:028e, `js0`) in use. Proton's `winebus`
reads gamepads from the system (SDL, evdev input, hidraw) whatever display the game is on. Nothing went past the title
(the save unchanged), but a pad could start a game. The test prefix's `winebus` service key now has `"Enable
SDL"=dword:0`, `"DisableHidraw"=dword:1`, `"DisableInput"=dword:1` (`pfx/pfx/system.reg`; the original kept as
`system.reg.bak-20261029-pad`): a `+hid` run then logged "SDL devices disabled in registry", "UDEV hidraw/input devices
disabled in registry" and made only Wine's own keyboard and mouse (845E:0001/0002).

**Never let a headless run go past the title.** The copy runs as app 21690 under the logged-in Steam client, and
RE5 saves to Steam Cloud (`savedata.bin`, one file) and unlocks Steam achievements: a headless game started would
write the user's real save and could unlock achievements. The user has a save since 2026-10-29 (backed up in
`~/.cache/re5cc-test/save-backup-20261029/`). A run to the title proves the mechanisms (proxy, the loader hook,
discovery, every hook and patch put in - `/proc/<pid>/mem` shows the patched bytes; the game's process is the one
named `-windowed`, its command line being the launcher's arguments - the tick, the overlay, input); everything in
play is checked against the game's code and left to the user's play. About 90 s idle at the title, the game starts
its attract movie (`uIdTitle01 gone`, GStreamer in `gs.log`): keep a run to 100 s. Make sure no headless gamescope
is left afterwards (`pgrep -f "[g]amescope --backend headless"`), and that the user's
`~/.local/share/Steam/userdata/*/21690/remote/savedata.bin` and `remotecache.vdf` are as they were (hash, time).
