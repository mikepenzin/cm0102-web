/* Greece: the B sides of Super League clubs follow the real rule (open item 242, 0.7.2).
 *
 * The real rule by season. Notes with the sources: run-logs/0.7.2-nor-b/research-gre.md (outside the repo), read on
 * 14 November 2026. The primary texts (the EPO "Kanonismos Omadon B" of 2021/22 and the Super League 2 notices) did
 * not open (a Cloudflare check, or no copy found), so the sources are the press of 2021 and en.wikipedia's season
 * pages of Super League Greece 2.
 * - 2001/02 to 2020/21: no B side played the national divisions. en.wikipedia "Super League Greece 2" counts the
 *   seasons of each club in the second tier from 1959/60: PAOK B 6, Olympiacos B 5, AEK Athens B 4, Panathinaikos B 3,
 *   Asteras Tripolis B 3, which are exactly their seasons from 2021/22 on. The 2021 press calls the B sides new
 *   (naftemporiki.gr, 9 July 2021: the EPO approved their entry into Super League 2). So a B side may play neither
 *   tier.
 * - From 2021/22: the B sides of Super League clubs play Super League 2 by the decision of the EPO (9 July 2021),
 *   not by results. They may not go up: "Promotion to the division of the first team will not exist" (tovima.gr,
 *   14 May 2021), and every season page from 2021/22 to 2026/27 marks them "ineligible for promotion". The next club
 *   of the table takes the place (the season pages: the group winners went up).
 * - 2021/22 and 2022/23: "B Teams are ineligible for promotion and relegation" (en.wikipedia, both seasons).
 * - From 2023/24 a B side goes down by the table (Olympiacos B and Panathinaikos B in 2024, AEK Athens B in 2025).
 * I don't know: what happens to a B side when its first team comes down into Super League 2 (no case found); the
 * extra relegation of 2024/25 ("if a reserve team (B) was relegated, an other team from the same group was
 * relegated", en.wikipedia 2024/25), and the four-side cap of the 2021 proposal. Those keep the game's behaviour.
 *
 * The original. GrePrm::start_new_season 0x5eaf50 (thiscall, this the National A Division) calls the promotion chain
 * (vtable +0xb0 = 0x689c20, at 0x5eaf62: the rows of status 0 or 5 of the National B Division up, the rows of status
 * 3 of the National A Division down, in pairs), then gre_prm_swap_second_and_third_division_clubs 0x5eb120 (at
 * 0x5eaf6a), adds 1 to its year +0x40 (0x5eafb0), runs set_up_rules (vtable +0x8c, 0x5eafc3) and GrePrm::create_table
 * 0x5ead60 (at 0x5eb001; 14 rows, malloc(0x33a) at 0x5ead78), and at its end starts the National B Division (vtable
 * +8, 0x5eb0f7). 0x5eb120 lists every club whose division (club +0x57) is the Greek Lower Division [0x9cf874]
 * (0x5eb1d7..0x5eb1ee, no nation test), sorts the list by reputation and shuffles the top 3 (0x4afbd0 at 0x5eb20f),
 * moves the first 2 into the National B Division [0x9cf870] (League::move_club_to_division 0x6830b0 at 0x5eb23e), then
 * sends the rows of status 3 of the National B Division's table to the Lower Division (0x5eb25d..0x5eb28a). With
 * fewer than 2 clubs in the list it moves nothing (0x5eb201). The season code wraps 0x5eb120 (gre_swap in
 * season_divisions.c): while the second group of the second tier plays (2013/14, 2014/15, 2021/22 to 2025/26) it
 * promotes the group's winners (0x689c80), fills the group from the Lower Division by the same sort (gre_fill) and
 * sends the group's rows of status 3 to the Lower Division. Nothing tests a B side, and club_get_linked_b_team
 * 0x540a50 holds no Greek pair. The databases from October 2021 hold AEK Athens B, Olympiacos Piraeus B, PAOK
 * Thessaloniki B, Panathinaikos (Athens) B and Asteras Aktor B in the Lower Division, so a B side with a high
 * reputation enters the second tier and goes up into the Super League by the table.
 *
 * The fix (season games, season_game_changes; the base game 3.9.68 has no Greek B side).
 * - A B side: a Greek club (club +0x53 the nation [0x9cf31c]) whose name passes season_clubs_reserve_name (last word
 *   B, II, 2 and the other reserve forms). Its first team: the Greek club named as the B side less its last word (case
 *   aside), else the pair of PARENTS. No known Greek first team has a reserve name (the check over the databases).
 * - During the season: before each 0x684cd0 (League::update_positions_and_status, the shared wrap of
 *   season_second_teams.c) of the National B Division or the group, a B side gets the original's mark 0xfc, so the next
 *   club takes the title, promotion and the play-off (0x684dea..0x68518b).
 * - Before the chain (GrePrm only): when a save comes from the days after the table was complete, st_shift_up hands
 *   the places on, in the National B Division and the group (the group goes up later, in gre_swap). In 2021/22 and
 *   2022/23 a B side of status 3 in the second tier gets 0xff and stays, and the lowest placed other club that has no
 *   place gets 3 and goes down in its place. A B side in the Super League (a save of an older build) gets 3 and the
 *   best placed club of status 3 stays up (0xff); the chain moves a row down only with a partner that goes up, so a B
 *   side with no such club left stays one more season (logged). When the season that starts is before 2021/22, a B
 *   side in the second tier (a save of an older build, or the test year) goes down to the Lower Division in the same
 *   way (0x5eb120 at 0x5eb25d and gre_swap send the rows of status 3 there).
 * - After the chain, before 0x5eb120: each B side of the Lower Division that the rule of the season that starts bars
 *   from the second tier (to 2020/21 every B side; from 2021/22 a B side whose first team, after the chain, is not in
 *   the Super League; a B side with no first team found may enter, as the game) reads as no division, so neither
 *   0x5eb120 nor gre_fill lists it and the next club of the sorted list takes its place (as the original's own rule for
 *   Spain, 0x8504c9..0x8504ec). The wrap of GrePrm::create_table 0x5ead60 (Greek only; callers 0x5ea4c3 in the
 *   constructor and 0x5eb001) gives each its division back, after 0x5eb120 and the group's moves.
 * - The club moves of a new game (season_clubs.c) put no B side into the Super League, nor into the second tier before
 *   2021/22 (barred_at_start). The second tier from 2021/22 has no first-team test at a new game.
 * Not covered (needs a change in season_divisions.c, owned by another file): gre_make fills a group that the module
 * makes at a season change (2013 and 2021 with changing rules on) before GrePrm's start, so with no hide. No database
 * before October 2021 has a Greek B side, so the case of 2013 cannot come about.
 *
 * The rule year: season_rule_year of the season played (GrePrm's year +0x40 before 0x5eafb0) for the marks and the
 * relegation; of the season that starts (+1) for the entry. With changing rules off, the start year's rule.
 *
 * Switches. RECOMP_GRE_SECOND=off (test switch): the original. RECOMP_GRE_SECOND_LOG=1: the clubs held, the places
 * handed on, who goes up and down, and a census before and after each change. Tests only: RECOMP_TEST_GRE_SECOND_REP=1
 * (each B side of the Lower Division gets the highest reputation before 0x5eb120), RECOMP_TEST_GRE_SECOND_YEAR=<y> (the
 * rule of y), RECOMP_TEST_GRE_SECOND_UP=1 (the best placed B side of each second-tier table takes a promotion status
 * before the chain), RECOMP_TEST_GRE_SECOND_DOWN=1 (the lowest placed B side of each second-tier table takes status 3
 * before the chain), RECOMP_TEST_GRE_SECOND_OLD=<n> (the original until n Greek season changes are done, as a game of
 * an older build). */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_comps.h"
#include "season_names.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEY_GREECE 0x9cf31cu            /* season_nation_table.c */
#define KEY_PRM 0x9cf86cu               /* the National A Division (Super League) */
#define KEY_SECOND 0x9cf870u            /* the National B Division (0x5eb12f, 0x5eb21e) */
#define KEY_LOWER 0x9cf874u             /* the Greek Lower Division (0x5eb1e0) */
#define LEAGUE_YEAR 0x40u               /* s16; 0x5eafb0 adds 1 after 0x5eb120 */
#define OBJ_RECORD 4u
#define TAG "gre second"

extern const SeasonCompDef season_div_gre_group;    /* season_divisions.c: the second group of the second tier */

/* B sides whose first team the name less its last word does not give (database name, first team names). */
static const char *const PARENTS[][3] = {
    {"Panathinaikos B", "Panathinaikos FC", "Panathinaikos Athens"},    /* April 2024, October 2024 */
};

static int log_on(void)
{
    static int v = -1;
    if (v < 0) v = st_env_is("RECOMP_GRE_SECOND_LOG", "1");
    return v;
}

/* RECOMP_TEST_GRE_SECOND_OLD=<n>: the Greek season changes left to play as the original. Read once. */
static int old_left = -1;

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_GRE_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_GRE_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

/* The rule of the season with year field y: season_rule_year, or the test year. */
static int rule_year(int y)
{
    const char *v = getenv("RECOMP_TEST_GRE_SECOND_YEAR");
    if (v && atoi(v) > 0) return atoi(v);
    return season_rule_year(y);
}

static int greek(uint32_t club) { return st_nation_is(club, KEY_GREECE); }

static int b_side(uint32_t club)
{
    return st_readable(club) && greek(club) && season_clubs_reserve_name(win_str(club + ST_CLUB_NAME));
}

static uint32_t group_obj(void)
{
    uint32_t o = season_comps_object(&season_div_gre_group);
    return st_readable(o) ? o : 0;
}

/* The group's record, read once per census or club walk (0 with no group). */
static uint32_t group_rec;

static void read_group(void)
{
    uint32_t g = group_obj();
    group_rec = g ? rd32(g_mem, g + OBJ_RECORD) : 0;
}

/* 1 for a club_comp record of the second tier: the National B Division or the group (read_group first). */
static int second_tier(uint32_t div)
{
    if (!st_readable(div)) return 0;
    return div == st_comp_of(KEY_SECOND) || (group_rec && div == group_rec);
}

/* The first team of a B side (0 when none is found). */
static uint32_t first_team(uint32_t club)
{
    int count;
    uint32_t table = st_clubs(&count);
    const char *name = win_str(club + ST_CLUB_NAME);
    for (size_t r = 0; r < sizeof PARENTS / sizeof *PARENTS; r++) {
        if (strncmp(name, PARENTS[r][0], 52)) continue;
        for (int k = 1; k < 3; k++)
            for (int i = 0; i < count; i++) {
                uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
                if (greek(c) && !strncmp(win_str(c + ST_CLUB_NAME), PARENTS[r][k], 52)) return c;
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
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
        if (c != club && greek(c) && !b_side(c) && !strncasecmp(win_str(c + ST_CLUB_NAME), want, 52)) return c;
    }
    return 0;
}

/* 1 when the rule of season y bars the B side from the second tier (its first team's division as it is now). */
static int barred_second(uint32_t club, int y)
{
    if (y < 2021) return 1;
    uint32_t p = first_team(club);
    if (!p) return 0;                                   /* no first team found: as the game */
    return rd32(g_mem, p + ST_CLUB_DIVISION) != st_comp_of(KEY_PRM);
}

/* ---- the clubs held out of the Lower Division's list (after the chain to GrePrm::create_table) ---- */

static uint32_t *held;
static int nheld;

static void give_back(void)
{
    uint32_t lower = st_comp_of(KEY_LOWER);
    for (int k = 0; k < nheld; k++) {
        wr32(g_mem, held[k] + ST_CLUB_DIVISION, lower);
        if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s stays in the Lower Division\n",
                              win_str(held[k] + ST_CLUB_NAME));
    }
    free(held);
    held = NULL;
    nheld = 0;
}

/* ---- census and the log of moves ---- */

static void census(const char *when)
{
    if (!log_on()) return;
    read_group();
    int count, c[3] = {0, 0, 0}, b[3] = {0, 0, 0};
    uint32_t table = st_clubs(&count), prm = st_comp_of(KEY_PRM), lower = st_comp_of(KEY_LOWER);
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE, d = rd32(g_mem, club + ST_CLUB_DIVISION);
        int k = d == prm ? 0 : second_tier(d) ? 1 : d == lower ? 2 : -1;
        if (k < 0) continue;
        c[k]++;
        b[k] += b_side(club);
    }
    fprintf(stderr, "recomp: " TAG ": census %s: Super League %d clubs, %d B sides; second tier %d clubs, %d B sides; "
            "Lower Division %d clubs, %d B sides\n", when, c[0], b[0], c[1], b[1], c[2], b[2]);
}

/* The level of each club before the change: 0 Super League, 1 second tier, 2 Lower Division, 3 other. */
static uint8_t *level_before;
static int level_count, change_on;

static int level_of(uint32_t club)
{
    uint32_t d = rd32(g_mem, club + ST_CLUB_DIVISION);
    return d == st_comp_of(KEY_PRM) ? 0 : second_tier(d) ? 1 : d == st_comp_of(KEY_LOWER) ? 2 : 3;
}

static void log_moves(void)
{
    static const char *const names[] = {"the Super League", "the second tier", "the Lower Division", "another division"};
    int count;
    uint32_t table = st_clubs(&count);
    read_group();
    for (int i = 0; level_before && i < count && i < level_count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        int now = level_of(club);
        if (now == level_before[i] || (now == 3 && level_before[i] == 3)) continue;
        fprintf(stderr, "recomp: " TAG ": %.51s goes %s to %s%s\n", win_str(club + ST_CLUB_NAME),
                now < level_before[i] ? "up" : "down", names[now], b_side(club) ? " (a B side)" : "");
    }
    free(level_before);
    level_before = NULL;
}

/* ---- test switches ---- */

/* The best placed B side with no up status takes `status` from the lowest placed other club that has it. */
static void test_take(uint32_t league, int up)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    if (!table) return;
    int b = -1, other = -1;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        uint8_t s = g_mem[row + ST_ROW_STATUS];
        int is_b = b_side(rd32(g_mem, row));
        int has = up ? (s == ST_UP || s == ST_TITLE) : s == ST_DOWN;
        if (is_b && has) return;                                         /* a B side has it already */
        if (is_b && (up ? b < 0 : 1)) b = i;                             /* up: the best placed; down: the lowest */
        if (!is_b && has && (up || other < 0)) other = i;                /* up: the lowest placed; down: the best */
    }
    if (b < 0 || other < 0) return;
    uint32_t ra = table + (uint32_t)b * ST_ROW_SIZE, rb = table + (uint32_t)other * ST_ROW_SIZE;
    uint8_t s = g_mem[rb + ST_ROW_STATUS];
    g_mem[rb + ST_ROW_STATUS] = g_mem[ra + ST_ROW_STATUS] == ST_BARRED ? ST_NONE : g_mem[ra + ST_ROW_STATUS];
    g_mem[ra + ST_ROW_STATUS] = s;
    fprintf(stderr, "recomp: " TAG ": test: %.51s takes status %d from %.51s\n", win_str(rd32(g_mem, ra) + ST_CLUB_NAME),
            s, win_str(rd32(g_mem, rb) + ST_CLUB_NAME));
}

/* ---- the rule at the season change ---- */

/* 2021/22 and 2022/23: a B side of status 3 stays (0xff), and the lowest placed other club with no place goes down. */
static void keep_up(uint32_t league, const char *what)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = n - 1; table && i >= 0; i--) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] != ST_DOWN || !b_side(club)) continue;
        g_mem[row + ST_ROW_STATUS] = ST_NONE;
        int k = n - 1;
        for (; k >= 0; k--) {
            uint32_t r = table + (uint32_t)k * ST_ROW_SIZE;
            uint8_t s = g_mem[r + ST_ROW_STATUS];
            if (s != ST_NONE || b_side(rd32(g_mem, r))) continue;
            g_mem[r + ST_ROW_STATUS] = ST_DOWN;
            break;
        }
        if (log_on())
            fprintf(stderr, "recomp: " TAG ": %.51s may not go down from %s, %.51s goes down in its place\n",
                    win_str(club + ST_CLUB_NAME), what,
                    k >= 0 ? win_str(rd32(g_mem, table + (uint32_t)k * ST_ROW_SIZE) + ST_CLUB_NAME) : "no club");
    }
}

/* Each B side of the league (an older build's save) goes down in exchange for the best placed club of status 3,
 * which stays up. With no such club left it stays one more season (logged): the chain moves a row of the Super League
 * down only with a partner that goes up, and the counts of the second tier stay as the original sets them. */
static void b_down(uint32_t league, const char *what)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    int stay[64], nstay = 0, used = 0;
    for (int i = 0; table && i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN && !b_side(rd32(g_mem, row))) stay[nstay++] = i;
    }
    for (int i = n - 1; table && i >= 0; i--) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN || !b_side(club)) continue;
        if (used == nstay) {
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s stays in %s one more season (no place goes down "
                                  "for it)\n", win_str(club + ST_CLUB_NAME), what);
            continue;
        }
        uint32_t up = table + (uint32_t)stay[used++] * ST_ROW_SIZE;
        g_mem[row + ST_ROW_STATUS] = ST_DOWN;
        g_mem[up + ST_ROW_STATUS] = ST_NONE;
        if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s goes down from %s, %.51s stays up\n",
                              win_str(club + ST_CLUB_NAME), what, win_str(rd32(g_mem, up) + ST_CLUB_NAME));
    }
}

static void gre_before_chain(uint32_t self)
{
    if (nheld) give_back();                         /* left by a change that did not reach 0x5ead60 */
    if (self != st_league_of(KEY_PRM) || !season_game_changes()) return;
    change_on = 1;
    census("before");
    int count;
    uint32_t table = st_clubs(&count);
    free(level_before);
    level_before = log_on() && count > 0 ? malloc((size_t)count) : NULL;
    level_count = count;
    read_group();
    for (int i = 0; level_before && i < count; i++) level_before[i] = (uint8_t)level_of(table + (uint32_t)i * ST_CLUB_SIZE);
    uint32_t second = st_league_of(KEY_SECOND), group = group_obj();
    int y = rule_year((int16_t)rd16(g_mem, self + LEAGUE_YEAR));
    if (st_env_is("RECOMP_TEST_GRE_SECOND_UP", "1")) {
        test_take(second, 1);
        if (group) test_take(group, 1);
    }
    if (st_env_is("RECOMP_TEST_GRE_SECOND_DOWN", "1")) {
        test_take(second, 0);
        if (group) test_take(group, 0);
    }
    if (!rule_on()) return;
    if (log_on()) fprintf(stderr, "recomp: " TAG ": season change after season %d (rule of %d)\n",
                          (int16_t)rd16(g_mem, self + LEAGUE_YEAR), y);
    st_shift_up(second, b_side, TAG, "at the season change", log_on());
    if (group) st_shift_up(group, b_side, TAG, "at the season change", log_on());
    if (y == 2021 || y == 2022) {
        keep_up(second, "the National B Division");
        if (group) keep_up(group, "the second group");
    }
    b_down(self, "the Super League");
    /* To 2020/21 no B side plays the second tier: one there (a save of an older build) goes to the Lower Division. */
    if (rule_year((int16_t)rd16(g_mem, self + LEAGUE_YEAR) + 1) < 2021) {
        b_down(second, "the National B Division");
        if (group) b_down(group, "the second group");
    }
}

static void gre_after_chain(uint32_t self)
{
    if (self != st_league_of(KEY_PRM) || !season_game_changes()) return;
    int count;
    uint32_t table = st_clubs(&count), lower = st_comp_of(KEY_LOWER);
    int test_rep = st_env_is("RECOMP_TEST_GRE_SECOND_REP", "1");
    int y = rule_year((int16_t)rd16(g_mem, self + LEAGUE_YEAR) + 1);
    for (int i = 0; i < count && !rt_unwinding; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (rd32(g_mem, club + ST_CLUB_DIVISION) != lower || !b_side(club)) continue;
        if (test_rep) wr16(g_mem, club + ST_CLUB_REPUTATION, 10000);
        if (!rule_on()) continue;
        if (!barred_second(club, y)) {
            if (log_on()) {
                uint32_t p = first_team(club);
                fprintf(stderr, "recomp: " TAG ": %.51s may enter the second tier (rule of %d: %s%s)\n",
                        win_str(club + ST_CLUB_NAME), y, p ? win_str(p + ST_CLUB_NAME) : "no first team",
                        p ? " plays the Super League" : "");
            }
            continue;
        }
        uint32_t *grown = realloc(held, sizeof *held * (size_t)(nheld + 1));
        if (!grown) continue;
        held = grown;
        held[nheld++] = club;
        wr32(g_mem, club + ST_CLUB_DIVISION, 0);
    }
}

/* GrePrm::create_table 0x5ead60, thiscall, no arguments: after 0x5eb120 and the group's moves (0x5eb001). */
void o_005ead60(void);
RT_WRAP(005ead60, gre_second_create_table)
{
    give_back();
    if (change_on && cpu.ecx == st_league_of(KEY_PRM)) {
        change_on = 0;
        log_moves();
        census("after");
        if (old_left > 0) old_left--;
    }
    o_005ead60();
}

/* Before 0x684cd0: the B sides of the second tier get 0xfc. */
static void gre_before_status(uint32_t league)
{
    if (!league || !rule_on()) return;
    uint32_t group = group_obj();
    if (league == st_league_of(KEY_SECOND) || (group && league == group))
        st_mark(league, b_side, TAG, league == group ? "the second group" : "the National B Division", log_on());
}

static int gre_barred(uint32_t club, uint32_t div)
{
    if (switched_off() || !b_side(club) || !st_readable(div)) return 0;
    if (div == st_comp_of(KEY_PRM)) return 1;
    /* A side held out of the Lower Division's list (division 0 until 0x5ead60 gives it back) may not be taken by the
     * size step that GrePrm::set_up_rules runs in between (0x5eafc3, season_sizes.c up_ok): give_back would put it
     * back in the Lower Division and leave the second tier one club short. */
    for (int k = 0; k < nheld; k++)
        if (held[k] == club) return 1;
    read_group();
    return second_tier(div) && rule_year(st_season_year(KEY_PRM)) < 2021;
}

static const SecondTeamHooks gre_hooks = {gre_before_status, gre_before_chain, gre_after_chain, gre_barred};
SEASON_SECOND_HOOKS(gre_hooks)
