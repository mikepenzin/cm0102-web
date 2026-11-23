/* The Italian rules of the league sizes that depend only on numbers (0.7.2 gap fixes, rows 30 and 32 of the size
 * gaps; season_sizes_ita.c applies them to the game). */
#ifndef SEASON_ITA_RULES_H
#define SEASON_ITA_RULES_H

/* The last season of the tie-break match (spareggio) of Serie B. From 2005/06 the head-to-head table (classifica
 * avulsa) settles a tie on points (it.wikipedia "Incontri di spareggio del campionato italiano di calcio",
 * "Serie B 2005-2006"). */
#define ITA_SPAREGGIO_LAST 2004

/* The tie-break zone of row `row` of a Serie B table of `clubs` rows in the season of `year` with `up` direct
 * promotion places and `down` direct relegation places (+0xbe, +0xc1): 1 the champion, 2 the other promotion places,
 * 3 the relegation places, 0 none. ItaSerB::update_for_date 0x647050 and create_tiebreak_playoff_stage 0x646bb0
 * start a tie-break when two rows with equal points have different zones. From 2005/06 every row has 0. */
int ita_serie_b_zone(int year, int clubs, int up, int down, int row);

/* The first season in which Serie A again plays a tie-break match, for the title and the relegation line only
 * (open item 254). */
#define ITA_SERIE_A_SPAREGGIO_BACK 2022

/* The tie-break zone of row `row` of a Serie A table of `clubs` rows with `down` direct relegation places (+0xc1) in
 * the season of `year`. `class_zone` is the byte the original wrote for the row (1 the title, 2 and 3 the European
 * places, 4 after them, 5 the relegation places of its 18; ItaSerA::update_for_date 0x642880 and
 * create_tiebreak_playoffs 0x641a70). To 2004/05 the class's title and European zones stay and the relegation zone
 * 5 is the last `down` rows. From 2005/06 to 2021/22 every row has 0 (the head-to-head table). From 2022/23 only
 * row 0 has 1 and the last `down` rows have 5. A row outside the table has 0. */
int ita_serie_a_zone(int year, int clubs, int down, int row, int class_zone);

/* A club as compare_clubs_by_seeding_pot 0x640590 reads it: its last division (+0x5b), that division against Serie B
 * ([0x9cf574]) and Serie A ([0x9cf570]), its division now (+0x57) against Serie B, whether the two divisions are one
 * record, and its last place (+0x5f, s8). */
typedef struct {
    int has_last, last_is_b, last_is_a, cur_is_b, last_is_cur, place;
} ItaCupClub;

/* The seeding pot of a club in the Coppa Italia (0x640590): 4 down from Serie A into Serie B or Serie B 3rd to 6th,
 * 3 Serie B 7th to 14th, 2 Serie B 15th to the 3rd-last or now in Serie B from below, 1 any other. The original
 * ends the third range at 18th, the 3rd-last of its 20 (0x6405fe, 0x64068e); `b_count` is the count of the last
 * Serie B (0 or 20: the original's 18th). */
int ita_cup_pot(const ItaCupClub *club, int b_count);

/* The count of the last Serie B that ita_cup_pot takes: `scanned` is the last place of the clubs whose last division
 * is Serie B. In the start season of a game (`season` <= `start`) the last places come from the database, and a
 * season database can hold part of the table (a 2004 start found 17, a 2005 start 18, against the real 24 and 22):
 * a count under 20 there gives 0, the original's 18th. A count over 20 there, or any count in a later season,
 * which the game played, stays. */
int ita_cup_b_count(int scanned, int season, int start);

/* The play-out of a C1 group (Serie C group from 2014/15) of `clubs` in the season of `year`: `ties` ties (0 to 2),
 * each as `slot[2k]` the lower row v `slot[2k + 1]` the higher row (0 = 1st), and `direct` rows down directly at the
 * foot. The slots are the order of the class's play-out list (ItaSerC1a::create_playoff_stages 0x64ba70: rows 16,
 * 13, 15, 14 at 0x64bc2c..0x64bc58, the same rows as seeds at 0x64bc61..0x64bc70: 17th v 14th, 16th v 15th, 18th
 * down). Sources: it.wikipedia "Serie C1 2001-2002" to "Serie C1 2007-2008", "Lega Pro Prima Divisione 2008-2009"
 * to "2013-2014", "Lega Pro 2014-2015" to "2016-2017", "Serie C 2017-2018" on, and the en.wikipedia pages of the
 * same seasons:
 * - the class's shape, the four rows above the last v each other and the last down, at any count: 2001/02 to
 *   2012/13 (19 and 18 in 2004/05, 17 and 16 in 2012/13), 2014/15 to 2016/17 and 2019/20 on;
 * - 2013/14: no relegation and no play-out (the two groups merged into the Serie C);
 * - 2017/18: the last four (16th v 19th, 17th v 18th), none down directly;
 * - 2018/19: one tie, 18th v 19th; a group of 20 sends its 20th down, a smaller group none.
 * A group under 6 has no play-out and its last row goes down. */
typedef struct {
    int ties, slot[4], direct;
} ItaPlayout;
void ita_c1_playout(int year, int clubs, ItaPlayout *p);

/* The points gap of the play-out: no tie when its clubs are more than 8 points apart, and the lower club goes down.
 * Written from 2016/17 and played in 2016/17, 2017/18, 2018/19 and from 2020/21 (it.wikipedia "Lega Pro 2016-2017",
 * "Serie C 2020-2021"; en.wikipedia "Serie C"). Not in 2015/16 ("regardless of the gap") or 2019/20 (not stated). */
#define ITA_PLAYOUT_GAP 8
int ita_c1_gap_rule(int year);

/* Takes out each tie of `p` whose rows are more than ITA_PLAYOUT_GAP apart on `points` (by row): its lower row goes
 * to down[], its higher row to safe[], in the order of the ties. The ties left move to the first slots. Returns the
 * count of ties taken out. */
int ita_c1_playout_gap(ItaPlayout *p, const int *points, int *down, int *safe);

#endif
