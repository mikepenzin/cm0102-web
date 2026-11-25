"""Referee ranges outside the officials table (src/recomp_rt/win/referees.c).

OfficialsManager::choose_match_official 0x79c7b0 takes a range of the officials table: a league entry of the nation
(0x79c88a..0x79c891), the nation's own range (0x79c8af, 0x79c9fe) or the top quarter of the range of another nation of
the continent (0x79c96a..0x79c998). From 0x79ca04 esi holds the first slot and edi the count; 0x79ca09..0x79ca1f pick
a start slot (utils_random 0x90cfc0 on edi - 1) into eax and [esp + 0x1c]; the loop from 0x79ca23 reads the busy flag
[[manager + 8] + id] of each row (0x79ca3f, 0x79ca67, manager in ebp) until one is free or it is back at the start.

set_up_officials 0x79bf60 leaves first 1000000 for a nation that has no continent when the game is made
(referees.c). The season code gives some of those nations a continent later, and the search for another nation of
the continent can then stop at one of them: the slot is far past the table, the id read from that row is junk, and
the busy flag read stops the browser with "memory access out of bounds" (report of 25 November 2026, a CONCACAF match
of September 2026 with Martinique and five other members in North America).

At 0x79ca23 (the first instruction after the start slot is known, reached from 0x79ca0f and 0x79ca1f; no flag is
read before the sub at 0x79ca33 sets them) rt_referee_out tests the range against the table. Outside, the range is the
whole table (esi 0, edi the table size) and the start slot is the picked one modulo the size, in eax and in
[esp + 0x1c], so the loop stays inside. Inside, nothing changes: the instruction runs as before.
"""

INSNS = {
    0x79CA23: ("mov ebx, dword ptr [0xae23e8]",
               "if (rt_referee_out(ebp, esi, edi)) { edi = rt_referee_count(ebp); esi = 0u; eax %= edi; "
               "W32(esp + 0x1cu, eax); } ebx = R32(0xae23e8u);"),
}
