"""Read-only isolated x86 routine probe. Writes only emulator memory, never game files."""
import sys
import struct
sys.path.insert(0, '/Users/mikepenzin/Documents/Projects/cm0102-web/tools/recomp')
from pe import Image
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_ECX

p = Image('/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game/cm0102.exe')
print('Original digest verified; creating isolated Unicorn instance', flush=True)
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(p.base, (p.size + 4095) // 4096 * 4096)
for _, va, vs, rp, rs, fl in p.sections:
    if rs:
        u.mem_write(va, p.data[rp:rp + rs])
u.mem_map(0x20000000, 0x10000)
u.mem_map(0x30000000, 0x10000)

def w32(a, x): u.mem_write(a, struct.pack('<I', x))
def w16(a, x): u.mem_write(a, struct.pack('<h', x))
def b(a, x): u.mem_write(a, bytes([x & 255]))

L = 0x20000000
A = L + 0x1000
B = A + 0x100
CA = L + 0x2000
CB = CA + 0x100
F = L + 0x3000
STOP = 0x30000000
SP = STOP + 0x8000
w32(0xb679fc, L)
w32(0xb67a00, 0)

def call(addr, args, ecx=0):
    u.mem_write(SP, struct.pack('<' + 'I' * (len(args) + 1), STOP, *args))
    u.reg_write(UC_X86_REG_ESP, SP)
    u.reg_write(UC_X86_REG_ECX, ecx)
    u.emu_start(addr, STOP, count=10000)
    r = u.reg_read(UC_X86_REG_EAX)
    return r if r < 2**31 else r - 2**32

def reset(rule):
    u.mem_write(L, bytes(0x4000))
    w32(A, CA); w32(B, CB); w32(CA, 10); w32(CB, 20)
    b(A + 0x11, 5); b(B + 0x11, 5); w16(L + 0xa7, 38)
    u.mem_write(L + 0xc5, bytes(rule + [0] * (4 - len(rule))))

checks = []
def check(label, got, want):
    checks.append((label, got, want))
    assert got == want, (label, got, want)
    print('PASS', label, got, flush=True)

print('Mapped original into emulator; beginning comparison vectors', flush=True)
reset([1, 2]); w16(A + 0x19, 12); w16(B + 0x19, 10)
check('points descending', call(0x683210, [A, B]), -2)
reset([1, 2]); w16(A + 0x15, 300); w16(A + 0x17, 250); w16(B + 0x15, 250); w16(B + 0x17, 210)
check('goal difference signed16', call(0x683210, [A, B]), -10)
reset([2]); w16(A + 0x15, 300); w16(B + 0x15, 290)
check('goals for signed16', call(0x683210, [A, B]), -10)
reset([3]); b(A + 0x12, 4); b(B + 0x12, 2)
check('criterion3 wins offset12', call(0x683210, [A, B]), -2)
reset([4]); b(A + 4, 2); b(B + 4, 4)
check('criterion4 scratch key descending', call(0x683210, [A, B]), 2)
reset([5]); w16(A + 0x31, 12); w16(B + 0x31, 7)
check('criterion5 offset31', call(0x683210, [A, B]), -5)
reset([6]); b(A + 4, 2); b(B + 4, 4)
check('criterion6 scratch key ascending', call(0x683210, [A, B]), -2)
reset([2]); b(A + 0x11, 0); b(B + 0x11, 0); w16(A + 0x15, 2)
check('unplayed rows use club ID', call(0x683210, [A, B]), -10)
reset([0])
check('fallback club ID', call(0x683210, [A, B]), -10)

for label, reg, et, pen, flags, want in [
    ('regulation win', (2, 1), (0, 0), (0, 0), 0, (3, 0)),
    ('regulation draw', (1, 1), (0, 0), (0, 0), 0, (1, 1)),
    ('extra-time win enabled', (1, 1), (2, 1), (0, 0), 2, (3, 0)),
    ('extra-time ignored', (1, 1), (2, 1), (0, 0), 0, (1, 1)),
    ('penalty win enabled', (1, 1), (0, 0), (5, 4), 1, (1, 0)),
    ('penalty ignored', (1, 1), (0, 0), (5, 4), 0, (1, 1)),
]:
    reset([0]); b(L + 0xc2, 3); b(L + 0xc3, 1); w16(L + 0xab, flags)
    w32(F + 0x1c, CA); w32(F + 0x20, CB)
    for off, pair in [(0x43, reg), (0x47, et), (0x49, pen)]:
        u.mem_write(F + off, bytes(pair))
    got = tuple(call(0x68aad0, [club, F], L) & 255 for club in [CA, CB])
    check(label, got, want)

print('TOTAL', len(checks), flush=True)
