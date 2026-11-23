"""Test switch RECOMP_TEST_MARKET_WEEKLY (open item 193, src/recomp_rt/win/market_weekly.c): the day loop's read of
g_computer_bids_enabled 0x9cd1f8 at 0x5cdc4e goes through rt_market_bids_flag, which gives the flag unchanged unless
the switch is on and the game date is not a Monday."""

INSNS = {
    0x5CDC4E: ("mov eax, dword ptr [0x9cd1f8]", "eax = rt_market_bids_flag(R32(0x9cd1f8u));"),
}
