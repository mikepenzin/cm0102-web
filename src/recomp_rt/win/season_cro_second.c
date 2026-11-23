/* Croatia: reserve sides follow the real rule (item 240, 0.7.2).
 *
 * The real rule, season by season (y the start year of the season: 2018 for 2018/19). The game plays the First
 * Division (1. HNL) and the two groups of the Second Division (tier 2: 2. HNL, from 2022/23 Prva NL). Sources, read
 * on 14 November 2026 (notes: run-logs/0.7.2-nor-b/research-cro.md, outside the repo):
 * - y <= 2013: I don't know. Novi list, 6 July 2013 (https://www.novilist.hr/?p=303055): "od sezone 2014./15. B
 *   momčadi hrvatskih prvoligaša smiju sudjelovati u redovnim natjecanjima". No text of the rule before it was
 *   found (the HNS Pravilnik o nogometnim natjecanjima of 2005 has no clause on second teams). The game's own
 *   behaviour stays.
 * - 2014 to 2021: a reserve side of a First Division club may play the Second Division, and never goes up into the
 *   First Division. Novi list (above): "B momčadi neće moći napredovati dalje od Druge HNL". Propozicije Druge HNL
 *   2018/19 (https://hns-cff.hr/files/documents/15497/) and 2020/21 Čl. 40 (https://hns-cff.hr/files/documents/
 *   19083/): "Druga momčad profesionalnog kluba ne može ostvariti plasman u Prvu HNL". 2020/21 Čl. 38: "Druge
 *   momčadi ne mogu nastupati u istom stupnju natjecanja u kojem se nalazi i njihova prva seniorska momčad". So a
 *   reserve side may play the Second Division only while its first team plays the First Division.
 * - 2022 to 2024: no reserve side plays above the third tier. HNS Izvršni odbor, 7 December 2021, from 1 July 2022
 *   (https://nogometplus.net/sluzbeno-od-sljedece-sezone-bez-drugih-momcadi-prvoligaskih-klubova-u-drugoj-hnl/):
 *   "ne mogu nastupati u višem stupnju natjecanja od trećeg stupnja". Dinamo II and Osijek II left the 2. NL.
 * - From 2025: a reserve side may go up to the Prva NL at most (HNS, 26 February 2025, https://www.telegram.hr/
 *   telesport/na-prvu/hns-potvrdio-vracaju-se-druge-momcadi-startat-ce-u-cetvrtom-rangu-no-mogu-lijepo-napredovati:
 *   "mogu napredovati najviše do Prve NL"); from 2026/27 Dinamo II plays it (index.hr, 2725032). So never the First
 *   Division. Whether a reserve side must leave the Prva NL when its first team comes down into it: I don't know,
 *   so it stays (the game's behaviour).
 * Who takes a barred place: the next club of the table (2018/19 and 2020/21 Čl. 40: the fourth placed club goes up
 * when the three reserve sides take the first three places). Real case: 2017/18 Dinamo II third, Varaždin to the
 * play-off.
 *
 * The original. CroA1::start_new_season 0x5149a0 calls the chain through vtable +0xb0 (at 0x5149b3), which for the
 * First Division is CroA1::process_promotion_relegation_chain 0x5150f0: league_promote_and_relegate 0x689c80 for
 * the pairs (First Division, North) at 0x515158 and (First Division, South) at 0x515180 (rows of status 0 or 5 go
 * up, rows of status 3 go down). It does not call 0x689c20, so the shared before_chain hook never runs here. Then
 * cro_a1_swap_second_and_third_tier_clubs 0x514ed0 (at 0x5149bb) lists the Croatian clubs ([0x9cf2ac], club +0x53,
 * 0x514f98..0x514fa9) whose division (club +0x57) is A Lower Division [0x9cf764] or Third Division North, South,
 * East or West [0x9cf888..0x9cf894] (0x514fab..0x514fdc), and with at least 4 of them (0x514ff5) sorts them by
 * reputation and shuffles the top band (comp_util_sort_and_shuffle_top_clubs 0x4afbd0 at 0x515003), moves the
 * first 4 into Second Division North [0x9cf8a8] (League::move_club_to_division 0x6830b0 at 0x515031) and sends the
 * rows of status 3 of the North and South tables to A Lower Division (0x6831a0 at 0x515088 and 0x5150c5).
 * cro_a1_realign_second_division_groups 0x515190 (at 0x5149c2) then shares the clubs of both groups out by latitude,
 * and 0x5149a0 adds 1 to the year +0x40 only after (0x514a08). No call of club_get_linked_b_team 0x540a50 and no
 * reserve test: a reserve side with a high reputation goes up into the Second Division and on by the table. The
 * Croatian Lower Division of the databases has no key, so its sides are not in the list.
 *
 * The fix (season games, season_game_changes; the rule of the season that starts through season_rule_year, so with
 * changing rules off the start year's rule; no change for a rule year before 2014):
 * - A reserve side: a Croatian club whose name passes season_clubs_reserve_name (last word II, B, 2, ..., or a word
 *   U19, U21, ...). Its first team: the Croatian club named as the reserve side less its last word, exactly (case
 *   aside). Over the 49 season databases every one finds its first team this way (Dinamo Zagreb II, Hajduk Split II,
 *   Osijek II, and the U19 sides of Dinamo Zagreb, Hajduk Split, Rijeka, Lokomotiva and Osijek), so no pair table is
 *   needed, and no Croatian first team has a reserve-looking name. Hajduk Split u19 (10 databases) and Dinamo Zagreb
 *   U19 (October 2019) sit in A Lower Division, in the original's list: I found no source on youth sides in the
 *   senior leagues, and the shared name test takes them as reserve sides, so the rule above bars them too.
 * - During the season: before each 0x684cd0 (the shared wrap of season_second_teams.c) of North or South, a reserve
 *   side gets 0xfc (st_mark), so the next club takes the title, promotion and the play-off.
 * - The wrap of 0x5150f0: when a save comes from the days after the table was complete, st_shift_up hands the
 *   places on first.
 * - The wrap of 0x514ed0 (after the chain, so a first team's new division counts): a reserve side of the First
 *   Division (an old save) goes to A Lower Division and the best placed club of the North and South tables still
 *   there goes up in its place; a reserve side that the rule bars from the Second Division gets status 3 (so
 *   0x514ed0 sends it down) or, when the chain just brought it, goes to A Lower Division at once. For each place,
 *   the best placed club of status 3 of the groups stays up (0xff); with none left, the club of the list with the
 *   highest reputation goes up into North after the call. For the call a barred reserve side of the list reads as
 *   no division and gets it back after, so the next club of the shuffled list goes up (as the original's own rule
 *   for Spain, 0x8504c9..0x8504ec).
 * - The club moves of a new game put no barred reserve side into the three divisions (barred_at_start).
 *
 * Test switches (tests/seasons/cro_second_run.py): RECOMP_CRO_SECOND=off (the original), RECOMP_CRO_SECOND_LOG=1
 * (the clubs held and moved, and a census of each change), RECOMP_TEST_CRO_SECOND_REP=1 (each reserve side of the
 * list gets the highest reputation before the call, and the list is cut to 4 clubs besides the reserve sides held,
 * so the original's shuffle takes every reserve side it may take), RECOMP_TEST_CRO_SECOND_YEAR=<y> (the rule of season y),
 * RECOMP_TEST_CRO_SECOND_OLD=<n> (the original, and no barred_at_start, until n Croatian season changes are done, as
 * a game of an older build), RECOMP_TEST_CRO_SECOND_UP=1 (before the chain the best placed reserve side of each group
 * takes the title or promotion status, so the original would send it up), RECOMP_TEST_CRO_SECOND_NO_MARK=1 (no 0xfc,
 * as a save made after the table was complete), RECOMP_TEST_CRO_SECOND_HOLD=1 (before each update each reserve side of
 * the First Division gets the points of the middle club, so an old save's reserve side is still there at the next
 * change). */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEY_CROATIA 0x9cf2acu
#define KEY_A1 0x9cf8a4u                /* the First Division (0x5150f8) */
#define KEY_NORTH 0x9cf8a8u             /* Second Division North (0x514edf, 0x515012) */
#define KEY_SOUTH 0x9cf8acu             /* Second Division South (0x514eec) */
#define KEY_LOWER 0x9cf764u             /* A Lower Division (0x514fb4, 0x51505f) */
#define LEAGUE_YEAR 0x40u               /* s16, the season year; 0x514a08 adds 1 after 0x514ed0 */
#define F_MOVE_UP 0x006830b0u           /* League::move_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define F_MOVE_DOWN 0x006831a0u         /* League::relegate_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define TAG "cro second"
#define LEVELS 2                        /* 0 the First Division, 1 the Second Division (North or South) */

/* The divisions of the original's list (0x514fb4..0x514fd6). */
static const uint32_t POOL_KEYS[] = {KEY_LOWER, 0x9cf888u, 0x9cf890u, 0x9cf88cu, 0x9cf894u};

static int log_on(void)
{
    static int v = -1;
    if (v < 0) v = st_env_is("RECOMP_CRO_SECOND_LOG", "1");
    return v;
}

/* RECOMP_TEST_CRO_SECOND_OLD=<n>: the Croatian season changes left to play as the original. Read once. */
static int old_left = -1;

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_CRO_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_CRO_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

/* The year whose rule holds in season y: season_rule_year, or RECOMP_TEST_CRO_SECOND_YEAR (tests only). */
static int rule_year(int y)
{
    static int test = -1;
    if (test < 0) {
        const char *v = getenv("RECOMP_TEST_CRO_SECOND_YEAR");
        test = v ? atoi(v) : 0;
    }
    return test > 0 ? test : season_rule_year(y);
}

static int croatian(uint32_t club) { return st_readable(club) && st_nation_is(club, KEY_CROATIA); }

static uint32_t division_of(uint32_t club) { return rd32(g_mem, club + ST_CLUB_DIVISION); }

static const char *name_of(uint32_t club) { return win_str(club + ST_CLUB_NAME); }

/* 0 the First Division, 1 North or South, LEVELS any other. */
static int level_of(uint32_t div)
{
    if (!st_readable(div)) return LEVELS;
    if (div == st_comp_of(KEY_A1)) return 0;
    if (div == st_comp_of(KEY_NORTH) || div == st_comp_of(KEY_SOUTH)) return 1;
    return LEVELS;
}

/* 1 when the division is one of the original's list (the id compare of 0x514fb2..0x514fdc). */
static int in_pool(uint32_t div)
{
    if (!st_readable(div)) return 0;
    int32_t id = (int32_t)rd32(g_mem, div);
    for (size_t k = 0; k < sizeof POOL_KEYS / sizeof *POOL_KEYS; k++)
        if (id == (int32_t)rd32(g_mem, POOL_KEYS[k])) return 1;
    return 0;
}

static int reserve_side(uint32_t club)
{
    return croatian(club) && season_clubs_reserve_name(name_of(club));
}

/* The first team of a reserve side: the Croatian club whose name is the reserve side's name less its last word,
 * exactly (case aside). 0 when none. */
static uint32_t first_team(uint32_t club)
{
    const char *name = name_of(club);
    size_t at = strnlen(name, 51);
    while (at && name[at - 1] == ' ') at--;
    while (at && name[at - 1] != ' ') at--;
    while (at && name[at - 1] == ' ') at--;
    char want[64];
    if (!at || at >= sizeof want) return 0;
    memcpy(want, name, at);
    want[at] = 0;
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; i < count; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
        if (c == club || !croatian(c)) continue;
        const char *n = name_of(c);
        if (!strncasecmp(n, want, 52) && !season_clubs_reserve_name(n)) return c;
    }
    return 0;
}

/* 1 when the rule of season y bars the club from level l (0 or 1). See the top for the sources. */
static int barred_in(uint32_t club, int l, int y)
{
    if (l < 0 || l >= LEVELS || !reserve_side(club)) return 0;
    y = rule_year(y);
    if (y < 2014) return 0;                     /* I don't know: the game's behaviour */
    if (l == 0) return 1;                       /* never the First Division */
    if (y >= 2022 && y <= 2024) return 1;       /* not above the third tier */
    if (y <= 2021) {                            /* only while the first team plays the First Division */
        uint32_t p = first_team(club);
        return p && level_of(season_clubs_planned_division(p)) != 0;   /* the moves' plan at a new game */
    }
    return 0;                                   /* from 2025: up to the Prva NL; the same-tier case I don't know */
}

/* The season the hooks of st_mark and st_shift_up read (they take the club only): the season played. barred_up
 * reads the rule of the next season (cur_year + 1), the season the side would go up into, as gre_after_chain does. */
static int cur_year;

static int barred_up(uint32_t club) { return barred_in(club, 0, cur_year + 1); }

static int season_of(uint32_t league) { return (int16_t)rd16(g_mem, league + LEAGUE_YEAR); }

static void cro_before_status(uint32_t league)
{
    static int no_mark = -1, hold = -1;
    if (no_mark < 0) no_mark = st_env_is("RECOMP_TEST_CRO_SECOND_NO_MARK", "1");
    if (hold < 0) hold = st_env_is("RECOMP_TEST_CRO_SECOND_HOLD", "1");
    if (hold && season_game_changes() && league == st_league_of(KEY_A1)) st_test_mid_table(league, reserve_side);
    if (no_mark || !rule_on()) return;
    if (league == st_league_of(KEY_NORTH) || league == st_league_of(KEY_SOUTH)) {
        cur_year = season_of(league);
        st_mark(league, barred_up, TAG, league == st_league_of(KEY_NORTH) ? "Second Division North" :
                "Second Division South", log_on());
    }
}

static void census(const char *when, int y)
{
    if (!log_on()) return;
    int n[3] = {0}, s[3] = {0}, pool = 0, count;
    uint32_t table = st_clubs(&count), north = st_comp_of(KEY_NORTH);
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (!croatian(club)) continue;
        uint32_t d = division_of(club);
        int l = level_of(d);
        if (l < LEVELS) {
            if (l == 1 && d != north) l = 2;
            n[l]++;
            s[l] += reserve_side(club);
        } else if (in_pool(d)) {
            pool += reserve_side(club);
        }
    }
    fprintf(stderr, "recomp: cro second: census %s: First Division %d clubs, %d reserve sides; North %d clubs, %d "
            "reserve sides; South %d clubs, %d reserve sides; list %d reserve sides; change into %d/%02d (rule of "
            "%d)\n", when, n[0], s[0], n[1], s[1], n[2], s[2], pool, y, (y + 1) % 100, rule_year(y));
}

/* RECOMP_TEST_CRO_SECOND_UP=1 (tests only): the best placed reserve side of the group's table takes the status of
 * the first row of status 5 or 0, which takes the reserve side's status. */
static void test_up(uint32_t league)
{
    int n;
    uint32_t table = league ? st_table_of(league, &n) : 0;
    if (!table) return;
    int res = -1, top = -1;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        uint8_t st = g_mem[row + ST_ROW_STATUS];
        int r = reserve_side(rd32(g_mem, row));
        if (r && (st == ST_TITLE || st == ST_UP)) return;
        if (r && res < 0) res = i;
        if (!r && top < 0 && (st == ST_TITLE || st == ST_UP)) top = i;
    }
    if (res < 0 || top < 0) return;
    uint32_t a = table + (uint32_t)res * ST_ROW_SIZE, b = table + (uint32_t)top * ST_ROW_SIZE;
    uint8_t st = g_mem[b + ST_ROW_STATUS];
    g_mem[b + ST_ROW_STATUS] = g_mem[a + ST_ROW_STATUS];
    g_mem[a + ST_ROW_STATUS] = st;
    fprintf(stderr, "recomp: cro second: test: %.51s takes status %d from %.51s\n", name_of(rd32(g_mem, a)), st,
            name_of(rd32(g_mem, b)));
}

/* CroA1::process_promotion_relegation_chain 0x5150f0, thiscall (this the First Division), one argument, ret 4. */
void o_005150f0(void);
RT_WRAP(005150f0, cro_second_chain)
{
    static int up = -1;
    if (up < 0) up = st_env_is("RECOMP_TEST_CRO_SECOND_UP", "1");
    uint32_t self = cpu.ecx;
    int count;
    uint32_t table = st_clubs(&count);
    if (!season_game_changes() || !table) {
        o_005150f0();
        return;
    }
    cur_year = season_of(self);
    if (up) {
        test_up(st_league_of(KEY_NORTH));
        test_up(st_league_of(KEY_SOUTH));
    }
    if (rule_on()) {
        st_shift_up(st_league_of(KEY_NORTH), barred_up, TAG, "at the season change", log_on());
        st_shift_up(st_league_of(KEY_SOUTH), barred_up, TAG, "at the season change", log_on());
    }
    char *was_a1 = log_on() ? calloc((size_t)count, 1) : NULL;
    uint32_t a1 = st_comp_of(KEY_A1);
    for (int i = 0; was_a1 && i < count; i++) was_a1[i] = division_of(table + (uint32_t)i * ST_CLUB_SIZE) == a1;
    o_005150f0();
    for (int i = 0; was_a1 && !rt_unwinding && i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (!was_a1[i] && division_of(club) == a1)
            fprintf(stderr, "recomp: cro second: %.51s goes up to the First Division%s\n", name_of(club),
                    reserve_side(club) ? " (a reserve side)" : "");
    }
    free(was_a1);
}

/* The row of the club in a league's table, or -1. */
static int row_of(uint32_t league, uint32_t club)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n; i++)
        if (rd32(g_mem, table + (uint32_t)i * ST_ROW_SIZE) == club) return i;
    return -1;
}

static void move(uint32_t fn, uint32_t self, uint32_t club, uint32_t div)
{
    uint32_t a[3] = {club, div, 1};
    st_call_this(fn, self, 3, a);
}

/* The best placed club of the North and South tables (North first at the same place) that is still in its group,
 * has no status 3 and may play the First Division in season y. Its row gets 0xff. 0 when none. */
static uint32_t next_up(int y)
{
    uint32_t leagues[2] = {st_league_of(KEY_NORTH), st_league_of(KEY_SOUTH)};
    uint32_t divs[2] = {st_comp_of(KEY_NORTH), st_comp_of(KEY_SOUTH)};
    int n[2] = {0, 0};
    uint32_t tables[2] = {st_table_of(leagues[0], &n[0]), st_table_of(leagues[1], &n[1])};
    for (int i = 0; i < n[0] || i < n[1]; i++)
        for (int g = 0; g < 2; g++) {
            if (i >= n[g]) continue;
            uint32_t row = tables[g] + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
            if (!st_readable(club) || division_of(club) != divs[g] || g_mem[row + ST_ROW_STATUS] == ST_DOWN ||
                barred_in(club, 0, y))
                continue;
            g_mem[row + ST_ROW_STATUS] = ST_NONE;
            if (log_on()) fprintf(stderr, "recomp: cro second: next club: %.51s, place %d of %s\n", name_of(club),
                                  i + 1, g ? "South" : "North");
            return club;
        }
    return 0;
}

/* The best placed club of status 3 of the North and South tables that may stay (0xff). 1 when one stays. */
static int stay_up(int y)
{
    uint32_t leagues[2] = {st_league_of(KEY_NORTH), st_league_of(KEY_SOUTH)};
    uint32_t divs[2] = {st_comp_of(KEY_NORTH), st_comp_of(KEY_SOUTH)};
    int n[2] = {0, 0};
    uint32_t tables[2] = {st_table_of(leagues[0], &n[0]), st_table_of(leagues[1], &n[1])};
    for (int i = 0; i < n[0] || i < n[1]; i++)
        for (int g = 0; g < 2; g++) {
            if (i >= n[g]) continue;
            uint32_t row = tables[g] + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
            if (g_mem[row + ST_ROW_STATUS] != ST_DOWN || !st_readable(club) || division_of(club) != divs[g] ||
                barred_in(club, 1, y))
                continue;
            g_mem[row + ST_ROW_STATUS] = ST_NONE;
            if (log_on()) fprintf(stderr, "recomp: cro second: %.51s stays up in the Second Division\n", name_of(club));
            return 1;
        }
    return 0;
}

static int season_cro_barred(uint32_t club, uint32_t div)
{
    if (switched_off() || !croatian(club)) return 0;
    int y = st_season_year(KEY_A1), barred = barred_in(club, level_of(div), y);
    if (barred && log_on())
        fprintf(stderr, "recomp: cro second: new game: %.51s may not play the %s (rule of %d)\n", name_of(club),
                level_of(div) ? "Second Division" : "First Division", rule_year(y));
    return barred;
}

/* cro_a1_swap_second_and_third_tier_clubs 0x514ed0, thiscall (this the First Division, kept in ebp for 0x6830b0),
 * no arguments. */
void o_00514ed0(void);
RT_WRAP(00514ed0, cro_second_exchange)
{
    static int test_rep = -1;
    if (test_rep < 0) test_rep = st_env_is("RECOMP_TEST_CRO_SECOND_REP", "1");
    int count;
    uint32_t table = st_clubs(&count);
    if (!season_game_changes() || !table) {
        o_00514ed0();
        return;
    }
    uint32_t self = cpu.ecx;
    int on = rule_on(), y = season_of(self) + 1;    /* the season that starts */
    uint32_t a1 = st_comp_of(KEY_A1), north = st_comp_of(KEY_NORTH), south = st_comp_of(KEY_SOUTH);
    uint32_t lower = st_comp_of(KEY_LOWER);
    char *was_top = calloc((size_t)count, 1);       /* in the First or Second Division before the call */
    uint32_t *held = malloc(sizeof *held * (size_t)count), *held_div = malloc(sizeof *held_div * (size_t)count);
    if (!was_top || !held || !held_div) {
        fprintf(stderr, "recomp: cro second: no memory for the season change: the original's list runs\n");
        free(was_top);
        free(held);
        free(held_div);
        o_00514ed0();
        return;
    }
    census("before", y);
    for (int i = 0; i < count; i++) was_top[i] = level_of(division_of(table + (uint32_t)i * ST_CLUB_SIZE)) < LEVELS;
    int need = 0, nheld = 0;
    if (on && !st_readable(lower)) {
        fprintf(stderr, "recomp: cro second: the data has no A Lower Division: the original's list runs\n");
        on = 0;
    }
    if (on) {
        /* The First Division (an old save): each reserve side goes to A Lower Division, the next club of the
         * groups takes its place. */
        int short_places = 0;
        for (int i = 0; i < count && !rt_unwinding; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (division_of(club) != a1 || !barred_in(club, 0, y)) continue;
            move(F_MOVE_DOWN, self, club, lower);
            short_places++;
            if (log_on()) fprintf(stderr, "recomp: cro second: %.51s goes down from the First Division\n",
                                  name_of(club));
        }
        for (; short_places > 0 && !rt_unwinding; short_places--) {
            uint32_t up = next_up(y);
            if (!up) {
                fprintf(stderr, "recomp: cro second: no club of the Second Division may take a place of the First "
                        "Division\n");
                break;
            }
            move(F_MOVE_UP, self, up, a1);
            need++;
            if (log_on()) fprintf(stderr, "recomp: cro second: %.51s goes up to the First Division in place of a "
                                  "reserve side\n", name_of(up));
        }
        /* North and South: a barred reserve side of a table gets status 3 (0x514ed0 sends it down), one that the
         * chain just brought goes to A Lower Division now. */
        for (int i = 0; i < count && !rt_unwinding; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE, d = division_of(club);
            if ((d != north && d != south) || !barred_in(club, 1, y)) continue;
            uint32_t league = st_league_of(d == north ? KEY_NORTH : KEY_SOUTH);
            int n, r = row_of(league, club);
            uint32_t rows = st_table_of(league, &n);
            if (r >= 0 && g_mem[rows + (uint32_t)r * ST_ROW_SIZE + ST_ROW_STATUS] == ST_DOWN) continue;
            if (r >= 0) g_mem[rows + (uint32_t)r * ST_ROW_SIZE + ST_ROW_STATUS] = ST_DOWN;
            else move(F_MOVE_DOWN, self, club, lower);
            uint32_t p = first_team(club);
            if (log_on()) fprintf(stderr, "recomp: cro second: %.51s goes down from the Second Division (rule of %d, "
                                  "first team %.51s)\n", name_of(club), rule_year(y), p ? name_of(p) : "none");
            need++;
        }
        while (need > 0 && stay_up(y)) need--;
    }
    /* The list: a barred reserve side, or one that left the divisions above in this call, reads as no division. */
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE, d = division_of(club);
        if (!in_pool(d) || !reserve_side(club)) continue;
        if (test_rep) wr16(g_mem, club + ST_CLUB_REPUTATION, 10000);
        if (!on) continue;
        if (!barred_in(club, 1, y) && !was_top[i]) {
            if (log_on()) {
                uint32_t p = first_team(club);
                fprintf(stderr, "recomp: cro second: %.51s may go up to the Second Division (rule of %d, first team "
                        "%.51s)\n", name_of(club), rule_year(y), p ? name_of(p) : "none");
            }
            continue;
        }
        held[nheld] = club;
        held_div[nheld++] = d;
        wr32(g_mem, club + ST_CLUB_DIVISION, 0);
    }
    /* 0x514ed0 moves no club up and none down when its list holds fewer than 4 clubs (0x514ff9). If the held sides
     * would leave fewer than 4, they stay in the list for this call (the original's pick), so the season's
     * relegation still runs; a barred side that goes up goes down at the next change (the old-save path above). */
    if (nheld) {
        int left = 0;
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (croatian(club) && in_pool(division_of(club))) left++;
        }
        if (left < 4) {
            for (int k = 0; k < nheld; k++) wr32(g_mem, held[k] + ST_CLUB_DIVISION, held_div[k]);
            if (log_on()) fprintf(stderr, "recomp: cro second: the list would hold %d clubs, so %d reserve sides "
                                  "stay in it for this call\n", left, nheld);
            nheld = 0;
        }
    }
    /* RECOMP_TEST_CRO_SECOND_REP=1: the list keeps the reserve sides that are not held and the clubs of highest
     * reputation, 4 in all (or the reserve sides alone when they are 4 or more), so the original's shuffle of the
     * top band takes every reserve side it may take. The others read as no division for the call (test only). */
    int nplant = nheld;
    if (test_rep) {
        int keep = 4;
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (croatian(club) && in_pool(division_of(club)) && reserve_side(club)) keep--;
        }
        for (;;) {
            uint32_t low = 0;
            int low_rep = 1 << 30, left = 0;
            for (int i = 0; i < count; i++) {
                uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
                if (!croatian(club) || !in_pool(division_of(club)) || reserve_side(club)) continue;
                left++;
                int rep = (int16_t)rd16(g_mem, club + ST_CLUB_REPUTATION);
                if (rep < low_rep) low = club, low_rep = rep;
            }
            if (!low || left <= (keep > 0 ? keep : 0)) break;
            held[nplant] = low;
            held_div[nplant++] = division_of(low);
            wr32(g_mem, low + ST_CLUB_DIVISION, 0);
        }
        fprintf(stderr, "recomp: cro second: test: %d clubs of the list read as no division for the call\n",
                nplant - nheld);
    }
    if (!rt_unwinding) o_00514ed0();
    for (int k = nheld; k < nplant; k++) wr32(g_mem, held[k] + ST_CLUB_DIVISION, held_div[k]);
    for (int k = 0; k < nheld; k++) {
        wr32(g_mem, held[k] + ST_CLUB_DIVISION, held_div[k]);
        if (log_on()) fprintf(stderr, "recomp: cro second: %.51s stays in %.51s\n", name_of(held[k]),
                              win_str(held_div[k] + 4));
    }
    /* Places left over: the club of the list with the highest reputation goes up into North (as 0x515031; the
     * original shuffles the top band, this takes no random number), never a club that was in the divisions above. */
    for (; on && need > 0 && !rt_unwinding; need--) {
        uint32_t best = 0;
        int best_rep = -1;
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (was_top[i] || !croatian(club) || !in_pool(division_of(club)) || barred_in(club, 1, y)) continue;
            int rep = (int16_t)rd16(g_mem, club + ST_CLUB_REPUTATION);
            if (rep > best_rep) best = club, best_rep = rep;
        }
        if (!best) break;
        move(F_MOVE_UP, self, best, north);
        if (log_on()) fprintf(stderr, "recomp: cro second: %.51s goes up to the Second Division in place of a reserve "
                              "side\n", name_of(best));
    }
    if (log_on() && !rt_unwinding)
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (!was_top[i] && level_of(division_of(club)) == 1)
                fprintf(stderr, "recomp: cro second: %.51s goes up to the Second Division%s\n", name_of(club),
                        reserve_side(club) ? " (a reserve side)" : "");
        }
    free(was_top);
    free(held);
    free(held_div);
    if (!rt_unwinding) {
        census("after", y);
        if (old_left > 0) old_left--;
    }
}

static const SecondTeamHooks cro_hooks = {cro_before_status, NULL, NULL, season_cro_barred};
SEASON_SECOND_HOOKS(cro_hooks)
