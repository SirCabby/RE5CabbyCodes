#!/usr/bin/env python3
"""The fields MT Framework classes register by name (createProperty), read out of the game's code.

    python3 -I tools/mt_props.py --dump ghidra_proj/re5dx9.dumped.exe --asm ghidra_proj/re5dx9.asm \
        CLASS [CLASS...] [--all]

Every MtObject class registers its fields in vtable slot 3 (createProperty): it
calls its parent's first, then, for each field, fills a small record on the
stack - {+0 name, +4 type (u16), +6 flags (u16), +8 owner, +0xC the field's
address, ...} - and hands it to MtPropertyList's add (0x422c00 in build
11465250, found as the call most of them make). The field's address is the
object plus the field's offset, so each record gives (name, type, offset).

The code is read from the objdump listing (`i686-w64-mingw32-objdump -d -M intel
--no-show-raw-insn re5dx9.fixed.exe`), which is plain text; the registers and
the stack are followed through the few instruction forms these functions use.
A field whose address the tracking cannot follow is printed with offset '?'.
--all prints every class's own fields (slow). Pure stdlib.
"""
import argparse
import collections
import re
import struct
import sys

B = 0x400000
LINE = re.compile(r"^\s+([0-9a-f]+):\s+(.*)$")
REGS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")


def load_asm(path):
    insns = {}
    order = []
    with open(path) as f:
        for line in f:
            m = LINE.match(line)
            if not m:
                continue
            a = int(m.group(1), 16)
            insns[a] = m.group(2).strip()
            order.append(a)
    return insns, order


class Image:
    def __init__(self, path):
        self.d = open(path, "rb").read()

    def u32(self, va):
        return struct.unpack_from("<I", self.d, va - B)[0]

    def cstr(self, va):
        o = va - B
        if not 0 <= o < len(self.d):
            return None
        e = self.d.find(b"\0", o, o + 200)
        if e < 0:
            return None
        s = self.d[o:e]
        return s.decode("latin-1") if all(32 <= c < 127 for c in s) else None


def imm(s):
    s = s.strip()
    if s.startswith("0x"):
        return int(s, 16)
    if re.fullmatch(r"-?\d+", s):
        return int(s)
    return None


def mem_operand(op):
    """'DWORD PTR [esp+0x10]' -> ('esp', 0x10); '[edi+0x60]' -> ('edi', 0x60)."""
    m = re.search(r"\[(\w+)(?:([+-])(0x[0-9a-f]+|\d+))?\]", op)
    if not m:
        return None
    off = int(m.group(3), 0) if m.group(3) else 0
    return m.group(1), -off if m.group(2) == "-" else off


def walk(insns, order_index, order, fn, add_fn, limit=4000):
    """Yield (name_va, type, flags, offset) for each add_fn call in fn; also returns the parent call."""
    i = order_index.get(fn)
    if i is None:
        return [], None
    regs = {"ecx": ("this", 0)}  # value: ('this', off) | ('stack', slot) | ('imm', v)
    delta = 0  # bytes pushed since entry (esp = entry_esp - delta)
    stack = {}  # slot (entry-relative, negative) -> value
    out = []
    parent = None
    calls = 0
    for _ in range(limit):
        if i >= len(order):
            break
        a = order[i]
        ins = insns[a]
        i += 1
        mn, _, ops = ins.partition(" ")
        ops = ops.strip()
        args = [x.strip() for x in re.split(r",(?![^\[]*\])", ops)] if ops else []
        if mn in ("ret", "jmp"):
            break
        if mn == "push":
            delta += 4
            v = regs.get(args[0]) if args[0] in REGS else ("imm", imm(args[0])) if imm(args[0]) is not None else None
            stack[-delta] = v
            continue
        if mn == "pop":
            if args[0] in REGS:
                regs[args[0]] = stack.get(-delta)
            delta -= 4
            continue
        if mn in ("sub", "add") and args and args[0] == "esp":
            v = imm(args[1]) or 0
            delta += v if mn == "sub" else -v
            continue
        if mn == "mov" and len(args) == 2:
            dst, src = args
            if dst in REGS:
                if src in REGS:
                    regs[dst] = regs.get(src)
                elif imm(src) is not None:
                    regs[dst] = ("imm", imm(src))
                else:
                    mo = mem_operand(src)
                    if mo and mo[0] == "esp":
                        regs[dst] = stack.get(mo[1] - delta)
                    else:
                        regs[dst] = None
                continue
            mo = mem_operand(dst)
            if mo and mo[0] == "esp":
                slot = mo[1] - delta
                if src in REGS:
                    stack[slot] = regs.get(src)
                elif imm(src) is not None:
                    width = 2 if dst.startswith("WORD") else 1 if dst.startswith("BYTE") else 4
                    stack[slot] = ("imm", imm(src), width)
                continue
            continue
        if mn == "lea" and len(args) == 2 and args[0] in REGS:
            mo = mem_operand(args[1])
            if mo:
                base, off = mo
                if base == "esp":
                    regs[args[0]] = ("stack", off - delta)
                else:
                    b = regs.get(base)
                    regs[args[0]] = ("this", b[1] + off) if b and b[0] == "this" else None
            else:
                regs[args[0]] = None
            continue
        if mn == "add" and len(args) == 2 and args[0] in REGS and imm(args[1]) is not None:
            b = regs.get(args[0])
            regs[args[0]] = ("this", b[1] + imm(args[1])) if b and b[0] == "this" else None
            continue
        if mn == "xor" and len(args) == 2 and args[0] == args[1] and args[0] in REGS:
            regs[args[0]] = ("imm", 0)
            continue
        if mn == "call":
            target = imm(args[0]) if args else None
            calls += 1
            if target == add_fn:
                rec = stack.get(-delta)
                if rec and rec[0] == "stack":
                    s = rec[1]
                    name = stack.get(s)
                    typ = stack.get(s + 4)
                    flags = stack.get(s + 6)
                    field = stack.get(s + 12)
                    nva = name[1] if name and name[0] == "imm" else None
                    t = typ[1] & 0xFFFF if typ and typ[0] == "imm" else None
                    fl = flags[1] & 0xFFFF if flags and flags[0] == "imm" else None
                    off = field[1] if field and field[0] == "this" else None
                    out.append((nva, t, fl, off))
                delta -= 4  # the record's pointer, taken off by the callee
            elif calls == 1 and target:
                parent = target
                delta -= 4  # the list, taken off by the callee
            for r in ("eax", "ecx", "edx"):
                regs[r] = None
            continue
        if args and args[0] in REGS:
            regs[args[0]] = None
    return out, parent


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", required=True)
    ap.add_argument("--asm", required=True)
    ap.add_argument("--add", type=lambda s: int(s, 0), default=0x422C00, help="MtPropertyList's add")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("classes", nargs="*")
    a = ap.parse_args()
    sys.path.insert(0, __file__.rsplit("/", 1)[0])
    import mt_classes
    img = mt_classes.Image(a.dump)
    root, recs = mt_classes.records(img)
    gd = mt_classes.getdti_index(img)
    byname = {}
    for dti, r in recs.items():
        byname.setdefault(r["name"], dti)
    insns, order = load_asm(a.asm)
    order_index = {addr: k for k, addr in enumerate(order)}
    im = Image(a.dump)

    def vt(dti):
        fn = gd.get(dti)
        v = mt_classes.vtables_of(img, fn) if fn else []
        return v[0] if v else 0

    def own(dti):
        v = vt(dti)
        if not v:
            return None, []
        fn = im.u32(v + 12)
        # Through an incremental-link thunk.
        if insns.get(fn, "").startswith("jmp"):
            t = imm(insns[fn].split()[-1])
            fn = t or fn
        props, _ = walk(insns, order_index, order, fn, a.add)
        return fn, props

    names = a.classes or ([r["name"] for r in recs.values()] if a.all else [])
    for cname in names:
        dti = byname.get(cname)
        if dti is None:
            print(f"{cname}: no such class")
            continue
        chain = []
        d = dti
        while d in recs:
            chain.append(d)
            d = recs[d]["parent"]
        print(f"== {cname} ({recs[dti]['size']:#x} bytes): {' < '.join(recs[c]['name'] for c in chain)}")
        seen = set()
        for c in reversed(chain):
            fn, props = own(c)
            if not props:
                continue
            key = tuple(props)
            if key in seen:
                continue
            seen.add(key)
            print(f"  -- {recs[c]['name']} (createProperty {fn:#x})")
            for nva, t, fl, off in props:
                nm = im.cstr(nva) if nva else None
                o = f"{off:#06x}" if off is not None else "     ?"
                print(f"    {o}  type {t if t is not None else '?':>3}  flags {fl if fl is not None else '?':>3}  {nm}")


if __name__ == "__main__":
    main()
