/* Republic of Ireland: a reserve side does not go up from the First Division (open item 241, 0.7.2).
 *
 * The real rule, season by season. The League of Ireland plays a calendar year, and the game's season Y/Y+1 holds
 * the League of Ireland season of the year Y+1 (docs/research/rules-0.6.0/wal-nir-irl.md, "New lists"). Sources,
 * read on 14 November 2026 (notes: run-logs/0.7.2-nor-b/research-ire.md, outside the repo):
 * - 2014 (game 2013/14): Shamrock Rovers B played the First Division. 98fm, 20 December 2013: "not eligible for
 *   promotion to the Premier division" (https://www.98fm.com/sport/shamrock-rovers-b-team-to-play-in-first-division-
 *   211124); the42.ie, 9 January 2014, the same. It finished sixth of eight, so no place was handed on in real life.
 *   The club withdrew the side after the one season (en.wikipedia "Shamrock Rovers F.C. II").
 * - 2020 (game 2019/20): Shamrock Rovers II played the First Division. leagueofireland.ie, the 2020 First Division
 *   fixtures: it "cannot gain promotion, compete in the play-off series for promotion". The FAI replaced it with
 *   Treaty United for 2021. Press of 2020 counts the play-off places among the nine "eligible" clubs, so the next club
 *   of the table takes a barred place (the decision of the 0.7.2 brief).
 * - Every other season from 2001/02 to 2026/27: no reserve side played the First Division, and no source gives a
 *   rule for one. I don't know the rule there, so the game keeps its behaviour (the brief: a season with no source
 *   keeps the game). The FAI National League of 2026 is a third tier that the game does not play.
 *
 * The original. IreFirst::set_up_rules 0x635aa0 gives the First Division the Premier Division [0x9cf80c] above and
 * no league below (season_size_wraps.c wraps it for the places). League::update_positions_and_status 0x684cd0 sets
 * the row statuses: 5 the title, 0 promotion, 1 the play-off. IreFirst::advance_stage 0x635cf0 updates the Premier
 * Division when it ends (vtable +0x94 at 0x635d9a) and calls IreFirst::create_promotion_playoff_stage 0x635dc0 (at
 * 0x635dab), which pairs the First Division row of status 1 (0x635e2f..0x635e44) with the Premier Division row of
 * status 2 (0x635ed4..0x635ef4). IrePrm::start_new_season 0x63a9b0 calls the promotion chain 0x689c20 (vtable +0xb0
 * at 0x63a9c2), which moves the rows of status 0 or 5 of the First Division [0x9cf810] up and the rows of status 3 of
 * the Premier Division down, then adds 1 to the year +0x40 (0x63a9f9). Nothing tests a reserve side, and club_get_
 * linked_b_team 0x540a50 has no Irish pairs. So a reserve side that tops the table goes up, and can meet its club.
 *
 * The fix (season games, season_game_changes; the rule of season Y through season_rule_year, so with changing rules
 * off the start year's rule holds in every season). In the seasons of the rule (rule years 2013 and 2019):
 * - Before each 0x684cd0 of the First Division (the shared wrap of season_second_teams.c), an Irish reserve side gets
 *   the original's mark 0xfc (st_mark), so the next club takes the title, promotion and the play-off.
 * - Before 0x635dc0 and before the chain (a save made after the table was complete), st_shift_up hands the places on.
 * - Before the chain, a reserve side in the Premier Division (an old save, or one that went up in a season of no
 *   rule) gets status 3, and the best placed club of status 3 stays up (0xff) in its place. The chain moves rows in
 *   pairs, so with no club of status 3 the reserve side stays one more season (logged).
 * - The club moves of a new game put no Irish reserve side into the Premier Division (barred_at_start).
 * A reserve side: an Irish club (club +0x53 the nation [0x9cf2dc]) whose name passes season_clubs_reserve_name (last
 * word B, II, 2, Reserves, a word U19 or U20, ...). The rule bars every such side, so the code needs no first team;
 * the log names it (the name less its last word, case aside, among the Irish clubs). Over the 49 databases of
 * run-logs/0.7.2-nor-b/db each reserve side of the Premier or First Division finds its first team this way
 * (Shamrock Rovers B, Shamrock Rovers U19; the season lists add Shamrock Rovers II), so no pair table is needed. No
 * Irish first team of those databases has a reserve-looking name.
 *
 * Test switches (tests/seasons/ire_second_run.py): RECOMP_IRE_SECOND=off (the original), RECOMP_IRE_SECOND_LOG=1 (the
 * marks, the hand-ons, the clubs that go up and down and a census before and after each change),
 * RECOMP_TEST_IRE_SECOND_TOP=1 or 2 (before each update a reserve side of either division gets 300 more points, as
 * season_bel_second.c, or the leader's points less 1, so it tops the table or holds the play-off place),
 * RECOMP_TEST_IRE_SECOND_YEAR=<y> (the rule of y in place of the real one), RECOMP_TEST_IRE_SECOND_NO_MARK=1 (no 0xfc,
 * as a save made after the table was complete), RECOMP_TEST_IRE_SECOND_OLD=<n> (the original until n Irish season
 * changes are done, as a game of an older build), RECOMP_TEST_IRE_SECOND_PLACE=1 (with _OLD: after an original change
 * the reserve side goes up in place of the last club of the Premier Division). In a 2013 start the game's Premier
 * Division has one relegation play-off place and no direct one (the rules report: +0xc0 1, +0xc1 0), so only the
 * play-off sends a club up; a reserve side of the Premier Division then has no club of status 3 to change places with
 * and stays one more season (logged). */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEY_IRELAND 0x9cf2dcu
#define KEY_PREMIER 0x9cf80cu           /* IrePrm::create_table 0x63a779 */
#define KEY_FIRST 0x9cf810u             /* IreFirst::create_table 0x635949, IrePrm::set_up_rules 0x63a96a */
#define LEAGUE_YEAR 0x40u               /* s16, the season year; 0x63a9f9 adds 1 after the chain */
#define ROW_POINTS 0x19u                /* s16 (season_bel_second.c) */

static int old_left = -1;

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_IRE_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_IRE_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = st_env_is("RECOMP_IRE_SECOND_LOG", "1");
    return log;
}

static int reserve_side(uint32_t club)
{
    return st_readable(club) && st_nation_is(club, KEY_IRELAND) &&
           season_clubs_reserve_name(win_str(club + ST_CLUB_NAME));
}

/* 1 when the rule of season y (the game's Y of Y/Y+1) bars a reserve side from promotion. */
static int rule_bars(int y)
{
    const char *v = getenv("RECOMP_TEST_IRE_SECOND_YEAR");
    int r = v && atoi(v) > 0 ? atoi(v) : season_rule_year(y);
    return r == 2013 || r == 2019;      /* the League of Ireland seasons 2014 and 2020 */
}

static int league_year(uint32_t league)
{
    return st_readable(league) ? (int16_t)rd16(g_mem, league + LEAGUE_YEAR) : 0;
}

static int rule_on(uint32_t league)
{
    return !switched_off() && season_game_changes() && rule_bars(league_year(league));
}

/* The first team of a reserve side for the log: the Irish club named as it less its last word (case aside). */
static uint32_t first_team(uint32_t club)
{
    const char *name = win_str(club + ST_CLUB_NAME);
    size_t at = strnlen(name, 51);
    while (at && name[at - 1] == ' ') at--;
    while (at && name[at - 1] != ' ') at--;
    while (at && name[at - 1] == ' ') at--;
    if (!at) return 0;
    char want[64];
    snprintf(want, sizeof want, "%.*s", (int)at, name);
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; i < count; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
        if (c != club && st_nation_is(c, KEY_IRELAND) && !strncasecmp(win_str(c + ST_CLUB_NAME), want, 52)) return c;
    }
    return 0;
}

static void census(const char *when, int y)
{
    if (!log_on()) return;
    int count, n[2] = {0, 0}, s[2] = {0, 0};
    uint32_t table = st_clubs(&count), div[2] = {st_comp_of(KEY_PREMIER), st_comp_of(KEY_FIRST)};
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE, d = rd32(g_mem, club + ST_CLUB_DIVISION);
        for (int k = 0; k < 2; k++)
            if (d == div[k]) {
                n[k]++;
                if (reserve_side(club)) {
                    s[k]++;
                    uint32_t p = first_team(club);
                    uint32_t pd = p ? rd32(g_mem, p + ST_CLUB_DIVISION) : 0;
                    fprintf(stderr, "recomp: ire second: %.51s (first team %.51s, %.51s)\n",
                            win_str(club + ST_CLUB_NAME), p ? win_str(p + ST_CLUB_NAME) : "none",
                            st_readable(pd) ? win_str(pd + 4) : "none");
                }
            }
    }
    fprintf(stderr, "recomp: ire second: census %s (season %d, rule %s): Premier Division %d clubs, %d reserve sides; "
            "First Division %d clubs, %d reserve sides\n", when, y, rule_bars(y) ? "bars" : "keeps the game", n[0],
            s[0], n[1], s[1]);
}

/* RECOMP_TEST_IRE_SECOND_TOP=1 (tests only): the reserve sides of the table (First or Premier Division) get 300 more
 * points, so one tops the table. RECOMP_TEST_IRE_SECOND_TOP=2: each gets the points of the leader less 1, so it holds
 * the 2nd place as a rule (the play-off place of the First Division, a place far from relegation in the Premier
 * Division); the leader is the best row that is no reserve side. */
static void test_top(uint32_t league)
{
    static int mode = -1;
    if (mode < 0) {
        const char *v = getenv("RECOMP_TEST_IRE_SECOND_TOP");
        mode = v ? atoi(v) : 0;
    }
    int n;
    uint32_t table = mode ? st_table_of(league, &n) : 0;
    int best = -32768;
    for (int i = 0; table && i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        int pts = (int16_t)rd16(g_mem, row + ROW_POINTS);
        if (!reserve_side(rd32(g_mem, row)) && pts > best) best = pts;
    }
    for (int i = 0; table && i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        int pts = (int16_t)rd16(g_mem, row + ROW_POINTS);
        if (!reserve_side(rd32(g_mem, row))) continue;
        if (mode == 1 && pts < 150) wr16(g_mem, row + ROW_POINTS, (uint16_t)(pts + 300));
        if (mode == 2 && best > 0) wr16(g_mem, row + ROW_POINTS, (uint16_t)(best - 1));
    }
}

/* RECOMP_TEST_IRE_SECOND_PLACE=1 (tests only, with RECOMP_TEST_IRE_SECOND_OLD): after the chain of an original change,
 * each reserve side of the First Division goes up (League::move_club_to_division 0x6830b0) and the lowest placed club
 * of the Premier Division's table that the chain left there goes down (League::relegate_club_to_division 0x6831a0), as a long game of an older
 * build in which the reserve side won the play-off. */
static void test_place(uint32_t prm_league)
{
    static int on = -1;
    if (on < 0) on = st_env_is("RECOMP_TEST_IRE_SECOND_PLACE", "1");
    if (!on || old_left <= 0) return;
    int count, n;
    uint32_t table = st_clubs(&count), prm = st_comp_of(KEY_PREMIER), first = st_comp_of(KEY_FIRST);
    uint32_t rows = st_table_of(prm_league, &n);
    for (int i = 0; i < count && n > 0 && !rt_unwinding; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (rd32(g_mem, club + ST_CLUB_DIVISION) != first || !reserve_side(club)) continue;
        uint32_t low = 0;              /* the lowest placed club of the table still in the Premier Division */
        while (n > 0 && !low) {
            uint32_t c = rd32(g_mem, rows + (uint32_t)(--n) * ST_ROW_SIZE);
            if (st_readable(c) && rd32(g_mem, c + ST_CLUB_DIVISION) == prm) low = c;
        }
        if (!low) break;
        uint32_t a[3] = {club, prm, 1}, b[3] = {low, first, 1};
        st_call_this(0x006830b0u, prm_league, 3, a);
        st_call_this(0x006831a0u, prm_league, 3, b);
        fprintf(stderr, "recomp: ire second: test: %.51s goes up in place of %.51s\n", win_str(club + ST_CLUB_NAME),
                win_str(low + ST_CLUB_NAME));
    }
}

static void ire_before_status(uint32_t league)
{
    static int no_mark = -1;
    if (no_mark < 0) no_mark = st_env_is("RECOMP_TEST_IRE_SECOND_NO_MARK", "1");
    if (league == st_league_of(KEY_PREMIER) && season_game_changes()) test_top(league);
    if (league != st_league_of(KEY_FIRST) || !season_game_changes()) return;
    test_top(league);
    if (!no_mark && rule_on(league)) st_mark(league, reserve_side, "ire second", "the First Division", log_on());
}

/* IreFirst::create_promotion_playoff_stage 0x635dc0, thiscall, no arguments. */
void o_00635dc0(void);
RT_WRAP(00635dc0, ire_second_playoff)
{
    uint32_t self = cpu.ecx;
    if (self == st_league_of(KEY_FIRST) && rule_on(self))
        st_shift_up(self, reserve_side, "ire second", "before the play-off", log_on());
    if (self == st_league_of(KEY_FIRST) && log_on()) {
        int n;
        uint32_t table = st_table_of(self, &n);
        for (int i = 0; i < n; i++) {
            uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
            if (g_mem[row + ST_ROW_STATUS] == ST_PLAYOFF)
                fprintf(stderr, "recomp: ire second: the play-off takes %.51s of the First Division%s\n",
                        win_str(club + ST_CLUB_NAME), reserve_side(club) ? " (a reserve side)" : "");
        }
    }
    o_00635dc0();
}

/* The rows of a table with status 5, 0 or 1 (the places up), for the log. */
static void log_places(uint32_t league, const char *what)
{
    if (!log_on()) return;
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        uint8_t s = g_mem[row + ST_ROW_STATUS];
        if (s == ST_TITLE || s == ST_UP || s == ST_PLAYOFF || s == ST_BARRED)
            fprintf(stderr, "recomp: ire second: %s position %d: %.51s status %d%s\n", what, i + 1,
                    win_str(rd32(g_mem, row) + ST_CLUB_NAME), s == ST_BARRED ? -4 : s,
                    reserve_side(rd32(g_mem, row)) ? " (a reserve side)" : "");
    }
}

static char *was_prm;
static int was_count;

static void ire_before_chain(uint32_t self)
{
    free(was_prm);                       /* left by a chain that unwound (no after_chain) */
    was_prm = NULL;
    if (self != st_league_of(KEY_PREMIER) || !season_game_changes()) return;
    int y = league_year(self);
    census("before", y);
    uint32_t first = st_league_of(KEY_FIRST);
    int count;
    uint32_t table = st_clubs(&count), prm = st_comp_of(KEY_PREMIER);
    was_prm = log_on() && count > 0 ? calloc((size_t)count, 1) : NULL;
    was_count = count;
    for (int i = 0; was_prm && i < count; i++)
        was_prm[i] = rd32(g_mem, table + (uint32_t)i * ST_CLUB_SIZE + ST_CLUB_DIVISION) == prm;
    if (rule_on(self)) {
        if (first) st_shift_up(first, reserve_side, "ire second", "at the season change", log_on());
        /* A reserve side of the Premier Division goes down in exchange for the best placed club of status 3. */
        int n;
        uint32_t rows = st_table_of(self, &n);
        for (int i = n - 1; i >= 0; i--) {
            uint32_t row = rows + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
            if (g_mem[row + ST_ROW_STATUS] == ST_DOWN || !reserve_side(club)) continue;
            int stay = -1;
            for (int k = 0; k < n && stay < 0; k++) {
                uint32_t r = rows + (uint32_t)k * ST_ROW_SIZE;
                if (g_mem[r + ST_ROW_STATUS] == ST_DOWN && !reserve_side(rd32(g_mem, r))) stay = k;
            }
            if (stay < 0) {
                if (log_on()) fprintf(stderr, "recomp: ire second: %.51s stays in the Premier Division one more "
                                      "season (no place goes down for it)\n", win_str(club + ST_CLUB_NAME));
                continue;
            }
            uint32_t up = rows + (uint32_t)stay * ST_ROW_SIZE;
            g_mem[up + ST_ROW_STATUS] = ST_NONE;
            g_mem[row + ST_ROW_STATUS] = ST_DOWN;
            if (log_on())
                fprintf(stderr, "recomp: ire second: %.51s goes down from the Premier Division, %.51s stays up\n",
                        win_str(club + ST_CLUB_NAME), win_str(rd32(g_mem, up) + ST_CLUB_NAME));
        }
    }
    if (first) log_places(first, "the First Division");
}

static void ire_after_chain(uint32_t self)
{
    if (self != st_league_of(KEY_PREMIER) || !season_game_changes()) return;
    int count;
    uint32_t table = st_clubs(&count), prm = st_comp_of(KEY_PREMIER);
    for (int i = 0; was_prm && i < was_count && i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        int now = rd32(g_mem, club + ST_CLUB_DIVISION) == prm;
        if (now != was_prm[i])
            fprintf(stderr, "recomp: ire second: %.51s goes %s the Premier Division%s\n", win_str(club + ST_CLUB_NAME),
                    now ? "up to" : "down from", reserve_side(club) ? " (a reserve side)" : "");
    }
    free(was_prm);
    was_prm = NULL;
    if (!rt_unwinding) test_place(self);
    census("after", league_year(self));
    if (old_left > 0) old_left--;
}

static int ire_barred(uint32_t club, uint32_t div)
{
    if (switched_off() || !reserve_side(club) || !st_readable(div)) return 0;
    if ((int32_t)rd32(g_mem, div) != (int32_t)rd32(g_mem, KEY_PREMIER)) return 0;
    return rule_bars(st_season_year(KEY_PREMIER));
}

static const SecondTeamHooks ire_hooks = {ire_before_status, ire_before_chain, ire_after_chain, ire_barred};
SEASON_SECOND_HOOKS(ire_hooks)
