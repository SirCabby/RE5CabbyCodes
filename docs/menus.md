# RE5CabbyCodes - Chapter Select

Part of the project's notes: `CLAUDE.md` is the guide and lists every section of `docs/`. The addresses are
build 11465250's. A section named in quotes ("The cranks") may be in another of these files:
`grep -n '^## ' docs/*.md`.

## Chapter Select from Item Management (`menus.cpp`) - built 2026-10-31

The user (2026-10-31): "when "starting" a game when at the screen to pick inventory items, add an imgui "Back" button
... Note that you can reach this screen after completing a level, and when selecting a level from continue or chapter
select ... make the button instead say "Chapter Select" instead of back, so all areas can take us back to that screen".
Then: "let's attempt to call the game's save feature to save the inventory before landing back at chapter select" - the
first build put back what the screen had changed; it is kept now.

- **Before a chapter** the play flow's readiness (aGame's vtable slot 5, FUN_00722990) runs its steps by +4 (0
  FUN_00718430, 2 FUN_007185E0 a network partner's sync, **3 Item Management, FUN_007189F0**, 4 FUN_0071D840 the load),
  then answers ready. Item Management's step (by +5): 0 goes on only offline (sBioMain+0x10434 bits 0-1 clear), in the
  story (cGameSys +0x58 0), not No Mercy (+0x79C), with sBioMain+0x10438 bit 0x40000 (the menus' starts set it); 4
  makes the screen (FUN_00A07980 with `mov ecx, <uIdItemBox00's record>`) into **aGame +0x2C**; 5 waits for its done
  (+0x3C), fades out, music 0xC; 6 lets it go and copies **its +0x3E (quit) into aGame +0x364** - unless its +0x5B6C or
  +0x5B6D (a partner joined or left) is set: then the flow steps back. After each step the readiness tests +0x364: set,
  FUN_00715BF0 (cSystemInfo's chapter-start restore from its checkpoint copy, FUN_00CF03D0, and sInfoMan's FUN_007BA540:
  the gold and the treasures from the copy, then cGameInfo's take-back, below) and step 0xC, ready - aGame's frame then
  runs FUN_00720BF0, the pause menu's Quit's end (fades, unloads, commits cSystemInfo, the area to aDemo).
- **The screen's own quit** (FUN_00A0A950 case 0xB, at the end of a co-op session there): FUN_00A079E0 (each pane's case
  fixed up, FUN_00A03DB0, then the working copy into the saved cases, FUN_007B7900), +0x3E = 1, its message +0x16C 0,
  +0x5B6C/+0x5B6D 0, its frame's step +0x14 = 3 (the fade out: sLoadMan idle and the timer +0x194 down, then step 4:
  +0x3C = 1). Its normal close (step 2, both panes' +0x3C at 4) makes the same save and sets +0x3D 1, +0x3E 0. Nothing
  in the game asks the quit offline. Before it the co-op quit lets the network session go (FUN_007965E0, cGameSys +0x2C
  0): not wanted offline.
- **The cases' working copy**: cGameInfo +0xF0 is a save struct of three 0x1590-byte blocks - A +0x10 (the saved
  state: records, the cases +0x140 + character x 0x420, the treasures +0x1230 ...), B +0x15A0, C +0x2B30. Item
  Management's cells come from and go to B's cases (cGameInfo +0x17D0; C's, +0x2D60, for a network partner's pane:
  FUN_00A34AF0). A chapter's end copies A into B (FUN_007B9310, by stage and scene); the screen's save puts each pane's
  case in order (FUN_00A03DB0: the slots numbered, something in hand) and copies B into A for each character in play
  (FUN_007B7900 -> FUN_00C92B70: the record, the case, the treasures); **CHAPTER SELECT's setup copies B into A**
  (FUN_00C92B10: "All weapons, money, etc. will be carried over").
- **cGameInfo keeps checkpoint copies too**: its commit (FUN_00C42580) copies its play data (mTempBase, mTempChap) to
  +0x4240.., a sub-object's (+4) to +0x5860 and **B to +0x42D0** (FUN_00C92CB0); its take-back (FUN_00C42610, through
  FUN_007BA540: the play flow's quit, the story flow's start, a load) copies them back - B from +0x42D0 (FUN_00C92CD0,
  its slot-10 entries cleared: no case keeps a vest, and a case's save never writes one), then A from B unless a new
  game - and commits. So the play flow's quit puts the cases back as well as the box, each to its last commit (the
  first notes here had it keep B, and the first build put B back itself for nothing). The results screen merges the
  chapter's treasures into the live profile (FUN_009E93B0 -> FUN_007BA850) and asks for the save file's write
  (FUN_007A5BC0); the save manager prepares its writes with FUN_007A5F50, which commits both first. The menus' own
  ITEM MANAGEMENT keeps its screen's changes by committing both right after it (FUN_00715C10), then writes the save
  file (FUN_007A5B00, on [0x11B1DD8]) when the screen changed something (its +0x188).
- **The front end**: aDemo's frame makes its scene by cGameSys +4 (5 cDemoStartUp, the start-up menu; 2 cDemoTitle ...).
  aDemo's start (FUN_00715450) goes by **cGameSys +0x18, the return reason**: every story start (FUN_00BF4380 for
  CONTINUE, CHAPTER SELECT and NEW GAME, FUN_00BF4650 for JOIN GAME) leaves 1 - the start-up menu, its story choice
  +0x1C set 0. cDemoStartUp (0x5C bytes) runs its step +4 (0 the start: uIdTopMenu00, the background, at +0x14; 2 the
  menus, FUN_00BF99E0; 3, 4 the game's start, 5), the next one at +8 (taken at its frame's end), sub-steps +5, +6, +7,
  +0x25. Step 2's sub-step 0 takes the reason (then 0): 1 -> the menus' music 0x98 (FUN_00715240 -> FUN_007A47F0), the
  top menu (uIdStartUp00, its choice into cGameSys +0xC); **7 -> +5 3, +6 2, +7 2 (10 online), +0x25 0, music 0x98**
  (the title's invite path, FUN_00BF4010, sets it with JOIN GAME); 3, 5, 9 -> +6 2, +7 0, no music; 2, 4, 6, 8, 10
  others (Versus, the Mercenaries, No Mercy). Sub-step 3 goes by +0xC: 0 the story flow FUN_00BF9760 (+6: 0 the story
  menu, uIdStartUp01, its choice into +0x1C; 2 by the choice, offline: 0 CONTINUE or NEW GAME, **1 CHAPTER SELECT
  (FUN_00BF6F40)**, 2 JOIN GAME (FUN_00BF7170, its session list uIdStartUp08), 3 ITEM MANAGEMENT (FUN_00BF22A0, the
  menus' own), 4 BONUS FEATURES, 5 SPECIAL SETTINGS, 6/10 THE MERCENARIES, 7-9/0xC VERSUS and EXTRA CONTENT - the
  menu's texts at 0x1131D90; past 0xC +6 = 0x5A, then back to the top menu). CHAPTER SELECT (+7): 0 its setup
  (Difficulty and StartChara defaults, 0x40000, B into A), falling into 1 the chapter list (**uIdStartUp05**: "CHAPTER
  SELECT", "Select a chapter."), 2 the difficulty, 3 the character, 4 ..., 8 the start (FUN_00BF4380).
- **The front end's archives** - the first build's crash (the user, 2026-10-31: "using the chapter select option crashed
  the game"; the log: the handoff at 12:09:53.120, CHAPTER SELECT's at .140, then "FATAL: ... Failed open file.
  ...\nativePC_MT\id\jpn\menu\top05.lsp 183" at .152). The menus' screens draw from layouts (`id\jpn\menu\topNN`;
  `jpn` is only the archives' path, the English game's too) in archives of their own: `TitleResource.arc` the top
  menu's and the story menu's (top00, top01, top26, top30), **`MenuResource.arc`** the rest (top02-top31, the chapter
  list's top05 among them: uIdStartUp05's slot 14, FUN_00A37E70 -> FUN_00A17010("id\jpn\menu\top05")). sLoadMan's
  **+0x207D4** says which front-end archives list 1 holds, by value: 1 TitleResource (Title2 too with sBioMain+0x10568,
  ShowTitle with +0x10434 bit 1), **2 MenuResource** (TrialMenuResource with +0x10434 bit 0; Menu2Resource too when
  FUN_007D3670), 4 ShopResource, 8 Versus and Dlc, 0x10 the Mercenaries' and the extras', 0x20 FigureResource, 0x40
  RecordResource, 0x80 ViewerResource, 0x100 CreditResource, 0x200 a chapter's end. Its chooser FUN_007D4D90:
  FUN_007D4870(1), FUN_007D4960(path, 0, 0) for each (the path copied into the entry's +4), the commit. aDemo's start
  zeroes the field (FUN_007D6410) and sets it by the reason - 1 alone for a game's end (2 more for reason 8, or with
  +0x10434 bit 0 and the save manager's +0x2B). aDemo's frame (FUN_00714E00, its step +4): 0 -> 1; **1 the chooser** ->
  2; 2 waits for the loader (FUN_007D3A50), makes the background -> 3; 3 makes the scene by cGameSys +4 -> 4 runs it.
  sArea runs an area's start and its first frame in one frame, so the chooser runs the frame after the start. The
  **story menu** (uIdStartUp01's frame, FUN_00A396C0) adds 2 and calls the chooser as a choice is made (CONTINUE or NEW
  GAME, CHAPTER SELECT, JOIN GAME, 4, 5; 4 for ITEM MANAGEMENT; 0x10 or 8 for the Mercenaries and Versus), and goes on
  only once the loader is idle. The way back skips that choice, so the chapter list asked for a layout no loaded archive
  held: the loose file, then the game's fatal error. (The play flow's Item Management leaves list 1 holding ShopResource
  alone: its step sets the field 4 and calls the chooser, 0x718A81. Other writers: cDemoStartUp's destructor
  FUN_00BF1A60 and the ends of BONUS FEATURES' and SPECIAL SETTINGS' flows, FUN_00BF3460/FUN_00BF35E0: 1 and the
  chooser; the play flow's results, 0x200.)
- **The start-up menu's background** - the second build's look (the user, 2026-10-31: "this is working now, however
  it's a little different from the vanilla game mechanics. The chapter select screen is opened in front of the main
  menu for the game. Usually there is no background menu behind it"). cDemoStartUp's step 0 (FUN_00BF6900): sub-step 10
  makes **uIdTopMenu00** (FUN_00A583D0, 0x5EC bytes) into its +0x14 and goes on by the reason (1: sTransition's
  +0x2C(1.0 s), a fade in, and sub-step 0x13, which waits for it; 0: sub-step 11; any other: its +0x34, FUN_007F2010,
  which drops the fade at once, and 0x14); then step 2. The top menu (uIdStartUp00, FUN_00A37250) is made at step 2's sub-step 1 into
  +0x18 and let go once a choice is made. uIdTopMenu00's start (slot 5, FUN_00A58580) loads `id\jpn\menu\soft\menu00`
  (an animated panel, +0x3C) and makes its parts: +0x54 **uIdStartUp11** (the main menu's six items, `title_01_<lang>`),
  +0x58 uIdStartUp12 (its cursor, `cursor_01`), +0x5C **uIdStartUp14** (a header, `title_02`), +0x60 uIdStartUp13 (the
  title, `title_00`) - uModels on lines 6, 10, 9, 3; uIdStartUp11's slot 33 (0xA40A00) loads the 11 languages' items
  from the table 0x113204C - and **by the return reason** picks the panel's segment (+0x6C: 7 a game's end or none,
  0x11 reason 7, 10 reasons 3-6/9/10, 0x1C 2, 5 8), shown at its end, and for reasons 2-10 calls FUN_00A58110(3).
  sUnit starts a unit in the same frame sArea makes it (sBioMain's frame: sArea's update at 0x783C5A, then sUnit's
  move at 0x783DE4 - its slot 6, FUN_00428630: the lines by priority, each through its slot 8, FUN_004287D0, which
  makes a unit in state 1 state 2 and calls its slot 5; its slot 10, FUN_004286E0, only calls each running unit's
  slot 8), so no tick lies between the background's making and its start. **FUN_00A58110(bits)** (the top menu at a choice: 1, then 2 as it closes):
  uIdStartUp11's FUN_00A3FAE0 - bit 1: picked +0xD51 = 1 (and +0xD64 0), bit 2: go on +0xD64 = 1 - and, while cGameSys
  +0xC is the story's choice, uIdStartUp14's FUN_00A3FD60(1): shown +0xD84. uIdStartUp11's frame (FUN_00A40D30): picked
  -> a 0.25 s blink (+0xD54), then with go on and its selection (+0xD58) the story's (0): motion 0xB, then holds 0x11
  (its pose while a submenu is up); back (+0xD52, FUN_00A3FB10): motion 0x40, idle again. uIdStartUp14's (FUN_00A3FDA0): shown -> fades
  in over a second (+0xD88), held, fades out when cleared. The menus change screens through uIdMenu's FUN_00A177E0(from,
  to) -> the screen's +0x128 (uIdTopMenu00) FUN_00A58D10: the table 0x1133030 (73 rows: from, to, the panel's segment)
  -> +0x68 = 1 (asked), +0x6C the segment, +0x70 0 (forward), then FUN_00A58880 (each screen's entry of 0x1133AF0
  copied in and cross-faded for a second, +0x80/+0x84); uIdTopMenu00's frame (FUN_00A58F10 -> FUN_00A57F70) plays an asked segment from 0 to its
  end (+0x74, an effect `id0002` with it). The top menu passes its choice's screen (0x1132044: [1, 14, 15, 16, 20, 98]),
  the story menu its own (1, 0x1C online) and its choice's (0x1131F28: [4, 5, 6, 98, 98, 10, ...]): top -> story
  segment 1, story -> CHAPTER SELECT's screen 5 **segment 8** (also 2 and 4: CONTINUE, NEW GAME), -> 6 (JOIN GAME) 9.
  The way back passes neither choice, so the start-up menu came up as a game's end leaves it: the main menu's list, its
  panel (segment 7) - and the chapter list over them.
- **The mod** (`kSigFlowQuit`, `kSigFlowItemBox`, `kSigFlowItemBoxDone`, `kSigBoxQuit`, `kSigBoxClose`, `kSigBoxCases`,
  `kSigCasesCommit`, `kSigCasesRestore`, `kSigMenuStep`, `kSigMenuReason`, `kSigMenuReasonBack`, `kSigMenuTop`,
  `kSigMenuStory`, `kSigFrontLoad`, `kSigMenusArchive`, `kSigMenuChosen`, `kSigMenuTransition`, `kSigStoryClose`;
  inline: the screen's save - its two panes, then the copy through sInfoMan's global, kSigBoxCases's -
  the block copies' bodies, their size and block agreed with each other, with the save struct and with the cases' working
  copies a block apart, the screen's frame switch, the menu functions' starts and their stubs - the reason step found as the one whose function takes the
  reasons, the normal reason as the one only cleared, the story as the top choice whose flow holds the choice switch,
  CHAPTER SELECT as the choice whose flow's step 1 makes the screen of uIdStartUp05's record; cGameSys through
  sBioMain's field, class-checked; aGame and aDemo among sArea's areas, cDemoStartUp in aDemo by class; aDemo's frame
  through sArea's frame slot, its step and the chooser's switch cases agreed, the chooser's first test of the same
  field, the menus' path pushed past the trial's by the online bit the mod reads, in the loader's archive folder;
  sArea's "started" byte; FUN_00A58110's setters' bodies, its test of the top menu's choice agreed with the story's,
  the story menu's close within uIdStartUp01's frame calling uIdMenu's change of screens, which jumps to FUN_00A58D10;
  uIdTopMenu00 in cDemoStartUp and its list and header by class): the button (`overlay.cpp`: a window of its own beside the item box's, no title bar, kept in
  front of it, one width - the user, 2026-10-31: "I want the button always visible, it's currently able to be collapsed
  inside inventory editor window"; one click) only for the play flow's Item Management (aGame +0x2C is it), offline,
  reason 1, top choice 0, the screen at its main step with its panes idle (`itembox::screen_idle`).
  Asked: the screen's own save (FUN_00A079E0, thiscall on the screen - the approved call), then the profile's
  checkpoint copy made its data (cSystemInfo +4 over +0x463C, 0x4638 bytes) and cGameInfo's copy of B made B (+0x1690
  over +0x42D0, 0x1590 bytes) - FUN_00715C10's commit less cGameInfo's play data, which stays at its last commit as
  the game's own quit leaves it - then the screen's quit writes. So FUN_00715BF0 takes back exactly what the screen
  left, and CHAPTER SELECT's setup carries it over. The save file is not written: the changes reach it with the game's
  next write (the next chapter's results among them). The tick aDemo is seen started (+0x20) with its step at most 1
  (its chooser's, this frame): sLoadMan +0x207D4 |= 2 - the menus' archive loaded with the title's, as the story menu's
  choice has it, and waited for by aDemo's own step 2. The first tick the start-up menu's background has started (its
  list and header made; cDemoStartUp still in its step 0, fading in - the fade and the background's look already taken
  from reason 1), with `Image\Archive\MenuResource` loaded in a list (`inventory::archive_loaded`; else given up: the
  top menu): reason 7, story choice 0xD (parked), and the background as the two choices leave it - uIdStartUp11 picked
  and go on (1, 1: it blinks and leaves under the fade), uIdStartUp14 shown (1), the panel's segment 8 asked for
  (+0x6C 8, +0x70 0, +0x68 1). Without the background understood, the same at step 2's sub-step 0 (the tick before its
  frame takes the reason), the background left as it is. Then the fade waited for (up to 10 s). The next tick (sub-step 3, +6 0x5A): +6 2,
  +7 0, the choice 1 - its frame runs CHAPTER SELECT's setup and makes the chapter list, the menus' music on. Anything
  unexpected: given up without a write (the game goes on to its own menus; what the screen changed is kept all the
  same). Built, the discovery replayed over the dump (all as expected: the save 0xA079E0, B +0x1690 of 0x1590 bytes,
  its copy +0x42D0; aDemo's step +4, its chooser at 1, the field +0x207D4, bit 2, "Image\Archive\MenuResource"; the
  background's list +0x54 (+0xD51, +0xD64), header +0x5C (+0xD84 = 1), panel +0x68/+0x6C/+0x70, segment 8), tested
  offline (`tests/test_menus.cpp`, the panel's, the loader's) and installed; the background's look not seen in play.

## Chapter Select from the pause (`menus.cpp`) - built 2026-10-31

The user (2026-10-31): "add the chapter select button to the pause screen as well, and put a confirmation on it";
then: "I want the answer to be yes on quit to save status of equipment and item box. I only want the confirmation to
exist while the player is in the game. I also want the chapter select button to exist on the game end screen".

- **The pause menu's panes** (uIdPause00, 0x1F8 bytes): its run (FUN_00A302D0, from its frame, slot 6) walks four
  panes from +0x48, 0x68 each (`kSigPausePanes`; one each local player): in use while its first byte is set, done at
  +4, its number +8, the player's +0x10, its pad's buttons +0x14; the option it runs +0x34 (0 its menu FUN_00A2E0F0,
  1 RESUME, 2-3 FUN_00A2F660, 4 OPTIONS ..., 0xC the network's end), the one before +0x38, its step +0x58. The menu's
  own step 2 makes a choice: +0x38 = +0x34, +0x34 = the choice, step 0. The menu's +0x3F set keeps a pane's cancel
  (its bit +0x1F6) from ending its option (`kSigPauseCancel`) and the pause button from closing the menu
  (FUN_00A2D3C0); +0x1E8 a network end (handled once +0x1F4 is set).
- **RESTART and QUIT** (FUN_00A2F660, its steps by two tables, `kSigPauseQuit`): 0-1 "Restart/Quit game?" (yes: +0x3F
  and +0x1F4 set, the fade out started, QUIT: the network session let go, RESTART: the play flow's copy restored;
  aGame +0x365 set - FUN_0071A940 starts no game over while it is - `kSigPauseLeaving`), 2 then 10 waits for the save
  manager ([0x1249C1C] +0x131C clear), 11 the save question (FUN_00A2E4C0 - see "The saved copies of the cases"), 0x14
  RESTART: FUN_00723420 (aGame's step 10); QUIT offline: sBioMain+0x10438 |= 0x10000, the play flow's quit (FUN_0071A4A0:
  aGame's step +4 made 0xC, as kSigFlowQuit's) - then the save manager's second player let go (FUN_007F1280: none
  offline) and the pane done. A story game's QUIT keeps the return reason (only the Mercenaries' and Versus' make it
  3/5), so the quit's end (FUN_00720BF0: the fade, the area let go, cSystemInfo committed) brings the start-up menu
  back with reason 1, as Item Management's quit does.
- **Who may save**: the question's first step (FUN_00A2D540) asks it only of a player (the pane's +0x10) whose slot
  in the save manager ([0x11B1DD8] +0x78 + player x 0x20, players 0-3; FUN_007F1520) has its +8 clear; otherwise it
  goes on at once - no save and no take-back.
- **The mod**: at the pause in a story chapter (not the Mercenaries'), offline, from the story's menu, one pane in use
  and at its menu, asked once confirmed: the menu's +0x3F and +0x1F4, aGame +0x365, the pane's option QUIT, the
  question's step its answer's (2) with its timer 0 and its answer yes (+0x60 = 0) - or, for a player the save manager
  does not let save, its first step (it goes on without asking) - then the pane's step 10. The game's own steps from
  there: the save manager waited for, QUIT's yes (the play flow's copy restored, the live cases saved and into the
  working block, a new copy and the commits - the box, the gold, the treasures -, the save file written), the play
  flow's quit. The first build answered no (the take-back: the cases back to the last commit's working block, money
  and treasures to the last checkpoint); the user asked for yes. The mod waits for the game to end (or the menus'
  area), then hands the start-up menu over as from Item Management - on its way while the pane still runs QUIT (the
  save takes its time), QUIT's bit of sBioMain's flags is set or the play flow quits; given up, said so, after 10 s
  of none of them. Nothing is moved or loaded meanwhile (`leaving`). The panel's own window beside the panel
  (`kChapterPauseWindow`), Yes or No first - the only place it asks (`CsPlace`, let go when the screen goes).
  Built and tested offline (`tests/test_menus.cpp`, `tests/test_panel.cpp`), the discovery replayed over the dump
  (panes +0x48 x4 0x68, done +4, option +0x34, step +0x58, player +0x10; QUIT 3 from its step 10, the question 11 -
  its step +0x59 made 2, timer +0x5C, answer +0x60 made 0, the save manager's slots +0x78 + 0x20 up to 3, +8 -, the
  quit 20: sBioMain +0x10438 |= 0x10000, aGame's step +4 made 0xC; +0x3F, +0x1F4, aGame +0x365; the play flow's copy
  aGame +0x130); not seen in play.

## Chapter Select from the Game Over screen (`menus.cpp`) - built 2026-10-31

"the game end screen" taken as the Game Over screen (a death: CONTINUE or QUIT); the chapter's results lead to Item
Management, which has the button already.

- **The screen** (`uIdGameover00`, 0x1F0 bytes; made by the play flow's game over step 9, FUN_00725030, into aGame
  +0x11C; its frame FUN_00A00B40, offline its second switch by +0x14): 0-1 its opening (`id\jpn\die\cp_die_360`),
  then shown (+0x194) and - one player - 2 its menu (the cursor +0x190: 0 CONTINUE, 1 QUIT; decided: +0x196, a delay
  into +0x198, FUN_00A00550: CONTINUE sBioMain+0x10438 |= 0x80000, QUIT |= 0x10000 and cGameSys +0x2C 0 - the network
  session's - and a network message, FUN_00791130, only in a session), or with a partner 10 (the choice -1, taken from
  the flags as they come: 0x80000 CONTINUE, 0x10000 QUIT; `kSigGameOverChoice`, `kSigGameOverDecided`); 3/11 the
  delay, 0x5A-0x5C the save manager waited for, +0x195 done.
- **The play flow's game over** (FUN_00725030 by +5): the screen done, the fade, the area let go, then step 11: the
  play flow's copy restored (FUN_00723870(2): back to the last checkpoint, the chapter's counters kept); QUIT's bit:
  aGame's step 0xB (FUN_00720A50): a pause menu unit made with +0x3F, +0x1F4 and +0x40 set (FUN_00A2CF50), whose open
  (FUN_00A2F4B0, +0x40) gives this player's pane QUIT at step 11 (the save question) - +0x40 skips the question and
  takes the profile back (FUN_007BA540), then writes the save file - then QUIT's step 0x14: the play flow's quit,
  the start-up menu with reason 1. Otherwise step 12: the area again (CONTINUE).
- **The mod**: offered while the screen is up in a story chapter (not the Mercenaries'), ready once shown (+0x194)
  and until decided (+0x196), not at the partner's step 10; one click (no question): QUIT's bit set as the game's own
  setter (FUN_00726120) sets it - under sBioMain's lock (+4) while its +0x1C or the global [0x1249C2E] says the game
  uses it -, the choice -1, the screen's step 10: its own step takes QUIT from the flags. Then as from the pause.
  The window alone on that screen (`kChapterOverWindow`, no panel; `dispatch`'s `show_gameover`), the game keeping
  its mouse (only a click over the window kept from it, as beside Item Management). Built and tested offline, the
  discovery replayed over the dump (shown +0x194, step +0x14, 10 the partner's, choice +0x190, decided +0x196; the
  flags +0x10438, QUIT 0x10000, CONTINUE 0x80000); not seen in play.

## Chapter Select from the chapter's results (`menus.cpp`) - built 2026-10-31

The user (2026-10-31), after the Game Over screen's: "I also wanted it on the chapter results screen".

- **The results** (`uIdChapEd00`, 0x330 bytes; made by the play flow's chapter end, FUN_00720190, into aGame +0x118;
  their frame FUN_009EA160 by +0x14): 0-4 counting up, 5 the chapter's treasures merged into the profile and the save
  file asked for (FUN_009E93B0), 6-7, 10 their menu (prompts 0xCC; +0x2EC the prompts' state). With their quit
  offered (+0x189 clear; FUN_00720190's step 0 sets it for the last chapter, mChapterNo 15 (6-3), for the
  Mercenaries' modes 3/4, and past chapter 1-1 with sBioMain's mSystemFlag (+0x10434) bit 2, which no code sets)
  a quit pressed asks first (`kSigResultsAsk`: the prompts made 3, +0x16C 0x13, +0x170 -1, step 0x14, message 0xCD);
  yes (`kSigResultsQuit`): step 0x15 with the quit +0x188 set, +0x170 0, the timer +0x174 the delay - then the session
  let go (FUN_0078FD10, FUN_00794EC0(0), cGameSys +0x2C 0), 0x16 the save manager waited for, 0x5A-0x5B done (+0x3C);
  no: back to 10. Decide at 10 goes on (step 12, done) to the next chapter.
- **Without their quit** (+0x189 set: one prompt, the quit button not taken) decide at 10 is the quit: step 12 sets
  +0x188 1, +0x170 0, +0x174 0 and goes to 0x5A (offline; 0x15 online). So after 6-3 the game goes back to its menus
  from the results. The ending is not after them: aGame's step 0xB is only the quit, aDemo's scenes (FUN_00714E00's
  step 3) are the logo, the movies, the title, the start-up menu and cDemoWaitSignOut, and uIdCredit00 is made only
  by a front-end menu's choice (FUN_00BF3B60, the credits viewer) - the ending plays in the chapter's last areas,
  before the results (the user's log, 2026-11-01: the characters gone before 6-3's results came up).
- **The play flow's chapter end** (FUN_00720190 by +6, `kSigResultsDone`): the results done, the fade, then their
  quit: aGame's step 0xB - the quit after a game's end, as the Game Over's QUIT (FUN_00720A50: the pause menu unit's
  QUIT from its save question, +0x40: the profile taken back from what the chapter's end just committed and saved,
  the save file written, then the quit's end) - else the next chapter (FUN_007F1630).
- **The mod**: offered while the results are up in a story chapter (not the Mercenaries'), ready at their menu (step
  10) - the last chapter's too; one click (no question): the prompts made 3, the quit set, +0x170 0, the timer 0, the
  step 0x15 - the yes of their own question without its question; their own steps from there, then the play flow's
  quit and the way to the chapter list as from the pause. On its way while their quit is set or the play flow is at
  its step 0xB. The window alone (`kChapterEndWindow`, shared with the Game Over screen's; `dispatch`'s `show_end`).
  Built and tested offline, the discovery replayed over the dump (no quit +0x189, step +0x14: menu 10, question 20,
  after its yes 21; quit +0x188, wait +0x170, timer +0x174, prompts +0x2EC made 3; aGame's results +0x118, its step
  after a game's end 11); seen in play 2026-11-01 (14:19, a chapter's results to the chapter list and a chapter
  started from it).
- **The last chapter's** (the user, 2026-11-01: "sometimes on the chapter end screen, the chapter select button
  remains greyed out"): the first build greyed the button there, "Not after this chapter: it goes on to the ending".
  It is ready there now, with the same writes: the quit set is what their own continue sets, and the steps from
  0x15 (the session let go, the save manager waited for) are what their quit's yes runs at any other chapter. The
  view's `last` gives its help: the chapter list instead of the main menu.
- **The log**: whether it can go at the Game Over screen and at the results is logged as it changes, and again for
  each new screen (`log_end`): "menus: Chapter Select at the chapter's results greyed: Wait for the results' menu.
  (their step 3, their quit offered; the return reason 1, the top menu's choice 0)", then "... ready (their step 10,
  ...)". At the Game Over screen the line has its shown, decided and step.

## Chapter Select only in a story game (`menus.cpp`) - 2026-11-03

The user (2026-11-03): "the chapter select button shows on the pause menu while in other game modes beyond the story.
Instead of saying the game wasn't launched from chapter select, it should just not show up".

- **Why it showed**: the pause, the Game Over screen and the results offered the button wherever the play flow (aGame)
  ran them and the Mercenaries' HUD (uIdMerce00) was not up, and greyed it in a game not started from the story's menu
  ("This game was not started from the story's menu."). The extra episodes are started from the story's menu itself:
  the story flow's choices (cGameSys +0x1C) 8 and 9 run FUN_00BF9560, which sets cGameSys's mode **+0x58 = (choice !=
  8) + 3** (3 Lost in Nightmares, 4 Desperate Escape) and starts them through FUN_00BF55E0 (its only caller), which
  leaves the return reason **9**; the top menu's choice stays the story's (0). Choice 7 (Versus, FUN_00BF9230) sets the
  mode 1; the Mercenaries' are 2 and 5 ("The modes" in cheats.md). So the extra episodes had the button, greyed, and
  Versus (online) had it greyed with "Not in an online game.".
- **The game's own test**: the play flow's Item Management step (FUN_007189F0, its case 0) goes on only offline
  (sBioMain +0x10434 bits 0-1), **in the story (cGameSys +0x58 == 0)** and not in No Mercy (+0x79C); each failing test
  clears the flag 0x40000 the screen is shown by (`push 0x40000; call 0x715A30`).
- **The mod**: Chapter Select is offered only in a story game: cGameSys's mode 0, the return reason 1 and the top
  menu's choice 0 (what CONTINUE, CHAPTER SELECT, NEW GAME and JOIN GAME leave). That holds at Item Management, the
  pause, the Game Over screen and the results alike. In any other game there is no window at all (`View::offered`
  false, `where` none), and the greyed "not started from the story's menu" is gone. An online story game is still
  offered, greyed ("Not in an online game."). The Mercenaries' HUD test stays too.
- **Discovery** (inline, no new signature): the step kSigFlowQuit calls, its switch (`56 8B F1 8A 46 ?? ...`, the
  table +25), its case 0, then within 0x60 bytes `8B 81 <cGameSys field> 33 DB 39 58 <mode> 74 ?? 68 <flag> E8` - the
  flag and the call the same as the online test's before it. The field is checked against the menus' own (kSigMenuReason's
  sBioMain +0x1042C) in discover_menus, the mode against cGameSys's size and its other fields.
- **The log**: left off, once for each screen: "menus: no Chapter Select on this screen - not a story game (the game's
  mode 3, the return reason 9, the top menu's choice 0)". The start's discovery line has "the game's mode +0x58 (0 the
  story: Chapter Select in no other)".
- **Tests**: `tests/test_menus.cpp` (the extra episodes at the pause, the Game Over screen and the results, Versus,
  the mode or the reason alone, the log; a build offering it in every mode fails 10 checks, one not testing the mode
  3) and `tests/test_discover.cpp` (discover_flow and discover_menus over the dump: the mode +0x58 through sBioMain
  +0x1042C, the return reason +0x18 - 1 normal, 7 back -, the top menu's choice +0xC - 0 the story -, the story menu's
  +0x1C - 1 CHAPTER SELECT, 13 parked). Installed 2026-11-03; not seen in play.
