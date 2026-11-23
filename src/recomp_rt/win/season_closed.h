/* Closed leagues: the clubs that fold and join in their real seasons (0.7.3 slice 6, issue 157, decision L7).
 *
 * A closed league has no division below, so the size step cannot bring it to the count of its row. This table names
 * the clubs of each closed league and the seasons that each one plays. The plan below works on plain values, so a
 * ctest case proves it (tests/seasons/test_season_closed.c). The league's own runtime applies the plan at its season
 * change (season_mls.c). The seasons are the real seasons of the league's editions. */
#ifndef RECOMP_SEASON_CLOSED_H
#define RECOMP_SEASON_CLOSED_H

/* One club of the league's nation, as the runtime reads it. */
typedef struct {
    const char *name;
    int in_league;      /* 1 when the club plays in the league in the season that ended */
    int human;          /* 1 when a human manages it: it keeps its league */
    int reserve;        /* 1 for a reserve or B side: it never joins */
    int reputation;
    const char *short_name;     /* NULL when the caller has none */
} ClosedClub;

enum { CLOSED_STAY = 0, CLOSED_JOIN = 1, CLOSED_FOLD = 2, CLOSED_CUT = 3 };

typedef struct {
    int joined, folded, cut;
    int human_stays;    /* clubs that the table folds and that stay because a human manages them */
    int missing;        /* clubs of the season that no club of the data can stand for */
    int count;          /* the clubs of the league after the plan */
} ClosedResult;

/* The closed league of a database division ("usa_mls" for "American Major League", "aus_nsl" for "Australian National
 * Soccer League"), or NULL. The key is the league's file in the rules table. */
const char *season_closed_league(const char *division);

/* 1 when the table decides the clubs of the league in the season. The A-League starts in 2005/06: the National Soccer
 * League of the seasons before keeps the clubs of its database. */
int season_closed_active(const char *league, int season);

/* 1 for a club of the league from another nation (New Zealand in the A-League). A database can hold it under that
 * nation, so the nation of the division does not find it. */
int season_closed_abroad(const char *league, const char *name, const char *short_name);

/* 1 when a closed league's table names the club for other seasons only: its record has no division until it joins. */
int season_closed_waits(const char *name, const char *short_name, int season);

/* The table row that has this name of a club, or -1. Two names of one club give the same row. */
int season_closed_row(const char *league, const char *name);

/* The table row of a club of a database. The update databases keep the 12 club records of the league and give a
 * retired record to a newer club: the record "Tampa Bay Mutiny" of October 2023 has the short name "LAFC" and the
 * squad of Los Angeles FC. So the short name of a club that the table names decides its row. short_name can be NULL. */
int season_closed_club(const char *league, const char *name, const char *short_name);

/* The real name of the club that season_closed_club finds, or name when the table does not name it. */
const char *season_closed_real_name(const char *league, const char *name, const char *short_name);

/* 1 when the club plays in the league in the season, 0 when the table names it for other seasons only, -1 when the
 * table does not name it. short_name can be NULL. */
int season_closed_member(const char *league, const char *name, const char *short_name, int season);

/* The clubs that the table names for the season. */
int season_closed_count(const char *league, int season);

/* The real name of table row i of the league, or NULL after the last row. */
const char *season_closed_real(const char *league, int i);

/* The season that row i of the league joins (its first season), 0 after the last row. */
int season_closed_first(const char *league, int i);

/* The plan of a season change. actions[i] takes the CLOSED_ value of clubs[i]. want is the count of the league's row
 * (0 or less: no count). Clubs that the table folds leave first. Then each club of the season that the league lacks
 * joins. A club that the table does not name leaves only while the league holds more clubs than want, the lowest
 * reputation first. A club with a human manager never leaves. The A-League never holds more than the 13 clubs of its
 * class: past that count a club that would join waits, then the named club with the lowest reputation leaves. */
void season_closed_plan(const char *league, int season, int want, const ClosedClub *clubs, int n, int *actions,
                        ClosedResult *result);

/* The clubs that take the places of the clubs that join (s073-16). A club that joins leaves a place in its division
 * from[i] (0: no division). Each such place, in the order of the list, takes the club that leaves the league
 * (CLOSED_CUT) with the highest reputation that has no place yet. When no such club is left, it takes the club with
 * pool[i] set (the caller sets it for a club of the nation with no division or in a spare division, not a reserve
 * side, a human's club or a club that the league's table names) with the highest reputation. pool may be NULL. to[i]
 * gets the new division of each club that takes a place and 0 for every other club. Returns the places filled.
 *
 * UsaOpenCup::collect_entrants 0x90b530 takes 10 clubs of the division with key id [0x9cf594] (0x90b56b, need 10 at
 * 0x90b5f0, comp_util 0x4afbd0 at 0x90b5f8) and boxes "comp_util..cpp 241" (0x4afc8d) with fewer. The October 2026
 * database holds the 18 MLS clubs of 2026 in that division, American A-League, and nothing else. All 18 join the MLS,
 * and the 18 stand-ins that the fill gave the MLS (USL clubs) left for no division, so the cup found 0 clubs. In
 * real life those clubs play in the USL, the second tier, so they take the places of the clubs that joined. In a
 * running game no club leaves when the MLS grows (a 2005 start on the October 2026 database: each season a club of
 * the A-League joins, and by 2015 fewer than 10 are left), so a USL club of the spare division takes the place. The
 * divisions that a club joins from (American A-League and D3-Pro League, the Australian and New Zealand divisions) have
 * no league of their own in the running game: the original has only UsaMls, UsaOpenCup and AusNsl there. */
int season_closed_places(const ClosedClub *clubs, const int *actions, const unsigned *from, const int *pool, int n,
                         unsigned *to);

#endif
