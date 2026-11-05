#!/usr/bin/env python3
"""Turn an unpacked re5dx9.exe image into a PE that Ghidra and objdump can read.

    python3 -I tools/fix_dump.py --dump re5dx9.dumped.exe --imports re5dx9.imports.txt \
        [--out re5dx9.fixed.exe] [--entry 0xVA]

re5dx9.exe is wrapped by Enigma Protector (and SteamStub over that): on disk its
code is compressed, and in memory the wrapper rebuilds the import address table
with the real addresses and no names. The dump is the image as it is in memory
once the game runs (0x400000 .. +SizeOfImage, raw offsets == RVAs) - written by
the mod (DumpImage = 1 in its ini) or read out of a headless run's /proc/<pid>/mem.
The imports map names each slot: 'slotVA value dll!name' lines (the mod writes it
beside the dump; the headless way resolves the values against the export tables of
the DLLs the process had mapped).

The output keeps the game's own five sections (.text, .rdata, .data, the fourth,
.rsrc) and drops the wrappers' (Enigma's runtime, its loader, SteamStub's .bind).
A sixth section holds an import directory made from the map, so every call through
the IAT reads as the function it reaches, with its signature. The entry point is
the game's (--entry), or the start of .text. Pure stdlib.
"""
import argparse
import struct

IMAGE_BASE = 0x400000
GAME_SECTIONS = 5  # .text, .rdata, .data, the 4th, .rsrc - Enigma's own follow
NAMES = [b".text", b".rdata", b".data", b".data2", b".rsrc"]
CHARS = [0x60000020, 0x40000040, 0xC0000040, 0xC0000040, 0x40000040]


def align(v, a):
    return (v + a - 1) & ~(a - 1)


def read_map(path):
    slots = []
    for line in open(path):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        p = line.split()
        va = int(p[0], 16)
        name = p[2] if len(p) > 2 else "?"
        slots.append((va, name))
    slots.sort()
    return slots


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", required=True)
    ap.add_argument("--imports", required=True)
    ap.add_argument("--out", default="re5dx9.fixed.exe")
    ap.add_argument("--entry", type=lambda s: int(s, 0), default=0)
    a = ap.parse_args()

    d = bytearray(open(a.dump, "rb").read())
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    opt = pe + 24
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    sect = opt + optsz
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    secs = []
    for i in range(nsec):
        vs, va = struct.unpack_from("<II", d, sect + 40 * i + 8)
        secs.append((va, vs))
    if nsec < GAME_SECTIONS:
        raise SystemExit(f"expected at least {GAME_SECTIONS} sections, found {nsec}")
    game_end = align(secs[GAME_SECTIONS - 1][0] + secs[GAME_SECTIONS - 1][1], 0x1000)

    # The import directory: one descriptor per run of slots of one DLL.
    slots = read_map(a.imports)
    runs = []
    for va, name in slots:
        dll, _, fn = name.partition("!")
        if not fn:
            dll, fn = "unknown.dll", f"unknown_{va:08x}"
        if runs and runs[-1][0] == dll and va == runs[-1][1][-1][0] + 4:
            runs[-1][1].append((va, fn))
        else:
            runs.append((dll, [(va, fn)]))
    idata_rva = game_end
    blob = bytearray()
    desc_size = 20 * (len(runs) + 1)
    blob += b"\0" * desc_size
    strings = bytearray()
    ilts = bytearray()
    ilt_base = desc_size
    total_slots = sum(len(r[1]) + 1 for r in runs)
    str_base = ilt_base + 4 * total_slots
    fix_slots = []
    for i, (dll, entries) in enumerate(runs):
        ilt_rva = idata_rva + ilt_base + len(ilts)
        for va, fn in entries:
            hn_rva = idata_rva + str_base + len(strings)
            strings += struct.pack("<H", 0) + fn.encode() + b"\0"
            if len(strings) % 2:
                strings += b"\0"
            ilts += struct.pack("<I", hn_rva)
            fix_slots.append((va - IMAGE_BASE, hn_rva))
        ilts += struct.pack("<I", 0)
        name_rva = idata_rva + str_base + len(strings)
        strings += dll.encode() + b"\0"
        if len(strings) % 2:
            strings += b"\0"
        first_thunk = entries[0][0] - IMAGE_BASE
        struct.pack_into("<IIIII", blob, 20 * i, ilt_rva, 0, 0, name_rva, first_thunk)
    blob += ilts + strings
    idata_vsize = len(blob)
    idata_raw = align(idata_vsize, 0x1000)

    out = bytearray(d[:game_end])
    out += blob + b"\0" * (idata_raw - idata_vsize)
    for rva, value in fix_slots:  # unbound: each slot names its import again
        struct.pack_into("<I", out, rva, value)

    # Headers: six sections, raw offsets == RVAs, every data directory cleared but
    # the imports, the IAT and the resources.
    struct.pack_into("<H", out, pe + 6, GAME_SECTIONS + 1)
    struct.pack_into("<I", out, opt + 36, 0x1000)  # FileAlignment
    struct.pack_into("<I", out, opt + 56, idata_rva + idata_raw)  # SizeOfImage
    struct.pack_into("<I", out, opt + 16, (a.entry - IMAGE_BASE) if a.entry else 0x1000)
    struct.pack_into("<I", out, opt + 64, 0)  # CheckSum
    for i in range(16):
        struct.pack_into("<II", out, opt + 96 + 8 * i, 0, 0)
    struct.pack_into("<II", out, opt + 96 + 8 * 1, idata_rva, desc_size)
    iat_lo = min(va for va, _ in slots) - IMAGE_BASE
    iat_hi = max(va for va, _ in slots) - IMAGE_BASE + 4
    struct.pack_into("<II", out, opt + 96 + 8 * 12, iat_lo, iat_hi - iat_lo)
    rsrc_va, rsrc_vs = secs[4]
    struct.pack_into("<II", out, opt + 96 + 8 * 2, rsrc_va, rsrc_vs)
    for i in range(GAME_SECTIONS):
        va, vs = secs[i]
        o = sect + 40 * i
        out[o:o + 8] = NAMES[i].ljust(8, b"\0")
        struct.pack_into("<IIII", out, o + 8, vs, va, align(vs, 0x1000), va)
        struct.pack_into("<I", out, o + 36, CHARS[i])
    o = sect + 40 * GAME_SECTIONS
    out[o:o + 40] = b".idata\0\0" + struct.pack("<IIIIIIHHI", idata_vsize, idata_rva, idata_raw, idata_rva, 0, 0, 0,
                                                    0, 0xC0000040)
    # The section table must still fit in the headers (it does: 8 entries were there).
    open(a.out, "wb").write(out)
    named = sum(1 for _, n in slots if "!" in n)
    print(f"wrote {a.out}: {len(runs)} import runs, {len(slots)} slots ({named} named), "
          f"entry {IMAGE_BASE + struct.unpack_from('<I', out, opt + 16)[0]:#x}")
    for va, vs in secs[:GAME_SECTIONS]:
        print(f"  {IMAGE_BASE + va:#x} +{vs:#x}")


if __name__ == "__main__":
    main()
