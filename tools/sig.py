#!/usr/bin/env python3
"""Look for byte patterns in an unpacked re5dx9.exe image (the dump, raw offsets == RVAs).

    python3 -I tools/sig.py --dump ghidra_proj/re5dx9.dumped.exe "8B 81 ?? ?? ?? ?? 85 C0 75 01 C3" [...]
    python3 -I tools/sig.py --dump ... --check          # every signature src/sigs.h lists

Each pattern prints its matches in the code section (VA, and the bytes there). A
pattern meant to find one site must match exactly once. --check reads
src/sigs.h's SIG(name, "pattern", expected matches) lines and reports any whose
count differs. Pure stdlib.
"""
import argparse
import re
import struct
import sys

B = 0x400000


def compile_pattern(p):
    out = b""
    for tok in p.split():
        out += b"." if tok.startswith("?") else re.escape(bytes([int(tok, 16)]))
    return re.compile(out, re.S)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", required=True)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--sigs", default="src/sigs.h")
    ap.add_argument("patterns", nargs="*")
    a = ap.parse_args()
    d = open(a.dump, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    vs, va = struct.unpack_from("<II", d, pe + 24 + optsz + 8)
    text = d[va:va + vs]
    if a.check:
        bad = 0
        n = 0
        # The macro's lines joined, comments dropped and adjacent string literals
        # concatenated, so a pattern written over several lines reads as one.
        src = open(a.sigs).read().replace("\\\n", " ")
        src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
        src = re.sub(r'"\s+"', "", src)
        seen = set()
        for m in re.finditer(r'SIG\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*(\d+)\s*\)', src):
            n += 1
            if m.group(1) in seen:  # the mod looks a signature up by name: a second one is never found
                bad += 1
                print(f"BAD {m.group(1):32s} defined again - the mod finds only the first")
            seen.add(m.group(1))
            hits = [B + va + x.start() for x in compile_pattern(m.group(2)).finditer(text)]
            want = int(m.group(3))
            ok = len(hits) == want
            bad += not ok
            print(f"{'ok ' if ok else 'BAD'} {m.group(1):32s} {len(hits)} match(es) (want {want}) "
                  f"{' '.join(hex(h) for h in hits[:4])}")
        print(f"{n} signatures, {bad} bad")
        sys.exit(1 if bad else 0)
    for p in a.patterns:
        hits = [va + x.start() for x in compile_pattern(p).finditer(text)]
        print(f"{len(hits)} match(es): {p}")
        for h in hits[:12]:
            print(f"  {B + h:#x}  {d[h:h + len(p.split())].hex(' ')}")


if __name__ == "__main__":
    main()
