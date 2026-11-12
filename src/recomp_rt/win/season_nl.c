/* The UEFA Nations League in the game (0.6.0 slice 28, season_nl.h). Research and evidence:
 * docs/research/2026-10-18-nations-league.md (the design), 2026-10-18-nations-league-class.md (the class
 * euro_champ_qual, named "class doc" below) and 2026-10-18-national-team-comps.md ("nations doc").
 *
 * The object. One object of the class euro_champ_qual (constructor 0x57ce10, size 0x102, vtable 0x96a0d4) holds
 * every league of an edition: the League object is group A1 and stage[0..G-2] are the other groups, in the
 * order of the leagues (class doc 10, "One object or four"). Stage[G-1] is the finals of four. It takes the
 * record "Asian Cup Qualifying" (record 444 in the 3.9.68 data), the one nation competition record that no
 * code of the original reads and that has no history (nations doc 5.5), through the shared module
 * season_comps.h, in league table entry 1 (create_other_int_comps 0x830ac0, update day 15 August).
 *
 * The class runs its own daily update (0x57fd60): on its draw day it calls collect_entrants 0x57f600, the draw
 * 0x57dab0, the table of group one 0x57e7e0, the fixtures, the group stages 0x57ed10 (eight, `cmp edi, 8` at
 * 0x57ff34) and the runners-up stage 0x57f920. These are direct calls (class doc 0, point 3), so the wraps
 * below test the object: for the Nations League the entrants and the runners-up do nothing, the draw writes the
 * groups of the edition into the draw grid +0xf7, the group sizes come from that draw, and the site at 0x57ff34
 * makes G - 1 group stages. A vtable copy changes start_new_season (slot 2), update_for_date (slot 5),
 * advance_stage (slot 10, the finals), the history (slot 12) and the round template (slot 15), and takes the
 * League base functions for the slots that feed the European Championship (slots 14, 17, 18, 23, 26, 28, 29,
 * 30 and 37, class doc 10).
 *
 * The leagues of each nation. The first edition of a game takes the real leagues and groups of the edition
 * (season_nl_tiers). A later edition takes the leagues from the results of the one before (season_nl_rules.c):
 * start_new_season keeps the tables of the edition that ended, so the save holds them until the draw of the
 * next edition, which reads them, then clears them with the original start_new_season 0x57eb20. The save needs
 * no new field.
 *
 * Every function here acts only in a season game (season_comps_on) and on the Nations League object. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_nl.h"
#include "season_wcq26.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORD_NAME "Asian Cup Qualifying"
#define ENTRY 1                         /* create_other_int_comps 0x830ac0 (nations doc 6) */
#define QUAL_CTOR 0x0057ce10u           /* thiscall (year, record), ret 8 */
#define QUAL_SIZE 0x102u                /* operator new at 0x830d15 */
#define QUAL_VTABLE 0x0096a0d4u
#define LEAGUE_VTABLE 0x0096d020u       /* League (written at 0x68227e) */
#define VTABLE_SLOTS 45
#define QUAL_SET_UP_RULES 0x0057e8f0u   /* thiscall (), returns 1: rules, draw date, template (slot 35) */
#define QUAL_START 0x0057eb20u          /* start_new_season, thiscall () */
#define QUAL_UPDATE 0x0057fd60u         /* update_for_date, thiscall (today, full), ret 8 */

#define GAME_NEW 0x00944cf1u            /* operator new, cdecl (size) */
#define GAME_MALLOC 0x00944e46u         /* malloc, cdecl (size) */
#define GAME_FREE 0x009452cau           /* free, cdecl (block) */
#define GAME_RANDOM 0x0090cfc0u         /* random, cdecl (n): 0 to n - 1 */
#define DATE_NEW 0x00549f70u            /* Date::Date, thiscall (day, month0, year, weekday or 0xffff), ret 0x10 */
#define NATIONAL_TEAM 0x0053b3d0u       /* cdecl (nation record) */
#define TABLE_SORT 0x006827d0u          /* League table sort, thiscall (0) */
#define CUP_STAGE_NEW 0x00522e00u       /* Cup_Stage ctor, thiscall, 15 arguments */
#define CUP_START 0x0051c800u           /* Cup::set_up_round, thiscall (0) */
#define ROUND_DATE 0x0068a160u          /* league_round_set_date, cdecl (rounds, i, day, month0, yoff, weekday,
                                           flag, year, 0) */
#define ROUND_MATCH 0x0068a1c0u         /* league_round_set_match_slot, cdecl (rounds, i, 0, -1, -1, -1, 0) */
#define CUP_FIRST_DATE 0x00521e60u      /* cup_set_round_draw_date, cdecl (rec, round, day, month0, yoff, weekday, year) */
#define CUP_LAST_DATE 0x00521eb0u       /* cup_set_round_match_date, cdecl (rec, round, day, month0, yoff, weekday,
                                           time, year, venue) */
#define CUP_SLOT 0x00521f10u            /* cdecl (rec, round, slot, weekday, time, 3, flag) */
#define QUAL_CLEAR 0x0090fbe0u          /* the class's slot 12: clear_competition_for_comp_clubs, thiscall () */

#define NATIONS 0xae23a8u               /* nation records, 0x122 bytes */
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define NATION_NAME 0x4u
#define NATION_CONTINENT 0x71u
#define NATION_REPUTATION 0x8eu
#define CONTINENT_EUROPE 0x9cfa10u
#define CLUBS 0xae23b4u                 /* club records, 0x245 bytes (0x53b4c2..0x53b4e0) */
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define GAME_YEAR 0xae2c92u

/* Object fields (class doc 2 and 3). */
#define F_RECORD 0x4u
#define F_STAGES 0xcu
#define F_STAGE_COUNT 0x2cu
#define F_STAGE 0x30u                   /* current stage */
#define F_YEAR 0x40u
#define F_DRAWN 0x45u                   /* 1 after the draw (0x57fe1d) */
#define F_FINISHED 0xddu                /* 1 once the League object's own table is done (0x689c06) */
#define F_TABLE 0xb1u                   /* League table: rows of 0x3b bytes (0x57e7f7..0x57e803) */
#define F_TEAMS 0x3eu
#define F_DRAW_DAY 0xb6u                /* word: day of the year of the draw (0x57e98f) */
#define F_DRAW_YEAR 0xb8u               /* word: its year offset from +0x40 (0x57e9aa) */
#define F_TEMPLATE 0xbau                /* round template (0x57e9b8) */
#define F_GRID 0xf7u                    /* the draw grid: 6 dwords a group (0x57edbe..0x57eddb) */
#define F_LAST_GROUP 0xffu              /* byte: index of the last group stage (0x57cea2) */
#define F_RUNNERS_UP 0x100u             /* byte (0x57cea9) */
#define F_PLAYOFF 0x101u                /* byte (0x57ceb0) */
#define ROW_SIZE 0x3bu
#define ROW_POINTS 0x19u                /* word: points of the overall table (comparator 0x683466) */
#define ROUND_SIZE 0x68u                /* cup round record */
#define LEAGUE_ROUND 0x41u              /* league round record (0x68a160) */

#define MAX_GROUPS 20
#define GROUP_ROOM 6                    /* draw grid places a group (0x57dabb) */

/* ---- Helpers ---- */

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_NL_LOG");
        on = v && *v == '1';
    }
    return on;
}

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t vcall(uint32_t self, uint32_t slot, int nargs, const uint32_t *args)
{
    return call_this(rd32(g_mem, rd32(g_mem, self) + slot), self, nargs, args);
}

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(GAME_MALLOC, 1, a);
    if (readable(p)) memset(g_mem + p, 0, size);
    return readable(p) ? p : 0;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (p) call_cdecl(GAME_FREE, 1, a);
}

static int random_below(int n)
{
    if (n <= 1) return 0;
    uint32_t a[1] = {(uint32_t)n};
    int r = (int16_t)call_cdecl(GAME_RANDOM, 1, a);
    return r < 0 || r >= n ? 0 : r;
}

static int year_of(uint32_t comp) { return (int16_t)rd16(g_mem, comp + F_YEAR); }

/* The day of the year of a date (Date::Date 0x549f70 with weekday 0xffff: no move, 0x54a0f2): +0 of the Date.
 * The game compares dates by year (+2) and day of the year (+0), as update_for_date does (0x57fdcf..0x57fdf0). */
int season_nl_day_of_year(int day, int month1, int year)
{
    uint32_t d = game_malloc(8);
    if (!d || rt_unwinding) return 0;
    uint32_t a[4] = {(uint32_t)day, (uint32_t)(month1 - 1), (uint32_t)year, 0xffffu};
    call_this(DATE_NEW, d, 4, a);
    int doy = rt_unwinding ? 0 : (int16_t)rd16(g_mem, d);
    game_free(d);
    return doy;
}

/* ---- Nations ---- */

static int nation_count(void) { return (int)rd32(g_mem, NATION_COUNT); }
static uint32_t nation_at(int id) { return rd32(g_mem, NATIONS) + (uint32_t)id * NATION_SIZE; }

static int european(int id)
{
    uint32_t cont = rd32(g_mem, nation_at(id) + NATION_CONTINENT);
    return readable(cont) && rd32(g_mem, cont) == rd32(g_mem, CONTINENT_EUROPE);
}

int season_nl_nation_of_club(uint32_t club)
{
    if (!readable(club)) return -1;
    uint32_t n = rd32(g_mem, club + CLUB_NATION), base = rd32(g_mem, NATIONS);
    if (!readable(n) || n < base || (n - base) % NATION_SIZE) return -1;
    int id = (int)((n - base) / NATION_SIZE);
    return id < nation_count() ? id : -1;
}

/* 1 when the nation has a continent (nation +0x71). The data keeps nations with no continent (-1 or -2), such as
 * Martinique, Guadeloupe, Bonaire, French Guiana, Sint Maarten and Netherlands Antilles in the October 2022
 * database; NationalTeams gives them no squad sizes (0x775520 reads 0 for each kind), so the top-up 0x90dae0 gives
 * their national team no players and init_match 0x6b7f20 stops at match_eng.cpp 612 or 652 (fewer than 11 at
 * 0x6b89be): Martinique in its first CONCACAF Nations League match of 2023/24 in a 2022 start. The field is a
 * pointer after the load's id pass and an id before it (the draw at game creation). The first form of this test
 * took NULL for continent id 0, so after the pass the nations with no continent still played: Bonaire, French
 * Guiana, Guadeloupe, Martinique and Saint-Martin in the 2019-20 CONCACAF Nations League (gate (a) run 7, 2018
 * start, where the national fill gave them squad sizes). */
int season_nl_nation_can_play(int id)
{
    if (id < 0 || id >= nation_count()) return 0;
    uint32_t c = rd32(g_mem, nation_at(id) + NATION_CONTINENT);
    if (readable(c)) return 1;
    /* After the id pass a continent id below 0 is NULL (0x5315a0), so NULL is no continent; before it the field is
     * the id, and 0 is a continent. The pass has run when any nation holds a pointer there. */
    for (int k = 0; k < nation_count(); k++)
        if (readable(rd32(g_mem, nation_at(k) + NATION_CONTINENT))) return 0;
    return (int32_t)c >= 0;
}

/* The national team of a nation: national_team 0x53b3d0 gives the club of the nation's row, which in some
 * season databases is an empty club with no nation (docs/research/2026-10-18-international.md, "Finals that
 * start before their qualifying ends"). Then the national team rows (from clubs - 2 * nations, 0x53b4c2) are
 * searched for the club whose nation is this one. 0 when none. */
uint32_t season_nl_nation_team(int id)
{
    uint32_t a[1] = {nation_at(id)};
    uint32_t c = call_cdecl(NATIONAL_TEAM, 1, a);
    if (rt_unwinding) return 0;
    if (season_nl_nation_of_club(c) == id) return c;
    int clubs = (int)rd32(g_mem, CLUB_COUNT), nations = nation_count();
    uint32_t base = rd32(g_mem, CLUBS);
    for (int i = clubs - 2 * nations; i >= 0 && i < clubs - nations; i++) {
        uint32_t club = base + (uint32_t)i * CLUB_SIZE;
        if (season_nl_nation_of_club(club) == id) return club;
    }
    return 0;
}

/* A name of the table ("Czech Republic/Czechia") against a name of the data. The table is UTF-8, the data
 * Windows-1252 (season_change_latin1). */
static int name_matches(const char *names, const char *data)
{
    char buf[128];
    snprintf(buf, sizeof buf, "%s", names);
    season_change_latin1(buf);
    char *p = buf;
    while (p && *p) {
        char *slash = strchr(p, '/');
        if (slash) *slash = 0;
        if (!strcmp(p, data)) return 1;
        p = slash ? slash + 1 : NULL;
    }
    return 0;
}

/* The nation of a table name "Old/New": the latest name first. A database can hold the old record and a record
 * with the new name (December 2025: nation 66 "FYR of Macedonia", with no clubs and no players, and nation 229
 * "North Macedonia"), and a first match by id took the old record: its national team had no squad, and the first
 * 2026-27 match stopped at virtual_staff.cpp 195 (VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0, box at 0x90d4c1:
 * fewer than 15 filled squad slots of +0xd7 after the virtual players). A database with the old name only (and
 * slice 27's rename, season_intl.c) still finds it by the earlier name. */
static int nation_by_name(const char *names)
{
    char buf[128], *alt[8];
    int n = 0;
    snprintf(buf, sizeof buf, "%s", names);
    season_change_latin1(buf);
    for (char *p = buf; p && *p && n < 8;) {
        char *slash = strchr(p, '/');
        if (slash) *slash = 0;
        alt[n++] = p;
        p = slash ? slash + 1 : NULL;
    }
    /* A nation that can play first: the season databases from 2013 keep the dissolved Netherlands Antilles (no
     * continent) beside Curaçao, so "Curaçao/Netherlands Antilles" takes Curaçao there and the Antilles only in
     * the data that has no Curaçao. */
    int any = -1;
    for (int k = n - 1; k >= 0; k--)
        for (int id = 0; id < nation_count(); id++)
            if (!strcmp(alt[k], win_str(nation_at(id) + NATION_NAME))) {
                if (season_nl_nation_can_play(id)) return id;
                if (any < 0) any = id;
            }
    return any;
}

/* ---- Editions ---- */

/* One object per confederation: the UEFA Nations League and the CONCACAF Nations League (part b). Each holds its
 * own edition after the table (`repeat`). */
typedef struct {
    const char *confed;
    const SeasonCompDef *def;
    SeasonNlEdition repeat;     /* an edition after the table, or with changing rules off */
    uint32_t log_state;         /* test log: the state of the edition last written */
} NlConf;

extern const SeasonCompDef season_nl_comp, season_nl_concacaf_comp;
static NlConf confs[] = {{"UEFA", &season_nl_comp, {0}, 0xffffffffu}, {"CONCACAF", &season_nl_concacaf_comp, {0}, 0xffffffffu}};
#define CONFS ((int)(sizeof confs / sizeof confs[0]))

static NlConf *conf_of_def(const SeasonCompDef *d)
{
    for (int i = 0; i < CONFS; i++)
        if (confs[i].def == d) return &confs[i];
    return NULL;
}

static const SeasonNlEdition *table_edition(const NlConf *c, int year)
{
    for (int i = 0; i < season_nl_edition_count; i++)
        if (!strcmp(season_nl_editions[i].confed, c->confed) && season_nl_editions[i].year == year)
            return &season_nl_editions[i];
    return NULL;
}

/* The edition of a year in this game. With changing rules on, the table's edition; after the table's last
 * edition the last one again every two years (Q248: the rules stay). With changing rules off, the edition of
 * the start season, again every two years (Q232). NULL for a year with no edition. */
static const SeasonNlEdition *edition(NlConf *c, int year)
{
    const SeasonNlEdition *base = NULL;
    if (season_changing_rules()) {
        const SeasonNlEdition *e = table_edition(c, year);
        if (e) return e;
        for (int i = 0; i < season_nl_edition_count; i++)
            if (!strcmp(season_nl_editions[i].confed, c->confed) && season_nl_editions[i].year < year)
                base = &season_nl_editions[i];
    } else {
        base = season_nl_first(c->confed, season_start_year());
        if (base && base->year == year) return base;
        if (base && base->year > year) base = NULL;
    }
    if (!base || (year - base->year) % 2) return NULL;
    c->repeat = *base;
    c->repeat.season = base->season + (year - base->year);
    c->repeat.year = year;
    return &c->repeat;
}

static int next_year(const NlConf *c, int year)
{
    if (season_changing_rules())
        for (int i = 0; i < season_nl_edition_count; i++)
            if (!strcmp(season_nl_editions[i].confed, c->confed) && season_nl_editions[i].year > year)
                return season_nl_editions[i].year;
    return year + 2;
}

/* The name of an edition in the test log: "2018-19" for UEFA (tests/seasons/season_nl_run.py reads it),
 * "CONCACAF-2019-20" for CONCACAF. */
static const char *label(const SeasonNlEdition *e)
{
    static char buf[2][40];
    static int k;
    if (!strcmp(e->confed, "UEFA")) return e->edition;
    k ^= 1;
    snprintf(buf[k], sizeof buf[k], "%s-%s", e->confed, e->edition);
    return buf[k];
}

static int group_count(const SeasonNlEdition *e)
{
    int g = 0;
    for (int l = 0; l < e->leagues; l++) g += e->groups[l];
    return g > MAX_GROUPS ? MAX_GROUPS : g;
}

/* The league of draw group g (0 = A1, the League object). */
static int group_league(const SeasonNlEdition *e, int g)
{
    for (int l = 0; l < e->leagues; l++) {
        if (g < e->groups[l]) return l;
        g -= e->groups[l];
    }
    return e->leagues - 1;
}

static int first_group(const SeasonNlEdition *e, int league)
{
    int g = 0;
    for (int l = 0; l < league && l < e->leagues; l++) g += e->groups[l];
    return g;
}

/* ---- The object ---- */

/* The confederation of a Nations League object in a season game, else NULL. */
static NlConf *conf_of(uint32_t self)
{
    if (!self || !season_comps_on()) return NULL;
    for (int i = 0; i < CONFS; i++)
        if (season_comps_object(confs[i].def) == self) return &confs[i];
    return NULL;
}

static int is_nl(uint32_t self) { return conf_of(self) != NULL; }

static uint32_t stage_at(uint32_t self, int i)
{
    uint32_t stages = rd32(g_mem, self + F_STAGES);
    int n = (int32_t)rd32(g_mem, self + F_STAGE_COUNT);
    if (i == -1) return self;
    if (!readable(stages) || i < 0 || i >= n) return 0;
    return rd32(g_mem, stages + 4u * (uint32_t)i);
}

/* The group count of the edition that the object holds: the stage array has one slot a group after group
 * one, and one for the finals (make_stages), so G = +0x2c. */
static int held_groups(uint32_t self) { return (int32_t)rd32(g_mem, self + F_STAGE_COUNT); }

static void set_stage_bytes(uint32_t self, int groups)
{
    wr8(g_mem, self + F_LAST_GROUP, (uint32_t)(groups - 2));
    wr8(g_mem, self + F_RUNNERS_UP, (uint32_t)(groups - 1));
    wr8(g_mem, self + F_PLAYOFF, (uint32_t)(groups - 1));
}

/* A new stage array of `groups` slots: groups 2 to G and the finals. The class's start_new_season keeps the
 * array (class doc 7) and the League load makes it from the saved count +0x2c (0x688e11, 0x688e53). */
static void make_stages(uint32_t self, int groups)
{
    if (held_groups(self) != groups) {
        uint32_t stages = game_malloc(4u * (uint32_t)groups);
        if (rt_unwinding || !stages) return;
        game_free(rd32(g_mem, self + F_STAGES));
        if (rt_unwinding) return;
        wr32(g_mem, self + F_STAGES, stages);
        wr32(g_mem, self + F_STAGE_COUNT, (uint32_t)groups);
    } else {
        /* start_new_season deleted the stages (0x57eb58..0x57eb6e) but keeps their addresses in the array. */
        uint32_t stages = rd32(g_mem, self + F_STAGES);
        if (readable(stages)) memset(g_mem + stages, 0, 4u * (size_t)groups);
    }
    set_stage_bytes(self, groups);
}

/* The draw day of the edition at +0x40 into +0xb6 and +0xb8. A draw day that has passed becomes tomorrow
 * (the 2018-19 draw of 24 January 2018 lies before a game that starts in July 2018). */
static void set_draw_day(uint32_t self, const SeasonNlEdition *e)
{
    int year = year_of(self);
    int y = year + e->draw.year, doy = season_nl_day_of_year(e->draw.day, e->draw.month, y);
    if (rt_unwinding) return;
    int ty = (int16_t)rd16(g_mem, 0xae2c90u + 2u), td = (int16_t)rd16(g_mem, 0xae2c90u);
    if (y < ty || (y == ty && doy <= td)) {
        y = ty;
        doy = td + 1;
        if (doy > season_nl_day_of_year(31, 12, ty)) {
            y++;
            doy = 0;
        }
    }
    wr16(g_mem, self + F_DRAW_DAY, (uint32_t)doy);
    wr16(g_mem, self + F_DRAW_YEAR, (uint32_t)(y - year));
}

/* The rules, the draw day and the template of group one for the edition at +0x40: set_up_rules 0x57e8f0 (its
 * template comes through vtable +0x3c, so from round_template below), then the draw day of the table. */
static void set_up(uint32_t self, const SeasonNlEdition *e)
{
    game_free(rd32(g_mem, self + F_TEMPLATE));
    wr32(g_mem, self + F_TEMPLATE, 0);
    if (rt_unwinding) return;
    call_this(QUAL_SET_UP_RULES, self, 0, NULL);
    if (rt_unwinding) return;
    set_draw_day(self, e);
}

/* ---- The draw ---- */

/* The groups of the next draw, made on the draw day before the class's update runs. */
static int draw_groups_n;
static uint32_t draw_team[MAX_GROUPS][GROUP_ROOM];
static int draw_size[MAX_GROUPS];

typedef struct {
    int nation, league, group, rep;
} Entrant;

static int by_rep(const void *a, const void *b)
{
    const Entrant *x = a, *y = b;
    if (x->rep != y->rep) return y->rep - x->rep;
    return x->nation - y->nation;
}

/* Fills the groups of league l with its nations: the real groups of the table where they are known, else by
 * pots of the reputation (nation +0x8e), one nation of each pot a group, at random. */
static void draw_league(const SeasonNlEdition *e, int l, Entrant *list, int n)
{
    int groups = e->groups[l], g0 = first_group(e, l);
    if (groups <= 0) return;
    Entrant mine[NL_MAX_NATIONS];
    int k = 0;
    for (int i = 0; i < n; i++)
        if (list[i].league == l && k < NL_MAX_NATIONS) mine[k++] = list[i];
    int known = k > 0, per[MAX_GROUPS] = {0};
    for (int i = 0; i < k; i++) {
        if (mine[i].group < 1 || mine[i].group > groups) known = 0;
        else per[mine[i].group - 1]++;
    }
    /* A real group that keeps fewer than two nations of the data (CONCACAF members that the game lacks, such as
     * Bonaire or Sint Maarten) cannot play: the league is drawn by pots instead. */
    for (int g = 0; known && g < groups; g++)
        if (per[g] < 2) known = 0;
    if (known) {
        for (int i = 0; i < k; i++) {
            int g = g0 + mine[i].group - 1;
            if (draw_size[g] < GROUP_ROOM) draw_team[g][draw_size[g]++] = season_nl_nation_team(mine[i].nation);
            if (rt_unwinding) return;
        }
        return;
    }
    for (int i = 0; i < k; i++) mine[i].rep = (int16_t)rd16(g_mem, nation_at(mine[i].nation) + NATION_REPUTATION);
    qsort(mine, (size_t)k, sizeof mine[0], by_rep);
    for (int pot = 0; pot * groups < k; pot++) {
        int order[MAX_GROUPS];
        for (int g = 0; g < groups; g++) order[g] = g;
        for (int g = groups - 1; g > 0; g--) {
            int r = random_below(g + 1);
            if (rt_unwinding) return;
            int t = order[g];
            order[g] = order[r];
            order[r] = t;
        }
        for (int j = 0; j < groups && pot * groups + j < k; j++) {
            int g = g0 + order[j];
            if (draw_size[g] < GROUP_ROOM) draw_team[g][draw_size[g]++] = season_nl_nation_team(mine[pot * groups + j].nation);
            if (rt_unwinding) return;
        }
    }
}

/* The real leagues of an edition, for the nations that the data holds. Returns the count. */
static int real_entrants(const SeasonNlEdition *e, Entrant *out)
{
    int n = 0;
    for (int i = 0; i < season_nl_tier_count && n < NL_MAX_NATIONS; i++) {
        const SeasonNlTier *t = &season_nl_tiers[i];
        if (strcmp(t->confed, e->confed) || strcmp(t->edition, e->edition) || t->out) continue;
        int id = nation_by_name(t->nation);
        if (id < 0) {
            if (logging()) fprintf(stderr, "recomp: season nl: %s %s: no nation %s in the data\n", e->confed,
                                   e->edition, t->nation);
            continue;
        }
        if (!season_nl_nation_can_play(id)) {
            if (logging()) fprintf(stderr, "recomp: season nl: %s %s: %s has no continent in the data\n", e->confed,
                                   e->edition, t->nation);
            continue;
        }
        out[n++] = (Entrant){id, t->league, t->group, 0};
    }
    return n;
}

/* 1 when the real leagues of the edition leave out a nation (Russia from 2022-23: the table holds it with out
 * set, and later editions do not list it). An edition with no real leagues leaves out none. */
static int left_out(const SeasonNlEdition *e, int nation)
{
    int any = 0;
    for (int i = 0; i < season_nl_tier_count; i++) {
        const SeasonNlTier *t = &season_nl_tiers[i];
        if (strcmp(t->confed, e->confed) || strcmp(t->edition, e->edition)) continue;
        any = 1;
        if (name_matches(t->nation, win_str(nation_at(nation) + NATION_NAME))) return t->out;
    }
    return any;
}

/* The results of the edition that the object holds (the group tables), one row a nation. */
static int read_results(uint32_t self, const SeasonNlEdition *old, SeasonNlRow *rows)
{
    int n = 0, groups = held_groups(self);
    /* Group g is the League object (g 0) or stage[g - 1]; the stage array has a slot for each group after the
     * first and one for the finals (make_stages), so the groups are 0 to +0x2c - 1. */
    for (int g = 0; g < groups && g < MAX_GROUPS; g++) {
        uint32_t st = stage_at(self, g - 1);
        if (!readable(st)) continue;
        uint32_t a[1] = {0};
        call_this(TABLE_SORT, st, 1, a);
        if (rt_unwinding) return n;
        int teams = (int16_t)rd16(g_mem, st + F_TEAMS);
        uint32_t table = rd32(g_mem, st + F_TABLE);
        if (!readable(table) || teams <= 0 || teams > GROUP_ROOM) continue;
        for (int r = 0; r < teams && n < NL_MAX_NATIONS; r++) {
            uint32_t row = table + (uint32_t)r * ROW_SIZE;
            int nation = season_nl_nation_of_club(rd32(g_mem, row));
            if (nation < 0) continue;
            rows[n++] = (SeasonNlRow){nation, group_league(old, g), g - first_group(old, group_league(old, g)) + 1,
                                      r + 1, rd8(g_mem, row + 0x11u), (int16_t)rd16(g_mem, row + ROW_POINTS), 0, 0,
                                      0};
        }
    }
    return n;
}

int season_nl_held_results(int year, SeasonNlRow *rows)
{
    NlConf *c = &confs[0];
    uint32_t self = season_comps_object(c->def), st0 = self ? stage_at(self, 0) : 0;
    if (!self || !readable(st0) || !rd32(g_mem, self + F_DRAWN) || year_of(st0) != year) return 0;
    const SeasonNlEdition *e = edition(c, year);
    if (!e) return 0;
    SeasonNlEdition old = *e;
    int n = read_results(self, &old, rows);
    if (rt_unwinding) return 0;
    season_nl_rank(rows, n);
    return n;
}

/* The entrants of the edition e: the leagues from the results of the edition that the object holds, or the
 * real leagues of e when the object holds none. */
static int entrants(uint32_t self, const SeasonNlEdition *e, Entrant *out)
{
    uint32_t st0 = stage_at(self, 0);
    NlConf *c = conf_of(self);
    const SeasonNlEdition *old = c && readable(st0) && rd32(g_mem, self + F_DRAWN) ? edition(c, year_of(st0)) : NULL;
    SeasonNlEdition from;
    if (old) from = *old;                               /* edition() may give the static `repeat` */
    SeasonNlRow rows[NL_MAX_NATIONS];
    int n = old ? read_results(self, &from, rows) : 0;
    if (rt_unwinding) return 0;
    if (n < 4) return real_entrants(e, out);
    int next[NL_MAX_NATIONS], k = 0;
    if (!season_nl_next_leagues(&from, e, rows, n, next)) return real_entrants(e, out);
    for (int i = 0; i < n; i++) {
        if (left_out(e, rows[i].nation) || !season_nl_nation_can_play(rows[i].nation)) continue;
        out[k++] = (Entrant){rows[i].nation, next[i], 0, 0};
        if (logging() && next[i] != rows[i].league)
            fprintf(stderr, strcmp(e->confed, "UEFA") ? "recomp: season nl: %s: %s moves from league %c to %c\n"
                                                      : "recomp: season nl: %s%s goes from league %c to %c\n",
                    strcmp(e->confed, "UEFA") ? e->confed : "", win_str(nation_at(rows[i].nation) + NATION_NAME),
                    'A' + rows[i].league, 'A' + next[i]);
    }
    /* A nation of the real leagues that did not play the edition before (a nation new in the data). */
    Entrant real[NL_MAX_NATIONS];
    int m = real_entrants(e, real);
    for (int j = 0; j < m && k < NL_MAX_NATIONS; j++) {
        int have = 0;
        for (int i = 0; i < k; i++) have |= out[i].nation == real[j].nation;
        if (!have) out[k++] = (Entrant){real[j].nation, real[j].league, 0, 0};
    }
    return k;
}

/* Every group needs two nations: a group of one club has no matches, and League_Stage::load_from_file 0x68b720
 * stops at the box "league_stage.cpp 533" when it reads the 0 bytes of its results (0x68c88d..0x68c898). A league
 * can hold fewer nations than two a group when the data lacks nations of the table (the non-FIFA members of
 * CONCACAF, and more of them in an old database: a 2001 start in 2022-23 had a League C group of one). A short
 * group takes the last nation (the lowest pot) of the largest group of its league that holds more than two, else
 * of the nearest league (above first). The nation then plays that edition in the group's league; the next
 * edition's leagues come from these groups (read_results). */
static void balance_groups(const SeasonNlEdition *e)
{
    for (int pass = 0; pass < MAX_GROUPS * GROUP_ROOM; pass++) {
        int g = 0;
        while (g < draw_groups_n && draw_size[g] >= 2) g++;
        if (g == draw_groups_n) return;
        int l = group_league(e, g), donor = -1;
        for (int dist = 0; donor < 0 && dist < e->leagues; dist++)
            for (int side = 0; side < 2 && donor < 0; side++) {
                int want = side ? l + dist : l - dist;
                if ((dist == 0 && side) || want < 0 || want >= e->leagues) continue;
                int best = 2;
                for (int d = 0; d < draw_groups_n; d++)
                    if (d != g && group_league(e, d) == want && draw_size[d] > best) best = draw_size[d], donor = d;
            }
        if (donor < 0) return;
        draw_team[g][draw_size[g]++] = draw_team[donor][--draw_size[donor]];
        if (logging())
            fprintf(stderr, "recomp: season nl: %s: a nation of group %d moves to group %d (fewer than two nations)\n",
                    label(e), donor + 1, g + 1);
    }
}

static void prepare_draw(uint32_t self, const SeasonNlEdition *e)
{
    memset(draw_team, 0, sizeof draw_team);
    memset(draw_size, 0, sizeof draw_size);
    draw_groups_n = group_count(e);
    Entrant list[NL_MAX_NATIONS];
    int n = entrants(self, e, list);
    for (int l = 0; l < e->leagues && !rt_unwinding; l++) draw_league(e, l, list, n);
    if (rt_unwinding) return;
    for (int g = 0; g < draw_groups_n; g++) {
        int k = 0;
        for (int i = 0; i < draw_size[g]; i++)
            if (draw_team[g][i]) draw_team[g][k++] = draw_team[g][i];
        draw_size[g] = k;
    }
    balance_groups(e);
    for (int g = 0; g < draw_groups_n; g++)
        if (draw_size[g] < 2)
            fprintf(stderr, "recomp: season nl: %s group %d holds %d nations\n", label(e), g + 1, draw_size[g]);
    if (logging())
        for (int g = 0; g < draw_groups_n; g++) {
            int l = group_league(e, g);
            fprintf(stderr, "recomp: season nl: %s draw league %c group %d:", label(e), 'A' + l,
                    g - first_group(e, l) + 1);
            for (int i = 0; i < draw_size[g]; i++)
                fprintf(stderr, " %s;", win_str(nation_at(season_nl_nation_of_club(draw_team[g][i])) + NATION_NAME));
            fputc('\n', stderr);
        }
}

/* ---- The round templates (vtable +0x3c) ---- */

void season_nl_league_round(uint32_t rec, int i, SeasonNlDate d, int year)
{
    /* The weekday of the date itself, so the date does not move; flag 1 for a weekend and 2 for a midweek
     * round, as the class's own rounds (Saturday 1, Wednesday 2: 0x57d22b..0x57d261). */
    int weekday = season_nl_weekday(d.day, d.month, year + d.year), flag = weekday >= 5 ? 1 : 2;
    uint32_t a[9] = {rec, (uint32_t)i, (uint32_t)d.day, (uint32_t)(d.month - 1), (uint32_t)d.year,
                     (uint32_t)weekday, (uint32_t)flag, (uint32_t)year, 0};
    call_cdecl(ROUND_DATE, 9, a);
    if (rt_unwinding) return;
    uint32_t b[7] = {rec, (uint32_t)i, 0, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0};
    call_cdecl(ROUND_MATCH, 7, b);
}

static void cup_round(uint32_t rec, int round, SeasonNlDate d, int year, int teams, int value, uint32_t flags)
{
    /* A cup round record keeps the day of the year and a year offset from the stage's year (+0x00 and +0x02 for
     * the draw, +0x03 and +0x05 for the match: 0x521ea6, 0x521ea9, 0x521ef6, 0x521efe), so the year argument is
     * the edition year and the offset is the day's (the class's play-off passes +0x40 and 1, 0x57d494..0x57d4b6).
     * The draw on the first day of the month, the match on its day, weekday -1 (no move). */
    uint32_t f[7] = {rec, (uint32_t)round, 1, (uint32_t)(d.month - 1), (uint32_t)d.year, 0xffffffffu, (uint32_t)year};
    call_cdecl(CUP_FIRST_DATE, 7, f);
    if (rt_unwinding) return;
    uint32_t l[9] = {rec, (uint32_t)round, (uint32_t)d.day, (uint32_t)(d.month - 1), (uint32_t)d.year, 0xffffffffu, 1,
                     (uint32_t)year, 0};
    call_cdecl(CUP_LAST_DATE, 9, l);
    if (rt_unwinding) return;
    uint32_t r = rec + (uint32_t)round * ROUND_SIZE;
    wr16(g_mem, r + 7u, (uint32_t)value);
    wr16(g_mem, r + 9u, (uint32_t)round);
    wr16(g_mem, r + 0xbu, 0);
    wr16(g_mem, r + 0xdu, flags);
    wr8(g_mem, r + 0x17u, 0xa);
    wr16(g_mem, r + 0x18u, (uint32_t)teams);
    wr16(g_mem, r + 0x1au, (uint32_t)teams / 2u);
    wr16(g_mem, r + 0x1cu, round == 0 ? (uint32_t)teams : 0u);
    wr16(g_mem, r + 0xfu, 0);
    wr16(g_mem, r + 0x1eu, 0);
    wr8(g_mem, r + 0x20u, 0);
    wr8(g_mem, r + 0x21u, 1);
    wr8(g_mem, r + 0x22u, 0);
    wr32(g_mem, r + 0x5cu, 0);
    wr32(g_mem, r + 0x60u, 0);
    wr32(g_mem, r + 0x64u, 0);
}

/* vtable +0x3c, thiscall (stage as a byte, short *rounds, short *code, int *is_league), ret 0x10 (the class's
 * own 0x57d190). A group (stage up to +0xff): six rounds on the matchdays of the edition, home and away (+0x3c
 * = 2); a group of three rests one team each round (create_fixtures rounds the count up, 0x6835e9, class doc
 * 3). The finals (+0x101): semi-finals and final, one match each with extra time and penalties (flags 3; 0x100
 * and 0x200 as the Euro's rounds: the match-day list and the fixed bracket, sweep-international section 7). */
static void round_template(void)
{
    uint32_t self = cpu.ecx, sp = cpu.esp;
    int stage = (int8_t)rd8(g_mem, sp + 4u);
    uint32_t out_rounds = rd32(g_mem, sp + 8u), out_code = rd32(g_mem, sp + 0xcu), out_league = rd32(g_mem, sp + 0x10u);
    NlConf *c = conf_of(self);
    const SeasonNlEdition *e = c ? edition(c, year_of(self)) : NULL;
    uint32_t rec = 0;
    int year = year_of(self);
    if (e && stage <= (int8_t)rd8(g_mem, self + F_LAST_GROUP)) {
        if (readable(out_league)) wr32(g_mem, out_league, 1);
        if (readable(out_rounds)) wr16(g_mem, out_rounds, 6);
        if (readable(out_code)) wr16(g_mem, out_code, (uint32_t)(0x3e9 + stage));
        rec = game_malloc(6u * LEAGUE_ROUND);
        for (int i = 0; rec && i < 6 && !rt_unwinding; i++) season_nl_league_round(rec, i, e->md[i], year);
    } else if (e && stage == (int8_t)rd8(g_mem, self + F_PLAYOFF)) {
        if (readable(out_league)) wr32(g_mem, out_league, 0);
        if (readable(out_rounds)) wr16(g_mem, out_rounds, 2);
        if (readable(out_code)) wr16(g_mem, out_code, 0);
        rec = game_malloc(2u * ROUND_SIZE);
        if (rec && !rt_unwinding) cup_round(rec, 0, e->sf, year, 4, 0x82, 0x303);
        if (rec && !rt_unwinding) cup_round(rec, 1, e->final, year, 2, 0x96, 0x203);
    } else {
        if (readable(out_rounds)) wr16(g_mem, out_rounds, 0);
    }
    if (rt_unwinding) return;
    rt_ret_stdcall(rec, 4);
}


/* ---- The vtable slots ---- */

/* The move to the next edition once the edition that the object holds is complete: its year and draw day.
 * The tables stay (the next draw reads them). The object then waits: its update does nothing before the draw
 * day (0x57fddf). A second call before the draw changes nothing. */
static void next_edition(uint32_t self)
{
    uint32_t st0 = stage_at(self, 0);
    int year = year_of(self);
    NlConf *c = conf_of(self);
    if (!c || !rd32(g_mem, self + F_DRAWN) || !readable(st0) || year_of(st0) != year) return;
    int next = next_year(c, year);
    const SeasonNlEdition *e = edition(c, next);
    if (!e) return;
    wr16(g_mem, self + F_YEAR, (uint32_t)next);
    set_draw_day(self, e);
    if (logging())
        fprintf(stderr, "recomp: season nl: the %s %d edition ends; the next is %s (%d), draw day %d of %d\n",
                c->confed, year, label(e), next, (int16_t)rd16(g_mem, self + F_DRAW_DAY), next + (int16_t)rd16(g_mem, self + F_DRAW_YEAR));
}

/* vtable +0x8, thiscall (): the season pass calls it on the update day of entry 1 (15 August) for a complete
 * competition whose vtable +0x2c gives 0 (0x5d5cab..0x5d5d1c; the copy takes the League base 0x48ce70 there,
 * the class's 0x57eb10 gives 1). Entry 1 changes its season only every second year (year step 2 at
 * 0x830fb6), so the update below also moves the object on the day after its finals end. */
static void start_new_season(void)
{
    uint32_t self = cpu.ecx;
    /* The hooks of the season change around it, as the wraps of every start_new_season do (season_starts.c):
     * the slot is a runtime function, so no wrap runs for it. The comps kind names the new season there. */
    int skip = season_change_start(self, 0);
    if (rt_unwinding) return;
    if (!skip && is_nl(self)) next_edition(self);
    if (rt_unwinding) return;
    season_change_start(self, 1);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

/* vtable +0x14, thiscall (Date *today, int full), ret 8. On the draw day of a new edition: the leagues from the
 * tables of the edition that ended, then the original start_new_season 0x57eb20 (it frees the tables and
 * stages, keeps the year, which is the game year, and calls set_up_rules), the stage array of the edition, and
 * the groups for the class's draw. Then the class's update 0x57fd60. */
static void update_for_date(void)
{
    uint32_t self = cpu.ecx, sp = cpu.esp, today = rd32(g_mem, sp + 4u), full = rd32(g_mem, sp + 8u);
    NlConf *conf = conf_of(self);
    if (conf && readable(today)) {
        int year = year_of(self);
        const SeasonNlEdition *e = edition(conf, year);
        int day = (int16_t)rd16(g_mem, today), ty = (int16_t)rd16(g_mem, today + 2u);
        int dday = (int16_t)rd16(g_mem, self + F_DRAW_DAY), dy = year + (int16_t)rd16(g_mem, self + F_DRAW_YEAR);
        if (e && full && day == dday && ty == dy) {
            SeasonNlEdition cur = *e;
            uint32_t st0 = stage_at(self, 0);
            int old = rd32(g_mem, self + F_DRAWN) && readable(st0) && year_of(st0) != year;
            if (old || !rd32(g_mem, self + F_DRAWN)) prepare_draw(self, &cur);
            if (rt_unwinding) return;
            if (old) {
                /* The year stays: the class steps it only while it lies before the game year (0x57eba3). */
                int16_t game_year = (int16_t)rd16(g_mem, GAME_YEAR);
                if (game_year > year) wr16(g_mem, GAME_YEAR, (uint32_t)year);
                game_free(rd32(g_mem, self + F_TEMPLATE));
                wr32(g_mem, self + F_TEMPLATE, 0);
                if (!rt_unwinding) call_this(QUAL_START, self, 0, NULL);
                if (game_year > year) wr16(g_mem, GAME_YEAR, (uint32_t)game_year);
                if (rt_unwinding) return;
                wr16(g_mem, self + F_YEAR, (uint32_t)year);
            }
            if (!rd32(g_mem, self + F_DRAWN)) {
                wr32(g_mem, self + F_FINISHED, 0);     /* else check_complete gives 1 at once (0x689b15) */
                make_stages(self, group_count(&cur));
                if (rt_unwinding) return;
                game_free(rd32(g_mem, self + F_TEMPLATE));
                wr32(g_mem, self + F_TEMPLATE, 0);
                if (rt_unwinding) return;
                call_this(QUAL_SET_UP_RULES, self, 0, NULL);
                if (rt_unwinding) return;
                wr16(g_mem, self + F_DRAW_DAY, (uint32_t)day);
                wr16(g_mem, self + F_DRAW_YEAR, (uint32_t)(ty - year));
                if (logging())
                    fprintf(stderr, "recomp: season nl: draw of %s (%d groups)\n", label(&cur), draw_groups_n);
            }
        }
    }
    uint32_t a[2] = {today, full};
    uint32_t r = call_this(QUAL_UPDATE, self, 2, a);
    if (rt_unwinding) return;
    /* All stages complete, the finals too: Comp::all_stages_complete (vtable +0xc, 0x48ce10) calls
     * check_complete (1, 1), which tests every stage up to +0x30 (0x689bb6..0x689be8). The flag +0xdd alone
     * is set when the League object's own table is done (0x689bea). */
    if (is_nl(self) && rd32(g_mem, self + F_FINISHED) && (int32_t)rd32(g_mem, self + F_STAGE) == held_groups(self) - 1 &&
        (vcall(self, 0xcu, 0, NULL) & 0xffu))
        next_edition(self);
    if (rt_unwinding) return;
    if (conf == &confs[0] && logging() && readable(today)) {
        season_nl_euro_log_slots((int16_t)rd16(g_mem, today), (int16_t)rd16(g_mem, today + 2u));
        if (rt_unwinding) return;
    }
    if (conf && logging() && readable(today)) {
        /* Test log: the state of the edition when it changes (stage, +0xdd of 0x689c06, the finals' complete
         * byte +0xb1 that Cup::check_complete 0x521e00 tests). */
        int groups = held_groups(self);
        uint32_t cup = groups >= 2 ? stage_at(self, groups - 1) : 0;
        uint32_t state = (rd32(g_mem, self + F_STAGE) & 0xff) | (rd32(g_mem, self + F_FINISHED) ? 0x100u : 0) |
                         (readable(cup) && rd8(g_mem, cup + 0xb1u) ? 0x200u : 0) | (readable(cup) ? 0x400u : 0);
        if (state != conf->log_state)
            fprintf(stderr, "recomp: season nl: %s day %d of %d: stage %d, finished %d, finals %s%s\n", conf->confed,
                    (int16_t)rd16(g_mem, today), (int16_t)rd16(g_mem, today + 2u), (int)(state & 0xff),
                    !!(state & 0x100), readable(cup) ? "made" : "none", (state & 0x200) ? " and finished" : "");
        conf->log_state = state;
    }
    rt_ret_stdcall(r, 2);
}

/* vtable +0x28, thiscall (): when every group is complete, the finals of four: the winners of the League A
 * groups, A1 v A4 and A2 v A3 in a fixed bracket (as the Euro's knockout, 0x57bb40). The real draw of the
 * semi-finals is not made (decision for Max). Nothing after the finals. */
static void advance_stage(void)
{
    uint32_t self = cpu.ecx;
    int groups = held_groups(self), cur = (int32_t)rd32(g_mem, self + F_STAGE);
    NlConf *c = conf_of(self);
    const SeasonNlEdition *e = c ? edition(c, year_of(self)) : NULL;
    if (e && cur < groups - 1 && groups >= 2 && e->groups[0] >= 2) {
        uint32_t w[4] = {0};
        int a_groups = e->groups[0] < 4 ? e->groups[0] : 4;
        for (int g = 0; g < a_groups; g++) {
            uint32_t st = stage_at(self, g - 1), z[1] = {0};
            if (!readable(st)) continue;
            call_this(TABLE_SORT, st, 1, z);
            if (rt_unwinding) return;
            uint32_t table = rd32(g_mem, st + F_TABLE);
            if (readable(table)) w[g] = rd32(g_mem, table);
        }
        uint32_t teams = game_malloc(16), out = game_malloc(8);
        if (rt_unwinding || !teams || !out) return;
        const uint32_t bracket[4] = {w[0], w[3], w[1], w[2]};
        for (int k = 0; k < 4; k++) wr32(g_mem, teams + 4u * (uint32_t)k, bracket[k]);
        uint32_t xa[4] = {(uint32_t)(groups - 1), out, out + 4u, 0};
        uint32_t tmpl = vcall(self, 0x3cu, 4, xa);
        if (rt_unwinding) return;
        uint32_t na[1] = {0xb2};
        uint32_t cup = call_cdecl(GAME_NEW, 1, na);
        if (rt_unwinding) return;
        if (readable(cup) && tmpl) {
            uint32_t ca[15] = {self, 4, teams, (uint32_t)(int16_t)rd16(g_mem, out), rd32(g_mem, self + F_RECORD), tmpl,
                               (uint32_t)year_of(self), (uint32_t)(groups - 1), 4,
                               (uint32_t)(int16_t)rd16(g_mem, out + 4u), 0x14, 1, 0, 0, 0};
            cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
            if (rt_unwinding) return;
        } else {
            cup = 0;
        }
        wr32(g_mem, rd32(g_mem, self + F_STAGES) + 4u * (uint32_t)(groups - 1), cup);
        game_free(teams);
        game_free(tmpl);
        game_free(out);
        if (rt_unwinding) return;
        if (cup) {
            uint32_t z[1] = {0};
            call_this(CUP_START, cup, 1, z);
            if (rt_unwinding) return;
            wr32(g_mem, self + F_STAGE, (uint32_t)(groups - 1));
        }
        if (logging() && cup) {
            /* Test log: the round records as the stage holds them (+0xa3, count +0x3c; class doc 12.1). */
            uint32_t rr = rd32(g_mem, cup + 0xa3u);
            int nr = (int16_t)rd16(g_mem, cup + 0x3cu);
            for (int k = 0; readable(rr) && k < nr && k < 4; k++) {
                uint32_t r = rr + (uint32_t)k * ROUND_SIZE;
                fprintf(stderr, "recomp: season nl: finals round %d: draw day %d +%d, match day %d +%d, id 0x%x, flags 0x%x, "
                        "teams %d, ties %d, entrants %d, legs %d\n", k, (int16_t)rd16(g_mem, r), (int8_t)rd8(g_mem, r + 2u),
                        (int16_t)rd16(g_mem, r + 3u), (int8_t)rd8(g_mem, r + 5u), rd16(g_mem, r + 7u), rd16(g_mem, r + 0xdu),
                        (int16_t)rd16(g_mem, r + 0x18u), (int16_t)rd16(g_mem, r + 0x1au), (int16_t)rd16(g_mem, r + 0x1cu),
                        rd8(g_mem, r + 0x21u));
            }
            fprintf(stderr, "recomp: season nl: finals stage: rounds %d, year %d, complete %d\n", nr,
                    (int16_t)rd16(g_mem, cup + 0x40u), rd8(g_mem, cup + 0xb1u));
        }
        if (logging()) {
            fprintf(stderr, "recomp: season nl: %s finals:", label(e));
            for (int k = 0; k < 4; k++)
                fprintf(stderr, " %s;", bracket[k] ? win_str(nation_at(season_nl_nation_of_club(bracket[k])) + NATION_NAME) : "none");
            fputc('\n', stderr);
        }
    }
    rt_ret_stdcall(0, 0);
}

/* vtable +0x30, thiscall () (the class's 0x90fbe0 takes no argument, ret at 0x90fbef): the history of the edition (the winner, runner-up and third), from the finals:
 * the Cup version takes the clubs with fates 1, 2 and 3 of its club list and writes them with 0x4afce0
 * (docs/research/2026-10-18-club-world-formats.md, slot 12 0x519a90). The Euro forwards to its knockout stage
 * the same way (0x40f590). With no finals, no history. */
static void history(void)
{
    uint32_t self = cpu.ecx;
    int groups = held_groups(self);
    uint32_t cup = groups >= 2 ? stage_at(self, groups - 1) : 0;
    if (is_nl(self) && readable(cup) && (int32_t)rd32(g_mem, self + F_STAGE) == groups - 1) {
        vcall(cup, 0x30u, 0, NULL);
        if (rt_unwinding) return;
    }
    call_this(QUAL_CLEAR, self, 0, NULL);       /* the class's own slot 12 (0x90fbe0, nation teams) */
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

static uint32_t build_vtable(const SeasonCompDef *d)
{
    uint32_t v = season_comps_vtable(d, QUAL_VTABLE, VTABLE_SLOTS);
    if (!v) return 0;
    /* The League base functions where the class feeds the European Championship (class doc 10), and slot 11
     * (0x48ce70 returns 0, so the season pass may start the object; the class's 0x57eb10 returns 1). */
    static const int base_slots[] = {11, 14, 17, 18, 23, 26, 28, 29, 30, 37};
    for (size_t i = 0; i < sizeof base_slots / sizeof base_slots[0]; i++)
        wr32(g_mem, v + 4u * (uint32_t)base_slots[i], rd32(g_mem, LEAGUE_VTABLE + 4u * (uint32_t)base_slots[i]));
    season_comps_set_slot(v, 2, "nl_start_new_season", start_new_season);
    season_comps_set_slot(v, 5, "nl_update_for_date", update_for_date);
    season_comps_set_slot(v, 10, "nl_advance_stage", advance_stage);
    season_comps_set_slot(v, 12, "nl_history", history);
    season_comps_set_slot(v, 15, "nl_round_template", round_template);
    return v;
}

/* The make function of the def (season_comps.h). New: the constructor with the edition year (an even year, so
 * the constructor's year, rounded to the Euro cycle from year - 1, is not below it and the 2002 history of
 * 0x57d01a does not run), then the vtable copy, the year, the stage array and the rules of the edition. Load:
 * the constructor reads the object from its file (0x57cf21); the stage bytes follow the saved stage count. */
/* 1 while nl_make runs the Euro qualifying constructor: its epilogue (0x57d03b) calls rt_euroq_rules while the
 * object still has the class's vtable, so the loaded places +0xbe and +0xbf of a Nations League would take the
 * Euro qualifying's (load sweep). */
static int in_ctor;
int season_nl_in_ctor(void) { return in_ctor; }

static uint32_t nl_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    NlConf *c = conf_of_def(d);
    const SeasonNlEdition *e = loading || !c ? NULL : season_nl_first(c->confed, year);
    if (!c || (!loading && !e)) return 0;
    uint32_t v = build_vtable(d);
    uint32_t na[1] = {QUAL_SIZE};
    uint32_t obj = v ? call_cdecl(GAME_NEW, 1, na) : 0;
    if (rt_unwinding || !readable(obj)) return 0;
    uint32_t ca[2] = {loading ? (uint32_t)year : (uint32_t)e->year, rec};
    in_ctor = 1;
    obj = call_this(QUAL_CTOR, obj, 2, ca);
    in_ctor = 0;
    if (rt_unwinding || !readable(obj)) return 0;
    wr32(g_mem, obj, v);
    if (loading) {
        if (held_groups(obj) >= 2) set_stage_bytes(obj, held_groups(obj));
        return obj;
    }
    wr16(g_mem, obj + F_YEAR, (uint32_t)e->year);
    make_stages(obj, group_count(e));
    if (rt_unwinding) return 0;
    set_up(obj, e);
    if (rt_unwinding) return 0;
    if (logging())
        fprintf(stderr, "recomp: season nl: %s (%d groups), draw day %d of %d\n", label(e), group_count(e),
                (int16_t)rd16(g_mem, obj + F_DRAW_DAY), e->year + (int16_t)rd16(g_mem, obj + F_DRAW_YEAR));
    return obj;
}

const SeasonCompDef season_nl_comp = {"UEFA Nations League", "new_uefa_nations_league", RECORD_NAME, ENTRY, nl_make};

/* Part b (Q368): the CONCACAF Nations League on a new record (season_comps.h, `like`): the continent, nation and
 * colour ids of the CONCACAF World Cup qualifying record. Level 14 (inference: below the qualifying of a finals,
 * which the officials code compares at 0x79d447; the World Cup has 20). */
const SeasonCompDef season_nl_concacaf_comp = {"CONCACAF Nations League", "new_concacaf_nations_league",
                                               "CONCACAF Nations League", ENTRY, nl_make,
                                               "World Cup CONCACAF Qualifying Section", "CONCACAF Nations League", 14};

/* ---- Wraps of the class's direct calls (class doc 2) ---- */

void o_0057f600(void);
void o_0057dab0(void);
void o_0057d9d0(void);
void o_0057f920(void);
void o_0057fa10(void);
void o_00669ae0(void);

/* collect_entrants, thiscall (), returns 1: the Nations League takes its nations at the draw. */
RT_WRAP(0057f600, nl_collect_entrants)
{
    if (!is_nl(cpu.ecx)) {
        uint32_t self = cpu.ecx;
        o_0057f600();
        if (!rt_unwinding) {
            uint32_t eax = cpu.eax;
            season_wcq26_log_pool(self, "euro qualifying (original)");      /* O188 test log */
            cpu.eax = eax;
        }
        return;
    }
    rt_ret_stdcall(1, 0);
}

/* draw_groups, thiscall (): the groups of prepare_draw into the draw grid +0xf7, six places a group. */
RT_WRAP(0057dab0, nl_draw_groups)
{
    uint32_t self = cpu.ecx;
    if (!is_nl(self)) {
        o_0057dab0();
        return;
    }
    uint32_t grid = game_malloc((uint32_t)(draw_groups_n > 0 ? draw_groups_n : 1) * GROUP_ROOM * 4u);
    if (rt_unwinding) return;
    for (int g = 0; grid && g < draw_groups_n; g++)
        for (int i = 0; i < draw_size[g]; i++)
            wr32(g_mem, grid + 4u * (uint32_t)(g * GROUP_ROOM + i), draw_team[g][i]);
    wr32(g_mem, self + F_GRID, grid);
    rt_ret_stdcall(0, 0);
}

/* get_group_team_count, thiscall (group index as a byte, from -1), ret 4, returns a short. */
RT_WRAP(0057d9d0, nl_group_size)
{
    uint32_t self = cpu.ecx;
    if (!is_nl(self)) {
        o_0057d9d0();
        return;
    }
    int g = (int8_t)rd8(g_mem, cpu.esp + 4u) + 1;
    rt_ret_stdcall(g >= 0 && g < draw_groups_n ? (uint32_t)draw_size[g] : 0u, 1);
}

/* create_runners_up_stage, thiscall (): none. The current stage is the last group (the original's sets
 * +0x30 = +0x100 at 0x57f9eb), so the League update calls advance_stage when every group is complete. */
RT_WRAP(0057f920, nl_runners_up)
{
    uint32_t self = cpu.ecx;
    if (!is_nl(self)) {
        o_0057f920();
        return;
    }
    wr32(g_mem, self + F_STAGE, (uint32_t)(held_groups(self) - 2));
    rt_ret_stdcall(0, 0);
}

/* update_runners_up_table, thiscall (): none. */
RT_WRAP(0057fa10, nl_runners_up_table)
{
    if (!is_nl(cpu.ecx)) {
        o_0057fa10();
        return;
    }
    rt_ret_stdcall(0, 0);
}

/* The group stages of the draw: `cmp edi, 8` at 0x57ff34 (seasons.py) makes stage[0..7]. */
uint32_t rt_nl_group_stages(uint32_t self, uint32_t original)
{
    return is_nl(self) ? (uint32_t)(held_groups(self) - 1) : original;
}

int season_nl_international(uint32_t rec)
{
    for (int i = 0; i < CONFS; i++) {
        uint32_t o = season_comps_object(confs[i].def);
        if (o && rec == rd32(g_mem, o + F_RECORD)) return 1;
    }
    return season_gold_cup_record(rec);
}

/* 0x669ae0, cdecl (record), returns 1 for the records of the list of international competitions (nations doc
 * 2.1): their fixtures are of type 1, so the squads are called up, the players are released, the friendlies
 * keep off them and the FIFA ranking counts them as the Euro qualifiers do (nations doc 1, point 4). */
RT_WRAP(00669ae0, nl_international)
{
    uint32_t rec = rd32(g_mem, cpu.esp + 4u);
    if (season_comps_on() && rec && season_nl_international(rec)) {
        rt_ret_cdecl(1);
        return;
    }
    o_00669ae0();
}
