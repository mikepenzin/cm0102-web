"""The cups of eastern Europe and Asia follow the divisions (0.7.2 league sizes, slice 6, D14).

Instruction rows (address: (the instruction in the original, C code)) for league_sizes.INSNS. The runtime is
src/recomp_rt/win/season_cup_sizes_east.c, which also wraps each cup's collect function, and its counts are in
season_cup_sizes_east.h. Each rt_ function returns the original's constant in the base game (Q152).
"""


def _cmp(width_mask, size, left, right):
    return (f"t0 = {left}; t1 = ({right}) & {width_mask}; t2 = ((t0 - t1 - 0) & {width_mask}); "
            f"SETF(K_SUB | {size}, t0, t1, t2, 0);")


INSNS = {
    # GreCup::collect_entrants 0x5e9a80: 16 lower picks of the lower division (pool in bl, at most 100), as
    # 0x4afbd0's count and as the copy bound.
    0x5E9B60: ("push 0x10", "t0 = rt_gre_cup_lower(ebx & 0xffu); esp -= 4; W32(esp, t0);"),
    0x5E9B86: ("cmp eax, 0x10", _cmp("0xffffffffu", "SZ4", "eax", "rt_gre_cup_lower(ebx & 0xffu)")),
    # RusCup::select_entrants 0x7e99a0: 14 lower picks (pool in the byte at [esp + 0x13]) and the first place of the
    # Premier League (the places taken, the byte at [esp + 0x12]).
    0x7E9B74: ("push 0xe", "t0 = rt_rus_cup_picks(esp); esp -= 4; W32(esp, t0);"),
    0x7E9C73: ("cmp edx, 0xe", _cmp("0xffffffffu", "SZ4", "edx", "rt_rus_cup_lower(R8(esp + 0x13u))")),
    0x7E9C7F: ("mov cl, byte ptr [esp + 0x12]",
               "ecx = (ecx & 0xffffff00u) | (rt_rus_cup_premier_place(R8(esp + 0x12u)) & 0xffu);"),
    # TurCup::collect_entrants 0x8fcb90: 26 lower picks copied from [esp + 0x7c] down, the second tier after them.
    0x8FCD30: ("lea ecx, [esp + 0x7c]", "ecx = rt_tur_cup_copy_from(esp);"),
    0x8FCD34: ("mov edx, 0x1a", "edx = rt_tur_cup_lower(esp);"),
    0x8FCD71: ("mov eax, 0x9c", "eax = rt_tur_cup_second_place(esp);"),
    # PolLgeCup::collect_entrants 0x7cad30: the limit of both second-tier loops.
    0x7CAE01: ("movsx edx, word ptr [esi + 0x3e]", "edx = rt_pol_lge_second(esi);"),
    0x7CAE84: ("movsx edx, word ptr [esi + 0x3e]", "edx = rt_pol_lge_second(esi);"),
    # JapJCup::collect_entrants 0x665a00: the limit of the J2 loop, and its stop.
    0x665ABA: ("movsx edx, word ptr [esi + 0x3e]", "edx = rt_jap_j_cup_j2(esi);"),
    0x665AE5: ("movsx edx, word ptr [esi + 0x3e]", "edx = rt_jap_j_cup_j2(esi);"),
    # JapEmpCup::collect_entrants 0x6624f0: 47 lower picks (pool in bl), then the places that follow them.
    0x66268B: ("cmp bl, 0x2f", _cmp("0xffu", "SZ1", "(ebx & 0xffu)", "rt_jap_emp_lower(ebx & 0xffu)")),
    0x6626B9: ("push 0x2f", "t0 = rt_jap_emp_place(0x2fu, 1u, 0u); esp -= 4; W32(esp, t0);"),
    0x662711: ("mov edx, 0x2f", "edx = rt_jap_emp_place(0x2fu, 1u, 0u);"),
    0x662740: ("mov eax, 0x11a", "eax = rt_jap_emp_place(0x11au, 6u, 0u);"),
    0x662778: ("mov eax, 0x126", "eax = rt_jap_emp_place(0x126u, 6u, 12u);"),
    0x662789: ("mov edi, 0x34", "edi = rt_jap_emp_place(0x34u, 1u, 5u);"),
    0x6627CC: ("mov eax, 0x138", "eax = rt_jap_emp_place(0x138u, 6u, 30u);"),
}
