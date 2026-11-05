#!/usr/bin/env python3
"""What a Resident Evil 5 save holds - read out of savedata.bin, never written.

    python3 -I tools/save.py --dump ghidra_proj/re5dx9.dumped.exe --asm ghidra_proj/re5dx9.asm \
        [--save PATH] [--missing]

The profile's gold and exchange points, the shop's unlocks, the item box, the
treasures and both characters' cases as the last save left them. --save defaults to
the first ~/.local/share/Steam/userdata/*/21690/remote/savedata.bin. --missing also
lists what the Add lists offer (src/items.h's `offer`) and the game's treasures that
the save holds none of.

The file is the save job's buffer (the save data's +0x110 on), each byte XORed with
the encrypted byte 8 before it - the first 8 with a key of the game's (its decrypt:
`mov ecx,[key]; mov edx,[key+4]; mov [esp+n],ecx; mov [esp+n+4],edx`). Decrypted, the
save's writer (`push ebx; mov ebx,ecx; mov [ebx+magic],..; mov [ebx+size],..`) gives
the rest: the profile's checkpoint copy (cSystemInfo from +4) in two parts and
cGameInfo's tail between them, each part's place, size and source; its checksum (the
sum of the u32s of two spans, as the game adds them) is checked. The box and the
treasures are where the mod's own signatures find them (src/sigs.h: kSigBoxFree,
kSigTreasureCount), the gold and the shop's bits where cSystemInfo's createProperty
names them (tools/mt_props.py), the shop's bits are its price list's entries
(tools/gen_items.py's PRICES), and a saved case is 24 entries of 0x2C bytes whose
entry k is slot k, entry 11 hand-to-hand - as the game's save of a case writes them.
Pure stdlib.
"""
import argparse
import glob
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_items  # noqa: E402  (PRICES, TREASURES)
import mt_classes  # noqa: E402
import mt_props  # noqa: E402

B = 0x400000
KEY = re.compile(rb"\x8b\x0d(....)\x8b\x15(....)\x89\x4c\x24.\x89\x54\x24.", re.S)
WRITER = re.compile(rb"\x53\x8b\xd9\xc7\x83(....)(....)\xc7\x83(....)(....)", re.S)
SYS_PART = re.compile(rb"\x69\xc9(....)\x8d\xb4\x01(....)\x68(....)\x8d\x93(....)\x56\x52\xe8", re.S)
SYS_REST = re.compile(rb"\x81\xc6(....)\x8d\xbb(....)\xb9(....)\xf3\xa5", re.S)
GAME_PART = re.compile(rb"\x69\xc9(....)\x8d\x94\x01(....)\x68(....)\x52\x8d\x83(....)\x50\xe8", re.S)
SYS_BASE = re.compile(rb"\x69\xc9(....)\x8d\x4c\x01(.)\xe8", re.S)
GAME_BASE = re.compile(rb"\x69\xc0(....)\x8d\x8c\x38(....)\xe8", re.S)
CHECKSUM = re.compile(rb"\x56\x33\xc0\x8d\x91(....)\xbe(....)\x57\x90\x8b\x7a\x10.{22}\x81\xc1(....)\xba(....)"
                      rb".{20,40}?\x56\x8b\xf1\xe8....\x3b\x86(....)", re.S)
CHARACTERS = ("Chris", "Sheva")


def die(msg):
    raise SystemExit(f"save.py: {msg}")


def one(rx, data, what, start=0, end=None):
    hits = list(rx.finditer(data, start, end if end is not None else len(data)))
    if len(hits) != 1:
        die(f"{what}: {len(hits)} matches (want 1)")
    return hits[0]


def u32s(m, *groups):
    return [struct.unpack("<I", m.group(g))[0] for g in groups]


def sig(sigs_text, name):
    """A signature of src/sigs.h as a regex (sig.py's reading of the file)."""
    src = sigs_text.replace("\\\n", " ")
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    src = re.sub(r'"\s+"', "", src)
    m = re.search(r'SIG\(\s*' + name + r'\s*,\s*"([^"]+)"', src)
    if not m:
        die(f"{name}: not in src/sigs.h")
    out = b""
    for tok in m.group(1).split():
        out += b"." if tok.startswith("?") else re.escape(bytes([int(tok, 16)]))
    return re.compile(out, re.S)


def items_h(path):
    names, offer = {}, set()
    for m in re.finditer(r'\{0x([0-9A-F]+), \d, \d, (?:true|false), (true|false), -?\d+, -?\d+, '
                         r'(?:nullptr|"[^"]*"), "(.*)"\}', open(path).read()):
        names[int(m.group(1), 16)] = m.group(3)
        if m.group(2) == "true":
            offer.add(int(m.group(1), 16))
    if not names:
        die(f"{path}: no items (regenerate it with tools/gen_items.py)")
    return names, offer


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", required=True)
    ap.add_argument("--asm", required=True)
    ap.add_argument("--save")
    ap.add_argument("--missing", action="store_true")
    a = ap.parse_args()
    root = os.path.dirname(HERE)
    img = open(a.dump, "rb").read()

    def u32(va):
        return struct.unpack_from("<I", img, va - B)[0]

    def u16(va):
        return struct.unpack_from("<H", img, va - B)[0]

    path = a.save
    if not path:
        found = sorted(glob.glob(os.path.expanduser("~/.local/share/Steam/userdata/*/21690/remote/savedata.bin")))
        if not found:
            die("no savedata.bin found - give --save")
        path = found[0]
    enc = open(path, "rb").read()

    # The key (the game's encrypt and decrypt load it alike).
    keys = {tuple(u32s(m, 1, 2)) for m in KEY.finditer(img)}
    keys = {k for k in keys if k[1] == k[0] + 4}
    if len(keys) != 1:
        die(f"the save's key: {len(keys)} candidates (want 1)")
    k = bytearray(img[next(iter(keys))[0] - B:][:8])
    p = bytearray(len(enc))
    for i, b in enumerate(enc):
        p[i] = b ^ k[i % 8]
        k[i % 8] = b

    # The writer: the file is its buffer from the magic's field on.
    w = one(WRITER, img, "the save's writer")
    start, magic, size_field, size = u32s(w, 1, 2, 3, 4)
    end = w.start() + 0x200
    sb = one(SYS_BASE, img, "the writer's profile", w.start(), end)
    sys_stride, sys_base = struct.unpack("<I", sb.group(1))[0], sb.group(2)[0]
    gb = one(GAME_BASE, img, "the writer's cGameInfo", w.start(), end)
    game_stride, game_base = u32s(gb, 1, 2)
    s1 = one(SYS_PART, img, "the writer's first part", w.start(), end)
    stride1, src1, size1, dst1 = u32s(s1, 1, 2, 3, 4)
    s2 = one(SYS_REST, img, "the writer's second part", w.start(), end)
    add2, dst2, dwords2 = u32s(s2, 1, 2, 3)
    g3 = one(GAME_PART, img, "the writer's cGameInfo part", w.start(), end)
    stride3, src3, size3, dst3 = u32s(g3, 1, 2, 3, 4)
    copy = src1 - sys_base  # cSystemInfo's checkpoint copy: the live object from +4
    if stride1 != sys_stride or stride3 != game_stride or add2 != size1 or size1 + 4 * dwords2 != copy - 4:
        die("the save's writer: its parts disagree")
    game_from = src3 - game_base  # the cGameInfo offset its part begins with
    cs = one(CHECKSUM, img, "the save's checksum")
    c_off1, c_n1, c_off2, c_n2, c_field = u32s(cs, 1, 2, 3, 4, 5)

    def at(buf_off):  # a buffer offset in the file
        return buf_off - start

    def word(o):
        return struct.unpack_from("<I", p, o)[0]

    if len(p) < at(dst2) + 4 * dwords2 or word(0) != magic:
        die(f"{path}: not a save of this build (magic {word(0):#x}, want {magic:#x})")
    total = sum(word(at(c_off1) + 4 * i) for i in range(5 * c_n1)) + \
        sum(word(at(c_off2) + 4 * i) for i in range(4 * c_n2))
    if total & 0xFFFFFFFF != word(at(c_field)):
        die(f"{path}: the checksum does not match (the decryption is off)")

    def sysinfo(f):  # a cSystemInfo field's place in the file
        o = f - 4
        return at(dst1) + o if o < size1 else at(dst2) + o - size1

    def gameinfo(f):
        if not game_from <= f < game_from + size3:
            die(f"cGameInfo +{f:#x} is not in the save")
        return at(dst3) + f - game_from

    # cSystemInfo's named fields.
    insns, order = mt_props.load_asm(a.asm)
    order_index = {addr: n for n, addr in enumerate(order)}
    mimg = mt_classes.Image(a.dump)
    _, recs = mt_classes.records(mimg)
    gd = mt_classes.getdti_index(mimg)
    dti = next((d for d, r in recs.items() if r["name"] == "cSystemInfo"), None)
    vt = mt_classes.vtables_of(mimg, gd.get(dti)) if dti else []
    if not vt:
        die("cSystemInfo's vtable not found")
    props, _ = mt_props.walk(insns, order_index, order, mimg.u32(vt[0] + 12), 0x422C00)
    field = {mimg.cstr(nva): off for nva, _t, _f, off in props if nva and isinstance(off, int)}
    for want in ("mTmpData.mHistory.mMoney", "mTmpData.mHistory.mPoint", "mTmpData.mHistory.mShopBuyItem[0]",
                 "mTmpData.mHistory.mShopBuyItem[1]"):
        if want not in field:
            die(f"cSystemInfo's {want}: not found")

    # The box and the treasures, as the mod's signatures find them.
    sigs_text = open(os.path.join(root, "src", "sigs.h")).read()
    bf = one(sig(sigs_text, "kSigBoxFree"), img, "kSigBoxFree")
    box_off, box_step, box_n = struct.unpack_from("<I", img, bf.start() + 2)[0], img[bf.start() + 19], \
        img[bf.start() + 22]
    tc = one(sig(sigs_text, "kSigTreasureCount"), img, "kSigTreasureCount")
    t_off = struct.unpack_from("<I", img, tc.start() + 23)[0]
    if struct.unpack_from("<I", img, tc.start() + 15)[0] != t_off + 2:
        die("kSigTreasureCount: its list and its counts disagree")
    names, offer = items_h(os.path.join(root, "src", "items.h"))

    def name(i):
        return names.get(i, f"item {i:#x}")

    # The shop's price list (its unlock bits' order) and the game's treasures.
    m = one(gen_items.PRICES, img, "the shop's price list")
    pids, psize = u32s(m, 1, 2)
    shop = [u16(pids + o) for o in range(0, psize, 12)]
    m = one(gen_items.TREASURES, img, "the treasures' table")
    tcount, ttable = u32s(m, 1, 2)
    treasures = [u16(ttable + 2 * i) for i in range(u32(tcount))]

    print(f"{path}: decrypted, magic {magic:#x} and checksum {total & 0xFFFFFFFF:#x} as the game writes them")
    print(f"gold {word(sysinfo(field['mTmpData.mHistory.mMoney']))}, "
          f"exchange points {word(sysinfo(field['mTmpData.mHistory.mPoint']))}")
    bits = word(sysinfo(field["mTmpData.mHistory.mShopBuyItem[0]"])) | \
        word(sysinfo(field["mTmpData.mHistory.mShopBuyItem[1]"])) << 32
    unlocked = [i for n, i in enumerate(shop) if bits >> n & 1]
    print(f"the shop's unlocked entries ({len(unlocked)}): " + ", ".join(name(i) for i in unlocked))
    held = set()
    print(f"the item box ({box_n} entries):")
    for e in range(box_n):
        o = sysinfo(box_off + box_step * e)
        i, n = struct.unpack_from("<HH", p, o)
        if i:
            held.add(i)
            lv = p[o + 4:o + box_step]
            print(f"  {e:2d}  {i:03X}  {name(i):26s} x{n}" + (f"  levels {lv.hex(' ')}" if any(lv) else ""))
    have = {}
    for e in range(54):
        i, n = struct.unpack_from("<HH", p, sysinfo(t_off + 4 * e))
        if i:
            have[i] = have.get(i, 0) + n
    print(f"the treasures ({len(have)} kinds, {sum(1 for t in treasures if t in have)} of the game's "
          f"{len(treasures)}): " + ", ".join(f"{name(i)} x{n}" for i, n in have.items()))
    held |= set(have)
    # The saved cases: the first block of 24 entries shaped as the game's save of a case.
    lo, hi = at(dst3), at(dst3) + size3
    ent, per = 0x2C, 24

    def is_case(o):
        return o + per * ent <= hi and word(o + 11 * ent) == 0x100 and \
            all(word(o + n * ent + 0x14) == n for n in range(12))

    first = next((o for o in range(lo, hi - per * ent, 4) if is_case(o)), None)
    if first is None or not is_case(first + per * ent):
        print("the cases: not found (no block of entries shaped as a saved case)")
    else:
        print(f"the cases (cGameInfo +{first - at(dst3) + game_from:#x}, {per * ent:#x} a character):")
        for c in range(2):
            o0 = first + c * per * ent
            row = []
            for n in range(12):
                i, num = word(o0 + n * ent), word(o0 + n * ent + 4)
                if i and i != 0x100:
                    held.add(i)
                    hand = " (in hand)" if word(o0 + n * ent + 0x18) == 1 else ""
                    row.append(f"slot {n}: {i:03X} {name(i)} x{num}{hand}")
            print(f"  {CHARACTERS[c]}: " + ("; ".join(row) or "empty"))
    if a.missing:
        held |= set(unlocked)
        print("offered by the Add lists, none in this save: " +
              (", ".join(name(i) for i in sorted(offer) if i not in held) or "none"))
        print("the game's treasures not held: " +
              (", ".join(name(i) for i in treasures if i not in have) or "none"))


if __name__ == "__main__":
    main()
