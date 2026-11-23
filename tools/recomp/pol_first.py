"""Club slot counts of PolFirst that become run-time values for the Ekstraklasa in one table (GitHub issue 92).
Runtime: src/recomp_rt/win/season_pol_first.c.

The constructor PolFirst::PolFirst 0x7c8a80 sorts the club slots +0xee (qsort count 16, push 0x10 at 0x7c8ce5, the
object in esi) and sets a predicted position for each (cmp di, 0x10 at 0x7c8d09). set_predicted_positions 0x7ca1e0
(vtable +0x5c) does the same with the object in edi (push 0x10 at 0x7ca264, cmp si, 0x10 at 0x7ca288). In one table
the slots hold every club of the division, 14 to 18; the original's form keeps 16.

The exchange of each season with the I liga: PolSecond::create_play_off_stage 0x7cc290 makes a Cup_Stage of 4 clubs
(push 4 at 0x7cc3d1, the object in esi), 2 when 1 pair plays; the chain 0x7ca490 moves the clubs in pairs (push 1 at
0x7ca4cc, the object in esi), so the sizes stay; 0x7ca030 takes 4 clubs of the third tier (cmp si, 4 at 0x7ca132,
push 4 at 0x7ca13c, mov edi, 4 at 0x7ca14a; the object in ebx).

The final table: Club_History::record_comp_season 0x442e80 records the Ekstraklasa only for the stage of index 1
(cmp byte [esi+0x44], 1 at 0x442f95) and adds the 14 matches of the groups to played (add byte [esp+0x3f], 0xe at
0x44301e). The league table screen adds 14, or 7 in the home and away views, for the stage of index 1 (cmp byte
[eax+0x44], 1 at 0x49215b; add eax, 0xe at 0x49216d; add edx, 7 at 0x49217c). In one table the final table is the
object itself, in the split the stage of index 0 after 30 rounds.
"""

PATCHES = {
    0x7C8CE5: (0x10, "rt_pol_first_slots(esi, 0x10u)"),
    0x7C8D09: (0x10, "rt_pol_first_slots(esi, 0x10u)"),
    0x7CA264: (0x10, "rt_pol_first_slots(edi, 0x10u)"),
    0x7CA288: (0x10, "rt_pol_first_slots(edi, 0x10u)"),
    0x7CC3D1: (0x4, "rt_pol_playoff_clubs(esi, 0x4u)"),
    0x7CA4CC: (0x1, "rt_pol_first_pairwise(esi, 0x1u)"),
    0x7CA132: (0x4, "rt_pol_lower_intake(ebx, 0x4u)"),
    0x7CA13C: (0x4, "rt_pol_lower_intake(ebx, 0x4u)"),
    0x7CA14A: (0x4, "rt_pol_lower_intake(ebx, 0x4u)"),
    0x442F95: (0x1, "rt_pol_history_index(esi, 0x1u)"),
    0x44301E: (0xE, "rt_pol_played_before(esi, 0xEu)"),
    0x49215B: (0x1, "rt_pol_split_index(eax, 0x1u)"),
    0x49216D: (0xE, "rt_pol_played_before(rd32(g_mem, esp + 0x121cu), 0xEu)"),
    0x49217C: (0x7, "rt_pol_played_before(rd32(g_mem, esp + 0x121cu), 0x7u)"),
}
