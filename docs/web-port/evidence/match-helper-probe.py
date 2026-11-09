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
base, staff, stack, stop = 0x50000000, 0x50010000, 0x51000000, 0x52000000
u.mem_map(base, 0x20000)
u.mem_map(stack, 0x10000)
u.mem_map(stop, 0x1000)
u.reg_write(UC_X86_REG_FPCW, 0x27f)

def call(addr, args=()):
    u.mem_write(stack + 0x8000, struct.pack('<' + 'I'*(1+len(args)), stop, *args))
    u.reg_write(UC_X86_REG_ECX, base)
    u.reg_write(UC_X86_REG_ESP, stack + 0x8000)
    u.emu_start(addr, stop, count=1000)
    if u.reg_read(UC_X86_REG_ESP) != stack + 0x8004 + 4*len(args):
        raise AssertionError('Unexpected stack cleanup')

def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]

mismatches, reaction_count = [], 0
u.mem_write(base + 0x69, struct.pack('<I', staff))
for temperament, value, factor in itertools.product(range(1,21), [0.,1.,10.,35.,50.,99.9,100.,150.], [0.,.5,1.,1.25,2.]):
    value, factor = f32(value), f32(factor)
    u.mem_write(staff + 0x5d, bytes([temperament]))
    u.mem_write(base + 0x1a2, struct.pack('<f', value))
    call(0x716450, [struct.unpack('<I', struct.pack('<f', factor))[0]])
    got = struct.unpack('<f', u.mem_read(base + 0x1a2, 4))[0]
    want = f32(min(100., max(value*factor, 2*(25-temperament))))
    reaction_count += 1
    if got != want:
        mismatches.append({'type':'reaction','temperament':temperament,'value':value,'factor':factor,'got':got,'want':want})

foul_count = 0
for args in [(2, staff, staff+0x100, 7, 1, 2, 3, 4, 5), (65535, 0, 0, 0, 255, 255, 255, 255, 255), (32768, staff+0x300, staff+0x400, 255, 0, 11, 0, 2, 3)]:
    start=base+0x475a
    initial=bytearray([0xa5]*16)
    initial[7:11]=bytes(4)
    u.mem_write(start, bytes(initial))
    call(0x6bb9a0, args)
    expected=bytearray(initial)
    expected[5:7]=struct.pack('<H', args[0]&65535)
    expected[7:11]=struct.pack('<I',args[1])
    expected[11:15]=struct.pack('<I',args[2])
    expected[15]=args[3]&255
    expected[0:5]=bytes(x&255 for x in args[4:9])
    if u.mem_read(start,16) != expected:mismatches.append({'type':'foul insertion','args':args})
    foul_count+=1
    if args[1]:
        before=bytes(u.mem_read(start,16))
        call(0x6bb9a0,(0,staff+100,staff+200,0,0,0,0,0,0))
        if bytes(u.mem_read(start,16))!=before:mismatches.append({'type':'occupied foul overwritten'})
        foul_count+=1
    call(0x6bba10)
    expected=bytes([0,255,255,255,255,255,255]+[0]*9)
    if bytes(u.mem_read(start,16))!=expected:mismatches.append({'type':'foul reset'})
    foul_count+=1

report={'artifactSha256':'0c78e7da6d0bcb4603fa179449bbe6d1b6b5e1c627a22c445d14d489bd9bff86','method':'Unicorn isolated original functions, synthetic records, x87 FPCW 0x27f; no native app launched','entries':['0x716450','0x6bb9a0','0x6bba10'],'reactionCases':reaction_count,'foulChecks':foul_count,'mismatches':mismatches,'limits':'Only synthetic finite ordinary inputs; no caller semantics, NaN, invalid pointers, full match or application validated.'}
Path('/private/tmp/cm0102-match-helper-probe.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report))
