/* The Argentine Primera at its real size: halves on and off by the row, one table of up to 30, two zones with the
 * inter-zone rounds, the derby round and the relegation averages at the count (0.7.2 league sizes, slice 15; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D9 and D22).
 *
 * The class ArgPrm plays the Apertura in the league object (19 dates of create_schedule(-1) 0x404570, one meeting at
 * 0x405680), the Clausura in stage 0 (a League_Stage, 0x405a94, of the 19 dates of create_schedule(0)) and the
 * aggregate table of both in stage 1 (no dates, 2 meetings at 0x405b7c; ArgPrm::add_result 0x405c50 gives it every
 * result, match +0x42 = 1 at 0x405c7a). In a season game with the sizes on (league_sizes.py GROWN mode halves):
 * - a row of halves (Apertura and Clausura) plays each half in the rounds of the row: 19 for 20 clubs, 16 for two
 *   zones of 15 (14 zone matches, the rival in the round where both rest, and one full round of drawn opponents);
 * - a row of one table plays it in the league object over the dates of both halves, the Clausura stage has no dates
 *   and is complete from the start, and its honours are not given;
 * - the zones and the derby round are fixed fixture lists (League::create_fixtures 0x6835c0 plays [+0xea] when its
 *   edition is the league's, 0x683828..0x683a8f);
 * - the averages hold the clubs of the division (ArgPrm::update_relegation_averages 0x406570 finds a free record for
 *   a club of stage 1 with none, 0x4065d2..0x406622), and the places of honours and progress follow the count
 *   (tools/recomp/arg_prm.py).
 * The base game and RECOMP_LEAGUE_SIZES=off keep the class. */
#ifndef RECOMP_SEASON_SIZES_ARG_H
#define RECOMP_SEASON_SIZES_ARG_H

#include <stdint.h>

enum { SEASON_ARG_RIVAL, SEASON_ARG_DRAW, SEASON_ARG_DERBY };

/* A club of a zone in the Apertura of a rules row (season 2024: the 2025 season). */
typedef struct {
    int season;
    char zone;              /* 'A' or 'B' */
    const char *club;       /* the long name of the club in the season's database (club.dat +4) */
} SeasonArgZone;

/* A match of an inter-zone or derby round of a rules row: the rival match of the round where both clubs rest, a match
 * of the round of drawn opponents, or a derby of the derby round. */
typedef struct {
    int season;
    int kind;               /* SEASON_ARG_RIVAL, _DRAW, _DERBY */
    int round;              /* the real round (fecha), from 1 */
    const char *home, *away;
} SeasonArgPair;

extern const SeasonArgZone season_arg_prm_zones[];
extern const int season_arg_prm_zone_count;
extern const SeasonArgPair season_arg_pairs[];
extern const int season_arg_pair_count;

/* After the original create_schedule 0x404570 of the Primera (season_sizes_schedule): the dates of the stage (-1 the
 * Apertura or the one table, 0 the Clausura) in the rounds of the row; the template in eax, its count at count_at. */
void season_arg_schedule(uint32_t obj, int stage, uint32_t count_at);

/* The matches of a club of the Primera with n clubs this edition (+0xa7 of the league object). */
int season_arg_matches(uint32_t obj, int n);

/* Before League::create_fixtures 0x6835c0 of any league: the fixed list of the Primera's zones or derby round for
 * this league (the Primera, or its Clausura stage being made), or 0. */
uint32_t season_arg_fixtures(uint32_t league);

/* 1 when a division record is the Argentine third division (key [0x9cf688], whose clubs
 * arg_prm_swap_second_and_third_division_clubs 0x4068d0 exchanges with the second division, 0x4069a6..0x4069ae): the
 * size step takes its clubs as clubs with no division, so the second division can refill to its 25. */
int season_arg_third(uint32_t record);
/* That record (0 outside a season game): the clubs that the step sends down from the second division go there, where
 * the swap 0x4068d0 finds them again. */
uint32_t season_arg_third_record(void);

/* The order of the Primera's clubs for the size step (season_sizes.c step_index, D13): best first by the relegation
 * averages (the records of ArgPrm::update_relegation_averages 0x406570, sorted at 0x40671b; ArgPrm::
 * record_season_honours 0x405ca0 sends down the last two), so the step sends down the worst averages. A club with no
 * record keeps its table order after those with one. */
void season_arg_step_order(uint32_t obj, uint32_t *club, int n);

/* The relegation averages of an Argentine league object (+0x18, +0x34 records of 0x13 bytes: ArgPrm 0x405f02..0x405f16,
 * ArgSecond 0x409402..0x409416, the same layout). _fit: the records of the clubs of the division and a free record (id
 * -1, the edition) for each other place, n records (file names the league in the log). _empty: n free records, as the
 * original's empty records (0x406538..0x40654a); the block is the game's. */
void season_arg_averages_fit(uint32_t self, int n, const char *file);
void season_arg_averages_empty(uint32_t self, int n);

/* tools/recomp/arg_prm.py: a constant of the class for 20 clubs plus (count - 20) in a season game. */
uint32_t rt_arg_count(uint32_t original);

/* tools/recomp/arg_prm.py: the Apertura's date count that create_schedule(0) checks (0x4054a8), 19 in the base game. */
uint32_t rt_arg_apertura_dates(uint32_t obj, uint32_t original);

/* Pure, for the test (tests/seasons/arg_prm_plan_test.c): the rounds of two zones of z clubs (z odd), one meeting,
 * the rival of each club of zone A at rival[i] (an index of zone B) played in the round where both rest, and a full
 * round of drawn opponents draw[i] at round draw_at. round[r][k] = {home, away}: 0..z-1 zone A, z..2z-1 zone B.
 * Returns the rounds (z + 1), or 0. */
#define SEASON_ARG_MAX_ROUNDS 64
#define SEASON_ARG_MAX_CLUBS 60
int season_arg_zone_plan(int z, const int *rival, const int *draw, int draw_at,
                         int round[][SEASON_ARG_MAX_CLUBS / 2][2]);

/* Pure: the rounds of one table of n clubs (n even), one meeting, and a derby round of derby[i] (the partner of club
 * i) at round derby_at. Returns the rounds (n), or 0. */
int season_arg_derby_plan(int n, const int *derby, int derby_at, int round[][SEASON_ARG_MAX_CLUBS / 2][2]);

/* Pure: the derby partners of n clubs (n even). listed[i] is the index of club i's partner in the research list when
 * that partner is in the division, -2 when the list pairs club i with a club not in the division, -1 when club i is
 * not in the list. A club of no list takes the partner of a listed club whose partner is not in the division (the
 * place of the club it replaced), the best reputation (rep[i]) first. Clubs left over meet the club next to them by
 * reputation. derby[i] gets the partner of club i. Returns the pairs made by replacement. */
int season_arg_derby_pairs(int n, const int *listed, const int *rep, int *derby);

/* Pure (item 286): the clubs that a rules row of the Primera sends down, by the averages and by the annual table. */
void season_arg_down_places(int row, int *avg, int *tab);
/* The clubs that the swap 0x4068d0 exchanges at most (cmp bl, 4 at 0x4069fb). */
#define SEASON_ARG_SWAP_DOWN 4
/* Pure (item 286): the second division's relegation places of a row. With changing rules on the row's places
 * (season_arg_second.c moves the clubs past the swap's 4), with them off at most the swap's 4. */
int season_arg_second_down_places(int row_down, int changing_rules);
/* The clubs that ArgSecond::create_promotion_cup_stage 0x408630 sends down by the averages (cmp bp, 2 at 0x408666). */
#define SEASON_ARG_CLASS_DOWN 2
/* Pure (item 291): the clubs that leave the second division's class form: the class's 2 and the surplus over the next
 * row's count, which the size step sends down. next_clubs -1: no next row. */
int season_arg_second_class_down(int clubs, int next_clubs);
/* The places that the rules report shows for an Argentine league: the moves that the season plays, where the classes
 * keep them outside the place bytes. Any other league keeps the bytes. */
void season_arg_report_places(uint32_t obj, int *up, int *po_up, int *po_down, int *down);
/* The second division's clubs for the size step in the class's form, best average first (item 291). */
void season_arg_second_step_order(uint32_t obj, uint32_t *club, int n);

/* The Torneo Reducido of the second division (item 291). Each tie is a Cup_Stage of its own, so that the survivors of a
 * round can be paired again by their places. Sources: en.wikipedia.org 2015 Primera B Nacional (2nd to 5th, two legs),
 * 2017-18 (2nd to 9th, one-match quarter-finals), 2018-19 (two legs, paired again after the first round), 2022 Primera
 * Nacional (4th to 13th, the 3rd in round two and the 2nd in the semi-finals; two-leg semi-finals and final). */
#define SEASON_ARG_REDUCIDO_ROUNDS 4
typedef struct { int fresh, ties, legs; } SeasonArgReducidoRound;
typedef struct {
    int places;             /* the clubs that take part, from the 2nd place on */
    int rounds, stages;     /* the rounds, and the ties of all rounds */
    int final_pens;         /* 1: a level final goes to penalties; 0: the better placed club goes through */
    int zone_last;          /* zone rows: the last zone place that enters (4 or 8); 0: one table */
    int loser_round;        /* zone rows: the round that the loser of the zone winners' final enters, else -1 */
    int final_one;          /* 1: the final is one match on neutral ground */
    int level_pens;         /* 1: a level tie of two legs goes to penalties in every round */
    int down_each;          /* zone rows: the clubs that each zone sends down */
    int rel_tie;            /* zone rows: 1 when the second last clubs of the zones play for one more relegation */
    SeasonArgReducidoRound round[SEASON_ARG_REDUCIDO_ROUNDS];
} SeasonArgReducido;
/* Zone rows: sources en.wikipedia.org 2021 Primera Nacional (2nd to 4th, the final's loser in the semi-finals, penalties)
 * and 2023 and 2024 Primera Nacional (2nd to 8th, one match then two legs, a one-match final on neutral ground, the higher
 * seed through on a level score). Row 2019 was abandoned in real life: it plays the form of row 2020. */
#define SEASON_ARG_ZONE_ROUNDS 40
#define SEASON_ARG_ZONE_TIES 20
/* Pure: zone[i] (0 A, 1 B) of the i-th club of n sorted by club id: in turn until zone A holds a clubs. */
void season_arg_second_zone_split(int n, int a, int *zone);
/* Pure: both zones in home and away rounds (clubs 0 to a - 1, then a to a + b - 1). Each round holds (a + b) / 2 entries;
 * an entry of -1 is no match. Returns the rounds. */
int season_arg_second_zone_plan(int a, int b, int round[][SEASON_ARG_ZONE_TIES][2]);
/* Pure: the Reducido of a rules row of the second division. Returns 0 for a row with none. */
int season_arg_reducido_of(int row, SeasonArgReducido *out);
/* Pure: the ties of n survivors (n even) sorted best place first: out[2i] the better, out[2i + 1] the worse place. */
void season_arg_reducido_pairs(const int *sorted, int n, int *out);
/* The second division's edition plays this Reducido now. */
int season_arg_reducido_marked(uint32_t obj);
/* The places and the stage array of an edition with a Reducido, after the row's places. */
void season_arg_reducido_rules(uint32_t obj);
/* The next round, the day every tie made so far is complete (ArgSecond::advance_stage 0x4080c0). */
void season_arg_reducido_advance(uint32_t obj);
/* A result of a tie of the edition: 1 when the class's handler takes it as the winner of its Reducido, 0 when it is done. */
int season_arg_reducido_result(uint32_t obj, int stage, int result, uint32_t club);
/* The edition plays two zones (n clubs): 1, with the sizes of the zones. */
int season_arg_second_zones(uint32_t obj, int n, int *a, int *b);
/* The same for the edition being played: its stages are the zone rows' (a count that a save holds). */
int season_arg_second_zones_live(uint32_t obj, int n, int *a, int *b);
/* The fixed list of two zones, or 0; the entries with no match are dropped by season_arg_second_drop_fixture. */
uint32_t season_arg_second_zone_fixtures(uint32_t league);
void season_arg_second_zone_fixtures_end(void);
/* Before the Primera's chain: only the two winners of a season of two zones go up. */
void season_arg_reducido_chain(uint32_t second);
int season_arg_second_drop_fixture(uint32_t fixture);

/* The title stages of a season (season_arg_stages.c): none, two zone tables a half and a knockout of 16 after each
 * half (2025, 2026), two zone tables and a final of the zone winners with a match of the runners-up (2016), or a
 * knockout of the first 16 of each half's table (2027 on). */
enum { SEASON_ARG_KO_NONE, SEASON_ARG_KO_ZONES, SEASON_ARG_KO_FINAL, SEASON_ARG_KO_TABLE, SEASON_ARG_KO_PROMO };
/* The rows of the promoción that the Primera plays (2002/03 to 2011/12; row 2001 keeps the second division's own). */
#define SEASON_ARG_PROMO_FIRST 2002
#define SEASON_ARG_PROMO_LAST 2011
int season_arg_stage_kind(uint32_t obj, int *season);
/* The clubs of zone A and zone B of the Primera's plan this edition (made with its fixtures); returns the zone size,
 * 0 when there is none. */
int season_arg_zone_lists(uint32_t obj, uint32_t *a, uint32_t *b);
/* The club records that go down at the end of the season (arg_down), for the Clausura's knockout. Returns the count. */
int season_arg_relegated(uint32_t prm, uint32_t *club, int max);
/* The 17th and 18th of the averages (club[0] the 17th), the clubs of the promoción. Returns 2, or 0. */
int season_arg_promo_primera(uint32_t prm, uint32_t *club);
/* The promoción's results once its ties are complete: the Primera clubs that lost (down) and the second division
 * clubs that beat them (up), in pairs. Returns the count of pairs. */
int season_arg_promo_results(uint32_t prm, uint32_t *down, uint32_t *up);
/* The complete ties of the promoción in stage order (the 17th's first): each winner and loser. Returns the count. */
int season_arg_promo_ties(uint32_t prm, uint32_t *win, uint32_t *lose);

/* season_arg_stages.c: after ArgPrm::create_stages 0x405950 (the wrap in season_sizes_arg.c). */
void season_arg_stages_made(uint32_t self);
/* After League::update_for_date 0x6847c0 of any league (season_split.c): the Apertura's knockout when its table is
 * complete. */
void season_arg_update(uint32_t obj);
/* The empty advance_stage 0x5a8f60 (season_uefa_cup.c): 1 when it built the Primera's last stages. */
int season_arg_advance(uint32_t obj);
/* League's +0x44 base 0x686940 (season_playoffs.c): 1 when the result is of a knockout stage of the Primera. */
int season_arg_ko_result(uint32_t obj, int stage);
/* League::record_season_honours 0x684640 of the Primera or its Clausura: the winner and the finalist of the half's
 * knockout take rows 0 and 1 for the call (their pointers swap with those rows'); restore puts every row back. saved
 * holds SEASON_ARG_HONOURS_ROWS. */
#define SEASON_ARG_HONOURS_ROWS 64
int season_arg_honours_swap(uint32_t league, uint32_t *saved);
void season_arg_honours_restore(uint32_t league, const uint32_t *saved);
/* The round name wrap (season_round_names_game.c): the names of the zone tables of the Primera, or NULL. */
const char *season_arg_stage_name(uint32_t context, int id);

#endif
