/* Rule kind "club_world" of the season change (0.6.0 slice 26, part a; season_change.h): the club competitions
 * of the original outside UEFA play an edition only in the seasons that the rules table holds them
 * (docs/research/rules-0.6.0/club-world.tsv). Examples: the Intercontinental Cup to 2004/05 only, the FIFA Club
 * World Championship every year from 2005/06 and not after 2024/25, the Copa Mercosur and the Merconorte Cup
 * in 2001/02 only, the Inter-American Cup never, the Oceania Champions Cup from 2004/05 except 2020/21.
 *
 * Evidence: docs/research/2026-10-18-club-world-idle.md. The method is the state that the original itself uses
 * between two editions of the Club World Championship: the object stays (other code reads it with no null
 * test: the Libertadores writes its winner into the Intercontinental Cup at 0x4c41a0..0x4c41d0, the FA Cup
 * walks the entry 2 array at 0x56fb08), and its season year +0x40 is the year of its next edition. The dates
 * of that edition lie in the future, so the object makes no fixtures (no constructor makes fixtures; a League
 * class draws on its start date, a Cup class on its draw date) and is not complete, so the season pass does
 * not start it again (all_stages_complete 0x48ce10, the pass at 0x5d5ccb). An edition of year Y plays in game
 * season Y for every class here (doc section 6).
 *
 * The two paths (build brief, "Common to slices 15b to 29"):
 *   - a new game of a season game (start year not 2001, or changing rules on): the wraps of the constructors
 *     below give the constructor the year of the first edition from the start season on (thiscall, ret 8:
 *     the year, then the record). The constructor builds every date from that year (doc section 1, point 2).
 *   - with changing rules on, at each season change: the before hook of start_new_season sets +0x40 to the
 *     year of the next edition minus the class's year step, so start_new_season makes that edition.
 * The base game (2001/02, changing rules off) and RECOMP_SEASON_CODE=off get none of it (Q342).
 *
 * A competition that the table never holds again waits for the year NEVER. Its menus still list it (the menus
 * walk the entry arrays, 0x49d156); what its screens show of that year is unknown (doc section 3.2).
 *
 * The year steps (doc section 5): the Club World Championship (+2 at 0x929d9e) and the Oceania Champions Cup
 * (+2 at 0x797dbe) step by 1 in a season game (tools/recomp/seasons.py, rt_club_world_step), and the Club
 * World Championship then draws on 1 July, after the update day of 20 June (rt_club_world_month and
 * rt_club_world_day at 0x929c39 and 0x929c3b): with the original's 5 February the draw date of the next
 * edition is past when start_new_season runs, and the class then never draws again (0x92a73d..0x92a74f). The
 * first edition loop of its constructor (0x92919c..0x9291bd) keeps an even year in a season game
 * (rt_club_world_annual, 0x9291b0). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_rules.h"

#include <stdio.h>
#include <string.h>

#define YEAR_AT 0x40u                   /* s16 season year of a competition object */
#define NAME_AT 4u                      /* the long name of the competition record (object +4) */
#define NEVER 2999                      /* the year of a competition that the table never holds again */

typedef struct {
    const char *file;                   /* the source file of the class (season_comp_classes) */
    int late;                           /* 1: the pass call does nothing, and the class starts itself later */
} ClubWorldClass;

/* The classes with a season that the table does not hold. con_champ and conmebol_liber play every season.
 * Asian Super Cup: AsiaSuperCup::start_new_season 0x410c50 works only in June (0x410c64), so the call of the
 * pass on 28 August does nothing; the class starts its next edition itself on 25 June (update_for_date
 * 0x410e20, 0x410e34..0x410e72, outside the pass). Its before hook of season S therefore sets the edition of
 * S + 1. */
static const ClubWorldClass classes[] = {
    {"world_club_champ", 0},     /* FIFA Club World Championship, entry 2 */
    {"world_club_cup", 0},       /* Intercontinental Cup, entry 2 */
    {"inter_amer_cup", 0},       /* Inter-American Cup, entry 2 */
    {"con_merc_cup", 0},         /* Merconorte Cup, entry 4 */
    {"conmebol_merc", 0},        /* Copa Mercosur, entry 5 */
    {"asia_club_champ", 0},      /* Asian Club Championship, entry 6 (none in 2022/23) */
    {"asia_cup_winner", 0},      /* Asian Cup Winners' Cup, entry 6 */
    {"asia_super_cup", 1},       /* Asian Super Cup, entry 6 */
    {"oceania_club_champ", 0},   /* Oceania Champions Cup, entry 7 */
};
#define CLASS_COUNT ((int)(sizeof classes / sizeof classes[0]))

static const ClubWorldClass *class_of(const char *file)
{
    for (int i = 0; file && i < CLASS_COUNT; i++)
        if (!strcmp(classes[i].file, file)) return &classes[i];
    return NULL;
}

/* 1 when a game of the season code runs or is made: a start year that is not 2001, or changing rules on. */
static int season_game(void)
{
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

/* The row of a class in season s. After the last row of the table the rule stays (Q248): the last row. */
static const SeasonRule *row_of(const char *file, int s)
{
    const SeasonRule *r = NULL;
    for (int y = s; y >= SEASON_RULE_FIRST && !r; y--) r = season_rules_find(y, file, 0);
    return r;
}

/* -1: no row (the kind changes nothing), 1: the table holds the competition in season s, 0: not held. */
static int held(const char *file, int s)
{
    const SeasonRule *r = row_of(file, s);
    if (!r) return -1;
    return r->clubs != SEASON_RULE_NOT_HELD && strcmp(r->format, "not_held") != 0;
}

/* The last season with a row of the file. The table is read once at the start (season_rules_init). */
static int last_row_season(const char *file)
{
    int last = 0;
    for (int y = SEASON_RULE_FIRST; y <= SEASON_RULE_LAST; y++)
        if (season_rules_find(y, file, 0)) last = y;
    return last;
}

/* The year of the next edition from season s on: s when the table holds it, else the next season that holds
 * it, else NEVER. 0 when the table has no row of the class (nothing changes). */
static int next_edition(const char *file, int s)
{
    int h = held(file, s);
    if (h < 0) return 0;
    if (h) return s;
    int last = last_row_season(file);
    for (int y = s + 1; y <= last; y++)
        if (held(file, y) == 1) return y;
    return NEVER;
}

/* The year of the first edition of a new game that starts in season `start`. With changing rules on: the next
 * edition from the start season on. With changing rules off the rules of the start season stay (Q232, lead
 * 2026-10-18): a competition that the start season holds plays, one that it does not hold never comes, even when
 * a later season of the table holds it. 0 when the table has no row. */
static int start_edition(const char *file, int start)
{
    if (season_changing_rules()) return next_edition(file, start);
    int h = held(file, start);
    return h < 0 ? 0 : h ? start : NEVER;
}

/* ---- The year steps of tools/recomp/seasons.py (rt.h) ---- */

/* Each site changes only when the table has rows of the class: with no table (or no row) the kind changes
 * nothing (Q248), so these give the original's value too. Else a season game with no table got the 1 July date
 * with the original's year (same-save run rules-on, 2026-10-20). */
static int cwc_rows(void) { return season_game() && last_row_season("world_club_champ") > 0; }
static int oceania_rows(void) { return season_game() && last_row_season("oceania_club_champ") > 0; }

uint32_t rt_club_world_annual(void) { return (uint32_t)cwc_rows(); }
uint32_t rt_club_world_step(uint32_t original) { return cwc_rows() ? 1u : original; }
uint32_t rt_club_world_month(uint32_t original) { return cwc_rows() ? 6u : original; }   /* 0-based: July */
uint32_t rt_club_world_day(uint32_t original) { return cwc_rows() ? 1u : original; }
uint32_t rt_oceania_step(uint32_t original) { return oceania_rows() ? 1u : original; }

/* The class's step after the patches above. */
static int step_of(const char *file)
{
    if (!strcmp(file, "world_club_champ")) return (int)rt_club_world_step(2u);
    if (!strcmp(file, "oceania_club_champ")) return (int)rt_oceania_step(2u);
    return 1;
}

/* ---- New game: the constructors ---- */

/* The year argument of a constructor at a new game of a season game: the first edition from the start season
 * on. At a load the creators run too, and the object takes its year from its saved state (League::load
 * 0x687b10 reads +0x40, doc section 2; Cup::save_cup_to_disk 0x5210f0 writes +0x40 at 0x521315, and
 * Cup::load_from_disk 0x51fc00 reads the same fields: inference from the symmetry, not read field by field),
 * so the wraps act only while a new game is made. */
static void ctor(const char *file, void (*original)(void))
{
    /* A new competition of the module on an original class (season_club_cups.c) takes its own year. */
    if (season_game() && season_change_is_creating() && !season_comps_is_def_record(rd32(g_mem, cpu.esp + 8))) {
        int start = season_start_year();
        int year = start_edition(file, start);
        uint32_t slot = cpu.esp + 4;                        /* thiscall (year, record), ret 8 */
        uint32_t arg = rd32(g_mem, slot);
        if (year && year != (int16_t)(arg & 0xffffu)) {
            wr32(g_mem, slot, (arg & 0xffff0000u) | ((uint32_t)year & 0xffffu));
            fprintf(stderr, "recomp: %s waits for %d (start season %d)\n", file, year, start);
        }
    }
    original();
}

void o_00929140(void);
void o_0092b4b0(void);
void o_00632080(void);
void o_004c16f0(void);
void o_004c46c0(void);
void o_0040aae0(void);
void o_0040cbe0(void);
void o_00410650(void);
void o_00796fd0(void);

RT_WRAP(00929140, world_club_champ_ctor) { ctor("world_club_champ", o_00929140); }
RT_WRAP(0092b4b0, world_club_cup_ctor) { ctor("world_club_cup", o_0092b4b0); }
RT_WRAP(00632080, inter_amer_cup_ctor) { ctor("inter_amer_cup", o_00632080); }
RT_WRAP(004c16f0, con_merc_cup_ctor) { ctor("con_merc_cup", o_004c16f0); }
RT_WRAP(004c46c0, conmebol_merc_ctor) { ctor("conmebol_merc", o_004c46c0); }
RT_WRAP(0040aae0, asia_club_champ_ctor) { ctor("asia_club_champ", o_0040aae0); }
RT_WRAP(0040cbe0, asia_cup_winner_ctor) { ctor("asia_cup_winner", o_0040cbe0); }
RT_WRAP(00410650, asia_super_cup_ctor) { ctor("asia_super_cup", o_00410650); }
RT_WRAP(00796fd0, oceania_club_champ_ctor) { ctor("oceania_club_champ", o_00796fd0); }

/* ---- The Copa Mercosur's own start ---- */

/* ConmebolMerc::update_for_date 0x4c5c90 calls start_new_season (0x4c5ce4) on 1 June when +0x40 is not the
 * game year (compare 0x4c5cdc, jump 0x4c5cde). A waiting object has a later year, so each 1 June would add 1
 * again. tools/recomp/seasons.py makes the jump at 0x4c5cde ask this function too: 1 skips the call. */
uint32_t rt_club_world_waits(uint32_t obj)
{
    if (!season_game()) return 0;
    int year = (int16_t)rd16(g_mem, obj + YEAR_AT);
    int now = (int16_t)rd16(g_mem, 0xae2c92u);              /* the game year */
    return year > now;
}

/* ---- The season change ---- */

static const char *name_of(uint32_t comp, char *buf, size_t n)
{
    uint32_t rec = rd32(g_mem, comp + NAME_AT);
    snprintf(buf, n, "%.50s", rt_readable(rec) ? win_str(rec + NAME_AT) : "");
    return buf;
}

static void club_world_before(const SeasonChange *c)
{
    if (!season_game()) return;                             /* never in the base game (Q152, Q342) */
    const ClubWorldClass *k = class_of(c->file);
    if (!k || !c->started) return;
    /* The start season hooks run with changing rules off too (season_change.h, "The start season"). A late
     * class sets the edition of the next season there, which is a change only with changing rules on. */
    if (k->late && c->start && !season_changing_rules()) return;
    int s = c->season + k->late;                            /* the season of the edition that starts next */
    /* With changing rules off the start season's rule stays (Q232), whatever the date of the hook. */
    int year = season_changing_rules() ? next_edition(k->file, s) : start_edition(k->file, season_start_year());
    if (!year) return;
    int want = year - step_of(k->file);
    int now = (int16_t)rd16(g_mem, c->comp + YEAR_AT);
    if (want == now) return;
    wr16(g_mem, c->comp + YEAR_AT, (uint16_t)want);
    fprintf(stderr, "recomp: %s in season %d waits for %d (+0x40 %d to %d)\n", k->file, c->season, year, now, want);
}

/* One sentence when the table starts or stops holding the competition in the season of the change (Q258). It
 * runs for a competition that starts and for one that waits (started 0), so a competition that comes back
 * after a gap gets its sentence too. */
static void club_world_after(const SeasonChange *c)
{
    if (!season_game()) return;                             /* never in the base game (Q152, Q342) */
    const ClubWorldClass *k = class_of(c->file);
    if (!k) return;
    int s = c->season;
    int now = held(k->file, s), before = held(k->file, s - 1);
    if (now < 0 || before < 0 || now == before) return;
    char name[64];
    name_of(c->comp, name, sizeof name);
    if (!*name) return;
    if (now)
        season_change_news("The %s is played again from the %d/%02d season.", name, s, (s + 1) % 100);
    else if (next_edition(k->file, s) == NEVER)
        season_change_news("The %s is no longer played.", name);
    else
        season_change_news("The %s is not played in the %d/%02d season.", name, s, (s + 1) % 100);
}

const SeasonRuleKind season_kind_club_world = {"club_world", NULL, NULL, NULL, NULL, club_world_before,
                                               club_world_after};

/* ---- Part b: the knockout of the FIFA Club World Championship (Form K) ----
 *
 * Evidence: docs/research/2026-10-18-club-world-formats.md. The original plays 3 groups of 4 and a knockout of 4
 * (12 clubs). The table gives a knockout of 6 clubs (2005/06, 2006/07, 2020/21) or 7 clubs (2007/08 to 2023/24,
 * not 2020/21): a play-off of the host champion against the OFC winner, quarter-finals, the UEFA and CONMEBOL
 * winners in the semi-finals, the final and the third place match. In a season game the start date branch of
 * WorldClubChamp::update_for_date 0x92a6b0 makes, in place of the groups, one Cup_Stage (0x522e00) in stage
 * slot 0 with stage index 0: League::add_result gives a result to the stage slot of the fixture's stage index
 * (0x685dd0..0x685dfa), and the parent updates the stages from slot 0 to +0x30 with no null test (0x6848e5).
 * The three slots that test the stage index for the knockout (2) get 2 for this stage: staff records 0x929c70,
 * fate news 0x9298a0, progress codes 0x92ad50. Clubs that enter in a later round are byes of the round table
 * (+0x1c new entrants from club list index +0x1e, map_fixture_tree 0x519265..0x519504). The third place match is the last round
 * (round id 140: Cup::process_fixture puts the semi-final losers there, 0x51b129..0x51b158). No fifth place
 * match: no class of the original has one (doc 3.4).
 *
 * The parent League holds no club: its club count +0x3e is 0 and its table +0xb1 one zeroed row, because the
 * parent ends its season only with a table (0x68495e, League::check_complete 0x689add) and League::update_for_date
 * reads row 0 (0x68484e). The history thunk 0x40f590 forwards to stage slot 2; for this form it forwards to
 * slot 0. The matches keep the original's weeks (late July and August), so the fixture checks are as before. */

#define CWC_VTABLE 0x97119cu
#define CWC_STAGES 0xcu                 /* stage pointer array (3 slots) */
#define CWC_STAGE_COUNT 0x2cu           /* s32 */
#define CWC_STAGE_NOW 0x30u             /* s32, -1 at a new edition */
#define CWC_CLUBS 0x3eu                 /* s16 */
#define CWC_DRAWN 0x45u                 /* s32 */
#define CWC_TABLE 0xb1u                 /* the parent's own table, rows of 0x3b bytes */
#define CWC_TABLE_B5 0xb5u              /* 0 for check_complete (0x689ae9) */
#define CWC_START_DAY 0xb6u             /* s16 day of the year */
#define CWC_START_OFFSET 0xb8u          /* s16 years after +0x40 */
#define CWC_PICKS 0xeeu                 /* Club *[12] of pick_entrants 0x92a8b0 */
#define CWC_PICK_COUNT 0xf2u
#define ROUND_SIZE 0x68u
#define TABLE_ROW 0x3bu

/* The places of pick_entrants 0x92a8b0 (calls 0x92a9f2..0x92ab7c): Africa 0, 1; Asia 2, 3; Europe 4 (the
 * Champions League), 5 (the UEFA Cup); North America 6, 7; Oceania 8; South America 9, 10; host 11. */
enum { PICK_CAF = 0, PICK_AFC = 2, PICK_UEFA = 4, PICK_CONCACAF = 6, PICK_OFC = 8, PICK_CONMEBOL = 9, PICK_HOST = 11 };

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* The club count of the knockout in the edition of year `year`: 6 or 7, or 0 for the original form. With
 * changing rules off the form of the start season stays (Q232). */
static int knockout_clubs(int year, int *host_in_place_of_ofc)
{
    *host_in_place_of_ofc = 0;
    if (!season_game()) return 0;
    int s = season_changing_rules() ? year : season_start_year();     /* the form of the start season, Q232 */
    const SeasonRule *r = s > 0 ? row_of("world_club_champ", s) : NULL;
    if (!r || (r->clubs != 6 && r->clubs != 7)) return 0;
    /* 2020/21: the OFC winner withdrew, so the host champion plays in the quarter-finals (club-world.tsv,
     * row club-world-363). */
    if (r->clubs == 6 && r->season == 2020) *host_in_place_of_ofc = 1;
    return r->clubs;
}

/* ---- Part 26c: the real dates of the knockout (Q401) ----
 *
 * The table's format text gives the days of each edition, for example "7–16 December 2007" or "4–11 February
 * 2021" (club-world.tsv). The year of the text minus the row's season is the year offset: 0 for December, 1 for
 * February. With changing rules off the row of the start season stays (Q232), so its days and offset move with
 * the edition's year. A day of the year counts from 0 (1 January), with the month starts of Date.cpp (0x9b8448,
 * 0x9b8460) and its leap test (src/si/date.c). */

/* The days of an edition: the first match and the final as days of the year of `year`, and `year` - +0x40. */
typedef struct { int first, last, year, offset; } KnockoutDates;

static const char *const month_names[12] = {"January", "February", "March", "April", "May", "June", "July",
                                            "August", "September", "October", "November", "December"};

static int is_leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

static int day_of_year(int day, int month, int year)
{
    static const short start[12] = {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333};
    return start[month - 1] + day + (month > 2 && is_leap(year));
}

/* "d.m.yyyy" of a day of the year, for the test log. */
static const char *date_text(int doy, int year, char *buf, size_t n)
{
    int m = 1;
    while (m < 12 && doy >= day_of_year(1, m + 1, year)) m++;
    snprintf(buf, n, "%d.%d.%d", doy - day_of_year(1, m, year) + 1, m, year);
    return buf;
}

/* Reads a number at *p (1 to 31). */
static int read_day(const char **p)
{
    int v = 0, n = 0;
    while (**p >= '0' && **p <= '9' && n < 3) v = v * 10 + (*(*p)++ - '0'), n++;
    return n && n < 3 && v >= 1 && v <= 31 ? v : 0;
}

/* Reads " <Month>" at *p: 1 to 12, or 0. */
static int read_month(const char **p)
{
    if (**p != ' ') return 0;
    for (int m = 0; m < 12; m++) {
        size_t len = strlen(month_names[m]);
        if (!strncmp(*p + 1, month_names[m], len)) { *p += 1 + len; return m + 1; }
    }
    return 0;
}

/* Reads spaces, a hyphen or an en dash (UTF-8), spaces. */
static int read_dash(const char **p)
{
    while (**p == ' ') (*p)++;
    if (**p == '-') (*p)++;
    else if ((unsigned char)(*p)[0] == 0xe2 && (unsigned char)(*p)[1] == 0x80 && (unsigned char)(*p)[2] == 0x93) *p += 3;
    else return 0;
    while (**p == ' ') (*p)++;
    return 1;
}

/* The days of the edition of year `edition` in the format text of its row: "7–16 December 2007" or "14 June –
 * 13 July 2025". 0 when the text holds no such days: the knockout then keeps the original's August days. */
static int edition_dates(int edition, KnockoutDates *d)
{
    const SeasonRule *r = row_of("world_club_champ", season_changing_rules() ? edition : season_start_year());
    const char *t = r ? r->format_text : NULL;
    for (const char *s = t; s && *s; s++) {
        if (*s < '0' || *s > '9' || (s > t && s[-1] >= '0' && s[-1] <= '9')) continue;
        const char *p = s;
        int d1 = read_day(&p), m1 = read_month(&p);
        if (!d1 || !read_dash(&p)) continue;
        int d2 = read_day(&p), m2 = read_month(&p), year;
        if (!d2 || !m2 || *p != ' ' || sscanf(p, " %4d", &year) != 1 || year < 2000) continue;
        if (!m1) m1 = m2;
        d->offset = year - r->season;
        d->year = edition + d->offset;
        d->first = day_of_year(d1, m1, d->year);
        d->last = day_of_year(d2, m2, d->year);
        if (d->first >= d->last) continue;
        return 1;
    }
    return 0;
}

/* The start date (the picks and the draw): one week before the first match (formats doc 5.4). The original's
 * start branch tests the same fields (0x92a73d..0x92a765), so it cannot draw the groups on 1 July first. */
static void set_start_date(uint32_t obj, int edition)
{
    KnockoutDates kd;
    if (!edition_dates(edition, &kd)) return;
    int y = kd.year, doy = kd.first - 7;
    if (doy < 0) { y--; doy += 365 + is_leap(y); }
    wr16(g_mem, obj + CWC_START_DAY, (uint16_t)doy);
    wr16(g_mem, obj + CWC_START_OFFSET, (uint16_t)(y - edition));
}

/* The parent: no club, one zeroed table row (see above). Returns 0 when it cannot. */
static int parent_table(uint32_t obj)
{
    uint32_t sz[1] = {TABLE_ROW};
    uint32_t row = call_cdecl(0x944cf1u, 1, sz);
    if (rt_unwinding || !row) return 0;
    memset(g_mem + row, 0, TABLE_ROW);
    wr16(g_mem, obj + CWC_CLUBS, 0);
    wr32(g_mem, obj + CWC_TABLE, row);
    g_mem[obj + CWC_TABLE_B5] = 0;
    return 1;
}

/* Builds the knockout on the start date. Returns 1 when the object now holds Form K. */
static int build_knockout(uint32_t obj, int clubs, int host_for_ofc)
{
    call_this(0x92a8b0u, obj, 0, NULL);                         /* pick_entrants: 12 clubs into +0xee */
    if (rt_unwinding) return 0;
    uint32_t picks = rd32(g_mem, obj + CWC_PICKS);
    if (!picks || (int32_t)rd32(g_mem, obj + CWC_PICK_COUNT) < 12) return 0;
    /* The club list in round order: the clubs of round 0, then each later round's new entrants. */
    int order7[7] = {PICK_HOST, PICK_OFC, PICK_AFC, PICK_CAF, PICK_CONCACAF, PICK_UEFA, PICK_CONMEBOL};
    int order6[6] = {PICK_AFC, PICK_CAF, PICK_CONCACAF, PICK_OFC, PICK_UEFA, PICK_CONMEBOL};
    int *order = clubs == 7 ? order7 : order6;
    if (clubs == 6 && host_for_ofc) order6[3] = PICK_HOST;
    /* The AFC place. pick_entrant 0x92ae60 takes for Asia the record at entry +0x1e of the Asian entry, else the
     * Asian Super Cup (0x92aebf..0x92aec7), which waits from 2003/04 (part a), so each edition would get its last
     * winner. So the AFC club is picked again with the record override (argument 5, 0x92aef7..0x92af0e) of the
     * Asian Club Championship (key id [0x9cf64c]): place 0 of its latest history row (0x4afd80), or a club of
     * the top reputations of Asia (0x92afb1..0x92b093). The old pick leaves the list first, so it is no bar.
     * pick_entrant is thiscall (continent, nation, Club **picked, short count, record), ret 0x14 (0x92a9e3..0x92a9f2
     * pass [0x9cfa08] for Africa; Asia is [0x9cfa0c], 0x92aa37). */
    int32_t acl = (int32_t)rd32(g_mem, 0x9cf64cu);
    if (acl >= 0) {
        wr32(g_mem, picks + 4u * PICK_AFC, 0);
        uint32_t pa[5] = {rd32(g_mem, 0x9cfa0cu), 0xffffffffu, picks, 12, (uint32_t)acl};
        uint32_t club = call_this(0x92ae60u, obj, 5, pa);
        if (rt_unwinding) return 0;
        wr32(g_mem, picks + 4u * PICK_AFC, club);
    }
    static uint32_t list;
    if (!list) list = win_static_alloc(4u * 8u);
    for (int i = 0; i < clubs; i++) {
        uint32_t club = rd32(g_mem, picks + 4u * (uint32_t)order[i]);
        if (!club) {                                            /* the original form then plays */
            uint32_t f[1] = {picks};
            call_cdecl(0x9452cau, 1, f);                        /* its start branch picks again (0x92a8bb) */
            wr32(g_mem, obj + CWC_PICKS, 0);
            wr32(g_mem, obj + CWC_PICK_COUNT, 0);
            return 0;
        }
        wr32(g_mem, list + 4u * (uint32_t)i, club);
    }
    uint32_t b[1] = {picks};
    call_cdecl(0x9452cau, 1, b);                                /* as the original frees it, 0x92a869 */
    wr32(g_mem, obj + CWC_PICKS, 0);
    wr32(g_mem, obj + CWC_PICK_COUNT, 0);

    /* The rounds, from the original's knockout table (create_schedule 0x929390, stage 2: semi-final, final,
     * third place, 0x929705..0x929884) as templates. Without dates in the table the days move from the
     * semi-final (draw 7 August, match 9 August): the play-off 8 days earlier, the quarter-finals 4 days earlier.
     * With the table's dates (part 26c): the first round on the first day, each later round 3 days after the one
     * before, the final and the third place match on the last day. A round's draw is 2 days before its first
     * round, else the day after the round before (as the original's final, 0x92975f). */
    int edition = (int16_t)rd16(g_mem, obj + YEAR_AT);
    KnockoutDates kd;
    int dated = edition_dates(edition, &kd);
    static uint32_t out;                                        /* rounds, code, is_league */
    if (!out) out = win_static_alloc(12);
    uint32_t a[4] = {2, out, out + 4, out + 8};
    uint32_t orig = call_this(0x929390u, obj, 4, a);
    if (rt_unwinding || !orig || (int16_t)rd16(g_mem, out) != 3) return 0;
    static uint32_t table;
    if (!table) table = win_static_alloc(5u * ROUND_SIZE);
    int n = 0;
    struct { int tmpl, id, teams, ties, fresh, first, days; } rounds[5];
    if (clubs == 7) {
        rounds[n++] = (typeof(rounds[0])){0, 0x14, 2, 1, 2, 0, -8};    /* play-off */
        rounds[n++] = (typeof(rounds[0])){0, 0x78, 4, 2, 3, 2, -4};    /* quarter-finals */
        rounds[n++] = (typeof(rounds[0])){0, 0x82, 4, 2, 2, 5, 0};     /* semi-finals */
    } else {
        rounds[n++] = (typeof(rounds[0])){0, 0x78, 4, 2, 4, 0, -4};
        rounds[n++] = (typeof(rounds[0])){0, 0x82, 4, 2, 2, 4, 0};
    }
    rounds[n++] = (typeof(rounds[0])){1, 0x96, 2, 1, 0, 0, 0};        /* final */
    rounds[n++] = (typeof(rounds[0])){2, 0x8c, 2, 1, 0, 0, 0};        /* third place */
    int play_year = dated ? kd.year : edition, first_day = dated ? kd.first : 0, last_day = dated ? kd.last : 0;
    for (int i = 0; i < n; i++) {
        uint32_t r = table + (uint32_t)i * ROUND_SIZE;
        memcpy(g_mem + r, g_mem + orig + (uint32_t)rounds[i].tmpl * ROUND_SIZE, ROUND_SIZE);
        if (dated) {
            int knockout = rounds[i].tmpl == 0;                  /* the rounds before the final */
            int match = knockout ? first_day + 3 * i : last_day;
            int draw = i == 0 ? match - 2 : (knockout ? first_day + 3 * (i - 1) : first_day + 3 * (n - 3)) + 1;
            wr16(g_mem, r + 0x00, (uint32_t)draw);               /* draw day of the year */
            wr16(g_mem, r + 0x03, (uint32_t)match);              /* match day of the year */
            g_mem[r + 0x02] = (uint8_t)kd.offset;                /* year offsets from +0x40 */
            g_mem[r + 0x05] = (uint8_t)kd.offset;
        } else {
            wr16(g_mem, r + 0x00, rd16(g_mem, r + 0x00) + (uint32_t)rounds[i].days);
            wr16(g_mem, r + 0x03, rd16(g_mem, r + 0x03) + (uint32_t)rounds[i].days);
        }
        wr16(g_mem, r + 0x07, (uint32_t)rounds[i].id);
        wr16(g_mem, r + 0x09, (uint32_t)i);
        /* Match flags: 0x2 extra time, 0x1 penalties; no 0x80 golden goal (is_golden_goal_active 0x6d4269 reads it,
         * cup-round-record doc section 2). */
        g_mem[r + 0x0d] = 0x03;
        wr16(g_mem, r + 0x18, (uint32_t)rounds[i].teams);
        wr16(g_mem, r + 0x1a, (uint32_t)rounds[i].ties);
        wr16(g_mem, r + 0x1c, (uint32_t)rounds[i].fresh);
        wr16(g_mem, r + 0x1e, (uint32_t)rounds[i].first);
    }
    uint32_t code = rd32(g_mem, out + 4);
    uint32_t f[1] = {orig};
    call_cdecl(0x9452cau, 1, f);                                /* as the original frees it, 0x92a38b */
    if (rt_unwinding) return 0;

    if (!parent_table(obj)) return 0;

    /* The stage, as 0x92a326..0x92a370 makes the original's knockout. */
    uint32_t sz2[1] = {0xb2};
    uint32_t stage = call_cdecl(0x944cf1u, 1, sz2);
    if (rt_unwinding || !stage) return 0;
    uint32_t args[15] = {obj, (uint32_t)clubs, list, (uint32_t)n, rd32(g_mem, obj + 4), table,
                         (uint32_t)(int16_t)rd16(g_mem, obj + YEAR_AT), 0, 2, code, 0x14, 0, 0, 0, 0};
    call_this(0x522e00u, stage, 15, args);
    if (rt_unwinding) return 0;
    wr32(g_mem, rd32(g_mem, obj + CWC_STAGES), stage);          /* slot 0 */
    wr32(g_mem, obj + CWC_STAGE_COUNT, 1);
    uint32_t r0[1] = {0};
    call_this(0x51c800u, stage, 1, r0);                         /* set_up_round(0), as 0x92a414 */
    if (rt_unwinding) return 0;
    wr32(g_mem, obj + CWC_STAGE_NOW, 0);
    wr32(g_mem, obj + CWC_DRAWN, 1);                            /* the original's start branch now skips */
    fprintf(stderr, "recomp: club world knockout of %d clubs for %d\n", clubs, edition);
    if (dated) {
        char a[16], b[16];
        fprintf(stderr, "recomp: club world knockout %d: first match %s, final %s\n", edition,
                date_text(first_day, play_year, a, sizeof a), date_text(last_day, play_year, b, sizeof b));
    }
    return 1;
}

/* Back to the original form (3 stage slots). After a load of a Form K game the stage array has 1 slot
 * (League::load 0x687b10 makes it with the saved +0x2c, 0x688e3a..0x688e5b, and the constructor skips its own
 * array after a load, 0x9291d1), so the object gets a new array of 3 slots from the operator new of the
 * constructor (0x944e46, as 0x9291e9) and the old one goes back with free (as the destructor, 0x929349). */
static void original_form(uint32_t obj)
{
    uint32_t sz[1] = {12};
    uint32_t arr = call_cdecl(0x944e46u, 1, sz);
    if (rt_unwinding || !arr) return;
    memset(g_mem + arr, 0, 12);
    uint32_t old[1] = {rd32(g_mem, obj + CWC_STAGES)};
    wr32(g_mem, obj + CWC_STAGES, arr);
    wr32(g_mem, obj + CWC_STAGE_COUNT, 3);
    if (old[0]) call_cdecl(0x9452cau, 1, old);
}

/* ---- Part 26c: the 32-club Club World Cup of 2024/25 (Form G) ----
 *
 * Evidence: docs/research/2026-10-18-club-world-formats.md section 4. The parent is group A, seven League_Stage
 * groups sit at stage slots 0 to 6 and the knockout Cup_Stage at slot 7, as in the Copa Libertadores: the
 * runtime calls its builders on the World_Club_Champ object, group A 0x4c37a0, groups B to H 0x4c3b10 and
 * advance_stage 0x4c3a80 (at stage 7 the knockout 0x4c3d10 of the top two of each group, 16 clubs). They read
 * only the League base and the object's vtable +0x3c, create_schedule 0x929390, which the wrap below answers
 * for this form: the groups one round-robin of 3 rounds, the knockout 4 one-match rounds with extra time and
 * penalties and no third place match. The picks: the original's 12 (pick_entrants 0x92a8b0) and 20 more from
 * pick_entrant 0x92ae60 (a second call for a continent gives the runner-up, later calls clubs of the top
 * reputations, formats doc 5.2), so UEFA 12, CONMEBOL 6, AFC 4, CAF 4, CONCACAF 4, OFC 1 and the host 1. The
 * draw: four pots of 8 in that order, each shuffled with the game's random function 0x90cfc0, at most one
 * club of a confederation in a group but two of UEFA. The edition plays 14 June to 13 July of +0x40 + 1. */

#define CWC_ADVANCE_TO 0xbeu             /* byte: clubs that go on from each group (0x929c03 writes 1) */
#define GROUP_STAGES 8

static uint32_t game_new(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(0x944e46u, 1, a);
    if (!rt_unwinding && p) memset(g_mem + p, 0, size);
    return rt_unwinding ? 0 : p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (p) call_cdecl(0x9452cau, 1, a);
}

/* 1 when the edition of `year` has 32 clubs in the table (with changing rules off the start season's row). */
static int groups_form(int year)
{
    if (!season_game()) return 0;
    const SeasonRule *r = row_of("world_club_champ", season_changing_rules() ? year : season_start_year());
    return r && r->clubs == 32;
}

static int is_groups(uint32_t obj)
{
    return rd32(g_mem, obj) == CWC_VTABLE && rd32(g_mem, obj + CWC_STAGE_COUNT) == GROUP_STAGES;
}

/* A new stage array of `slots` empty slots in place of the old one (formats doc risk 3). */
static int stage_array(uint32_t obj, int slots)
{
    uint32_t arr = game_new(4u * (uint32_t)slots);
    if (!arr) return 0;
    uint32_t old = rd32(g_mem, obj + CWC_STAGES);
    wr32(g_mem, obj + CWC_STAGES, arr);
    wr32(g_mem, obj + CWC_STAGE_COUNT, (uint32_t)slots);
    game_free(old);
    return !rt_unwinding;
}

static int game_random(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    int r = (int)call_cdecl(0x90cfc0u, 1, a);
    if (r < 0) r = -r;
    return n > 0 ? r % n : 0;
}

/* Confederations of the picks below, for the draw rule. */
enum { CONF_UEFA, CONF_CONMEBOL, CONF_AFC, CONF_CAF, CONF_CONCACAF, CONF_OFC, CONF_HOST };

/* The 32 entrants in group order (A1..A4, B1..B4, ...) into +0xee. Returns 0 when a pick gives no club. */
static int pick_groups(uint32_t obj)
{
    call_this(0x92a8b0u, obj, 0, NULL);                         /* pick_entrants: 12 clubs into +0xee */
    if (rt_unwinding) return 0;
    uint32_t picks = rd32(g_mem, obj + CWC_PICKS);
    if (!picks || (int32_t)rd32(g_mem, obj + CWC_PICK_COUNT) < 12) return 0;
    uint32_t all = game_new(4u * 32u);
    if (!all) return 0;
    memcpy(g_mem + all, g_mem + picks, 4u * 12u);
    game_free(picks);
    wr32(g_mem, obj + CWC_PICKS, 0);
    wr32(g_mem, obj + CWC_PICK_COUNT, 0);
    /* The AFC winner, as in Form K (build_knockout): the Asian Club Championship record in place of the Asian
     * Super Cup. */
    int32_t acl = (int32_t)rd32(g_mem, 0x9cf64cu);
    if (acl >= 0) {
        wr32(g_mem, all + 4u * PICK_AFC, 0);
        uint32_t pa[5] = {rd32(g_mem, 0x9cfa0cu), 0xffffffffu, all, 12, (uint32_t)acl};
        uint32_t club = call_this(0x92ae60u, obj, 5, pa);
        if (rt_unwinding) { game_free(all); return 0; }
        wr32(g_mem, all + 4u * PICK_AFC, club);
    }
    /* 20 more: indexes 12 to 21 UEFA, 22 to 25 CONMEBOL, 26 and 27 AFC, 28 and 29 CAF, 30 and 31 CONCACAF, the
     * clubs of the highest reputation of the continent not yet picked (season_comps_top_club: pick_entrant gives
     * at most about 6 clubs of a continent, then its error box 0x92b0fd). The continent globals are those of
     * pick_entrants (0x92a9e3..0x92ab5a). */
    static const struct { uint32_t continent; int count; } more[] = {
        {0x9cfa10u, 10}, {0x9cfa1cu, 4}, {0x9cfa0cu, 2}, {0x9cfa08u, 2}, {0x9cfa14u, 2}};
    uint32_t got[32];
    for (int i = 0; i < 12; i++) got[i] = rd32(g_mem, all + 4u * (uint32_t)i);
    int n = 12;
    for (int k = 0; k < 5; k++)
        for (int i = 0; i < more[k].count; i++, n++) {
            got[n] = season_comps_top_club(more[k].continent, got, n);
            wr32(g_mem, all + 4u * (uint32_t)n, got[n]);
        }
    for (int i = 0; i < 32; i++)
        if (!rd32(g_mem, all + 4u * (uint32_t)i)) { game_free(all); return 0; }
    /* The pots by index of `all` (the places of pick_entrants: Africa 0, 1; Asia 2, 3; Europe 4, 5; North America
     * 6, 7; Oceania 8; South America 9, 10; host 11). */
    int pot[4][8] = {{4, 5, 12, 13, 9, 10, 22, 23},
                     {14, 15, 16, 17, 18, 19, 20, 21},
                     {2, 3, 26, 27, 6, 7, 30, 31},
                     {0, 1, 28, 29, 24, 25, 8, 11}};
    int conf[32];
    for (int i = 0; i < 32; i++) conf[i] = CONF_UEFA;
    int sa[] = {9, 10, 22, 23, 24, 25}, afc[] = {2, 3, 26, 27}, caf[] = {0, 1, 28, 29}, na[] = {6, 7, 30, 31};
    for (int i = 0; i < 6; i++) conf[sa[i]] = CONF_CONMEBOL;
    for (int i = 0; i < 4; i++) conf[afc[i]] = CONF_AFC, conf[caf[i]] = CONF_CAF, conf[na[i]] = CONF_CONCACAF;
    conf[8] = CONF_OFC;
    conf[11] = CONF_HOST;
    for (int p = 0; p < 4; p++)
        for (int i = 7; i > 0; i--) {
            int j = game_random(i + 1);
            if (rt_unwinding) { game_free(all); return 0; }
            int t = pot[p][i]; pot[p][i] = pot[p][j]; pot[p][j] = t;
        }
    /* Only pot 4 can break the rule: its two CONMEBOL clubs go to groups whose pot 1 club is from UEFA. */
    for (int g = 0; g < 8; g++) {
        if (conf[pot[3][g]] != CONF_CONMEBOL || conf[pot[0][g]] != CONF_CONMEBOL) continue;
        for (int h = 0; h < 8; h++)
            if (conf[pot[0][h]] == CONF_UEFA && conf[pot[3][h]] != CONF_CONMEBOL) {
                int t = pot[3][g]; pot[3][g] = pot[3][h]; pot[3][h] = t;
                break;
            }
    }
    uint32_t list = game_new(4u * 32u);
    if (!list) { game_free(all); return 0; }
    for (int g = 0; g < 8; g++)
        for (int k = 0; k < 4; k++)
            wr32(g_mem, list + 4u * (uint32_t)(4 * g + k), rd32(g_mem, all + 4u * (uint32_t)pot[k][g]));
    game_free(all);
    wr32(g_mem, obj + CWC_PICKS, list);
    wr32(g_mem, obj + CWC_PICK_COUNT, 32);
    return 1;
}

/* The groups on the start date, in the order of the Libertadores update 0x4c447d..0x4c45e7: group A (the parent,
 * 0x4c37a0), its fixtures (League::create_fixtures 0x6835c0), its table sort (0x6827d0 with 0), groups B to H
 * (0x4c3b10 with 0 to 6). No set_predicted_positions (vtable +0x5c, 0x92aba0): it reads 12 entrants and shuffles
 * the list (0x92ad36). Returns 1 when the object now holds Form G. */
static int build_groups(uint32_t obj)
{
    if (!stage_array(obj, GROUP_STAGES)) return 0;
    g_mem[obj + CWC_ADVANCE_TO] = 2;
    if (!pick_groups(obj)) return 0;
    if (!call_this(0x4c37a0u, obj, 0, NULL) || rt_unwinding) return 0;
    call_this(0x6835c0u, obj, 0, NULL);
    if (rt_unwinding) return 0;
    uint32_t zero[1] = {0};
    call_this(0x6827d0u, obj, 1, zero);
    if (rt_unwinding) return 0;
    for (uint32_t i = 0; i < 7; i++) {
        uint32_t a[1] = {i};
        call_this(0x4c3b10u, obj, 1, a);
        if (rt_unwinding) return 0;
    }
    game_free(rd32(g_mem, obj + CWC_PICKS));
    wr32(g_mem, obj + CWC_PICKS, 0);
    wr32(g_mem, obj + CWC_PICK_COUNT, 0);
    wr32(g_mem, obj + CWC_DRAWN, 1);                            /* the original's start branch now skips */
    int edition = (int16_t)rd16(g_mem, obj + YEAR_AT);
    fprintf(stderr, "recomp: club world groups of 32 clubs for %d\n", edition);
    KnockoutDates kd;
    if (edition_dates(edition, &kd)) {
        char a[16], b[16];
        fprintf(stderr, "recomp: club world knockout %d: first match %s, final %s\n", edition,
                date_text(kd.first, kd.year, a, sizeof a), date_text(kd.last, kd.year, b, sizeof b));
    }
    return 1;
}

void o_00929390(void);

/* WorldClubChamp::create_schedule (char stage, short *rounds, short *code, int *is_league), thiscall, ret 0x10.
 * Form G: stage -1 to 6 a group of 3 rounds (the original's table of stage -1, 0 or 1, 0x9293a2, with the days of
 * the table: the first day, 4 and 9 days later), stage 7 the knockout of 4 rounds (from the original's stage 2
 * rounds, 0x929632: the round of 16 14 days after the first day, the quarter-finals 20, the semi-finals 24, the
 * final on the last day). Every other case is the original's. */
static int in_schedule;                 /* 1 while the wrap below calls the original's stage 2 table */

RT_WRAP(00929390, world_club_champ_schedule)
{
    if (in_schedule) { o_00929390(); return; }                 /* the template call below: the original */
    uint32_t obj = cpu.ecx;
    int year = (int16_t)rd16(g_mem, obj + YEAR_AT), stage = (int8_t)(rt_arg(0) & 0xffu);
    KnockoutDates kd;
    /* Stages 0 to 7 only after build_groups made the 8 slots: a failed build plays the original form. */
    if (rd32(g_mem, obj) != CWC_VTABLE || !groups_form(year) || stage > 7 || (stage >= 0 && !is_groups(obj)) ||
        !edition_dates(year, &kd)) {
        o_00929390();
        return;
    }
    uint32_t rounds_at = rt_arg(1), code_at = rt_arg(2), league_at = rt_arg(3);
    if (stage < 7) {
        g_mem[cpu.esp + 4] = (uint8_t)(stage < 0 ? 0xff : stage == 0 ? 0 : 1);
        o_00929390();
        uint32_t table = cpu.eax;
        if (rt_unwinding || !table) return;
        static const int day[3] = {0, 4, 9};
        for (uint32_t i = 0; i < 3; i++) {
            wr16(g_mem, table + i * 0x41u, (uint16_t)(kd.first + day[i]));   /* League round: +0 day, +2 year */
            wr16(g_mem, table + i * 0x41u + 2, (uint16_t)kd.offset);         /* offset (0x68a1a5, 0x68a1ac) */
        }
        wr16(g_mem, code_at, (uint16_t)(0x3e9 + stage));
        return;
    }
    static uint32_t out;                                        /* rounds, code, is_league */
    if (!out) out = win_static_alloc(12);
    uint32_t a[4] = {2, out, out + 4, out + 8};
    in_schedule = 1;
    uint32_t orig = call_this(0x929390u, obj, 4, a);
    in_schedule = 0;
    if (rt_unwinding) return;
    uint32_t table = orig && (int16_t)rd16(g_mem, out) == 3 ? game_new(4u * ROUND_SIZE) : 0;
    if (rt_unwinding) return;
    static const struct { int tmpl, id, teams, ties, fresh, match; } r4[4] = {
        {0, 0x14, 16, 8, 16, 14}, {0, 0x78, 8, 4, 0, 20}, {0, 0x82, 4, 2, 0, 24}, {1, 0x96, 2, 1, 0, -1}};
    for (uint32_t i = 0; table && i < 4; i++) {
        uint32_t r = table + i * ROUND_SIZE;
        memcpy(g_mem + r, g_mem + orig + (uint32_t)r4[i].tmpl * ROUND_SIZE, ROUND_SIZE);
        int match = r4[i].match < 0 ? kd.last : kd.first + r4[i].match;
        int draw = i == 0 ? match - 2 : kd.first + r4[i - 1].match + 1;
        wr16(g_mem, r + 0x00, (uint16_t)draw);
        wr16(g_mem, r + 0x03, (uint16_t)match);
        g_mem[r + 0x02] = (uint8_t)kd.offset;
        g_mem[r + 0x05] = (uint8_t)kd.offset;
        wr16(g_mem, r + 0x07, (uint32_t)r4[i].id);
        wr16(g_mem, r + 0x09, i);
        g_mem[r + 0x0d] = 0x03;                                 /* extra time, penalties, no golden goal */
        wr16(g_mem, r + 0x18, (uint32_t)r4[i].teams);
        wr16(g_mem, r + 0x1a, (uint32_t)r4[i].ties);
        wr16(g_mem, r + 0x1c, (uint32_t)r4[i].fresh);
        wr16(g_mem, r + 0x1e, 0);
    }
    game_free(orig);
    if (rt_unwinding) return;
    wr16(g_mem, rounds_at, table ? 4 : 0);
    wr16(g_mem, code_at, 0);
    wr32(g_mem, league_at, 0);
    rt_ret_stdcall(table, 4);
}

void o_00929dd0(void);

/* WorldClubChamp::advance_stage (vtable +0x28): Form G takes the Libertadores advance 0x4c3a80 (+0x30 + 1, at 7
 * the knockout 0x4c3d10), which has the same calling form (thiscall, no argument, 0x4c3afa). */
RT_WRAP(00929dd0, world_club_champ_advance)
{
    if (is_groups(cpu.ecx)) {
        rt_jump(0x4c3a80u);
        return;
    }
    o_00929dd0();
}

void o_0092a6b0(void);

/* After League::load 0x687b10 (season_load.c). The load reads the saved club count +0x3e and makes the table +0xb1
 * only when the count is not 0 (0x6889db..0x688a07; the read lies in 0x687b10, which runs on past 0x688230). A
 * Form K parent holds 0 clubs and one zeroed row (parent_table), so a loaded one had no table and never ended
 * (League::check_complete 0x689add). The table is made here, as at the draw. */
void season_club_world_loaded(uint32_t obj)
{
    if (rd32(g_mem, obj) != CWC_VTABLE || rd32(g_mem, obj + CWC_STAGE_COUNT) != 1 || !rd32(g_mem, obj + CWC_DRAWN) ||
        rd32(g_mem, obj + CWC_TABLE) || rd16(g_mem, obj + CWC_CLUBS))
        return;
    Cpu saved = cpu;
    int ok = parent_table(obj);
    if (rt_unwinding) return;
    cpu = saved;
    fprintf(stderr, "recomp: club world knockout %d: parent table made at the load (%s)\n",
            (int16_t)rd16(g_mem, obj + YEAR_AT), ok ? "made" : "failed");
}

/* WorldClubChamp::update_for_date, thiscall (Date *today, int arg2), ret 8. Date: s16 day of the year, s16 year.
 * The start date test is that of 0x92a73d..0x92a765. */
RT_WRAP(0092a6b0, world_club_champ_update)
{
    uint32_t obj = cpu.ecx, today = rt_arg(0);
    int host_for_ofc;
    int year = (int16_t)rd16(g_mem, obj + YEAR_AT);
    int clubs = knockout_clubs(year, &host_for_ofc), groups = groups_form(year);
    if (groups && rd32(g_mem, obj) == CWC_VTABLE && !rd32(g_mem, obj + CWC_DRAWN) && rt_arg(1) == 0) {
        set_start_date(obj, year);                              /* part 26c, Form G */
        int day = (int16_t)rd16(g_mem, obj + CWC_START_DAY);
        int start_year = year + (int16_t)rd16(g_mem, obj + CWC_START_OFFSET);
        if ((int16_t)rd16(g_mem, today) == day && (int16_t)rd16(g_mem, today + 2) == start_year) {
            Cpu saved = cpu;
            int ok = build_groups(obj);
            if (rt_unwinding) return;
            cpu = saved;
            if (!ok) {
                fprintf(stderr, "recomp: club world groups %d failed, the original form plays\n", year);
                game_free(rd32(g_mem, obj + CWC_PICKS));
                wr32(g_mem, obj + CWC_PICKS, 0);
                wr32(g_mem, obj + CWC_PICK_COUNT, 0);
                g_mem[obj + CWC_ADVANCE_TO] = 1;
                if (rd32(g_mem, obj + CWC_STAGE_COUNT) != 3) original_form(obj);
                if (rt_unwinding) return;
                cpu = saved;
            }
        }
    } else if (clubs && rd32(g_mem, obj) == CWC_VTABLE && !rd32(g_mem, obj + CWC_DRAWN) && rt_arg(1) == 0) {
        set_start_date(obj, year);                              /* part 26c */
        int day = (int16_t)rd16(g_mem, obj + CWC_START_DAY);
        int start_year = year + (int16_t)rd16(g_mem, obj + CWC_START_OFFSET);
        if ((int16_t)rd16(g_mem, today) == day && (int16_t)rd16(g_mem, today + 2) == start_year) {
            Cpu saved = cpu;
            int ok = build_knockout(obj, clubs, host_for_ofc);
            if (rt_unwinding) return;
            cpu = saved;
            if (!ok && rd32(g_mem, obj + CWC_STAGE_COUNT) == 1) original_form(obj);
        }
    } else if (rd32(g_mem, obj) == CWC_VTABLE && rd32(g_mem, obj + CWC_STAGE_COUNT) == 1 &&
               rd32(g_mem, obj + CWC_DRAWN) && !rd32(g_mem, obj + CWC_TABLE)) {
        /* Net: season_club_world_loaded makes the table at the load. A Form K parent with no table here is a
         * fault; it never ends without one (0x68495e). */
        Cpu saved = cpu;
        int ok = parent_table(obj);
        if (rt_unwinding) return;
        cpu = saved;
        fprintf(stderr, "recomp: club world knockout %d: no parent table at the update, the net makes it (%s)\n",
                year, ok ? "made" : "failed");
    } else if (clubs == 0 && !groups && rd32(g_mem, obj) == CWC_VTABLE && rd32(g_mem, obj + CWC_STAGE_COUNT) != 3 &&
               !rd32(g_mem, obj + CWC_DRAWN)) {
        original_form(obj);                                     /* a later edition of the original form */
    }
    o_0092a6b0();
}

/* The history thunk of slot 12 (vtable +0x30), shared by 4 vtables: it forwards to stage slot 2 (0x40f593). A
 * Club World Championship of Form K holds its knockout in slot 0. Called first by the one wrap of 0x40f590
 * (season_intl_copa.c, season_change.h). Returns 1 when it handled the call. The target, Cup::record_season_honours 0x519a90 of the
 * Cup_Stage (vtable 0x969468 +0x30), is thiscall with no argument (ret 0 at 0x519c89). With slot 0 empty it
 * returns as that function does and writes no history: after a load the stage array has the saved +0x2c slots
 * (League::load 0x688e3a), so the original thunk would read slot 2 of a 1-slot array. */
int season_club_world_history_thunk(void)
{
    uint32_t obj = cpu.ecx;
    /* Form K: the knockout in slot 0. Form G (part 26c): in slot 7, as the Libertadores slot 12 0x637753. */
    uint32_t slot;
    if (rd32(g_mem, obj) != CWC_VTABLE) return 0;
    if (rd32(g_mem, obj + CWC_STAGE_COUNT) == 1) slot = 0;
    else if (rd32(g_mem, obj + CWC_STAGE_COUNT) == GROUP_STAGES) slot = 7;
    else return 0;                                              /* the original form */
    uint32_t stage = rd32(g_mem, rd32(g_mem, obj + CWC_STAGES) + 4u * slot);
    if (!stage) {
        rt_ret_cdecl(0);                                        /* ret 0: the return address only */
        return 1;
    }
    /* Test log: the clubs with fates 1 to 3 of the club list +0xa7 (6 bytes a club: pointer, seed, fate;
     * Cup slot 12 0x519aca..0x519aeb reads them), count +0x3e. The club's name is at +0x04 (51 bytes). */
    uint32_t list = rd32(g_mem, stage + 0xa7);
    int n = (int16_t)rd16(g_mem, stage + 0x3e);
    for (int i = 0; list && i < n && i < 32; i++) {
        uint32_t e = list + 6u * (uint32_t)i, club = rd32(g_mem, e);
        int fate = g_mem[e + 5];
        char today[16];
        if (club && fate >= 1 && fate <= 3)
            fprintf(stderr, "recomp: club world knockout %d: place %d %.50s on %s\n",
                    (int16_t)rd16(g_mem, obj + YEAR_AT), fate, win_str(club + 4),
                    date_text((int16_t)rd16(g_mem, 0xae2c90u), (int16_t)rd16(g_mem, 0xae2c92u), today, sizeof today));
    }
    cpu.ecx = stage;
    rt_jump(rd32(g_mem, rd32(g_mem, stage) + 0x30));
    return 1;
}

/* Form K: 1 for a Club World Championship whose knockout is its only stage. */
static int is_knockout(uint32_t obj)
{
    return rd32(g_mem, obj) == CWC_VTABLE && rd32(g_mem, obj + CWC_STAGE_COUNT) == 1;
}

/* The stage index argument (a byte) at esp + 4 + 4 * arg: 0 becomes 2 for Form K. For Form G (part 26c) the
 * groups 2 to 6 become 1 (a group, < 2) and the knockout 7 becomes 2. thiscall, so the callee pops the argument
 * and its slot needs no restore. */
static void stage_as_knockout(int arg)
{
    uint32_t slot = cpu.esp + 4u + 4u * (uint32_t)arg;
    if (is_knockout(cpu.ecx) && g_mem[slot] == 0) g_mem[slot] = 2;
    else if (is_groups(cpu.ecx) && g_mem[slot] >= 2 && g_mem[slot] <= 7) g_mem[slot] = g_mem[slot] == 7 ? 2 : 1;
}

void o_00929c70(void);
void o_009298a0(void);
void o_0092ad50(void);

/* WorldClubChamp::update_staff_records_for_status (club, fate, stage, ...), ret 0x18: stage < 2 at 0x929c74. */
RT_WRAP(00929c70, world_club_champ_staff_records) { stage_as_knockout(2); o_00929c70(); }
/* WorldClubChamp::build_fate_news_text (club index, fate, stage, ...), ret 0x28: stage < 2 at 0x9298a6. */
RT_WRAP(009298a0, world_club_champ_fate_news) { stage_as_knockout(2); o_009298a0(); }
/* WorldClubChamp::set_club_progress (club, stage, ...), ret 0x14: stage < 2 at 0x92ad5e. */
RT_WRAP(0092ad50, world_club_champ_progress) { stage_as_knockout(1); o_0092ad50(); }
