"""Crash guards (0.5.0 slice 1, #46): NULL tests at the known crash sites of the original.

The original stops at these sites when a pointer is NULL, because Windows does not map page 0. The port maps
page 0 (src/recomp_rt/host/posix.c, win32.c), so without a guard the game goes on with wrong data, and a NULL
write puts bytes at a low address that later NULL reads see. Each guard tests the pointer just before the
instruction that reads or writes through it. If the pointer is NULL, the guard goes to the exit that the
crash research lists (docs/research/2026-10-16-crash-fixes.md, section 1a). If the pointer
is not NULL, the instruction runs as before: a game that does not reach the fault takes the same path. A guard
does not change the flags.

Each entry names the address of one instruction, its text, the NULL test and the C code that runs when the test
is true. The translation checks the text and the build stops if it does not match (translate.py). A guard sits
at the first instruction where the registers are the same as at the jump of the patch, so the exit sees the same
state. tests/recomp/test_crashguards.py runs each guarded function with a NULL pointer and with a good pointer.

Left out: the guard at 0x5f74ac (issue 132 found no path to it; original_faults.py logs a hit there) and every
patch that changes a rule.

Issue 132 (0.8.0) added the stage bound of the Argentine average points at 0x409e25. It is not a NULL test: it
stops a loop before it reads an object of the wrong type (see the entry).
"""

# address: (instruction text, NULL test, C code when the test is true)
GUARDS = {
    # Null News Item: the text renderer 0x4aeaf0 takes the length of its text, [[this]], with repne scasb.
    # Patch: if the text is NULL, skip the scan (ecx = -1, eax = 0 already). 0x4aeb29 then returns 0.
    0x4AEB0F: ("repne scasb al, byte ptr es:[edi]", "edi == 0", "goto L_4aeb11;"),

    # World Cup NULL pointer: the compare 0x4b07b0 of two teams by [team+0x80], for a sort. Patch: a NULL
    # slot gets the other team, so the two compare equal. Both NULL: the patch loops for ever; the guard
    # returns 0 (equal) and writes nothing.
    0x4B07BA: ("movsx edx, word ptr [ecx + 0x80]", "ecx == 0 || R32(eax) == 0",
               "if (ecx == 0 && R32(eax) == 0) { eax = 0; esp += 4; SYNC(); return; } "
               "if (ecx == 0) { ecx = R32(eax); W32(R32(esp + 4), ecx); } else W32(eax, ecx);"),

    # Competition table: the loop of 0x48ce10 over the competition table [0xadadfc] calls virtual +0x10 of each
    # competition. Patch: a NULL competition ends the loop with 1 (drop the two pushed arguments).
    0x48CE46: ("mov edx, dword ptr [esi]", "esi == 0", "esp += 8; goto L_48ce56;"),

    # transfer_manager: 0x84aeb0 reads [arg]. Patch: a NULL argument goes to the exit.
    0x84AEBB: ("mov eax, dword ptr [eax]", "eax == 0", "rt_crash_site(0x84aebbu); goto L_84aef1;"),

    # virtual_staff 195: 0x85d8a0 reads a byte of [[[arg+0x20]+0x53]]. Patch: use 0. The patch also clobbers
    # the flags of the cmp at 0x85d8af that jne 0x85d8ed reads, on every call; the guard keeps them.
    0x85D8B8: ("movsx eax, byte ptr [edx + 0x7e]", "edx == 0", "rt_crash_site(0x85d8b8u); eax = 0; goto L_85d8bc;"),

    # Transfer Offer (CM84): 0x8e7b50 reads [[staff+0x61]+0xb]. Patch: no player data, skip the value.
    0x8E7BEF: ("mov dx, word ptr [ecx + 0xb]", "ecx == 0", "goto L_8e7c2d;"),

    # transfer_offer type test: 0x8e8800 reads [[staff+0x53]]. Patch: skip the type test.
    0x8E8827: ("mov ecx, dword ptr [eax]", "eax == 0", "goto L_8e8845;"),

    # squad_manager: 0x85a470 reads [[staff+0x61]+0xf]. Patch: the existing skip path.
    0x85A556: ("cmp byte ptr [eax + 0xf], 0xf", "eax == 0", "goto L_85a745;"),

    # staff_records: a part-exchange player whose contract ended has club [staff+0x39] NULL.
    # 0x86cd70 reads [club]; patch: skip to the next player.
    0x86D609: ("mov edx, dword ptr [edx]", "edx == 0", "goto L_86d6ae;"),
    # 0x8c9390 searches the 50 squad slots of the club for the player; patch: skip the search (al = 0).
    0x8C94CA: ("movsx ebx, al", "edx == 0", "goto L_8c94ec;"),

    # Row counter: 0x8c39f0 adds 1 to a counter of the row that bsearch 0x946464 finds. Not found
    # (or the paths at 0x8c3c7a, 0x8c3c83, 0x8c3dda and 0x8c3dec, which set eax to 0) writes through NULL.
    # Patch: skip the increment. For the second site the patch adds 1 at +4 instead of +6; the guard does not.
    0x8C3CAD: ("inc word ptr [eax + 4]", "eax == 0", "goto L_8c3cb1;"),
    0x8C3E16: ("inc word ptr [eax + 6]", "eax == 0", "goto L_8c3e1a;"),

    # Argentine second division average points (issue 132). Not a NULL test. ArgSecond::update_relegation_averages 0x409c20 adds the points of
    # its stages [this+0xc][i], i < [this+0x30], to the averages table. The stage index is in eax (0x409d41), and
    # the club search reuses eax (0x409d5b, 0x409d77, 0x409d9e) without saving it, so the next stage is the
    # averages index of the last club found, plus 1 (0x409e24). Stages 0 to 2 are the League_Stage zones (ctor
    # 0x68b100, vtable 0x96d0f0, made at 0x408417, 0x4084bf, 0x408567). After the promotion play-offs start
    # ([this+0x30] = 5 at 0x408ca8) stages 3 to 5 are Cup_Stage objects of 0xb2 bytes (vtable 0x969468, ctor
    # 0x522e00), which set only a byte at +0xb1. The loop then reads the dword at +0xb1 as a table of rows
    # (0x409d5e) and goes through it (0x409d80, 0x409d85): rows from junk, a crash in the original when the junk
    # is not a readable address.
    # The loop now ends at the first stage that is not a League_Stage. A loop that reads only zones runs as before.
    # A change of jle to jmp at 0x409d7e would change the points of every Argentine game instead.
    0x409E25: ("movsx edx, ax",
               "(int32_t)(int16_t)eax < (int32_t)ecx && "
               "R32(R32(R32(esi + 0xcu) + 4u * (uint32_t)(int32_t)(int16_t)eax)) != 0x96d0f0u",
               "rt_crash_site(0x409e25u); goto L_409e30;"),
}
