# RE5CabbyCodes - the engine and the mod's foundations

Part of the project's notes: `CLAUDE.md` is the guide and lists every section of `docs/`. The addresses are
build 11465250's. A section named in quotes ("The cranks") may be in another of these files:
`grep -n '^## ' docs/*.md`.

## Why d3d9.dll, and Enigma's loader

`re5dx9.exe` is wrapped by Enigma Protector: every section is compressed on disk (the code's 0x41CA00 bytes unpack
to 0xB49000), the on-disk import table is the wrapper's (`kernel32!GetModuleHandleA/GetProcAddress/ExitProcess/
LoadLibraryA`, `user32!MessageBoxA`, and one function of each DLL the game needs), and **the game's
`steam_api.dll` is carried inside the exe** - Enigma serves it as a virtual file at
`<game dir>\steam_api.dll` (WINEDEBUG=+loaddll shows it "loaded" from there, mapped at 0x3B400000; there is no such
file). So there is no Steam DLL to stand in for.

Of the DLLs the wrapper imports, **d3d9.dll** is the one that loads from the game's folder everywhere with no
launch option: Windows looks in the program's folder first (it is not a known DLL), and Proton marks DXVK's
d3d9.dll native (`d3d9=n` in its own WINEDLLOVERRIDES, for every game unless PROTON_USE_WINED3D), so a d3d9.dll
beside the exe is found first there too. dinput8, xinput1_3, winmm, dsound, version and the rest are Wine builtins.
The proxy (`src/proxy.cpp`, `d3d9.def`, `src/proxy_exports.inc` by hand) forwards all 16 other exports by jump
thunks to the real d3d9.dll - `d3d9_orig.dll` beside it when there is one (another graphics mod the player had),
else `<system dir>\d3d9.dll` by its full path - and `Direct3DCreate9` is the mod's own (`overlay_dx9.cpp`).

**Enigma does not resolve the game's imports against the loaded d3d9.dll**: it loads the system's itself, so the
game's `Direct3DCreate9` slot pointed at DXVK's (0x7986EA60 in the first test) and the mod's export was never
called. What it does instead (seen with `Trace = 1`): its runtime resolves ~640 names of its own through the
exe's GetProcAddress import - and one of them is **ntdll's `LdrGetProcedureAddress`**, through which it then
resolves the game's 270 imports. So DllMain re-points that GetProcAddress slot (exe+0x16E6F88, found in the
wrapper's import table by value, `mem::iat_slot`) at `boot.cpp`'s hook; when the wrapper asks for
`LdrGetProcedureAddress` it gets the mod's wrapper of it, which hands the game the mod's **Direct3DCreate9**,
**DirectInput8Create** (the input guard), **SetUnhandledExceptionFilter** (the crash chain), **FatalAppExitA** and
**ExitProcess** (the game's own ends, logged: see "The game's own ends") in place of the real ones.
(DirectInput8Create comes through GetProcAddress itself.)

## Threads and the tick

- **DllMain** (the loader thread = the game's main thread): the real d3d9.dll, the crash logger, the ini, the
  loader hook, the locks, the mod thread (it only reports until the first tick).
- **boot::unpacked()** (the game's first `Direct3DCreate9`, on the main thread, during sBioMain's start-up): the
  image's sections (`image.cpp`: the game's five first, blank names, `.rsrc` fifth; Enigma's and `.bind` after),
  the check that the code is in place (its `ret; int3` and int3 padding), the import table Enigma rebuilt
  (0xF4A000.., real addresses, no names - `iat()` walks it while the values are module addresses), MT's class
  records, `game::discover`, `cheats::install`, `inventory::discover`, the tick's hook, and the dump when asked.
- **The tick** (`dispatch.cpp`): WinMain makes the application (`sApp`, on its stack) with sBioMain's class
  record; `sApp::init` makes the main system through that record (`[sApp+0x257C]`), and the main loop
  (0x423990) calls its **vtable slot 6** once a frame - sBioMain's frame (0x783AD0), which runs every system's
  update in turn. The mod re-points that slot of **sBioMain's own vtable** (found by name; sMain's is another) at
  a hook that ticks, then runs the frame. Main thread only (checked).
- **The panel** draws in the device's Present (the main thread too in every run so far: RenderingThread = ON in
  `config.ini`, yet Present came on thread 336), from the snapshots the tick publishes. The window procedure
  (class `MTFramework`, a Unicode window) is subclassed for ImGui's input.
- **Input** (`input.cpp`, RE4OG's): DirectInput8Create → CreateDevice → the device class's Acquire/GetDeviceState/
  GetDeviceData; the game opens a mouse and a keyboard (`sMouse`, `sKeyboard`). While the panel is up the mouse is
  let go and refused; the keyboard reads "nothing" while a panel field has focus (but Esc and Alt). The pad
  (XInput) is the game's own and is left alone.

## Windows (audited by design; never run there)

- **DLL search order**: d3d9.dll is not a known DLL, so the game folder's comes first. A d3d9.dll of another mod
  must be renamed d3d9_orig.dll (README); `make install` does it.
- **The Steam overlay's D3D9 hooks**: as RE4OG - on Windows the game's IDirect3D9, device and swap chains get
  vtables of their own (`adopt.cpp`); the game's window is known by its class, `MTFramework`.
- **The overlay's DirectInput wrappers**: the guard hooks each device class it meets (`input.cpp`).
- **Exit**: DllMain does nothing on a process exit; the DLL pins itself.
- **ASLR**: re5dx9.exe has relocations stripped (fixed 0x400000), but nothing in the mod depends on it.
- **Open**: never run on Windows. Whether Enigma on Windows also resolves the game's imports through
  LdrGetProcedureAddress is the one thing the whole mod rests on (it is the same wrapper code, run by the same
  exe); the log says "the game's Direct3DCreate9 resolved through LdrGetProcedureAddress" when it does.

## MT Framework's class records (`mt.cpp`)

Every MtObject class has a static record (MtDTI) in the data sections: +0 its own vtable, +4 name, +8 next sibling,
+0xC first child, +0x10 parent, +0x1C size (23 bits), +0x20 an id. The tree's root has no name; MtObject is its
child (4355 records). A class answers its record from **vtable slot 4** (getDTI: `mov eax, record; ret`), so its
vtable is found from its name: one pass over the code for every such function, one over .rdata for the vtables
holding each in slot 4 (4333 found; 164 more vtables answer another class's record - classes without a record of
their own; `mt::vtables_of` gives all of them). Slot 3 is createProperty (the fields a class registers by name -
`tools/mt_props.py` reads them out of the code: cItem's, cGameInfo's temporary ones, sInfoMan's profiles).

## The systems and the units

- sBioMain's instance holds every system (cSystem subclass) between +0x10340 and +0x103F8: the tick's first frame
  names 45 of them by their records (sUnit, sPlayer, sEnemy, sItem, sInfoMan, sActionButton, ...).
- **sUnit** keeps the units in 32 move lines (sUnit + i * 0x20: +0x28 flags (bit 1: the line is off), +0x2C first
  unit); a unit's +4 flags (bits 0-2: 1 starting, 2 running, 4 dying), +8 next - all read out of sUnit's slot 9
  (its line walk) and slot 6 (`units.cpp`).
- **The screens** are units of `uId*` classes: `uIdPause00` the pause menu, `uIdCockpit00` the HUD,
  `uIdSubScreen00` the real-time inventory, `uIdItemBox00` the item management between chapters, `uIdShop00` the
  shop, `uIdChapEd00` the results, `uIdMerce00` the Mercenaries' HUD (its clock), `uIdTitle00/01`, `uIdTopMenu00`,
  `uIdGameover00`, `uIdOption04` the options page with Action Assist. `Trace = 1` logs each as it comes and goes.
  Item Management's unit is made as it opens and killed as it closes: its being there is `Screen::kItemBox`.
- **The pause menu's unit is there all through a chapter**: the area's start (0x720A50, step 0) makes it
  (FUN_00A2DDB0, on line 26, flag 0x2000) and it waits. Its frame (slot 6, 0xA308C0) has two steps (+0x14): 0 asks
  whether to open (0xA2D0C0: the pause pressed, nothing in the way) and opens (0xA2F4B0: +0x3C = 1, its +4 |= 0x800,
  sBioMain+0x10438 |= 0x800 - the game stops - when not online); 1 keeps sBioMain+0x10438 |= 0x1000 and runs the
  menu until it closes (0xA2DDE0: +0x3C = 0, both flags cleared). Its draw (slot 9) does nothing while +0x3C is 0.
  v1 took "the unit exists" for "paused": the user's first play (2026-10-29) had the panel up - and the mouse let
  go - all through the chapters and the cutscenes. `discover_pause` reads +0x3C out of the draw and the close.

## The characters (`game.cpp`)

- **sPlayer**: the characters at +0x24, 4 of them (sPlayer's own "is this a character" loop, `kSigIsPlayer`).
  A character is a `uPlayer` (`uPl00Chris*`, `uPl01Sheba*`, `uPl02Jill*` ...; the costumes are subclasses).
- **Health**: +0x1364 (int16), its most +0x1366 - uCharacter's set-health (vtable slot 67, `kSigSetLife`); slot 68
  adds, slot 69 subtracts, each clamped. Enemies use the same fields.
- **Who drives a player**: +0x2DA8 - 0 this machine's pad, 1 the AI, 2 the other player over the network
  (`kSigPlayerDamage`: the damage turns 2 away). The AI's brain (`uPlIntel`, a unit on line 5) is at +0x2D64.
- **A player's damage** (vtable slot 66, 0xB64EB0 in all 32 player vtables): `(damage, info) -> died`; the network
  partner gets nothing (`return false`); otherwise the damage is scaled by the mode (70%/50%/80% in some modes),
  then uCharacter's damage (0xB56FB0): keep `info` at +0x1368, subtract-health (slot 69), `return health < 1`.
- **An enemy's damage slot** (slot 66, 0x9DEE50 in 94 of the 108 enemy vtables): keep `info` (a float) at +0x1368,
  subtract-health, and if the health is gone but the game may not let it die yet (FUN_007F2930, or +0x1E24 /
  +0x1E38) set-health(1); `return health < 1`. The story's companions are `uEmNpc` subclasses (Josh, Excella,
  Irving, Spencer, Dave, the BSAA men ...: 22 vtables).
- **A hit on an enemy does not use that slot.** The game's hit delivery (FUN_0078D590) hands a hit on a uEnemy to
  its damage handler (FUN_009E0840), which calls FUN_009DEA50(hit, slot 131's answer). That works the damage out
  of the hit (FUN_0071B010; the hit's +0x3C bit 0x10000000 makes it the whole health), clamps it to int16 and
  calls **subtract-health (slot 69) itself**. Then: no health and (+0x1E3D clear, the hit's player AI-driven) ->
  set-health(1); may die (FUN_007F2930, +0x1E24/+0x1E38 clear) -> +0x1910 = 1, set-health(0); else set-health(1).
  All 93 uEnemy classes share uEnemy's slots 66-69 (0x9DEE50, set 0x84F740, add 0x84F780, subtract 0x84F7C0 -
  `hp -= n`, clamped to 0..most). Other subtracts: cEm40/cEm41ActFarCatchOk (1), cEm10ActDmgFlyoff (350),
  FUN_00AC03E0. v1 hooked slot 66 only, so no shot ever reached it: the user's first test (2026-10-29) "the one hit
  kills cheat is not working".

## The mouse

- **sMouse** (0xA8 bytes; vtable 0x10B8D40, frame slot 6 0x6658C0) opens a DirectInput8 mouse, NONEXCLUSIVE |
  BACKGROUND, DIMOUSESTATE2. Each frame (0x6656E0) it takes GetDeviceState's lX/lY/lZ times **MouseBaseSpeed**
  (+0x9C: `[GAME] MouseBaseSpeed` of the game's `config.ini`, default 2.0) and truncates each to an int (+0x2C/+0x30/
  +0x34). Below 1.0 the truncation loses slow movement. The pointer: +0x20/+0x24, shown +0x28 (ShowCursor when it
  changes, 0x6659D0), clipped to the window +0x29 (ClipCursor). It is warped (SetCursorPos, 0x665620) only when the
  cursor comes back (0x79AD64).
- The pad's mouse fields (0xC6E150; PlayerPad +0x20C/+0x210) are those times the in-game **Mouse Sensitivity**
  (Options, keyboard/mouse: cSystemInfo +0x60, an index 0-9 -> x0.1..x1.0, table 0x10E88A0, FUN_007B71C0).
- RE3Controls' source (kbinput.cpp, MouseUnitRadians) puts the mouse aim at 0.02 x sParameter+0x42C (5.0) x 1.2 x
  render width / 1280 degrees per unit: 0.18 at 1920 wide, 0.36 per count at the defaults. Not traced here.
- The pointer in play: sPad's FUN_0079AB10 decides it each frame. It is hidden unless paused or the window is not
  in front (0x77F4B0: not minimized, GetForegroundWindow and GetFocus are the game's). It is clipped while
  cGameSys (sBioMain+0x1042C) +0x28 (set at a game's start) and the window is in front, or in fullscreen. The window
  class's cursor is IDC_ARROW. sCursor (0x7ADC30) calls SetCursor itself.
- **Under the user's gamescope the mouse is broken** (2026-10-29: "extremely sensitive even on lowest settings ...
  the aim at the bottom right of the screen and is unmovable ... The mouse also isn't locked to the game"). Each step
  below was read in its source:
  - gamescope 3.16.31 nested locks the pointer and sends relative motion only when the focused cursor image is
    empty *and* the pointer is constrained (steamcompmgr.cpp). Otherwise `wlserver_mousewarp` sends absolute
    motion only.
  - Xwayland posts motion that has no relative part through its absolute pointer device (0..0xFFFF), raw events
    included (`dispatch_absolute_motion`: POINTER_NORAW only with relative motion).
  - Proton Experimental's winex11, unlike upstream, turns absolute raw valuators into absolute raw input
    ("absolute input %d,%d").
  - Wine's dinput then subtracts GetCursorPos from that absolute value ("Unimplemented MOUSE_VIRTUAL_DESKTOP
    flag"): a huge positive lX/lY on every event.
  - RE5's mouse is non-exclusive, so Wine never warps it. Its ClipCursor grabs Wine's own clip window.
    Nothing gives gamescope a constraint.

  The fix offered is `--force-grab-cursor` in GAMESCOPE_ARGS (always relative). The user's launch has carried it
  since 2026-10-29 (their game's gamescope command line that evening), with no mouse complaint since.
- Not the mod: the input guard passes the mouse through while the panel is closed, and wineserver hides the cursor
  while the show count is below 0, whatever shape is set. ImGui's handler still takes WM_SETCURSOR (sets the shape,
  returns 1) with the panel hidden. The game only notes its hit-test (sApp+0x16C, read nowhere). No RE5 mod changes
  the mouse's speed. Fusion Fix's modern controls and RE3Controls turn their cameras from the same scaled fields.

## The game's own ends (`crash.cpp`) - logged 2026-10-30

The user (2026-10-30): "the game just crashed trying to go through a door in chapter 3-2".

- **What the log had**: "process exiting" at 17:47:55, 10 s after a pause closed, and no CRASH line. Nothing of the
  mod's was logged after 17:41 (no item moved, nothing asked of the loader, the scope's hold never engaged), and two
  area changes earlier in that run (17:33:38, 17:39:26) had gone through. Steam saw the processes go at 17:48:12. As
  with the item box's crash (above), the game ended itself.
- **The game's fatal error** (FUN_006CFEF0, `(the handler's object 0x11AF634, format, ...)`) formats its text into
  1 KB on its stack, calls the handler's +0xC with it - FatalAppExitA (0x422D00) - then exit(1). Wine's FatalAppExitA
  shows a message box (MB_SYSTEMMODAL, no title), then ends the process (RtlExitUserProcess: the DLLs' detach, no C
  runtime flush - so `re5LoadFails_jim.txt`, opened "a" once and kept open at 0x1249C8C, stays empty). Its 42
  callers' texts:
  - "Failed open file. %s %d" (0x6F96EA, after "Fail: '%s' %d" into that file);
  - a Direct3D call's error, "%s : %s" with the error's name (0x45B465..0x45B6B1: E_FAIL, D3DERR_*), or "ERR08 :
    Memory overrun." (0x45B6C0);
  - "inflate: %s : %d" (zlib, 0x44631A, 0x6216CE);
  - "ERROR!!!! sActionButton::setResource : no resource" (0x7642A9; the play flow loads that resource at 0x718D74);
  - at start-up only: ERR01 (DirectInput), ERR02 (SSE2), ERR05-07 and 0A (Direct3D, shaders, PIX), Steam's,
    sNetwork::init. ("%s: Memory allocation failed (%d)" is the network session's, logged only.)
- **Known in 3-2**: a Steam thread (2023) has "failed open file wp0100.mod 183" on arriving at the Oil Fields: Chris's
  knife (0x101, `uWp01`) changed by a trainer - a cutscene there has Josh and Irving use it and expects its model
  loaded. Here `uWp01` was loaded in that area (the 17:41:59 pause's list), so not that.
- **The mod's preloads, then a cutscene** (the user's 5-3, 2026-10-31: "when fighting with Jill and Wesker, after
  damaging wesker the game crashed"): "FATAL: the game ends itself with its own error: "Failed open file.
  ...\nativePC_MT\pawn\pl\pl00\model\evf0000.mod 183"" at 21:48:41 - Chris's face for cutscenes, which only
  `Image\Archive\event\evfpl00` holds. Stage 309 (the Wesker and Jill fight) had played its opening cutscene
  (ev309_00, preloaded with the faces) at 21:46:52, which let the cutscenes' list go; at the pause of 21:48:15 the mod
  asked for 13 item archives into the list chosen last - the cutscenes', empty. Damaging Wesker with One hit kills
  took all his health at once: in stage 309 his death reaction is none (FUN_009BE870: -1 there, 0x67 elsewhere) and
  the fight moves on to its next cutscene, ev309_10 (aS309's frame FUN_0074DEE0: at scene 9540 -> FUN_00719180, event
  0x25, scene 9550). The event step found loaded entries in the cutscenes' list (the mod's 13), took ev309_10 for
  preloaded and played it unloaded - see "What an area has loaded". Fixed there the same night. The 3-2 door
  (2026-10-30, above: a pause's preloads at 17:41, then the game's own end going through a door at 17:47:55, before
  the FATAL logger) fits the same pattern, a cutscene due at the door - not proven. The game was running the 17:50
  build: the Reaper's hook was not in it.
- **Chapter Select's first build** (2026-10-31, 12:09:53) ended the game this way, the logger's first catch: "FATAL:
  the game ends itself with its own error: "Failed open file. S:\steamapps\common\Resident Evil
  5\nativePC_MT\id\jpn\menu\top05.lsp 183"" - the chapter list's layout, its archive not loaded (see "Chapter
  Select from Item Management"). The ten addresses logged were all in the text's formatting (the C runtime, exe+0xA7xxxx).
- **Memory**: the exe is not flagged large-address-aware, but Proton sets WINE_LARGE_ADDRESS_AWARE=1 for every game
  not on its own list (`forcelgadd`; app 21690 is not on it): 4 GB. The mod's preloads in that area were 10 archives,
  12 MB unpacked.
- **The mod** takes the game's FatalAppExitA and ExitProcess imports as it does SetUnhandledExceptionFilter (through
  LdrGetProcedureAddress; the import table's slot as a fallback, whose boot line then reads "imports: the game's
  FatalAppExitA slot already holds our hook"). It logs "FATAL: the game ends itself with its own error: "..."" with
  up to ten return addresses into the game's code (2048 slots up: past the text), or "the game ends:
  ExitProcess(n)" (a quit's 0 alone, any other code with where it came from), then calls the real one. Built, tested
  offline (`tests/test_crash.cpp`) and installed.
