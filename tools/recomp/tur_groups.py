"""Club counts of TurSecondB that become run-time values for the Turkish Category B in three groups (Summer 2002,
spec docs/specs/2026-10-24-summer-2002-db-spec.md slice 5). Runtime: src/recomp_rt/win/season_tur_groups.c.
Evidence: docs/research/2026-10-24-summer-2002-turkish-groups.md.

TurSecondB::create_table 0x9002c0 sizes its table for 10 rows (push 0x24e at 0x9002cd, the object in edi from
0x9002c9) and stores 10 at 0x9002d2. create_group_stage 0x900780 (the object in esi from 0x90079e) allocates 10 club
pointers (push 0x28 at 0x900812), takes clubs while it has fewer than 10 (cmp ax, 0xa at 0x90089d), wants 10
(cmp word ptr [esp + 0x10], 0xa at 0x900932) and makes a League_Stage of 10 (push 0xa at 0x9009e4). The runtime gives
the class's 10 for every database but Summer 2002, and the size of the group in its three-group form.
"""

PATCHES = {
    0x9002CD: (0x24E, "rt_tur_table_bytes(edi, 0x24eu)"),
    0x900812: (0x28, "rt_tur_stage_bytes(esi, 0x28u)"),
    0x90089D: (0xA, "rt_tur_stage_clubs(esi, 0xau)"),
    0x900932: (0xA, "rt_tur_stage_clubs(esi, 0xau)"),
    0x9009E4: (0xA, "rt_tur_stage_clubs(esi, 0xau)"),
}

INSNS = {
    0x9002D2: ("mov word ptr [edi + 0x3e], 0xa", "W16(edi + 0x3eu, rt_tur_table_clubs(edi, 0xau));"),
}
