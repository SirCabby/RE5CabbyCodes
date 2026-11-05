#!/usr/bin/env python3
"""Generate src/items.h - every item the panel lists, from the game's own data.

    python3 -I tools/gen_items.py --dump ghidra_proj/re5dx9.dumped.exe \
        --arc "$GAME_DIR/nativePC_MT/Image/Archive/Msg2Resource_e.arc" --out src/items.h

The item table: the game's item-info lookup (`mov eax,[esp+4]; cmp eax,0FFFFh; ...;
mov eax,[ecx*4+table]; lea eax,[eax+edx*8]; ret` - `id >> 8` picks a category's
array, `(id & 0xFF) * 0x48` the record) is found by its code in the unpacked image
(re5dx9.exe is Enigma-wrapped; see tools/fix_dump.py for the dump). Categories 1-6
are items: 1 weapons, 2 ammo, 3 recovery, 4 treasures, 5 key items, 6 armor and
the like. A record (0x48 bytes): +8 the count an item comes with, +0xC its most,
+0x24 the weapon's kind (1 firearm, 2 knife/melee, 3 thrown, 7 lantern), +0x44
2 for a magazine-fed firearm, +0x40 the line of its name in mes_item.

The names are the game's English text: etc\\message\\mes_item_e in the message
archive (an MT Framework ARC, zlib), an MSG2 file - each character a pair of u16
{glyph, width}, {0, 1025} ending a line, widths over 144 control codes. The
glyphs of the English font run in pairs: digits from 0 (2 each), then ( ) space &
. ' - +, the capitals from 56, the small letters from 108, _ at 282 (worked out
from the names themselves; a glyph this does not know stops the tool).

Left out: records whose name is "dummy..." or "(dummy...)" or empty, "none"/"Nothing", category 1
past the player's weapons (the enemies' weapons, from 0x150), and anything with no
name line.

What the Add lists offer (`offer`): what the story gives Chris and Sheva to carry -
the items of the shop's price list (below: its weapons, rounds, grenades, eggs, ammo,
recovery items and vests) and every weapon the "every kind of weapon" achievement
counts (`mov edx,[count]; test edx,edx; mov eax,table; jle; movzx ecx,word [eax+2]`,
4 bytes an entry: the kind, then the id), which adds the launcher's other loads. Not
offered, though named: the knives (each character's own, never added or moved), the
Mercenaries' guns, the scene-only weapons and their rounds, the floor herbs (a pickup
makes them the shop's herbs), the launcher's in-hand id, and the leftovers.

Who may have it (`only`): Item Management's own check of an item a pane takes from
the box (`movzx ecx,word [edx]; cmp cx,a; jne; cmp eax,n; je; cmp cx,b; jne; test
eax,eax; je`): `a` refused for character n, `b` for character 0 - so each is the
other character's alone (0 Chris, 1 Sheva: the pane's character).

The treasures: the game's own table of them - the ids its treasure achievement
counts (`mov esi,[count]; lea ebx,[...]; ... cmp di,[eax*2+table]`), the treasures
Item Management lists in its own order - each with what the shop pays for one: the
shop's price list (`movzx esi,word [eax+ids]; cmp edx,esi; ... cmp eax,size; ...
mov eax,[ecx*4+prices]`, 12 bytes an item), of which Item Management pays a tenth.
Pure stdlib.
"""
import argparse
import re
import struct
import sys
import zlib

B = 0x400000
LOOKUP = re.compile(rb"\x8b\x44\x24\x04\x3d\xff\xff\x00\x00\x75\x03\x33\xc0\xc3\x0f\xb6\xcc\x25\xff\x00\x00\x00"
                    rb"\x8d\x14\xc0\x8b\x04\x8d(....)\x8d\x04\xd0\xc3", re.S)
TREASURES = re.compile(rb"\x8b\x35(....)\x8d\x9c\x28....\x33\xc0\x57\x89\x6c\x24\x14\x89\x44\x24\x18\x89\x44\x24"
                       rb"\x1c\xc7\x44\x24\x10\x36\x00\x00\x00\x0f\xb7\x3b\x66\x85\xff\x74.\x33\xc0\x85\xf6\x7e."
                       rb"\x66\x3b\x3c\x45(....)", re.S)
PRICES = re.compile(rb"\x0f\xb7\xb0(....)\x3b\xd6\x74.\x83\xc0\x0c\x83\xc1\x01\x3d(....)\x72.\x33\xc0\x5e\xc3"
                    rb"\x8d\x0c\x49\x8b\x04\x8d(....)\x5e\xc3", re.S)
WEAPON_KINDS = re.compile(rb"\x8b\x15(....)\x85\xd2\xb8(....)\x7e.\x0f\xb7\x48\x02", re.S)
ONLY = re.compile(rb"\x0f\xb7\x0a\x66\x81\xf9(..)\x75\x05\x83\xf8(.)\x74.\x66\x81\xf9(..)\x75\x04\x85\xc0\x74.", re.S)
CHARACTERS = {0: "Chris", 1: "Sheva"}  # the game's character numbers (a pane's, the saved cases')
PUNCT = {24: "(", 26: ")", 28: " ", 30: "&", 38: ".", 42: "'", 46: "-", 48: "+", 282: "_"}
KINDS = {1: "weapon", 2: "ammo", 3: "recovery", 4: "treasure", 5: "key", 6: "other"}


def msg_lines(arc_path, name):
    d = open(arc_path, "rb").read()
    magic, ver, n = struct.unpack_from("<4sHH", d, 0)
    if magic != b"ARC\0":
        raise SystemExit(f"{arc_path}: not an MT Framework archive")
    for i in range(n):
        o = 8 + i * 0x50
        entry = d[o:o + 64].split(b"\0")[0].decode("latin-1")
        _, csz, _, off = struct.unpack_from("<IIII", d, o + 64)
        if entry.endswith(name):
            raw = zlib.decompress(d[off:off + csz])
            break
    else:
        raise SystemExit(f"{arc_path}: no {name}")
    if raw[:4] != b"MSG2":
        raise SystemExit(f"{name}: not an MSG2 file")
    lines, cur = [], []
    for o in range(0x40, len(raw) - 3, 4):
        g, w = struct.unpack_from("<HH", raw, o)
        if g == 0 and w == 1025:
            lines.append(cur)
            cur = []
        else:
            cur.append((g, w))
    return lines


def decode(line):
    s = ""
    for g, w in line:
        if w > 144:
            continue  # a control code (colour, an end mark)
        if g < 20 and g % 2 == 0:
            s += chr(ord("0") + g // 2)
        elif g in PUNCT:
            s += PUNCT[g]
        elif 56 <= g <= 106 and g % 2 == 0:
            s += chr(ord("A") + (g - 56) // 2)
        elif 108 <= g <= 158 and g % 2 == 0:
            s += chr(ord("a") + (g - 108) // 2)
        else:
            raise ValueError(f"glyph {g} (width {w}) unknown")
    return re.sub(r"\s+", " ", s).strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", required=True)
    ap.add_argument("--arc", required=True)
    ap.add_argument("--out", default="src/items.h")
    a = ap.parse_args()
    img = open(a.dump, "rb").read()

    def u32(va):
        return struct.unpack_from("<I", img, va - B)[0]

    hits = list(LOOKUP.finditer(img))
    if len(hits) != 1:
        raise SystemExit(f"the item-info lookup: {len(hits)} matches (want 1)")
    table = struct.unpack("<I", hits[0].group(1))[0]
    # Categories 0-6, and the pointer after them (it bounds category 6).
    cats = [u32(table + 4 * c) for c in range(8)]
    record_vt = u32(cats[1])  # every item record's first word
    lines = msg_lines(a.arc, "mes_item_e")
    items = []
    for c in range(1, 7):
        base = cats[c]
        ends = sorted(p for p in cats if p > base)
        n = (ends[0] - base) // 0x48 if ends else 64
        for k in range(n):
            if c == 1 and k >= 0x50:
                break  # the enemies' weapons
            e = base + 0x48 * k
            f = struct.unpack_from("<18I", img, e - B)
            if f[0] != record_vt:
                break
            name_line = f[16]
            if name_line >= len(lines):
                continue
            try:
                name = decode(lines[name_line])
            except ValueError as err:
                raise SystemExit(f"item {(c << 8) | k:#x}: {err}")
            low = name.lower()
            if not name or low.strip("()").startswith("dummy") or low in ("none", "nothing"):
                continue
            num, most = f[2], f[3]
            kind = f[9]  # +0x24
            magazine = f[17] == 2  # +0x44
            items.append(((c << 8) | k, c, name, num, most, kind, magazine))

    def u16(va):
        return struct.unpack_from("<H", img, va - B)[0]

    hits = list(TREASURES.finditer(img))
    if len(hits) != 1:
        raise SystemExit(f"the treasures' table: {len(hits)} matches (want 1)")
    tcount, ttable = (struct.unpack("<I", hits[0].group(g))[0] for g in (1, 2))
    hits = list(PRICES.finditer(img))
    if len(hits) != 1:
        raise SystemExit(f"the shop's price list: {len(hits)} matches (want 1)")
    pids, psize, pvalues = (struct.unpack("<I", hits[0].group(g))[0] for g in (1, 2, 3))
    if pvalues != pids + 4 or psize % 12:
        raise SystemExit("the shop's price list: its ids and prices disagree")
    prices = {u16(pids + o): u32(pvalues + o) for o in range(0, psize, 12)}
    kinds = {it[0]: it[1] for it in items}
    treasures = []
    for i in range(u32(tcount)):
        tid = u16(ttable + 2 * i)
        if kinds.get(tid) != 4 or tid not in prices:
            raise SystemExit(f"treasure {tid:#x}: not a treasure of the item table, or no price")
        treasures.append((tid, prices[tid] // 10))
    hits = list(WEAPON_KINDS.finditer(img))
    if len(hits) != 1:
        raise SystemExit(f"the weapons achievement's table: {len(hits)} matches (want 1)")
    wcount, wtable = (struct.unpack("<I", hits[0].group(g))[0] for g in (1, 2))
    weapons = {u16(wtable + 4 * i + 2) for i in range(u32(wcount))}
    if not weapons or any(kinds.get(w) != 1 for w in weapons):
        raise SystemExit("the weapons achievement's table: not the item table's weapons")
    hits = list(ONLY.finditer(img))
    if len(hits) != 1:
        raise SystemExit(f"Item Management's check of who takes an item: {len(hits)} matches (want 1)")
    refused = {struct.unpack("<H", hits[0].group(1))[0]: hits[0].group(2)[0],
               struct.unpack("<H", hits[0].group(3))[0]: 0}
    if len(refused) != 2 or any(n not in CHARACTERS or kinds.get(i) != 1 for i, n in refused.items()):
        raise SystemExit("Item Management's check of who takes an item: not two weapons, one for each character")
    only = {i: CHARACTERS[1 - n] for i, n in refused.items()}
    offered = {it[0] for it in items if it[1] in (1, 2, 3, 6) and (it[0] in prices or it[0] in weapons)}
    if any(i not in offered for i in only):
        raise SystemExit("a weapon the game keeps for one character is not offered")
    with open(a.out, "w") as out:
        out.write("#pragma once\n\n// Generated by tools/gen_items.py from the game's item table and its English\n"
                  "// mes_item text - do not edit by hand.\n\n#include <cstdint>\n\nnamespace re5cc::items {\n\n")
        out.write("// cat: 1 weapon, 2 ammo, 3 recovery, 4 treasure, 5 key item, 6 armor and the like.\n"
                  "// kind (weapons): 1 firearm, 2 knife or melee, 3 thrown, 7 lantern. magazine: a\n"
                  "// magazine-fed firearm (its count is the rounds loaded).\n"
                  "// offer: an Add list offers it - what the story gives Chris and Sheva to carry: the\n"
                  "// shop's price list's items and the weapons its \"every kind of weapon\" achievement\n"
                  "// counts (the launcher's loads). The rest are named for what the game holds: each\n"
                  "// character's knife, the Mercenaries' guns, the scene-only weapons, the floor herbs.\n"
                  "// only: the one character Item Management lets take it (its own check), or nullptr.\n")
        out.write("struct Info {\n  uint16_t id;\n  uint8_t cat;\n  uint8_t kind;\n  bool magazine;\n"
                  "  bool offer;\n  int32_t num;   // the count it comes with\n  int32_t most;  // the record's most "
                  "(0: none)\n  const char* only;\n  const char* name;\n};\n\n")
        out.write(f"constexpr Info kItems[] = {{\n")
        for idv, c, name, num, most, kind, mag in items:
            esc = name.replace("\\", "\\\\").replace('"', '\\"')
            who = f'"{only[idv]}"' if idv in only else "nullptr"
            out.write(f'    {{0x{idv:03X}, {c}, {kind if c == 1 else 0}, {"true" if mag else "false"}, '
                      f'{"true" if idv in offered else "false"}, {num}, {most}, {who}, "{esc}"}},\n')
        out.write("};\n")
        out.write(f"constexpr int kCount = {len(items)};\n\n")
        out.write("inline const Info* find(uint32_t id) {\n  for (const Info& i : kItems)\n"
                  "    if (i.id == id) return &i;\n  return nullptr;\n}\n\n")
        out.write("// The treasures, as the game's own table of them has them (Item Management lists\n"
                  "// them, and its treasure achievement counts them), with the gold the shop pays\n"
                  "// for one.\n")
        out.write("struct Treasure {\n  uint16_t id;\n  int32_t price;\n};\n\n")
        out.write("constexpr Treasure kTreasures[] = {\n")
        for tid, price in treasures:
            out.write(f"    {{0x{tid:03X}, {price}}},\n")
        out.write("};\n")
        out.write(f"constexpr int kTreasureCount = {len(treasures)};\n\n")
        out.write("inline const Treasure* treasure(uint32_t id) {\n  for (const Treasure& t : kTreasures)\n"
                  "    if (t.id == id) return &t;\n  return nullptr;\n}\n\n}  // namespace re5cc::items\n")
    per = {}
    for it in items:
        per[it[1]] = per.get(it[1], 0) + 1
    print(f"wrote {a.out}: {len(items)} items - " + ", ".join(f"{KINDS[c]} {n}" for c, n in sorted(per.items())) +
          f"; {len(offered)} offered in the Add lists (" +
          ", ".join(f"{name} only {who}" for idv, who in sorted(only.items())
                    for (i, _, name, *_r) in items if i == idv) +
          f"); {len(treasures)} of the treasures in the game's own table")


if __name__ == "__main__":
    main()
