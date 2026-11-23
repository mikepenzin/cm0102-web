"""Tests the crash guards (tools/recomp/crashguards.py) against the Unicorn run of the original.

Usage:
    python tests/recomp/test_crashguards.py <cm0102.exe> [<work folder>]

The test recompiles only the guarded functions (and _ftol), with a stub for every function that they call, and
links them with tests/recomp/guards_driver.c. Each case puts fake objects in memory, fills page 0 with 0xCC,
calls one guarded function in the recompiled program and in Unicorn, and compares eax, esp, st(0), page 0, the
stack, the work area (the objects and the log of stub calls) and the image.

- A good pointer: the original runs in Unicorn as it is. The guard must not act, and the results must be equal.
- A NULL pointer: the original would crash. Unicorn runs the original with the jump of the guard
  (a hook at the guard address that goes to the exit). The guard must act, and the results must be equal.
  Page 0 must keep its 0xCC bytes: no NULL write.

The stubs answer from a queue per function (0 when the queue is empty), so both runs take the same path.
"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "recomp"))
sys.setrecursionlimit(50000)

from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EDX,  # noqa: E402
                               UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP)

from analysis import Analyzer  # noqa: E402
from crashguards import GUARDS  # noqa: E402
from pe import Image  # noqa: E402
from reference import RETURN_ADDR, STACK_BASE, STACK_SIZE, STACK_TOP, Reference  # noqa: E402

WORK, WORK_SIZE = 0x30000000, 0x200000
LOG = WORK + 0x1F0000
RESULT = 0x30FF0000          # Unicorn: st(0) is stored here after the call
VFUNC = 0x401000             # a stub that the vtables of the competition table case name (thiscall, 2 arguments)
FTOL = 0x945640
ARG_ZONE_VTABLE, ARG_CUP_VTABLE = 0x96D0F0, 0x969468   # League_Stage (0x68b196), Cup_Stage (0x522e80)

STAFF_PTR, STAFF_COUNT, CLUB_PTR, COMP_PTR = 0xAE23BC, 0xAE2364, 0xAE23B4, 0xADADFC
STAFF_SIZE, CLUB_SIZE = 110, 581

# The guarded functions: entry -> the guard addresses in it.
FUNCTIONS = {
    0x4AEAF0: [0x4AEB0F], 0x4B07B0: [0x4B07BA], 0x48CE10: [0x48CE46], 0x84AEB0: [0x84AEBB],
    0x85D8A0: [0x85D8B8], 0x8E7B50: [0x8E7BEF], 0x8E8800: [0x8E8827], 0x85A470: [0x85A556],
    0x86CD70: [0x86D609], 0x8C9390: [0x8C94CA], 0x8C39F0: [0x8C3CAD, 0x8C3E16], 0x409C20: [0x409E25],
}


# What each guard does at its jump, in Python, for the Unicorn run. Returns True when it acts.
def _u32(uc, a):
    return struct.unpack("<I", bytes(uc.mem_read(a, 4)))[0]


def _worldcup(uc):
    ecx, eax = uc.reg_read(UC_X86_REG_ECX), uc.reg_read(UC_X86_REG_EAX)
    other = _u32(uc, eax)
    if ecx and other:
        return False
    if not ecx and not other:          # both NULL: return 0
        esp = uc.reg_read(UC_X86_REG_ESP)
        uc.reg_write(UC_X86_REG_EAX, 0)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, _u32(uc, esp))
        return True
    if not ecx:                        # the first slot gets the second team
        uc.reg_write(UC_X86_REG_ECX, other)
        slot1 = _u32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)
        uc.mem_write(slot1, struct.pack("<I", other))
    else:                              # the second slot gets the first team
        uc.mem_write(eax, struct.pack("<I", ecx))
    return True                        # the compare runs on, with the slot filled


def _if_zero(reg, to, **set_regs):
    def act(uc):
        if uc.reg_read(reg):
            return False
        for r, v in set_regs.items():
            uc.reg_write(int(r[1:]), v)
        uc.reg_write(UC_X86_REG_EIP, to)
        return True
    return act


EMULATE = {
    0x4AEB0F: _if_zero(UC_X86_REG_EDI, 0x4AEB11),
    0x4B07BA: _worldcup,
    0x48CE46: lambda uc: not uc.reg_read(UC_X86_REG_ESI) and (
        uc.reg_write(UC_X86_REG_ESP, uc.reg_read(UC_X86_REG_ESP) + 8), uc.reg_write(UC_X86_REG_EIP, 0x48CE56), True)[-1],
    0x84AEBB: _if_zero(UC_X86_REG_EAX, 0x84AEF1),
    0x85D8B8: _if_zero(UC_X86_REG_EDX, 0x85D8BC, **{f"r{UC_X86_REG_EAX}": 0}),
    0x8E7BEF: _if_zero(UC_X86_REG_ECX, 0x8E7C2D),
    0x8E8827: _if_zero(UC_X86_REG_EAX, 0x8E8845),
    0x85A556: _if_zero(UC_X86_REG_EAX, 0x85A745),
    0x86D609: _if_zero(UC_X86_REG_EDX, 0x86D6AE),
    0x8C94CA: _if_zero(UC_X86_REG_EDX, 0x8C94EC),
    0x8C3CAD: _if_zero(UC_X86_REG_EAX, 0x8C3CB1),
    0x8C3E16: _if_zero(UC_X86_REG_EAX, 0x8C3E1A),
    0x409E25: lambda uc: _arg_stage_not_zone(uc),
}


def _arg_stage_not_zone(uc):
    """The stage bound at 0x409e25: the next stage index is below [esi+0x30] and that stage is not a League_Stage."""
    i = uc.reg_read(UC_X86_REG_EAX) & 0xFFFF
    i = i - 0x10000 if i & 0x8000 else i
    esi = uc.reg_read(UC_X86_REG_ESI)
    if i >= struct.unpack("<i", bytes(uc.mem_read(esi + 0x30, 4)))[0]:
        return False
    if _u32(uc, _u32(uc, _u32(uc, esi + 0xC) + 4 * i)) == ARG_ZONE_VTABLE:
        return False
    uc.reg_write(UC_X86_REG_EIP, 0x409E30)
    return True


class Mem:
    """The memory of one case: blocks to write, and a bump allocator in the work area."""
    def __init__(self):
        self.blocks = {}
        self.top = WORK + 0x1000

    def new(self, size):
        a = self.top
        self.top += (size + 15) & ~15
        self.blocks[a] = bytearray(size)
        return a

    def write(self, addr, data):
        self.blocks[addr] = bytearray(data)

    def u8(self, addr, v):
        self.write(addr, struct.pack("<B", v & 0xFF))

    def u16(self, addr, v):
        self.write(addr, struct.pack("<H", v & 0xFFFF))

    def u32(self, addr, v):
        self.write(addr, struct.pack("<I", v & 0xFFFFFFFF))

    def f64(self, addr, v):
        self.write(addr, struct.pack("<d", v))

    def tables(self, staff=8, clubs=4):
        """A staff table and a club table, with the count of staff."""
        s = self.new(STAFF_SIZE * staff)
        c = self.new(CLUB_SIZE * clubs)
        self.u32(STAFF_PTR, s)
        self.u32(STAFF_COUNT, 100)
        self.u32(CLUB_PTR, c)
        return s, c


class Case:
    def __init__(self, name, entry, null, ecx=0, args=(), mem=None, queues=None, fpu=False, stack=None):
        self.name, self.entry, self.null = name, entry, null
        self.ecx, self.args, self.mem = ecx, list(args), mem or Mem()
        self.queues = queues or {}
        self.fpu = fpu
        if stack:   # bytes of the stack below the arguments: (offset below the entry esp, data)
            esp0 = STACK_TOP - 4 * (len(self.args) + 1)
            for off, data in stack:
                self.mem.write(esp0 - off, data)

    def blocks(self):
        # In order of address: a write into an allocated block comes after the block (they are all zeros).
        return [(a, bytes(d)) for a, d in sorted(self.mem.blocks.items())]


# ---- The cases ----

def news(null):
    m = Mem()
    obj = m.new(16)
    if not null:
        text = m.new(16)
        m.write(text, b"Hi\0")
        m.u32(obj, text)
    return Case("news " + ("NULL text" if null else "text"), 0x4AEAF0, null, ecx=obj, args=[0] * 8, mem=m)


def worldcup(which):
    m = Mem()
    s1, s2 = m.new(4), m.new(4)
    t1, t2 = m.new(0x100), m.new(0x100)
    m.u16(t1 + 0x80, 5)
    m.u16(t2 + 0x80, 9)
    if which != 1 and which != 3:
        m.u32(s1, t1)
    if which != 2 and which != 3:
        m.u32(s2, t2)
    names = {0: "two teams", 1: "first team NULL", 2: "second team NULL", 3: "both NULL"}
    return Case("world cup " + names[which], 0x4B07B0, which != 0, args=[s1, s2], mem=m)


def competition_table(null):
    m = Mem()
    vt = m.new(0x20)
    m.u32(vt + 0x10, VFUNC)
    obj, c1, c2, table = m.new(0x40), m.new(0x40), m.new(0x40), m.new(8)
    for o in (obj, c1, c2):
        m.u32(o, vt)
    m.u32(obj + 0x1C, 0xFFFFFFFF)
    m.u32(obj + 0x20, 0)
    m.u32(c1 + 0x1C, 0xFFFFFFFF)
    m.u32(c1 + 0x20, 1)
    m.u32(c2 + 0x20, 0xFFFFFFFF)
    m.u32(table, c1)
    if not null:
        m.u32(table + 4, c2)
    m.u32(COMP_PTR, table)
    return Case("competition table " + ("NULL competition" if null else "competitions"), 0x48CE10, null, ecx=obj, mem=m,
                queues={VFUNC: [1, 1, 1]})


def transfer_manager(null):
    m = Mem()
    obj, tb, lst, a, b = m.new(0x100), m.new(0x40), m.new(40 * 0x25), m.new(4), m.new(4)
    m.u32(obj + 0xDC, tb)
    m.u32(a, 2)
    m.u8(tb + 18 + 4, 1)
    m.u32(tb + 18, lst)
    m.u32(lst + 3 * 0x25 + 0xF, 77)
    m.u32(b, 77)
    return Case("transfer_manager " + ("NULL argument" if null else "argument"), 0x84AEB0, null, ecx=obj,
                args=[0 if null else a, b], mem=m)


def virtual_staff(null, bonus):
    m = Mem()
    a, b, d, p, q, c = m.new(0x40), m.new(0x60), m.new(0x80), m.new(0x40), m.new(0x40), m.new(0x60)
    m.u32(a + 0x20, b)
    m.u16(a + 0x30, 0xA0 if bonus else 0x10)
    m.u8(a + 0x3A, 5 if bonus else 2)
    m.u8(a + 0x3C, 1)
    m.u32(a + 0x1C, c)
    if not null:
        m.u32(b + 0x53, d)
        m.u8(d + 0x7E, 12)
    for o, x, y in ((p, 3.0, 4.0), (q, 1.0, 1.5)):
        m.f64(o + 0x23, x)
        m.f64(o + 0x2B, y)
    lo, hi = struct.unpack("<II", struct.pack("<d", 0.25))
    name = "virtual_staff 195 " + ("NULL staff" if null else "staff") + (", 0xa0 bonus" if bonus else "")
    return Case(name, 0x85D8A0, null, args=[a, p, q, lo, hi], mem=m, fpu=True)


def transfer_offer(null):
    m = Mem()
    staff, clubs = m.tables()
    s1 = staff + STAFF_SIZE
    club = clubs + CLUB_SIZE
    offer = m.new(0x50)
    m.u32(offer + 0xC, 1)
    m.u32(offer + 0x1B, 1000)
    m.u8(offer + 0x11, 0xB)
    m.u32(s1 + 0x39, club)
    if not null:
        pd = m.new(0x20)
        m.u16(pd + 0xB, 21)
        m.u32(s1 + 0x61, pd)
    return Case("Transfer Offer " + ("NULL player data" if null else "player data"), 0x8E7B50, null, ecx=offer,
                args=[1, 0, 0, 0, 0], mem=m, queues={0x6E86D0: [3, 7]})


def transfer_offer_type(null, same_type):
    m = Mem()
    obj, r, s, x = m.new(0x20), m.new(0x20), m.new(0x80), m.new(0x10)
    m.u8(obj + 0x13, 4)
    m.u32(r + 8, 5)
    m.u32(s, 6)
    m.u32(0x9CF4DC, 0x11111111)
    m.u32(x, 0x11111111 if same_type else 0x22222222)
    if not null:
        m.u32(s + 0x53, x)
    name = "transfer_offer type " + ("NULL" if null else "same type" if same_type else "other type")
    return Case(name, 0x8E8800, null, ecx=obj, args=[r, s, 0, 0], mem=m)


def squad_manager_case(null):
    m = Mem()
    staff, _ = m.tables()
    s = staff + 2 * STAFF_SIZE
    m.u32(s, 5)
    if not null:
        pd = m.new(0x20)
        m.u8(pd + 0xF, 0xF)
        m.u32(s + 0x61, pd)
    obj = m.new(0x20)
    return Case("squad_manager " + ("NULL player data" if null else "player data"), 0x85A470, null, ecx=obj,
                args=[0x1234, s], mem=m)


def staff_records_club(null):
    m = Mem()
    staff, clubs = m.tables()
    s1, s2 = staff + STAFF_SIZE, staff + 2 * STAFF_SIZE
    c1, c3 = clubs + CLUB_SIZE, clubs + 3 * CLUB_SIZE
    m.u32(clubs + 2 * CLUB_SIZE, 42)                 # the club id of c2
    m.u32(c1, 41)
    this, rec, lst, x, p, arr = m.new(0x20), m.new(0x20), m.new(0x20), m.new(0x40), m.new(0x10), m.new(0x400)
    m.u32(this + 0xC, arr)
    m.u32(rec + 0xC, 1)
    m.u32(rec + 8, 1)
    m.u32(rec + 4, lst)
    m.u8(rec + 0x11, 3)
    m.u32(x + 4, 2)
    m.u32(p, 2)
    m.u8(p + 6, 9)
    m.u32(s1, 7)
    m.u32(s2, 8)
    if not null:
        m.u32(c3, 43)
        m.u32(s2 + 0x39, c3)
    return Case("staff_records 0x86cd70 " + ("NULL club" if null else "club"), 0x86CD70, null, ecx=this, args=[rec],
                mem=m, queues={0x4CF5A0: [x], 0x68D1B0: [p, 0]})


def staff_records_squad(null):
    m = Mem()
    staff, clubs = m.tables()
    s1, s2 = staff + STAFF_SIZE, staff + 2 * STAFF_SIZE
    c1, c2 = clubs + CLUB_SIZE, clubs + 2 * CLUB_SIZE
    k, dm, ft, tm, tb, rec, lst, p = (m.new(0xA00), m.new(0x20), m.new(0x400), m.new(0x100), m.new(0x400),
                                      m.new(0x40), m.new(0x20), m.new(0x10))
    m.u32(dm + 0xA, 0xFFFFFFFF)
    for i in range(28):
        m.u32(k + 0x8AC + 4 * i, dm)
    m.u32(k + 0x86E, ft)
    m.u32(k + 0x856, tm)
    m.u32(tm + 0xDC, tb)
    m.u32(c1, 3)
    m.u32(c1 + 0x53, 0x12345)
    m.u32(rec + 0xC, 1)
    m.u32(rec + 8, 1)
    m.u8(rec + 0x1F, 1)
    m.u32(rec + 4, lst)
    m.u32(p, 2)
    m.u8(ft + 8, 1)
    if not null:
        m.u32(s2 + 0x39, c2)
        m.u32(c2 + 0xD7 + 4 * 7, s2)
    return Case("staff_records 0x8c9390 " + ("NULL club" if null else "club"), 0x8C9390, null, ecx=k, args=[rec],
                mem=m, queues={0x68D1B0: [p, 0, 0], 0x600A50: [1, 1]})


def row_counter(site, null):
    m = Mem()
    staff, _ = m.tables()
    s1 = staff + STAFF_SIZE
    k, ft, match, slots, pd, row = m.new(0x1000), m.new(0x400), m.new(0x60), m.new(2 * 1573), m.new(0x20), m.new(0x40)
    m.u32(k + 0x86E, ft)
    m.u32(k + 0x93A, 0x5555)
    m.u32(k + 0x93E, 0x6666)
    m.u32(match, 0xFFFFFFFE)
    m.u32(match + 0x1C, 0x111)
    m.u32(match + 0x20, 0x222)
    m.u32(s1, 1)
    m.u32(s1 + 0x61, pd)
    m.u8(ft + 8, 0x20)
    for t in range(2):
        for dl in range(20):
            m.u32(slots + 1573 * t + 73 * dl + 0x190, 0xFFFFFFFF)
    stack = None
    if site == 1:
        m.u32(slots + 0x190, 1)
        m.u8(slots + 0x1AD, 0)
    else:
        stack = [(0xC8, struct.pack("<I", s1))]   # the list at [esp+0x110] of 0x8c39f0, which 0x541f90 fills
    queues = {} if null else {0x946464: [row, row + 0x10]}
    name = f"row counter site {site} " + ("not found" if null else "found")
    return Case(name, 0x8C39F0, null, ecx=k, args=[match, slots], mem=m, queues=queues, stack=stack)


def arg_averages(stages, last_club, second_club=None):
    """ArgSecond::update_relegation_averages 0x409c20 with no rows of its own (+0x3e = 0) and flag 0, so only the
    stage loop and the average sums run. Averages table [this+0x18]: 4 rows of 19 bytes, club ids 10 to 13. Stages
    0 to 2 are League_Stage zones (vtable at +0, rows +0x3e, table +0xb1, rows of 59 bytes: club pointer, games
    +0x11, points +0x19). Stages 3 and 4 are Cup_Stage blocks of 0xb2 bytes whose +0xb1 dword is junk. The stage
    loop goes next to the averages index of the last club found, plus 1 (0x409e24)."""
    m = Mem()
    this, arr, avg = m.new(0x100), m.new(4 * 5), m.new(19 * 4)
    m.u32(this + 0xC, arr)
    m.u32(this + 0x18, avg)
    m.u32(this + 0x30, stages)
    m.u16(this + 0x34, 4)
    for k in range(4):
        m.u32(avg + 19 * k, 10 + k)
        m.u16(avg + 19 * k + 0x10, 30 + k)
        m.u8(avg + 19 * k + 0x12, 10)
    clubs = {}
    for cid in (10, 11, 12, 13, 99):
        clubs[cid] = m.new(8)
        m.u32(clubs[cid], cid)
    for z, cid in enumerate((last_club, second_club, None)):
        st = m.new(0xC0)
        m.u32(st, ARG_ZONE_VTABLE)
        m.u32(arr + 4 * z, st)
        if cid is not None:
            rows = m.new(59)
            m.u16(st + 0x3E, 1)
            m.write(st + 0xB1, struct.pack("<I", rows))
            m.u32(rows, clubs[cid])
            m.u8(rows + 0x11, 2)
            m.u16(rows + 0x19, 4)
    for z in (3, 4):
        st = m.new(0xB2 + 3)
        m.u32(st, ARG_CUP_VTABLE)
        m.u16(st + 0x3E, 8)
        m.write(st + 0xB1, b"\x00\xcc\xcc\xcc")
        m.u32(arr + 4 * z, st)
    null = stages > 3 and last_club == 12
    name = f"arg average points, {stages} stages, last club of zone 0 at index {last_club - 10}"
    return Case(name, 0x409C20, null, ecx=this, args=[0], mem=m)


def cases():
    out = [news(True), news(False)]
    out += [worldcup(w) for w in range(4)]
    out += [competition_table(True), competition_table(False), transfer_manager(True), transfer_manager(False)]
    out += [virtual_staff(True, False), virtual_staff(False, False), virtual_staff(True, True), virtual_staff(False, True)]
    out += [transfer_offer(True), transfer_offer(False), transfer_offer_type(True, True), transfer_offer_type(False, True), transfer_offer_type(False, False)]
    out += [squad_manager_case(True), squad_manager_case(False), staff_records_club(True), staff_records_club(False)]
    out += [staff_records_squad(True), staff_records_squad(False)]
    out += [row_counter(1, True), row_counter(1, False), row_counter(2, True), row_counter(2, False)]
    out += [arg_averages(5, 12), arg_averages(3, 12), arg_averages(5, 10, second_club=99)]
    return out


# ---- The build ----

def stubs(image):
    """Every function that a guarded function calls, with the bytes its ret pops."""
    an = Analyzer(image, set(int(x, 16) for x in open(os.path.join(ROOT, "tools", "recomp", "functions.txt")).read().split()))
    real = set(FUNCTIONS) | {FTOL}
    out = {VFUNC: 8}
    for e in FUNCTIONS:
        for c in an.function(e).calls:
            if c in real or c in out:
                continue
            pops = {i.operands[0].imm if i.operands else 0 for i in an.function(c).insns.values() if i.mnemonic == "ret"}
            if len(pops) != 1:
                raise SystemExit(f"{c:#x}: cannot tell what its ret pops ({pops})")
            out[c] = pops.pop()
    return out


def build(exe, work, stub_pops):
    rt = os.path.join(work, "guards_stubs.c")
    with open(rt, "w") as f:
        f.write('#include "rt.h"\nvoid guard_stub(uint32_t addr, uint32_t pop);\n')
        for a, pop in sorted(stub_pops.items()):
            f.write(f"RT_REPLACE({a:08x}, stub_{a:08x}) {{ guard_stub(0x{a:x}u, {pop}u); }}\n")
    entries = ",".join("0x%x" % a for a in sorted(set(FUNCTIONS) | {FTOL}))
    cmd = [sys.executable, os.path.join(ROOT, "tools", "recomp", "recomp.py"), "--exe", exe, "--out", work,
           "--functions", os.path.join(ROOT, "tools", "recomp", "functions.txt"), "--entries", entries,
           "--no-follow", "--runtime", rt, "--compile", os.path.join(HERE, "guards_driver.c")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("the build failed:\n" + r.stdout[-2000:] + r.stderr[-4000:])
    return os.path.join(work, "program")


# ---- The two runs ----

def case_file(case, path):
    w = [case.entry, case.ecx, 1 if case.fpu else 0, len(case.args)] + case.args
    blocks = case.blocks()
    w.append(len(blocks))
    data = struct.pack(f"<{len(w)}I", *w)
    for a, d in blocks:
        pad = d + b"\0" * (-len(d) % 4)
        data += struct.pack("<II", a, len(d)) + pad
    q = struct.pack("<I", len(case.queues))
    for a, values in sorted(case.queues.items()):
        q += struct.pack(f"<II{len(values)}I", a, len(values), *values)
    with open(path, "wb") as f:
        f.write(data + q)


def run_recompiled(program, exe, case, work):
    path = os.path.join(work, "case.bin")
    case_file(case, path)
    r = subprocess.run([program, exe, path, work], capture_output=True, text=True)
    if r.returncode:
        return None, r.stderr.strip()[-500:]
    words = r.stdout.split()
    regs = (int(words[1], 16), int(words[3], 16), int(words[5], 16))
    mem = {n: open(os.path.join(work, n + ".bin"), "rb").read() for n in ("image", "stack", "work", "page0")}
    return (regs, mem), None


class GuardRef(Reference):
    def __init__(self, exe, stub_pops, queues):
        self.stub_pops, self.queues = stub_pops, {a: list(v) for a, v in queues.items()}
        super().__init__(exe, ".")
        uc = self.uc
        uc.mem_map(WORK, WORK_SIZE)
        uc.mem_map(RESULT, 0x1000)
        uc.mem_write(0, b"\xcc" * 0x1000)
        self.acted = set()
        for a in GUARDS:
            uc.hook_add(UC_HOOK_CODE, self._guard, begin=a, end=a)

    def replacements(self):
        return {}

    def install_stubs(self):
        for a, pop in self.stub_pops.items():
            self.uc.mem_write(a, b"\xc2" + struct.pack("<H", pop) if pop else b"\xc3")
            self.uc.hook_add(UC_HOOK_CODE, self._stub, begin=a, end=a)

    def _stub(self, uc, address, size, user_data):
        esp = uc.reg_read(UC_X86_REG_ESP)
        n = self.u32(LOG)
        e = LOG + 4 + 16 * n
        self.write(e, struct.pack("<IIII", address, uc.reg_read(UC_X86_REG_ECX), self.u32(esp + 4), self.u32(esp + 8)))
        self.write_u32(LOG, n + 1)
        q = self.queues.get(address)
        uc.reg_write(UC_X86_REG_EAX, q.pop(0) if q else 0)

    def _guard(self, uc, address, size, user_data):
        if EMULATE[address](uc):
            self.acted.add(address)


def run_unicorn(exe, stub_pops, case):
    r = GuardRef(exe, stub_pops, case.queues)
    r.install_stubs()
    r.set_control_word(0x27F)
    for a, d in case.blocks():
        r.write(a, d)
    eax = r.call(case.entry, *case.args, this=case.ecx)
    esp = r.uc.reg_read(UC_X86_REG_ESP)
    stack = r.read(STACK_BASE, STACK_SIZE)
    st0 = 0
    if case.fpu:
        r.write(RESULT + 0x100, b"\xdd\x1d" + struct.pack("<I", RESULT) + b"\xc3")   # fstp qword [RESULT]; ret
        r.call(RESULT + 0x100)
        st0 = struct.unpack("<Q", r.read(RESULT, 8))[0]
    image = bytearray(r.read(r.image.base, r.image.size))
    for a in stub_pops:          # put back the bytes under the stub rets
        image[a - r.image.base:a - r.image.base + 3] = r.image.read(a, 3)
    mem = {"image": bytes(image), "stack": stack, "work": r.read(WORK, WORK_SIZE),
           "page0": r.read(0, 0x1000)}
    return (eax, esp, st0), mem, r.acted


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    exe = argv[1]
    work = argv[2] if len(argv) > 2 else tempfile.mkdtemp(prefix="recomp-guards-")
    os.makedirs(work, exist_ok=True)
    image = Image(exe)
    stub_pops = stubs(image)
    program = build(exe, work, stub_pops)
    print(f"built {program} with {len(stub_pops)} stubs")

    failed = 0
    tested = set()
    for case in cases():
        mine, err = run_recompiled(program, exe, case, work)
        try:
            regs, theirs, acted = run_unicorn(exe, stub_pops, case)
        except RuntimeError as exc:
            regs, theirs, acted = None, None, set()
            err = (err or "") + f" unicorn: {exc}"
        problems = []
        if mine is None or theirs is None:
            problems.append(err)
        else:
            my_regs, my_mem = mine
            if my_regs != regs:
                problems.append(f"eax/esp/st0 {tuple(hex(x) for x in my_regs)} != original {tuple(hex(x) for x in regs)}")
            for name in ("page0", "stack", "work", "image"):
                if my_mem[name] != theirs[name]:
                    at = next(i for i, (x, y) in enumerate(zip(my_mem[name], theirs[name])) if x != y)
                    problems.append(f"{name} differs at +{at:#x}")
            if theirs["page0"] != b"\xcc" * 0x1000:
                problems.append("page 0 was written")
            guards = set(FUNCTIONS[case.entry])
            if case.null and not acted & guards:
                problems.append("the NULL path did not reach a guard")
            if not case.null and acted & guards:
                problems.append("a guard acted on a good pointer")
            tested |= acted
        ok = not problems
        failed += not ok
        print(f"{'ok   ' if ok else 'FAIL '} {case.name}" + ("" if ok else ": " + "; ".join(problems)))
    missing = set(GUARDS) - tested
    if missing:
        print("FAIL  no NULL case reached " + ", ".join("%#x" % a for a in sorted(missing)))
        failed += 1
    print("PASS" if not failed else f"FAIL ({failed})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
