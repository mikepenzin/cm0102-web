"""The Croatian second tier at 18 clubs (0.7.2 league sizes, slice 11, D22).

Instruction rows (address: (the instruction in the original, C code)) for league_sizes.INSNS. The runtime is
src/recomp_rt/win/season_sizes_cro.c. CroA2a::create_tiebreak_playoff_stage 0x5160f0 puts the tied clubs in a
stack array of 16 pointers at esp+0x1c; a table of 18 can tie 9 pairs. With a table of more than 16 rows in a
season game the stores and the list that Cup_Stage::Cup_Stage 0x522e00 reads go to a list of the runtime; else the
stack array of the original (Q152).

The regroup of the second tier, 0x515190 (called by CroA1::start_new_season at 0x5149c2), sorts the clubs of the
North and South groups in a stack array of 32 at esp+0x14 and gives the first 16 the North, for 32 slots. In a season
game it uses a list of the runtime, walks the clubs it found, and gives the North the count of its row when the South
is not held (2006/07 on). With one group the order is by results and the South does not go up to the First
Division (gap row 40); with two groups (2002/03 to 2005/06) each takes its row (gap row 38).

The Greek divisions at a new game. create_game 0x828110 calls fix_division_club_count 0x839920 (record id, count) for
the National A [0x9cf86c] and National B [0x9cf870] divisions with 14 (0x8283c0, 0x8283ce), when Greece is selected
(0x8283b0). It keeps the first count clubs of the division, sends the others to A Lower Division (0x83995a..0x839978)
and fills a short division from the nation's clubs of A Lower Division, A Premier Division or a lower rank
(0x839a19..0x839a5e). In a season game the count is the row of the start season, so the start season plays its real
count (the club moves of season_clubs.c run before, at 0x524d70).
"""

def _cmp(width_mask, size, left, right):
    return (f"t0 = {left}; t1 = ({right}) & {width_mask}; t2 = ((t0 - t1 - 0) & {width_mask}); "
            f"SETF(K_SUB | {size}, t0, t1, t2, 0);")


def _skip_if_empty(ret, pops, value):
    """A call of the South's set-up left out when its table has no row (rt_cro_south_empty): drop its stack
    arguments (callee cleanup) and give eax the value of a success."""
    return (f"if (rt_cro_south_empty(esi)) {{ esp += {pops}u; eax = {value}u; }} else {{ esp -= 4; W32(esp, {ret}u); "
            "SYNC(); @FN@; LOAD(); RT_AFTER_CALL(); }")


INSNS = {
    # Open item 255: the South of a new game in a season of one group has no club (season_sizes_cro.c). Its set-up
    # leaves out the calls that need rows: the prize fund (League::calc_prize_fund 0x682f70, one argument) and the TV
    # money (League::pay_tv_rights 0x683010, two) of CroA2b::create_table, and the fixtures (League::create_fixtures
    # 0x6835c0, none, 1 on success) and the sort (League::sort_table 0x6827d0, one, 1 on success) of the constructor
    # CroA2b::CroA2b 0x516430.
    0x516ED3: ("call 0x682f70", _skip_if_empty("0x516ed8", 4, 0).replace("@FN@", "f_00682f70()")),
    0x516EEB: ("call 0x683010", _skip_if_empty("0x516ef0", 8, 0).replace("@FN@", "f_00683010()")),
    0x516525: ("call 0x6835c0", _skip_if_empty("0x51652a", 0, 1).replace("@FN@", "f_006835c0()")),
    0x516561: ("call 0x6827d0", _skip_if_empty("0x516566", 4, 1).replace("@FN@", "f_006827d0()")),
    # The same calls of CroA2b::start_new_season 0x516fc0, which runs with changing rules off (no before hook keeps the
    # South waiting then): the fixtures at 0x5170a1 and the sort at 0x5170dd (push edi, 0).
    0x5170A1: ("call 0x6835c0", _skip_if_empty("0x5170a6", 0, 1).replace("@FN@", "f_006835c0()")),
    0x5170DD: ("call 0x6827d0", _skip_if_empty("0x5170e2", 4, 1).replace("@FN@", "f_006827d0()")),
    # The chain 0x5150f0: the South's exchange with the First Division (league_promote_and_relegate 0x689c80, six
    # arguments, ret 0x18). None when the season that ends had one group (season_sizes_cro.c rt_cro_south_swap).
    # The chain's record_final_positions of the South (vtable +0xa4, thiscall, no arguments): none after a season of
    # one group, when the South kept its old table (rt_cro_south_idle).
    0x51512F: ("call dword ptr [edx + 0xa4]", "if (!rt_cro_south_idle()) { ad = (uint32_t)(edx + 0xa4u); t0 = R32(ad); "
                                              "esp -= 4; W32(esp, 0x515135u); SYNC(); rt_call(t0); LOAD(); "
                                              "RT_AFTER_CALL(); }"),
    # 0x514ed0: the South's relegated row (status 3) to A Lower Division; no status after a season of one group.
    0x515097: ("cmp byte ptr [eax + 0x37], bl", _cmp("0xffu", "SZ1", "(rt_cro_south_idle() ? 0u : R8(eax + 0x37u))",
                                                      "ebx")),
    0x515180: ("call 0x689c80", "if (rt_cro_south_swap()) { esp -= 4; W32(esp, 0x515185u); SYNC(); f_00689c80(); "
                                "LOAD(); RT_AFTER_CALL(); } else { esp += 0x18u; }"),
    # The Greek National A Division count at a new game (record id in eax, 0x8283bb).
    0x8283C0: ("push 0xe", "esp -= 4; W32(esp, rt_gre_fix_count(0xeu, 0u));"),
    # The Greek National B Division count (record id in ecx, 0x8283c8).
    0x8283CE: ("push 0xe", "esp -= 4; W32(esp, rt_gre_fix_count(0xeu, 1u));"),
    # The first club of a tied pair (index in edi, from bp at 0x516164).
    0x51616A: ("mov dword ptr [esp + edi*4 + 0x1c], ebp", "W32(rt_cro_tiebreak_slot(esi, esp + 0x1cu, edi), ebp);"),
    # The second club (index in edi, from di at 0x516181).
    0x516190: ("mov dword ptr [esp + edi*4 + 0x1c], ecx", "W32(rt_cro_tiebreak_slot(esi, esp + 0x1cu, edi), ecx);"),
    # The list for Cup_Stage::Cup_Stage: esp+0x48 here is the array (11 pushes after 0x516286).
    0x51629A: ("lea ecx, [esp + 0x48]", "ecx = rt_cro_tiebreak_list(esi, esp + 0x48u);"),
    # The regroup 0x515190: the bound of the clubs it takes (dl counts them, je at 0x515203).
    0x5151FC: ("cmp dl, 0x20", _cmp("0xffu", "SZ1", "(edx & 0xffu)", "rt_cro_regroup_max(0x20u)")),
    # Each club it takes (index in esi, from dl at 0x5151f7).
    0x5151FF: ("mov dword ptr [esp + esi*4 + 0x14], eax", "W32(rt_cro_regroup_slot(esp + 0x14u, esi), eax);"),
    # The array of the sort 0x945333 (esp+0x14 two pushes later), with the count it found in eax (0x51520f).
    0x515219: ("lea ecx, [esp + 0x1c]", "ecx = rt_cro_regroup_list(esp + 0x1cu, eax);"),
    # The count of the sort by latitude: none when the second tier is one group (the list is in order of results).
    0x51521D: ("push eax", "esp -= 4; W32(esp, rt_cro_regroup_sort_count(eax));"),
    # The array of the walk.
    0x51522D: ("lea esi, [esp + 0x14]", "esi = rt_cro_regroup_list(esp + 0x14u, 0xffffffffu);"),
    # The North's count: the first 16 go North (jl 0x51523a), the others South.
    0x515231: ("cmp eax, 0x10", _cmp("0xffffffffu", "SZ4", "eax", "rt_cro_regroup_north(0x10u)")),
    # The division of each club of the walk: the South keeps its 16 rows, the clubs after North + 16 go to A Lower
    # Division (season_sizes_cro.c). The place is [esp+0x10] (0x515229, 0x5152c1).
    0x51523A: ("mov eax, dword ptr [esi]", "ebx = rt_cro_regroup_target(ebx, R32(esp + 0x10u)); eax = R32(esi);"),
    # The end of the walk (jl 0x515231).
    0x5152BB: ("cmp eax, 0x20", _cmp("0xffffffffu", "SZ4", "eax", "rt_cro_regroup_end(0x20u)")),
}
