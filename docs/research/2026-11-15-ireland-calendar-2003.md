# The League of Ireland moves to calendar years in 2003 (gap row 2 of 0.7.2)

The code is `src/recomp_rt/win/season_calendar.h` (rules and dates, pure), `season_calendar_game.c` (the game),
and the hooks in `season_sizes.c`, `season_history.c`, `season_cup_breaks.c` and `season_league.c`. Each comment cites
the addresses of the original that it relies on. Sources of the real dates: RSSSF ier03, ier03a and ier04, and the
Wikipedia season pages named in rows wal-nir-irl-41, 41b, 42 and 42b.

## Real life

- 2002-03, the short season: Premier Division 10 clubs, 27 matches, 5 July 2002 to 26 January 2003. First Division
  12 clubs, 22 matches. Play-off of the Premier 9th and the First Division 2nd to 4th, 29 January to 8 February 2003.
- 2003, the first calendar season: Premier Division 10 clubs, 36 matches, 10 April to 30 November 2003. First
  Division 12 clubs, 33 matches. Play-off 3 to 13 December 2003.
- FAI Cup 2002-03: 14 July to 27 October 2002. FAI Cup 2003: 25 May to 26 October 2003.
- PFAI awards of the 2003 season: 22 February 2004.

## Decisions

1. Rows. Season 2002/03 holds two rows of each Irish league: 41 and 42 the short season, 41b and 42b the 2003 season.
   A league's edition +0x40 takes row Y when its real season begins with Y, else any row of Y - 1 whose real season
   begins with Y (`season_league_edition_row`). Edition 2002 takes 41, 2003 takes 41b, 2004 takes 66 (real 2004).
2. States. Winter up to edition 2001, the short season for edition 2002 with changing rules on, calendar seasons from
   edition 2003, and a start in 2003 or later is calendar from the creation. Changing rules off with a 2001 or 2002
   start stays winter. The league sizes must be on. The state comes from the start year, changing rules and IrePrm's
   edition, which the save holds, so no save field is added.
3. Update day 20 February (day 50) in both new states. It lies before day 60, so `key_nation_get_club_season_year`
   0x669950 gives the calendar year, and after the short season's play-off. Each season has one change: 20 June 2002,
   then each 20 February.
4. Templates. IrePrm becomes a grown class that counts the clubs of its data. In a calendar season the class's dates
   (15 August to 23 April) move back by whole weeks so that the last round falls nearest 30 November: about 21 March
   to 27 November. The short season plans from the class's dates before 1 February 2003. IreFirst's play-off (its own
   stage, draw 24 April and match 27 April of the next year in the original) starts 3 days after its table ends.
5. Cups. The FAI Cup moves by whole weeks: in calendar seasons its final falls nearest 26 October (first round in late
   May). In 2002-03 its first draw cannot come before the change of 20 June 2002, so it plays mid July to mid December
   2002. Every other cup of the entry ends 14 days before the update day: the President's Cup (July and August) keeps
   its dates, the Munster and Leinster Cups (August to May) move back about 13 weeks in calendar seasons, and in
   2002-03, which is too short for them, each date moves by its own whole weeks into the season. The League Cup plays
   in September and is not held in 2002-03 by its row. The Super Cup is not held from 2002/03.
6. Rows of every reader. While the move can happen in a game (league sizes on, and changing rules on or a start in
   2003 or later), a file of Ireland (ire_*) plays edition Y, or the season S of the February change of Y, in the row
   that holds the real season Y (`season_rules_row_of`). The cup breaks, the substitutes, the nation rules and the
   play-offs read through it; `tests/seasons/rules_readers_check.py` lists every reader of the table. Off, the rows
   of a season stand: a winter Ireland reads row Y.
7. Awards on 16 February in both new states, after the season and its play-off and before the update day (the real
   2003 awards were on 22 February 2004, after our update day).
8. Labels. Seasons up to 2002/03 keep their two-year labels after the flag flips (the wraps of 0x669630 and
   0x6697c0). The history screens pass a row's label to 0x6697c0 (0x499c5b..0x499c77). A league or cup that moves
   (the two leagues, the FAI Cup, the League Cup) writes its edition as the label in the short and calendar seasons:
   the short season 2002, shown "2002/03", the 2003 season 2003, shown "2003", and no later label moves by one. The
   2001/02 row keeps the original's label, the year it ends (2002).
9. A game that starts in 2003 or later with Ireland as the main nation starts on 1 March of the start year, the start
   of the entry's calendar season.
10. Transfer windows keep the original's (open item 258: no source for 2003 and 2004).

## Notes

- A 2002 start with changing rules off stays winter and keeps the start season's row 42 (2 meetings, 22 matches) in
  every season. That follows the rule that the start season's rules hold with changing rules off (Q232).

## Open risks

- Kinds that read rows by the season S of a change (names, substitutes, nation rules, cup formats) read row Y for the
  February change of Y, where the leagues and moving cups play the row of Y - 1. The Irish rows of these kinds change
  little, but this is not checked.
- IrePrm::create_table 0x63a740 takes only 10 or 12 clubs. The rows give 10 or 12, so the club moves and the
  exchange must keep those counts.
