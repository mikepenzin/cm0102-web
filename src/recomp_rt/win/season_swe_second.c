/* Sweden: no second team or combined team goes up into the Second Division (open item 244, 0.7.2).
 *
 * The real rule. SvFF Tävlingsbestämmelser, 2 kap. 16 §: "Endast representationslag får delta i förbundsserierna
 * eller i kval till förbundsserierna" (a representationslag is a club's senior A team), and 15 §: combined teams
 * ("kombinerade lag") take no part in the förbundstävlingar. The förbundsserier are Allsvenskan to Division 3 for
 * men, so they hold all three divisions that the game plays. Sources, read on 14 November 2026 (notes:
 * run-logs/0.7.2-nor-b/research-swe.md, outside the repo):
 * - TB 2026 (svenskfotboll.se, tb-2026-v2-202603242.pdf), TB 2024 (tb-2024-v1-231207.pdf), TB 2023 (Stockholm copy)
 *   and TB 2022 (lawinsider copy, in force from 26 November 2021): 2 kap. 15 § and 16 § as above;
 * - TB 2016 (Västerbotten copy, in force from 27 November 2015): 10.3, the same sentence;
 * - TB 2010 (club copy on laget.se, in force from 28 November 2009): 10.2 names the förbundsserier "förbundsserier för
 *   representationslag", 10.3 bars combined teams from them and from the district level that qualifies for them.
 * So the rule holds in the Swedish seasons 2010 and 2016 onwards (the text of 2026 is the latest; later seasons keep
 * it). For 2001 to 2009 and 2011 to 2015 no text was found: I don't know, and the game keeps the original's
 * behaviour in those seasons. The rules name no club that takes a barred place, so the next club of the original's
 * list takes it. Örebro Ungdom, Helsingborgs IF Akademi, Umeå FC Akademi and IF Karlstad Fotbollutveckling are
 * clubs of their own (Örebro SK Ungdom is "a separate legal entity", en.wikipedia; HIF Akademi was Ramlösa Södra FF),
 * so 16 § does not touch them and this file leaves them alone.
 *
 * The original. The Second Division (SweSecond, the extra division of Sweden) is played only with the extra
 * division flag. SwePrm's constructor 0x88fb10 puts every Swedish club outside the top two divisions and the six
 * group records (0x9cf5f4..0x9cf608) in the Lower Division [0x9cf6ac]. SweSecond::create_relegation_play_off_stage
 * 0x892bb0 (thiscall, this the Second Division league, the North group) lists the clubs of nation [0x9cf4a8]
 * (club +0x53, 0x892c59..0x892c63) whose division (club +0x57) is the Lower Division (0x892c4f..0x892c57; a club with
 * no division is not listed, 0x892c4b), sorts and shuffles 24 of them by reputation (0x4afbd0 at 0x892d11), keeps the
 * first 12 at league +0xee (0x892d19..0x892d39) and puts the next 12 in the relegation play-off, stage 6 (0x522e00 at
 * 0x892e23, [league +0xc] +0x18 at 0x892e3a), with a row of status 2 of each group (0x892d54..0x892dc0). At the
 * season start SwePrm 0x8905f0 calls 0x890b40 (at 0x89061d; 0x890980 when the Second Division is not played, which
 * takes no Lower Division club). 0x890b40 moves the winners of stage 6 that are not in the Second Division up
 * (0x6830b0 at 0x890b8c), the rows of status 3 of the six groups down to the Lower Division (0x6831a0 at 0x890bfb)
 * and the 12 clubs of +0xee up (0x6830b0 at 0x890c51). From the groups the play-off of stage 5 leads to Superettan.
 * The original's own 0xfc marks of the groups (0x8922d4 in 0x8921b0, 0x8929de in 0x892670) follow
 * club_get_linked_b_team 0x540a50 (0x8922a8, 0x8929be), which holds pairs of Spain, Germany and Portugal only, so
 * they never mark a Swedish club. Nothing tests a Swedish second team.
 *
 * The data. Over the 49 season databases the Swedish second and combined teams are Friska Viljor FC Lag 2 (23
 * databases, October 2003 to March 2013 and the 3.9.60, 3.9.65 and 3.9.68 originals), Friska Viljor FC 2/Arnäs IF
 * (30) and Gävle GIK/Sätra 2 (October 2017 to March 2019), all in the Lower Division. No Swedish first team has a
 * second-team name.
 *
 * The fix (season games, season_game_changes; the rule year through season_rule_year, so with changing rules off
 * the rule of the start year holds in every season):
 * - In the wrap of 0x892bb0, a barred club (below) of the Lower Division reads as having no division for the call, so
 *   it is no part of the list and the next club by reputation takes its place; then it gets its division back. The
 *   rule of the season played (league +0x40, read at 0x892e00) holds, as the play-off is played in it.
 * - Before League::update_positions_and_status 0x684cd0 of each group (the league and [league +0xc][0..4]), a barred
 *   row gets 0xfc (season_second_teams.c st_mark), so it takes no place of promotion or of the play-off to Superettan.
 * - Old saves (a game of an older build can hold a barred club in a group): in the wrap of 0x890b40, when the rule of
 *   the season that starts bars it, each barred row of a group gets status 3, so 0x890b40 sends it down to the Lower
 *   Division, and the best placed club of status 3 of the same group gets 0xff and stays up, so the group keeps its
 *   size. A group with no such club loses one club (swe_second_groups.c takes a group of fewer clubs; logged).
 *   Not handled: a barred club in Superettan or Allsvenskan (an older build had to take it through both the pick and
 *   the play-off of stage 5).
 * - The club moves of a new game (season_clubs.c) put no barred club in Allsvenskan [0x9cf5ec], Superettan
 *   [0x9cf5f0], the Second Division [0x9cf7c8] or its six group records (season_swe_barred, through
 *   season_second_barred), when the rule of the start year bars it.
 * A barred club: nation Sweden and a second-team name (season_clubs_reserve_name: a last word 2, II or B and the other
 * reserve forms), or a name of the COMBINED table. Its first team (for the log and the names check only, as the rule
 * bars every second team): a pair of the PAIRS table, else the Swedish club named as the second team less its last
 * word (case aside). Every case of the 49 databases finds one this way (run-logs/0.7.2-nor-b/swe/names_check.py).
 *
 * The 3.9.68 base game (start 2001, changing rules off) is unchanged: season_game_changes is 0. Switches:
 * RECOMP_SWE_SECOND=off (test switch) keeps the original. RECOMP_SWE_SECOND_LOG=1 logs the clubs held out, the picks,
 * the clubs that go up and down, and a count of the barred clubs of the Second Division at each season start.
 * RECOMP_TEST_SWE_SECOND_REP=1 (tests only) gives each barred club of the Lower Division the highest reputation before
 * 0x892bb0, so the original would pick it. RECOMP_TEST_SWE_SECOND_OLD=<n> (tests only) plays as the original until n
 * Swedish season starts (0x890b40) are done, as a game of an older build. RECOMP_TEST_SWE_SECOND_YEAR=<y> (tests only)
 * takes the rule of year y. RECOMP_TEST_SWE_SECOND_HOLD=1 (tests only): before each update each barred club of a group
 * gets the points of the middle club, so an old save's barred club is still in the Second Division at the next start. */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_second_teams.h"
#include "season_swe_groups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEY_SWEDEN 0x9cf4a8u
#define KEY_PREMIER 0x9cf5ecu           /* Allsvenskan */
#define KEY_FIRST 0x9cf5f0u             /* Superettan */
#define KEY_GROUP1 0x9cf5f4u            /* the six group records, 0x9cf5f4..0x9cf608 */
#define GROUPS 6
#define KEY_SECOND 0x9cf7c8u            /* the Second Division record (SweSecond) */
#define KEY_LOWER 0x9cf6acu             /* the Swedish Lower Division */
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE ST_CLUB_SIZE
#define CLUB_NAME ST_CLUB_NAME
#define CLUB_DIVISION ST_CLUB_DIVISION
#define CLUB_REPUTATION ST_CLUB_REPUTATION
#define LEAGUE_YEAR 0x40u
#define LEAGUE_STAGES 0x0cu             /* the stage table: [0..4] the other five groups, [6] the relegation play-off */
#define LEAGUE_STAGE_NOW 0x30u          /* s32: the last stage made */
#define LEAGUE_PICKS 0xeeu              /* 12 club indices that go up (0x892d19..0x892d39, 0x890c12..0x890c5a) */
#define CUP_COUNT 0x3eu
#define CUP_ENTRIES 0xa7u               /* 6 bytes an entry, +0 the club (0x890b6d..0x890b7c) */

/* Combined teams (TB 15 §) whose name is no second-team name, and the first team of each second or combined team
 * whose name less its last word names none. Windows-1252. */
static const char *const COMBINED[] = {"Friska Viljor FC 2/Arn\xe4s IF"};
static const char *const PAIRS[][3] = {
    {"Friska Viljor FC Lag 2", "Friska Viljor FC", "Friska Viljor"},
    {"Friska Viljor FC 2/Arn\xe4s IF", "Friska Viljor FC", "Friska Viljor"},
    {"G\xe4vle GIK/S\xe4tra 2", "G\xe4vle GIK", "IK S\xe4tra"},
};

static int old_left = -1;

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_SWE_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_SWE_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = st_env_is("RECOMP_SWE_SECOND_LOG", "1");
    return log;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

/* The year whose rule holds in Swedish season y. */
static int rule_year_of(int y)
{
    const char *v = getenv("RECOMP_TEST_SWE_SECOND_YEAR");
    if (v && atoi(v) > 0) return atoi(v);
    return season_rule_year(y);
}

/* 1 when the rule of year y bars second and combined teams (TB 2010, TB 2016 to 2026; other years: I don't know). */
static int year_bars(int y) { return y == 2010 || y >= 2016; }

static int swedish(uint32_t club) { return st_nation_is(club, KEY_SWEDEN); }

static int barred_name(const char *name)
{
    if (season_clubs_reserve_name(name)) return 1;
    for (size_t i = 0; i < sizeof COMBINED / sizeof *COMBINED; i++)
        if (!strncmp(name, COMBINED[i], 52)) return 1;
    return 0;
}

static int barred_club(uint32_t club)
{
    return st_readable(club) && swedish(club) && barred_name(win_str(club + CLUB_NAME));
}

/* The first team of a barred club (0 when none is found): log and names check only. */
static uint32_t first_team(uint32_t club)
{
    const char *name = win_str(club + CLUB_NAME);
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!st_readable(table)) return 0;
    for (size_t r = 0; r < sizeof PAIRS / sizeof *PAIRS; r++) {
        if (strncmp(name, PAIRS[r][0], 52)) continue;
        for (int k = 1; k < 3; k++)
            for (int i = 0; i < count; i++) {
                uint32_t c = table + (uint32_t)i * CLUB_SIZE;
                if (swedish(c) && !strncmp(win_str(c + CLUB_NAME), PAIRS[r][k], 52)) return c;
            }
        return 0;
    }
    size_t at = strnlen(name, 51);
    while (at && name[at - 1] == ' ') at--;
    while (at && name[at - 1] != ' ') at--;
    while (at && name[at - 1] == ' ') at--;
    if (!at) return 0;
    char want[64];
    snprintf(want, sizeof want, "%.*s", (int)at, name);
    for (int i = 0; i < count; i++) {
        uint32_t c = table + (uint32_t)i * CLUB_SIZE;
        if (c != club && swedish(c) && !strncasecmp(win_str(c + CLUB_NAME), want, 52)) return c;
    }
    return 0;
}

static const char *first_name(uint32_t club)
{
    uint32_t p = first_team(club);
    return p ? win_str(p + CLUB_NAME) : "no first team";
}

/* 1 when the division record div is that of the key. */
static int div_is(uint32_t div, uint32_t key)
{
    return st_readable(div) && (int32_t)rd32(g_mem, div) == (int32_t)rd32(g_mem, key);
}

static int top_division(uint32_t div)
{
    if (div_is(div, KEY_PREMIER) || div_is(div, KEY_FIRST) || div_is(div, KEY_SECOND)) return 1;
    for (uint32_t k = 0; k < GROUPS; k++)
        if (div_is(div, KEY_GROUP1 + 4u * k)) return 1;
    return 0;
}

int season_swe_barred(uint32_t club, uint32_t div)
{
    if (switched_off() || !barred_club(club) || !top_division(div)) return 0;
    return year_bars(rule_year_of(st_season_year(KEY_PREMIER)));
}

/* The six group stages of the Second Division league: the league (North) and [league +0xc][0..4]. */
static int group_stages(uint32_t league, uint32_t out[GROUPS])
{
    if (!st_readable(league)) return 0;
    int n = 0;
    out[n++] = league;
    uint32_t stages = rd32(g_mem, league + LEAGUE_STAGES);
    int last = (int32_t)rd32(g_mem, league + LEAGUE_STAGE_NOW);     /* issue 106: two groups hold one stage */
    for (uint32_t k = 0; st_readable(stages) && k < 5 && (int)k <= last; k++) {
        uint32_t s = rd32(g_mem, stages + 4u * k);
        if (st_readable(s)) out[n++] = s;
    }
    return n;
}

static int season_of(uint32_t league)
{
    return st_readable(league) ? (int16_t)rd16(g_mem, league + LEAGUE_YEAR) : 0;
}

/* The barred clubs of the Second Division, and its clubs. */
static void log_census(const char *when)
{
    if (!log_on()) return;
    uint32_t table = rd32(g_mem, CLUBS), second = st_comp_of(KEY_SECOND), lower = st_comp_of(KEY_LOWER);
    int count = st_readable(table) ? (int)rd32(g_mem, CLUB_COUNT) : 0, clubs = 0, barred = 0, low = 0;
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
        if (div == second) {
            clubs++;
            barred += barred_club(club);
        } else if (div == lower) {
            low += barred_club(club);
        }
    }
    fprintf(stderr, "recomp: swe second: census %s: Second Division %d clubs, %d barred; Lower Division %d barred\n",
            when, clubs, barred, low);
}

/* The picks of 0x892bb0: the 12 of league +0xee and the Lower Division clubs of stage 6. */
static void log_picks(uint32_t league)
{
    uint32_t table = rd32(g_mem, CLUBS), lower = st_comp_of(KEY_LOWER);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (uint32_t k = 0; k < 12; k++) {
        uint32_t i = rd32(g_mem, league + LEAGUE_PICKS + 4u * k);
        if ((int)i < 0 || (int)i >= count) continue;
        uint32_t club = table + i * CLUB_SIZE;
        fprintf(stderr, "recomp: swe second: pick %.51s (up)%s\n", win_str(club + CLUB_NAME),
                barred_club(club) ? " (a second team)" : "");
    }
    uint32_t stages = rd32(g_mem, league + LEAGUE_STAGES);
    uint32_t cup = st_readable(stages) ? rd32(g_mem, stages + 0x18u) : 0;
    if (!st_readable(cup)) return;
    int n = (int16_t)rd16(g_mem, cup + CUP_COUNT);
    uint32_t e = rd32(g_mem, cup + CUP_ENTRIES);
    for (int i = 0; st_readable(e) && i < n && i < 64; i++) {
        uint32_t club = rd32(g_mem, e + 6u * (uint32_t)i);
        if (!st_readable(club) || rd32(g_mem, club + CLUB_DIVISION) != lower) continue;
        fprintf(stderr, "recomp: swe second: pick %.51s (play-off)%s\n", win_str(club + CLUB_NAME),
                barred_club(club) ? " (a second team)" : "");
    }
}

void o_00892bb0(void);
RT_WRAP(00892bb0, swe_second_picks)
{
    static int test_rep = -1;
    if (test_rep < 0) test_rep = st_env_is("RECOMP_TEST_SWE_SECOND_REP", "1");
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!season_game_changes() || !st_readable(table) || count <= 0) {
        o_00892bb0();
        return;
    }
    uint32_t self = cpu.ecx, lower = st_comp_of(KEY_LOWER);
    int season = season_of(self), year = rule_year_of(season), bar = rule_on() && year_bars(year);
    int *held = malloc(sizeof *held * (size_t)count), nheld = 0;
    for (int i = 0; held && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, club + CLUB_DIVISION) != lower || !barred_club(club)) continue;
        if (test_rep) wr16(g_mem, club + CLUB_REPUTATION, 10000);
        if (!bar) {
            if (log_on())
                fprintf(stderr, "recomp: swe second: %.51s may be picked (season %d, rule of %d%s)\n",
                        win_str(club + CLUB_NAME), season, year, rule_on() ? ": no bar known" : ": switch off");
            continue;
        }
        held[nheld++] = i;
        wr32(g_mem, club + CLUB_DIVISION, 0);
    }
    o_00892bb0();
    for (int k = 0; k < nheld; k++) {
        uint32_t club = table + (uint32_t)held[k] * CLUB_SIZE;
        wr32(g_mem, club + CLUB_DIVISION, lower);
        if (log_on())
            fprintf(stderr, "recomp: swe second: %.51s stays out of the picks (season %d, rule of %d; first team "
                    "%.51s)\n", win_str(club + CLUB_NAME), season, year, first_name(club));
    }
    free(held);
    if (log_on() && !rt_unwinding) log_picks(self);
}

/* Old saves: before 0x890b40, each barred row of a group gets status 3 and the best placed club of status 3 of the
 * group stays up. */
static void season_down(uint32_t league, int season, int year)
{
    uint32_t second = st_comp_of(KEY_SECOND);
    uint32_t st[GROUPS];
    int ns = group_stages(league, st);
    for (int s = 0; s < ns; s++) {
        int n;
        uint32_t table = st_table_of(st[s], &n);
        for (int i = n - 1; i >= 0; i--) {
            uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
            /* 0x890b40 sends every row of status 3 down with no division test (0x890bd8..0x890bfb): only a club
             * that still plays the Second Division (not one the chain 0x890602 just moved up) gets 3. */
            if (g_mem[row + ST_ROW_STATUS] == ST_DOWN || !barred_club(club) ||
                rd32(g_mem, club + CLUB_DIVISION) != second)
                continue;
            g_mem[row + ST_ROW_STATUS] = ST_DOWN;
            int kept = -1;
            for (int j = 0; j < n; j++) {
                uint32_t r = table + (uint32_t)j * ST_ROW_SIZE;
                if (g_mem[r + ST_ROW_STATUS] == ST_DOWN && !barred_club(rd32(g_mem, r))) {
                    g_mem[r + ST_ROW_STATUS] = ST_NONE;
                    kept = j;
                    break;
                }
            }
            if (log_on()) {
                fprintf(stderr, "recomp: swe second: %.51s goes down from the Second Division (season %d, rule of "
                        "%d)\n", win_str(club + CLUB_NAME), season, year);
                if (kept >= 0)
                    fprintf(stderr, "recomp: swe second: %.51s stays up in its group\n",
                            win_str(rd32(g_mem, table + (uint32_t)kept * ST_ROW_SIZE) + CLUB_NAME));
                else
                    fprintf(stderr, "recomp: swe second: no club of its group stays up for it\n");
            }
        }
    }
}

void o_00890b40(void);
RT_WRAP(00890b40, swe_second_season)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!season_game_changes() || !st_readable(table) || count <= 0) {
        o_00890b40();
        return;
    }
    uint32_t league = st_league_of(KEY_SECOND), second = st_comp_of(KEY_SECOND);
    int season = season_of(league), year = rule_year_of(season + 1);
    log_census("before");
    if (rule_on() && year_bars(year)) season_down(league, season, year);
    if (season_swe_groups_down(cpu.ecx)) {             /* issue 106: create_table's deal fills the places */
        log_census("after");
        if (!rt_unwinding) {
            if (old_left > 0) old_left--;
            rt_ret_stdcall(0, 0);
        }
        return;
    }
    char *was = log_on() ? calloc((size_t)count, 1) : NULL;
    for (int i = 0; was && i < count; i++) was[i] = rd32(g_mem, table + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION) == second;
    o_00890b40();
    if (rt_unwinding) {
        free(was);
        return;
    }
    for (int i = 0; was && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (!was[i] && rd32(g_mem, club + CLUB_DIVISION) == second)
            fprintf(stderr, "recomp: swe second: %.51s goes up to the Second Division%s\n", win_str(club + CLUB_NAME),
                    barred_club(club) ? " (a second team)" : "");
    }
    free(was);
    log_census("after");
    if (old_left > 0) old_left--;
}

/* Before 0x684cd0 (season_second_teams.c): the barred rows of the six groups get 0xfc. */
static void swe_before_status(uint32_t league)
{
    static int hold = -1;
    if (hold < 0) hold = st_env_is("RECOMP_TEST_SWE_SECOND_HOLD", "1");
    if (!season_game_changes()) return;
    uint32_t second = st_league_of(KEY_SECOND), st[GROUPS];
    int ns = group_stages(second, st);
    for (int s = 0; s < ns; s++) {
        if (league != st[s]) continue;
        if (hold) st_test_mid_table(league, barred_club);
        if (rule_on() && year_bars(rule_year_of(season_of(second))))
            st_mark(league, barred_club, "swe second", "the Second Division", log_on());
    }
}

static const SecondTeamHooks swe_hooks = {swe_before_status, NULL, NULL, season_swe_barred};
SEASON_SECOND_HOOKS(swe_hooks)
