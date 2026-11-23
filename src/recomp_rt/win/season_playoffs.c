/* Promotion play-offs in leagues whose class has none (0.6.0 slice 19; spec Q247, Q272; research and design in
 * docs/research/2026-10-18-playoffs-splits-calendar.md).
 *
 * Which leagues. A league row of the rules table whose play-off text gives four places in a row just below the
 * direct places ("3rd to 6th", tools/seasons/encode_leagues.py, season_league_table.c), of a League class with
 * no play-off code of its own: advance_stage (vtable +0x28) is the empty function 0x5a8f60 and the result
 * handler (+0x44) is the base League one 0x686940. Examples: the Segunda from 2010/11, the Turkish second tier
 * from 2010/11, each only when the division above relegates the direct places and one more (see balanced).
 * So not the Conference (League Two relegates 1) and not the Greek second tier (GrePrm relegates 2). The Argentine
 * second tier has no set_up_rules wrap of slice 18, so it gets no play-off. J2 has one (0x665140). A class with its own
 * builder (England, Serie B, Norway, Ireland and the others of the doc) keeps the original's play-off.
 *
 * How. After each set_up_rules of such a league (vtable +0x8c: the constructor and start_new_season call it;
 * season_sizes_rules of slice 18 calls season_playoffs_rules at its end), the direct places +0xbe become the
 * places above the play-off, the play-off places +0xbf 4, and the stage count +0x2c 1 with no stage yet (+0x30
 * -1). The row is that of the start season at a new game and with changing rules off (Q232), else that of the
 * league's edition, as slice 18 reads it. League::update_for_date 0x6847c0 then calls advance_stage on the day
 * the table is complete (0x6849d1..0x6849e3: check_complete(0, 0) first), and the wrap of 0x5a8f60 (season_uefa_cup.c calls
 * season_playoffs_advance) builds the stage as EngFirst::create_playoff_stage 0x573080 does: the round table of
 * EngFirst's +0x3c 0x571980 (stage 0: two-leg semi-finals and a one-match final, codes 0x82 and 0x96; it reads
 * only the year +0x40), new dates from the day after the table is complete, the clubs 6th v 3rd and 5th v 4th
 * (fixed pairs, flag 0x200), Cup_Stage::Cup_Stage 0x522e00 and Cup::set_up_round 0x51c800. The season of the
 * league closes when the stage is complete (0x6849aa..0x6849cf, check_complete 0x689bbc..0x689be8). The results
 * of the stage reach the league's +0x44 with a stage id (Comp 0x519690, 0x5196f2..0x51970c): the wrap of
 * 0x686940 hands them to the English handler 0x576c50, which gives the final's winner status 0 (0x576d9e), so
 * the promotion chain moves it up (league_promote_and_relegate 0x689c80, status 0 or 5). It reads only base
 * League fields (+4, +0x3e, +0xb1).
 *
 * Save and load. The base League save 0x6891c0 and load 0x687b10 write and read +0x2c, +0x30 and each stage
 * (Cup_Stage type 1: 0x523eb0 and 0x5230e0, 0x523380), for every class of this kind. The rules: stages[0] is a
 * live object whenever +0x30 >= 0 (0x689918), and +0x2c >= 1 then (0x688e49). The kind sets +0x30 only after
 * the stage exists.
 *
 * The time. The play-off takes 12 days (semi-finals on days 3 and 7, the final on day 11 after the table is
 * complete). A league whose season change (its league table entry's update day, +0x15) comes sooner has no
 * play-off that season: the first play-off place goes up directly, the nearest form (Q247).
 *
 * The places +0xbe..+0xc1 are in the save (0x689589..0x689592), so a loaded game keeps the play-off.
 *
 * The base game and RECOMP_SEASON_CODE=off: the rule kinds do not run (season_change.c), so no league gets a
 * stage count, and both wraps pass to the original (a league with +0x2c 0 is never marked). The test switch
 * RECOMP_PLAYOFFS=off turns the kind off. */
#include "season_sizes_arg.h"
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_cro_gre_places.h"
#include "season_league.h"
#include "season_names.h"
#include "season_rules.h"
#include "season_sizes.h"
#include "season_playoffs.h"
#include "season_ned_playoff.h"
#include "season_pol_playoff.h"
#include "season_cup_third_place.h"
#include "season_second_teams.h"
#include "season_ofc_first.h"
#include "season_bel_playoffs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F_EMPTY 0x005a8f60u             /* the empty function: advance_stage of the classes with no stages */
#define F_BASE_RESULT 0x00686940u       /* League +0x44, base: news of champions, promotion, relegation */
#define F_ENG_RESULT 0x00576c50u        /* EngFirst +0x44: a stage result; code 1 gives status 0 (0x576d9e) */
#define F_ENG_ROUNDS 0x00571980u        /* EngFirst +0x3c (stage, &rounds, &name, flag), thiscall */
#define F_CUP_STAGE 0x00522e00u         /* Cup_Stage::Cup_Stage, thiscall, 15 arguments */
#define F_SET_UP_ROUND 0x0051c800u      /* Cup::set_up_round, thiscall (round) */
#define F_DRAW_DATE 0x00521e60u         /* cdecl (table, round, day, month, year+, weekday, year) */
#define F_MATCH_DATE 0x00521eb0u        /* cdecl (table, round, day, month, year+, weekday, a7, year, venue) */
#define F_MALLOC 0x00944e46u            /* cdecl (size) */
#define F_FREE 0x009452cau              /* cdecl (block) */
#define F_LINKED 0x00540a50u            /* club_get_linked_b_team, cdecl (club, &out, require_in_league) */
#define CLUB_DIVISION 0x57u             /* the competition record of a club's division (the club lines of the report) */
#define F_NEW 0x00944cf1u               /* operator new, cdecl (size) */

#define GAME_DATE 0x00ae2c90u           /* s16 day of the year (0 = 1 January), +2 s16 year */
#define ENTRIES 0x00b63d60u             /* the league table entries, 35 x 0x48 */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35
#define ENTRY_UPDATE_DAY 0x15u          /* s16 day of the year of the season change (compared at 0x5d5bc8) */
#define LATE_DAY 273                    /* 1 October: an update day from here is a calendar-year change */
#define WAIT_DAYS 60                    /* the longest wait of a season change for a play-off */

#define COMP_RECORD 4u
#define COMP_STAGES 0xcu                /* the stage array */
#define COMP_STAGE_COUNT 0x2cu          /* s32 */
#define COMP_STAGE_NOW 0x30u            /* s32, -1 before the first stage */
#define COMP_CLUBS 0x3eu
#define COMP_YEAR 0x40u
#define COMP_TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define UP 0xbeu
#define UP_PLAYOFF 0xbfu
#define DOWN_PLAYOFF 0xc0u
#define DOWN 0xc1u

#define COMPS_MAX 256                   /* the most competitions of an entry that season_change.c reads (its own cap) */
#define PLAYOFF_CLUBS 4
#define SIX_CLUBS 6                     /* the Conference from 2017/18: 2nd to 7th (six_form) */
#define ROUND_SIZE 0x68u
#define PLAYOFF_DAYS 12                 /* the last match on day 11 after the table is complete */
#define FINAL_LEG_DAYS 3                /* a two-leg final: the second leg 3 days after the first (0x425f99) */

/* The days between the two legs of the Segunda's final in the season that starts in `year`. The real gap was 3 days
 * in 2010/11 to 2019/20 and longer from 2020/21 (final legs 13 and 20 June 2021, 11 and 19 June 2022, 11 and 17 June
 * 2023, 16 and 23 June 2024, 15 and 21 June 2025, 14 and 20 June 2026: 7, 8, 6, 7, 6, 6 days; each with es.wikipedia
 * and a news page, lead-notes/research/segunda-final-legs-2020-2025.md). Later seasons keep the last gap. */
static int final_gap(int year)
{
    static const int gap[] = {7, 8, 6, 7, 6, 6};              /* 2020/21 to 2025/26 */
    if (year < 2020) return FINAL_LEG_DAYS;
    return gap[year >= 2025 ? 5 : year - 2020];
}
#define NO_WEEKDAY 0xffffffffu          /* Date::Date 0x549f70 keeps the date for weekday -1 (src/si/date.c) */

static int readable(uint32_t p) { return rt_readable(p); }

static int playoffs_on(void)
{
    static int off = -1;
    if (off < 0) off = getenv("RECOMP_PLAYOFFS") && !strcmp(getenv("RECOMP_PLAYOFFS"), "off");
    return !off && season_game_changes();
}

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

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    if (!rt_unwinding && readable(p)) memset(g_mem + p, 0, size);
    return rt_unwinding ? 0 : p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (readable(p) && !rt_unwinding) call_cdecl(F_FREE, 1, a);
}

/* A League object of a class with no play-off code of its own. */
static int plain_league(uint32_t obj)
{
    if (!readable(obj)) return 0;
    uint32_t vt = rd32(g_mem, obj);
    return readable(vt) && rd32(g_mem, vt + 0x28) == F_EMPTY && rd32(g_mem, vt + 0x44) == F_BASE_RESULT &&
           season_change_file(obj)[0] != '?';
}

static int cross_marked(uint32_t obj);

/* A league that this kind gave a play-off: a plain league with one stage and 4 play-off places, or the lower
 * division of a Scottish play-off between two divisions (cross_marked, below). */
static int marked(uint32_t obj)
{
    int places = (int8_t)g_mem[obj + UP_PLAYOFF];
    return (plain_league(obj) && (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) == 1 &&
            (places == PLAYOFF_CLUBS || places == SIX_CLUBS)) || cross_marked(obj);
}

/* The encoded row of a file and season (season_league_table.c), or NULL. */
static const SeasonLeagueRule *league_rule(const char *file, int season)
{
    return season_league_rule(file, season);
}

/* The row of the rules of a season, or the last before it with no end year (the after-table rule, Q248), as
 * season_sizes.c reads it. */
static const SeasonRule *row_of_season(const char *file, int season)
{
    return season_rules_row_of(file, season);
}

/* 1 when the row's play-off is the National League's of 2017/18 on (item 273): 2nd to 7th, quarter-finals 4th v 7th
 * and 5th v 6th, semi-finals at the grounds of the 2nd and the 3rd, a final; all single matches (rows eng-192 to
 * eng-292, "2nd-7th: 4v7 and 5v6 single-leg quarter-finals, single-leg semi-finals hosted by 2nd and 3rd, final").
 * The other rows with 6 places (EngFirst, Eredivisie, Serie B, Segunda B) are classes with their own play-off. */
static int six_form(const SeasonRule *r, const SeasonLeagueRule *e)
{
    return e->po_up == SIX_CLUBS && r->playoffs_text && strstr(r->playoffs_text, "4v7 and 5v6") != NULL;
}

/* The clubs of the play-off of a row that this kind plays (4, or 6 for six_form), else 0: places in a row, directly
 * below the direct places of the row. The up column counts the play-off winner in most parts ("2 direct + 1 via
 * play-offs": the first play-off place is up) and only the direct places in the English part (the Conference 2002/03:
 * up 1, places 2 to 5). *direct gets the direct places. season_playoffs_shape gives 1 for such a row. */
static int shape_clubs(const char *file, int season, int *direct)
{
    const SeasonRule *r = row_of_season(file, season);
    const SeasonLeagueRule *e = !r ? NULL : season_rules_moved(file) ? season_league_rule_of(file, r)
                                                                      : league_rule(file, r->season);
    if (!e || r->up < 0) return 0;
    int clubs = e->po_up == PLAYOFF_CLUBS ? PLAYOFF_CLUBS : six_form(r, e) ? SIX_CLUBS : 0;
    if (!clubs) return 0;
    /* Two ties whose winners both go up (Série B from 2025) are not one play-off for one place. The test is wider than
     * season_playoffs_winners_up ("the two winners go up"): the rows of Belgium and Portugal 2008/09 say "two
     * winners" too, and are no play-off of this kind either. */
    if (r->playoffs_text && strstr(r->playoffs_text, "two winners")) return 0;
    int d;
    if (e->po_up_first == r->up && r->up >= 1) d = r->up - 1;
    else if (e->po_up_first == r->up + 1) d = r->up;
    else return 0;
    if (r->clubs > 0 && e->po_up_first + clubs - 1 > r->clubs) return 0;
    if (direct) *direct = d;
    return clubs;
}

int season_playoffs_shape(const char *file, int season, int *direct)
{
    return shape_clubs(file, season, direct) != 0;
}

/* ---- Dates ---- */

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int year_days(int y) { return leap(y) ? 366 : 365; }

/* Day of the year (0 = 1 January) to day (1..31) and month (0..11). */
static void day_month(int doy, int year, int *day, int *month)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 0;
    while (m < 11) {
        int n = len[m] + (m == 1 && leap(year));
        if (doy < n) break;
        doy -= n;
        m++;
    }
    *day = doy + 1;
    *month = m;
}

/* Today plus n days: day of the year and year. */
static void date_plus(int n, int *doy, int *year)
{
    int d = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2);
    d += n;
    while (d >= year_days(y)) d -= year_days(y), y++;
    *doy = d;
    *year = y;
}

/* The days from today to the update day of the league's table entry (0 to 365), or -1 when not found. */
static int days_to_change(uint32_t obj)
{
    uint32_t buf[COMPS_MAX];
    for (int e = 0; e < ENTRY_COUNT; e++) {
        int n = season_change_entry_comps(e, buf, COMPS_MAX);
        for (int i = 0; i < n; i++) {
            if (buf[i] != obj) continue;
            int upd = (int16_t)rd16(g_mem, ENTRIES + (uint32_t)e * ENTRY_SIZE + ENTRY_UPDATE_DAY);
            int today = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2);
            int left = upd - today;
            if (left < 0) left += year_days(y);
            return left;
        }
    }
    return -1;
}

/* 1 when the league's table entry has its update day from LATE_DAY on (a calendar-year change). */
static int change_is_late(uint32_t obj)
{
    uint32_t buf[COMPS_MAX];
    for (int e = 0; e < ENTRY_COUNT; e++) {
        int n = season_change_entry_comps(e, buf, COMPS_MAX);
        for (int i = 0; i < n; i++)
            if (buf[i] == obj)
                return (int16_t)rd16(g_mem, ENTRIES + (uint32_t)e * ENTRY_SIZE + ENTRY_UPDATE_DAY) >= LATE_DAY;
    }
    return 0;
}

static void set_dates(uint32_t table, int round, int draw_in, int match_in, uint32_t a7, uint32_t venue,
                      uint32_t year)
{
    int doy, y, day, month;
    date_plus(draw_in, &doy, &y);
    day_month(doy, y, &day, &month);
    uint32_t d[7] = {table, (uint32_t)round, (uint32_t)day, (uint32_t)month, (uint32_t)(y - (int)year), NO_WEEKDAY,
                     year};
    call_cdecl(F_DRAW_DATE, 7, d);
    if (rt_unwinding) return;
    date_plus(match_in, &doy, &y);
    day_month(doy, y, &day, &month);
    uint32_t m[9] = {table, (uint32_t)round, (uint32_t)day, (uint32_t)month, (uint32_t)(y - (int)year), NO_WEEKDAY,
                     a7, year, venue};
    call_cdecl(F_MATCH_DATE, 9, m);
}

void season_playoffs_round_dates(uint32_t table, int round, int draw_in, int match_in, uint32_t a7, uint32_t venue,
                                 uint32_t year)
{
    set_dates(table, round, draw_in, match_in, a7, venue, year);
}

/* ---- The stage ---- */

static uint32_t row_club(uint32_t obj, int index)
{
    uint32_t table = rd32(g_mem, obj + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    if (!readable(table) || index < 0 || index >= n) return 0;
    return rd32(g_mem, table + (uint32_t)index * ROW_SIZE);
}

/* No time for the play-off: the first play-off place goes up as the winner would (status 0, as 0x576d9e). */
static void promote_first(uint32_t obj, int first)
{
    uint32_t table = rd32(g_mem, obj + COMP_TABLE);
    if (readable(table)) g_mem[table + (uint32_t)first * ROW_SIZE + ROW_STATUS] = 0;
    wr32(g_mem, obj + COMP_STAGE_COUNT, 0);          /* the season closes as the original's (0x6849d1) */
}

static uint32_t league_above(uint32_t obj);
static int season_now(uint32_t obj, const char *file);

/* 1 when a club's paired side plays in the division above: a reserve side may not go up into the division of its
 * senior club (the rows' play-off text: "reserve teams excluded, next club moves up"). club_get_linked_b_team
 * 0x540a50 gives the paired club of a Spanish, German or Portuguese club from fixed key id pairs (the nation test
 * at 0x540af6..0x540b10, the pairs from 0x540b1b and 0x541871, *out 1 at 0x54187c), or 0. Which side is the
 * reserve one does not matter: a senior club's reserve side never plays in the division above it. A club with no
 * pair asks the reserve-side rules of the nations (season_second_barred, items 111 to 122 and 232 to 244): 1 when
 * the rule of its nation bars it from the division above (for example a Polish reserve side in the I liga play-off
 * of issue 92, item 243). */
static int barred(uint32_t obj, uint32_t club)
{
    uint32_t above = league_above(obj);
    if (!above || !readable(club)) return 0;
    uint32_t out = game_malloc(4);
    if (!out) return 0;
    uint32_t a[3] = {club, out, 0};
    uint32_t pair = call_cdecl(F_LINKED, 3, a);
    game_free(out);
    if (rt_unwinding) return 0;
    if (!readable(pair)) return season_second_barred(club, rd32(g_mem, above + COMP_RECORD));
    uint32_t div = rd32(g_mem, pair + CLUB_DIVISION);
    return readable(div) && div == rd32(g_mem, above + COMP_RECORD);
}

/* The rows of the play-off: the `want` rows from the first play-off place on that no reserve side holds. Gives the
 * count found (fewer only at the foot of a short table). */
static int playoff_rows(uint32_t obj, int first, int rows[], int want)
{
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS), k = 0;
    for (int i = first; i < n && k < want && !rt_unwinding; i++) {
        if (barred(obj, row_club(obj, i))) {
            fprintf(stderr, "recomp: playoffs: %s: place %d is a reserve side of a club above, the next place plays\n",
                    season_change_file(obj), i + 1);
            continue;
        }
        rows[k++] = i;
    }
    return k;
}

/* 1 when the final of the row is two legs at the clubs' grounds ("two-legged final": the Segunda from 2010/11).
 * Else it is one match at a neutral ground, as EngFirst's final (venue flag 4, 0x572b49..0x572b5b; outside
 * England Stadium::set_fixture_venue 0x85c260 gives a neutral venue, 0x85c5a7: the Turkish final). */
static int two_leg_final(uint32_t obj, const char *file)
{
    const SeasonRule *r = row_of_season(file, season_now(obj, file));
    return r && r->playoffs_text && strstr(r->playoffs_text, "two-legged final");
}

/* RECOMP_TEST_PLAYOFF_LEVEL=1 (tests only): the two legs of the two-leg final are processed as a level tie. Leg 1 is
 * processed as 1-1. Leg 2 as 0-0 (level on aggregate, away goals off), and when its match flags ask for extra time
 * and penalties (bits 0x3, 0x51f696 and 0x51c1c9) the result block gets what the engine would give: 0-0 in extra
 * time and penalties 5-4. Without those flags nothing decides the tie, as in the old fault (Cup.cpp 1450). The
 * result block of the fixture is +0x43..+0x4c: goals at 90 minutes +0, +1, extra time +4, +5, penalties +6, +7
 * (copied into the tie at 0x14.., 0x51a2c0..; the tie code reads 0x14/0x15, 0x18/0x19, 0x1a/0x1b at 0x51f2f0). */
static uint32_t test_stage;
static int test_final_seen;

void o_0051a150(void);
RT_WRAP(0051a150, playoff_level_test)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_TEST_PLAYOFF_LEVEL") && *getenv("RECOMP_TEST_PLAYOFF_LEVEL") == '1';
    uint32_t fx = rd32(g_mem, cpu.esp + 4);
    if (on && test_stage && cpu.ecx == test_stage && fx && (int16_t)rd16(g_mem, fx + 0x34) == 1) {
        uint32_t b = fx + 0x43;
        uint16_t flags = rd16(g_mem, fx + 0x38);
        int leg = ++test_final_seen;
        for (int i = 0; i < 10; i++) g_mem[b + (uint32_t)i] = 0;
        if (leg == 1) {
            g_mem[b] = 1;
            g_mem[b + 1] = 1;
        } else if ((flags & 3u) == 3u) {
            g_mem[b + 6] = 5;
            g_mem[b + 7] = 4;
        }
        fprintf(stderr, "recomp: playoffs: test: final leg %d forced %s, match flags %04x%s\n", leg,
                leg == 1 ? "1-1" : "0-0 level on aggregate", flags,
                leg == 1 ? "" : (flags & 3u) == 3u ? ", extra time 0-0 and penalties 5-4 given"
                                                    : ", no extra time or penalties asked for");
    }
    season_ofc_first_test_fixture(cpu.ecx, fx);  /* item 245, tests only */
    ThirdPlaceTies third;                       /* open item 73: season_cup_third_place.h */
    season_cup_third_place_before(cpu.ecx, &third);
    o_0051a150();
    if (!rt_unwinding) season_cup_third_place_after(&third);
}

/* RECOMP_TEST_PLAYOFF_FIXTURES=1 (tests only), called from the add_fixture wrap (season_kind_cup_breaks.c): the first
 * match day of each season of the First Division, the Second Division and the Copa del Rey of Spain, to compare a
 * season whose change waited for the play-off with one that did not (the fixture's competition record +0x14, its
 * year +0x28 and day +0x2a). A line each time an earlier day arrives for a competition and season. */
void season_playoffs_fixture_log(uint32_t f)
{
    static int on = -1;
    static struct { uint32_t rec; int key, when; } seen[96];
    static int n;
    if (on < 0) on = getenv("RECOMP_TEST_PLAYOFF_FIXTURES") && *getenv("RECOMP_TEST_PLAYOFF_FIXTURES") == '1';
    if (!on || !readable(f)) return;
    uint32_t rec = rd32(g_mem, f + 0x14);
    if (!readable(rec)) return;
    const char *name = (const char *)g_mem + rec + 4;
    if (strncmp(name, "Primera", 7) && strncmp(name, "Segunda Divisi", 14) && strncmp(name, "Copa del Rey", 12) &&
        strncmp(name, "Spanish", 7))
        return;
    int y = (int16_t)rd16(g_mem, f + 0x28), d = (int16_t)rd16(g_mem, f + 0x2a);
    if (y < 1990 || y > SEASON_YEAR_MAX || d < 0 || d > 366) return;
    int key = d >= 180 ? y : y - 1, when = y * 400 + d, i;
    for (i = 0; i < n; i++)
        if (seen[i].rec == rec && seen[i].key == key) break;
    if (i == n) {
        if (n == 96) return;
        seen[n].rec = rec; seen[n].key = key; seen[n].when = 1 << 30; n++;
    }
    if (when >= seen[i].when) return;
    seen[i].when = when;
    fprintf(stderr, "recomp: fixtures: %.30s season %d: first match day %d of %d\n", name, key, d, y);
}

/* The ties of the play-off of six, before Cup::draw_round 0x51c410 builds a round's fixtures (the wrap in
 * season_new_cups.c; item 273). The rounds have the fixed pairs bit (+0x0d 0x200), so the draw 0x51e220 does not run
 * (0x51c5e4..0x51c5f1) and the round plays the ties as they are here. The stage does not keep the list order (read
 * from built stages: the home and away slots of a quarter-final, and the order of the 2nd and 3rd, change from game
 * to game), and it fills the semi-finals in tie order: tie 1 the two quarter-final winners, tie 2 the 2nd and the 3rd.
 * So the ties are set here by table place. Quarter-finals: the higher place at home. Semi-finals: the 2nd at home to
 * the winner of 5th v 6th, the 3rd at home to the winner of 4th v 7th. A side of a tie (0x35 bytes, the round's ties at
 * stage +0xad) is its list slot (+0 home, +2 away; the list +0xa7, 6 bytes a slot, the club at +0) and the tie and
 * round it came from (home +0x0a and +0x0e, away +0x0c and +0x10; Cup::set_home_teams_for_round 0x51e870 swaps these
 * with the slots, 0x51e93a..0x51e978). The stage is known by its round table (+0xa3, saved with it: 3 rounds, round 2
 * with 2 new clubs from list slot 4, build_six) and its league by its record (+4; the record's id indexes the object
 * table [0xadadfc], as 0x51c936 reads it). */
typedef struct { int16_t slot, tie, round; } SixSide;

static void six_side_get(uint32_t tie, int side, SixSide *v)
{
    v->slot = (int16_t)rd16(g_mem, tie + 2u * (uint32_t)side);
    v->tie = (int16_t)rd16(g_mem, tie + 0x0au + 2u * (uint32_t)side);
    v->round = (int16_t)rd16(g_mem, tie + 0x0eu + 2u * (uint32_t)side);
}

static void six_side_set(uint32_t tie, int side, const SixSide *v)
{
    wr16(g_mem, tie + 2u * (uint32_t)side, (uint16_t)v->slot);
    wr16(g_mem, tie + 0x0au + 2u * (uint32_t)side, (uint16_t)v->tie);
    wr16(g_mem, tie + 0x0eu + 2u * (uint32_t)side, (uint16_t)v->round);
}

static int place_of(uint32_t obj, uint32_t club);

/* The table place of a slot's club (1 for the top), 0 when not found. */
static int six_place(uint32_t cup, uint32_t league, int16_t slot)
{
    uint32_t list = rd32(g_mem, cup + 0xa7);
    if (slot < 0 || slot > 5 || !readable(list)) return 0;
    return place_of(league, rd32(g_mem, list + 6u * (uint32_t)slot));
}

static int over_marked(uint32_t obj);
static int over_last_at_upper(uint32_t league);

/* The play-off over a play-off of four (build_over): the higher place at home in the semi-finals, and the upper
 * division's club, the side that came from no tie, at home in a last tie of one match at the clubs' grounds. */
static void over_draw_round(uint32_t cup, uint32_t league, uint32_t arr, int round)
{
    uint32_t t0 = rd32(g_mem, arr + 4u * (uint32_t)round);
    if (!readable(t0)) return;
    if (round == 2) {
        SixSide a, b;
        if (g_mem[t0 + 0x34] || !over_last_at_upper(league)) return;
        six_side_get(t0, 0, &a), six_side_get(t0, 1, &b);
        if (a.tie >= 0 && b.tie < 0) six_side_set(t0, 0, &b), six_side_set(t0, 1, &a);
        return;
    }
    for (uint32_t t = 0; t < 2; t++) {
        uint32_t tie = t0 + t * 0x35u;
        SixSide a, b;
        if (g_mem[tie + 0x34]) return;           /* a round already built */
        six_side_get(tie, 0, &a), six_side_get(tie, 1, &b);
        int pa = six_place(cup, league, a.slot), pb = six_place(cup, league, b.slot);
        if (pa && pb && pa > pb) six_side_set(tie, 0, &b), six_side_set(tie, 1, &a);
    }
}

void season_playoffs_draw_round(uint32_t cup, int round)
{
    if (!playoffs_on() || round < 0 || round > 2 || !readable(cup) || (int16_t)rd16(g_mem, cup + 0x3c) != 3) return;
    uint32_t rounds = rd32(g_mem, cup + 0xa3), arr = rd32(g_mem, cup + 0xad), rec = rd32(g_mem, cup + COMP_RECORD);
    uint32_t objects = rd32(g_mem, 0x00adadfcu);
    if (!readable(rounds) || !readable(arr) || !readable(rec) || !readable(objects)) return;
    int32_t over_id = (int32_t)rd32(g_mem, rec), over_slots = (int32_t)rd32(g_mem, 0x00adadf8u);
    uint32_t over = over_id >= 0 && over_id < over_slots ? rd32(g_mem, objects + 4u * (uint32_t)over_id) : 0;
    if (readable(over) && over_marked(over) && rd16(g_mem, rounds + ROUND_SIZE + 0x1c) == 0 &&
        rd16(g_mem, rounds + 2 * ROUND_SIZE + 0x1c) == 1) {
        if (round != 1) over_draw_round(cup, over, arr, round);
        return;
    }
    if (round == 2) return;
    uint32_t r1 = rounds + ROUND_SIZE;
    if (rd16(g_mem, r1 + 0x07) != 130 || rd16(g_mem, r1 + 0x1c) != 2 || rd16(g_mem, r1 + 0x1e) != 4 ||
        rd16(g_mem, rounds + 0x1c) != 4 || !(rd16(g_mem, rounds + ROUND_SIZE * (uint32_t)round + 0x0d) & 0x200u))
        return;
    int32_t id = (int32_t)rd32(g_mem, rec), slots = (int32_t)rd32(g_mem, 0x00adadf8u);
    uint32_t league = id >= 0 && id < slots ? rd32(g_mem, objects + 4u * (uint32_t)id) : 0;
    if (!readable(league) || !marked(league) || (int8_t)g_mem[league + UP_PLAYOFF] != SIX_CLUBS) return;
    uint32_t t0 = rd32(g_mem, arr + 4u * (uint32_t)round), t1 = t0 + 0x35u;
    if (!readable(t0) || g_mem[t0 + 0x34] || g_mem[t1 + 0x34]) return;      /* a round already built */
    SixSide s[4];
    six_side_get(t0, 0, &s[0]), six_side_get(t0, 1, &s[1]), six_side_get(t1, 0, &s[2]), six_side_get(t1, 1, &s[3]);
    int p[4];
    for (int k = 0; k < 4; k++) {
        p[k] = six_place(cup, league, s[k].slot);
        if (!p[k]) return;                       /* a winner to come, or a club not in the table */
    }
    if (round == 0) {
        for (int t = 0; t < 2; t++)
            if (p[2 * t] > p[2 * t + 1]) {
                SixSide x = s[2 * t];
                s[2 * t] = s[2 * t + 1], s[2 * t + 1] = x;
            }
    } else {
        /* The new clubs are the sides that came from no tie; the higher of them is the 2nd. */
        int fresh[2], won[2], nf = 0, nw = 0;
        for (int k = 0; k < 4; k++) {
            if (s[k].tie < 0 && nf < 2) fresh[nf++] = k;
            else if (s[k].tie >= 0 && nw < 2) won[nw++] = k;
        }
        if (nf != 2 || nw != 2) return;
        int second = p[fresh[0]] < p[fresh[1]] ? fresh[0] : fresh[1], third = fresh[0] + fresh[1] - second;
        /* The winner of 5th v 6th comes from quarter-final 2 (tie index 1: list 2 v 3, build_six), whatever places
         * a reserve side skipped (playoff_rows). */
        int mid = s[won[0]].tie == 1 ? won[0] : won[1], out = won[0] + won[1] - mid;
        SixSide n[4] = {s[second], s[mid], s[third], s[out]};
        memcpy(s, n, sizeof s);
    }
    six_side_set(t0, 0, &s[0]), six_side_set(t0, 1, &s[1]), six_side_set(t1, 0, &s[2]), six_side_set(t1, 1, &s[3]);
}

/* The play-off of six (six_form, item 273): three rounds of the stage, all single matches. The rounds start from
 * EngFirst's final (one match, its flags 3 in +0x0d: extra time and penalties, 0x572b78), as the cross play-off starts
 * from its semi-final. Quarter-finals: 4 new clubs, 2 ties, fixed pairs (+0x0d bit 0x200: Cup::set_up_round's tie
 * builder 0x51c410 skips the draw 0x51e220 at 0x51c5e4..0x51c5f1), at the clubs' grounds (venue 0). Semi-finals: 2
 * new clubs (the 2nd and the 3rd), fixed pairs. The final: 2 clubs, 1 tie, no fixed pairs, neutral venue 4 (as
 * build_stage). Days 3, 7 and 11 after the table, as the play-off of four (PLAYOFF_DAYS). Returns 0 when it did
 * not build (the caller then sends the first play-off place up). */
static int build_six(uint32_t obj, int rows[SIX_CLUBS])
{
    const char *file = season_change_file(obj);
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, obj + COMP_YEAR);
    uint32_t outs = game_malloc(8);
    if (!outs) return 0;
    uint32_t ra[4] = {0, outs, outs + 4, 0};
    uint32_t eng = call_this(F_ENG_ROUNDS, obj, 4, ra);
    int eng_rounds = (int16_t)rd16(g_mem, outs);
    game_free(outs);
    if (rt_unwinding) return 0;
    if (!readable(eng) || eng_rounds != 2) {
        game_free(eng);
        return 0;
    }
    uint32_t table = game_malloc(3 * ROUND_SIZE);
    if (!table) {
        game_free(eng);
        return 0;
    }
    static const uint16_t id[3] = {120, 130, 150}, clubs[3] = {4, 4, 2}, ties[3] = {2, 2, 1}, add[3] = {4, 2, 0},
                          from[3] = {0, 4, 0};
    for (int r = 0; r < 3; r++) {
        uint32_t t = table + (uint32_t)r * ROUND_SIZE;
        memcpy(g_mem + t, g_mem + eng + ROUND_SIZE, ROUND_SIZE);
        wr16(g_mem, t + 0x07, id[r]);
        wr16(g_mem, t + 0x09, (uint16_t)r);
        wr16(g_mem, t + 0x18, clubs[r]);
        wr16(g_mem, t + 0x1a, ties[r]);
        wr16(g_mem, t + 0x1c, add[r]);
        wr16(g_mem, t + 0x1e, from[r]);
        uint16_t flags = rd16(g_mem, t + 0x0d) & ~0x200u;
        wr16(g_mem, t + 0x0d, r < 2 ? flags | 0x200u : flags);
        /* The higher place at home: no home swap of Cup::set_home_teams_for_round 0x51e870 (+0x0b bit 1 asks
         * Cup_Stage +0x90 at 0x51e8b7..0x51e92f, bit 8 compares the list's byte +4 at 0x51e9c7..0x51ea3d); with
         * neither, only a host nation (+0x24, -1 here) swaps (0x51eac3). */
        if (r < 2) g_mem[t + 0x0b] &= (uint8_t)~0x09u;
    }
    game_free(eng);
    for (int r = 0; r < 3 && !rt_unwinding; r++)
        set_dates(table, r, 1 + 4 * r, 3 + 4 * r, r == 2 ? 2 : 1, r == 2 ? 4 : 0, year);
    if (rt_unwinding) return 0;
    /* Entrant order: the quarter-finals 4th v 7th and 5th v 6th (list 0 v 1, 2 v 3), then the 2nd and the 3rd. */
    int order[SIX_CLUBS] = {rows[2], rows[5], rows[3], rows[4], rows[0], rows[1]};
    uint32_t list = game_malloc(4 * SIX_CLUBS);
    if (!list) {
        game_free(table);
        return 0;
    }
    for (int i = 0; i < SIX_CLUBS; i++) wr32(g_mem, list + 4u * (uint32_t)i, row_club(obj, order[i]));
    uint32_t sz[1] = {0xb2};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) {
        game_free(list);
        game_free(table);
        return 0;
    }
    uint32_t args[15] = {obj, SIX_CLUBS, list, 3, rd32(g_mem, obj + COMP_RECORD), table, year, 0, 1, 0xa0, 0x14,
                         0, 0, 0, 0};                /* as build_stage */
    call_this(F_CUP_STAGE, stage, 15, args);
    if (rt_unwinding) return 1;
    wr32(g_mem, rd32(g_mem, obj + COMP_STAGES), stage);
    test_stage = stage;
    test_final_seen = 0;
    game_free(list);
    game_free(table);
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, stage, 1, r0);
    if (rt_unwinding) return 1;
    wr32(g_mem, obj + COMP_STAGE_NOW, 0);
    fprintf(stderr, "recomp: playoffs: %s %u: play-off of places %d to %d drawn on day %d: quarter-finals %d v %d and "
                    "%d v %d, semi-finals at places %d and %d, a one-match final\n", file, year, rows[0] + 1,
            rows[5] + 1, (int16_t)rd16(g_mem, GAME_DATE), rows[2] + 1, rows[5] + 1, rows[3] + 1, rows[4] + 1,
            rows[0] + 1, rows[1] + 1);
    return 1;
}

/* Builds the play-off stage of a marked league on the day its table is complete. */
static void build_stage(uint32_t obj)
{
    const char *file = season_change_file(obj);
    int first = (int8_t)g_mem[obj + UP];             /* row index of the first play-off place */
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, obj + COMP_YEAR);
    if ((int8_t)g_mem[obj + UP_PLAYOFF] == SIX_CLUBS) {
        int rows[SIX_CLUBS], left = days_to_change(obj);
        int found = playoff_rows(obj, first, rows, SIX_CLUBS);
        if (rt_unwinding) return;
        if (found == SIX_CLUBS && left >= 0 && PLAYOFF_DAYS <= left + WAIT_DAYS && build_six(obj, rows)) return;
        if (rt_unwinding) return;
        fprintf(stderr, "recomp: playoffs: %s %u: no play-off of six (%d days to the season change), place %d goes "
                        "up\n", file, year, left, (found ? rows[0] : first) + 1);
        promote_first(obj, found ? rows[0] : first);
        return;
    }
    int left = days_to_change(obj), two = two_leg_final(obj, file);
    int rows[PLAYOFF_CLUBS] = {first, first + 1, first + 2, first + 3};
    if (playoff_rows(obj, first, rows, PLAYOFF_CLUBS) < PLAYOFF_CLUBS) left = -1;
    if (rt_unwinding) return;
    uint32_t clubs[PLAYOFF_CLUBS] = {row_club(obj, rows[3]), row_club(obj, rows[0]), row_club(obj, rows[2]),
                                     row_club(obj, rows[1])};
    for (int i = 0; i < PLAYOFF_CLUBS; i++)
        if (!readable(clubs[i])) left = -1;
    /* The play-off needs PLAYOFF_DAYS (+ the second leg of a two-leg final) after the table. When it ends after the
     * update day, the season change waits for it (season_playoffs_hold, up to WAIT_DAYS late, as in 2012: the Segunda's
     * final came 4 days after Spain's update day 170). The real Segunda ends on the first weekend of June and its
     * play-off final comes in the third week (2013/14: the league on 8 June, the final legs on 19 and 22 June), so
     * the play-off normally ends after day 170 and the early exit that stood here (gate (b), eee3e71a: 7 seasons of
     * Spain with 12 to 15 days left, O151/O259) skipped a play-off that the wait covers. Now only a play-off that
     * the wait cannot cover goes up directly: a net that does not act at the real dates. */
    int span = PLAYOFF_DAYS + (two ? final_gap((int)year) : 0);
    if (left >= 0 && (getenv("RECOMP_TEST_PLAYOFF_OLD_NOTIME") ? span >= left : span > left + WAIT_DAYS)) {
        fprintf(stderr, "recomp: playoffs: %s %u: %d days to the season change, no time for the play-off; place %d "
                        "goes up\n", file, year, left, rows[0] + 1);
        promote_first(obj, rows[0]);
        return;
    }
    if (left < 0) {
        fprintf(stderr, "recomp: playoffs: %s %u: no entry or no clubs, place %d goes up\n", file, year, rows[0] + 1);
        promote_first(obj, rows[0]);
        return;
    }
    /* The round table of EngFirst's stage 0: two rounds of 0x68 bytes in the right layout. */
    uint32_t outs = game_malloc(8);
    if (!outs) return;
    uint32_t ra[4] = {0, outs, outs + 4, 0};
    uint32_t table = call_this(F_ENG_ROUNDS, obj, 4, ra);
    int rounds = (int16_t)rd16(g_mem, outs);
    game_free(outs);
    if (rt_unwinding) return;
    if (!readable(table) || rounds != 2) {
        fprintf(stderr, "recomp: playoffs: %s %u: no round table (%d rounds), place %d goes up\n", file, year, rounds,
                first + 1);
        game_free(table);
        promote_first(obj, first);
        return;
    }
    set_dates(table, 0, 1, 3, 1, 0, year);          /* semi-finals: first legs on day 3, second legs 4 days later */
    if (!rt_unwinding) set_dates(table, 1, 8, 11, 2, two ? 0 : 4, year);   /* the final on day 11 (venue 4 as 0x572b5b) */
    if (two) {
        /* A two-leg final at the clubs' grounds, as the semi-finals of this table (+0x21 2 legs and +0x22 4 days
         * between them, 0x572b1c, 0x572b20) and the two-leg final of BraChampCup::create_schedule 0x425c10 (+0x89 2
         * at 0x425f92, +0x8a 3 days at 0x425f99, venue 0 at 0x425f38). final_gap() days here (3 to 2019/20, so that the play-off ends 15
         * days after the table). The final's tie rule (+0xd) is that of the semi-finals without their fixed pairs
         * flag 0x200 (0x572af3). Inference: +0x21 is the count of legs and +0x22 the days between them. */
        uint32_t f = table + ROUND_SIZE;
        g_mem[f + 0x21] = 2;
        g_mem[f + 0x22] = (uint8_t)final_gap((int)year);
        wr16(g_mem, f + 0xd, rd16(g_mem, table + 0xd) & 0xffu);
        /* The deciding flags +0xf: after the first leg the round's +0xf is OR'd into the second leg's flags
         * (Cup::process_fixture 0x51a6b3 into 0x521d76; the doc 2026-10-17-cup-round-record.md section 3: normally 3,
         * extra time and penalties). The semi-finals have 3 (0x572b0f), but the one-match final of the table has 0
         * (0x572b92: its flags 3 are in +0xd, 0x572b78). Without it the second leg had no extra time and no
         * penalties, a level tie went to replays (0x51a6e0..0x51a803) and the third match boxed (Cup.cpp 1450,
         * 0x51aa0d..0x51aac5; the Segunda play-off, day 175 of 2012). */
        if (!getenv("RECOMP_TEST_PLAYOFF_OLD_FINAL"))      /* tests only: the old final, to see the fault again */
            wr16(g_mem, f + 0xf, rd16(g_mem, table + 0xf));
    }
    uint32_t list = rt_unwinding ? 0 : game_malloc(4 * PLAYOFF_CLUBS);
    if (!list) {
        game_free(table);
        return;
    }
    for (int i = 0; i < PLAYOFF_CLUBS; i++) wr32(g_mem, list + 4u * (uint32_t)i, clubs[i]);
    uint32_t sz[1] = {0xb2};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) {
        game_free(list);
        game_free(table);
        return;
    }
    uint32_t args[15] = {obj, PLAYOFF_CLUBS, list, 2, rd32(g_mem, obj + COMP_RECORD), table, year, 0, 1, 0xa0, 0x14,
                         0, 0, 0, 0};                /* as 0x573180..0x573189 */
    call_this(F_CUP_STAGE, stage, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, obj + COMP_STAGES), stage);
    test_stage = stage;
    test_final_seen = 0;
    game_free(list);
    game_free(table);
    uint32_t r0[1] = {0};
    if (!rt_unwinding) call_this(F_SET_UP_ROUND, stage, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, obj + COMP_STAGE_NOW, 0);            /* only now: the save reads stages[0..+0x30] (0x689918) */
    fprintf(stderr, "recomp: playoffs: %s %u: play-off of places %d, %d, %d and %d drawn on day %d, %s final\n", file,
            year, rows[0] + 1, rows[1] + 1, rows[2] + 1, rows[3] + 1, (int16_t)rd16(g_mem, GAME_DATE),
            two ? "a two-leg" : "a one-match");
}

/* The play-off of 4 places after the direct places of a league whose class has its own play-off code: the I liga's
 * own play-off from 2019/20 (season_pol_first.c). The caller sets its places and one stage. */
int season_playoffs_build_for(uint32_t obj)
{
    if (!readable(rd32(g_mem, obj + COMP_STAGES))) return 0;     /* +0x30 is 0 already: the caller is advance_stage */
    build_stage(obj);
    return 1;
}
static void build_cross(uint32_t obj);
static void cross_result(uint32_t obj);

int season_playoffs_advance(uint32_t obj)
{
    if (cross_marked(obj)) {
        if ((int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) == -1 && readable(rd32(g_mem, obj + COMP_STAGES)))
            build_cross(obj);
        return 1;
    }
    if (!marked(obj) || (int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) != -1) return 0;
    if (!readable(rd32(g_mem, obj + COMP_STAGES))) return 0;
    build_stage(obj);
    return 1;
}

/* RECOMP_PLAYOFF_LOG (tests): the ties of each round of a league's play-off stage, at the final's result. The stage
 * (a Cup) holds its rounds' ties at +0xad (one block per round, 0x35 bytes a tie), the home and away slots at +0 and
 * +2 (-1 for none), each slot a 6-byte entry of the list +0xa7 whose +0 is the club (Cup::set_up_round 0x51c800:
 * 0x51c8a5..0x51c8be); the round count is +0x3c and the ties of a round its +0x1a (0x51c88f). */
static int place_of(uint32_t obj, uint32_t club)
{
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    for (int i = 0; club && i < n && i < 64; i++)
        if (row_club(obj, i) == club) return i + 1;
    return 0;
}

static void log_ties(uint32_t obj)
{
    uint32_t stage = rd32(g_mem, rd32(g_mem, obj + COMP_STAGES));
    if (!readable(stage)) return;
    uint32_t ties = rd32(g_mem, stage + 0xad), slots = rd32(g_mem, stage + 0xa7), rounds = rd32(g_mem, stage + 0xa3);
    int n = (int16_t)rd16(g_mem, stage + 0x3c);
    for (int r = 0; r < n && r < 8 && readable(ties) && readable(slots) && readable(rounds); r++) {
        uint32_t block = rd32(g_mem, ties + 4u * (uint32_t)r);
        int k = (int16_t)rd16(g_mem, rounds + (uint32_t)r * ROUND_SIZE + 0x1a);
        for (int t = 0; t < k && t < 16 && readable(block); t++) {
            uint32_t tie = block + 0x35u * (uint32_t)t;
            int16_t h = (int16_t)rd16(g_mem, tie), a = (int16_t)rd16(g_mem, tie + 2);
            uint32_t hc = h >= 0 ? rd32(g_mem, slots + 6u * (uint32_t)h) : 0;
            uint32_t ac = a >= 0 ? rd32(g_mem, slots + 6u * (uint32_t)a) : 0;
            fprintf(stderr, "recomp: playoffs: %s: round %d tie %d: %.40s (place %d) v %.40s (place %d)\n",
                    season_change_file(obj), r + 1, t + 1, readable(hc) ? win_str(hc + 4) : "?", place_of(obj, hc),
                    readable(ac) ? win_str(ac + 4) : "?", place_of(obj, ac));
        }
    }
}

/* League +0x44, base 0x686940 (club, code, stage id, &status, fixture, arg; ret 0x18): a result of the play-off
 * stage of a marked league goes to the English handler 0x576c50, which reads only base League fields. */
void o_00686940(void);
RT_WRAP(00686940, playoff_result)
{
    /* The Argentine Primera's knockouts (season_arg_stages.c): no news of promotion or relegation for their results. */
    if (season_arg_ko_result(cpu.ecx, (int8_t)g_mem[cpu.esp + 0xc])) {
        rt_ret_stdcall(0, 6);
        return;
    }
    /* Issue 150: the stages of the Belgian First Division. */
    if (season_bel_po_result(cpu.ecx, (int8_t)g_mem[cpu.esp + 0xc])) {
        rt_ret_stdcall(0, 6);
        return;
    }
    if (marked(cpu.ecx) && g_mem[cpu.esp + 0xc] != 0xff) {
        if (getenv("RECOMP_PLAYOFF_LOG"))
            fprintf(stderr, "recomp: playoffs: %s: result club %x (%.30s) code %d stage %d, day %d of %d\n",
                    season_change_file(cpu.ecx), rd32(g_mem, cpu.esp + 4),
                    readable(rd32(g_mem, cpu.esp + 4)) ? win_str(rd32(g_mem, cpu.esp + 4) + 4) : "?",
                    (int8_t)g_mem[cpu.esp + 8],
                    (int8_t)g_mem[cpu.esp + 0xc], (int16_t)rd16(g_mem, GAME_DATE), (int16_t)rd16(g_mem, GAME_DATE + 2));
        if (getenv("RECOMP_PLAYOFF_LOG") && (int8_t)g_mem[cpu.esp + 8] == 1) log_ties(cpu.ecx);
        cross_result(cpu.ecx);
        rt_jump(F_ENG_RESULT);
        return;
    }
    o_00686940();
}

/* ---- The places of each season ---- */

/* The season of the rules of a league object now, as season_sizes.c row_now: the edition of the start season at a
 * new game and with changing rules off (Q232), else the edition +0x40 (season_sizes_edition: a calendar-year league
 * takes row Y - 1). */
static int season_now(uint32_t obj, const char *file)
{
    int year = !season_changing_rules() || season_change_is_creating() ? season_start_year()
                                                                        : (int16_t)rd16(g_mem, obj + COMP_YEAR);
    /* The Irish calendar move: the edition itself, which season_rules_row_of maps to its row (41b for 2003). */
    return season_rules_moved(file) ? year : season_sizes_edition(file, year);
}

/* No stage of a season before: a stage object that the class's start_new_season did not free stays in the heap
 * (the class may free it after set_up_rules, so a free here could free it twice). */
static void drop_stage(uint32_t obj)
{
    uint32_t arr = rd32(g_mem, obj + COMP_STAGES);
    if (readable(arr)) wr32(g_mem, arr, 0);
    wr32(g_mem, obj + COMP_STAGE_NOW, 0xffffffffu);
}

/* The object of the division above (league +0x1c, the record id, -1 for none; EngFirst::set_up_rules stores it at
 * 0x572dea), or 0. The object of a record is g_comp_object_table [0xadadfc] at the record id, below its slot
 * count [0xadadf8] (League::process_promotion_relegation_chain 0x689c42), as season_sizes.c reads it. */
static uint32_t league_above(uint32_t obj)
{
    int32_t id = (int32_t)rd32(g_mem, obj + 0x1cu), slots = (int32_t)rd32(g_mem, 0x00adadf8u);
    uint32_t table = rd32(g_mem, 0x00adadfcu);
    if (id < 0 || id >= slots || !readable(table)) return 0;
    uint32_t other = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(other) ? other : 0;
}

/* 1 when the division above relegates the direct places and the play-off winner (+0xc1 = direct + 1). With
 * fewer, the winner stays down. Observed in a first build: the Conference 2002/03 (League Two keeps 1 down, its
 * floor in slice 18) and the Greek second tier 2010/11 (GrePrm 2 down; the play-off was played, 3 clubs had
 * status 0 and the 2 direct clubs went up). Inference: the chain 0x689c80 moves as many clubs up as come down. Then the
 * league keeps its class's places and has no play-off (the nearest form, Q247). The places of the division above
 * are those of its last set_up_rules: at a new game the constructors run before g_comp_object_table is filled,
 * so no division is found there, and the new_game pass of season_sizes.c (sizes_new_game) runs this again. */
static int balanced(uint32_t obj, int direct)
{
    uint32_t above = league_above(obj);
    return above && (int8_t)g_mem[above + DOWN] == direct + 1;
}

static int cross_rules(uint32_t obj);

void season_playoffs_rules(uint32_t obj)
{
    if (season_bel_po_rules(obj)) return;       /* issue 150: the Belgian First Division's own stages */
    if (!playoffs_on() || cross_rules(obj) || !plain_league(obj)) return;
    const char *file = season_change_file(obj);
    int direct = 0, season = season_now(obj, file);
    int clubs = shape_clubs(file, season, &direct);
    /* Open item 283: the real 2023. J1 sends 1 down and takes 3 up, because it grows from 18 to 20 clubs, so the J2
     * play-off plays with no third club coming down (the wrap of 0x689c80 moves each club on its own). */
    int want = clubs && (balanced(obj, direct) || (!strcmp(file, "jap_j2") && season_changing_rules() &&
                                                  season_places_jap_grows(season, direct)));
    int have = (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) == 1;
    if (!want) {
        /* The class set_up_rules rewrites +0xbf (ScoFirst 0x7eee9a), so after a cross season the mark can be gone
         * while the stage count stays. ScoSecond and ScoThird are plain leagues with no stage of their own. */
        if (have || (strcmp(file, "sco_first") && (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) == 1)) {                                  /* the play-off ends: no stage count, no stage */
            drop_stage(obj);
            wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        }
        return;
    }
    if (!readable(rd32(g_mem, obj + COMP_STAGES))) {
        uint32_t arr = game_malloc(4);
        if (!arr) return;
        wr32(g_mem, obj + COMP_STAGES, arr);
    }
    drop_stage(obj);
    wr32(g_mem, obj + COMP_STAGE_COUNT, 1);
    g_mem[obj + UP] = (uint8_t)direct;
    g_mem[obj + UP_PLAYOFF] = (uint8_t)clubs;
    fprintf(stderr, "recomp: playoffs: %s %d: places %d to %d\n", file, season, direct + 1, direct + clubs);
    if (getenv("RECOMP_PLAYOFF_LOG"))                /* the reserve sides that the play-off would skip */
        for (int i = 0; i < (int16_t)rd16(g_mem, obj + COMP_CLUBS) && i < 64 && !rt_unwinding; i++)
            if (barred(obj, row_club(obj, i)))
                fprintf(stderr, "recomp: playoffs: %s %d: %.51s is a reserve side of a club above\n", file, season,
                        win_str(row_club(obj, i) + 4));
}

/* ---- Scotland: the play-off between two divisions (0.7.2 gap row 11) ----
 *
 * Real life (the rules rows of Scotland; research note sz04-exchange-research, the SPFL rules of June 2026 C18 to C34):
 * from 2005/06 the 9th club of a division plays the 2nd to 4th of the division below for one place in the upper
 * division, semi-finals 4th v 9th and 3rd v 2nd, a final, all two legs ("2nd to 4th play off with Championship 9th").
 * From 2013/14 the Premiership's 11th plays the Championship's play-off as a ladder: 3rd v 4th, the winner v 2nd,
 * that winner v the 11th, all two legs (the row's text "quarter-final 3rd v 4th, semi-final 2nd v winner"). The
 * encoded rows hold it: the lower division has 3 promotion play-off places from 2nd (po_up 3, po_up_first 2) and the
 * upper division 1 relegation play-off place (po_down 1). The League Two 10th's play-off with the Highland or Lowland
 * League winner (from 2014/15) has no league below in the game, so that club stays (the nearest form).
 *
 * The original has no such play-off. ScoSecond and ScoThird are plain leagues (advance_stage 0x5a8f60, result
 * 0x686940). ScoFirst has its own: advance_stage 0x7ef270 builds a League_Stage of three clubs (0x68b100 at
 * 0x7ef517), its two rows of status 1 and the Premier Division's row of status 2 (0x7ef3e1, 0x7ef48a), when its +0xbf
 * places give two rows of status 1 (0x7ef3fb). The class data never sets those places, so it never runs; the wraps
 * below take its two slots for the cross play-off only.
 *
 * How. The lower division is marked as a plain league of this kind is (stage count +0x2c 1, no stage +0x30 -1) with 3
 * promotion play-off places (+0xbf); the upper division gets 1 relegation play-off place (+0xc0). League::
 * update_positions_and_status 0x684cd0 then gives the lower rows 2 to 4 status 1 and the upper's place status 2,
 * as ScoFirst's own play-off reads them. On the days after the lower table is complete, advance_stage builds the stage
 * once the upper table (and the Premier Division's split stages) is complete: four clubs, one of them from the upper
 * division, a Cup_Stage as in build_stage with two-leg semi-finals and final, or the three rounds of the ladder (one
 * tie each, new entrants 2, 1 and 1: +0x1c and +0x1e as the English play-off rounds, 0x572b09 and 0x572b15). The
 * final's winner (result code 1, the English handler 0x576c50 gives a row of the lower table status 0 at 0x576d9e)
 * goes up; when it is a lower club, the upper club's row gets status 3 (cross_result). The chain moves the rows of
 * status 3 of the upper division with those of status 0 or 5 of the lower one in pairs (0x689c20 passes pairs at
 * 0x689c5a, expected counts -1), so 2 and 2 move when a lower club wins and 1 and 1 when the upper club wins. */

#define CROSS_LOWER 3                   /* the play-off places of the lower division: 2nd to 4th */
#define CROSS_JAPAN 1                   /* J2's play-off place: its 3rd (open item 283) */
#define JAPAN_DIRECT 2                  /* J2's direct places in those seasons (JapJ2::set_up_rules 0x665140, +0xbe at 0x665190) */
#define JAPAN_THIRD 2                   /* the table row of J2's 3rd */
#define CROSS_SPAN 22                   /* days from the table to the last leg of the ladder */
#define F_SCO_FIRST_ADVANCE 0x007ef270u /* ScoFirst +0x28: advance_stage */
#define F_SCO_FIRST_RESULT 0x007ef5d0u  /* ScoFirst +0x44: a stage result (ret 0x18, as 0x686940) */
#define STATUS_PLAYOFF_DOWN 2           /* a row of the relegation play-off places (0x7ef48a) */
#define STATUS_DOWN 3

static int sco_division(const char *f)
{
    return f && (!strcmp(f, "sco_prm") || !strcmp(f, "sco_first") || !strcmp(f, "sco_second") ||
                 !strcmp(f, "sco_third"));
}

/* Japan (open item 283): the real seasons 2004 to 2008 play J1's 16th against J2's 3rd over two legs, the winner in J1
 * ("16th v J2 3rd in a two-legged promotion/relegation series", rows aus-jap-kor-192 to 196 and 219 to 223). The same
 * play-off with one tie: J2 is a plain league (vtable 0x96caf4: advance_stage 0x5a8f60, result 0x686940), and J1's
 * table gives its place status 2 by +0xc0 (League::update_positions_and_status 0x684cd0). */
static int jap_lower(const char *f) { return f && !strcmp(f, "jap_j2"); }
static int cross_top(const char *f) { return f && (!strcmp(f, "sco_prm") || !strcmp(f, "jap_j1")); }
static int cross_division(const char *f) { return sco_division(f) || jap_lower(f) || cross_top(f); }

/* The division below (league +0x20, the record id, -1 for none; 0x689c35), or 0. */
static uint32_t league_below(uint32_t obj)
{
    int32_t id = (int32_t)rd32(g_mem, obj + 0x20u), slots = (int32_t)rd32(g_mem, 0x00adadf8u);
    uint32_t table = rd32(g_mem, 0x00adadfcu);
    if (id < 0 || id >= slots || !readable(table)) return 0;
    uint32_t other = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(other) ? other : 0;
}

/* 1 when the rows of season give lower and the division above it a play-off between them; *ladder 1 for the
 * Premiership's three rounds. Both rows are read for one season: the divisions of a nation change their season in
 * one pass, but not at once, so the year +0x40 of the other division can still be the old one. */
static int cross_shape(uint32_t lower, int season, uint32_t *upper, int *ladder)
{
    const char *lf = readable(lower) ? season_change_file(lower) : NULL;
    if (jap_lower(lf)) {
        uint32_t j1 = league_above(lower);
        /* Only with changing rules on: with them off the game stays as 0.7.2 plays it (spec L2). */
        if (!season_changing_rules() || !j1 || strcmp(season_change_file(j1), "jap_j1") ||
            !season_places_jap_cross(season))
            return 0;
        if (upper) *upper = j1;
        if (ladder) *ladder = 0;
        return 1;
    }
    if (!sco_division(lf) || !strcmp(lf, "sco_prm")) return 0;
    uint32_t up = league_above(lower);
    const char *uf = up ? season_change_file(up) : NULL;
    if (!sco_division(uf)) return 0;
    const SeasonRule *lr = row_of_season(lf, season), *ur = row_of_season(uf, season);
    const SeasonLeagueRule *le = lr ? league_rule(lf, lr->season) : NULL, *ue = ur ? league_rule(uf, ur->season) : NULL;
    if (!le || !ue || le->po_up != CROSS_LOWER || le->po_up_first != 2 || ue->po_down != 1 || lr->up != 1) return 0;
    if (upper) *upper = up;
    if (ladder) *ladder = lr->playoffs_text && strstr(lr->playoffs_text, "quarter-final") != NULL;
    return 1;
}

/* ---- Portugal: one tie between the Primeira Liga and the Segunda Liga (issue 154, open item 248) ----
 *
 * Real life (rows por-109 and por-110): in 2013/14 the Primeira Liga's 15th played the Segunda Liga's 3rd club that is
 * not a reserve side, over two legs, for a place in the Primeira Liga. The rows from 2020/21 hold the same tie with the
 * 16th. The encoded rows give the lower division 1 promotion play-off place (po_up 1) and the upper division 1
 * relegation play-off place (po_down 1).
 *
 * PorPrm (vtable 0x96e6f0) and PorSecond (vtable 0x96e7a4) are plain leagues: advance_stage 0x5a8f60, result 0x686940.
 * So the tie uses the marks, the wait for the upper division, the result and the chain of the Scottish play-off above,
 * with one round of two clubs. */
#define TIE_CLUBS 2
#define TIE_SPAN 10                     /* days from the table to the second leg */

/* Greece and Croatia (issue 156, open items 281 and 282) play the same tie: the rows give the Football League's 2nd
 * or 3rd against the Super League's place above its direct places, and the Druga HNL's 2nd against the Prva HNL's. Only
 * with changing rules on, and only in a season of one group: the two groups of the Greek second tier (2013/14, 2014/15,
 * 2021/22 to 2025/26) and of the Druga HNL (to 2005/06) have no single club for the tie. GreSecond (vtable 0x96b61c) is a
 * plain league. CroA2a (vtable 0x969178) has its own advance_stage 0x516060 and result 0x7ebc40 (wraps below). */
#define KEY_GRE_SECOND 0x009cf870u      /* the division index of the National B Division (0x5ebd54) */
#define COMP_CLOSED 0x4cu               /* s32: not 0 keeps League::update_for_date from closing the season (0x6849e8) */
#define COMP_TIE_FLAG 0xe6u             /* s32: a tie-break of level clubs is still open (0x516394, 0x684a1e) */

static const char *const TIE_PAIRS[][2] = {
    {"por_second", "por_prm"}, {"gre_second", "gre_prm"}, {"cro_a2a", "cro_a1"},
};

static const char *tie_upper_of(const char *lower)
{
    for (unsigned i = 0; lower && i < sizeof TIE_PAIRS / sizeof TIE_PAIRS[0]; i++)
        if (!strcmp(lower, TIE_PAIRS[i][0])) return TIE_PAIRS[i][1];
    return NULL;
}

static int tie_lower(const char *f) { return tie_upper_of(f) != NULL; }

static int tie_upper(const char *f)
{
    for (unsigned i = 0; f && i < sizeof TIE_PAIRS / sizeof TIE_PAIRS[0]; i++)
        if (!strcmp(f, TIE_PAIRS[i][1])) return 1;
    return 0;
}

/* 1 when a second group of the tier plays this season. The second gre_second object is that group or waits for it. */
static int tie_two_groups(uint32_t lower, const char *file, int season)
{
    if (!strcmp(file, "cro_a2a")) return season_sizes_clubs("cro_a2b", season) >= 2;
    if (strcmp(file, "gre_second")) return 0;
    uint32_t rec = rd32(g_mem, lower + COMP_RECORD);
    return !readable(rec) || rd32(g_mem, rec) != rd32(g_mem, KEY_GRE_SECOND) ||
           season_rules_find(season, file, 1) != NULL;
}

/* 1 when the rows of season give lower and the division above it the tie. Only with changing rules on: with them off
 * the chain moves pairs (season_sizes.c) and the game keeps the class's places (decision L2). */
static int tie_shape(uint32_t lower, int season, uint32_t *upper)
{
    if (!season_changing_rules()) return 0;
    const char *lf = readable(lower) ? season_change_file(lower) : NULL;
    uint32_t up = tie_lower(lf) ? league_above(lower) : 0;
    const char *uf = up ? season_change_file(up) : NULL;
    if (!uf || strcmp(uf, tie_upper_of(lf))) return 0;
    const SeasonRule *lr = row_of_season(lf, season), *ur = row_of_season(uf, season);
    const SeasonLeagueRule *le = lr ? league_rule(lf, lr->season) : NULL, *ue = ur ? league_rule(uf, ur->season) : NULL;
    if (!le || !ue) return 0;
    if (!season_tie_played(le->po_up, ue->po_down, tie_two_groups(lower, lf, season), 1)) return 0;
    if (upper) *upper = up;
    return 1;
}

/* ---- A relegation play-off over a play-off of four (issue 160, item 276) ----
 *
 * Four clubs of the lower division play semi-finals and a final, and the winner meets the upper division's play-off
 * club for one place. The list of leagues and seasons, with the sources of the legs, is in
 * docs/research/2026-11-18-relegation-playoffs-over-four.md. The rows decide the seasons: the lower row has 4 play-off
 * places directly below its direct places (po_up 4, po_up_first up + 1) and the upper row 1 relegation play-off place
 * (po_down 1). The original plays only the last tie for Norway and Ireland (NorFirst::create_play_off_stage 0x790e20
 * and IreFirst::create_promotion_playoff_stage 0x635dc0: one row of status 1 against the upper row of status 2), and
 * nothing for J2 and Ligue 3, whose classes have no stage (advance_stage 0x5a8f60). */
#define OVER_LOWER 4                    /* the play-off places of the lower division */
#define OVER_CLUBS 5                    /* the four and the upper division's club */

typedef struct {
    const char *lower, *upper;
    uint8_t legs[3];                    /* semi-finals, the lower division's final, the tie with the upper club */
    uint8_t last_venue;                 /* the venue code of the last tie: 0 the clubs' grounds, 4 neutral */
} OverFour;

static const OverFour OVER_FOUR[] = {
    {"nor_first", "nor_prm", {1, 1, 2}, 0},
    {"ire_first", "ire_prm", {2, 1, 1}, 4},
    {"jap_j2", "jap_j1", {1, 1, 1}, 0},
    {"fra_third", "fra_second", {1, 1, 2}, 0},
};

/* The pair of a lower (upper 0) or upper (upper 1) division file, or NULL. */
static const OverFour *over_pair(const char *file, int upper)
{
    for (unsigned i = 0; file && i < sizeof OVER_FOUR / sizeof OVER_FOUR[0]; i++)
        if (!strcmp(file, upper ? OVER_FOUR[i].upper : OVER_FOUR[i].lower)) return &OVER_FOUR[i];
    return NULL;
}

/* 1 when the rows of season give the lower division and the division above it this play-off. Both rows are read for
 * one season, as cross_shape. *direct gets the lower division's direct places. Only with changing rules on (issue 160,
 * spec L2): with them off the game plays as 0.7.2. J2's other forms need other rows: the tie of its 3rd has po_up 1
 * (season_places_jap_cross), and a plain play-off of four has no relegation play-off place in J1. */
static int over_shape(uint32_t lower, int season, uint32_t *upper, int *direct)
{
    if (!season_changing_rules()) return 0;
    const char *lf = readable(lower) ? season_change_file(lower) : NULL;
    const OverFour *o = over_pair(lf, 0);
    uint32_t up = o ? league_above(lower) : 0;
    if (!up || strcmp(season_change_file(up), o->upper)) return 0;
    const SeasonRule *lr = row_of_season(lf, season), *ur = row_of_season(o->upper, season);
    const SeasonLeagueRule *le = lr ? league_rule(lf, lr->season) : NULL;
    const SeasonLeagueRule *ue = ur ? league_rule(o->upper, ur->season) : NULL;
    if (!le || !ue || lr->up < 0 || le->po_up != OVER_LOWER || le->po_up_first != lr->up + 1 || ue->po_down != 1)
        return 0;
    if (upper) *upper = up;
    if (direct) *direct = lr->up;
    return 1;
}

/* The lower division of a play-off over a play-off of four in its season (over_rules). */
static int over_marked(uint32_t obj)
{
    if (!readable(obj) || !over_pair(season_change_file(obj), 0)) return 0;
    return (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) == 1 && (int8_t)g_mem[obj + UP_PLAYOFF] == OVER_LOWER &&
           over_shape(obj, season_now(obj, season_change_file(obj)), NULL, NULL);
}

/* 1 when the last tie of the league's pair is one match at the clubs' grounds (J2: the J1 club is at home). */
static int over_last_at_upper(uint32_t league)
{
    const OverFour *o = over_pair(season_change_file(league), 0);
    return o && o->legs[2] == 1 && o->last_venue == 0;
}

/* The lower division of a play-off between two divisions this season: a Scottish division below the top with one
 * stage and 3 play-off places, J2 with one stage and 1 play-off place (cross_rules), the Segunda Liga with one stage
 * and 1 play-off place (tie_rules), or the lower division of a play-off over a play-off of four (over_rules). */
static int cross_marked(uint32_t obj)
{
    if (!readable(obj) || !playoffs_on()) return 0;
    const char *f = season_change_file(obj);
    if (over_marked(obj)) return 1;
    if ((int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) != 1) return 0;
    if (tie_lower(f)) return (int8_t)g_mem[obj + UP_PLAYOFF] == 1;
    if (jap_lower(f)) return (int8_t)g_mem[obj + UP_PLAYOFF] == CROSS_JAPAN;
    return sco_division(f) && strcmp(f, "sco_prm") && (int8_t)g_mem[obj + UP_PLAYOFF] == CROSS_LOWER;
}

/* The places of the Primeira Liga and the Segunda Liga for the tie (after their set_up_rules). 1 when this season has
 * the tie or a mark of a season before was cleared: the plain play-off of this kind is not theirs then. */
static int tie_rules(uint32_t obj, const char *file)
{
    int season = season_now(obj, file);
    uint32_t upper = 0, below = league_below(obj);
    if (tie_upper(file)) {
        int want = below && tie_shape(below, season, &upper) && upper == obj;
        if (want && (int8_t)g_mem[obj + DOWN_PLAYOFF] != 1) {
            g_mem[obj + DOWN_PLAYOFF] = 1;
            fprintf(stderr, "recomp: playoffs: %s %d: place %d plays off with %s\n", file, season,
                    (int16_t)rd16(g_mem, obj + COMP_CLUBS) - (int8_t)g_mem[obj + DOWN], season_change_file(below));
        } else if (!want && (int8_t)g_mem[obj + DOWN_PLAYOFF] == 1) {
            g_mem[obj + DOWN_PLAYOFF] = 0;
        }
        return want;
    }
    if (!tie_shape(obj, season, &upper)) {
        if (!cross_marked(obj)) return 0;
        drop_stage(obj);
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        g_mem[obj + UP_PLAYOFF] = 0;
        return 1;
    }
    if (!readable(rd32(g_mem, obj + COMP_STAGES))) {
        uint32_t arr = game_malloc(4);
        if (!arr) return 1;
        wr32(g_mem, obj + COMP_STAGES, arr);
    }
    drop_stage(obj);
    wr32(g_mem, obj + COMP_STAGE_COUNT, 1);
    g_mem[obj + UP_PLAYOFF] = 1;
    /* CroA2a holds its season open for its tie-break: start_new_season sets +0x4c (0x515f01), set_up_rules sets +0xe6
     * (0x515e80), and only 0x516360 clears them, with stage count 0 (0x516385). The tie's count of 1 skips that code, so
     * the season closes as a plain league's does (League::update_for_date 0x6849e8, 0x684a1e). */
    if (!strcmp(file, "cro_a2a")) {
        wr32(g_mem, obj + COMP_CLOSED, 0);
        wr32(g_mem, obj + COMP_TIE_FLAG, 0);
    }
    fprintf(stderr, "recomp: playoffs: %s %d: one place plays off with %s\n", file, season, season_change_file(upper));
    return 1;
}

/* The places of a division of a pair, after its set_up_rules. 1 when this form owns the division this season. The
 * upper division's place: each class writes +0xc0 in every set_up_rules (NorPrm 0x792758, IrePrm 0x63a957, JapJ1
 * 0x66367e, FraSecond 0x5bf104), so a season with no such play-off needs no reset here. */
static int over_rules(uint32_t obj, const char *file)
{
    int season = season_now(obj, file), direct = 0;
    uint32_t below = league_below(obj);
    if (over_pair(file, 1) && below && over_shape(below, season, NULL, NULL)) g_mem[obj + DOWN_PLAYOFF] = 1;
    if (!over_pair(file, 0) || !over_shape(obj, season, NULL, &direct)) return 0;
    if (!readable(rd32(g_mem, obj + COMP_STAGES))) {
        uint32_t arr = game_malloc(4);
        if (!arr) return 1;
        wr32(g_mem, obj + COMP_STAGES, arr);
    }
    drop_stage(obj);
    wr32(g_mem, obj + COMP_STAGE_COUNT, 1);
    g_mem[obj + UP] = (uint8_t)direct;
    g_mem[obj + UP_PLAYOFF] = OVER_LOWER;
    fprintf(stderr, "recomp: playoffs: %s %d: places %d to %d play off, the winner meets the play-off club of %s\n",
            file, season, direct + 1, direct + OVER_LOWER, over_pair(file, 0)->upper);
    return 1;
}

/* The places of a division of a play-off between two divisions (after its set_up_rules, as season_playoffs_rules). 1
 * when the plain play-off of this kind is not its own: a Scottish division, J1, and J2 in a season of the tie. J2 in
 * another season goes on to the plain play-off (its four clubs from the real 2011). */
static int cross_rules(uint32_t obj)
{
    const char *file = readable(obj) ? season_change_file(obj) : NULL;
    if (file && (tie_lower(file) || tie_upper(file))) return tie_rules(obj, file);
    /* J2 has two forms: over_rules takes the seasons of 4 play-off places under J1's play-off place, and the code
     * below the seasons of the tie of its 3rd. */
    if (file && (over_pair(file, 0) || over_pair(file, 1)) && over_rules(obj, file)) return 1;
    if (!cross_division(file)) return 0;
    int season = season_now(obj, file), ladder = 0;
    uint32_t upper = 0, below = league_below(obj);
    if (below && cross_shape(below, season, &upper, NULL) && upper == obj && (int8_t)g_mem[obj + DOWN_PLAYOFF] != 1) {
        g_mem[obj + DOWN_PLAYOFF] = 1;
        fprintf(stderr, "recomp: playoffs: %s %d: place %d plays off with %s\n", file, season,
                (int16_t)rd16(g_mem, obj + COMP_CLUBS) - (int8_t)g_mem[obj + DOWN], season_change_file(below));
    }
    if (cross_top(file)) return 1;
    int want = cross_shape(obj, season, &upper, &ladder), have = cross_marked(obj);
    if (!want && jap_lower(file)) return 0;     /* the class wrote +0xbf 0; season_playoffs_rules drops the stage */
    if (!want) {
        /* The class set_up_rules rewrites +0xbf (ScoFirst 0x7eee9a), so after a cross season the mark can be gone
         * while the stage count stays. ScoSecond and ScoThird are plain leagues with no stage of their own. */
        if (have || (strcmp(file, "sco_first") && (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) == 1)) {
            drop_stage(obj);
            wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
            g_mem[obj + UP_PLAYOFF] = 0;
        }
        return 1;
    }
    if (!readable(rd32(g_mem, obj + COMP_STAGES))) {
        uint32_t arr = game_malloc(4);
        if (!arr) return 1;
        wr32(g_mem, obj + COMP_STAGES, arr);
    }
    drop_stage(obj);
    wr32(g_mem, obj + COMP_STAGE_COUNT, 1);
    if (jap_lower(file)) {
        g_mem[obj + UP] = JAPAN_DIRECT;
        g_mem[obj + UP_PLAYOFF] = CROSS_JAPAN;
        fprintf(stderr, "recomp: playoffs: %s %d: place 3 plays off with %s\n", file, season,
                season_change_file(upper));
        return 1;
    }
    g_mem[obj + UP] = 1;
    g_mem[obj + UP_PLAYOFF] = CROSS_LOWER;
    fprintf(stderr, "recomp: playoffs: %s %d: places 2 to 4 play off with %s%s\n", file, season,
            season_change_file(upper), ladder ? " (3 rounds)" : "");
    return 1;
}

/* League::check_complete 0x689ad0 of a table, read only (as season_pol_playoff.c league_complete): a table (+0xb1)
 * that is not locked (+0xb5) of a league that is made (+0x45), each row (0x3b bytes) at its matches (+0xa7, or
 * (clubs - 1) * meetings before the first test, 0x689b1d..0x689b36), or the complete flag +0xdd (0x689b01). */
static int table_complete(uint32_t lg)
{
    if (!readable(lg) || !readable(rd32(g_mem, lg + COMP_TABLE)) || g_mem[lg + 0xb5u] || !rd32(g_mem, lg + 0x45u))
        return 0;
    if (rd32(g_mem, lg + 0xddu)) return 1;
    int clubs = (int16_t)rd16(g_mem, lg + COMP_CLUBS), matches = (int16_t)rd16(g_mem, lg + 0xa7u);
    if (matches == -1 && clubs > 0) matches = (int16_t)((clubs - 1) * (int16_t)rd16(g_mem, lg + 0x3cu));
    uint32_t rows = rd32(g_mem, lg + COMP_TABLE);
    for (int i = 0; i < clubs; i++)
        if ((int8_t)g_mem[rows + ROW_SIZE * (uint32_t)i + 0x11u] < matches) return 0;
    return 1;
}

/* The upper division's places are final: its table, and for the Premier Division its split stages (+0x2c of them,
 * built up to +0x30; the stage part of check_complete 0x689bb6..0x689be8). Another division's stage is its own
 * play-off as a lower division, which does not move its places. */
static int upper_done(uint32_t up)
{
    if (!table_complete(up)) return 0;
    /* CroA1 decides its tie-break after the table. CroA2a::update_for_date 0x516360, which CroA1 shares, clears +0xe6 when
     * no clubs are level (0x5163e5), else it makes one stage (0x516400, 0x516416): a Cup_Stage of 0xb2 bytes (0x514dce), so
     * its end is asked of the stage itself, as League::update_for_date does (vtable +0x10 with 0 and 1, 0x6849b3..0x6849be). */
    if (!strcmp(season_change_file(up), "cro_a1")) {
        int32_t made = (int32_t)rd32(g_mem, up + COMP_STAGE_COUNT), now = (int32_t)rd32(g_mem, up + COMP_STAGE_NOW);
        if (made == 0) return rd32(g_mem, up + COMP_TIE_FLAG) == 0;
        uint32_t slots = rd32(g_mem, up + COMP_STAGES), stage = readable(slots) ? rd32(g_mem, slots) : 0;
        if (now < 0 || !readable(stage)) return 0;
        uint32_t ask[2] = {0, 1};
        return call_this(rd32(g_mem, rd32(g_mem, stage) + 0x10), stage, 2, ask) & 0xff;
    }
    if (strcmp(season_change_file(up), "sco_prm")) return 1;
    int32_t built = (int32_t)rd32(g_mem, up + COMP_STAGE_NOW), count = (int32_t)rd32(g_mem, up + COMP_STAGE_COUNT);
    uint32_t stages = rd32(g_mem, up + COMP_STAGES);
    if (built < count - 1) return 0;
    for (int32_t i = 0; i <= built && i < 64; i++)
        if (!readable(stages) || !table_complete(rd32(g_mem, stages + 4u * (uint32_t)i))) return 0;
    return 1;
}

/* The upper division's play-off club: its row of status 2, else the place above its direct relegation places. -1 for
 * none. */
static int upper_row(uint32_t up)
{
    uint32_t table = rd32(g_mem, up + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, up + COMP_CLUBS);
    if (!readable(table) || n <= 0 || n > 64) return -1;
    for (int i = 0; i < n; i++)
        if (g_mem[table + (uint32_t)i * ROW_SIZE + ROW_STATUS] == STATUS_PLAYOFF_DOWN) return i;
    int i = n - (int8_t)g_mem[up + DOWN] - 1;
    return i >= 0 && i < n ? i : -1;
}

static int in_table(uint32_t obj, uint32_t club)
{
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    for (int i = 0; i < n && i < 64; i++)
        if (row_club(obj, i) == club) return 1;
    return 0;
}

/* The lower club of the tie: the first club after the direct places that is not a reserve side barred from the upper
 * division (row por-110: "the 3rd club that is not a reserve team"). 0 for none. */
/* The clubs before the lower club of the tie. Portugal counts the clubs that are not reserve sides after the direct
 * places (row por-110), whatever the row's first play-off place says (4th in 2013/14, when a reserve side was 3rd). */
static int tie_skip(const char *file, int season, int direct)
{
    const SeasonRule *lr = row_of_season(file, season);
    const SeasonLeagueRule *le = lr ? league_rule(file, lr->season) : NULL;
    return season_tie_skip(le && strcmp(file, "por_second") ? le->po_up_first : 0, direct);
}

static uint32_t tie_club(uint32_t obj)
{
    const char *file = season_change_file(obj);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    int skip = tie_skip(file, season_now(obj, file), (int8_t)g_mem[obj + UP]);
    for (int i = 0; i < n && i < 64 && !rt_unwinding; i++) {
        uint32_t c = row_club(obj, i);
        if (!readable(c) || barred(obj, c)) continue;
        if (skip-- <= 0) return c;
    }
    return 0;
}

uint32_t season_playoffs_tie_club(uint32_t lower, int edition, const uint32_t *club, int n)
{
    const char *file = readable(lower) ? season_change_file(lower) : NULL;
    if (!playoffs_on() || !tie_shape(lower, edition, NULL)) return 0;
    const SeasonRule *r = row_of_season(file, edition);
    int skip = tie_skip(file, edition, r && r->up > 0 ? r->up : 0);    /* the club that tie_club picks */
    for (int i = 0; i < n && !rt_unwinding; i++) {
        if (!readable(club[i]) || barred(lower, club[i])) continue;
        if (skip-- <= 0) return club[i];
    }
    return 0;
}

/* One round of two clubs over two legs: EngFirst's semi-final round (two legs, extra time and penalties in the second
 * leg, 0x572af3..0x572b20) with the final's id 150 (+0x07, 0x572b6e), so that the winner gets result code 1. */
static void build_tie(uint32_t obj)
{
    const char *file = season_change_file(obj);
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR), left = days_to_change(obj);
    uint32_t up = 0;
    int shape = tie_shape(obj, season_now(obj, file), &up), time = left >= 0 && TIE_SPAN <= left + WAIT_DAYS;
    if (shape && time && !upper_done(up)) return;   /* the next day again, as build_cross */
    int urow = shape ? upper_row(up) : -1;
    uint32_t u = urow >= 0 ? row_club(up, urow) : 0, l = shape ? tie_club(obj) : 0;
    if (rt_unwinding) return;
    if (!shape || !time || !readable(u) || !readable(l)) {
        fprintf(stderr, "recomp: playoffs: %s %d: no cross play-off (%d days to the season change)\n", file, year, left);
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        return;
    }
    uint32_t outs = game_malloc(8);
    if (!outs) return;
    uint32_t ra[4] = {0, outs, outs + 4, 0};
    uint32_t eng = call_this(F_ENG_ROUNDS, obj, 4, ra);
    int eng_rounds = (int16_t)rd16(g_mem, outs);
    game_free(outs);
    if (rt_unwinding) return;
    uint32_t table = readable(eng) && eng_rounds == 2 ? game_malloc(ROUND_SIZE) : 0;
    if (!table) {
        game_free(eng);
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        return;
    }
    memcpy(g_mem + table, g_mem + eng, ROUND_SIZE);
    wr16(g_mem, table + 0x07, rd16(g_mem, eng + ROUND_SIZE + 0x07));
    wr16(g_mem, table + 0x09, 0);
    wr16(g_mem, table + 0x18, TIE_CLUBS);
    wr16(g_mem, table + 0x1a, 1);
    wr16(g_mem, table + 0x1c, TIE_CLUBS);
    wr16(g_mem, table + 0x1e, 0);
    game_free(eng);
    set_dates(table, 0, 1, 3, 2, 0, (uint32_t)year);
    uint32_t list = rt_unwinding ? 0 : game_malloc(4 * TIE_CLUBS);
    if (!list) {
        game_free(table);
        return;
    }
    wr32(g_mem, list, l);
    wr32(g_mem, list + 4, u);
    uint32_t sz[1] = {0xb2};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) {
        game_free(list);
        game_free(table);
        return;
    }
    uint32_t args[15] = {obj, TIE_CLUBS, list, 1, rd32(g_mem, obj + COMP_RECORD), table,
                         (uint32_t)year, 0, 1, 0xa0, 0x14, 0, 0, 0, 0};    /* as build_stage, 0x573180..0x573189 */
    call_this(F_CUP_STAGE, stage, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, obj + COMP_STAGES), stage);
    game_free(list);
    game_free(table);
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, stage, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, obj + COMP_STAGE_NOW, 0);
    fprintf(stderr, "recomp: playoffs: cross\t%s\t%d\tdrawn\t%.51s\t%.51s\ttie\tday %d\n", file, year, win_str(l + 4),
            win_str(u + 4), (int16_t)rd16(g_mem, GAME_DATE));
}

/* The stage of a play-off over a play-off of four: three rounds of the stage. Semi-finals of 4 new clubs in fixed pairs,
 * the lower division's final with no new club, then the winner against 1 new club, the upper division's (list slot 4;
 * +0x18 clubs, +0x1a ties, +0x1c new clubs and +0x1e the first new entrant, as the ladder of build_cross). A round of two
 * legs starts from EngFirst's semi-final (0x572af3..0x572b20), a round of one match from its final (flags 3 in +0x0d,
 * 0x572b78), as build_six. With no time or no clubs the direct places alone move. */
static void build_over(uint32_t obj, const OverFour *o)
{
    const char *file = season_change_file(obj);
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, obj + COMP_YEAR), up = 0;
    int shape = over_shape(obj, season_now(obj, file), &up, NULL), left = days_to_change(obj);
    /* season_playoffs_hold does not wait for an update day from LATE_DAY (Norway: 27 November), so there the stage must
     * end before the change. */
    int time = left >= 0 && CROSS_SPAN <= left + (change_is_late(obj) ? 0 : WAIT_DAYS);
    if (shape && time && !upper_done(up)) return;   /* the next day again while the upper division plays */
    int rows[OVER_LOWER], found = shape ? playoff_rows(obj, (int8_t)g_mem[obj + UP], rows, OVER_LOWER) : 0;
    if (rt_unwinding) return;
    int urow = shape ? upper_row(up) : -1;
    uint32_t u = urow >= 0 ? row_club(up, urow) : 0;
    if (!shape || !time || found < OVER_LOWER || !readable(u)) {
        fprintf(stderr, "recomp: playoffs: %s %u: no play-off over the four (%d days to the season change)\n", file,
                year, left);
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        return;
    }
    uint32_t outs = game_malloc(8);
    if (!outs) return;
    uint32_t ra[4] = {0, outs, outs + 4, 0};
    uint32_t eng = call_this(F_ENG_ROUNDS, obj, 4, ra);
    int eng_rounds = (int16_t)rd16(g_mem, outs);
    game_free(outs);
    if (rt_unwinding) return;
    uint32_t table = readable(eng) && eng_rounds == 2 ? game_malloc(3 * ROUND_SIZE) : 0;
    if (!table) {
        game_free(eng);
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        return;
    }
    static const uint16_t id[3] = {120, 130, 150}, clubs[3] = {4, 2, 2}, ties[3] = {2, 1, 1}, add[3] = {4, 0, 1},
                          from[3] = {0, 0, 4};
    /* A row can name one match on neutral ground for the last tie (Norway, rows nor-fin-183 and nor-fin-193). */
    const SeasonRule *row = row_of_season(file, season_now(obj, file));
    int neutral = row && row->playoffs_text && strstr(row->playoffs_text, "one match on neutral ground");
    int last_legs = neutral ? 1 : o->legs[2], last_venue = neutral ? 4 : o->last_venue;
    for (int r = 0; r < 3; r++) {
        uint32_t t = table + (uint32_t)r * ROUND_SIZE;
        memcpy(g_mem + t, g_mem + eng + ((r == 2 ? last_legs : o->legs[r]) == 2 ? 0 : ROUND_SIZE), ROUND_SIZE);
        wr16(g_mem, t + 0x07, id[r]);
        wr16(g_mem, t + 0x09, (uint16_t)r);
        wr16(g_mem, t + 0x18, clubs[r]);
        wr16(g_mem, t + 0x1a, ties[r]);
        wr16(g_mem, t + 0x1c, add[r]);
        wr16(g_mem, t + 0x1e, from[r]);
        /* Fixed pairs keep the sides that over_draw_round sets: without bit 0x200 the draw 0x51e220 runs after the
         * hook (0x51c5e4..0x51c5f1) and can swap them. */
        int fixed = r == 0 || (r == 2 && last_legs == 1 && last_venue == 0);
        uint16_t flags = rd16(g_mem, t + 0x0d) & ~0x200u;
        wr16(g_mem, t + 0x0d, fixed ? flags | 0x200u : flags);
        if (fixed) g_mem[t + 0x0b] &= (uint8_t)~0x09u;    /* no home swap, as build_six */
    }
    game_free(eng);
    for (int r = 0; r < 3 && !rt_unwinding; r++)
        set_dates(table, r, 1 + 7 * r, 3 + 7 * r, r == 2 ? 2 : 1, r == 2 ? (uint32_t)last_venue : 0, year);
    if (rt_unwinding) return;
    /* The list: semi-finals of the last place against the first and the third against the second, then the upper club. */
    uint32_t order[OVER_CLUBS] = {row_club(obj, rows[3]), row_club(obj, rows[0]), row_club(obj, rows[2]),
                                  row_club(obj, rows[1]), u};
    uint32_t list = game_malloc(4 * OVER_CLUBS);
    if (!list) {
        game_free(table);
        return;
    }
    for (int i = 0; i < OVER_CLUBS; i++) wr32(g_mem, list + 4u * (uint32_t)i, order[i]);
    uint32_t sz[1] = {0xb2};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) {
        game_free(list);
        game_free(table);
        return;
    }
    uint32_t args[15] = {obj, OVER_CLUBS, list, 3, rd32(g_mem, obj + COMP_RECORD), table, year, 0, 1, 0xa0, 0x14,
                         0, 0, 0, 0};                /* as build_stage, 0x573180..0x573189 */
    call_this(F_CUP_STAGE, stage, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, obj + COMP_STAGES), stage);
    game_free(list);
    game_free(table);
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, stage, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, obj + COMP_STAGE_NOW, 0);
    fprintf(stderr, "recomp: playoffs: cross\t%s\t%u\tdrawn\t%.51s\t%.51s\t%.51s\t%.51s\t%.51s\tover four\tday %d\n", file,
            year, win_str(order[0] + 4), win_str(order[1] + 4), win_str(order[2] + 4), win_str(order[3] + 4),
            win_str(order[4] + 4), (int16_t)rd16(g_mem, GAME_DATE));
}

static void build_cross(uint32_t obj)
{
    const char *file = season_change_file(obj);
    if (over_marked(obj)) {
        build_over(obj, over_pair(file, 0));
        return;
    }
    if (tie_lower(file)) {
        build_tie(obj);
        return;
    }
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR), ladder = 0;
    uint32_t up = 0;
    int shape = cross_shape(obj, season_now(obj, file), &up, &ladder), left = days_to_change(obj);
    int time = left >= 0 && CROSS_SPAN <= left + WAIT_DAYS;
    /* The next day again while the upper division plays and the play-off still fits before the change. */
    if (shape && time && !upper_done(up)) return;
    int urow = shape ? upper_row(up) : -1;
    int single = jap_lower(file);
    uint32_t u = urow >= 0 ? row_club(up, urow) : 0, l2 = row_club(obj, 1), l3 = row_club(obj, JAPAN_THIRD),
             l4 = row_club(obj, 3);
    if (!shape || !time || !readable(u) || !readable(l3) || (!single && (!readable(l2) || !readable(l4)))) {
        /* No play-off: both play-off places stay (1 and 1 move, the direct places). */
        fprintf(stderr, "recomp: playoffs: %s %d: no cross play-off (%d days to the season change)\n", file, year,
                left);
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        return;
    }
    uint32_t outs = game_malloc(8);
    if (!outs) return;
    uint32_t ra[4] = {0, outs, outs + 4, 0};
    uint32_t eng = call_this(F_ENG_ROUNDS, obj, 4, ra);
    int eng_rounds = (int16_t)rd16(g_mem, outs);
    game_free(outs);
    if (rt_unwinding) return;
    if (!readable(eng) || eng_rounds != 2) {
        game_free(eng);
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        return;
    }
    int rounds = single ? 1 : ladder ? 3 : 2, entrants = single ? 2 : PLAYOFF_CLUBS;
    uint32_t table = game_malloc((uint32_t)rounds * ROUND_SIZE);
    if (!table) {
        game_free(eng);
        return;
    }
    /* Every round as EngFirst's semi-finals (two legs, 4 days apart, extra time and penalties in the second leg,
     * 0x572af3..0x572b20); the final keeps its id 150 (+0x07, 0x572b6e). */
    for (int r = 0; r < rounds; r++) memcpy(g_mem + table + (uint32_t)r * ROUND_SIZE, g_mem + eng, ROUND_SIZE);
    uint32_t fin = table + (uint32_t)(rounds - 1) * ROUND_SIZE;
    wr16(g_mem, fin + 0x07, rd16(g_mem, eng + ROUND_SIZE + 0x07));
    if (single) {
        /* One tie of two legs: 2 clubs, both new, fixed as the list gives them. */
        wr16(g_mem, fin + 0x09, 0);
        wr16(g_mem, fin + 0x18, 2);
        wr16(g_mem, fin + 0x1a, 1);
        wr16(g_mem, fin + 0x1c, 2);
        wr16(g_mem, fin + 0x1e, 0);
    } else if (ladder) {
        /* One tie in each round: 2 new clubs, then 1 and 1 (+0x18 clubs, +0x1a ties, +0x1c new, +0x1e the first
         * entrant). No fixed pairs after the first round (+0x0d bit 0x200): a winner meets the new club. */
        static const uint16_t id[3] = {120, 130, 150}, add[3] = {2, 1, 1}, first[3] = {0, 2, 3};
        for (int r = 0; r < 3; r++) {
            uint32_t t = table + (uint32_t)r * ROUND_SIZE;
            wr16(g_mem, t + 0x07, id[r]);
            wr16(g_mem, t + 0x09, (uint16_t)r);
            wr16(g_mem, t + 0x18, 2);
            wr16(g_mem, t + 0x1a, 1);
            wr16(g_mem, t + 0x1c, add[r]);
            wr16(g_mem, t + 0x1e, first[r]);
            if (r) wr16(g_mem, t + 0x0d, rd16(g_mem, t + 0x0d) & ~0x200u);
        }
    } else {
        /* The final: 2 clubs, 1 tie, no new club, no fixed pairs (as EngFirst's final, 0x572b63..0x572b86). */
        wr16(g_mem, fin + 0x09, 1);
        wr16(g_mem, fin + 0x18, 2);
        wr16(g_mem, fin + 0x1a, 1);
        wr16(g_mem, fin + 0x1c, 0);
        wr16(g_mem, fin + 0x1e, 0);
        wr16(g_mem, fin + 0x0d, rd16(g_mem, fin + 0x0d) & ~0x200u);
    }
    game_free(eng);
    for (int r = 0; r < rounds && !rt_unwinding; r++)
        set_dates(table, r, 1 + 7 * r, 3 + 7 * r, r == rounds - 1 ? 2 : 1, 0, (uint32_t)year);
    if (rt_unwinding) return;
    /* The list in entrant order. Semi-finals of fixed pairs (list 0 v 1, 2 v 3, home side of the first leg drawn
     * by map_fixture_tree 0x519302): 4th v 9th, 3rd v 2nd. The ladder: 4th v 3rd, then the 2nd, then the 11th. */
    uint32_t clubs[PLAYOFF_CLUBS] = {l4, u, l3, l2};
    if (ladder) clubs[1] = l3, clubs[2] = l2, clubs[3] = u;
    if (single) clubs[0] = l3, clubs[1] = u;
    uint32_t list = game_malloc(4u * (uint32_t)entrants);
    if (!list) {
        game_free(table);
        return;
    }
    for (int i = 0; i < entrants; i++) wr32(g_mem, list + 4u * (uint32_t)i, clubs[i]);
    uint32_t sz[1] = {0xb2};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) {
        game_free(list);
        game_free(table);
        return;
    }
    uint32_t args[15] = {obj, (uint32_t)entrants, list, (uint32_t)rounds, rd32(g_mem, obj + COMP_RECORD), table,
                         (uint32_t)year, 0, 1, 0xa0, 0x14, 0, 0, 0, 0};    /* as build_stage, 0x573180..0x573189 */
    call_this(F_CUP_STAGE, stage, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, obj + COMP_STAGES), stage);
    game_free(list);
    game_free(table);
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, stage, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, obj + COMP_STAGE_NOW, 0);
    fprintf(stderr, "recomp: playoffs: cross\t%s\t%d\tdrawn\t%.51s\t%.51s\t%.51s\t%.51s\t%s\tday %d\n", file, year,
            win_str(clubs[0] + 4), win_str(clubs[1] + 4), single ? "-" : win_str(clubs[2] + 4),
            single ? "-" : win_str(clubs[3] + 4), single ? "one tie" : ladder ? "ladder" : "semi-finals",
            (int16_t)rd16(g_mem, GAME_DATE));
}

/* A result of the stage of a marked lower division (the args of +0x44: club, code, stage id; the stage id is not
 * 0xff). Code 1 is the final's winner (0x576d71). A lower club goes up (the English handler gives its row status 0
 * after this) and the upper club down (status 3 here); the upper club that wins stays. */
static void cross_result(uint32_t obj)
{
    if (!cross_marked(obj) || (int8_t)g_mem[cpu.esp + 8] != 1) return;
    uint32_t club = rd32(g_mem, cpu.esp + 4), up = league_above(obj);
    int urow = up ? upper_row(up) : -1;
    if (!readable(club) || urow < 0) return;
    uint32_t uclub = row_club(up, urow);
    int lower_won = in_table(obj, club);
    if (lower_won) g_mem[rd32(g_mem, up + COMP_TABLE) + (uint32_t)urow * ROW_SIZE + ROW_STATUS] = STATUS_DOWN;
    fprintf(stderr, "recomp: playoffs: cross\t%s\t%d\tfinal\t%.51s\t%s\t%.51s\t%s\n", season_change_file(obj),
            (int16_t)rd16(g_mem, obj + COMP_YEAR), win_str(club + 4),
            season_change_file(lower_won ? obj : up), readable(uclub) ? win_str(uclub + 4) : "?",
            season_change_file(up));
}

/* ScoFirst's own advance_stage and result handler (thiscall; advance_stage has no argument, the handler ret 0x18):
 * the cross play-off when it is the lower division, else the original. */
void o_007ef270(void);
RT_WRAP(007ef270, sco_first_advance)
{
    if (cross_marked(cpu.ecx)) {
        season_playoffs_advance(cpu.ecx);
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    o_007ef270();
}

/* CroA2a's advance_stage and the Croatian result handler in a season of the tie. CroA2a::update_for_date 0x516360 makes
 * its tie-break stage only with stage count 0 (0x516385), so the tie's count of 1 keeps that stage out of the season. */
void o_00516060(void);
RT_WRAP(00516060, cro_a2a_advance)
{
    if (cross_marked(cpu.ecx)) {
        season_playoffs_advance(cpu.ecx);
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    o_00516060();
}

void o_007ebc40(void);
RT_WRAP(007ebc40, cro_a2a_result)
{
    if (cross_marked(cpu.ecx) && g_mem[cpu.esp + 0xc] != 0xff) {
        cross_result(cpu.ecx);
        if (!rt_unwinding) rt_jump(F_ENG_RESULT);
        return;
    }
    o_007ebc40();
}

void o_007ef5d0(void);
RT_WRAP(007ef5d0, sco_first_result)
{
    if (cross_marked(cpu.ecx) && g_mem[cpu.esp + 0xc] != 0xff) {
        cross_result(cpu.ecx);
        if (!rt_unwinding) rt_jump(F_ENG_RESULT);
        return;
    }
    o_007ef5d0();
}

/* NorFirst and IreFirst have their own advance_stage (0x790d50, 0x635cf0: thiscall, no argument) and result handler
 * (0x791080, 0x636020: ret 0x18, the codes of the English handler for stage 0xff). In a season of a play-off over a
 * play-off of four the stage is this kind's, as for ScoFirst above. */
void o_00790d50(void);
RT_WRAP(00790d50, nor_first_advance)
{
    if (over_marked(cpu.ecx) && playoffs_on()) {
        season_playoffs_advance(cpu.ecx);
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    o_00790d50();
}

void o_00635cf0(void);
RT_WRAP(00635cf0, ire_first_advance)
{
    if (over_marked(cpu.ecx) && playoffs_on()) {
        season_playoffs_advance(cpu.ecx);
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    o_00635cf0();
}

void o_00791080(void);
RT_WRAP(00791080, nor_first_result)
{
    if (over_marked(cpu.ecx) && playoffs_on() && g_mem[cpu.esp + 0xc] != 0xff) {
        cross_result(cpu.ecx);
        if (!rt_unwinding) rt_jump(F_ENG_RESULT);
        return;
    }
    o_00791080();
}

void o_00636020(void);
RT_WRAP(00636020, ire_first_result)
{
    if (over_marked(cpu.ecx) && playoffs_on() && g_mem[cpu.esp + 0xc] != 0xff) {
        cross_result(cpu.ecx);
        if (!rt_unwinding) rt_jump(F_ENG_RESULT);
        return;
    }
    o_00636020();
}

/* ---- The season change waits for the play-off ---- */

/* game_season_update_pass 0x5d5ae0 calls start_new_season only on an entry's update day (+0x15 against the day of
 * the year at 0x5d5bc8) and only for a competition whose vtable +0xc is true (0x5d5ccb). For a league that is
 * 0x48ce10: false when it has a division above (+0x1c >= 0, 0x48ce13), else its own check_complete(1, 1) (vtable
 * +0x10) and that of each division below along +0x20 (0x48ce30..0x48ce48). So the pass starts only the top
 * division, and a lower division that is not complete holds the whole chain. The game can move a play-off match: in
 * a test run the Segunda's final of 2011/12, due on 13 June 2012, was played after the update day of 20 June, and
 * all Spanish leagues stayed in their old season (year +0x40 2011 in 2012/13). So an entry with a marked league
 * that is not complete (check_complete(1, 1): the table and every stage, 0x689bbc..0x689be8) waits: on its update
 * day the pass sees no update day for it, and on the first later day that the league is complete the pass sees
 * today. The original then does the change as on its own day. A waiting entry is one past its update day with a
 * marked league whose year +0x40 is before the game year (start_new_season moves it). +0x40 is in the save. A first
 * form read the entry's season year +0x35 instead, and a game loaded in the wait never changed its season
 * (inference: a load sets +0x35 from the game year, as create_spanish_comps does at 0x83761b). At most WAIT_DAYS:
 * then the pass sees today whether the stage is complete or not. The original's chain test (0x48ce10) is still false
 * while a lower division is not complete, so the chain stays one season behind (the doc's test). A play-off of 12
 * days cannot normally take 60 days. */

static int16_t held_day[ENTRY_COUNT];
static int held[ENTRY_COUNT];
static int logged[ENTRY_COUNT];         /* the day of the last log line of an entry, +1 */
static int held_net[ENTRY_COUNT];       /* 1 while an entry waits through the net (entry_unfinished) */

/* The marked leagues of an entry with a built stage and a year before the game year. 0: none; 1: all their
 * stages are complete; 2: one is not (*file gets its file). */
static int entry_playoffs(int e, int year, const char **file)
{
    uint32_t buf[COMPS_MAX];
    int n = season_change_entry_comps(e, buf, COMPS_MAX), seen = 0;
    for (int i = 0; i < n && !rt_unwinding; i++) {
        if ((!marked(buf[i]) && !season_ned_playoff_pending(buf[i])) || (int16_t)rd16(g_mem, buf[i] + COMP_YEAR) >= year)
            continue;
        /* A Scottish cross play-off is built only once the upper division is done (build_cross): until then the
         * entry waits for it, as for a stage that is not complete. */
        if ((int32_t)rd32(g_mem, buf[i] + COMP_STAGE_NOW) < 0) {
            if (!cross_marked(buf[i])) continue;
            *file = season_change_file(buf[i]);
            return 2;
        }
        seen = 1;
        uint32_t vt = rd32(g_mem, buf[i]), args[2] = {1, 1};
        if (!(call_this(rd32(g_mem, vt + 0x10), buf[i], 2, args) & 0xff)) {   /* check_complete(1, 1) */
            *file = season_change_file(buf[i]);
            return 2;
        }
    }
    return seen;
}

/* The net of O288 (den-stall-2, lead 4 under O259: it must never act). A plain league of an entry that is not
 * complete on the entry's update day misses the change and runs a season late (the Danish leagues in gate (a) runs 7
 * and 8: a league match moved past the update day). fixture_season_end.c keeps the moves of the game before the
 * update day at the source. If a plain league is still not complete, the entry waits as for a play-off (at most
 * WAIT_DAYS), with a line "season change net" that soak_run.py counts as a failed leg. 0: no plain league in the
 * wait; 1: all are complete; 2: one is not (*file gets its file). */
static int entry_unfinished(int e, int year, const char **file)
{
    uint32_t buf[COMPS_MAX];
    int n = season_change_entry_comps(e, buf, COMPS_MAX), seen = 0;
    for (int i = 0; i < n && !rt_unwinding; i++) {
        if (!plain_league(buf[i]) || marked(buf[i]) || season_bel_po_holds(buf[i]) ||
            (int16_t)rd16(g_mem, buf[i] + COMP_YEAR) >= year)
            continue;
        seen = 1;
        uint32_t vt = rd32(g_mem, buf[i]), args[2] = {1, 1};
        if (!(call_this(rd32(g_mem, vt + 0x10), buf[i], 2, args) & 0xff)) {   /* check_complete(1, 1) */
            *file = season_change_file(buf[i]);
            return 2;
        }
    }
    return seen;
}

/* 1 when an entry holds a league that stalled in an earlier season (season_sizes_cro_behind). */
static int entry_behind(int e, int year)
{
    uint32_t buf[COMPS_MAX];
    int n = season_change_entry_comps(e, buf, COMPS_MAX);
    for (int i = 0; i < n && !rt_unwinding; i++)
        if (season_sizes_cro_behind(buf[i], year)) return 1;
    return 0;
}

int season_playoffs_hold(void)
{
    if (!season_game_changes()) return 0;
    int today = (int16_t)rd16(g_mem, GAME_DATE), year = (int16_t)rd16(g_mem, GAME_DATE + 2), any = 0;
    for (int e = 0; e < ENTRY_COUNT && !rt_unwinding; e++) {
        uint32_t at = ENTRIES + (uint32_t)e * ENTRY_SIZE;
        int upd = (int16_t)rd16(g_mem, at + ENTRY_UPDATE_DAY);
        held[e] = 0;
        /* A save of a game whose Croatian leagues stalled (0.7.2 and 0.7.3 before the fix in season_sizes_cro_loaded:
         * the HNL holds last season's edition after its update day). The entry changes its season today, as on its own
         * day, once the South has its table back. */
        if (upd >= 0 && upd < LATE_DAY && today > upd && entry_behind(e, year)) {
            held_day[e] = (int16_t)upd;
            held[e] = 1;
            wr16(g_mem, at + ENTRY_UPDATE_DAY, (uint16_t)today);
            if (logged[e] != today + 1)
                fprintf(stderr, "recomp: season change: entry %d holds last season's edition after its update day %d "
                                "of %d: it changes its season on day %d\n", e, upd, year, today);
            logged[e] = today + 1;
            any = 1;
            continue;
        }
        if (upd < 0 || upd >= LATE_DAY || today < upd || today - upd > WAIT_DAYS) continue;
        const char *file = "";
        int p = playoffs_on() ? entry_playoffs(e, year, &file) : 0;
        /* Item 104: the I liga's play-off, made late after the Ekstraklasa's spring stage (season_pol_playoff.c). It is
         * the original's play-off, not one of this kind, so RECOMP_PLAYOFFS=off does not turn it off. */
        if (p != 2) {
            const char *f3 = "";
            int q = season_pol_playoff_unfinished(e, year, &f3);
            if (q == 2) p = 2, file = f3;
            else if (q && !p) p = 1;
        }
        /* Issue 150: the Belgian play-offs and the final round that waits for them. */
        if (p != 2) {
            const char *f4 = "";
            int q = season_bel_po_unfinished(e, year, &f4);
            if (q == 2) p = 2, file = f4;
            else if (q && !p) p = 1;
        }
        /* The net only for an entry with a plain league that is not complete on its update day itself. */
        const char *f2 = "";
        int u = today == upd || held_net[e] ? entry_unfinished(e, year, &f2) : 0;
        if (today == upd) held_net[e] = u == 2;
        int net = held_net[e];
        int state = p == 2 || (net && u == 2) ? 2 : p || (net && u) ? 1 : 0;
        if (net && p != 2) file = f2;
        if (!state) continue;
        if (today == upd && state == 2) {
            held_day[e] = (int16_t)upd;
            held[e] = 1;
            wr16(g_mem, at + ENTRY_UPDATE_DAY, 0xffffu);
            if (logged[e] != today + 1)
                fprintf(stderr, net ? "recomp: season change net: entry %d waits for %s, not complete on its update "
                                      "day %d of %d\n"
                                    : "recomp: playoffs: entry %d waits for the play-off of %s (update day %d of %d)\n",
                        e, file, upd, year);
        } else if (today > upd && (state == 1 || today - upd == WAIT_DAYS)) {
            held_day[e] = (int16_t)upd;
            held[e] = 1;
            wr16(g_mem, at + ENTRY_UPDATE_DAY, (uint16_t)today);
            if (logged[e] != today + 1)
                fprintf(stderr, "recomp: %s: entry %d changes its season on day %d of %d, %d days late%s\n",
                        net ? "season change net" : "playoffs", e, today, year, today - upd,
                        state == 2 ? " (not complete)" : "");
            held_net[e] = 0;
        }
        if (held[e]) logged[e] = today + 1;
        any |= held[e];
    }
    return any;
}

void season_playoffs_release(int any)
{
    if (!any) return;
    for (int e = 0; e < ENTRY_COUNT; e++)
        if (held[e]) {
            wr16(g_mem, ENTRIES + (uint32_t)e * ENTRY_SIZE + ENTRY_UPDATE_DAY, (uint16_t)held_day[e]);
            held[e] = 0;
        }
}

/* ---- The final places of a waiting entry before the European places (open item 94) ---- */

#define COMP_UPPER 0x1cu                /* s32 record id of the division above, -1 at the top (0x689c24) */
#define COMP_LOWER 0x20u                /* s32 record id of the division below, -1 at the bottom (0x689c35) */
#define COMP_OBJECTS 0x00adadfcu        /* the competition object of each record id */

static int league_class(uint32_t obj)
{
    uint32_t vt = readable(obj) ? rd32(g_mem, obj) : 0;
    for (int i = 0; vt && i < season_comp_class_count; i++)
        if (season_comp_classes[i].vtable == vt) return season_comp_classes[i].kind == SEASON_KIND_LEAGUE;
    return 0;
}

void season_playoffs_final_places(void)
{
    if (!season_game_changes()) return;
    int today = (int16_t)rd16(g_mem, GAME_DATE), year = (int16_t)rd16(g_mem, GAME_DATE + 2);
    uint32_t objects = rd32(g_mem, COMP_OBJECTS);
    for (int e = 0; e < ENTRY_COUNT && !rt_unwinding; e++) {
        int upd = held[e] ? held_day[e] : (int16_t)rd16(g_mem, ENTRIES + (uint32_t)e * ENTRY_SIZE + ENTRY_UPDATE_DAY);
        if (upd < 0 || upd >= LATE_DAY || today <= upd || today - upd > WAIT_DAYS) continue;
        uint32_t buf[COMPS_MAX];
        int n = season_change_entry_comps(e, buf, COMPS_MAX);
        for (int i = 0; i < n && !rt_unwinding; i++) {
            uint32_t top = buf[i];
            if (!league_class(top) || (int32_t)rd32(g_mem, top + COMP_UPPER) >= 0 ||
                (int16_t)rd16(g_mem, top + COMP_YEAR) >= year)
                continue;
            /* Only a finished top table: an entry that waits through the net (entry_unfinished) has matches left. */
            uint32_t args[2] = {1, 1};
            if (!(call_this(rd32(g_mem, rd32(g_mem, top) + 0x10), top, 2, args) & 0xff)) continue;   /* check_complete */
            int k = 0;
            for (uint32_t obj = top; readable(obj) && k < 16 && !rt_unwinding; k++) {
                call_this(rd32(g_mem, rd32(g_mem, obj) + 0xa4), obj, 0, NULL);   /* record_final_positions */
                int32_t lower = (int32_t)rd32(g_mem, obj + COMP_LOWER);
                obj = lower >= 0 && readable(objects) ? rd32(g_mem, objects + 4u * (uint32_t)lower) : 0;
            }
            fprintf(stderr, "recomp: playoffs: entry %d waits for its season change (update day %d): the final places "
                            "of %s and %d divisions below it come before the European places\n", e, upd,
                    season_change_file(top), k - 1);
        }
    }
}

/* ---- Rule kind "playoffs": the news ---- */

static void playoffs_after(const SeasonChange *c)
{
    if (!playoffs_on() || !c->started || c->start || !plain_league(c->comp)) return;
    int direct = 0, clubs = shape_clubs(c->file, c->season, &direct);
    if (marked(c->comp) && clubs && !season_playoffs_shape(c->file, c->season - 1, NULL))
        season_change_news("The %.50s now has promotion play-offs: places %d to %d play for one place.",
                           (const char *)g_mem + rd32(g_mem, c->comp + COMP_RECORD) + 4, direct + 1,
                           direct + clubs);
}

const SeasonRuleKind season_kind_playoffs = {"playoffs", NULL, NULL, NULL, NULL, NULL, playoffs_after};
