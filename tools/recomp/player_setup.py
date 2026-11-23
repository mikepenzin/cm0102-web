"""The dates of a season database's player_setup.cfg move to the season of a season game (open item 229;
src/recomp_rt/win/player_setup.c has the trace and the rule: that of the database's contract ends).

player_setup_apply_line 0x7a3a10 reads each dated line's day, month and year, makes two-digit years whole and checks
them (years 1980 to 2010 for LOAN, RETIREMENT and FUTURE_TRANSFER, at most 2010 for the bans). Each patch takes the
place of the first instruction after those checks and moves the year through rt_player_setup_year (nothing moves in a
2001 start, so the base game is the original's):
    bans, the end date in ax and [esp + 0x10], before the check against the game's year [0xae2c92]:
        DRUGS 0x7a3d6c, MISCONDUCT 0x7a3fd5, FALSE_PASSPORT 0x7a422c (each `xor ecx, ecx`)
    RETIREMENT 0x7a48a1, the year [esp + 0x10] for the Date constructor 0x549f70 at 0x7a48b8
    FUTURE_TRANSFER 0x7a4cb8, the year in eax for the Date constructor at 0x7a4cd0 (pushed at 0x7a4cc9)
    LOAN 0x7a4fb4, the start year in eax for the constructor at 0x7a4fbf, and 0x7a4fc4, the end year [esp + 0x48] for
        the constructor at 0x7a4fe6; both dates go to TRANSFER_MANAGER::add_config_loan 0x8bf400 (0x7a500c)
The flags the replaced instructions set are not read before the next compare or call.
"""

_BAN = "eax = rt_player_setup_year(eax); W16(esp + 0x10u, (uint16_t)eax); ecx = 0u;"

INSNS = {
    0x7A3D6C: ("xor ecx, ecx", _BAN),
    0x7A3FD5: ("xor ecx, ecx", _BAN),
    0x7A422C: ("xor ecx, ecx", _BAN),
    0x7A48A1: ("mov eax, dword ptr [esp + 0x10]", "eax = rt_player_setup_year(R32(esp + 0x10u));"),
    0x7A4CB8: ("mov edi, dword ptr [esp + 0x2f0]", "edi = R32(esp + 0x2f0u); eax = rt_player_setup_year(eax);"),
    0x7A4FB4: ("or ebx, 0xffffffff", "ebx = 0xffffffffu; eax = rt_player_setup_year(eax);"),
    0x7A4FC4: ("mov eax, dword ptr [esp + 0x48]", "eax = rt_player_setup_year(R32(esp + 0x48u));"),
}
