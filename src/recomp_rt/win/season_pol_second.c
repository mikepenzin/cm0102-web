/* Poland: reserve sides follow the real rule (open item 243, 0.7.2).
 *
 * The real rule by season (notes and quotes: run-logs/0.7.2-nor-b/research-pol.md, outside the repo; all read on
 * 14 November 2026). The game plays the top two tiers: the Ekstraklasa (the I liga before 2008/09) and the Second
 * Division (the II liga before 2008/09, the I liga from 2008/09).
 * - 2001/02 to 2007/08: no reserve side plays the top two tiers (the "szczebel centralny"). No rule text was found.
 *   The cases: Legia II won its III liga group in 2000/01 and "jako druzyna rezerw nie mogla awansowac na szczebel
 *   centralny", Jagiellonia (second) went up; Amica II won its group in 2001/02, "nie mogla awansowac do II ligi",
 *   Aluminium Konin (second) went up (pl.wikipedia "III liga polska w pilce noznej (2001/2002)" and "(2002/2003)").
 *   No reserve side is in an old I or II liga table of 2000/01 to 2007/08, and the text of 2008 keeps the bar, so the
 *   bar runs in every season from 2001/02.
 * - 2008/09 to 2018/19: PZPN Zarzad uchwala IX/140 of 3 and 7 July 2008, § 2 ust. 8: a reserve side plays at most
 *   the III liga ("kluby Ekstraklasy, I i II ligi ... do III ligi wlacznie" to 2011; from 2011/13 an Ekstraklasa
 *   club's reserve side "do II ligi wlacznie", tier 3, still in the text after V/98 of 23 May 2018), and none goes up
 *   into a class of its first team. The II liga of 2008 to 2014 promotes a top-two side only when it is not the
 *   reserve side of an Ekstraklasa or I liga club (pl.wikipedia "II liga polska w pilce noznej"). So no reserve side
 *   plays the top two tiers. When the first team comes down, its reserve side moves down a class at once. A barred
 *   place goes to the next club of the table (Korona Kielce II won its III liga group in 2008/09 and LKS Nieciecza,
 *   second, went up).
 * - From 2019/20: the consolidated text of IX/140 of 4 November 2020, § 2 ust. 6 to 8: a reserve side plays only
 *   classes below that of its first team, two sides of a club never play one class, and a reserve side moves down
 *   when its first team comes down into its class. No tier limit is left, so a reserve side of an Ekstraklasa club may
 *   play the Second Division (Slask Wroclaw II played the I liga play-off of 2025/26), and no reserve side plays the
 *   Ekstraklasa. Which amendment dropped the limit (one of March 2019 to November 2020) I don't know: for 2019/20 and
 *   2020/21 both texts bar the Ekstraklasa and a reserve side in its first team's class, and for the rest (a reserve
 *   side of an Ekstraklasa club in the Second Division) these seasons keep the game's current behaviour, which lets
 *   it play there.
 * In every case found a barred place goes to the next club of the table.
 *
 * The original. PolFirst::start_new_season 0x7c9740 (thiscall, this the Ekstraklasa) calls the promotion chain
 * (vtable +0xb0, PolFirst::process_promotion_relegation_chain 0x7ca490, at 0x7c9753: league_promote_and_relegate
 * 0x689c80 moves the rows of status 0 or 5 of the Second Division up and the rows of status 3 of the Ekstraklasa's
 * spring stage down), then pol_first_promote_lower_division_clubs 0x7ca030 (at 0x7c975b), and adds 1 to the season
 * year +0x40 only after it (0x7c9791). 0x7ca030 lists the Polish clubs (club +0x53 the nation [0x9cf430]) whose division
 * (club +0x57) is the Polish Lower Division [0x9cf868] (0x7ca0e7..0x7ca109), and with 4 or more (0x7ca132) sorts them by
 * reputation and shuffles the top band (comp_util_sort_and_shuffle_top_clubs 0x4afbd0 with 4 picks at 0x7ca140),
 * moves the first 4 into the Second Division [0x9cf85c] (League::move_club_to_division 0x6830b0 at 0x7ca16f), then
 * sends the clubs of the Second Division's table rows from place (clubs +0x3e) - (down +0xc1) to the last
 * (League::get_table_row 0x6826d0 at 0x7ca1b4) to the Lower Division (League::relegate_club_to_division 0x6831a0 at
 * 0x7ca1be). No step tests a reserve side, and club_get_linked_b_team 0x540a50 has no Polish pairs (0x540af6..
 * 0x540b12). So a reserve side with a high reputation goes up into the Second Division, and on by the table.
 * PolSecond::set_up_rules 0x7cbf40 gives the Second Division 2 promotion places (+0xbe) and 2 play-off places (+0xbf,
 * PolSecond::create_play_off_stage 0x7cc290 takes the rows of status 1).
 *
 * The fix (season games, season_game_changes; the rule of the season that starts, through season_rule_year: with
 * changing rules off, the rule of the start year in every season, so the base game of 2001 keeps the original).
 * - A reserve side: a Polish club whose name passes season_clubs_reserve_name, or a name of PARENTS, except the first
 *   teams of FIRST_TEAMS. Its first team: the Polish club named as the reserve side less its last word (exact, case
 *   aside), else the names of PARENTS. Over the season databases every reserve side of the Lower Division finds its
 *   first team this way (run-logs/0.7.2-nor-b/pol/names-check.txt).
 * - During the season: before each 0x684cd0 (the shared wrap of season_second_teams.c) of the Second Division, a
 *   reserve side gets the original's mark 0xfc (0x684dea..0x68518b), so the next club takes the title, promotion and
 *   the play-off. A save from the days after the table was complete: st_shift_up before the chain 0x7ca490 and before
 *   the play-off 0x7cc290 hands the places on.
 * - The wrap of 0x7ca030 (after the chain, so a first team's new division counts):
 *   a reserve side of the Ekstraklasa (an older game) goes to the Lower Division (0x6831a0), and the best placed
 *   club of the Second Division's table still in that division goes up in its place (0x6830b0);
 *   a reserve side of the Second Division that the rule bars goes to the Lower Division after the call. For each
 *   such place, and each club that went up from the Second Division above, one club fewer goes down (the down count
 *   +0xc1 is lowered for the call), so the best placed club of the zone stays up. With no club of the zone left, the
 *   club of the Lower Division with the highest reputation goes up after the call (never one just sent down);
 *   a reserve side of the Lower Division that the rule bars reads as no division for the call, so the next club of
 *   the shuffled list goes up (as the original's own rule for Spain, 0x8504c9..0x8504ec).
 * - The club moves of a new game put no barred reserve side into the two divisions (barred_at_start).
 *
 * Test switches (tests/seasons/pol_second_run.py): RECOMP_POL_SECOND=off (the original), RECOMP_POL_SECOND_LOG=1 (the
 * clubs held, moved and the census of each change), RECOMP_TEST_POL_SECOND_REP=1 (each reserve side of the Lower
 * Division gets the highest reputation before the call), RECOMP_TEST_POL_SECOND_YEAR=<y> (the rule of season y in place
 * of the real one), RECOMP_TEST_POL_SECOND_PLACE=1 (before the call, the reserve sides of the Lower Division swap
 * divisions with the weakest clubs of the Second Division above the zone, as a game of an older build). */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_pol_second.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEY_POLAND 0x9cf430u
#define KEY_TOP 0x9cf858u               /* the Ekstraklasa (0x7cbfb6) */
#define KEY_SECOND 0x9cf85cu            /* the Second Division (0x7ca03f, 0x7ca14f) */
#define KEY_LOWER 0x9cf868u             /* the Polish Lower Division (0x7ca103, 0x7ca197) */
#define LEAGUE_YEAR 0x40u               /* s16, the season year; 0x7c9791 adds 1 after 0x7ca030 */
#define LEAGUE_DOWN 0xc1u               /* s8, the places that go down (0x7ca182) */
#define F_MOVE_UP 0x006830b0u           /* League::move_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define F_MOVE_DOWN 0x006831a0u         /* League::relegate_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define TAG "pol second"
#define POL_RULE_FROM 2001              /* the bar of the cases of 2000/01 and 2001/02 (research-pol.md) */
#define POL_FREE_FROM 2019              /* no tier limit (the text of 4 November 2020; research-pol.md) */

/* Polish first teams whose names the reserve test reads as reserve sides (exact names of the databases). */
static const char *const FIRST_TEAMS[] = {"Zawisza Bydgoszcz (2)"};   /* a second club of the name (May 2026) */

/* Reserve sides whose first team the name less its last word does not give (database name, first teams). */
static const char *const PARENTS[][3] = {
    {"Gornik Zabrze II", "G\xf3rnik Zabrze", NULL},
    {"Legia Warsaw II", "Legia Warszawa", "KP Legia Warszawa"},
    {"KP Legia II Warszawa", "KP Legia Warszawa", "Legia Warszawa"},
    {"Rakow Czestochowa II", "Rak\xf3w Czestochowa", NULL},
    {"Wisla Krakow II", "Wisla Krak\xf3w", "TS Wisla Krak\xf3w"},
};

static int log_on(void)
{
    static int v = -1;
    if (v < 0) v = st_env_is("RECOMP_POL_SECOND_LOG", "1");
    return v;
}

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_POL_SECOND", "off");
    return off;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

/* The year whose rule holds in season y: season_rule_year, or RECOMP_TEST_POL_SECOND_YEAR (tests only). */
static int rule_year_of(int y)
{
    static int test = -1;
    if (test < 0) {
        const char *v = getenv("RECOMP_TEST_POL_SECOND_YEAR");
        test = v ? atoi(v) : 0;
    }
    return test > 0 ? test : season_rule_year(y);
}

static int polish(uint32_t club) { return st_nation_is(club, KEY_POLAND); }

static int pair_of(const char *name)
{
    for (size_t i = 0; i < sizeof PARENTS / sizeof *PARENTS; i++)
        if (!strncmp(name, PARENTS[i][0], 52)) return (int)i;
    return -1;
}

static int reserve_name(const char *name)
{
    for (size_t i = 0; i < sizeof FIRST_TEAMS / sizeof *FIRST_TEAMS; i++)
        if (!strncmp(name, FIRST_TEAMS[i], 52)) return 0;
    return season_clubs_reserve_name(name) || pair_of(name) >= 0;
}

/* 1 for a Polish reserve side. */
static int reserve_side(uint32_t club)
{
    return st_readable(club) && polish(club) && reserve_name(win_str(club + ST_CLUB_NAME));
}

/* The Polish club (not a reserve side) of this exact name, case aside (0 when none). */
static uint32_t club_named(const char *want, uint32_t not_this)
{
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; table && i < count; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
        if (c == not_this || !polish(c)) continue;
        const char *n = win_str(c + ST_CLUB_NAME);
        if (!strncasecmp(n, want, 52) && !reserve_name(n)) return c;
    }
    return 0;
}

/* The first team of a reserve side: the name less its last word, else the names of PARENTS (0 when none). */
static uint32_t first_team(uint32_t club)
{
    const char *name = win_str(club + ST_CLUB_NAME);
    size_t at = strnlen(name, 51);
    while (at && name[at - 1] == ' ') at--;
    while (at && name[at - 1] != ' ') at--;
    while (at && name[at - 1] == ' ') at--;
    if (at) {
        char want[64];
        snprintf(want, sizeof want, "%.*s", (int)at, name);
        uint32_t c = club_named(want, club);
        if (c) return c;
    }
    int p = pair_of(name);
    for (int k = 1; p >= 0 && k < 3 && PARENTS[p][k]; k++) {
        uint32_t c = club_named(PARENTS[p][k], club);
        if (c) return c;
    }
    return 0;
}

/* 1 when the rule of season y bars a reserve side from the Ekstraklasa. */
static int top_barred(int y) { return y >= POL_RULE_FROM; }

/* 1 when the rule of season y bars the reserve side from the Second Division. *why gets the reason for the log. */
static int second_barred(uint32_t club, int y, const char **why)
{
    *why = "";
    if (y < POL_RULE_FROM) return 0;
    if (y < POL_FREE_FROM) {
        *why = "no reserve side in the top two tiers";
        return 1;
    }
    uint32_t p = first_team(club);
    if (!p) {
        *why = "no first team found";
        return 0;
    }
    uint32_t d = season_clubs_planned_division(p);       /* the moves' plan at a new game, else +0x57 */
    if (st_readable(d) && d == st_comp_of(KEY_TOP)) {
        *why = "its first team plays the Ekstraklasa";
        return 0;
    }
    *why = "its first team plays below the Ekstraklasa";
    return 1;
}

/* The season year of a league (s16 +0x40). */
static int league_year(uint32_t league)
{
    return st_readable(league) ? (int16_t)rd16(g_mem, league + LEAGUE_YEAR) : 0;
}

/* For the mark: a reserve side may not go up from the Second Division in the rule of the season played. */
static int mark_year;
static int barred_up(uint32_t club) { return top_barred(mark_year) && reserve_side(club); }

static void census(uint32_t div, int *clubs, int *reserves)
{
    *clubs = *reserves = 0;
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; table && st_readable(div) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (rd32(g_mem, club + ST_CLUB_DIVISION) != div) continue;
        ++*clubs;
        *reserves += reserve_side(club);
    }
}

static void log_census(const char *when, int season, int y)
{
    if (!log_on()) return;
    int c1, r1, c2, r2, c3, r3;
    census(st_comp_of(KEY_TOP), &c1, &r1);
    census(st_comp_of(KEY_SECOND), &c2, &r2);
    census(st_comp_of(KEY_LOWER), &c3, &r3);
    fprintf(stderr, "recomp: " TAG ": census %s (season %d, rule of %d): Ekstraklasa %d clubs, %d reserve sides; Second "
            "Division %d clubs, %d reserve sides; Lower Division %d clubs, %d reserve sides\n", when, season, y, c1, r1,
            c2, r2, c3, r3);
}

/* The row index of the club in the league's table, or -1. */
static int row_of(uint32_t table, int n, uint32_t club)
{
    for (int i = 0; i < n; i++)
        if (rd32(g_mem, table + (uint32_t)i * ST_ROW_SIZE) == club) return i;
    return -1;
}

static uint32_t move(uint32_t fn, uint32_t self, uint32_t club, uint32_t comp)
{
    uint32_t a[3] = {club, comp, 1};
    return st_call_this(fn, self, 3, a);
}

/* RECOMP_TEST_POL_SECOND_PLACE=1 (tests only): reserve sides of the Lower Division swap divisions with the weakest
 * clubs of the Second Division's table above the zone, as a game of an older build where they went up. */
static void test_place(uint32_t second_league, uint32_t second, uint32_t lower)
{
    static int on = -1;
    if (on < 0) on = st_env_is("RECOMP_TEST_POL_SECOND_PLACE", "1");
    int n, count;
    uint32_t rows = on ? st_table_of(second_league, &n) : 0, table = st_clubs(&count);
    if (!rows || !table) return;
    int down = (int8_t)g_mem[second_league + LEAGUE_DOWN], at = n - down - 1, done = 0;
    for (int i = 0; i < count && at >= 2 && done < 2; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (rd32(g_mem, club + ST_CLUB_DIVISION) != lower || !reserve_side(club)) continue;
        uint32_t other = rd32(g_mem, rows + (uint32_t)at * ST_ROW_SIZE);
        at--;
        if (!st_readable(other) || rd32(g_mem, other + ST_CLUB_DIVISION) != second) continue;
        wr32(g_mem, club + ST_CLUB_DIVISION, second);
        wr32(g_mem, other + ST_CLUB_DIVISION, lower);
        done++;
        fprintf(stderr, "recomp: " TAG ": test: %.51s takes the place of %.51s in the Second Division\n",
                win_str(club + ST_CLUB_NAME), win_str(other + ST_CLUB_NAME));
    }
}

int season_pol_barred(uint32_t club, uint32_t div);
int season_pol_barred(uint32_t club, uint32_t div)
{
    if (switched_off() || !reserve_side(club) || !st_readable(div)) return 0;
    int y = rule_year_of(st_season_year(KEY_TOP));
    const char *why;
    if (div == st_comp_of(KEY_TOP)) return top_barred(y);
    if (div == st_comp_of(KEY_SECOND)) return second_barred(club, y, &why);
    return 0;
}

/* pol_first_promote_lower_division_clubs 0x7ca030, thiscall (this the Ekstraklasa), no arguments. */
void o_007ca030(void);
RT_WRAP(007ca030, pol_second_lower)
{
    static int test_rep = -1;
    if (test_rep < 0) test_rep = st_env_is("RECOMP_TEST_POL_SECOND_REP", "1");
    int count;
    uint32_t table = st_clubs(&count);
    if (!season_game_changes() || !table || count <= 0) {
        o_007ca030();
        return;
    }
    uint32_t self = cpu.ecx, top = st_comp_of(KEY_TOP), second = st_comp_of(KEY_SECOND), lower = st_comp_of(KEY_LOWER);
    uint32_t second_league = st_league_of(KEY_SECOND);
    int season = league_year(self) + 1, y = rule_year_of(season);
    for (int i = 0; test_rep && i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (rd32(g_mem, club + ST_CLUB_DIVISION) == lower && reserve_side(club))
            wr16(g_mem, club + ST_CLUB_REPUTATION, 10000);
    }
    test_place(second_league, second, lower);
    log_census("before", season, y);
    char *was_second = calloc((size_t)count, 1);
    for (int i = 0; was_second && i < count; i++)
        was_second[i] = rd32(g_mem, table + (uint32_t)i * ST_CLUB_SIZE + ST_CLUB_DIVISION) == second;
    int on = rule_on() && y >= POL_RULE_FROM && st_readable(second_league) && st_readable(lower);
    int keep = 0;                       /* places of the zone that stay up */
    int *held = NULL, nheld = 0;
    int8_t down = st_readable(second_league) ? (int8_t)g_mem[second_league + LEAGUE_DOWN] : 0;
    if (on) {
        /* A reserve side of the Ekstraklasa (an older game) leaves; the best placed club of the Second Division's
         * table still in that division goes up in its place. */
        int n;
        uint32_t rows = st_table_of(second_league, &n);
        for (int i = 0; i < count && !rt_unwinding; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (rd32(g_mem, club + ST_CLUB_DIVISION) != top || !reserve_side(club)) continue;
            move(F_MOVE_DOWN, self, club, lower);
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s goes down from the Ekstraklasa (rule of %d)\n",
                                  win_str(club + ST_CLUB_NAME), y);
            for (int r = 0; r < n; r++) {
                uint32_t up = rd32(g_mem, rows + (uint32_t)r * ST_ROW_SIZE);
                if (!st_readable(up) || rd32(g_mem, up + ST_CLUB_DIVISION) != second || reserve_side(up)) continue;
                move(F_MOVE_UP, self, up, top);
                keep++;
                if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s goes up to the Ekstraklasa in its place\n",
                                      win_str(up + ST_CLUB_NAME));
                break;
            }
        }
        /* The barred reserve sides of the Second Division above the zone: one place of the zone stays up for each.
         * The zone shrinks as places stay up, so count again until no more is found. */
        int base = keep;
        for (int again = 1; again && !rt_unwinding;) {
            int start = n - down + keep, k = 0;
            for (int i = 0; i < count; i++) {
                uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
                const char *why;
                if (rd32(g_mem, club + ST_CLUB_DIVISION) != second || !reserve_side(club) || !second_barred(club, y, &why))
                    continue;
                int r = rows ? row_of(rows, n, club) : -1;
                if (r < 0 || r < start) k++;
            }
            again = base + k != keep;
            keep = base + k;
            if (keep > down + n) break;
        }
        /* The barred reserve sides of the Lower Division read as no division for the call. */
        held = malloc(sizeof *held * (size_t)count);
        for (int i = 0; held && i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            const char *why;
            if (rd32(g_mem, club + ST_CLUB_DIVISION) != lower || !reserve_side(club) || !second_barred(club, y, &why))
                continue;
            held[nheld++] = i;
            wr32(g_mem, club + ST_CLUB_DIVISION, 0);
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s stays in the Polish Lower Division (rule of %d: %s)\n",
                                  win_str(club + ST_CLUB_NAME), y, why);
        }
        g_mem[second_league + LEAGUE_DOWN] = (uint8_t)(keep < down ? down - keep : 0);
    }
    o_007ca030();
    if (on) {
        g_mem[second_league + LEAGUE_DOWN] = (uint8_t)down;
        for (int k = 0; k < nheld; k++)
            wr32(g_mem, table + (uint32_t)held[k] * ST_CLUB_SIZE + ST_CLUB_DIVISION, lower);
    }
    if (on && !rt_unwinding) {
        for (int i = 0; i < count && !rt_unwinding; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            const char *why;
            if (rd32(g_mem, club + ST_CLUB_DIVISION) != second || !reserve_side(club) || !second_barred(club, y, &why))
                continue;
            move(F_MOVE_DOWN, self, club, lower);
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s goes down from the Second Division (rule of %d: %s)\n",
                                  win_str(club + ST_CLUB_NAME), y, why);
        }
        /* Places left with no club of the zone: the clubs of the Lower Division with the highest reputation. */
        for (int extra = keep - down; extra > 0 && !rt_unwinding; extra--) {
            int best = -1, best_rep = -1;
            for (int i = 0; i < count; i++) {
                uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
                const char *why;
                if ((was_second && was_second[i]) || rd32(g_mem, club + ST_CLUB_DIVISION) != lower || !polish(club) ||
                    (reserve_side(club) && second_barred(club, y, &why)))
                    continue;
                int rep = (int16_t)rd16(g_mem, club + ST_CLUB_REPUTATION);
                if (rep > best_rep) best = i, best_rep = rep;
            }
            if (best < 0) break;
            move(F_MOVE_UP, self, table + (uint32_t)best * ST_CLUB_SIZE, second);
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s goes up to the Second Division (a place left)\n",
                                  win_str(table + (uint32_t)best * ST_CLUB_SIZE + ST_CLUB_NAME));
        }
    }
    if (!rt_unwinding && log_on() && was_second)
        for (int i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (!was_second[i] && rd32(g_mem, club + ST_CLUB_DIVISION) == second) {
                uint32_t p = reserve_side(club) ? first_team(club) : 0;
                uint32_t d = p ? rd32(g_mem, p + ST_CLUB_DIVISION) : 0;
                fprintf(stderr, "recomp: " TAG ": %.51s goes up to the Second Division%s%.51s%s\n",
                        win_str(club + ST_CLUB_NAME), reserve_side(club) ? " (a reserve side; first team " : "",
                        reserve_side(club) ? (p ? win_str(p + ST_CLUB_NAME) : "none") : "",
                        reserve_side(club) ? (d == top ? ", Ekstraklasa)" : ", below the Ekstraklasa)") : "");
            }
        }
    free(held);
    free(was_second);
    if (!rt_unwinding) log_census("after", season, y);
}

/* Before League::update_positions_and_status 0x684cd0 (season_second_teams.c): the reserve sides of the Second
 * Division get 0xfc. */
static void pol_before_status(uint32_t league)
{
    if (!league || league != st_league_of(KEY_SECOND) || !rule_on()) return;
    mark_year = rule_year_of(league_year(league));
    st_mark(league, barred_up, TAG, "the Second Division", log_on());
}

/* PolFirst::process_promotion_relegation_chain 0x7ca490 (vtable +0xb0 of PolFirst, called at 0x7c9753), thiscall, one
 * argument (ret 4). Not 0x689c20, so the shared hooks before_chain do not run for Poland. */
void o_007ca490(void);
RT_WRAP(007ca490, pol_second_chain)
{
    uint32_t second = st_league_of(KEY_SECOND);
    if (second && rule_on()) {
        mark_year = rule_year_of(league_year(second));
        st_shift_up(second, barred_up, TAG, "at the season change", log_on());
    }
    o_007ca490();
}

/* Before PolSecond::create_play_off_stage 0x7cc290 (thiscall, no arguments; it takes the rows of status 1): the
 * places of barred reserve sides go on to the next clubs. The one wrap of 0x7cc290 is the I liga play-off of issue 92
 * (season_pol_first.c, pol_second_playoff), which calls this first. Issue 92's own play-off (season_playoffs_build_for) picks its rows by place; its reserve test (barred, season_playoffs.c)
 * asks season_second_barred, so it skips a reserve side that this file bars (season_pol_barred). */
void season_pol_second_before_playoff(uint32_t league)
{
    if (league == st_league_of(KEY_SECOND) && rule_on()) {
        mark_year = rule_year_of(league_year(league));
        st_shift_up(league, barred_up, TAG, "before the play-off", log_on());
    }
}

static const SecondTeamHooks pol_hooks = {pol_before_status, NULL, NULL, season_pol_barred};
SEASON_SECOND_HOOKS(pol_hooks)
