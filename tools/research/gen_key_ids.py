"""Writes src/cm3/key_ids_table.c: the name chains of FUN_00601a40 (index.cpp), read from cm0102.exe.

Usage: python3 tools/research/gen_key_ids.py <path to cm0102.exe> > src/cm3/key_ids_table.c

A chain compares a name with fixed strings through FUN_00951c50 (_stricmp) and stores the row in the
global of the first string that is equal:

    push <string>; push <name>; call 0x951c50; add esp, 8; test eax, eax; jne <next block>
    mov dword ptr [<global>], <row>; jmp <end of the chain>

Some blocks share one store block: "je <store block>" in place of the jne. A store block can store
the row in two globals. A function can hold more than
one chain: the end of one chain is the first block of the next, and the blocks of the last chain
return. The script reads the chains with objdump (Intel syntax) and stops at any other shape. The
differential test (cases_database.py) checks the result against the original.
"""
import re
import struct
import subprocess
import sys

STRICMP = 0x951C50
INIT = (0x601A4A, 0x60386C)                 # the code of FUN_00601a40 that sets the key ids to -1
REPORT = (0x6043AC, 0x60BA80)               # its checks: a key id that stays -1 gives the box index.cpp 5809

# (C name, start, end, what the name is): the name chains of FUN_00601a40, in its order.
CHAINS = [
    ("g_key_colour_chain", 0x60386C, 0x603C3E, "colour names, inline at 0x60386c"),
    ("g_key_nation_chain", 0x60D810, 0x60EFD0, "nation names, FUN_0060d810"),
    ("g_key_club_comp_chain", 0x60EFD0, 0x6115E0, "club comp names, FUN_0060efd0"),
    ("g_key_continent_chain", 0x604289, 0x604330, "continent names, inline at 0x604289"),
    ("g_key_staff_comp_chain", 0x6115E0, 0x6146B0, "staff comp names, FUN_006115e0"),
    ("g_key_club_chain", 0x6146B0, 0x616F20, "club names, FUN_006146b0"),
]


def disassemble(exe, start, end):
    out = subprocess.run(["objdump", "-d", "--x86-asm-syntax=intel", f"--start-address={start:#x}",
                          f"--stop-address={end:#x}", exe], capture_output=True, text=True, check=True).stdout
    lines = []
    for line in out.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s*(.*)$", line)
        if m:
            lines.append((int(m.group(1), 16), " ".join(m.group(2).split())))
    return lines


class Image:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        count = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.base = struct.unpack_from("<I", self.data, pe + 24 + 28)[0]
        self.sections = [struct.unpack_from("<8sIIII", self.data, pe + 24 + optional + i * 40)[1:]
                         for i in range(count)]

    def c_string(self, address):
        rva = address - self.base
        for vsize, va, rsize, rptr in self.sections:
            if va <= rva < va + rsize:
                start = rptr + rva - va
                return self.data[start:self.data.index(b"\0", start)]
        raise ValueError(f"{address:#x} is not in the file")

    def value(self, address):
        """Four bytes of the loaded image: 0 past the bytes of the file (uninitialised data)."""
        rva = address - self.base
        for vsize, va, rsize, rptr in self.sections:
            if va <= rva < va + max(vsize, rsize):
                start = rptr + rva - va
                return self.data[start:start + 4] if rva - va < rsize else bytes(4)
        raise ValueError(f"{address:#x} is not in the image")


def block_exit(lines, j):
    """Where a block goes after its store: the target of a jmp, "return", or the next instruction."""
    text = lines[j][1] if j < len(lines) else ""
    if text.startswith("jmp"):
        return int(text.split()[1], 16)
    k = j
    while k < len(lines) and lines[k][1].startswith("pop"):
        k += 1
    if k < len(lines) and lines[k][1] == "ret":
        return "return"
    return lines[j][0]                     # falls through to the next instruction


def read_chain(exe, image, start, end):
    """[(address of name, globals, chain)] in the order of the code."""
    lines = disassemble(exe, start, end)
    at = {address: i for i, (address, _) in enumerate(lines)}
    calls = [i for i, (_, text) in enumerate(lines) if text.startswith(f"call {STRICMP:#x}")]
    if not calls:
        raise SystemExit(f"{start:#x}: no compare")
    entries = []
    chain = 0
    chain_exit = None
    for n, i in enumerate(calls):
        address = lines[i][0]
        # push <string>; [lea <name>, ...]; push <name>; call
        if not re.fullmatch(r"push e[a-z]{2}", lines[i - 1][1]):
            raise SystemExit(f"{address:#x}: unexpected name operand {lines[i - 1][1]}")
        k = i - 2
        while not re.fullmatch(r"push 0x[0-9a-f]+", lines[k][1]):
            if i - k > 3:
                raise SystemExit(f"{address:#x}: no string push")
            k -= 1
        first = lines[k][0]
        name = int(lines[k][1].split()[1], 16)
        image.c_string(name)               # the name must be in the file
        j = i + 1
        while not lines[j][1].startswith(("jne", "je ")):
            if j - i > 4:
                raise SystemExit(f"{address:#x}: no jump after the compare")
            j += 1
        if lines[j][1].startswith("je "):
            j = at[int(lines[j][1].split()[1], 16)]
        else:
            j += 1
        stores = []
        while j < len(lines):
            s = re.fullmatch(r"mov dword ptr \[(0x[0-9a-f]+)\], e[a-z]{2}", lines[j][1])
            if not s:
                break
            stores.append(int(s.group(1), 16))
            j += 1
        if not 1 <= len(stores) <= 2:
            raise SystemExit(f"{address:#x}: {len(stores)} stores")
        # The inline chains end where their range ends: the last block falls through.
        leave = block_exit(lines, j) if j < len(lines) else end
        if chain_exit is not None and first == chain_exit:
            chain += 1                     # the first block of the next chain
            chain_exit = None
        if chain_exit is None:
            chain_exit = leave
        elif leave != chain_exit and n != len(calls) - 1:
            raise SystemExit(f"{address:#x}: leaves to {leave}, its chain to {chain_exit}")
        entries.append((name, stores, chain))
    return entries


def key_globals(exe):
    """The globals that FUN_00601a40 sets to -1 first (0x601a4a..0x603864), by address."""
    found = set()
    for address, text in disassemble(exe, *INIT):
        m = re.fullmatch(r"mov dword ptr \[(0x[0-9a-f]+)\], edi", text)
        if m:
            found.add(int(m.group(1), 16))
    return sorted(found)


def reported_globals(exe):
    """The globals that FUN_00601a40 checks after the chains (0x6043ac..0x60ba80): "cmp dword ptr [g], edi"
    with edi -1 (0x6043a9), then the error box of index.cpp 5809 when g is still -1 (0.6.1 item 10)."""
    found = set()
    for address, text in disassemble(exe, *REPORT):
        m = re.fullmatch(r"cmp dword ptr \[(0x[0-9a-f]+)\], edi", text)
        if m:
            found.add(int(m.group(1), 16))
    return found


def main(exe):
    image = Image(exe)
    reset = key_globals(exe)
    report = reported_globals(exe)
    chains = [(symbol, read_chain(exe, image, start, end), what) for symbol, start, end, what in CHAINS]
    globals_ = sorted(set(reset) | {g for _, entries, _ in chains for _, stores, _ in entries for g in stores})
    slot = {g: i for i, g in enumerate(globals_)}
    print("/*")
    print(" * Generated by tools/research/gen_key_ids.py from cm0102.exe. Do not edit.")
    print(" * The key ids of FUN_00601a40 (index.cpp): the globals that it sets to -1")
    print(" * or that its name chains set, with their value in the file, and the name")
    print(" * chains. Each entry of a chain is a name, the one or two key ids that a row")
    print(" * with that name sets (as indexes into g_key_id_globals, -1 for none), and")
    print(" * its chain: in each chain the first equal name wins. The names are not")
    print(" * here: each entry holds the address of its name in cm0102.exe, which the")
    print(" * port reads from the player's own file (exe_image.h).")
    print(" */")
    print('#include "key_index.h"')
    print()
    print(f"/* {len(globals_)} globals: the address, the value in the file, 1 when FUN_00601a40")
    print("   sets it to -1 first, and 1 when it shows the box index.cpp 5809 for it at -1. */")
    print("const key_id_global_t g_key_id_globals[KEY_ID_COUNT] = {")
    for g in globals_:
        value = struct.unpack("<i", image.value(g))[0]
        print(f"    {{{g:#x}, {value}, {int(g in reset)}, {int(g in report)}}},")
    print("};")
    print()
    total = 0
    for symbol, entries, what in chains:
        print(f"/* {len(entries)} {what}, in {entries[-1][2] + 1} chain(s). */")
        print(f"const key_id_entry_t {symbol}[] = {{")
        for name, stores, chain in entries:
            missing = [g for g in stores if g not in slot]
            if missing:
                raise SystemExit(f"{symbol}: {name}: global {missing[0]:#x} is not a key id")
            slots = [slot[g] for g in stores] + [-1]
            print(f"    {{{name:#x}, {slots[0]}, {slots[1]}, {chain}}},")
        print("};")
        print(f"const s32 {symbol}_length = {len(entries)};")
        print()
        total += len(entries)
    print(f"/* {total} entries. */")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
