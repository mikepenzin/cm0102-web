"""The Oceania entry at a load (src/recomp_rt/win/season_intl.c, rt_oceania_entry_kept).

key_nation_create_all_comps 0x669180 runs each league table entry's creator at a new game and at a load. The Oceania
creator makes the OFC Champions League (OceaniaClubChamp, constructor 0x796fd0 at 0x83207e) only when a selected nation
of the key nation table has the continent Oceania (key_nation_continent_has_selected_nation 0x6699d0 on entry +4 at
0x831f7a, je 0x8320aa at 0x831f84). The save 0x6692f0 writes every object of every entry. A season game with changing
rules on from 2005/06 or before with Australia selected makes the object, and Australia moves to Asia on 1 January
2006: a load after that made no object, so the OFC Champions League stopped after the season of the load, and each
later save packed the part file of the load again. rt_oceania_entry_kept makes the load take the object in that game,
with the test of the Oceania ruling body (discipline_ban.py, 0x55deb6). The Asian creator has the same test the other
way (0x6699d0 at 0x831c6b, je 0x831e5b at 0x831c75): in that game with no Asian nation selected but Australia, a load
made the Asian objects with no part in the save; rt_asia_entry_left_out keeps them out, as the new game did.
"""

INSNS = {
    0x831F84: ("je 0x8320aa", "if (cc_e(F) && !rt_oceania_entry_kept()) goto L_8320aa;"),
    0x831C75: ("je 0x831e5b", "if (cc_e(F) || rt_asia_entry_left_out(R32(esi + 4u))) goto L_831e5b;"),
}
