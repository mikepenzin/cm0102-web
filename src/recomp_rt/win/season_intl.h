/* International competitions in the original (0.6.0 slice 27; spec docs/specs/2026-10-16-seasons-0.6.0-spec.md,
 * Q248, Q276, Q277, Q288, Q366, Q367). Research and evidence: docs/research/2026-10-18-international.md.
 *
 * The editions of the finals come from one table (season_intl_table.c, made by tools/seasons/make_intl.py
 * from the rules table): edition year, season, team count, golden goal, first day and hosts. Slices 28 and 29
 * add their competitions to the same table. */
#ifndef RECOMP_SEASON_INTL_H
#define RECOMP_SEASON_INTL_H

#include <stdint.h>

#include "season_nl.h"                 /* SeasonNlGoldCup */

typedef struct {
    const char *file;       /* source file of the class: world_cup, euro_champ, fifa_confed, olympics, goldcup,
                               african_nations, asia_nations, oceania_nations */
    int season;             /* the season of the rules table row (2003 for 2003/04) */
    int year;               /* the edition year that the class holds at +0x40 */
    int teams;              /* teams in the finals */
    int golden;             /* 1: golden goal in extra time */
    int start_year;         /* the year of the first day of the finals (2025 for the 2026 edition of ACN) */
    int month, day;         /* first day of the finals (month 1 to 12), 0 when the table gives none */
    const char *hosts;      /* host nations: ";" between hosts, "/" between the names a database can use */
    const char *row;        /* id of the rules table row */
} SeasonIntlEdition;

extern const SeasonIntlEdition season_intl_editions[];
extern const int season_intl_edition_count;

/* 1 for a season game: the season code is on, and the start year is not 2001 or changing rules is on. The
 * base game (2001/02, changing rules off) and RECOMP_SEASON_CODE=off give 0. */
int season_intl_on(void);

/* The edition of a class in a year, or NULL. After the last edition of its table a class continues on its last
 * cycle (0.6.2 decision A): the edition of such a year is a copy of the last table edition with that year, its
 * season and first day moved with it, no golden goal and no host (row "after-table"). The pointer of a copy stays
 * good for the next 31 lookups. An ended class (the Confederations Cup) has none. */
const SeasonIntlEdition *season_intl_edition(const char *file, int year);

/* The edition of a class in a year of the table only, or NULL: the host rows (season_intl.c), which go only to
 * the table's years until the hosts after the table (0.6.2 slice 5). */
const SeasonIntlEdition *season_intl_table_edition(const char *file, int year);

/* The first edition of a class whose season is the start season or later, after the table too, or NULL. */
const SeasonIntlEdition *season_intl_first(const char *file, int start_season);

/* The next edition of a class after the edition year, after the table too, or NULL. */
const SeasonIntlEdition *season_intl_next(const char *file, int year);

/* The calls of the code sites (tools/recomp/seasons.py). Each gives the original's value when
 * season_intl_on() is 0, so the base game is unchanged. */
uint32_t rt_intl_first_year(uint32_t cls, uint32_t year);              /* ctor: year after the original's rule */
uint32_t rt_intl_next_year(uint32_t cls, uint32_t year, uint32_t step); /* start_new_season: year + step */
uint32_t rt_intl_round_flags(uint32_t cls, uint32_t year, uint32_t flags); /* golden goal bit 0x80 */
uint32_t rt_intl_wc_holder(uint32_t holder);       /* a qualifying section's holder: 0 in a season game */
uint32_t rt_intl_wc_holder_slot(uint32_t year);    /* 1 when the World Cup of the year gives the holder a slot */

/* The draw of a host row (daily_update 0x5faaae): the choice in a season game (Q6, Q32; 0.6.2 slice 5). */
uint32_t rt_intl_host_pick(uint32_t row, uint32_t r);

/* The host of a history row (Cup::record_season_honours 0x519c2f): club, or the national side of the edition's
 * first host when club is 0 in a season game (0.6.2 open item 18). */
uint32_t rt_intl_history_host(uint32_t cup, uint32_t club);

/* The hosts of the news of the qualified nations (issue 146): the two host fields on the stack when the news is
 * made (0x777b07), and host k of the edition when its text is written (0x777d07). */
void rt_intl_news_hosts(uint32_t key, uint32_t first_at, uint32_t second_at);
uint32_t rt_intl_news_text_host(uint32_t key, uint32_t year, uint32_t stored, uint32_t k);

/* A new game (season.c, before the original makes it): the start of Australia's move to Asia. */
void season_intl_at_create(void);

/* The Oceania section's left-out nation at 0x91fc89 (New Caledonia): none with Australia in Asia. */
uint32_t rt_intl_oceania_left_out(uint32_t id);

/* 1 when a European Championship without a host goes on (seasons.py 0x57be3d): in a season game. */
uint32_t rt_intl_euro_no_host(void);

/* The template function of the European Championship (vtable +0x3c, 0x57ab90) for editions of 24 teams
 * (season_intl_euro.c). The wrap of 0x57ab90 calls it at the entry state with the original o_0057ab90. It
 * completes the call (it calls the original or makes the knockout of 24 itself) and gives the result. */
uint32_t season_intl_euro_template(void (*original)(void));

/* The template function of the African Cup of Nations (cls INTL_ACN, 0x4012e0) and the Asian Cup (INTL_ASIAN,
 * 0x40eb00) for editions of 24 teams (season_intl_cups.c). The wraps call it at the entry state with the
 * original. It completes the call and gives the result. */
uint32_t season_intl_cup_template(uint32_t cls, void (*original)(void));

/* The template function of the Copa America (0x5e0900) for editions of 16 teams (season_intl_copa.c). The wrap
 * calls it at the entry state with the original o_005e0900. It completes the call and gives the result. */
uint32_t season_intl_copa_template(void (*original)(void));

/* The edition whose format a class plays in an edition year: that year's with changing rules on, the start
 * season's with changing rules off (Q232). NULL outside a season game. */
const SeasonIntlEdition *season_intl_rule_edition(const char *file, int year);

/* The days by which the finals of a class in an edition year move, so the first group day is the table's
 * (0 for the base game, an edition the table does not hold, or one without a date). */
int season_intl_date_shift(uint32_t cls, int year);
/* 1 when the fix of original-faults group confed acts (original_faults.c): the Confederations Cup of an edition that
 * keeps the original's dates plays from the day of the 2003 edition, 18 June, in place of 29 July. */
int season_intl_confed_june(void);

/* The confederations, in the order of the key continent ids 0x9cfa08 to 0x9cfa1c (docs/research/
 * 2026-10-16-sweep-international.md, section 8): the id of confederation c is the dword at 0x9cfa08 + 4 * c. */
enum {
    SEASON_CONF_AFRICA, SEASON_CONF_ASIA, SEASON_CONF_EUROPE, SEASON_CONF_NORTH_AMERICA, SEASON_CONF_OCEANIA,
    SEASON_CONF_SOUTH_AMERICA, SEASON_CONFS
};

/* 1 when day doy (0 is 1 January) of year y lies in the days of a final that the nations of confederation conf
 * play in a season game: from 2 days before the first group day that the class plays (the original's day moved by
 * season_intl_date_shift) to the end of its span for its team count. The World Cup counts for every
 * confederation, the Copa America for CONMEBOL and CONCACAF in every edition (the guests of 2016 and 2024 are
 * CONCACAF's; the guests that the original takes for 12 teams, 0x5e1fbd..0x5e206c, were not read). The Confederations Cup and the Olympics
 * (8 nations of all confederations, an under-23 side) do not count. 0 outside a season game. */
int season_intl_finals_busy(int conf, int y, int doy);

/* The CONCACAF Gold Cup of a year (open item 55; the lookup of season_gold_cup.c, slice 28 part b): with changing
 * rules on, the edition of season_nl_table.c, after its last one the last again every two years (Q248); with it
 * off, the first edition after the start year, again every two years (Q232). NULL for a year with none. A
 * repeated edition is a static copy with its year, valid to the next call. */
const SeasonNlGoldCup *season_intl_gold_edition(int year);

/* 1 when day doy of year y lies in the days of a Gold Cup after the start year, from 2 days before its first day
 * to the end of its span (as season_intl_finals_busy). 0 outside a season game. The caller asks only when the game
 * has a Gold Cup object (season_gold_cup_busy). */
int season_intl_gold_busy(int y, int doy);

/* The first group day of the finals of class cls in edition year `year` that the game plays (the days of
 * season_intl_finals_busy), as days from 1 January 1970 in *day; 0 when the class plays no finals that year. */
int season_intl_finals_first_day(uint32_t cls, int year, long *day);
/* Day doy (0 is 1 January) of year y as days from 1 January 1970. */
long season_intl_abs_day(int y, int doy);

/* The source file of a class of the code sites ("?" for none). */
const char *season_intl_class_file(uint32_t cls);

/* Inside a template function of a class (its stage builder): every date that the template makes moves days more,
 * until the same call with -days (the knockouts of the Asian Cup of 16, season_intl_cups.c, and of the Copa America
 * of 16, season_intl_copa.c). */
void season_intl_shift_more(int days);
int season_intl_shift_now(void);               /* the shift that the template's dates take now */

/* Inside the template of a knockout (season_intl_cups.c): the whole weeks by which the knockout must move so that
 * its first date (day, month from 0, weekday flag of Date::Date) comes 3 or more days after today. */
int season_intl_rest_delay(const char *file, int year, int day, int month0, int weekday);
/* The venue flags of a group that takes another group's template (season_intl_cups.c, 0.6.2 item 217). */
void season_intl_twin_venues(uint32_t rec, uint32_t rounds_out);

/* Moves a date (day 1 to 31, month from 0, year) by shift days. */
void season_intl_shift_date(int shift, int *day, int *month0, int *year);

/* Fills the empty Euro team slots in a season game (season_intl.c): at the seeding and at the draw. */
void season_intl_euro_fill(uint32_t self);

/* The hosts of the host row of a competition (its key id, the value of its key global) and an edition year: up
 * to six nation ids in the order of the table for a fixed row (slice 29: three hosts in 2026), the chosen pair of
 * a drawn row (choice 0 to 2, as find_hosts 0x5fa788 reads it). Gives the count; 0 for no row or a row not yet
 * drawn (0.6.2 slice 5: the rows after the table are drawn 5 years before the finals). */
int season_intl_host_ids(uint32_t key, int year, int ids[6]);

/* A host row (season_intl.c): 0x22 bytes, six host fields (nation ids) from +6, the choice at +0x1e (0xfd fixed).
 * The constructor HostCountry 0x5f97c0 makes 57 rows (0x5f98bd) before init_default_table 0x5f9970 fills them. */
#define SEASON_INTL_ROW_SIZE 0x22u
#define SEASON_INTL_ROW_PAIRS 0x6u
#define SEASON_INTL_ROW_CHOICE 0x1eu
#define SEASON_INTL_CHOICE_FIXED 0xfdu
#define SEASON_INTL_ORIGINAL_ROWS 57

/* The original's host rows (count rows at rows, season_intl_rows.c): the host fields after the first two of each
 * fixed row become -1, as HostCountry::init_default_table 0x5f9970 leaves them unset (0.6.2 open item 103). Gives
 * the count of rows changed. Only for the original's own table: a fixed row of the table (row_write) may hold
 * more hosts. */
int season_intl_rows_clean(uint8_t *rows, int count);

/* 0.6.2 item 181: the draw of a finals edition of 2001/02 in a 2001 game (season_intl_rows.c). The original's
 * fixed row of one or two hosts that differ from the n hosts of the table (ids) takes them in pair 2 with choice 1,
 * and keeps the original's hosts in pair 1. Gives 1 when the row changed, 0 when it holds the table's hosts or has
 * no room (a row that is not fixed, more than two hosts). season_intl_row_undo_draw_hosts gives a copy of such a
 * row the original's form back (fixed, pair 1). Gives 1 when it changed. */
int season_intl_row_draw_hosts(uint8_t *row, const int32_t *ids, int n);
int season_intl_row_undo_draw_hosts(uint8_t *row, const int32_t *ids, int n);
/* The host row of an edition of 2001/02 at its draw, in a 2001 game with changing rules on (season_intl.c): the
 * table's hosts when the original's row names others. Nothing in other games. */
void season_intl_draw_hosts(const char *file, int year);

/* ---- Hosts after the table (0.6.2 slice 5, decisions Q4, Q5, Q6, Q32; season_intl_rules.c) ---- */

/* The hosts announced for an edition after the table (Q5), or NULL: the World Cup of 2034, the Euro of 2032, the
 * Olympics of 2028 and 2032. The format of these editions is the continued one (decision A). */
const char *season_intl_announced_hosts(const char *file, int year);

/* The host of an edition with no host of its own in the history row only (the Euros of 2020 and 2028: hosts with
 * no automatic places), or NULL. */
const char *season_intl_history_host(const char *file, int year);

/* The hosts that the news of the qualified nations names for an edition whose host row stays empty (the Euros of
 * 2020 and 2028), or NULL: ";" between at most two nations. */
const char *season_intl_news_hosts(const char *file, int year);

/* The original's host rows of a class after the table (Q4): `rows` rows from `base` every `step` years, each
 * moving on by rows * step years (HostCountry::init_default_table 0x5f9970, daily_update 0x5faafa). Gives 0 for a
 * class whose hosts rotate (Q32). */
int season_intl_host_slots(const char *file, int *base, int *step, int *rows);

/* The slot (0 to rows - 1) of the original's row that an edition year takes: the rows in order, one an edition. */
int season_intl_host_slot(const char *file, int year);

/* Q32: the nations that hosted a class in the table, each once in the order of their first edition, those that
 * still exist and are still in the class's confederation; `alive` says it of a host name ("a/b" names one
 * nation) for a class. Gives the host of rotation step k (from 0),
 * or NULL for none. The name lives in a buffer that the next call writes again, in the same order. */
const char *season_intl_rotation_host(const char *file, int k, int (*alive)(const char *file, const char *name));

/* The count of the hosts of the rotation of a class (Q32). */
int season_intl_rotation_count(const char *file, int (*alive)(const char *file, const char *name));

/* The rotation step of an edition after the table: its count of editions after the last table edition, from 0. */
int season_intl_rotation_step(const char *file, int year);

/* Q6: the pair a draw takes, from the original's draw r (0 to 2) and the pairs that name only nations that exist
 * (valid[k] 1): the drawn pair when valid, else a valid pair chosen by redraw (0 to the count of valid pairs - 1)
 * among the other pairs. -1 when no pair is valid (then the host rotates, Q32). */
int season_intl_pick_pair(const int valid[3], int r, int redraw);

/* The game's test of a host name, for the rotation of season_intl_edition (season_intl.c sets it; NULL: all live). */
void season_intl_set_alive(int (*alive)(const char *file, const char *name));

/* The inter-confederation ties of the World Cup of a year that this run built (season_intl_qual.c): up to max
 * pairs into teams (two teams each). Gives the pair count. */
int season_intl_wc_ties(int year, uint32_t *teams, int max);
void season_intl_qual_reset(void);             /* a load or new game: no ties (load sweep) */

/* Qualifying campaigns that a save of 0.6.1 or older made in the original's shape (0.6.2 slice 4b, open item 58,
 * season_intl.c): 1 while the object holds its marked campaign year, so the gates keep the original's shape. The
 * marks are the field of the season part of version 5 (season_change.c): u32 count, then u32 record id and u32
 * campaign year each; read gives 0 for damaged bytes. */
int season_intl_old_shape(uint32_t obj);
void season_intl_mark_old(uint32_t obj);       /* the load repair: the object's running campaign */
void season_intl_marks_reset(void);
int season_intl_marks_count(void);
uint32_t season_intl_mark_object(int i);       /* the object of mark i while it holds its year, else 0 */
uint32_t season_intl_marks_size(void);         /* 0: no mark, no field */
void season_intl_marks_write(uint8_t *out);
int season_intl_marks_read(const uint8_t *in, uint32_t n);

/* The classes of the code sites (cls of the calls above). */
enum {
    INTL_WORLD_CUP, INTL_EURO, INTL_CONFED, INTL_COPA, INTL_ACN, INTL_ASIAN, INTL_OFC, INTL_OLYMPICS,
    INTL_CLASSES
};

/* ---- Nation records (season_intl_rules.c; season_intl.c applies them to the data) ---- */

/* The renamed nations of int-world.tsv ("nation rules" rows, the season of the row). */
typedef struct {
    int season;
    const char *old, *name, *short_name, *code, *adjective, *row;
} SeasonIntlRename;
extern const SeasonIntlRename season_intl_renames[];
extern const int season_intl_rename_count;

/* 1 when the game renamed a nation from old to now: old is an earlier name of now in a chain of the rename table
 * (Turkey for T\xfcrkiye), without case, and no nation record holds old any more. A database that holds Serbia
 * beside its dead record Yugoslavia gives 0 for Yugoslavia: that nation was never renamed. */
int season_intl_renamed_from(const char *old, const char *now);

/* 1 when the data holds a later name of the chain of rename row i (Serbia for Yugoslavia), by held(name, ctx),
 * which compares names without case (the databases from 2019 write "eSwatini"). The record of the old name is
 * then a dead record beside the live one. */
int season_intl_later_held(int i, int (*held)(const char *name, void *ctx), void *ctx);

/* The season from which a nation is a UEFA member, for a nation that a database may hold with no continent
 * (Kosovo, admitted in May 2016: the April 2015, October 2015 and April 2016 databases), or 0. */
int season_intl_uefa_from(const char *name);

/* The season from which a nation is a CONCACAF member, for a nation that a database holds with no continent though
 * it plays CONCACAF's own competitions (the members that are not FIFA members: Martinique, Guadeloupe, French Guiana,
 * Saint-Martin, Sint Maarten and Bonaire in the databases from October 2016), or 0. */
int season_intl_concacaf_from(const char *name);

/* The season from which a nation is an AFC member, for a nation that a database holds with no continent (Timor in
 * the 3.9.68 data: Timor-Leste joined the AFC in 2005, item 200 of 0.6.2), or 0. */
int season_intl_afc_from(const char *name);

/* The real draw of a finals edition whose qualifying began before the game (O259 A9): the names of its
 * finalists, ';' between nations and '/' between the names of one nation, or NULL. */
const char *season_intl_real_draw(const char *file, int year);
/* 1 in a season game that starts in the finals' season or the one before, for an edition of that table. */
int season_intl_late_start(const char *file, int year);
/* The real inter-confederation play-off of a World Cup (O259 A16): k 0 the AFC team, 1 its partner, 2 and 3 the
 * other tie; for 2026 the six of the play-off tournament (k 0 to 5, season_wcq26.c); NULL when the table has
 * none. */
const char *season_intl_real_tie(int year, int k);
/* The national team of the first nation that one of the names ('/' between them) gives: no case, not a dead
 * record, a continent; 0 when the data has none (season_intl.c). */
uint32_t season_intl_team_named(const char *names);

/* After the OFC Nations Cup's draw of 6 (the wrap of 0x799e30 in season_intl.c): an edition of 8 teams gets two
 * more, in groups of 4 (0.6.2 open item 45, season_intl_ofc.c). */
void season_intl_ofc_draw(uint32_t self);
/* 1 for a team that the OFC draw now being made gave a qualifying place (0.7.0, option b): ofc_hosts keeps it. */
int season_intl_ofc_kept(uint32_t team);
/* The OFC Nations Cup's template 0x798b60 (the wrap in season_intl.c, inside its date shift): a group made after a
 * draw of 8 gets a second kick-off slot on each match day. */
void season_intl_ofc_template(void (*original)(void));
/* After the OFC Nations Cup's set_up_rules 0x799690 (the wrap in season_intl.c): the stage count and draw date of the
 * edition of one group, home and away (2008), the stage count of the group of 6 and its final (2004), and the stage
 * count of 2 again after them. */
void season_intl_ofc_rules(uint32_t self);
/* A load (intl_loaded): the edition of one group that an older build set up with two stages takes the stage count 0
 * and its group's matches of each team (+0xa7), so it ends with its group. */
void season_intl_ofc_loaded(void);
/* Slot +0x30 0x595160 of the OFC Nations Cup's edition of one group: League::record_season_honours 0x684640 in place
 * of the knockout's; of the group of 6: the final's in stage[0]; then the return. 1 when it ran
 * (season_cup_groups.c). */
int season_intl_ofc_honours(uint32_t self);

/* 1 while the Oceania section must not advance from its final to the play-off: the cycle's tie has a CONCACAF
 * partner whose section is not complete (O259 A16; season_intl_qual.c, called by the wrap of 0x91f0d0). */
int season_intl_oceania_wait(uint32_t self);

/* After the original's Oceania template 0x91e2e0 (the wrap in season_wcq26.c): stage 2, the other tie of the
 * cycles 2006 to 2018, takes its real dates (season_intl_qual.c). rec is the round record that the template gave. */
void season_intl_oceania_tie_dates(uint32_t self, int stage, uint32_t rec);

#endif
