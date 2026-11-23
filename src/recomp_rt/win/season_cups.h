/* The round formats of the domestic cups by season (0.6.0 slice 21; spec docs/specs/2026-10-16-seasons-0.6.0-spec.md,
 * slice 21). Research and evidence: docs/research/2026-10-17-cup-round-record.md.
 *
 * A cup class of the original makes its round table at every season start (set_up_rounds, vtable +0x8c, which
 * calls create_round_table) and at a new game. A round is 0x68 bytes. Five of its fields say how the round is
 * played: the match flags of the first match (+0x0d), the flags of the deciding match (+0x0f, the replay or the
 * second leg), the replays allowed (+0x20), the legs (+0x21) and the days to the replay or second leg (+0x22).
 * The table cup_formats.tsv gives one word for each round (the types below). This file holds the part that does
 * not touch the game's memory, so a test on the host can run it. season_kind_cups.c is the rule kind. */
#ifndef RECOMP_SEASON_CUPS_H
#define RECOMP_SEASON_CUPS_H

#include <stdint.h>

typedef struct {
    const char *file;        /* the source file of the cup class: eng_fa_cup */
    int season;              /* the first season of the format (start year), 2002 or later */
    int rounds;              /* the round count that the class makes (cup+0x3c) */
    const char *types;       /* one letter for each round: . E P R S L N Q H (make_cups_table.py) */
    const char *sentence;    /* the news sentence of the change */
} SeasonCupRow;

extern const SeasonCupRow season_cup_rows[];     /* season_cups_table.c, sorted by file and season */
extern const int season_cup_row_count;

/* The latest row of the cup that is not later than the season, or NULL (the cup keeps the original's rounds). */
const SeasonCupRow *season_cups_row(const char *file, int season);

/* The five fields of a round. */
typedef struct {
    uint16_t first;          /* +0x0d match flags of the first match (both legs of a two-leg tie) */
    uint16_t deciding;       /* +0x0f flags of the deciding match */
    uint8_t replays;         /* +0x20 */
    uint8_t legs;            /* +0x21 */
    uint8_t days;            /* +0x22 */
} SeasonCupRound;

/* Match flag bits (docs/research/2026-10-17-cup-round-record.md, section 2). */
#define CUP_PENALTIES 0x0001u
#define CUP_EXTRA_TIME 0x0002u
#define CUP_NO_AWAY_GOALS 0x0004u
#define CUP_AWAY_AFTER_ET 0x4000u
#define CUP_RULE_BITS 0x4007u       /* the bits that a type sets; the draw bits 0x100, 0x200, 0x400 stay */

/* Sets a round to the type of a letter (. keeps it). Returns 1 when the round changed, 0 when it did not or the
 * letter is not a type. */
int season_cups_apply(char type, SeasonCupRound *round);

/* Slice 21b (season_kind_cup_breaks.c): 1 while a constructor runs with SEASON_CUPS_NEVER, so League::create_fixtures
 * 0x6835c0 makes no fixture and returns 1 (its one wrap is in season_uefa_league.c). */
int season_cup_breaks_no_fixtures(void);

/* ---- Cups that the rules table does not hold in some seasons (slice 21b, season_cup_breaks.c) ----
 *
 * An edition of a cup is its season year (cup+0x40). The rules table puts the edition of a split-season cup
 * (2009/10) in the row of season 2009, and the edition of a calendar-year cup (the Norwegian Cup of 2010) in
 * the row of season 2009 (2009/10), as docs/research/rules-0.6.0/club-world.md says. These functions read the
 * rules table (season_rules.h) and no game memory. */
#define SEASON_CUPS_NEVER 2999   /* the edition of a cup that the table never holds again */

/* 1 when the table puts edition Y of the cup in the row of season Y - 1: its first row (2001/02) holds the real
 * season 2002 alone. 0 for a split-season cup and for a file with no row. */
int season_cups_calendar(const char *file);

/* 1 when the table holds the edition, 0 when the row says not held, -1 with no row. After the last row of the
 * table the rule stays (Q248). */
int season_cups_held(const char *file, int edition);

/* The first edition from `edition` on that the table holds, SEASON_CUPS_NEVER when none, 0 with no row. */
int season_cups_next(const char *file, int edition);

/* ---- Four-club super cups (slice 21c, season_cups_more.c; the game part is season_super_cups.c) ----
 *
 * A super cup class of the original makes one round of two clubs. In a season whose rules table row gives 4
 * clubs, the cup plays two semi-finals and a final, one match each, on the dates of the row below. */
typedef struct {
    const char *file;        /* the class file (ita_super), or the file of a new competition (new_tur_super) */
    int season;              /* the first season of these dates */
    uint16_t flags;          /* the match flags (+0x0d) of both rounds: CUP_PENALTIES, and CUP_EXTRA_TIME */
    int8_t sf_day, sf_month, sf_year;   /* the semi-finals: day 1 to 28, month 0 to 11, year after the season year */
    int8_t f_day, f_month, f_year;      /* the final */
} SeasonSuperFour;

/* The four-club row of a cup in a season, or NULL when the rules table gives it two clubs (or no row). After the
 * last row of the table the rule stays (Q248). */
const SeasonSuperFour *season_super_four(const char *file, int season);

/* The four clubs: league[] holds the league's clubs of the last season by place (league[0] the winner, league[1]
 * the runner-up, 0 ends the list), cw and cru the cup winner and runner-up (0: none). A cup place whose club
 * already has a place, or none, goes to the best league club that has no place yet: third, then fourth, and so on
 * (the Supercopa from 2019/20, O342). out[] gets the semi-final pairs: league winner v cup runner-up, cup winner v
 * league runner-up. Returns 0 when four clubs are not found (the cup then keeps the class's two clubs). */
int season_super_four_pick(const uint32_t *league, int count, uint32_t cw, uint32_t cru, uint32_t out[4]);

/* The rules report lines "cupmore" of slice 21c (season_super_cups.c); file is the report's FILE *. */
void season_cups_more_report(void *file, int season, const char *phase);

/* ---- New cups (slice 21c, season_cups_more.c; the game part is season_new_cups.c) ----
 *
 * A new cup of slice 21c is an object of the class ItaSuper (a Cup of the original) with its own vtable copy, made
 * by the new competitions module (season_comps.h). An edition is its year (+0x40). The rules table puts edition E
 * in the row of season E - row_shift (row_shift 1 for a calendar-year cup in the row of the season before). A
 * round is one match or two legs; the dates are the first match of each round, by day, month (0 to 11) and year
 * after the edition year. */
typedef struct {
    int8_t legs;             /* 1 or 2 */
    uint16_t flags;          /* the match flags of the deciding match: CUP_PENALTIES, CUP_EXTRA_TIME */
    int8_t day, month, year; /* the first match */
    int8_t gap;              /* the days from the first leg to the second */
} SeasonNewRound;

typedef struct {
    const char *file;        /* new_tur_super */
    int from;                /* the first edition of this format */
    int row_shift;
    int clubs;               /* 2: league champion v cup winner; 0: every club of the division; n: n clubs;
                              * -1: the game does not play these editions (the reason is in the table's comment) */
    SeasonNewRound round[5]; /* from the final back: final, semi-finals, quarter-finals, the rounds before them */
    int8_t picks[2];         /* item 145: the clubs of each division by last place (0, 0: half from each, the rest
                              * from the other) */
    uint8_t extra;           /* item 145: NEW_CUP_CUP_WINNERS, NEW_CUP_HOLDERS; item 148: NEW_CUP_PAIRED */
} SeasonNewCup;

/* Item 145, the clubs that a row adds to its picks. NEW_CUP_CUP_WINNERS: the last winner of each division's national
 * cup (NewCupClubs.cup), or when it already has a place the next club of that division. NEW_CUP_HOLDERS: the last
 * winner of this cup, or when it has a place or none the next club of the first division. */
#define NEW_CUP_CUP_WINNERS 1u
#define NEW_CUP_HOLDERS 2u
/* Item 148: each tie of the first round drawn pairs a club of the first division with a club of the second
 * (season_new_cups.c, paired_draw). */
#define NEW_CUP_PAIRED 4u

/* The format of an edition, or NULL (no format: before the first, or a file that is not a new cup). */
const SeasonNewCup *season_new_cup(const char *file, int edition);

/* The format that the game plays for an edition: its own with changing rules on (rules not 0); with changing rules off
 * the format of the edition that the start season's row holds, for every edition, as the rules of the start season
 * stay (open item 131). NULL as season_new_cup. */
const SeasonNewCup *season_new_cup_played(const char *file, int edition, int rules, int start);

/* 1 when the game plays the edition (the table holds it and the format can be played), 0 when not, -1 with no row.
 * After the last row of the table the rule stays (Q248). */
int season_new_cup_held(const char *file, int edition);

/* The edition that the row of season s holds: s, or s + 1 for a calendar-year cup in the row of the season before
 * (row_shift 1). */
int season_new_cup_row_edition(const char *file, int season);

/* The late start of the All-Ireland cup (season_new_cups.c, season_new_cups_day): 1 on day (0 = 1 January) when that
 * is the original update day of Ireland's league table entry, 20 June (day 170: key_nation_build_table 0x667150 builds
 * it as Date(20 June 2001) at 0x66840f..0x668438, the same day in a leap year), the entry's update day entry_day is
 * another day, the cup's edition is before the game year and the game plays the cup in that year (held 1). */
#define SEASON_NEW_CUP_LATE_DAY 170
int season_new_cup_late(int day, int entry_day, int edition, int year, int held);

/* The first edition from `edition` on that the game plays, SEASON_CUPS_NEVER when none, 0 with no row. */
int season_new_cup_next(const char *file, int edition);

/* The knockout of n clubs: the ties of the first round and the round count (a first round with byes when n is not a
 * power of 2). Returns 0 for fewer than 2 clubs or more than 32. */
int season_new_cup_layout(int clubs, int *ties0, int *rounds);

/* The round code of round r of n (name function 0x4b4850): 150 the final, 130 the semi-finals, 120 the
 * quarter-finals, 10, 20, 30 the rounds before them from the first. */
int season_new_cup_round_code(int rounds, int r);

/* The game part of the new cups (season_new_cups.c), for the wraps of season_super_cups.c: the file of the def whose
 * object is being made, or NULL; the clubs (after the team picker) and the rounds (after create_round_table, the
 * table in cpu.eax) of a new cup. */
const char *season_new_cups_making(void);
int season_new_cups_teams(uint32_t cup, const char *file);
int season_new_cups_clubs_ready(const char *file, int edition, uint32_t record);
int season_new_cups_rounds(uint32_t cup, const char *file, uint32_t count_ptr);

/* The game part of the group stages (season_cup_groups.c): after the class's create_round_table (stage, count_ptr as
 * its wrap read them; the table in cpu.eax); slot 10 of a cup (1 when it was a cup of a group format, which this call
 * advanced); the knockout stage of a cup whose knockout is drawn, or 0; the stage array before a load. */
void season_cup_groups_table(uint32_t cup, uint32_t stage, uint32_t count_ptr);
int season_cup_groups_advance(uint32_t cup);
uint32_t season_cup_groups_ko(uint32_t cup);
void season_cup_groups_load(uint32_t cup);

/* Forgets the big stage arrays of this game (season_change_reset: a new game, a load, the end of a game). */
void season_cup_groups_reset(void);

/* ---- Group stages (slice 21c, season_cups_more.c; the game part is season_cup_groups.c) ----
 *
 * A cup whose season has a group stage: the class's first base_rounds rounds (0: none), then groups of the clubs
 * that won them and the clubs that had not played yet (less the byes), then a knockout of ko_clubs clubs: per_group
 * clubs of each group, the best clubs of the next place, and the byes (the clubs of the cup that play in a European
 * competition, when byes is 1), on the rounds of ko[] (from the final back, as SeasonNewRound). */
typedef struct {
    const char *file;        /* sco_lge_cup */
    int from;                /* the first season of the format */
    int groups;              /* 0: the class's format from this season */
    int base_rounds;         /* the class's rounds before the groups */
    int meetings;            /* 1 single, 2 double round robin */
    int per_group;           /* the clubs of each group that go through */
    int byes;                /* 1: the European clubs of the cup go straight to the knockout */
    int ko_clubs;            /* the clubs of the knockout: a power of 2 */
    int8_t days[10][3];      /* the group match days: day, month (0 to 11), year after the season year */
    SeasonNewRound ko[5];    /* the knockout rounds from the final back */
    int nation_pots;         /* item 145: 1 the pots take the clubs of one division after the other (each group gets
                              * the same share of each division); 0 the pots by strength alone */
    int8_t draw[3];          /* item 145: the day of the group draw (day, month 0 to 11, year after the season year);
                              * day 0: at once, when the season's round table is made */
} SeasonCupGroups;

/* The group format of a cup in a season, or NULL (the class's format). After the last row the rule stays (Q248). */
const SeasonCupGroups *season_cup_groups(const char *file, int season);

/* The sizes of g groups for n clubs, as even as they can be (the larger groups first). Returns 0 when a group
 * would have fewer than 2 clubs or more than 10. */
int season_cup_group_sizes(int clubs, int groups, int *sizes);

/* The match days of a group of n clubs that meet `meetings` times: n - 1 rounds a meeting, n with a club resting
 * when n is odd (League::create_fixtures 0x6835c0 adds a resting place, 0x683c1b). */
int season_cup_group_days(int clubs, int meetings);

/* After Cup::map_fixture_tree 0x518790 of a cup (season_cup_groups.c): a cup with base rounds and groups marks its
 * later slots. The wrap is in season_uefa_qual.c (one wrap for slices 21c and 23). */
void season_cup_groups_mapped(uint32_t self);

/* Before Cup::map_fixture_tree 0x518790 (season_cup_entrants.c): a domestic cup's empty places get clubs of its
 * nation from the lowest division of its list and below (gate fix O151). */
void season_cup_fill_entrants(uint32_t cup);

/* Test only, RECOMP_CUP_GROUPS_READ=1: reads the cup's round table as Comp::build_match_date_list 0x48d500 does (the
 * Schedule tab, scout duty), before the rules report writes its cupmore line (season_super_cups.c). */
void season_cup_groups_read_test(uint32_t cup);

void season_super_cups_trace(uint32_t cup, uint32_t today);   /* test log, RECOMP_SUPER_TRACE (O326) */

#endif
