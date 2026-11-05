#!/usr/bin/env python3
"""List MT Framework's class records (MtDTI) in an unpacked re5dx9.exe image.

    python3 -I tools/mt_classes.py --dump ghidra_proj/re5dx9.dumped.exe [--grep uPlayer] [--tree NAME]
        [--vtable NAME]

The dump is the image as it is in memory (0x400000.., raw offsets == RVAs; see
tools/fix_dump.py). Each record: +4 name, +8 next sibling, +0xC child, +0x10
parent, +0x1C size (23 bits), +0x20 id. Every class answers its record from
vtable slot 4 (`mov eax, record; ret`), which gives its vtable (src/mt.h).
Default output: one line per class - record, vtable, size, parent chain.
--vtable NAME prints the class's vtable slots (and which class each function's
first ancestor with the same slot defines it in, for spotting overrides).
Pure stdlib.
"""
import argparse
import re
import struct

B = 0x400000


class Image:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        d = self.d
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        sect = pe + 24 + optsz
        self.secs = []
        for i in range(5):
            vs, va = struct.unpack_from("<II", d, sect + 40 * i + 8)
            self.secs.append((B + va, B + va + vs))
        self.text, self.rdata = self.secs[0], self.secs[1]
        self.data = (self.secs[2][0], max(self.secs[2][1], self.secs[3][1]))

    def u32(self, va):
        return struct.unpack_from("<I", self.d, va - B)[0]

    def cstr(self, va):
        o = va - B
        if not 0 <= o < len(self.d):
            return None
        e = self.d.find(b"\0", o, o + 256)
        return self.d[o:e].decode("latin-1") if e >= o else None

    def inr(self, r, va, n=4):
        return r[0] <= va and va + n <= r[1]


def records(img):
    s = img.d.find(b"\0MtObject\0") + 1 + B
    for m in re.finditer(re.escape(struct.pack("<I", s)), img.d):
        dti = B + m.start() - 4
        if not img.inr(img.data, dti, 0x24):
            continue
        parent = img.u32(dti + 0x10)
        if img.inr(img.data, parent, 0x24) and img.u32(parent + 4) == 0 and img.u32(parent + 0xC) == dti:
            root = parent
            break
    else:
        raise SystemExit("MtObject's record not found")
    out = {}
    stack = [img.u32(root + 0xC)]
    while stack:
        dti = stack.pop()
        if not dti or dti in out or not img.inr(img.data, dti, 0x24):
            continue
        name = img.cstr(img.u32(dti + 4)) or ""
        out[dti] = dict(name=name, size=img.u32(dti + 0x1C) & 0x7FFFFF, parent=img.u32(dti + 0x10),
                        flags=img.u32(dti + 0x1C) >> 23)
        stack.append(img.u32(dti + 8))
        stack.append(img.u32(dti + 0xC))
    return root, out


def getdti_index(img):
    """record -> getDTI function, from every `mov eax, imm32; ret` in the code."""
    t0, t1 = img.text
    seg = img.d[t0 - B:t1 - B]
    out = {}
    for m in re.finditer(rb"\xb8(....)\xc3", seg, re.S):
        out.setdefault(struct.unpack("<I", m.group(1))[0], t0 + m.start())
    return out


def vtables_of(img, fn):
    r0, r1 = img.rdata
    seg = img.d[r0 - B:r1 - B]
    return [r0 + m.start() - 16 for m in re.finditer(re.escape(struct.pack("<I", fn)), seg) if m.start() % 4 == 0]


def chain(recs, dti):
    names = []
    while dti in recs:
        names.append(recs[dti]["name"])
        dti = recs[dti]["parent"]
    return names


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", required=True)
    ap.add_argument("--grep")
    ap.add_argument("--tree")
    ap.add_argument("--vtable")
    a = ap.parse_args()
    img = Image(a.dump)
    root, recs = records(img)
    byname = {}
    for dti, r in recs.items():
        byname.setdefault(r["name"], dti)
    gd = getdti_index(img)

    def vt(dti):
        fn = gd.get(dti)
        v = vtables_of(img, fn) if fn else []
        return v[0] if v else 0

    if a.vtable:
        dti = byname[a.vtable]
        v = vt(dti)
        print(f"{a.vtable}: record {dti:#x}, vtable {v:#x}, size {recs[dti]['size']:#x}, chain {' < '.join(chain(recs, dti))}")
        par = recs[dti]["parent"]
        pv = vt(par) if par in recs else 0
        i = 0
        while img.inr(img.text, img.u32(v + 4 * i), 1):
            f = img.u32(v + 4 * i)
            pf = img.u32(pv + 4 * i) if pv and img.inr(img.text, img.u32(pv + 4 * i), 1) else None
            print(f"  [{i:3d}] +{4*i:#05x} {f:#x}{'' if pf == f else '  (own)' if pf is not None else '  (new slot)'}")
            i += 1
        return
    if a.tree:
        dti = byname[a.tree]

        def walk(d, depth):
            r = recs[d]
            print(f"{'  ' * depth}{r['name']}  size {r['size']:#x}  record {d:#x}")
            c = img.u32(d + 0xC)
            while c in recs:
                walk(c, depth + 1)
                c = img.u32(c + 8)
        walk(dti, 0)
        return
    print(f"# root {root:#x}, {len(recs)} records")
    for dti, r in sorted(recs.items(), key=lambda kv: kv[1]["name"]):
        if a.grep and not re.search(a.grep, r["name"]):
            continue
        print(f"{r['name']:40s} record {dti:#x} vtable {vt(dti):#x} size {r['size']:#7x}  {' < '.join(chain(recs, dti)[1:4])}")


if __name__ == "__main__":
    main()
