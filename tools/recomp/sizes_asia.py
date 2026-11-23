"""J1 halves of any count (0.7.2 league sizes, slice 14, D8).

Instruction rows (address: (the instruction in the original, C code)) for league_sizes.INSNS. The runtime is
src/recomp_rt/win/season_sizes_asia.c. Each rt_ function returns the original's constant in the base game (Q152).

JapJ1 plays two halves as stages with no fixtures of their own: JapJ1::add_result 0x663940 gives each result of the
league (fixture +0x42 = 0xff, 0x663954) to half 1 when its round (fixture +0x34) is 15 or more (`cmp word ptr
[edi + 0x34], 0xf` at 0x66395a, setge at 0x66395f), and to half 0 before. JapJ1::create_half_season_stage 0x663c20
makes each half as a League_Stage (0x68b100 at 0x663d77) of the clubs of the table, 1 meeting (push 1 at 0x663d70)
and 15 matches a club (push 0xf at 0x663d36, argument 19, +0xa7). Both are the halves of 16 clubs in 2 meetings: 30
rounds (D 0x1e at 0x662e6d). With the count of the row they are half the rounds of the template (+0xa9) and half the
matches of the table.
"""


def _cmp16(left, right):
    return (f"t0 = {left}; t1 = ({right}) & 0xffffu; t2 = ((t0 - t1 - 0) & 0xffffu); "
            "SETF(K_SUB | SZ2, t0, t1, t2, 0);")


INSNS = {
    # JapJ1::add_result 0x663940: the first round of the second half (esi the league, edi the fixture).
    0x66395A: ("cmp word ptr [edi + 0x34], 0xf", _cmp16("R16(edi + 0x34u)", "rt_jap_half_rounds(esi, 0xfu)")),
    # JapJ1::create_half_season_stage 0x663c20: the matches of a club in a half (esi the league).
    0x663D36: ("push 0xf", "t0 = rt_jap_half_matches(esi, 0xfu); esp -= 4; W32(esp, t0);"),
}
