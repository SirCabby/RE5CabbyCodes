# RE5CabbyCodes - the profile, the inventory and the item box

Part of the project's notes: `CLAUDE.md` is the guide and lists every section of `docs/`. The addresses are
build 11465250's. A section named in quotes ("The cranks") may be in another of these files:
`grep -n '^## ' docs/*.md`.

## The profile and the chapter's numbers

- **sInfoMan** (0x39760 bytes): the profile in use at +0x20 (0 or 1), **cSystemInfo[2]** at +0x38 (0x8C74 each:
  gold +0x188, exchange points +0x18C - the save's 0x194/0x198), **cGameInfo[2]** at +0x23210 (0x5940 each).
  Read out of the shop's gold read (`kSigShopGold`) and a death's count (`kSigDeathCounted`).
- **cSystemInfo keeps a checkpoint copy** (its constructor 0xCF0A50: a vtable, the live data from +4, 0x4638
  bytes, then a copy at +0x463C, the same size). The commit (0xCEDFD0, copy <- live) runs from the play flow
  (0x715C10 - with cGameInfo's, FUN_00C42580 - from a chapter's start, FUN_00723660, for each character in play;
  0x71C4B0, 0x71C890, 0x71CAB0 ...), the save file's write (0x7A5F50: both commits, then the copies into its buffer),
  the menus' own ITEM MANAGEMENT right after its screen (FUN_00BF22A0 step 2: 0x715C10, then the save file's write,
  FUN_007A5B00, when the screen changed something, its +0x188) and the top menu's start (0xA393C0, FUN_00A3E4A0 step
  0, when sBioMain+0x10568 is set - once taken for Item Management's close). Two things take the copy back: `kSigCheckpoint` (0xCEE0C0: gold and treasures from the copy,
  then commit), through sInfoMan's 0x7BA540 from the play flow and the pause menu's own options (0xA2E6A4,
  0xA2E6D1); and a chapter's start (0xCF03D0: all of it back, the box cleaned, commit). The save holds the
  copy's values: the user's save of 15:50 on 2026-10-29 had 700 gold where the editor had set 100000 at 15:36,
  with a play restart between. So the gold editor writes both (`Layout::checkpoint` = 0x4638).
- **cGameInfo**'s temporary block (createProperty names it): mTempBase.{NewGame, StartChara, Difficulty,
  GamePoint, ChapterNo, ChapterStart, StageNo, StartPosIdx, SceNo, OperationNo} at +0x41B0.., and **mTempChap[4]**
  from +0x41D8, 0x18 each: DieNum, EnemyKillNum, AtkFireNum, AtkHitNum (each capped at 9,999,999 by its add,
  0xC41870/0xC418A0/0xC418D0/0xC41900), PlayTime (float seconds, +0x10), PlayTimeEnable (+0x14). The index is the
  character (0 Chris, 1 Sheva); the results screen shows the starting character's (mStartChara).
- **PlayTime is one clock for the chapter**: its only add (0xC41930) is called by sInfoMan's FUN_007B7200, which adds
  the frame's time to all four characters' alike - from 0x724190 (0x7241EE), skipped while sBioMain+0x10438 has
  0x800 (paused) or cGameSys +0x79C is set. FUN_00C419A0 swaps characters 0 and 1's whole blocks (from 0x793EB0 and
  0x7B8370, a network game's flow). The panel shows character 0's and its Set writes all four (`Edit::kTime`; the
  user, 2026-10-29: "isn't this a single timer for the game instead of per player?").

## The item box (Item Management, `uIdItemBox00`; `itembox.cpp`) - built 2026-10-29

- **The box**: cSystemInfo +0x3840, **54 entries of 12 bytes**: u16 id, u16 count (a weapon's: its rounds
  loaded), then 8 bytes of the weapon's levels (their packing into the nine of cItem not read yet). The save keeps
  it at 0x384C (the live offset + 0xC, as the gold). **Treasures** follow at +0x3AC8: 54 x {u16 id, u16 count} -
  see "The treasures" below.
- **Store** (0xCEF2F0, thiscall on cSystemInfo, `(entry*, apply)`): id 0 and 0x100 (hand-to-hand) never. Ids
  0x106-0x108, 0x10A, 0x136, 0x13C-0x13E and 0x201-0x47E stack to 999 in entries of the same id, the rest one to
  an entry. What is left takes the first empty entry. With apply 0 it only answers whether it fits. 0xCEF620:
  "is there an empty entry". 0xCF0700 `(id, apply)`: an entry of the item as it comes (a weapon's magazine, else
  the record's count), stored. A new game (0xCF08B0) stores the M92F (10) and a First aid spray (1): the user's
  box held just those on 2026-10-29. A chapter's start (0xCF03D0) drops entries of id 0x400+ but 0x601 and 0x606.
- **The screen** (uIdItemBox00 < uIdShop, 0x5B88 bytes; slot 6 0xA0F260 frame, slot 9 draw) works on its own
  cells (0x48 bytes: id +0, count +2, where from +5 (2: the box), its box index +6, value +0xC, levels
  +0x30..+0x37). Both cases: +0x1678 + character x 0x288 + slot x 0x48. The box list is rebuilt from the live box
  by **FUN_00A04500(screen, pane)** after each store or take: +0x1C3C + list x 0x1E60, its count +0x1C2C + list x 8,
  sorted by the table at 0x1128C70. The treasures' list (FUN_00A048A0) goes to +0x2B6C, count +0x1C30. A pane is
  +0x1B0 + k x 0xA64 (its character +2, its list +0x58). Selling a weapon puts its rounds in the box (0xA09020).
  Its close commits nothing itself (0xA393C0, once read as its close, is the top menu's start: the costume bits and a
  commit): the box edited live while it is open is kept by the commit that follows - the chapter's start between
  chapters, the menus' ITEM MANAGEMENT's own after it (see "cSystemInfo keeps a checkpoint copy").
- **The saved cases** (cGameInfo; `kSigSavedCases`): +0x240 + character x 0x420, 24 entries of 0x2C bytes - a
  cItem's data from its id on (id, count, most, .., slot +0x14, in hand +0x18, the nine levels +0x1C) - **the
  entry is the slot** (0-8 the grid, 9 the knife, 10 the vest, 11 hand-to-hand), the counts +0x12C0 + 4 x
  character (12). Play makes each character from them as it starts or restarts (FUN_007E9800: the record +0x100
  + 0x50 x character - health, position - then the case through FUN_00B64970, the case's add per entry).
  Item Management works on another copy of the same block, +0x17D0 (FUN_00A34AF0; +0x2D60 for a pane whose +5
  is set): the blocks lie 0x1590 apart. **The game's own save of a case** (FUN_00C932D0, with the character's record:
  health, position): for slots 0-9 `getItemBySlot`'s 0x2C bytes from cItem +4 copied raw (its model pointer too),
  a used-up stack (count 0, its record's most not 0) dropped, the top bit of BulletMax (+0x1F) cleared, 0x10C with a
  model made the variant's id; slot 10 id 0, slot 11 0x100; each entry's slot its index; the count 12; nothing in
  hand -> the one before (or 11) put in hand. That bit is the bonus feature's **infinite ammo**: making a character
  (0x7EA03C) sets it on each weapon whose bit is set in cGameSys +0x4B0 + 4 x character (0x72A8xx sets and clears
  it in these saved entries by the same mask). The save file keeps 12 entries a character by position (0x785070
  writes id, count, nine levels, which is in hand and the one before; 0x7851A0 reads them back: slot = the index,
  most and the next two -1, alive 1, model 0); the loader (FUN_00B64970) skips id 0 and adds each into its slot.
  Read live on 2026-10-29 at Item Management: +0x240 held the cases as they were, +0x17D0 the cases after the user
  had moved ammo, herbs and a grenade into the box.
- **The levels a box entry keeps**: seven of the nine (FUN_00A35AD0, a saved item into a cell; `kSigBoxLevels`) -
  the cItem's levels 0, 2, 3, 5, 6, 7, 8 (Power, ReloadSpeed, BulletMax, Critical, Through, Range, Scope); FireSpeed
  and BulrRate are not kept, the eighth byte is 0.
- **Idle panes** also include the opening state's menu: state 0 ends its animation by setting step 10 (`mov byte
  [esi+1], 0Ah`), its menu (the screen's own: organize, the shop). The user's first try (2026-10-29) had the window
  greyed out: pane 0 was at state 0, step 10 (read live), which the first build did not count as idle.
- **The screen's unit lives only while it is open**: made by FUN_00A07980 from two flows (between chapters,
  0x7189F0; the other, 0xBF22A0) onto line 26 with flag 0x2000, and killed as it closes. Its frame steps +0x14: 0-1
  fading in, 2 the main one (each pane in use updated: FUN_00A0EF00), 3-4 fading out once both panes are done
  (state 0xE, +0x3C 4). A pane: its state the first byte (0..0xD, the update's jump table), the step within it +1;
  in use while its int +0x54 is not negative. State 0 opens it and shows its menu (step 10), **1 browses the case**
  (FUN_00A0B1F0: from it 2 an item's menu, 3 an item picked up (+0x47 = 1), 8/10/11 menus and questions), **5
  browses the box** (FUN_00A0BED0: the cell under the cursor = +0x1C3C + ((+0x590C[list] (0 the box, 1 the
  treasures) + list x 2) x 54 + the scroll +0x5924[list] + the cursor +0x5C) x 0x48). Both open alike: step 0 sets
  +0x70 (the item under the cursor shown afresh) and step 1; step 1 runs; any other waits.
- The rebuild also checks two achievements (0x7B9700, 0x7B9860, both reading both cases and the box): all four eggs
  (0x136, 0x13C-0x13E) -> FUN_007B8BC0(0x14), and every kind of weapon (27 kinds, 32 ids at 0x10CB620, more kinds
  owned than the last entry's 26) -> (0x16). FUN_007B8BC0 unlocks through FUN_00714320(n + 1) unless paused or in a
  network game. The game runs it whenever the box is listed, so an item added by the mod counts as a bought one.
- **The mod** (`kSigBoxFree`, `kSigBoxRules`, `kSigBoxKeep`, `kSigBoxRebuildCall`, `kSigBoxPanes`,
  `kSigBoxPaneUpdate`; inline: the rebuild's start, the panes' state switch, each state's stub `push esi; mov ecx,
  edi; call` and the browsing entry): the box's place, size and count from "is there an empty entry", agreed by the
  store's; the store's rules out of its compares (never 0x100; to 999 for 0x106-0x108, 0x10A, 0x136, 0x13C-0x13E,
  0x201-0x47E; one an entry otherwise); the chapter's keep (below 0x400, or 0x601/0x606); the rebuild from the sale's
  store-then-rebuild pair (the store checked to hold the rules, the call's profile and cSystemInfo to match
  discovery's); the panes from the frame's walk; the browsing states as those whose handler opens with the browsing
  entry - exactly 1 and 5, found by scanning the jump table. `itembox::tick` makes a batch only while every pane in
  use is in a browsing state at step 1. It writes the live box (an entry's change only while it still holds the id
  it showed; adds placed as the store places them, a weapon as it comes - its magazine - and no levels), then
  rebuilds each pane in use and sets the step of one browsing to 0, as the screen's own changes leave it. The window
  (`overlay.cpp` `draw_itembox`, "###RE5CC item box") is drawn instead of the panel while Item Management is up;
  its Add offers the case's tabs as far as the box takes and keeps them. Beside it the game's mouse is not held
  (`dispatch.cpp`): only a click over the window is kept from the game.
- **The box's list** (both places, `draw_box_rows`) is under the same tabs as the Add list (Weapons, Sub-weapons,
  Ammo, Recovery, Armor - each with how many entries it holds - Other items while the box holds something filed
  under none of them, then Treasures: see "The treasures"), in the Add list's order, with a filter, ten rows tall at most (each tab its own scroll), as
  the sibling mods' boxes are. Ticks and typed counts stay under any tab: All ticks the rows shown, Take, Throw away
  and Apply all work on every tab's (the bar says how many ticked are under other tabs - under its buttons when it
  does not fit beside them). The first build had one flat list (the user, 2026-10-29: "the item box imgui menu
  doesn't have the category tabs like we had for previous RE mod installments"). **Max all** (the user, 2026-10-30:
  "add an option in the imgui ui to max all item quantities in the item box") fills every count the list has, under
  every tab and whatever the filter shows: each entry that stacks to its most (999) and each treasure held to its most
  (999), as one batch of the changes Apply makes (`kSet` with the entry's index and id, `kTreasureSet`), so the tick
  side is unchanged (an entry changed since is left alone; at the pause the checkpoint copy and every treasures' list
  too). A gun keeps its rounds (one an entry; its capacity by its level is not read yet) and nothing is added.
- **At the pause in a story chapter** (the user, 2026-10-29: "I want the option to manage the item box while in game ...
  with the normal controls we had for management like in RE4 remake mod") the panel has the box's section: every
  write goes to the live box and its checkpoint copy alike (the game never reverts the box in a chapter: the quit's
  end commits it, a chapter's start takes the copy back). **Store** (a case's slot): the whole item into the box (only
  what the Add lists offer, a grenade launcher as its load: see "What the Add lists offer"; a
  stack onto what the box has, a weapon an entry with its rounds and seven levels), then the inventory's own
  `cItem::clear`, then the item gone from every saved copy of the case (below). **Take** (an entry, into the
  character picked): the inventory's own `cItemManager::add` with the entry's count (a stack: what one case slot
  holds, `items.h`'s most; the rest stays) and its levels (1 and 4 at 0), the entry lessened or cleared, then the
  new cItem's data put in every saved copy of the case. The item in hand is never stored. The first build wrote only
  the saved state's case and took a retry to rebuild the case from it: RESTART and a death's CONTINUE rebuild it from
  the play flow's own copy, QUIT's "No" from the working block's checkpoint copy - see "The saved copies of the cases"
  (the user's play, 2026-10-31: the Dragunov twice and the rocket launcher gone). The panel keeps what was typed or ticked until it goes (`visible_now`); the first build let the
  box's go every frame at the pause, so its ticks never stayed (the user's third play). **Take** (and the case's
  Add) of what the area has not loaded has the game's loader load it first (`inventory::load`, below): the user's
  first Take (2026-10-29, 22:54, an H&K MP5 with 65 rounds into Chris's slot 1) made the game quit - see "What an
  area has loaded".

## The treasures (`itembox.cpp`; `kSigTreasureCount`, `kSigTreasureAdd`, `kSigTreasureLoad`, `kSigBoxLists`) - built 2026-10-30

The user (2026-10-30): "we don't yet seem to have a treasures area for the item box".

- **The lists**: 54 entries of {u16 id, u16 count} (0xD8 bytes), in four places:
  - **cSystemInfo +0x3AC8**, the profile's, right after the box (its checkpoint copy +0x8100): what Item Management
    lists and the shop buys from; the save keeps it.
  - **sPlayer +0x260 + character x 0xD8**, each character's in play. A pickup of category 4 (FUN_007E9DF0) is gold
    for 0x401-0x403 and the score stars 0x427-0x429 (FUN_007E9D50); anything else goes into **every** character's
    list alike (FUN_007E9D80 -> FUN_007E8C20, thiscall on sPlayer `(character, id)`: an entry of the id below 999
    gets one more, else the first empty one is (id, 1); with no room its price goes to the character's gold +0x2DE4).
  - **cGameInfo +0x1330 + character x 0xD8**, each character's saved list, in the saved cases' block (block
    +0x1230). The game's save of a case (FUN_00C932D0) copies the character's list there. Making a character
    (FUN_007E9800, after its case) copies it back. Blocks B (+0x28C0) and C (+0x3E50) are whole-block copies
    (FUN_00C92B70).
  - Between them: a chapter's start (FUN_0072E6E0 -> FUN_007B7C20) copies cSystemInfo's list into the saved list of
    every character in play (cGameSys +0x478 bits). A checkpoint (FUN_007B9310, from 0x7BA830 and the pause menu's
    0xA2E579: the cases saved by FUN_00C93630, then the character's saved list into cSystemInfo's) and the chapter's
    end (FUN_007BA850 -> FUN_007B95D0 for each character of this machine's profile) copy back. Both mark each of the
    game's treasures found (cSystemInfo bits, FUN_00CEE380 / FUN_00CEE3B0: the "new" Item Management shows).
    FUN_00CEE090 commits the gold and the treasures to the checkpoint copy; `kSigCheckpoint` takes them back.
- **A count set** (FUN_00CEF5D0, thiscall on cSystemInfo `(index, count)`, the sale's): at 0 the id is cleared only
  when another entry holds the treasure, else the entry stays at 0 - **the mark of one found**. Item Management lists
  every entry with an id (count 0 too), and its treasure achievement counts them.
- **The game's treasures**: its own table of 50 ids at 0x10CB6A0 (count 0x10CB704): 0x417-0x425 and the gems
  0x450-0x47E (five cuts in five colors, and the five specials 0x46C-0x470). Item Management's sort table 0x1128C70
  lists the same 50 in another order, each up to 999. Not in it: 0x401-0x403 (gold pickups), 0x404 Sapphire and 0x426
  Beetle (Emerald green) (leftovers), 0x427-0x429 (score stars, gold). `tools/gen_items.py` writes them to
  `items.h`'s `kTreasures` with the gold the shop pays for one: its price list 0x112C4E8 (FUN_00A356B0) over 10 - a
  cell's value; a sale (FUN_00A09200) pays the value times the count, "sell all" is FUN_00A053C0.
- **The screen's treasures list**: FUN_00A048A0(screen, pane) copies every entry with an id from cSystemInfo's (the
  pane's profile chosen around it: FUN_007B66E0 / FUN_007B6740) to +0x2B6C + list x 0x1E60 (right after the box's 54
  cells), its count +0x1C30 + list x 8. Then FUN_007B9A50: all 50 in the list -> FUN_007B8BC0(0x18), the treasure
  achievement. FUN_00A089C0 (from the screen's FUN_00A0AEA0) rebuilds both lists of each pane; a sale rebuilds the
  treasures' once a count reaches 0.
- **The mod**. Discovery reads:
  - cSystemInfo's list from the set-count (checked to lie right after the box, and by `kSigCheckpoint`);
  - sPlayer's from the add (the 999 from its compare);
  - the saved ones from the character's making (after `kSigSavedCases`; the profile index and cGameInfo's size
    agree);
  - the treasures' rebuild from FUN_00A089C0: its first call is the box's rebuild as found, and its panes and list
    field agree. The rebuild's own start has the same list field, step and cells, its list right after the box's.

  A change carries an id and how many are held (an Add: how many more, up to 999 in all). The tick works the change
  out by the list shown, then makes it in every list by as much: none below 0, entries topped up to 999 and then empty
  ones, the mark of one found kept or cleared as FUN_00CEF5D0 does. Beside Item Management that is cSystemInfo's list
  alone, then FUN_00A048A0 for each pane in use (approved 2026-10-30). At the pause it is each character's list in
  play (this machine's first: the one shown), each one's saved list, and cSystemInfo's with its copy. So a retry or a
  chapter's end keeps it, and a list behind (a pickup since the checkpoint) keeps its difference.

  The panel shows them as a **Treasures** tab of the box's banks (after Armor and Other items) and of its Add list
  (the game's 50, each with a count up to 999). Ticks and typed counts are kept by id; Throw away and Apply all work
  with the box's; there is no Take.

## Items (`inventory.cpp`, `items.h`)

- Each player has a **cItemManager** at +0x21A0 (`kSigCaseLoad`: the chapter start loads the case through its
  add): a vtable, then **24 cItems** from +4, 0x30 bytes each (`kSigItemMgrCtor`; cItem's record says 0x30).
- **cItem** (its createProperty names the fields; `kSigItemSet`, `kSigItemModel`, `kSigItemAlive` read them out of
  cItem's set): +4 Id, +8 Num (a gun's: the rounds loaded), +0xC Max (a gun's: its capacity), +0x18 SlotNo (0-8 the
  3 x 3 grid, 13 none), +0x1C EquipType (1 in hand, 2 the one before), +0x20.. the nine levels (Power, FireSpeed,
  ReloadSpeed, BulletMax, BulrRate, Critical, Through, Range, Scope; BulletMax 14+ = infinite), +0x29 Alive, +0x2C
  the weapon's model (a unit).
- A slot is in use when an alive item has that SlotNo (`cItemManager::getItemBySlot`, 0xC4B700). The add
  (0xC4E4F0, through sItem's lock 0xC4B480) takes a free item, picks a slot (`findFreeSlot` 0xC4D590: ammo, herbs,
  sprays and vests the even slots first, the rest the odd ones; a vest's record type 3 goes to slot 10), then
  cItem's set (0xC4E290). With an owner (the character) and an item whose record's +0x1C is set (every weapon,
  ammo, recovery item and vest), the set gives the item a model in the owner's hand (FUN_00C4C9B0): the one handed
  over as the last argument (a pickup hands over the floor's), else a new unit of the item's model class - see
  "What an area has loaded". The model is a `uItem` (`uWp*`, `uIt*`); the set then writes its +0xFDC = the owner,
  +0x1936 = 1.
- **The item table** (`tools/gen_items.py` -> `src/items.h`, 185 items): the item-info lookup 0x763190 indexes a
  per-category array (1 weapons .. 6 armor) of 0x48-byte records: +8 the count it comes with (a treasure's: its
  price), +0xC its most, +0x18 a recovery item's healing (a float), +0x24 a weapon's kind (1 firearm, 2 melee, 3
  thrown, 7 the lantern), +0x2C how it is used (1 a recovery item; 2 a weapon with an action of its own - the
  Proximity bomb, and the scene-only weapons and their rounds, see "What the Add lists offer"; 3 the Rocket; 4 the
  eggs), +0x44 2 for a magazine-fed firearm, +0x40 its name's line in **mes_item_e** (`Image/Archive/Msg2Resource_e.arc`, an MSG2 file: u16 pairs
  {glyph, width}, {0,1025} ends a line; the English glyphs run in pairs - digits from 0, `( ) space & . ' - +`,
  capitals from 56, small letters from 108, `_` 282).
- The Add list's tabs: Weapons (magazine-fed firearms), Sub-weapons (the other weapons), Ammo, Recovery, Armor
  (the vests; American spelling, at the user's word, 2026-10-29). Key items and treasures are not offered: the
  treasures go in a list of their own, which the item box's Treasures tab edits (see "The treasures"). Both Add
  lists offer only what the story gives Chris and Sheva to carry (`items.h`'s `offer`, 54 items), and the
  Gatling gun and the Longbow only to their owner (`only`): see "What the Add lists offer".
- `items.h` also has `kTreasures`: the game's own table of its 50 treasures, with the gold the shop pays for one
  (`gen_items.py` finds the table and the price list by their code, and stops if a treasure is not in the item
  table).

## What the Add lists offer (`items.h`'s `offer` and `only`) - 2026-11-01

The user (2026-11-01): "There seems to be extra items that aren't real in there. Compare with save data to see what
I've found, and question the items I haven't." The Add lists offered every item of the table that has a model: four
knives, two Samurai Edges, seven grenade launchers, two of each herb among them.

- **The user's save** (14:57; `tools/save.py`, see "The save file"):
  - Every weapon the shop sells is unlocked: cSystemInfo's mShopBuyItem bits 0-32, the shop's price list's first 33
    entries (its 23 weapons, the six kinds of rounds, the spray, the two vests).
  - The box and the cases hold all of those and all 11 kinds of ammo, the herbs 0x305-0x307 and 0x309, and the White
    and Rotten eggs.
  - Chris's knife 0x101 and Sheva's 0x10E sit in each case's slot 9.
  - 49 of the 50 treasures (not the Sapphire (Marquise) 0x475).
- **The rule**: an Add list offers the items of the shop's price list (0x112C4E8, 100 entries: its weapons, rounds,
  grenades, eggs, ammo, recovery items and vests; the treasures it buys stay off the grid's Add), and every weapon the
  "every kind of weapon" achievement counts (0x10CB620, 32 {kind, id} pairs, 27 kinds). The achievement adds the
  launcher's five other loads.
- **Kept**:
  - The user's finds.
  - The Brown and Gold eggs (in the price list; the egg achievement wants all four: 0x136, 0x13C-0x13E).
  - The launcher's six loads, 0x125-0x127 and 0x139-0x13B (the user: keep all six). Each is the same gun with
    explosive, acid, nitrogen, flame, flash or electric rounds:
    - The model switch (FUN_00C4C9B0) gives each load its rounds (0x140-0x142, 0x14D-0x14F).
    - The game's save of a case writes the load's id back (FUN_00C932D0).
    - The combine recipes make them (0x1146A00, u16 triples: launcher + rounds = load).
    - The achievement counts all six as one kind (19).
- **Left out** (the user's answer for each group: remove):
  - **The floor herbs**, 0x301 Herb (Green) and 0x302 Herb (Red), are what enemies drop (the random drop table
    0x10E88F0, 0x6C an entry: entries 4 and 10). A pickup (FUN_007E9DF0) turns them into the shop's 0x305/0x306
    (FUN_007BC990) before the case's add, so no case holds one. The game's counts of what the players hold do the
    same. 0x302's record heals 1000 and is usable alone; 0x306's heals 10000 and is not.
  - **The launcher's in-hand id**, 0x10C "Grenade launcher": the switch gives it the explosive rounds, as 0x125, and
    the save writes it as 0x125.
  - **The knives**:
    - 0x101 is Chris's and 0x10E Sheva's, each in its case's slot 9; the game never adds, stores or moves them.
    - 0x123 and 0x124 are neither's. 0x123 is a re-skin of Chris's (its archive uses wp01's motion and effects);
      0x124 has a motion of its own.
  - **The Mercenaries' guns**: Samurai Edge 0x129, Samurai Edge 0x112 and the Px4 0x11F.
    - 0x112 is a later copy of 0x129 (its archive reuses wp29's sounds and effects).
    - The Px4's archive holds Jill's own motion (pl02wp1f). Guides: Jill (BSAA)'s gun in the Mercenaries; Wesker's
      and Barry's Samurai Edges.
    - None is in the price list or the achievement. The drop table's handgun ammo checks for all three (the
      Mercenaries share its drops).
  - **The scene-only weapons and their rounds** (record +0x2C = 2, as the Proximity bomb's - which the shop sells;
    the 2026-11-01 note "exactly these five" missed it):
    - the Lantern 0x130 (guides: 2-2's dark mine path);
    - the Flamethrower 0x121 (5-2's Uroboros Mkono fight, on a refuelling stand);
    - the L.T.D. 0x134 (6-2's satellite laser);
    - the RPG-7 NVS 0x135 and the RPG round 0x211 (6-3's hangar, against Wesker).
  - Left out before:
    - with no count: hand-to-hand 0x100/0x137, the gun turret 0x12F;
    - with no model: the Rocket, the Arrow, Herb (X), the G+G+G/G+X/G+R+X herbs, the recovery-filed eggs
      0x30C-0x30E, the Nile perches, Water.
- **Who may have it** (the user: "Follow the game"):
  - Item Management's menu for a box entry (FUN_00A08A00) offers it to a pane's character (+2: 0 Chris, 1 Sheva),
    unless it is 0x115 and the character 1, or 0x10F and 0. So the Gatling gun is Chris's alone, the Longbow Sheva's.
  - Their archives agree: uWp15 holds only Chris's skeleton's motion, uWp0f only Sheva's.
  - `gen_items.py` reads the two ids and characters out of that code (`only`).
  - The case's Add greys the other character's (its hover says whose).
  - The box's Take into the other's case is greyed. A batch's Take of one is refused by the tick, and the note says so.
  - The tick refuses both.
- **Weapons by skeleton**: a weapon's archive holds the motions of the skeletons that can wield it.
  - Chris's skeleton (pl00): Chris, Wesker, Josh, Barry, Irving. Sheva's (pl01): Sheva, Jill, Excella, Rebecca. Each
    `uPl*.arc` holds pl00wp06 or pl01wp06.
  - Chris's only: the knives 0x101 and 0x123, the Gatling gun, both Samurai Edges. Sheva's only: the knives 0x10E
    and 0x124, the Longbow.
- **The mod**: `items.h`'s `offer` (`gen_items.py`: the price list's items and the achievement's weapons, categories
  1-3 and 6) and `only` (the check's).
  - `tab_of` files only what is offered. So both Add lists follow it, and so do the box's tabs (an entry of something
    not offered is under Other items).
  - `list_name` gives an id only to a name the offered items share ("Herb (Green)" goes by its name).
  - The tick's Add and the box's Add take nothing else.
- **Store** (the user, 2026-11-01, of a scene's own weapon held at the pause: "store should refuse those types of
  items"): the box takes only what the Add lists offer, so such an item stays in the case.
  - `itembox::store` refuses it (the note says so, the log names it); the panel greys its Store and says why.
  - It goes by the id the box keeps the item as: a grenade launcher's load. In play the case's item has the
    launcher's id 0x10C once it has its model (the switch, FUN_00C4C9B0: `kSigModelMake`), and the model (a
    uWpLauncher) keeps the rounds loaded at +0x1C80.
  - The game's own save of a case (FUN_00C932D0, `kSigSaveLauncher`) writes the load back from them: 0x140-0x142 ->
    0x125-0x127 (EXP, ACD, ICE), 0x14D-0x14F -> 0x139-0x13B (FLM, FLS, ELC); any other, or a model of another
    class, keeps 0x10C. `inventory::as_saved` reads the same (discovery checks the id against the switch's, the
    field against cItem's, the record against uWpLauncher's, and each load's model against the launcher's), and
    the view gives each case's item the id it is kept as (`kept_as`).
  - Store used to put 0x10C itself in the box: under Other items once 0x10C left the Add lists, and loaded with
    explosive rounds whatever it had (the switch gives a bare 0x10C 0x140), so an acid load came back explosive.
  - A launcher whose load cannot be read stays in the case.

## The save file (`savedata.bin`; `tools/save.py`) - read 2026-11-01

- **Encrypted**: each byte is XORed with the encrypted byte 8 before it, the first 8 with the key at 0x11AFE48
  (FUN_00714380 encrypts, FUN_00714400 decrypts).
  - The save job (FUN_007D4510) copies the save data's buffer (+0x110, 0x5D50 bytes) and writes it through the
    encrypt.
  - The load (FUN_007D4330, FUN_007D3840) decrypts into it.
- **Decrypted** (the writer FUN_007A5F50):
  - +0 the magic 0x08112100, +4 0x4BC0, +8 the checksum.
  - The checksum (FUN_007A5C00) is the u32 sum of 0x3BBC bytes from +0x10 and 0x1700 from +0x3BB0. FUN_007A6060
    checks it and the magic before it takes the copies back.
  - +0x10: the profile's checkpoint copy (cSystemInfo from +4: a live field f at f + 0xC), 0x3B9C bytes. Its last
    0xA9C bytes are at +0x52B0.
  - +0x3BB0: cGameInfo +0x4240..+0x5940. The working block's checkpoint copy starts at +0x3C40: the cases at +0x3D80
    + character x 0x420, their saved treasures' lists at +0x4E70 + character x 0xD8.
  - So the gold is at +0x194, the box at +0x384C, the treasures at +0x3AD4.
- **The shop's unlocks**: cSystemInfo's mTmpData.mHistory.mShopBuyItem[2] (+0x13C, createProperty's name), bit k
  the price list's entry k. mShopBuyNew, mShopChgItem/New (Bonus Features), mSpsWeapon and mViewer* sit beside it
  (+0x110..+0x160).
- `tools/save.py` prints a save's gold, exchange points, the shop's unlocked entries, the box, the treasures and both
  cases. With --missing it also lists what the Add lists offer and the game's treasures that the save holds none of.
  It only reads. It finds each place by:
  - the game's code (the key, the writer's parts, the checksum);
  - the mod's signatures (kSigBoxFree, kSigTreasureCount);
  - createProperty's names (`mt_props.py`);
  - a saved case's shape, as the game's save of one leaves it (entry k is slot k, entry 11 hand-to-hand).

## What an area has loaded (`inventory.cpp`; `kSigLoadList`, `kSigLoadReady`, `kSigLoadCurrent`, `kSigModelSwitch`, `kSigModelMake`)

- **The crash** (the user's play, 2026-10-29): the log's last lines were "entry 12 (H&K MP5 (MG), 65) taken into
  Chris's case (slot 1)" at 22:54:25.795, then "process exiting" 1.8 s later at the pause - no CRASH line. The game
  had created `re5LoadFails_jim.txt` in its folder at 22:54:25.810 (empty: the game quit before its write was
  flushed). On a failed file open (0x6F9690) the game appends "Fail: '%s' %d" to that file and calls its fatal error
  (0x6CFEF0 "Failed open file. %s %d"), whose handler is FatalAppExitA (0x422D00, set by sApp's constructor into
  0x11AF640): a message box, then the process ends - through no exception, so the crash logger sees nothing.
- **Why a file**: the add's model for an item with no model handed over is FUN_007BD7B0(7, id, 0, 0): the MT class
  record keyed `(7 << 16 | id) << 8` (the record's +0x20; FUN_006D0690's hash table at 0x12386A0), checked to derive
  from `uItem`, made a unit on line 10 (weapons) or 15 (FUN_007BD750). The MP5's is `uWp13`, handgun ammo's
  `uIt0201`, the spray's `uIt0304`, a vest's `uIt0601`; Arrow, Rocket, Herb (X) and some others have none (the add
  then fails). Its resources - `pawn\wp\wp13\model\wp1300`, its motion, both characters' motions for it
  (`pawn\pl\pl00\motion\pl00wp13`, `pl01wp13`), sounds, effects - are all in the class's archive,
  `nativePC_MT/Image/Archive/uWp13.arc` (one per class: 128 `uWp*.arc`, 130 `uIt*.arc`); there is no loose
  `pawn\wp\wp13`. Made while its archive is not loaded, the unit looks for the loose files.
- **The model's switch** (FUN_00C4C9B0, `kSigModelSwitch` + `kSigModelMake`): for ids 0x10C..0x13B a byte table
  (0xC4CB40) into a jump table (0xC4CB24); 0x10C, 0x125-0x127 and 0x139-0x13B (the grenade launcher and its
  rounds' variants) set the cItem's id to 0x10C, make `uWp0c` and give the new `uWpLauncher` its rounds (+0x1C80 =
  0x140..0x142, 0x14D..0x14F); the rest their own.
- **sLoadMan** (the system `sLoadMan`, 0x208E8 bytes; its instance at [0x11B26C4]): `FUN_007D5820(kind, id, variant)`
  asks it for a class's archive - `Image\Archive\<class name>` (FUN_007D4960; a few special names: Josh's costumes,
  uEm10) - and for what the class needs with it (kind 7: FUN_00770380's item info +0x1C and +0x2C, and a switch:
  the rocket launcher's rocket 0x144, the grenade launcher's rounds 0x206.., ...); 108 call sites. The requests go
  into **10 lists** (MtArrays at +0x20708 + k x 0x14: count +4, items +0x10), the one +0x207D0 names; an entry
  (0x54 bytes, vtable 0xF643A4): path +4 (0x40 chars), the archive (an rArchive resource) +0x44, state +0x48 (1 or
  4 asked, 2 loading, 3 loaded; 0 after the list is chosen again, until asked again), the class record +0x4C. Its
  frame (slot 6, 0x7D53E0) runs jobs (+0x20340 + k x 0x54, 11 of them; job 1 a list's archives, FUN_007D4180:
  `sResource::create(rArchive, path, 2)`, done when the archive's flags +0x4C have bit 0; bit 4 failed, asked
  again); packs (`Image\Pack\Pack%d`) load several at once. The lists keep their entries (and the archives) until
  the area or the chapter lets them go (FUN_007D4AF0 drops the never-loaded ones of lists 1-9).
- **Loading in play** (the game's own way, as an area's load asks): the request (FUN_007D5820, thiscall `(kind,
  number, variant, 0)`, `ret 10h`; `kSigLoadRequest`, its format "Image\Archive\%s" by `kSigLoadPath`) adds an
  entry (state 1) to the list +0x207D0 names unless it holds one (FUN_007D48D0: one in state 0 made 3 again), and
  asks for what the class needs; the commit (FUN_007D4A10, thiscall, no arguments; `kSigLoadCommit`) lets go of the
  list's entries in state 0 (with their archives) and, when it holds new ones (1 or 4), sets the list's job (+0x20340
  + k x 0x54: +0x44 = 1 its kind, +0x48 = 1 asked). sLoadMan's frame (sBioMain's frame calls every system's slot 6,
  paused or not) takes the job up (+0x20: 0 idle, 1 three frames on, 4 the jobs' steps), FUN_007D4180 makes each new
  entry's archive (`sResource::create(rArchive, path, 2)`) and polls it to state 3; at the end FUN_0079CFF0 clears a
  table of load records. Packs (+0x207E0) are never on in this build (only the constructor and destructor write it).
  The area's own sequence is FUN_007D4870 (choose the list: all its entries to 0), its requests, then the commit -
  the busy test FUN_007D3A50 (+0x20, the jobs, sResource's queue) is polled only by the play flow's loading states
  and the load or quit flow.
- **The lists' roles** (their choosers, read 2026-10-30): 0 the core (FUN_007D4CB0/FUN_007D4CD0: LoadResource,
  CoreResource, coreEff, Msg2Resource), 1 the title and menus (FUN_007D4D90), 2 the game's (FUN_007D67A0:
  GameResource, and FUN_007D6440's common items - ammo, recovery items, the eggs, gold), 3 the characters' (FUN_0072E6E0,
  an area's start), 4 the area's sets (FUN_0072CF40), 5 its objects (FUN_0072ADE0, `uOm*`), 6 the stage (FUN_007D5050:
  `Image\Archive\s%03d`, and `s%03dSub`), 7 an event's (FUN_007D5130), 8-9 Item Management's item models
  (FUN_007D6660, from FUN_00A07690). A list's leftovers go only when that list is chosen and committed again. A
  commit while the list's job still loads starts the job over (its +0x40.. +0x50 set again); the mod commits only at
  a pause. The job (FUN_007D4180) makes the archive of an entry in state 1 or 4 (`sResource::create(rArchive, path,
  2)`, state 2), polls it to 3 (bit 0; bit 4 failed: made again), and skips 0 and 3; a request finding its path in
  the list in state 0 makes it 3 at once (FUN_007D48D0) - an archive still loading then finishes in sResource's
  queue, which the busy test covers too.
- **Lists 6 and 7 are not like the others.** The stage's (6) has walks of its own (FUN_007D5240, FUN_007D5320). The
  cutscenes' (7) is chosen only by the event loader FUN_007D5130 (in the story both players' face archives,
  `Image\Archive\event\evfpl<costume>` by cGameSys +0x4A0 (FUN_007B14D0: pl00, or pl10/11/20/22 for Chris's costumes;
  pl01, pl13/12/21/23 Sheva's), then the event's `Image\Archive\event\ev<stage>_<n>` (FUN_007B1470, the table
  0x10E7C08)), and is let go whole once a cutscene has played (FUN_0071E800 -> FUN_007D4BA0(7): every entry and its
  archive, the array freed) - yet it stays the list chosen last (+0x207D0 is set only by a choice). A cutscene is
  either preloaded - an area's start loads the scene's next one (FUN_007286C0, the table 0x10CA9A0: scene -> event;
  stage 309's scene 9500 -> ev309_00) - or loaded as it is due: the play flow's event step (FUN_0071E600, from the
  event sequence FUN_00725DB0's step 1) skips the load when **any** entry of list 7 is loaded (FUN_007D51D0, taking
  it for the cutscene preloaded) and otherwise loads it and waits. A cutscene's scene names its models by data
  (`event\s309\ev10\ev_scene`: `pawn\pl\pl%02d\model\evf0000`, the costume's number filled in), so a cutscene
  played unloaded fails on its first model: Chris's face.
- **The mod** (`inventory.cpp`): in play an item goes into a case (the case's Add, the box's Take) when some list
  holds `Image\Archive\<its model's class>` with its archive loaded (bit 0, not bit 4) - read as the pause opens,
  every half second while it is up and before each change. Otherwise, when the item has a model class, `load` asks
  the game's request for that class (and for the model's own class when the switch gives the item another's: the
  grenade launcher's rounds, `uWp0c`), then commits once - into the area's sets' list (4, `kSigSetsLoad`: chosen and
  committed at each stage's load, FUN_0072CF40, and after a cutscene that changes the sets), whichever list the game
  chose last, which is put back after; never while any list holds an entry in state 0 (the game choosing a list's
  archives: a commit would let them go, and a request into list 4 makes the characters' list's entry of the same
  path 3, FUN_007D4960) - and notes the archives that were new to the list; the
  change waits (an Add in `g_waiting`, a box batch in `g_pending` with the batches after it behind it) until what
  the item needs is loaded (`ready`: its model's class, and its own when the two differ), then is made; dropped if
  the pause closes first, made without it after about 20 s. **Preloading** (the user, 2026-10-29: "We shouldn't
  expect the user to wait on their own, we want this seamless"): as a pause opens, everything the box holds that the
  area has not loaded is asked for in one commit (`preload`, each item once a pause, the weapons first) - the game
  stands still, so nothing stutters, and it is in long before a Take can be pressed; an item that enters the box at
  the pause too; an Add list's pick at the next tick (`preload_soon`). Nothing is preloaded past 64 items' and
  weapons' archives loaded at once (`kItemArchivesMax`; the game is not large-address-aware - a weapon's archive
  unpacks to 1.4 MB on average, an item's to 0.1 MB); the rest load when taken. While something asked for is
  still loading the loaded list is read every frame. Every item the panel offers has its archives on disk, and so has
  everything the request asks for with it (checked over the dump and the folder, 2026-10-29).
- **Until 2026-10-31 the mod asked into the list chosen last** - after a cutscene, the cutscenes' list, empty: the
  pause's preloads made it hold loaded entries, so the next cutscene due was taken for preloaded and played unloaded
  (the user's 5-3, below). Now never: the sets' list, read out of `kSigSetsLoad` (its choice's call checked to be
  `kSigLoadCurrent`'s), and the cutscenes' list out of `kSigCutscenePreloaded` (FUN_007D51D0: its count and items one
  list apart, the state it counts the entries'), logged at the start: "inventory: the mod asks the loader into the
  area's sets' list (4: ...), never the cutscenes' (7: ...)".
- **The item list's leftovers** (`inventory::carried`): Arrow, Rocket, Herb (X) (twice), Herb (G+G+G), Herb (G+X),
  Herb (G+R+X), a second set of eggs filed as recovery items, the two Nile perch and Water have no model class. No
  weapon names the Arrow or the Rocket as its ammo (FUN_00770380's +0x1C: the Longbow's and the launchers' are none),
  and the game's own chapter start could not keep one in a case (its add, with the character, fails without a
  model). Neither Add list offers them (the user, 2026-10-29: "those items ... really don't have models").

## The saved copies of the cases (the item box's moves at the pause) - built 2026-10-31

The user (2026-10-31): "I put a rifle into the item box while in game then quit the level, and this duplicated the
item, and I think it also destroyed my rocket launcher". The log: 15:26:53 the rocket launcher taken from entry 23
into Chris's slot 3, 15:27:41 the Dragunov stored from his slot 1, 15:27:46 the game ended - RESTART (Item Management
at once: the last checkpoint was the chapter's start). The case came back as the chapter began (the Dragunov in it,
no rocket launcher) while the box kept both moves.

- **The pause menu's options** (mes_system_e 1832-1851: RESUME, RESTART "Restart game from last checkpoint." / "Restart
  game? (All unsaved progress will be lost.)", OPTIONS, END CO-OP PLAY, QUIT "End game session." / "Quit game? (All
  unsaved progress will be lost.)"): option ids 1 RESUME, 2 RESTART, 3 QUIT (the run FUN_00A302D0's switch on the
  pane's +0x34, `kSigPauseOption`); RESTART and QUIT share their handler FUN_00A2F660 - see "Chapter Select from the
  pause".
- **The play flow's copy of cGameInfo** (aGame +0x130, `kSigFlowSnapshot`): made (a cGameInfo, its constructor
  FUN_00726F50) at the first FUN_00723660 and copied over at each (the area's start FUN_007247F0, a checkpoint
  FUN_00724750, an event step FUN_00724B50, the pause's "yes" save 0xA2E5A0): FUN_00C41600 (thiscall on the profile's,
  the copy the argument) copies mTempBase/mTempChap (+0x41B0, 0x88 bytes), the sub-object at +4 (0xE0 bytes) and the
  save data's three blocks (FUN_00C92C50 on +0xF0: the saved state +0x10, the working block +0x15A0, the partner's
  +0x2B30, 0x1590 bytes each) - not the working block's checkpoint copy (+0x42D0). FUN_00723870 copies it back
  (thiscall on aGame, `(keep)`: 1 and 2 keep the chapter's counters and more): RESTART (0xA2F807, 0), a death's
  CONTINUE (0x72573F, 2), the pause's "yes" save (0xA2E56A, 0) and two other flows. FUN_00723660 also saves the cases
  (FUN_007BA830: FUN_007B9310 for each character, the live cases into the saved state, FUN_00C93630) and commits
  (FUN_00715C10: cSystemInfo's and cGameInfo's - the working block over its copy).
- **The save question** ("Overwrite current equipment and status? (You will lose any unsaved progress by choosing
  No.)", mes_system_e 1859): FUN_00A2E4C0 (the pane, its step +0x59): asked only where FUN_00A2D540 says the player may
  save - QUIT (3), 7, 10, 0xC, and RESTART only online - so offline RESTART asks nothing and keeps nothing. Yes: the
  play flow's copy restored (FUN_00723870(0)), the live cases saved (FUN_007B9310), FUN_00723660(2) (a new copy, the
  commits), the save file written (FUN_007A5B00). No (its step 2, `kSigPauseNoSave`): unless RESTART, sInfoMan's
  take-back FUN_007BA540 - cSystemInfo's gold and treasures back from its copy (FUN_00CEE0C0, `kSigCheckpoint`),
  cGameInfo's take-back FUN_00C42610 (its play data back, the working block from its copy, the saved state from the
  working block unless NewGame), then the commit.
- **So the case is made again from**: the saved state (a chapter's or a checkpoint's start, CONTINUE), which RESTART
  and a death's CONTINUE take back from the play flow's copy, and QUIT's "No" (and the story menu's start) from the
  working block, itself back from its checkpoint copy. Each save of the cases (FUN_007B9310: a checkpoint, an area's
  start, QUIT's yes) copies the saved state into the working block too - except in a 6xx stage (606, 607, 609-620:
  special stages) or before the scene 0x497 (chapter 1-1's opening; the stage table 0x10CAFB0's scene numbers run
  from 1000) - and each stage area's load (FUN_0072E6E0, the stage's readiness FUN_0072EAC0) copies the working block
  back into the saved state (FUN_007B7900, B into A for each character in play). So the working block is the last
  save's equipment, its checkpoint copy the last commit's. The box is in none of them.
- **The mod** (`itembox.cpp`, `menus::saves`: the saved state +0x100, the working block +0x1690, its copy +0x42D0, the
  play flow's copy by its field, checked a cGameInfo each time): a Store or a Take at the pause is made in each of the
  five copies of the character's case (the profile's three blocks, the play flow's saved state and working block) as
  if it had been made before each was saved. A Store: the item gone - its slot's entry first, else another of the same
  item (by its model's id: a launcher's case item is 0x10C, the save keeps its rounds' variant), a stack lessened by
  what was stored (none below 0), anything else let go; the copy that had it in hand puts the one before (or
  hand-to-hand) in hand, as the game's save of a case leaves one. A copy without it (picked up since) is left alone:
  the box keeps it through a restart to that copy. A Take: the new item as the game's save of a case writes one (the
  box's id, the entry's slot, not in hand, BulletMax's top bit cleared) in the slot it took in the live case when that
  is free in the copy, else the first free one; a copy full when it was saved gives up an entry of another stack the
  live case holds less of now (what a restart would have handed back); with none such the Take is refused before
  anything is done ("not taken: that case as the game last saved it has no room"). The treasures' changes go to each
  character's list in the four other copies too. No moves without the copies understood, nor while Chapter Select
  goes (`menus::leaving`: the inventory's changes and preloads wait too).

## The extra episodes' item boxes (`itembox.cpp`, `game.cpp`; `kSigEpisodeCommit`) - built 2026-11-03

The user (2026-11-03): "We need a separate instance of the item box for the other game modes such as Lost in
Nightmares. These extra content portions should all have their own item box".

- **The modes** (cGameSys +0x58; "The modes" in cheats.md): the story menu's choices 8 and 9 (EXTRA CONTENT's LOST IN
  NIGHTMARES and DESPERATE ESCAPE, mes_system_e 568-569) run FUN_00BF9560, which sets the mode to (choice != 8) + 3:
  **3 Lost in Nightmares, 4 Desperate Escape**. The game's own test of them is FUN_00715AE0 (on cGameSys: `cmp eax,3;
  je; cmp eax,4`); FUN_00715AC0 is the Mercenaries' (2 or 5).
- **An episode's start** (FUN_00BF55E0, on the profile's save data, cGameInfo +0xF0):
  - FUN_00C92D60(player, character, costume) makes each player's record in the saved state and a case from the
    character's table: Lost in Nightmares (0, Chris, 8) and (1, Jill = 2, 3); Desperate Escape (0, Josh = 4, 1) and
    (1, Jill, 2). The saved treasures' lists are emptied.
  - FUN_00C93000(player, loadout) then puts the episode's loadout in the saved state's case. The table is at 0x10FBDD8,
    12 bytes an entry: the items (0x2C each, as a saved case has them) and their count at +8. Lost in Nightmares'
    0x36/0x37: the M92F and a First aid spray each. Desperate Escape's 0x3A: Josh the M92F and a spray; 0x3B: Jill the
    VZ61. Each character's knife is in slot 9 (Josh's 0x114).
  - The profile keeps only each episode's unlocks (cSystemInfo +0x4628 + 4 x episode, & 0xF: FUN_00CEEB30 - its
    second character and Professional).
- **Lost in Nightmares' stage 705** (FUN_0072E6E0, an area's start, in a mode other than the story's: stage 705 at its
  start position): the loadouts 0x38/0x39 go into the saved state - both cases empty but the knife and hand-to-hand
  (in hand). Whatever was taken from a box before then goes with the rest.
- **What an episode keeps, and where**:
  - Its checkpoints save the cases into the saved state (FUN_007B9310, all modes but 1, 2 and 5). In stages 700-706
    they also copy the saved state into the working block, and each character's saved treasures over cSystemInfo's
    live list.
  - The play flow's checkpoint save (FUN_00723660) makes its copy of the save data (aGame +0x130), but never commits
    in an episode: `kSigEpisodeCommit` is its call of FUN_00715AE0 before the commits. So cSystemInfo's and the
    working block's checkpoint copies stay the story's.
  - RESTART and a death's CONTINUE take the saved state and the working block back from the play flow's copy
    (FUN_00723870, all modes but 1, 2 and 5).
  - Only a story area's start makes the saved state from the working block (FUN_0072E6E0, mode 0: FUN_007B7900).
  - An episode's QUIT asks no save question (FUN_00A2E4C0 step 0 asks only in mode 0). It takes the profile back
    (FUN_007BA540: the gold and treasures, the working block from its checkpoint copy, the saved state from that)
    before the save file is written. The story menu's start does the same (FUN_00BF9760 step 0).
  - So nothing of an episode reaches the story's save, and no episode keeps a box or its cases from one run to the
    next.
- **The mod before**: the panel's box at the pause was the story's in every game but the Mercenaries'
  (`!run.mercenaries`). So an episode showed the story's box and treasures, and a move there was made in all five
  saved copies of the case - the working block's checkpoint copy, the story's, among them.
- **The mod**. Discovery: `kSigEpisodeCommit` (the play flow's checkpoint save) gives cGameSys's field in sBioMain (+15)
  and the call of FUN_00715AE0 (+19), whose body is matched with the two modes in it and gives the mode's offset (+2).
  `game::Run::mode` is read each tick from an object of cGameSys's class (-1 when not understood). `itembox.cpp` picks
  the box by the mode: the story's (0, the game's own), an episode's (3, 4, the mod's), or none (the Mercenaries,
  Versus, the mode unknown).
  - **An episode's box**: a buffer in the game's box's layout (g_count entries of g_entry bytes: id, count, the eight
    level bytes), under the same rules (`most_of`, `kept`, the Add lists' offer), with no checkpoint copy. No
    treasures: the view's `treasures` is false, so there is no tab and nothing is changed.
  - **Its file** beside the mod: `RE5CabbyCodes.itembox.lost-in-nightmares.ini`,
    `RE5CabbyCodes.itembox.desperate-escape.ini`. Comment lines start with `;`; an entry is `<id>:<count>[:<the eight
    level bytes in hex>] ; <name>`.
    - Read at discovery. No file: an empty box. Any line that is not an entry the box can hold (what the Add lists do
      not offer, what the box's rules refuse, a stack past 999 or with levels, a 55th entry): the file is left as it
      is and the box unused this run (`unreadable`).
    - Written whole (a .tmp beside it, flushed, then moved over it with `files::replace`) after each batch that
      changed the box. A Store writes it before the item leaves the case, and is refused (the box put back) when it
      cannot. A write that fails is tried again a second later (`unwritten` meanwhile).
  - **Its moves at the pause** go into the saved state and the play flow's copy of it only (`saved_cases(index,
    story)`), never into the working block or its copy.
  - **The panel**: "Item box - Lost in Nightmares (n/54)", a line saying it is the episode's own and where it is
    kept, and an Add list without what the game keeps for a character not in the episode (`only`: Sheva's Longbow;
    in Desperate Escape Chris's Gatling gun too). An unreadable file is said so, with nothing of the box under it.
  - **Log**: "game: the game's mode cGameSys +0x58 (sBioMain +0x1042C; ...)"; "itembox: ... read - the box of ...
    holds N entries" (or "no ... yet", or "ERROR: ... could not be read"); "itembox: a game of Lost in Nightmares -
    the panel's box is the episode's own (...)"; "itembox: ... written ...".

## The game's calls (all from the tick: the pause menu up, or Item Management browsing)

- **`cItemManager::add`** (0xC4B480, thiscall, 18 arguments, `ret 0x48`; `kSigItemAdd` finds its tail): `(id, count,
  -1, -1, -1, 13, 0 x 9 levels, the character, [character+0x262C], 0)` - the arguments the case grid's own add
  passes (0xC65CEE, `kSigGridAdd` gives +0x262C); a weapon's count `-2` = its whole magazine. Returns the item (0:
  no room). It makes the item's model (no model handed over): only for what the area has loaded (above).
- **`cItem::clear`** (0xC4C4E0, thiscall, `(true)`: let the weapon's model go through sItem, then empty the item) -
  what the game calls when an item is used up or thrown away (22 callers). Never for the item in hand.
- **Item Management's rebuild of a pane's box list** (FUN_00A04500, thiscall `(screen, pane)`, `ret 4`) - what the
  screen runs after each store and take of its own - for each pane in use, after the mod's changes to the box, at
  Item Management while its panes are browsing (approved by the user on 2026-10-29).
- **Item Management's rebuild of a pane's treasures list** (FUN_00A048A0, thiscall `(screen, pane)`) - what the
  screen runs after a sale of its own, and with the box's when it sets up its lists - for each pane in use, after the
  mod's changes to the treasures, at Item Management while its panes are browsing (approved by the user on
  2026-10-30: "Yes, add the call"). See "The treasures".
- (Chapter Select at the pause calls nothing: it sets the pause menu's own fields, and the game's own steps make the
  take-back and the quit, as its QUIT does when its save question is answered no.)
- **Item Management's save of its cases** (FUN_00A079E0, thiscall `(screen)`: each pane's case put in order, then the
  working block into the saved state for each character in play) - what its own close and its quit make first - once,
  as Chapter Select leaves the screen with its quit (approved by the user on 2026-10-31: "let's attempt to call the
  game's save feature to save the inventory before landing back at chapter select"). See "Chapter Select from Item
  Management".
- **sLoadMan's request** (FUN_007D5820, thiscall `(7, item, 0, 0)`, `ret 10h`) **and commit** (FUN_007D4A10,
  thiscall, no arguments) - what an area's load asks of it - for an item put in a case at the pause whose model's
  archive the area has not loaded, before the inventory's add (approved by the user on 2026-10-29: "yes make it work
  for any item"). See "What an area has loaded".
