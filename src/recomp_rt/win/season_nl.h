/* The Nations Leagues (0.6.0 slice 28; spec docs/specs/2026-10-16-seasons-0.6.0-spec.md, slice 28, Q169, Q248,
 * Q277, Q278, Q288). Research and evidence: docs/research/2026-10-18-nations-league.md.
 *
 * This header has two parts. The rules (season_nl_rules.c) read no game memory, so the unit test
 * tests/seasons/test_season_nl.c builds them alone: the editions, the real leagues of each edition, the overall
 * ranking of an edition, the leagues of the next edition (promotion and relegation) and the play-off paths that
 * the results pick. The game part (season_nl.c) makes the competition objects and keeps the leagues of each
 * nation in the season part of the save. */
#ifndef RECOMP_SEASON_NL_H
#define RECOMP_SEASON_NL_H

#include <stdint.h>

#define NL_MAX_LEAGUES 4        /* UEFA A to D; CONCACAF A to C */
#define NL_MAX_NATIONS 64       /* UEFA has 55 members */

/* A day of an edition: month 1 to 12, year as an offset from the edition year (0 for the year of the league
 * phase, 1 for the finals the year after, -1 for a draw in the year before). Day 0: none. */
typedef struct {
    int day, month, year;
} SeasonNlDate;

/* One edition of a Nations League (season_nl_table.c, made by tools/seasons/make_nl.py from the rules table
 * int-continental.tsv and docs/research/rules-0.6.0/nations-league-tiers.tsv). */
typedef struct {
    const char *confed;         /* "UEFA" or "CONCACAF" */
    const char *edition;        /* "2018-19" */
    int season;                 /* the season of the first match of the league phase (2018 for 2018/19; 2021
                                   for 2022-23, which started on 1 June 2022) */
    int year;                   /* the edition year: the first year of the name (2022 for 2022-23) */
    int leagues;                /* 4 (A to D) or 3 (A to C) */
    int size[NL_MAX_LEAGUES];   /* nations in each league */
    int groups[NL_MAX_LEAGUES]; /* groups in each league */
    /* Promotion and relegation into the next edition (the up and down columns of the row). */
    int up_winners;             /* 1: the group winners of each league below A go up one league */
    int down_last[NL_MAX_LEAGUES];   /* 1: the last of each group of the league goes down one league */
    int down_lowest[NL_MAX_LEAGUES]; /* more nations of the league that go down: the lowest of the ranking */
    int up_second[NL_MAX_LEAGUES];   /* the best runners-up of the league by the ranking that go up too (the best
                                        League C runner-up of CONCACAF from 2023-24) */
    SeasonNlDate draw;          /* the draw of the league phase */
    SeasonNlDate md[6];         /* the first day of each of the six matchdays of the league phase */
    SeasonNlDate sf, final;     /* the semi-finals and the final of the finals of four */
    const char *row;            /* id of the rules table row */
} SeasonNlEdition;

/* The real league and group of a nation in an edition (the draw). */
typedef struct {
    const char *confed;
    const char *edition;
    int league;                 /* 0 = A */
    int group;                  /* 1 to 4, 0 when the draw is not known */
    const char *nation;         /* names as the game's nation.dat writes them, "/" between other names */
    int out;                    /* 1: in the draw, but out before the first match (Russia in 2022-23) */
} SeasonNlTier;

/* The group matchdays of a qualifying with Nations League paths (Euro 2020, Euro 2024 and the 2022 World Cup):
 * the real days, in the year after the campaign year that the class holds at +0x40. */
typedef struct {
    int euro;                   /* the year of the finals */
    SeasonNlDate md[10];        /* year: offset from the campaign year (1) */
    int paths;                  /* play-off paths of four nations (4 for Euro 2020, 3 for Euro 2024) */
    SeasonNlDate po_sf, po_final;  /* the path semi-finals and finals (year: offset from the campaign year, 2) */
    int world_cup;              /* 1: the World Cup European qualifying (wc_europe_league), 0: the Euro's */
} SeasonNlQualifying;

extern const SeasonNlQualifying season_nl_qualifying[];
extern const int season_nl_qualifying_count;

/* A CONCACAF Gold Cup that the class copa_america plays (season_gold_cup.c): 12 or 16 teams and the first day of
 * the edition (year offset 0). From int-continental.tsv and the dates file. */
typedef struct {
    int year;
    int teams;
    SeasonNlDate start;
    const char *row;
    const char *hosts;          /* the host nations, names of the game's nation.dat, ";" between them */
} SeasonNlGoldCup;

extern const SeasonNlGoldCup season_nl_gold_cups[];
extern const int season_nl_gold_cup_count;

extern const SeasonNlEdition season_nl_editions[];
extern const int season_nl_edition_count;
extern const SeasonNlTier season_nl_tiers[];
extern const int season_nl_tier_count;

/* The day of the week of a date (month 1 to 12): 0 Monday to 6 Sunday, the code of Date::Date 0x549f70 and of
 * the round dates (5 Saturday and 2 Wednesday at 0x57d22f and 0x57d257). */
int season_nl_weekday(int day, int month, int year);

/* The first edition of a confederation whose league phase starts in season s or later, or NULL. */
const SeasonNlEdition *season_nl_first(const char *confed, int s);
/* The edition after e, or NULL. */
const SeasonNlEdition *season_nl_after(const SeasonNlEdition *e);

/* 0.6.2 item 209: the years of the editions after the table's last edition (year `last`) with changing rules on.
 * UEFA: every second year from `last` (Q248). CONCACAF: a year whose autumn the confederation's World Cup
 * qualifying leaves free, as in the 2023-26 cycle (league phases September to November 2023 and 2024, qualifying
 * from March 2024). The game's CONCACAF section holds the campaign year W - 2 for the World Cup of year W
 * (0x914a91) and plays from March of W - 2 to November of W - 1 (preliminary ties in spring, the semi-final groups
 * in September to November of W - 2, the final group in W - 1), so an edition starts in W - 4 and W - 3 only.
 * wc(y): 1 when a World Cup is played in year y. season_nl_held_after gives 1 for a year that holds an edition;
 * season_nl_next_after the first such year after `year` (0 for none within 8 years). */
int season_nl_held_after(const char *confed, int last, int year, int (*wc)(int));
int season_nl_next_after(const char *confed, int last, int year, int (*wc)(int));

/* Item 209: the table edition whose draw day an edition of `year` after the table takes (season_nl.c edition).
 * Friendly::update_international_friendlies 0x5cbb00 arranges friendlies 12 weeks ahead, and
 * arrange_international_friendlies 0x5cbdf0 skips a nation only for fixtures already stored within 4 days of the
 * date, so a league phase drawn less than 12 weeks before its first matchday meets friendlies that were made first.
 * The real CONCACAF draws lie 16 to 23 weeks before the first matchday (27 March 2019, 16 May 2023, 6 May 2024),
 * except in a World Cup year (23 July 2026: the finals were in North America). So for CONCACAF the draw of the
 * latest table edition of the same kind of year (wc(year) the same); for UEFA, and when no such edition exists,
 * the last edition (NULL when the confederation has none). A World Cup year keeps 23 July: the North American rows
 * of the friendly date table for that year (0x5ca4f0, offset 0) are 25 August, 18 November and 15 December, so no
 * friendly row falls in the September and October windows, and the November row is arranged in late August,
 * after the draw (docs/research/2026-10-18-national-team-comps.md, section 7.1). */
const SeasonNlEdition *season_nl_draw_of(const char *confed, int year, int (*wc)(int));

/* Item 215: 1 when a friendly of confederation `confed` on day d.m.y would meet a league-phase matchday (within
 * NL_FRIENDLY_NEAR days, the guard of arrange_international_friendlies 0x5cbdf0) of a table edition that is drawn
 * on or after the day the friendly is arranged (NL_FRIENDLY_AHEAD days before it, 0x5cbb00). The game holds one
 * edition at a time (season_nl.c next_edition), so it draws an edition no earlier than the day after the final of
 * the edition before. Real life drew the 2023-24 edition on 16 May 2023, before the 2022-23 finals of 15 and
 * 18 June 2023, and the September to November 2023 windows were the confederation's. In the game the draw came on
 * 19 June 2023, the friendlies of 5 September 2023 were made on 13 June, and 11 matches of the first matchday
 * (7 September) moved off the FIFA days. The table gives 1 also near the June 2022 matchdays of CONCACAF 2022-23
 * (draw 4 April 2022) and the September 2026 ones of 2026-27 (draw 23 July 2026), but the North American rows of
 * the friendly date table (0x5ca4f0) hold no day there, so only the 2023 row changes. */
#define NL_FRIENDLY_AHEAD 84
#define NL_FRIENDLY_NEAR 4
int season_nl_late_window(const char *confed, int y, int m, int d);

/* The result of one nation in one edition. The rules rank and move these rows; the nation is the game's id. */
typedef struct {
    int nation;                 /* the game's nation id (or a test id) */
    int league;                 /* 0 = A */
    int group;                  /* 1 to 4 */
    int pos;                    /* final place in the group, 1 = winner; 0 = a nation new to the competition
                                 * (league: its league in the table), which season_nl_next_leagues places last */
    int played, points, gd, gf;
    int qualified;              /* 1: qualified for the finals of the cycle by the qualifying groups */
} SeasonNlRow;

/* Sorts the rows into the overall ranking of the edition: by league (A first), then group place, then points,
 * goal difference and goals scored, then nation id (UEFA Nations League regulations, overall ranking; the
 * results against the fourth of a group of four are not left out: inference, see the doc). */
void season_nl_rank(SeasonNlRow *rows, int n);

/* season_nl.c: the record of the UEFA Nations League takes its identity when a history row lands on it. */
void season_nl_history_record(uint32_t rec);

/* The leagues of the next edition. rows: the edition's results in any order (sorted on return). Writes the
 * league of each row of the sorted rows into next[]. The group winners go up and the last go down as the
 * edition says, then the leagues are filled to the sizes of `to` from the overall ranking: a league with room
 * takes the best of the leagues below it, a full league passes its lowest down, and the last league takes all
 * that are left (Russia in a game where it still plays). A row of pos 0 (a nation new to the competition) comes
 * after the nations of its league, so it takes room that is left there or goes down to the first league with
 * room (open item 183). Returns 0 for bad arguments, else the row count. */
int season_nl_next_leagues(const SeasonNlEdition *from, const SeasonNlEdition *to, SeasonNlRow *rows, int n,
                           int *next);

/* The play-off paths of a qualifying cycle (UEFA Euro 2020 and Euro 2024 qualifying, World Cup 2022): each path
 * takes `per` nations that did not qualify. rows: the ranked rows (season_nl_rank). Path p (0 = league A)
 * first takes the group winners of its league, then the other nations of its league by the ranking; the
 * places that a league cannot fill go to the best nations of the ranking that no path holds yet, path A
 * first. Writes paths * per nation ids into out (path p at out[p * per]), -1 for a place with no nation.
 * Returns the count of nations placed. */
int season_nl_paths(const SeasonNlRow *rows, int n, int paths, int per, int *out);

/* The semi-finals of a path of four (out[0..3] in the path's order, best first): 1 v 4 and 2 v 3, the
 * better nation at home. Writes home and away of each semi-final into pairs[0..3]. */
void season_nl_path_pairs(const int *path, int *pairs);

/* The game part (season_nl.c, season_nl_euro.c). */
/* One round of a league template (0x41 bytes a round) on a day: league_round_set_date 0x68a160 and the match
 * slot 0x68a1c0, as the class's own template 0x57d190. year: the year that d.year is an offset from. */
void season_nl_league_round(uint32_t rec, int i, SeasonNlDate d, int year);

/* The day of the year of a date (Date::Date 0x549f70, no weekday move), as the game compares dates. */
int season_nl_day_of_year(int day, int month1, int year);

/* The nation id of a team (club +0x53), or -1. */
int season_nl_nation_of_club(uint32_t club);

/* The national team of a nation id (national_team 0x53b3d0, or the national team row whose nation is the
 * nation), or 0. */
uint32_t season_nl_nation_team(int id);

/* 1 when the nation id has a continent in the data (nation +0x71), so its national team can take a squad. */
int season_nl_nation_can_play(int id);

/* The ranked results of the UEFA edition of year `year` (2018 for 2018-19) while the Nations League object
 * holds its tables (from its draw to the draw of the next edition), else 0. */
int season_nl_held_results(int year, SeasonNlRow *rows);

/* One round of a path play-off template (season_nl_euro.c): a cup round record of 0x68 bytes at rec + round *
 * 0x68, one match with extra time and penalties on day d (year offset from the campaign year), the final with
 * bit 0x200 for the fixed links. */
/* Test log: the Euro's team slots around its seeding (season_nl_euro.c). */
void season_nl_euro_log_slots(int day, int year);
/* A Euro with no slot array after a load gets one (season_nl_euro.c), called by slice 27's Euro slot code in a
 * season game. */
void season_euro_slots_after_load(uint32_t euro);

void season_nl_path_round(uint32_t rec, int round, SeasonNlDate d, int campaign, int teams);

/* Part b (season_gold_cup.c): the CONCACAF Gold Cup and the Finalissima. */
int season_gold_cup_of(uint32_t self);           /* 1 for the Gold Cup object (by its record) in a season game */
int season_gold_cup_teams(uint32_t self);        /* its teams in the edition it holds (12 or 16), else 0 */
int season_gold_cup_date_shift(uint32_t self, int *shift);   /* 1 for the Gold Cup: *shift in days */
/* 1 when the game has a Gold Cup object (or makes it now) and day doy of year y lies in its days
 * (season_intl_gold_busy). */
int season_gold_cup_busy(int y, int doy);
uint32_t season_gold_cup_entrants(uint32_t self);            /* the entrants into +0xee; 1, or 0 */
int season_gold_cup_record(uint32_t rec);
int season_finalissima_object(uint32_t obj);     /* 1 for the Finalissima object in a season game */
/* The Finalissima's round table in the wrap of 0x57a3c0 (season_uefa_super_cup.c): 1 when handled. */
int season_finalissima_round_table(void (*original)(void));        /* 1 for the record of the Gold Cup or the Finalissima */
/* 1 for the record of an object of slice 28 (season_nl.c): its fixtures are international (0x669ae0). */
int season_nl_international(uint32_t rec);
/* 1 while a Nations League object runs the Euro qualifying constructor (season_nl.c, load sweep). */
int season_nl_in_ctor(void);

#endif
