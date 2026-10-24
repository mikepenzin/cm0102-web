# Free competition rows for the Summer 2002 Turkish groups (research, 2026-10-24)

Question: can the Summer 2002 build give three unused 3.9.68 rows of club_comp.dat the three Xbox "Turkish 2.
Division Category B Group A to C" competitions, so that no row is added (spec 2026-10-24-summer-2002-db-spec.md,
slice 3)?

Answer: no. 3.9.68 has two free Turkish rows, and the groups need three.

## The five "Turkish 2. Division Group" rows

The 3.9.68 rows 168 to 172 ("Turkish 2. Division Group 1" to "Group 5") have no club at +0x57. They are not free:

- The game finds them by name. set_key_ids (0x601a40) runs the club comp name chain 0x60efd0, whose names include
  "turkish 2. division group 1" to "group 5" (src/cm3/key_ids_table.c, the globals 0x9cf900 to 0x9cf910). After
  the chains, it checks each key id and calls report_missing_key_id (0x60d780) for one that stays -1, which
  shows an error box. For Group 1: `cmp dword ptr [0x9cf900], edi` at 0x607547, then the push of the name and the
  call at 0x60754f and 0x607554 (objdump of cm0102.exe 3.9.68, 0x6043ac to 0x60ba6e). include/key_index.h notes
  the same box. A rename of the row leaves the key id at -1.
- 53 clubs hold them as their last division (club +0x5b): 9, 11, 13, 10 and 10 clubs.

## The rows that are free

A free row has no club that points at it (+0x57, +0x5b, +0x60 of club.dat and nat_club.dat), no history row
(club_comp_history and nation_comp_history +0x04) and a name that no key chain holds. Of the Turkish rows (nation
Turkey, or no nation with a Turkish name), two are free: 287 "Turkish 2. Divison Category B Group 1" and 288
"Turkish 2. Division". Free rows of other nations: 159 Northern Irish County Antrim Shield (the season history
table names it, src/recomp_rt/win/season_history_table.c), 219 PreLibertadores and 233 Korean All-Star Cup.

Q29 and Q43 settled the way forward (slice 5): the build puts the three Xbox rows into the free rows 287, 288 and 233
and tags the member clubs. No row is added (nation comp ids follow the club comp ids). See
`2026-10-24-summer-2002-turkish-groups.md`.
