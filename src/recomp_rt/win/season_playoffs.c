/* Promotion play-offs in leagues whose class has none (0.6.0 slice 19; spec Q247, Q272; research and design in
 * docs/research/2026-10-18-playoffs-splits-calendar.md).
 *
 * Which leagues. A league row of the rules table whose play-off text gives four places in a row just below the
 * direct places ("3rd to 6th", tools/seasons/encode_leagues.py, season_league_table.c), of a League class with
 * no play-off code of its own: advance_stage (vtable +0x28) is the empty function 0x5a8f60 and the result
 * handler (+0x44) is the base League one 0x686940. Examples: the Segunda from 2010/11, the Turkish second tier
 * from 2010/11, each only when the division above relegates the direct places and one more (see balanced).
 * So not the Conference (League Two relegates 1) and not the Greek second tier (GrePrm relegates 2). J2 and the
 * Argentine second tier have no set_up_rules wrap of slice 18, so they get no play-off. A class with its own
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
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_league.h"
#include "season_names.h"
#include "season_rules.h"
#include "season_playoffs.h"

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
#define ROUND_SIZE 0x68u
#define PLAYOFF_DAYS 12                 /* the last match on day 11 after the table is complete */
#define FINAL_LEG_DAYS 3                /* a two-leg final: the second leg 3 days after the first (0x425f99) */
#define NO_WEEKDAY 0xffffffffu          /* Date::Date 0x549f70 keeps the date for weekday -1 (src/si/date.c) */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

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

/* A league that this kind gave a play-off: a plain league with one stage and 4 play-off places. */
static int marked(uint32_t obj)
{
    return plain_league(obj) && (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) == 1 &&
           (int8_t)g_mem[obj + UP_PLAYOFF] == PLAYOFF_CLUBS;
}

/* The encoded row of a file and season (season_league_table.c), or NULL. */
static const SeasonLeagueRule *league_rule(const char *file, int season)
{
    for (int i = 0; i < season_league_rule_count; i++)
        if (season_league_rules[i].season == season && !strcmp(season_league_rules[i].file, file))
            return &season_league_rules[i];
    return NULL;
}

/* The row of the rules of a season, or the last before it (Q248), as season_sizes.c reads it. */
static const SeasonRule *row_of_season(const char *file, int season)
{
    for (int s = season; s >= SEASON_FIRST_YEAR && s > season - 10; s--) {
        const SeasonRule *r = season_rules_find(s, file, 0);
        if (r) return r;
    }
    return NULL;
}

/* 1 when the row of season gives a play-off that this kind plays: 4 places in a row, directly below the direct
 * places of the row. The up column counts the play-off winner in most parts ("2 direct + 1 via play-offs":
 * the first play-off place is up) and only the direct places in the English part (the Conference 2002/03: up 1,
 * places 2 to 5). *direct gets the direct places. */
int season_playoffs_shape(const char *file, int season, int *direct)
{
    const SeasonRule *r = row_of_season(file, season);
    const SeasonLeagueRule *e = r ? league_rule(file, r->season) : NULL;
    if (!e || e->po_up != PLAYOFF_CLUBS || r->up < 0) return 0;
    /* Two ties whose winners both go up (Série B from 2025) are not one play-off for one place. */
    if (r->playoffs_text && strstr(r->playoffs_text, "two winners")) return 0;
    int d;
    if (e->po_up_first == r->up && r->up >= 1) d = r->up - 1;
    else if (e->po_up_first == r->up + 1) d = r->up;
    else return 0;
    if (r->clubs > 0 && e->po_up_first + PLAYOFF_CLUBS - 1 > r->clubs) return 0;
    if (direct) *direct = d;
    return 1;
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
 * reserve one does not matter: a senior club's reserve side never plays in the division above it. */
static int barred(uint32_t obj, uint32_t club)
{
    uint32_t above = league_above(obj);
    if (!above || !readable(club)) return 0;
    uint32_t out = game_malloc(4);
    if (!out) return 0;
    uint32_t a[3] = {club, out, 0};
    uint32_t pair = call_cdecl(F_LINKED, 3, a);
    game_free(out);
    if (rt_unwinding || !readable(pair)) return 0;
    uint32_t div = rd32(g_mem, pair + CLUB_DIVISION);
    return readable(div) && div == rd32(g_mem, above + COMP_RECORD);
}

/* The rows of the play-off: the 4 rows from the first play-off place on that no reserve side holds. Gives the
 * count found (fewer than 4 only at the foot of a short table). */
static int playoff_rows(uint32_t obj, int first, int rows[PLAYOFF_CLUBS])
{
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS), k = 0;
    for (int i = first; i < n && k < PLAYOFF_CLUBS && !rt_unwinding; i++) {
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
    o_0051a150();
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
    if (y < 1990 || y > 2100 || d < 0 || d > 366) return;
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

/* Builds the play-off stage of a marked league on the day its table is complete. */
static void build_stage(uint32_t obj)
{
    const char *file = season_change_file(obj);
    int first = (int8_t)g_mem[obj + UP];             /* row index of the first play-off place */
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, obj + COMP_YEAR);
    int left = days_to_change(obj), two = two_leg_final(obj, file);
    int rows[PLAYOFF_CLUBS] = {first, first + 1, first + 2, first + 3};
    if (playoff_rows(obj, first, rows) < PLAYOFF_CLUBS) left = -1;
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
    int span = PLAYOFF_DAYS + (two ? FINAL_LEG_DAYS : 0);
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
         * at 0x425f92, +0x8a 3 days at 0x425f99, venue 0 at 0x425f38). 3 days here, so that the play-off ends 15
         * days after the table. The final's tie rule (+0xd) is that of the semi-finals without their fixed pairs
         * flag 0x200 (0x572af3). Inference: +0x21 is the count of legs and +0x22 the days between them. */
        uint32_t f = table + ROUND_SIZE;
        g_mem[f + 0x21] = 2;
        g_mem[f + 0x22] = FINAL_LEG_DAYS;
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

int season_playoffs_advance(uint32_t obj)
{
    if (!marked(obj) || (int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) != -1) return 0;
    if (!readable(rd32(g_mem, obj + COMP_STAGES))) return 0;
    build_stage(obj);
    return 1;
}

/* League +0x44, base 0x686940 (club, code, stage id, &status, fixture, arg; ret 0x18): a result of the play-off
 * stage of a marked league goes to the English handler 0x576c50, which reads only base League fields. */
void o_00686940(void);
RT_WRAP(00686940, playoff_result)
{
    if (marked(cpu.ecx) && g_mem[cpu.esp + 0xc] != 0xff) {
        if (getenv("RECOMP_PLAYOFF_LOG"))
            fprintf(stderr, "recomp: playoffs: %s: result club %x (%.30s) code %d stage %d, day %d of %d\n",
                    season_change_file(cpu.ecx), rd32(g_mem, cpu.esp + 4),
                    readable(rd32(g_mem, cpu.esp + 4)) ? win_str(rd32(g_mem, cpu.esp + 4) + 4) : "?",
                    (int8_t)g_mem[cpu.esp + 8],
                    (int8_t)g_mem[cpu.esp + 0xc], (int16_t)rd16(g_mem, GAME_DATE), (int16_t)rd16(g_mem, GAME_DATE + 2));
        rt_jump(F_ENG_RESULT);
        return;
    }
    o_00686940();
}

/* ---- The places of each season ---- */

/* 1 when the real season of a row begins with the year (as season_sizes.c). */
static int real_starts(const SeasonRule *r, int year)
{
    char y[8];
    snprintf(y, sizeof y, "%d", year);
    return r && r->real_season && !strncmp(r->real_season, y, 4) &&
           (r->real_season[4] < '0' || r->real_season[4] > '9');
}

/* The season of the rules of a league object now, as season_sizes.c row_now: the start season at a new game and
 * with changing rules off (Q232), else the row whose real season begins with the edition +0x40. */
static int season_now(uint32_t obj, const char *file)
{
    if (!season_changing_rules() || season_change_is_creating()) return season_start_year();
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    for (int s = year - 1; s <= year; s++)
        if (real_starts(season_rules_find(s, file, 0), year)) return s;
    return year;
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

void season_playoffs_rules(uint32_t obj)
{
    if (!playoffs_on() || !plain_league(obj)) return;
    const char *file = season_change_file(obj);
    int direct = 0, season = season_now(obj, file);
    int want = season_playoffs_shape(file, season, &direct) && balanced(obj, direct);
    int have = (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) == 1;
    if (!want) {
        if (have) {                                  /* the play-off ends: no stage count, no stage */
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
    g_mem[obj + UP_PLAYOFF] = PLAYOFF_CLUBS;
    fprintf(stderr, "recomp: playoffs: %s %d: places %d to %d\n", file, season, direct + 1, direct + PLAYOFF_CLUBS);
    if (getenv("RECOMP_PLAYOFF_LOG"))                /* the reserve sides that the play-off would skip */
        for (int i = 0; i < (int16_t)rd16(g_mem, obj + COMP_CLUBS) && i < 64 && !rt_unwinding; i++)
            if (barred(obj, row_club(obj, i)))
                fprintf(stderr, "recomp: playoffs: %s %d: %.51s is a reserve side of a club above\n", file, season,
                        win_str(row_club(obj, i) + 4));
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

/* The marked leagues of an entry with a built stage and a year before the game year. 0: none; 1: all their
 * stages are complete; 2: one is not (*file gets its file). */
static int entry_playoffs(int e, int year, const char **file)
{
    uint32_t buf[COMPS_MAX];
    int n = season_change_entry_comps(e, buf, COMPS_MAX), seen = 0;
    for (int i = 0; i < n && !rt_unwinding; i++) {
        if (!marked(buf[i]) || (int32_t)rd32(g_mem, buf[i] + COMP_STAGE_NOW) < 0 ||
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

int season_playoffs_hold(void)
{
    if (!playoffs_on()) return 0;
    int today = (int16_t)rd16(g_mem, GAME_DATE), year = (int16_t)rd16(g_mem, GAME_DATE + 2), any = 0;
    for (int e = 0; e < ENTRY_COUNT && !rt_unwinding; e++) {
        uint32_t at = ENTRIES + (uint32_t)e * ENTRY_SIZE;
        int upd = (int16_t)rd16(g_mem, at + ENTRY_UPDATE_DAY);
        held[e] = 0;
        if (upd < 0 || upd >= LATE_DAY || today < upd || today - upd > WAIT_DAYS) continue;
        const char *file = "";
        int state = entry_playoffs(e, year, &file);
        if (!state) continue;
        if (today == upd && state == 2) {
            held_day[e] = (int16_t)upd;
            held[e] = 1;
            wr16(g_mem, at + ENTRY_UPDATE_DAY, 0xffffu);
            if (logged[e] != today + 1)
                fprintf(stderr, "recomp: playoffs: entry %d waits for the play-off of %s (update day %d of %d)\n", e,
                        file, upd, year);
        } else if (today > upd && (state == 1 || today - upd == WAIT_DAYS)) {
            held_day[e] = (int16_t)upd;
            held[e] = 1;
            wr16(g_mem, at + ENTRY_UPDATE_DAY, (uint16_t)today);
            if (logged[e] != today + 1)
                fprintf(stderr, "recomp: playoffs: entry %d changes its season on day %d of %d, %d days late%s\n", e,
                        today, year, today - upd, state == 2 ? " (the play-off is not complete)" : "");
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

/* ---- Rule kind "playoffs": the news ---- */

static void playoffs_after(const SeasonChange *c)
{
    if (!playoffs_on() || !c->started || c->start || !plain_league(c->comp)) return;
    int direct = 0;
    if (marked(c->comp) && season_playoffs_shape(c->file, c->season, &direct) &&
        !season_playoffs_shape(c->file, c->season - 1, NULL))
        season_change_news("The %.50s now has promotion play-offs: places %d to %d play for one place.",
                           (const char *)g_mem + rd32(g_mem, c->comp + COMP_RECORD) + 4, direct + 1,
                           direct + PLAYOFF_CLUBS);
}

const SeasonRuleKind season_kind_playoffs = {"playoffs", NULL, NULL, NULL, NULL, NULL, playoffs_after};
