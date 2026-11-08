# RE5CabbyCodes — project guide

A client-side mod for **Resident Evil 5** — Steam app 21690, `re5dx9.exe` build **11465250** (PE32, image base
0x400000, relocations stripped; wrapped by **Enigma Protector** under SteamStub's `.bind`; MT Framework 1;
Direct3D 9) — cross-built on Linux with mingw-w64. It draws a Dear ImGui panel beside the game's **pause menu**:
- **The cheats**: God mode (the 2-3 jeep too), Partner takes no damage, One hit kills, Infinite ammo (no overheating
  for a player's mounted gun), Steady aim (through a scope too), Automatic QTEs (cranks and the volcano's boulder
  too), Open map at chapter start (an extra episode's start too), Infinite money with a gold editor, and Freeze
  countdown timer (the Mercenaries' clock and the story's countdowns) with a time-left editor.
- **The Mercenaries** (a section shown only in a match): Endless combo, Combo Bonus always on, and Kill enemies as
  they appear (every area made to send; each kill a hand-to-hand one, +5 s; it lasts one match).
- **The chapter's results**: Freeze play time, Deaths don't count, Misses don't count, the chapter's clock, and each
  character's counters.
- **Each character's inventory**: counts, throw away, Add.
- **The game's item box and its treasures**: counts, Max all, throw away, Take into a case, Store from one, and Add,
  under the Add list's tabs.
- **Lost in Nightmares' and Desperate Escape's own item boxes** (the game has none there): the same, no treasures, each
  kept by the mod in a file beside it. The box at the pause goes by the game's mode (`game::Run::mode`).

Beside **Item Management** (between chapters) a window of its own edits the box and the treasures.

A **Chapter Select** button goes to the start-up menu's chapter list. It is offered in a story game only (no button in
the extra episodes, Versus or the Mercenaries):
- at Item Management (it saves what was changed there);
- at the pause in a story chapter (Yes or No first; the pause menu's own QUIT, with its save answered yes);
- on the Game Over screen (its own QUIT);
- at a chapter's results (their own quit).

Every switch is written back to the ini. `README.md` is the user doc (`README_BBCode.txt` the same for forums).

The engine-independent parts come from `../RE4OGCabbyCodes`, which is also 32-bit D3D9: the overlay, the D3D9 hooks
and the Windows "adopt" path, the input guard, config, the log, the crash logger and `mem.h`. The Enigma side (a dump
of the unpacked image, the hook on the wrapper's own GetProcAddress import) follows `../RE1-1996CabbyCodes`. The
conventions (windows per screen, folding, F7, item tabs) follow `../RE4CabbyCodes`.

**This file is the guide; what was reverse-engineered is in `docs/`** (see "Notes" below; read the section a task
needs). Keep this file short. A finding goes into its `docs/` section. A new feature gets a section there, one line
under "Status", and its report in `docs/history.md`.

## Build & deploy

```sh
make            # -> build/d3d9.dll (config.mk sets GAME_DIR; gitignored)
make install    # installs it; a d3d9.dll there that is not ours is kept as d3d9_orig.dll (once)
make uninstall  # removes the mod (and puts a kept d3d9.dll back)
make package    # dist/RE5CabbyCodes_v<version>.zip (no VERSION file: 0.0.0)
# reverse engineering (the exe on disk is compressed - everything works on a dump of the running image):
python3 -I tools/fix_dump.py --dump ghidra_proj/re5dx9.dumped.exe --imports ghidra_proj/re5dx9.imports.txt \
    --out ghidra_proj/re5dx9.fixed.exe                       # a PE Ghidra and objdump read
python3 -I tools/sig.py --dump ghidra_proj/re5dx9.dumped.exe --check      # every signature in src/sigs.h
python3 -I tools/mt_classes.py --dump ghidra_proj/re5dx9.dumped.exe [--grep uIdPause | --vtable uPlayer]
python3 -I tools/mt_props.py --dump ghidra_proj/re5dx9.dumped.exe --asm ghidra_proj/re5dx9.asm cItem cGameInfo
python3 -I tools/gen_items.py --dump ghidra_proj/re5dx9.dumped.exe \
    --arc "$GAME_DIR/nativePC_MT/Image/Archive/Msg2Resource_e.arc" --out src/items.h
python3 -I tools/save.py --dump ghidra_proj/re5dx9.dumped.exe --asm ghidra_proj/re5dx9.asm --missing  # a save, read
tools/ghidra/run.sh dec 0xb64eb0           # decompile (several addresses per run; x:ADDR for references)
```

**Always install after a change.** Once a change builds and its checks pass, install it yourself:
- First check the game is closed. Under Proton its process is named `-windowed`, so `pgrep -x re5dx9.exe` (the
  Makefile's own guard) does not see it; use `pgrep -f 'steamapps/common/Resident Evil 5/(Launcher|re5dx9)\.exe'`.
- If it is running, say so and install as soon as it closes. An install's rename leaves a running game on the build
  it loaded (`d3d9.dll (deleted)` in `/proc/<pid>/maps`, as on 2026-10-31).
- Run `make install`, confirm with `cmp build/d3d9.dll "$GAME_DIR/d3d9.dll"`, and say in the reply that it is
  installed.

Installing is not a release. The VERSION file is the user's: never create or change it, never run `make
version`/`make rev`, never suggest a bump.

**Beside the DLL**:
- `RE5CabbyCodes.log` (the run before it in `.prev.log`);
- `RE5CabbyCodes.ini`;
- `RE5CabbyCodes.imgui.ini` (the window `###RE5CC panel` and which sections are folded);
- `RE5CabbyCodes.itembox.lost-in-nightmares.ini` and `RE5CabbyCodes.itembox.desperate-escape.ini` (the extra
  episodes' boxes, once used).

**Diagnostic ini keys**: `Trace = 1` (every name Enigma's loader resolves, every system, every `uId*` screen unit that
comes and goes), `AlwaysShow = 1` (the panel everywhere), `Watch = 0x...:4` (logs values), `Disable =
overlay,tick,game,input` (bisects a fault). Keep them on only for a diagnostic run, and reset them after.

**The dump.** `DumpImage = 1` makes the mod write `re5dx9.dumped.exe` and `re5dx9.imports.txt` beside the DLL once the
game is unpacked. The first holds the game's five sections as they are in memory (raw offsets == RVAs); the second
each import slot's `module!export`. A headless run can also read the image out of `/proc/<pid>/mem`.

`ghidra_proj/` (gitignored) holds the dump of 2026-10-28, the fixed PE, the Ghidra project `RE5`, `re5dx9.asm` and
`classes.txt` (every MT class record). `re5dx9.asm` is a full objdump, so "who touches this field" is a grep.

## Tests (offline, under Wine)

Run them in a scratch prefix (`WINEPREFIX=<scratchpad>/wpfx`, `env -u DISPLAY -u WAYLAND_DISPLAY`), never the
user's, and `wineserver -k` afterwards. Each test's build line is in its header. What each one checks in full (and the
broken builds each caught) is in `docs/testing.md`.

| Test | Builds, over a made-up game | Covers | Checks |
|---|---|---|---|
| `tests/test_panel.cpp` | the real panel (`overlay.cpp` built in), headless, with ImGui's test-engine hooks | the cheats and their editors, the Mercenaries' section, the results, the cases, the box's tabs and ticks, Take, Store (greyed for what no list offers, a launcher by its load), Add (the story's items only, the Gatling gun and Longbow for their owner), Max all, Treasures, the extra episodes' boxes, the Item Management window, and Chapter Select in each place | 133, all passing 2026-11-04 |
| `tests/test_loader.cpp` | `inventory.cpp` and `itembox.cpp`, over sLoadMan at the real offsets, a case and the box | what is loaded, the request and commit, waiting Adds and Takes, preloads and their cap, the one-character weapons and what no list offers refused (a Store too), the treasures in every list, the pause's moves in every saved copy of the case (a grenade launcher stored as its load), the list the loader is asked into, and the extra episodes' boxes (their own, their files, the copies an episode restarts from) | 90, all passing 2026-11-03 |
| `tests/test_cheats.cpp` | `cheats.cpp`'s tick and hooks, over made-up characters, areas, prompts and enemies | Steady aim's hold through a scope, the stages' countdowns, Automatic QTEs (cranks and the volcano's boulder at 60, 144 and 20 fps), One hit kills on a Reaper and on Uroboros Aheri, and Open map at chapter start (the extra episodes' starts too) | 125, all passing 2026-11-04 |
| `tests/test_crash.cpp` | `crash.cpp` (the mod's -O2), over a made-up fatal error | the game's own ends logged, then passed on; the first report of a fault in the mod's code (before any handler); another thread's place (`where_is`); a replacing filter named | 24, all passing 2026-11-05 |
| `tests/test_fpu.cpp` | `log.cpp` with `fpu.h` (the mod's -O2; fopen wrapped as a hook in the file calls), each x87/SSE state in a child | the boot's first float on nine states, bare and guarded; the state put back exactly; logf on them; a file call changing the unit; `logf_nowait` past a held lock | 69, all passing 2026-11-05 |
| `tests/test_merc.cpp` | `merc.cpp`, over a made-up play state, enemies with hit records, a gun in hand and a stage's area boxes, the game's scoring and clocks run as the dump has them | Endless combo, Combo Bonus always on, Kill enemies as they appear (the shot written as the shooter's hand-to-hand kill, who is hit - the chickens too -, its pace and its every-5-s retry, one match, a Chainsaw Majini's second life taken) and the areas widened and put back | 74, all passing 2026-11-03 |
| `tests/test_discover.cpp` | the real `mt.cpp`, `merc.cpp` and `menus.cpp` discovery over the dump, copied to its own addresses (a linker script: see its header) | every signature's name unique, the class records, the Mercenaries' play state, hit record, hand-to-hand rule, stages' areas and the Chainsaw Majini's second life, the story's mode and menu fields Chapter Select goes by, and the game's mode for the item box, found as the game has them | 23, all passing 2026-11-03 |
| `tests/test_adopt.cpp` | `adopt.cpp`, with RE5CC_ADOPT set (the Windows path), over message-only windows | the game's window known by its class, `MTFramework` (until 2026-11-05 RE4OG's `RE4HDWnd`: no device was adopted on Windows, no panel) - not RE4's, not a tool's, not none | 6, all passing 2026-11-05 |
| `tests/test_menus.cpp` | `menus.cpp`, over sBioMain, sArea, aGame/aDemo, cDemoStartUp, Item Management, the pause menu, Game Over and the results | Chapter Select from each place (in a story game only), the front end's archive, the start-up menu's background, and the saved copies | 103, all passing 2026-11-03 |

`test_panel` with `shots` writes a picture of each step. Each frame it runs the panel's own keep rule (`visible_now`)
before drawing: without it, the test missed the ticks the pause's box dropped every frame. In the real game the
discovery is proven by `tools/sig.py --check` over the dump and by a headless run to the title (below).

## Testing without playing

- **The user play-tests.** Never drive the game in their session, send it input or take screenshots there.
- **A headless run needs the user's word in that conversation.** On this NVIDIA driver a gamescope's exit can wedge
  the GPU until a reboot (`Xid 51 ... BAD_TSG` + `Xid 154`). It did so twice at test runs' ends (2026-10-29) and once
  at the user's own session end (2026-10-31). When the game will not start, look for an Xid first: `journalctl -k -b
  -o short-iso | grep -i xid`. CPU rendering (lavapipe) is untried.
- **Never past the title.** The copy runs as app 21690 under the logged-in Steam, so a game started would write the
  user's Steam Cloud save (`savedata.bin`) and could unlock achievements. Keep a run to 100 s: the attract movie starts
  after ~90 s idle.
  - Afterwards, check that no headless gamescope is left (`pgrep -f "[g]amescope --backend headless"`).
  - Check that `~/.local/share/Steam/userdata/*/21690/remote/savedata.bin` and `remotecache.vdf` are as they were
    (hash, time). The user's save is backed up in `~/.cache/re5cc-test/save-backup-*`.
- **The test bed** is `~/.cache/re5cc-test/`, outside the repo:
  - `run_slr.sh SECONDS` runs a copy of the game in a `gamescope --backend headless`: SLR 4 + Proton Experimental,
    the user's launch options, prefix `pfx/`.
  - `shot_run.sh` takes screenshots of that display only.
  - `guard.sh` refuses unless the gamescope is ours.
  - The prefix has winebus's gamepad buses off, since the user's pad once reached a run.
  - Details, the user's launch, the reset storm and the GPU: `docs/testing.md` "Testing without playing".
- A run to the title proves the mechanisms: the proxy, the loader hook, the discovery's log lines, the hooks and
  patches (`/proc/<pid>/mem`), the tick, the overlay and input. Everything in play is checked against the game's code
  and left to the user's play.

## ⛔ Rules

- **Never patch `re5dx9.exe` on disk.** Every code patch is in memory, prepared from a signature with its bytes
  re-checked (`mem::Patch::prepare`), put in and taken out with its switch, reverted on unload.
- **Nothing before the game's code is unpacked.** Enigma unpacks after every DllMain, so DllMain only loads the real
  d3d9.dll and hooks the wrapper's own import slot. Everything else waits for the game's first `Direct3DCreate9`
  (`boot.cpp`). RE3Controls (a mod for this build) notes that patching the code while Enigma is still unpacking
  makes the game quit with "Program integrity check failed".
- **Never use absolute addresses or hard-coded offsets in code.**
  - Classes are found by name in MT Framework's own type records (`mt.h`).
  - Fields and functions are found by the signatures in `src/sigs.h`: read out of the matched instructions, then
    cross-checked against each other and against the classes' sizes.
  - The addresses in `docs/` document build 11465250 only.
- **Game memory from the tick or a hook only.** The panel runs on the render thread: it flips atomics and posts
  requests, and the tick applies them (`dispatch.cpp`). Hooks touch only the objects the game handed them.
- **Edits only while the pause menu is up**, or beside Item Management while its panes are idle (`itembox.cpp`: its
  menu, or browsing the case or the box) - the screen that keeps them. Chapter Select (`menus.cpp`) is asked there
  too. Its last writes (the start-up menu's return reason and choice) follow in the menus it leads to, as they come
  up, and only after that button.
- **Game calls: only the approved kinds.** **Ask before adding any other call.**
  - The siblings' user approved: the inventory's own functions (RE4OG's attache case: PutInCase / get / erase), the
    item box's Take and Store, the difficulty switch, the save files' delete and refresh, and event hooks.
  - This mod calls the inventory's own **`cItemManager::add`** and **`cItem::clear`**.
  - Item Management's own **rebuild of its box list** (FUN_00A04500; the user, 2026-10-29: "you may add the new
    hook").
  - Item Management's own **rebuild of its treasures list** (FUN_00A048A0; 2026-10-30: "Yes, add the call").
  - Item Management's own **save of its cases** (FUN_00A079E0; 2026-10-31: "let's attempt to call the game's save
    feature to save the inventory before landing back at chapter select").
  - **sLoadMan's request and commit** (FUN_007D5820, FUN_007D4A10; 2026-10-29: "yes make it work for any item").
  - Details: `docs/items.md` "The game's calls".
  - Field writes, vtable hooks and verified byte patches need no approval. Automatic QTEs re-points the game's own
    call of its prompt check at the mod, which calls on to the check with the game's arguments, as a vtable hook
    does. It also patches two tests of a setting. Neither is a new call.
- Back up `Steam/userdata/<id>/21690/remote/` before experiments that write saves.
- The user commits every repo themselves - do not `git commit`/`push` unless asked.

## How it works (in brief; `docs/engine.md` has the detail)

- **The proxy is d3d9.dll.** Of the DLLs the wrapper imports, it is the one that loads from the game's folder
  everywhere, with no launch option: on Windows the program's folder comes first, and Proton marks DXVK's d3d9 native.
  The game's `steam_api.dll` is inside the exe (an Enigma virtual file), so there is no Steam DLL to stand in for. The
  proxy forwards the 16 other exports to `d3d9_orig.dll` (another mod's) or the system's.
- **Enigma's loader.** Enigma resolves the game's imports through ntdll's `LdrGetProcedureAddress`, which it gets
  through the exe's own GetProcAddress import. DllMain re-points that import slot (`boot.cpp`). The mod's wrapper then
  hands the game the mod's `Direct3DCreate9`, `DirectInput8Create`, `SetUnhandledExceptionFilter`, `FatalAppExitA`
  and `ExitProcess`.
- **Boot** happens at the game's first `Direct3DCreate9`, on the main thread. It reads the sections and MT's class
  records, runs the discovery (`game::discover`, `cheats::install`, `inventory::discover`, ...), hooks the tick, and
  writes the dump when asked. A class's vtable is the one whose slot 4 (getDTI) answers its record. `src/sigs.h` has
  134 signatures; a name may be used once (`game::sig_one` finds a name's first: `tools/sig.py --check` fails a
  second).
- **The tick** is sBioMain's own vtable slot 6 (its frame), on the main thread. The panel draws in Present from the
  snapshots the tick publishes, and sends its requests through `dispatch.cpp`. The window procedure (class
  `MTFramework`) is subclassed for ImGui.
- **Input** goes through the DirectInput guard. While the panel is up the mouse is let go, and the keyboard reads
  nothing while a field has focus (but Esc and Alt). The pad (XInput) is the game's own and is left alone.
- **Screens** are `uId*` units. The pause is `uIdPause00`'s open byte (+0x3C): the unit itself lives all chapter.
  `uIdItemBox00` is Item Management, `uIdGameover00` the Game Over screen, `uIdChapEd00` the results.
- **Windows** is audited by design. One user's run (2026-11-05) showed Enigma resolves the game's imports through
  `LdrGetProcedureAddress` there too (the one thing the mod rests on), then stopped without a line at the boot's
  first float (`docs/engine.md` "Windows"). The device is adopted only for the game's window, known by its class
  `MTFramework` - RE4OG's `RE4HDWnd` until 2026-11-05, which kept the panel off on Windows (`docs/history.md`).
- **Floats**: the mod is x87 code on the game's threads (the device keeps FPU_PRESERVE): `fpu::Guard` runs the boot,
  each log line, the tick and ImGui with every exception masked and the stack empty, and puts the thread's own back.

## Notes (`docs/`)

**`docs/engine.md`**
- Why d3d9.dll, and Enigma's loader.
- Threads and the tick.
- Windows: the first run (a user's, 2026-11-05) and what is open about it.
- The x87 and SSE units: what faults the mod's floats, the guard (`fpu.h`).
- MT Framework's class records.
- The systems and the units.
- The characters: health, who drives a player (+0x2DA8), the damage slots, an enemy hit's path through
  FUN_009DEA50.
- The mouse, and how the user's gamescope broke it.
- The game's own ends: its fatal error and exits, and the first report of a fault in the mod's code (`crash.cpp`).

**`docs/cheats.md`**
- The cheats: each switch's patch or hook.
- The jeep and the mounted guns.
- The bosses: the Ndesu, Irving (Transformed), the Reaper, Uroboros Aheri.
- The scope.
- The countdowns.
- The action buttons (QTEs), the cranks and the volcano's boulder.
- The map.
- The Mercenaries: the play state's step, clock, kills and score slot, a kill's scoring and report, the combo and the
  Combo Bonus, an enemy's hit record (Kill enemies as they appear), the stages' area tasks, and the Chainsaw Majini's
  second life.

An enemy with its own hit check (vtable slot 129) can refuse One hit kills. "The Ndesu" lists the 16 classes that
have one, and every class's name from the game's kill-record table. Check that first when an enemy is reported.

A QTE that Automatic QTEs misses: find its prompt (category, kind, label) among "The action buttons" 280 sites, and
what AUTO's tail does with it. AUTO turns some prompts into one plain press and leaves that press to the player:
the boulder's punches were that.

**One hit kills on a boss** (the user, 2026-11-01): the boss itself may need a hit per phase, but every piece that
drives its fight forward must die at one hit - a weak spot, a part, a counter it keeps ("each piece that needs hit
only being hit once each to kill them"). Its phases and its death stay the game's.

**`docs/items.md`**
- The profile and the chapter's numbers: cSystemInfo and its checkpoint copy, cGameInfo, the results' counters.
- The item box and the treasures.
- Items: cItemManager, cItem, the item table.
- What the Add lists offer: the story's items only; the Gatling gun is Chris's, the Longbow Sheva's. Store takes
  only those (a grenade launcher as its load).
- The save file: its cipher and layout (`tools/save.py` reads one).
- What an area has loaded: sLoadMan. Preloads go into list 4, never 7, the cutscenes'.
- The saved copies of the cases.
- The extra episodes' item boxes: the mod's own, in files beside it; their moves only in the copies an episode
  restarts from.
- The game's calls.

**`docs/menus.md`**
- Chapter Select from Item Management: the play flow, the front end and its archives, and the start-up menu's
  background.
- Chapter Select from the pause, from the Game Over screen and from the chapter's results.
- Chapter Select only in a story game: the game's modes (cGameSys +0x58) and how the extra episodes start.

**`docs/testing.md`**
- Each test's full check list, and the broken builds each caught.
- Testing without playing: the test bed, the user's launch, the reset storm, the GPU, the gamepad and the title rule.

**`docs/history.md`**
- The play log: each report of the user's, its cause, its fix, and the log lines to look for.
- The in-game checklist.
- Next to build.

## Status (2026-11-05)

Everything above is built, tested offline and installed.

The user's play has confirmed:
- the panel only at the pause;
- the item box's ticks staying;
- Chapter Select from Item Management, including its look ("this is working now"). It took two fixes: the chapter
  list's unloaded MenuResource layout, then the main menu showing behind the list.
- Chapter Select from a chapter's results (2026-11-01). The mod greyed it after 6-3 ("sometimes ... greyed out");
  it goes from there too now.

Built since and not seen in play:
- the jeep and the mounted guns;
- One hit kills on the Ndesu, on Irving, on the Reaper and on Uroboros Aheri's cores and doom attack;
- the scope's hold;
- the countdowns;
- the cranks;
- the volcano's boulder;
- the map (2026-11-04: at the extra episodes' starts too - it had tested for the story's mode alone);
- Chapter Select at the pause, on the Game Over screen and after 6-3's results;
- Chapter Select left off outside a story game (2026-11-03: no button in the extra episodes or Versus, where it was
  greyed);
- the extra episodes' own item boxes (2026-11-03: Lost in Nightmares and Desperate Escape showed the story's box);
- the saved copies of the cases;
- preloads into the sets' list;
- the Add lists cut to the story's items, and the Gatling gun and the Longbow kept for their owner (2026-11-01);
- Store refusing a scene's own weapon, and storing a grenade launcher as its load (2026-11-01);
- the Mercenaries' Endless combo, Combo Bonus always on and Kill enemies as they appear (2026-11-02). The user's first
  match (2026-11-03) stopped at 149 kills: each stage's 150 includes its chickens, which it skipped as animals, and
  the stages' areas were not understood (a signature named twice) - both fixed, not seen in play since. Later that
  day one stopped at 399 of 400: a Chainsaw Majini that got up for a second round after the mod's 25 tries were
  spent while it lay down. Now its second life is taken with its hit, and an enemy still standing after 25 tries
  is hit every 5 s instead of left (not seen in play).

What to look for in each is in `docs/history.md`, under "In-game checklist".

Built since and not seen in a log: the x87/SSE guard and the first-report logging (2026-11-05, after a Windows user's
first run ended without a line at the boot's first float).

Open:
- What ended that Windows run is not proven (the FPU fits only in part; the user's symptom is not known). Their next
  log's last lines, or Windows' own record of the crash, should say (`docs/history.md`, 2026-11-05).
- The cause of the 3-2 door crash (2026-10-30) is not proven. The mod's preloads into the cutscenes' list fit it, and
  that was fixed 2026-10-31.
- The FATAL logger's return addresses land in the C runtime's formatting, so they have been useless both times.
  Better ones were offered as a follow-up.

Next to build: the key items list and weapon upgrade editing. **Not wanted** (the user,
2026-10-29): a save file manager, a difficulty switch, anything for the exchange points.

## Tooling notes

- **Redoing the Ghidra project**: take a dump (`DumpImage = 1`), run `tools/fix_dump.py`, then `MAXMEM=12G
  /opt/ghidra/support/analyzeHeadless ghidra_proj RE5 -import ghidra_proj/re5dx9.fixed.exe -overwrite -loader
  PeLoader` (~30 min). `tools/ghidra/run.sh dec|xref|funcs ADDR...` takes about 5 s a run.
- **Text**: a prompt's label is a line of `mes_system_e` (`Image/Archive/Msg2Resource_e.arc`; `gen_items.py`'s
  `msg_lines` and `decode` read any file of it), and so are the options' texts. The catalogue of the 280 prompt
  requests (category, kind, type, label: `docs/cheats.md` "The action buttons") came from a throwaway script over
  `re5dx9.asm`, reading the pushes before each call of 0x764480.
- **Reading material** (not dependencies; clones in old scratchpads):
  - RE3Controls (bananzaza, a 1.2.0 mod with an address map of this exact build);
  - ThirteenAG's RE5 FusionFix;
  - krustytoe's FearLess table t=1954;
  - cangyuyao's MSG2 scripts;
  - the save editor shinneider/RE5-Save-Editor (the save's layout).
