from pathlib import Path
import struct, itertools, json, hashlib
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import *

executable = Path('/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game/cm0102.exe')
b = executable.read_bytes()
assert hashlib.sha256(b).hexdigest() == '0c78e7da6d0bcb4603fa179449bbe6d1b6b5e1c627a22c445d14d489bd9bff86'
pe = struct.unpack_from('<I', b, 0x3c)[0]
n = struct.unpack_from('<H', b, pe + 6)[0]
opt = struct.unpack_from('<H', b, pe + 20)[0]
base = struct.unpack_from('<I', b, pe + 52)[0]
image_size = struct.unpack_from('<I', b, pe + 24 + 56)[0]
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(base, (image_size + 0xfff) & ~0xfff)
for i in range(n):
    s = pe + 24 + opt + i * 40
    size, va, rawsize, raw = struct.unpack_from('<4I', b, s + 8)
    u.mem_write(base + va, b[raw:raw + rawsize])
u.mem_map(0x2000000, 0x10000)
u.mem_map(0x3000000, 0x10000)
u.mem_map(0x4000000, 0x1000)
obj, stack, stop = 0x3000000, 0x2008000, 0x4000000

def wr(a, v):
    u.mem_write(a, struct.pack('<I', v & 0xffffffff))

def invoke(addr, args=(), ecx=obj):
    u.reg_write(UC_X86_REG_ESP, stack)
    u.reg_write(UC_X86_REG_ECX, ecx)
    u.reg_write(UC_X86_REG_EAX, 0xa5a50000)
    u.reg_write(UC_X86_REG_EFLAGS, 2)
    wr(stack, stop)
    for i, v in enumerate(args):
        wr(stack + 4 + i * 4, v)
    u.emu_start(addr, stop, count=2000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    return u.reg_read(UC_X86_REG_EAX)

for status, sub in itertools.product(range(256), repeat=2):
    u.mem_write(obj + 0x2c, bytes([status]))
    u.mem_write(obj + 0x2e, bytes([sub]))
    before = bytes(u.mem_read(obj, 0x50))
    r = invoke(0x8d0d90, [obj])
    assert r == int(status in [2, 3, 4, 16, 20] or (status == 15 and sub >= 3)), (status, sub, r)
    assert bytes(u.mem_read(obj, 0x50)) == before

for flag in range(256):
    u.mem_write(obj + 0x35, bytes([flag]))
    assert invoke(0x85f9f0) & 255 == int(flag & 63 == 3)

for a, b, c in itertools.product([0, 1, 2, 255], repeat=3):
    for off, val in [(0x1c, a), (0x1d, b), (0x1f, c)]:
        u.mem_write(obj + off, bytes([val]))
    assert invoke(0x867b50) == int(a == 2 or b == 1 or c == 2)

# Primary ordinary; primary state3 mutation; absent; ordered duplicate identifiers.
u.mem_write(obj, bytes(256))
wr(obj + 0xc, 123)
u.mem_write(obj + 0x10, bytes([7]))
assert invoke(0x8e7f10, [123]) & 255 == 7
u.mem_write(obj + 0x2c, bytes([3]))
assert invoke(0x8e7f10, [123]) & 255 == 4
assert bytes(u.mem_read(obj + 0x10, 1)) == bytes([4])
assert invoke(0x8e7f10, [456]) & 255 == 4
lst = obj + 0x200
nodes = [obj + 0x300, obj + 0x310]
values = [obj + 0x400, obj + 0x410]
wr(obj + 4, lst)
wr(lst + 2, nodes[0])
wr(lst + 10, 0)
for i in range(2):
    wr(nodes[i], values[i])
    wr(nodes[i] + 6, nodes[i + 1] if i == 0 else 0)
    wr(values[i], 456)
    u.mem_write(values[i] + 4, bytes([6 + i]))
assert invoke(0x8e7f10, [456]) & 255 == 6
assert struct.unpack('<I', u.mem_read(lst + 10, 4))[0] == nodes[1]

# No-list aggregation: signed optional terms, percent flag, signed byte, arithmetic overflow.
staff = obj + 0x1000
wr(0xae23bc, staff)
cases = [
    (100, 20, 30, 10, 12345, 0, 0, 150),
    (100, 20, 30, 10, 12345, 0, 1, 1384),
    (100, -20, 30, 10, 12345, 0, 0, 130),
    (100, 20, 30, 10, 12345, 1, 1, 100),
    (100, 20, 30, 200, 12345, 0, 1, 150),
    (0, 0, 0, 100, 2147483647, 0, 1, -1),
    (2147483647, 1, 0, 0, 0, 0, 0, -2147483648),
]
for initial, c1, c2, pct, value, arg0, arg3, expected in cases:
    u.mem_write(obj, bytes(256))
    wr(obj + 0xc, 0)
    wr(obj + 0x1b, initial)
    wr(obj + 0x35, c1)
    wr(obj + 0x3a, c2)
    u.mem_write(obj + 0x3f, bytes([pct]))
    wr(staff + 0x52, value)
    r = invoke(0x8e7a70, [arg0, 0, 0, arg3])
    r = r if r < 2**31 else r - 2**32
    assert r == expected, (r, expected)

print(json.dumps({
    'engine': 'Unicorn x86 32-bit, original PE instructions, synthetic memory only',
    'liveGameRun': False,
    'originalSha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
    'closedPredicateCases': 65536,
    'expiredPredicateCases': 256,
    'releaseClausePredicateCases': 64,
    'talkStatusCases': 4,
    'aggregationCases': len(cases),
    'totalCases': 65536 + 256 + 64 + 4 + len(cases),
    'allPassed': True,
}))
