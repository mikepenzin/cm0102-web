#!/usr/bin/env python3
"""Every cup list of the original starts cleared in a season game (0.6.2 item 64).

A cup class makes its list of places (6 bytes a club) with the allocator 0x944e46, which does not clear the block,
stores it to +0xa7 and writes only the places it finds clubs for. A place it does not write keeps stale bytes: a
club of another cup, or no club at all (CroCup::collect_entrants 0x517bb0 in a 2005 start: Database.cpp 13471).
This check finds each `call 0x944e46` followed by `mov dword ptr [reg + 0xa7], eax` in the original and fails when
the store is not in CUP_INSNS of tools/recomp/league_sizes.py with rt_cup_places_list, or in LIST_STORES_LEFT.

Usage: cup_list_clear_check.py --exe <cm0102.exe>
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "recomp"))
import league_sizes  # noqa: E402
from pe import Image  # noqa: E402

ALLOC = 0x944E46
STORE = re.compile(r"mov dword ptr \[(e[a-z]{2}) \+ 0xa7\], eax$")
# The None rows: the block is the count at +0x3e times 6: `movsx x, word ptr [obj + 0x3e]` (or `mov word ptr
# [obj + 0x3e], ax` then `movsx x, ax`), `lea y, [x + x*2]`, `shl y, 1`, `push y`, with other instructions between.
WINDOW = 0xC0
# IntertotoCup::collect_entrants 0x6336c0 reads its count at 0x633798 and comes to the size at 0x633810 by the jge at
# 0x63379e: store: (count read, the jump to the size, the size). The check proves each instruction.
JUMPS = {0x63381E: ((0x633798, "movsx eax, word ptr [esi + 0x3e]"), (0x63379E, "jge 0x633810"),
                    (0x633810, "lea edx, [eax + eax*2]"))}


def list_stores(img):
    """(store address, store text, object register, the instructions before the call) of each store to +0xa7
    within 8 instructions after a call of the allocator."""
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    start, end = img.text
    code = img.read(start, end - start)
    out = []
    for p in range(len(code) - 5):
        if code[p] != 0xE8 or (start + p + 5 + int.from_bytes(code[p + 1:p + 5], "little", signed=True)) \
                & 0xFFFFFFFF != ALLOC:
            continue
        for ins in list(md.disasm(code[p:p + 48], start + p))[1:9]:
            m = STORE.match(f"{ins.mnemonic} {ins.op_str}")
            if m:
                before = []
                for back in range(WINDOW, 4, -1):      # the first decoding that ends on the call
                    b = list(md.disasm(code[p - back:p + 5], start + p - back))
                    if b and b[-1].address == start + p:
                        before = [f"{x.mnemonic} {x.op_str}" for x in b[:-1]]
                        break
                out.append((ins.address, f"{ins.mnemonic} {ins.op_str}", m.group(1), before))
                break
    return out


def size_fault(reg, size, before):
    """Why the block size of a LIST_STORES row is not the size the original allocates, or None."""
    pushes = [i for i, x in enumerate(before) if x.startswith("push ")]
    if not pushes:
        return "no push before the call"
    last = before[pushes[-1]]
    if size is not None:
        return None if last == f"push {size:#x}" else f"{last}, not push {size:#x}"
    y = last.split()[1]
    shl = [i for i in range(pushes[-1]) if before[i] == f"shl {y}, 1"]
    lea = [i for i in range(shl[-1] if shl else 0) if re.fullmatch(rf"lea {y}, \[(e..) \+ \1\*2\]", before[i])]
    if not shl or not lea:
        return f"{last} is not 6 times a count"
    x = re.fullmatch(r"lea e.., \[(e..) \+ e..\*2\]", before[lea[-1]]).group(1)
    for i in range(lea[-1] - 1, -1, -1):
        if before[i] == f"movsx {x}, word ptr [{reg} + 0x3e]":
            return None
        if before[i] == f"movsx {x}, ax" and any(b.startswith(f"mov word ptr [{reg} + 0x3e], ax") for b in before[:i]):
            return None
        if before[i].split(" ", 1)[-1].startswith(f"{x},"):
            break
    return f"the count of {last} is not the count at +0x3e"


def main(argv):
    img = Image(argv[argv.index("--exe") + 1])
    stores = list_stores(img)
    bad = []
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for at, text, reg, before in stores:
        if at in league_sizes.LIST_STORES_LEFT:
            continue
        row = league_sizes.CUP_INSNS.get(at)
        if not row or row[0] != text or "rt_cup_places_list" not in row[1] or at not in league_sizes.LIST_STORES:
            bad.append(f"{at:#x} {text}: the list is not cleared")
            continue
        r, size = league_sizes.LIST_STORES[at]
        why = size_fault(r, size, before) if r == reg else f"register {r}, not {reg}"
        if at in JUMPS and size is None and r == reg:
            seen = [next(md.disasm(img.read(a, 16), a)) for a, _t in JUMPS[at]]
            why = None if [f"{x.mnemonic} {x.op_str}" for x in seen] == [t for _a, t in JUMPS[at]] else \
                f"not the path of JUMPS: {[f'{x.mnemonic} {x.op_str}' for x in seen]}"
        if why:
            bad.append(f"{at:#x}: {why}")
    found = {a for a, _t, _r, _b in stores}
    for at in list(league_sizes.LIST_STORES) + list(league_sizes.LIST_STORES_LEFT):
        if at not in found:
            bad.append(f"{at:#x}: in league_sizes.py, but not a list store of the original")
    for b in bad:
        print("FAIL " + b)
    print(f"cup_list_clear_check: {len(stores)} list stores, {len(bad)} faults")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
