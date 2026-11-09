import sys
import struct
import itertools
import json
from pathlib import Path
sys.path.insert(0, '/Users/mikepenzin/Documents/Projects/cm0102-web/tools/recomp')
from pe import Image
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_FPCW

im = Image('/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game/cm0102.exe')
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(im.base, (im.size + 4095) & ~4095)
for name, va, vs, off, rs, flags in im.sections:
    if rs:
        u.mem_write(va, im.data[off:off + rs])
base, stack, stop = 0x50000000, 0x51000000, 0x52000000
u.mem_map(base, 0x1000)
u.mem_map(stack, 0x10000)
u.mem_map(stop, 0x1000)
u.reg_write(UC_X86_REG_FPCW, 0x27f)
mismatches, examples = [], []
for vals in itertools.product([0, 10, 25, 50], repeat=5):
    u.mem_write(base + 4, bytes(vals))
    u.mem_write(stack + 0x8000, struct.pack('<I', stop))
    u.reg_write(UC_X86_REG_ECX, base)
    u.reg_write(UC_X86_REG_ESP, stack + 0x8000)
    u.emu_start(0x8b4f60, stop, count=1000)
    got = u.reg_read(UC_X86_REG_EAX) & 0xffff
    if got >= 0x8000:
        got -= 65536
    f, t, s, k, g = vals
    want = min(100, int(g * .5) - int(f * -1.3) - int(k * -.5) - int(s * -.5) - int(t * -.5))
    if got != want:
        mismatches.append({'levels': vals, 'actual': got, 'expected': want})
    if vals in [(25,25,25,25,25), (50,50,50,50,50), (0,0,0,0,0), (25,0,0,0,0)]:
        examples.append({'levels': vals, 'result': got})
report = {
    'artifactSha256': '0c78e7da6d0bcb4603fa179449bbe6d1b6b5e1c627a22c445d14d489bd9bff86',
    'entry': '0x8b4f60',
    'method': 'Unicorn isolated original-function execution; synthetic schedule; x87 FPCW=0x27f; no native app launched',
    'cases': 1024,
    'mismatches': mismatches,
    'examples': examples,
}
Path('/private/tmp/cm0102-training-load-probe.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report))
