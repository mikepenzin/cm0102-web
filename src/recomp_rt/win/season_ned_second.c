/* Netherlands: a Jong side never goes up from the Eerste Divisie (open item 114, 0.7.0).
 *
 * The real rule. From 2013/14 the second teams of Eredivisie clubs (Jong Ajax, Jong PSV and others) play in the Eerste
 * Divisie. A Jong side may finish first but never goes up: the highest first teams of the table take the direct
 * places (Jong Ajax won the 2017/18 Eerste Divisie and Fortuna Sittard, second, went up). A Jong side takes no part
 * in the promotion play-off. When it wins a period title, the place goes to the best placed team of that period table
 * with no period title (Reglement periodekampioenschappen eerste divisie 2019/'20, Art. 6 (2)), and from 2021/22 the
 * play-off is filled by the highest first teams of the table with no period title (Reglement play-off 2021/'22).
 * Only the second team of an Eredivisie club may play in the Eerste Divisie, and a club has one team per competition
 * (Promotie- en degradatieregeling 2025/'26, Titel I (b) and (e)). When the first team goes down from the Eredivisie,
 * its Jong side goes down from the Eerste Divisie at once (2025/'26 Art. 10), and the highest Jong side of the
 * Tweede Divisie takes the place (Dutch Wikipedia, from 2020/21). Sources (each read on 3 November 2026; the
 * research is run-logs/0.6.2-s111b/research/NED.md): the KNVB texts named above, https://en.wikipedia.org/wiki/
 * Eerste_Divisie and https://nl.wikipedia.org/wiki/Eerste_divisie_(voetbal_Nederland). Before 2013/14 no reserve
 * side played in the two Dutch divisions that the game plays, so the bar holds in every season. The texts of
 * 2013/14 to 2018/19 were not read (every source gives the same bar from 2013/14). Who takes the place of a Jong side
 * that goes down with its first team before 2020/21: no source (no such case in real life).
 *
 * The original. HolPrm::start_new_season 0x5f8610 calls the chain (vtable +0xb0, 0x689c20, at 0x5f8623), which moves
 * the rows of status 0 or 5 of the Eerste Divisie up (league_promote_and_relegate 0x689c80 at 0x689c60). Each period
 * of the Eerste Divisie is a league of its own (HolFirst +0xc, one per period): HolFirst::update_positions_and_status
 * 0x5f6d20 calls 0x684cd0 (0x5f6d29), takes the row of status 1 of the period's table as its winner (0x5f6d9f), and
 * gives that club status 1 in the main table unless it holds 0 or 5 (0x5f6ddb..0x5f6e09); the club then gets 0xfe in
 * the tables of the later periods (0x5f6e78). HolFirst::create_play_off_groups 0x5f6720 counts the rows of status 1
 * (0x5f675f..0x5f6774) and tops up to six with the rows of status 0xff in table order (0x5f677e..0x5f67cd). No step
 * has a reserve check: club_get_linked_b_team 0x540a50 links no Dutch club (the nation test 0x540af8..0x540b10). No
 * club moves into the Eerste Divisie from below (HolFirst::set_up_rules 0x5f6340 gives it no relegation places), and
 * nothing moves a Jong side down when its first team comes down.
 *
 * The fix (season games only):
 * - Before each 0x684cd0 of the Eerste Divisie and of its period leagues, a Jong row gets the original's mark 0xfc
 *   (st_mark, season_second_teams.c: 0x684cd0 gives a row of 0xfc neither the title, promotion nor the play-off).
 *   So the next first teams take the direct places, the next club of a period table wins the period, and the play-off
 *   top-up 0x5f6720 passes over the Jong rows (it takes rows of 0xff only, 0x5f6791).
 * - A table that a save of an older build left with a Jong side on an up status hands its places on (st_shift_up)
 *   before the play-off 0x5f6720 and before the chain. When the save comes after the play-off (HolFirst 0x5f6ee0 gives
 *   the winners 0, 0x5f707e, and the other rows 0xfe, 0x5f709d and 0x5f70b8), the play-off stays as it was played:
 *   the Jong side's direct place goes to the best placed club that is not already up (after_playoff).
 * - At the season change, before the chain: a Jong side in the Eredivisie (an older build let it go up) gets status 3
 *   in exchange for the best placed club of status 3, which stays up (the chain moves the rows in pairs). With more
 *   Jong sides than such clubs, the rest stay one more season (logged).
 * - After the chain: a Jong side in the Eerste Divisie whose first team is not in the Eredivisie (Art. 10 and Titel I
 *   (b)) goes to A Lower Division [0x9cf764], the division of the Dutch reserve sides in the data (League::
 *   relegate_club_to_division 0x6831a0). Its place goes to the Dutch reserve side of the highest reputation outside
 *   the two divisions whose first team plays in the Eredivisie (the game has no Tweede Divisie table, so it takes no
 *   place in a table); with none, to the Dutch first team of the highest reputation outside the two divisions, so
 *   the Eerste Divisie keeps its size (League::move_club_to_division 0x6830b0). A Jong side whose first team the
 *   name link does not find stays (logged).
 * - The club moves of a new game (season_clubs.c, through season_second_barred) fill no place of the Eredivisie or
 *   the Eerste Divisie with a Jong side. A Jong side that a season's club list names keeps its place (the lists
 *   hold the real clubs; fill groups and database places come after the lists).
 *
 * The link. No field of the club record names its first team, so the name links them: "Jong X", "X Jong Y" (the word
 * Jong left out) or "X U21" (and the other reserve last words of season_clubs_reserve_name) is the side of the Dutch
 * first team X. Exceptions, read in the Dutch clubs of the 18 databases of the research (run-logs/0.6.2-s111b/
 * research/db): "Willem II" and "Willem II Tilburg" are first teams; AZ's
 * first team is "AZ" ("AZ Alkmaar U21", "Jong AZ Alkmaar"); "FC Jong Twente" is the side of "FC Twente Enschede" in
 * March 2018. No fuzzy matching.
 *
 * Known effects, accepted: a Jong side that finishes first stays the champion in the competition history, as in real
 * life (League::record_season_honours 0x684640 takes table row 0, 0x68464b..0x68465b), but the title news goes to the
 * next club (0x684cd0 sends it with status 5, 0x685149..0x68529d). The cap of four Jong sides (Titel I (f)) is not checked: no
 * step here adds one more.
 *
 * Switches. RECOMP_NED_SECOND=off (test switch): the original. RECOMP_NED_SECOND_LOG=1: the marks, the places handed
 * on, the clubs that go up and down at the season change, and a count of the Jong sides before and after it.
 * RECOMP_TEST_NED_SECOND_TOP=1 (tests only): before each 0x684cd0 of the Eerste Divisie or a period league, each Jong
 * row gets 30 points more than the best other row (row +0x19, the column of comparator 0x683466), so it would win the
 * league and every period. RECOMP_TEST_NED_SECOND_DROP=1 (tests only): before each 0x684cd0 of the Eredivisie, each
 * first team of a Jong side of the Eerste Divisie gets 30 points fewer than the lowest other row, so it goes down.
 * The two test switches also act with RECOMP_NED_SECOND=off. RECOMP_TEST_NED_SECOND_OLD=<n> (tests only) plays as the
 * original until n Dutch season changes are done, as a game of an older build. RECOMP_TEST_NED_SECOND_LATE=1 (tests
 * only) gives no marks until the first Dutch season change, as a save of an older build made after the play-off. */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_ned_playoff.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEY_HOLLAND 0x9cf338u
#define KEY_PREMIER 0x9cf568u           /* the Eredivisie, "Dutch Premier Division" (read at 0x5f6395) */
#define KEY_FIRST 0x9cf56cu             /* the Eerste Divisie, "Dutch First Division" (read at 0x5f62ed) */
#define KEY_LOWER 0x9cf764u             /* "A Lower Division" (0x60efd0: the name at 0x610141, the store at 0x610153) */
#define FIRST_PERIODS 0x0cu             /* HolFirst: the period leagues (0x5f6d8f) */
#define PERIODS 4                       /* the period flags +0xee..+0xf1 (0x5f6d2e..0x5f6d66) */
#define LEAGUE_SORT_COLUMN 0xadu        /* the column of the sort (0x68341c) */
#define ROW_POINTS 0x19u                /* s16, + 14 * column (0x68345b..0x683466) */
#define F_MOVE_UP 0x006830b0u           /* League::move_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define F_MOVE_DOWN 0x006831a0u         /* League::relegate_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define TAG "ned second"

/* RECOMP_TEST_NED_SECOND_OLD=<n>: the Dutch season changes left to play as the original. Read once. */
static int old_left = -1;

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_NED_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_NED_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = st_env_is("RECOMP_NED_SECOND_LOG", "1");
    return log;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

/* RECOMP_TEST_NED_SECOND_LATE=1 (tests only): no marks and no hand-on before the play-off until the first Dutch
 * season change, as a save of an older build made after the play-off. */
static int chain_seen;

static int marks_on(void)
{
    static int late = -1;
    if (late < 0) late = st_env_is("RECOMP_TEST_NED_SECOND_LATE", "1");
    return rule_on() && (!late || chain_seen);
}

static int dutch(uint32_t club) { return st_readable(club) && st_nation_is(club, KEY_HOLLAND); }

static const char *name_of(uint32_t club) { return win_str(club + ST_CLUB_NAME); }

/* First teams that the name test reads as reserve sides. */
static const char *const FIRST_TEAMS[] = {"Willem II", "Willem II Tilburg"};

/* Reserve sides whose first team has another name than the rule below gives: {reserve side, first team}. */
static const char *const LINKS[][2] = {
    {"AZ Alkmaar U21", "AZ"},
    {"Jong AZ Alkmaar", "AZ"},
    {"FC Jong Twente", "FC Twente Enschede"},
};

/* Copies the words of name into out without the word Jong. Returns 1 when that word was there. */
static int without_jong(const char *name, char *out, size_t room)
{
    size_t o = 0;
    int jong = 0;
    for (const char *p = name; *p;) {
        while (*p == ' ') p++;
        const char *w = p;
        while (*p && *p != ' ') p++;
        size_t n = (size_t)(p - w);
        if (!n) break;
        if (n == 4 && !strncasecmp(w, "jong", 4)) {
            jong = 1;
            continue;
        }
        if (o && o + 1 < room) out[o++] = ' ';
        for (size_t k = 0; k < n && o + 1 < room; k++) out[o++] = w[k];
    }
    out[o] = 0;
    return jong;
}

/* 1 for a Dutch reserve side: a word Jong, or a reserve name of season_clubs_reserve_name (a last word U21, 2, II,
 * B and the other forms), and not one of FIRST_TEAMS. */
static int reserve_side(const char *name)
{
    for (size_t i = 0; i < sizeof FIRST_TEAMS / sizeof *FIRST_TEAMS; i++)
        if (!strcmp(name, FIRST_TEAMS[i])) return 0;
    char plain[64];
    return without_jong(name, plain, sizeof plain) || season_clubs_reserve_name(name);
}

static int jong_club(uint32_t club) { return dutch(club) && reserve_side(name_of(club)); }

/* The name of the first team of a reserve side (the rule): the words without Jong, and without a last reserve word
 * when the name had no Jong. */
static void first_name(const char *name, char *out, size_t room)
{
    if (without_jong(name, out, room)) return;
    char *sp = strrchr(out, ' ');
    if (sp) *sp = 0;
}

/* The first team of a Dutch reserve side (0 when the link finds none). */
static uint32_t first_team(uint32_t side)
{
    const char *name = name_of(side);
    char want[64];
    first_name(name, want, sizeof want);
    const char *alt = NULL;
    for (size_t i = 0; i < sizeof LINKS / sizeof *LINKS; i++)
        if (!strcmp(name, LINKS[i][0])) alt = LINKS[i][1];
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (club == side || !dutch(club)) continue;
        const char *n = name_of(club);
        if ((!strcmp(n, want) || (alt && !strcmp(n, alt))) && !reserve_side(n)) return club;
    }
    return 0;
}

/* 1 when league is one of the period leagues of the Eerste Divisie. */
static int period_of(uint32_t first, uint32_t league)
{
    uint32_t periods = st_readable(first) ? rd32(g_mem, first + FIRST_PERIODS) : 0;
    for (uint32_t k = 0; st_readable(periods) && k < PERIODS; k++)
        if (rd32(g_mem, periods + 4u * k) == league) return 1;
    return 0;
}

/* Test switches: the points of the overall column (or the column of the sort) of each row that pick() chooses go to
 * 30 above the best other row (up) or 30 below the lowest (down). */
static void test_points(uint32_t league, int (*pick)(uint32_t club), int up)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    uint32_t col = ROW_POINTS + 14u * (uint32_t)(int32_t)rd32(g_mem, league + LEAGUE_SORT_COLUMN);
    if (col > ST_ROW_SIZE - 2) return;
    int best = -32768, low = 32767, any = 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (pick(rd32(g_mem, row))) {
            any = 1;
            continue;
        }
        int p = (int16_t)rd16(g_mem, row + col);
        if (p > best) best = p;
        if (p < low) low = p;
    }
    if (!any || best == -32768) return;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (!pick(rd32(g_mem, row))) continue;
        int p = (int16_t)rd16(g_mem, row + col), want = up ? best + 30 : low - 30;
        if (up ? p < want : p > want) wr16(g_mem, row + col, (uint16_t)(int16_t)want);
    }
}

/* The first teams of the Jong sides of the Eerste Divisie (the test switch DROP), found once per call. */
static uint32_t parents[16];
static int nparents;

static void find_parents(void)
{
    nparents = 0;
    uint32_t first = st_comp_of(KEY_FIRST);
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; i < count && nparents < 16; i++) {
        uint32_t side = table + (uint32_t)i * ST_CLUB_SIZE, parent;
        if (rd32(g_mem, side + ST_CLUB_DIVISION) == first && jong_club(side) && (parent = first_team(side)))
            parents[nparents++] = parent;
    }
}

static int jong_parent(uint32_t club)
{
    for (int i = 0; i < nparents; i++)
        if (parents[i] == club) return 1;
    return 0;
}

static void ned_before_status(uint32_t league)
{
    static int top = -1, drop = -1;
    if (top < 0) {
        top = st_env_is("RECOMP_TEST_NED_SECOND_TOP", "1");
        drop = st_env_is("RECOMP_TEST_NED_SECOND_DROP", "1");
    }
    if (!season_game_changes()) return;
    uint32_t first = st_league_of(KEY_FIRST);
    if (!first) return;
    if (league == first || period_of(first, league)) {
        if (top) test_points(league, jong_club, 1);
        if (marks_on()) st_mark(league, jong_club, TAG, league == first ? "the Eerste Divisie" : "a period", log_on());
    } else if (drop && league == st_league_of(KEY_PREMIER)) {
        find_parents();
        test_points(league, jong_parent, 0);
    }
}

/* HolFirst::create_play_off_groups 0x5f6720, thiscall, no arguments. */
void o_005f6720(void);
RT_WRAP(005f6720, ned_second_playoff)
{
    uint32_t self = cpu.ecx;
    int ours = self && self == st_league_of(KEY_FIRST) && season_game_changes();
    if (ours && marks_on()) st_shift_up(self, jong_club, TAG, "before the play-off", log_on());
    /* From 2005/06 the knockout takes the place of the groups (issue 150, season_ned_playoff.c). In its place the wrap
     * does the original's `ret` (0x5f6ac2): with the return address left on the stack, advance_stage's `pop esi` at
     * 0x5f6715 loads 0x5f6715, and League::update_for_date then passes it as the league to the club records at
     * 0x684abc (the box of line 1265 at 0x44747b). */
    if (!ours || !season_ned_playoff_build(self)) o_005f6720();
    else if (!rt_unwinding) cpu.esp += 4;
    if (!ours || !log_on() || rt_unwinding) return;
    int n;
    uint32_t table = st_table_of(self, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (g_mem[row + ST_ROW_STATUS] == ST_PLAYOFF)
            fprintf(stderr, "recomp: " TAG ": play-off: %.51s%s\n", name_of(rd32(g_mem, row)),
                    jong_club(rd32(g_mem, row)) ? " (a Jong side)" : "");
    }
}

/* The Jong sides of a division, and its clubs. */
static void census(uint32_t div, int *clubs, int *sides)
{
    *clubs = *sides = 0;
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; st_readable(div) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (rd32(g_mem, club + ST_CLUB_DIVISION) != div) continue;
        ++*clubs;
        *sides += jong_club(club);
    }
}

static void log_census(const char *when)
{
    if (!log_on()) return;
    int c1, s1, c2, s2;
    census(st_comp_of(KEY_PREMIER), &c1, &s1);
    census(st_comp_of(KEY_FIRST), &c2, &s2);
    fprintf(stderr, "recomp: " TAG ": census %s: Eredivisie %d clubs, %d Jong sides; Eerste Divisie %d clubs, %d Jong "
            "sides\n", when, c1, s1, c2, s2);
}

/* A save of an older build made after the play-off (a row of 0xfe, 0x5f709d): each Jong side of status 5 or 0 gets
 * 0xfc, and its status goes to the best placed club that is not up, so the play-off winners keep their places. 1 when
 * the table is in that state. */
static int after_playoff(uint32_t league)
{
    int n, done = 0;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n && !done; i++) done = g_mem[table + (uint32_t)i * ST_ROW_SIZE + ST_ROW_STATUS] == 0xfe;
    if (!done) return 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        uint8_t s = g_mem[row + ST_ROW_STATUS];
        if ((s != ST_TITLE && s != ST_UP) || !jong_club(rd32(g_mem, row))) continue;
        g_mem[row + ST_ROW_STATUS] = ST_BARRED;
        for (int k = 0; k < n; k++) {
            uint32_t to = table + (uint32_t)k * ST_ROW_SIZE;
            uint8_t t = g_mem[to + ST_ROW_STATUS];
            if (t == ST_TITLE || t == ST_UP || t == ST_DOWN || t == ST_BARRED || jong_club(rd32(g_mem, to))) continue;
            g_mem[to + ST_ROW_STATUS] = s;
            if (log_on())
                fprintf(stderr, "recomp: " TAG ": %.51s takes status %d in place of a Jong side after the play-off\n",
                        name_of(rd32(g_mem, to)), s);
            break;
        }
    }
    return 1;
}

/* The clubs of the Eerste Divisie before the chain, then those of the Eredivisie (for the log). */
static char *was_first;
static int was_count;

/* Before the chain of HolPrm: a Jong side of the Eredivisie goes down in exchange for the best placed club of
 * status 3, which stays up. */
static void ned_before_chain(uint32_t self)
{
    free(was_first);                     /* left by a chain that unwound (no after_chain) */
    was_first = NULL;
    if (self != st_league_of(KEY_PREMIER) || !season_game_changes()) return;
    chain_seen = 1;
    log_census("before");
    int count;
    uint32_t clubs = st_clubs(&count), first_comp = st_comp_of(KEY_FIRST);
    was_first = log_on() && count ? calloc((size_t)count, 2) : NULL;
    was_count = count;
    uint32_t prm_comp = st_comp_of(KEY_PREMIER);
    for (int i = 0; was_first && i < count; i++) {
        uint32_t div = rd32(g_mem, clubs + (uint32_t)i * ST_CLUB_SIZE + ST_CLUB_DIVISION);
        was_first[i] = div == first_comp;
        was_first[count + i] = div == prm_comp;     /* was_prm */
    }
    if (!rule_on()) return;
    uint32_t first = st_league_of(KEY_FIRST);
    if (!after_playoff(first)) st_shift_up(first, jong_club, TAG, "at the season change", log_on());
    int n;
    uint32_t table = st_table_of(self, &n);
    int stay[64], nstay = 0, used = 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN && !jong_club(rd32(g_mem, row))) stay[nstay++] = i;
    }
    for (int i = n - 1; i >= 0; i--) {            /* the lowest placed Jong side first */
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN || !jong_club(club)) continue;
        if (used == nstay) {
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s stays in the Eredivisie one more season (no place "
                                  "goes down for it)\n", name_of(club));
            continue;
        }
        g_mem[row + ST_ROW_STATUS] = ST_DOWN;
        uint32_t up = table + (uint32_t)stay[used++] * ST_ROW_SIZE;
        g_mem[up + ST_ROW_STATUS] = ST_NONE;
        if (log_on())
            fprintf(stderr, "recomp: " TAG ": %.51s goes down from the Eredivisie, %.51s stays up\n", name_of(club),
                    name_of(rd32(g_mem, up)));
    }
}

/* The club that takes the place of a Jong side that went down: the Dutch reserve side of the highest reputation
 * outside the two divisions whose first team plays in the Eredivisie; else the Dutch first team of the highest
 * reputation outside them. Never gone (the side that just went down). */
static uint32_t successor(uint32_t gone)
{
    uint32_t prm = st_comp_of(KEY_PREMIER), first = st_comp_of(KEY_FIRST), best[2] = {0, 0};
    int rep[2] = {-1, -1};
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE, div = rd32(g_mem, club + ST_CLUB_DIVISION);
        if (club == gone || !dutch(club) || div == prm || div == first || !st_readable(div)) continue;
        int side = reserve_side(name_of(club));
        if (side) {
            uint32_t parent = first_team(club);
            if (!parent || rd32(g_mem, parent + ST_CLUB_DIVISION) != prm) continue;
        }
        int r = (int16_t)rd16(g_mem, club + ST_CLUB_REPUTATION), k = side ? 0 : 1;
        if (r > rep[k]) rep[k] = r, best[k] = club;
    }
    return best[0] ? best[0] : best[1];
}

/* After the chain of HolPrm: a Jong side whose first team is not in the Eredivisie leaves the Eerste Divisie. */
static void ned_after_chain(uint32_t self)
{
    if (self != st_league_of(KEY_PREMIER) || !season_game_changes()) return;
    int count;
    uint32_t table = st_clubs(&count), prm = st_comp_of(KEY_PREMIER), first = st_comp_of(KEY_FIRST),
             lower = st_comp_of(KEY_LOWER);
    int lower_ok = (int32_t)rd32(g_mem, KEY_LOWER) >= 0 && st_readable(lower);
    if (rule_on() && !lower_ok && log_on())
        fprintf(stderr, "recomp: " TAG ": no A Lower Division in this database, no Jong side moves down\n");
    for (int i = 0; rule_on() && lower_ok && i < count && !rt_unwinding; i++) {
        uint32_t side = table + (uint32_t)i * ST_CLUB_SIZE;
        if (rd32(g_mem, side + ST_CLUB_DIVISION) != first || !jong_club(side)) continue;
        uint32_t parent = first_team(side);
        if (!parent) {
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s stays: no first team found by name\n", name_of(side));
            continue;
        }
        if (rd32(g_mem, parent + ST_CLUB_DIVISION) == prm) continue;
        uint32_t a[3] = {side, lower, 1};
        st_call_this(F_MOVE_DOWN, self, 3, a);
        if (rt_unwinding) break;
        uint32_t next = successor(side);
        if (log_on())
            fprintf(stderr, "recomp: " TAG ": %.51s goes down from the Eerste Divisie (its first team %.51s left the "
                    "Eredivisie), %.51s takes its place\n", name_of(side), name_of(parent),
                    next ? name_of(next) : "no club");
        if (next) {
            uint32_t b[3] = {next, first, 1};
            st_call_this(F_MOVE_UP, self, 3, b);
            if (rt_unwinding) break;
        }
        if (log_on())
            fprintf(stderr, "recomp: " TAG ": now %.51s in %.51s, %.51s in %.51s\n", name_of(side),
                    win_str(rd32(g_mem, side + ST_CLUB_DIVISION) + 4), next ? name_of(next) : "no club",
                    next ? win_str(rd32(g_mem, next + ST_CLUB_DIVISION) + 4) : "-");
    }
    for (int i = 0; was_first && !rt_unwinding && i < was_count && i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE, div = rd32(g_mem, club + ST_CLUB_DIVISION);
        if (was_first[i] && div == prm)
            fprintf(stderr, "recomp: " TAG ": %.51s goes up to the Eredivisie%s\n", name_of(club),
                    jong_club(club) ? " (a Jong side)" : "");
        if (was_first[was_count + i] && div != prm)
            fprintf(stderr, "recomp: " TAG ": %.51s leaves the Eredivisie\n", name_of(club));
    }
    free(was_first);
    was_first = NULL;
    if (!rt_unwinding) {
        log_census("after");
        if (old_left > 0) old_left--;
    }
}

/* The club moves of a new game: no Jong side fills a place of the Eredivisie or the Eerste Divisie (Titel I (b): a
 * fill could put one beside its own first team). */
static int ned_barred_at_start(uint32_t club, uint32_t div)
{
    if (switched_off() || !st_readable(div) || !jong_club(club)) return 0;
    int32_t id = (int32_t)rd32(g_mem, div);
    return id == (int32_t)rd32(g_mem, KEY_PREMIER) || id == (int32_t)rd32(g_mem, KEY_FIRST);
}

static const SecondTeamHooks ned_hooks = {ned_before_status, ned_before_chain, ned_after_chain, ned_barred_at_start};
SEASON_SECOND_HOOKS(ned_hooks)
