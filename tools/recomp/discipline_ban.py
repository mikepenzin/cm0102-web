"""Fixes for the error boxes discipline..cpp 1149 and ruling_body..cpp 661 of the original's bans
(src/recomp_rt/win/discipline_ban.c, src/recomp_rt/win/season_intl.c).

Discipline::release_ban 0x556d00 (this in esi, the staff id in ebp) frees a player's four ban slots. For slot 3,
the international ban, it also removes his entry from the nation index of the Discipline object (this+0xc, count
this+0x12 at 0x556c47, entries of 8 bytes: the staff id and the key of his first nation +0x1a when the ban was
added, 0x556c10..0x556c57). It looks for the entry with bsearch 0x946464 (comparator 0x55e5f0: the nation key,
then the staff id) under the key of his first nation now (0x556d54..0x556d94), then of his second nation +0x1e
(0x556d9a..0x556dcd). When neither finds it, it shows the box at 0x556dcf..0x556e25 (line 0x47d) and goes on
with the next slot (0x556e32). The box came in a 16-nation long game in June 2014 (Discipline::serve_international_bans
0x557120, from RulingBody::process_match 0x7e8095): the player's first nation was no longer the one of his entry.
rt_discipline_ban_entry finds the entry by the staff id alone: the release removes it (0x556e34) and logs the
nations. With no entry at all it goes on with the next slot, as the original does after its box.

The load. The save 0x555dd0 writes every rule set of the 43 that exists (0x555f2e..0x555f4a); the rule-set load
0x55d420 makes the Oceania body RbOceania (0x7e13d0, slot 35) only when a selected nation has the continent Oceania
(0x6699d0 at 0x55deac, je 0x55def0 at 0x55deb6). A season game with changing rules on from 2005/06 or before with
Australia selected makes the body, and Australia moves to Asia on 1 January 2006 (season_intl.c). A later load left
the body out and read the rest of discipline.dat out of step: a nation index of 1 entry for 16 world bans, the box
at the next release, then a double free. rt_discipline_oceania_body makes the load take the body in that game. The Oceania entry's creator has the same test
(oceania_entry.py).

The held ban (ruling_body..cpp 661). RulingBody::find_or_add_discipline_entry 0x7e82e0 (this in esi, the staff id
in ebx) looks for the player's ban slot of this body (Discipline::get_slot_value 0x55cae0 at 0x7e8301). With none
and the create flag set, it looks in the held-ban list (0x55cb60 at 0x7e832b): a held ban of the same slot and the
same body is the box at 0x7e833e..0x7e8394 (line 0x295), and then the original goes on at 0x7e83a1 and adds a new
entry. Discipline::hold_bans_for_loan 0x5563f0 holds the bans of a domestic body whose nation is not the loan club's
(0x556463..0x55649b), so the original never meets such a body for that player: its domestic cups have the clubs of
one nation. The All-Ireland cup of 0.6.0 (season_new_cups.c, "new_all_ireland") has the clubs of the Republic of
Ireland and of Northern Ireland and one ruling body: a player of an Irish club on loan at a Northern Irish club
(his Irish ban held) met the Irish body in it (14 February 2011 and 2012 in the gate runs). rt_ruling_body_held
logs the player and goes on at 0x7e83a1 without the box, as the original does after it. The cause fix is at the
loan: rt_ban_nation_applies (season_new_cups.c) makes the hold 0x5563f0 and the restore 0x556570 count the two
nations as one while the cup is in the game (je 0x55647d, je 0x55671e), so no such held ban is made; the skip at
0x7e833c stays as a logged safety net.
"""

INSNS = {
    0x556DCF: ("lea ecx, [esp + 0x1c]",
               "{ uint32_t e_ = rt_discipline_ban_entry(esi, ebp); if (e_ == 1u) ecx = (uint32_t)(esp + 0x1cu); "
               "else if (e_) { eax = e_; goto L_556e34; } else goto L_556e63; }"),
    0x55DEB6: ("je 0x55def0", "if (cc_e(F) && !rt_discipline_oceania_body()) goto L_55def0;"),
    0x55647D: ("je 0x5564e0", "if (cc_e(F) || rt_ban_nation_applies(R32(eax), ecx)) goto L_5564e0;"),
    0x55671E: ("je 0x556799", "if (cc_e(F) || rt_ban_nation_applies(R32(edx), eax)) goto L_556799;"),
    0x7E833C: ("jne 0x7e83a1", "if (!cc_e(F) || rt_ruling_body_held(esi, ebx)) goto L_7e83a1;"),
}
