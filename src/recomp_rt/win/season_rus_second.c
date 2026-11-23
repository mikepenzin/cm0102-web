/* Russia: second teams ("-2" sides) of Russian clubs (open item 120, 0.7.0).
 *
 * The real rule. A second team plays below its club's first team: it never goes up into the Premier League or its
 * play-off (the next club takes the place), it goes up from the third tier only while its first team plays in the
 * Premier League, two teams of one club never play in one division, and a second team goes down when its first team
 * comes down into its division, as one of the clubs that go down. Sources (read 3 November 2026, notes in
 * run-logs/0.6.2-s111c-rus/research.md):
 * - FNL Regulation 2015/16 art. 4.3 and 4.4 (RFS executive decision 166/4 of 24 June 2015), FNL Regulations 2021/22
 *   and 2022/23 art. 3.3, 3.3.1 and 3.4 (1fnl.ru PDFs): a second team of an RPL club in places 1 to 6 gets no right
 *   to go up or to the play-off, the clubs below it get it; when its club comes down into the FNL it goes down
 *   whatever its place and counts among the clubs that go down;
 * - First League Regulation 2024/25 art. 3.3.1 and 3.4: the same. Second League Division A 2023/24 and 2024/25 art.
 *   3.19 and 3.41, 2025/26 and 2026/27 art. 3.12, 3.14 and 3.30: two teams of one club never play in one competition
 *   or go up into a league where another team of the club plays, the second team of a First League club does not go
 *   up while its first team plays there (the next club does), and a second team counts among the clubs that go down
 *   when its first team comes down into its league;
 * - 2023/24, 2025/26 and 2026/27: the First League Regulations have no clause on a second team's promotion to the
 *   Premier League. The bar there is an inference from the Division A rule above (two teams of one club never in one
 *   league). No second team played the First League in those seasons;
 * - 2001 to 2014/15: no regulation text found. Russian Wikipedia (Система футбольных лиг России, uncited) says that
 *   a second team could go up from the PFL to the FNL only while its first team played in the top flight; none
 *   played the FNL before 2015/16 (Spartak-2 went up from PFL West 2014/15). The same rule is used.
 * The zones are not played, so a first team that comes down into its second team's zone changes nothing here.
 *
 * The original. The game plays two Russian divisions: the Premier League (RusPrm, key 0x9cf930) and the First
 * Division (RusFirst, key 0x9cf92c). The six zones of the Second Division (keys 0x9cf914..0x9cf928) only hold clubs.
 * RusPrm::start_new_season 0x7eb800 first calls the promotion chain (vtable +0xb0, League::process_promotion_
 * relegation_chain 0x689c20, at 0x7eb813), which moves the rows of status 0 or 5 of the First Division up and the
 * rows of status 3 of the Premier League down (league_promote_and_relegate 0x689c80). Then it calls
 * rus_prm_exchange_first_and_second_division_clubs 0x7ebe20 (at 0x7eb81b). That function lists every club whose
 * division is one of the six zones (0x7ebf87..0x7ebfa8), returns with no move when the list holds fewer than 3
 * (0x7ebfeb), sorts the list by reputation and shuffles the top 3 (comp_util_sort_and_shuffle_top_clubs 0x4afbd0 at
 * 0x7ec009), moves the first club of zones 1 or 2, of zones 3 or 4 and of zones 5 or 6 into the First Division
 * (League::move_club_to_division 0x6830b0 at 0x7ec0be), and then sends the clubs of the last three rows of the First
 * Division table (0x6826d0: table +0xb1 plus the row times 0x3b, rows +0x3e - 1, - 2 and - 3) to zones 0x9cf918,
 * 0x9cf91c and 0x9cf924 (League::relegate_club_to_division 0x6831a0 at 0x7ec100, 0x7ec11c, 0x7ec138). It reads only
 * the club of each row (+0), so the rows give the order of last season's table; neither move writes the table. No
 * step tests a second team: club_get_linked_b_team 0x540a50 holds pairs of Spain, Germany and Portugal only (the
 * nation test at 0x540af6..0x540b12). So a "-2" side with a high reputation goes up from its zone in every season,
 * wins promotion to the Premier League by table, and can play in its own club's division.
 *
 * The link. No field of the club record names the club of a second team. A Russian club is a second team when its
 * name, in plain words (letters and digits, without the words "FK", "FC" and the like), has a word "2", "II" or "M"
 * ("Zenit 2 St. Petersburg", "Arsenal-2 Tula", "CSKA Moscow II", "Fakel-M Voronezh"; not "Krasnodar-2000"), and a
 * Russian club whose plain name is the same without that word exists (its parent). Two small tables, by exact name:
 * the parent of the names that differ from it ("Torpedo 2 Moskau" for Torpedo Moscow), and first teams whose names
 * have such a word ("Volgar-Gazprom-2 Astrakhan", the club of Astrakhan in 2009 to 2011; "FK Orenburg 2", FK Orenburg
 * itself in the March 2018 database, the club id of "FK Orenburg" of 2023, so "FK Orenburg-2" there links to it;
 * "Saturn-2 Moscow Oblast", an independent club from 2011). A club with no parent in the data is no second team here.
 * Evidence: the names of every test database, run-logs/0.6.2-s111c-rus/namecheck.txt. The links are kept for one game
 * day (a cache by club table, count and date), so the hook before each 0x684cd0 costs no scan of the clubs.
 *
 * The fix, season games only (season_game_changes):
 * - the zone exchange: a second team that may not play in the First Division is no part of the list of 0x7ebe20
 *   (for the call its division reads as none, then it gets its zone back), so the next club of its zones goes up.
 *   It may not when its club plays below the Premier League, or when another second team of its club stays in the
 *   First Division, or stands before it (higher reputation, then the lower club record) in a zone of another pair.
 *   With fewer than three clubs left in the list the original moves no club (0x7ebfeb); the zones hold far more;
 * - the same call: a second team in the First Division that may not stay there takes, for the call only, the club
 *   of the highest of the last three rows (row +0x3e - 3, then - 2, then - 1) that may stay, so the original sends
 *   it to that row's zone and the club of the row stays up: it counts among the clubs that go down. A second team
 *   beyond the three rows goes down after the call to zone 0x9cf918 (0x6831a0, as 0x7ec100), and the club of the
 *   zones with the highest reputation that may go up and did not just come down takes its place (0x6830b0);
 * - the First Division table: before each League::update_positions_and_status 0x684cd0 of RusFirst every second
 *   team gets the original's mark 0xfc (st_mark, season_second_teams.c), so the next club takes promotion. A save
 *   made after the table was complete: before the chain the places go on to the next clubs (st_shift_up);
 * - the Premier League (a game of an older build): before the chain each second team gets status 3 in exchange for
 *   the best placed club of status 3, which stays up (the chain moves pairs). It comes down into the First Division
 *   and goes on to a zone as above when it may not stay there either;
 * - the club moves of a new game (season_clubs.c) put no second team into the Premier League or the First Division
 *   to fill a place (they read the database's divisions, not those of the moves). A second team that the research
 *   list keeps there (Spartak-2 and Zenit-2 in 2015/16 to 2018/19) stays.
 *
 * Switches. RECOMP_RUS_SECOND=off (test) gives the original. RECOMP_RUS_SECOND_LOG=1 logs the clubs held, moved and
 * marked, and a census of each division before and after each season change. Tests only: RECOMP_TEST_RUS_SECOND_REP=1
 * gives each second team of the zones that may not go up the reputation 10000 and every other one 9000 before
 * 0x7ebe20, so the original takes them first (=allowed: the other way round); RECOMP_TEST_RUS_SECOND_PTS=1 gives each second team of the First
 * Division and the Premier League 500 more points before a 0x684cd0 while it has fewer than 500, so the original
 * promotes it or gives it the title;
 * RECOMP_TEST_RUS_SECOND_PARENT_DOWN=1 sends the parent of a second team of the First Division down from the Premier
 * League at the next season change (status 3, in exchange for the best placed club of status 3);
 * RECOMP_TEST_RUS_SECOND_OLD=<n> plays as the original until n Russian season changes are done, as a game of an older
 * build; RECOMP_TEST_RUS_SECOND_ALL_DOWN=1 sends every second team of the First Division down at the zone exchange
 * (more than three: the path after the call); RECOMP_TEST_RUS_SECOND_NO_MARK=1 gives no mark before 0x684cd0, as a save made after the table was complete,
 * so that the season change hands the places on. */
#include "win.h"
#include "season.h"
#include "season_names.h"
#include "season_second_teams.h"
#include "season_rus_second.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_RUSSIA 0x9cf448u
#define KEY_PREMIER 0x9cf930u
#define KEY_FIRST 0x9cf92cu
#define KEY_ZONE1 0x9cf914u             /* the six zones of the Second Division, 0x9cf914..0x9cf928 */
#define KEY_ZONE_DOWN 0x9cf918u         /* the zone of the first club that 0x7ebe20 sends down (0x7ec100) */
#define ZONES 6
#define GAME_DATE 0xae2c90u             /* s16 day of the year, s16 year */
#define NAME_ROOM 51                    /* the bytes of a club name that the rule reads */
#define ROW_POINTS 0x19u                /* s16: the points of a row in the sort 0x683410 (0x683466, kind +0xad 0) */
#define F_MOVE_UP 0x006830b0u           /* League::move_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define F_MOVE_DOWN 0x006831a0u         /* League::relegate_club_to_division (this unused), (club, comp, 1) ret 0xc */
#define TAG "rus second"

/* ---- switches ---- */

static int old_left = -1;               /* RECOMP_TEST_RUS_SECOND_OLD: season changes left to play as the original */

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_RUS_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_RUS_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = st_env_is("RECOMP_RUS_SECOND_LOG", "1");
    return log;
}

static int test_on(const char *name)
{
    const char *v = getenv(name);
    return v && !strcmp(v, "1");
}

/* ---- names ---- */

static int russian(uint32_t club) { return st_readable(club) && st_nation_is(club, KEY_RUSSIA); }

/* The first NAME_ROOM bytes of a club's name, ended. */
static void name_of(uint32_t club, char *out)
{
    const char *s = win_str(club + ST_CLUB_NAME);
    size_t n = 0;
    while (n < NAME_ROOM && s[n]) out[n] = s[n], n++;
    out[n] = 0;
}

/* The plain words of an ended name, lower case, joined by one space (the drop list of season_clubs.c plain_words,
 * copied: that function is static there). *token gets 1 when a word is "2", "ii" or "m", which out leaves out. */
static void plain(const char *in, char *out, size_t room, int *token)
{
    static const char *const drop[] = {"fc", "cf", "sc", "ac", "ec", "ca", "cd", "sd", "ud", "if", "fk", "sk", "bk",
                                       "afc", "club", "clube", "futebol", "football", "ltda", "de", "da", "do", "das",
                                       "dos", "the", "e", "y", "and"};
    char word[64];
    size_t n = 0, w = 0;
    out[0] = 0;
    *token = 0;
    for (const unsigned char *p = (const unsigned char *)in;; p++) {
        int ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) && w < sizeof word - 1) {
            word[w++] = (char)ch;
            continue;
        }
        if (w) {
            word[w] = 0;
            int skip = 0;
            for (size_t k = 0; k < sizeof drop / sizeof drop[0] && !skip; k++) skip = !strcmp(word, drop[k]);
            if (!skip && (!strcmp(word, "2") || !strcmp(word, "ii") || !strcmp(word, "m"))) {
                skip = 1;
                *token = 1;
            }
            if (!skip && n + w + 2 < room) {
                if (n) out[n++] = ' ';
                memcpy(out + n, word, w);
                n += w;
                out[n] = 0;
            }
            w = 0;
        }
        if (!*p) break;
    }
}

/* Second teams whose parent has another name in the data (plain words: the second team, then its parent). */
static const char *const other_parent[][2] = {
    {"torpedo moskau", "torpedo moscow"},         /* Torpedo 2 Moskau (April 2023 to December 2025) */
    {"chertanovo moskau", "chertanovo moscow"},   /* Chertanovo Moskau II, Chertanovo 2 Moskau */
    {"ural ekaterinburg", "ural yekaterinburg"},  /* Ural 2 Ekaterinburg */
    {"akron togliatti", "akron tolyatti"},        /* Akron-2 Togliatti (May 2026) */
};

/* First teams whose names have a word of a second team (exact names). */
static const char *const first_teams[] = {
    "Volgar-Gazprom-2 Astrakhan",   /* the club of Astrakhan, 2009 to 2011 */
    "FK Orenburg 2",                /* FK Orenburg in the March 2018 database (club id 4324, "FK Orenburg" in 2023) */
    "Saturn-2 Moscow Oblast",       /* an independent club from 2011 (research.md) */
};

/* The cache of the links: for each club of the table, its parent (0 for none). */
static uint32_t names_sig(uint32_t table, int count);
static uint32_t c_table, c_date, c_sig;
static int c_count = -1;
static uint32_t *c_parent;
static int *c_seconds, c_nseconds;      /* the clubs (indices) that have a parent */

static void cache_build(uint32_t table, int count)
{
    free(c_parent);
    free(c_seconds);
    c_parent = calloc((size_t)count, sizeof *c_parent);
    c_seconds = malloc(sizeof *c_seconds * (size_t)count);
    char (*names)[128] = malloc(sizeof *names * (size_t)count);
    char *token = calloc((size_t)count, 1), *rus = calloc((size_t)count, 1);
    c_nseconds = 0;
    if (!c_parent || !c_seconds || !names || !token || !rus) {
        free(c_parent);
        free(c_seconds);
        c_parent = NULL;
        c_seconds = NULL;
        count = 0;
    }
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (!russian(club)) continue;
        char raw[NAME_ROOM + 1];
        int t;
        name_of(club, raw);
        plain(raw, names[i], sizeof names[i], &t);
        for (size_t k = 0; t && k < sizeof first_teams / sizeof first_teams[0]; k++)
            if (!strcmp(raw, first_teams[k])) t = 0;
        rus[i] = 1;
        token[i] = (char)t;
    }
    for (int i = 0; i < count; i++) {
        if (!rus[i] || !token[i] || !names[i][0]) continue;
        const char *want = names[i];
        for (size_t k = 0; k < sizeof other_parent / sizeof other_parent[0]; k++)
            if (!strcmp(want, other_parent[k][0])) want = other_parent[k][1];
        for (int j = 0; j < count; j++)
            if (j != i && rus[j] && !token[j] && !strcmp(names[j], want)) {
                c_parent[i] = table + (uint32_t)j * ST_CLUB_SIZE;
                c_seconds[c_nseconds++] = i;
                break;
            }
    }
    free(names);
    free(token);
    free(rus);
    c_table = table;
    c_count = count;
    c_date = rd32(g_mem, GAME_DATE);
    c_sig = names_sig(table, count);
}

/* A short check of the names (the first, middle and last club), so that a load of another game with the same table,
 * count and date builds the links again. */
static uint32_t names_sig(uint32_t table, int count)
{
    uint32_t h = 2166136261u;
    for (int k = 0; count > 0 && k < 3; k++) {
        char raw[NAME_ROOM + 1];
        name_of(table + (uint32_t)(k == 0 ? 0 : k == 1 ? count / 2 : count - 1) * ST_CLUB_SIZE, raw);
        for (const char *p = raw; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    }
    return h;
}

/* The index of a club in the table (-1 when none), with the cache up to date. */
static int club_index(uint32_t club)
{
    int count;
    uint32_t table = st_clubs(&count);
    if (!table) return -1;
    if (table != c_table || count != c_count || rd32(g_mem, GAME_DATE) != c_date || names_sig(table, count) != c_sig)
        cache_build(table, count);
    if (!c_parent || club < table || (club - table) % ST_CLUB_SIZE) return -1;
    uint32_t i = (club - table) / ST_CLUB_SIZE;
    return i < (uint32_t)c_count ? (int)i : -1;
}

uint32_t season_rus_parent(uint32_t club)
{
    int i = club_index(club);
    return i < 0 ? 0 : c_parent[i];
}

/* ---- divisions ---- */

/* 0 the Premier League, 1 the First Division, 2 a zone, 3 any other division or none. */
static int level_of(uint32_t div)
{
    if (!st_readable(div)) return 3;
    int32_t id = (int32_t)rd32(g_mem, div);
    if (id == (int32_t)rd32(g_mem, KEY_PREMIER)) return 0;
    if (id == (int32_t)rd32(g_mem, KEY_FIRST)) return 1;
    for (uint32_t k = 0; k < ZONES; k++)
        if (id == (int32_t)rd32(g_mem, KEY_ZONE1 + 4u * k)) return 2;
    return 3;
}

static int level_of_club(uint32_t club) { return level_of(rd32(g_mem, club + ST_CLUB_DIVISION)); }

/* A second team that may not take promotion from the First Division (every second team: the Premier League is never
 * below its parent). */
static int barred_up(uint32_t club) { return season_rus_parent(club) != 0; }

/* 1 when club a stands before club b: a higher reputation, then the lower club record. */
static int before(uint32_t a, uint32_t b)
{
    int ra = (int16_t)rd16(g_mem, a + ST_CLUB_REPUTATION), rb = (int16_t)rd16(g_mem, b + ST_CLUB_REPUTATION);
    return ra != rb ? ra > rb : a < b;
}

/* The zone (0 to 5) of a club, or -1. */
static int zone_of(uint32_t club)
{
    uint32_t div = rd32(g_mem, club + ST_CLUB_DIVISION);
    if (!st_readable(div)) return -1;
    for (uint32_t k = 0; k < ZONES; k++)
        if ((int32_t)rd32(g_mem, div) == (int32_t)rd32(g_mem, KEY_ZONE1 + 4u * k)) return (int)k;
    return -1;
}

/* 1 when the club holds one of the last three rows of the First Division table (0x7ebe20 sends it down). */
static int in_last_rows(uint32_t club)
{
    int n;
    uint32_t table = st_table_of(st_league_of(KEY_FIRST), &n);
    for (int r = n - 3; table && n >= 3 && r < n; r++)
        if (rd32(g_mem, table + (uint32_t)r * ST_ROW_SIZE) == club) return 1;
    return 0;
}

/* A second team that may not stay in or go up to the First Division: its club plays below the Premier League, or
 * another second team of its club stays there (in the First Division and not in the last three rows; with two
 * there, the one that stands behind goes), or stands before it in a zone of another pair (0x7ebe20 takes one club
 * of each pair of zones, so two of one pair never go up together). */
static int barred_first(uint32_t club)
{
    uint32_t parent = season_rus_parent(club);
    if (!parent) return 0;
    if (level_of_club(parent) != 0) return 1;
    int self = level_of_club(club), zone = zone_of(club);
    for (int k = 0; k < c_nseconds; k++) {
        uint32_t other = c_table + (uint32_t)c_seconds[k] * ST_CLUB_SIZE;
        if (other == club || c_parent[c_seconds[k]] != parent) continue;
        int l = level_of_club(other);
        if (l == 1 && !in_last_rows(other) && (self != 1 || before(other, club))) return 1;
        if (l == 2 && self == 2 && zone_of(other) / 2 != zone / 2 && before(other, club)) return 1;
    }
    return 0;
}

static void census(const char *when)
{
    if (!log_on()) return;
    int count, n[2] = {0, 0}, s[2] = {0, 0};
    uint32_t table = st_clubs(&count);
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        int l = level_of_club(club);
        if (l > 1 || !russian(club)) continue;
        n[l]++;
        s[l] += season_rus_parent(club) != 0;
    }
    fprintf(stderr, "recomp: " TAG ": census %s: Premier League %d clubs, %d second teams; First Division %d clubs, "
            "%d second teams\n", when, n[0], s[0], n[1], s[1]);
}

/* ---- the First Division table: before each 0x684cd0 ---- */

static void before_status(uint32_t league)
{
    if (!league || !season_game_changes()) return;
    int first = league == st_league_of(KEY_FIRST);
    if (!first && league != st_league_of(KEY_PREMIER)) return;
    if (test_on("RECOMP_TEST_RUS_SECOND_PTS")) {
        int n;
        uint32_t table = st_table_of(league, &n);
        for (int i = 0; i < n; i++) {
            uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
            int pts = (int16_t)rd16(g_mem, row + ROW_POINTS);
            if (barred_up(rd32(g_mem, row)) && pts < 500) wr16(g_mem, row + ROW_POINTS, (uint16_t)(pts + 500));
        }
    }
    if (first && rule_on() && !test_on("RECOMP_TEST_RUS_SECOND_NO_MARK"))
        st_mark(league, barred_up, TAG, "the First Division", log_on());
}

/* ---- the promotion chain of the Premier League: before 0x689c20 ---- */

/* Up to k rows of status 3 that are no second teams, the best placed first, stay up (0xff); skip is left alone.
 * Returns how many. */
static int stay_up(uint32_t league, int k, uint32_t skip)
{
    int n, done = 0;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n && done < k; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] != ST_DOWN || club == skip || barred_up(club)) continue;
        g_mem[row + ST_ROW_STATUS] = ST_NONE;
        done++;
        if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s stays up in the Premier League\n", win_str(club + ST_CLUB_NAME));
    }
    return done;
}

/* RECOMP_TEST_RUS_SECOND_PARENT_DOWN=1: the parent of the first second team of the First Division table that plays
 * in the Premier League goes down, and a club of status 3 stays up for it. */
static void test_parent_down(uint32_t prm)
{
    int n, np;
    uint32_t first = st_table_of(st_league_of(KEY_FIRST), &n), top = st_table_of(prm, &np);
    for (int i = 0; i < n; i++) {
        uint32_t parent = season_rus_parent(rd32(g_mem, first + (uint32_t)i * ST_ROW_SIZE));
        if (!parent || level_of_club(parent) != 0) continue;
        for (int j = 0; j < np; j++) {
            uint32_t row = top + (uint32_t)j * ST_ROW_SIZE;
            if (rd32(g_mem, row) != parent || g_mem[row + ST_ROW_STATUS] == ST_DOWN) continue;
            if (!stay_up(prm, 1, parent)) return;
            g_mem[row + ST_ROW_STATUS] = ST_DOWN;
            fprintf(stderr, "recomp: " TAG ": test: %.51s goes down from the Premier League (its second team %.51s)\n",
                    win_str(parent + ST_CLUB_NAME), win_str(rd32(g_mem, first + (uint32_t)i * ST_ROW_SIZE) + ST_CLUB_NAME));
            return;
        }
    }
}

static void before_chain(uint32_t league)
{
    if (!league || league != st_league_of(KEY_PREMIER) || !season_game_changes()) return;
    census("before");
    if (test_on("RECOMP_TEST_RUS_SECOND_PARENT_DOWN")) test_parent_down(league);
    if (!rule_on()) return;
    st_shift_up(st_league_of(KEY_FIRST), barred_up, TAG, "at the season change", log_on());
    /* A second team in the Premier League (a game of an older build) goes down, in exchange for the best placed club
     * of status 3 (the chain moves pairs, 0x689c60); the lowest placed first. */
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = n - 1; i >= 0; i--) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (!barred_up(club)) continue;
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN) {
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s goes down from the Premier League by its place\n",
                                  win_str(club + ST_CLUB_NAME));
            continue;
        }
        if (!stay_up(league, 1, 0)) {
            if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s stays in the Premier League one more season (no "
                                  "place goes down for it)\n", win_str(club + ST_CLUB_NAME));
            continue;
        }
        g_mem[row + ST_ROW_STATUS] = ST_DOWN;
        if (log_on()) fprintf(stderr, "recomp: " TAG ": %.51s goes down from the Premier League\n",
                              win_str(club + ST_CLUB_NAME));
    }
}

/* ---- the club moves of a new game ---- */

static int barred_at_start(uint32_t club, uint32_t div)
{
    if (switched_off() || !russian(club)) return 0;
    uint32_t parent = season_rus_parent(club);
    if (!parent) return 0;
    return level_of(div) <= 1;
}

static const SecondTeamHooks rus_hooks = {before_status, before_chain, NULL, barred_at_start};
SEASON_SECOND_HOOKS(rus_hooks)

/* ---- the zone exchange 0x7ebe20, thiscall (RusPrm), no arguments ---- */

#define MAX_DOWN 64

void o_007ebe20(void);
RT_WRAP(007ebe20, rus_second_exchange)
{
    int count;
    uint32_t table = st_clubs(&count);
    if (!season_game_changes() || !count) {
        o_007ebe20();
        return;
    }
    int on = rule_on(), log = log_on(), all_down = test_on("RECOMP_TEST_RUS_SECOND_ALL_DOWN");
    uint32_t self = cpu.ecx, first = st_comp_of(KEY_FIRST);
    int *held = malloc(sizeof *held * (size_t)count);
    uint32_t *div_of = malloc(sizeof *div_of * (size_t)count);
    char *was = calloc((size_t)count, 1);       /* 1 the First Division, 2 a zone, before the call */
    char *barred = calloc((size_t)count, 1);
    int nheld = 0;
    if (!held || !div_of || !was || !barred) {
        free(held);
        free(div_of);
        free(was);
        free(barred);
        o_007ebe20();
        return;
    }
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        int l = level_of_club(club);
        was[i] = l == 1 ? 1 : l == 2 ? 2 : 0;
        barred[i] = (char)(l <= 2 && barred_first(club));
        if (all_down && l == 1 && season_rus_parent(club)) barred[i] = 1;
    }
    int rep = test_on("RECOMP_TEST_RUS_SECOND_REP") ? 1 : st_env_is("RECOMP_TEST_RUS_SECOND_REP", "allowed") ? 2 : 0;
    for (int i = 0; rep && i < count; i++) {
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (was[i] == 2 && season_rus_parent(club))
            wr16(g_mem, club + ST_CLUB_REPUTATION, (barred[i] == (rep == 1)) ? 10000 : 9000);
    }
    for (int i = 0; on && i < count; i++) {
        if (was[i] != 2 || !barred[i]) continue;
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        held[nheld] = i;
        div_of[nheld++] = rd32(g_mem, club + ST_CLUB_DIVISION);
        wr32(g_mem, club + ST_CLUB_DIVISION, 0);
    }
    /* Second teams of the First Division that may not stay: each takes, for the call, the highest of the last three
     * rows whose club may stay. The rest go down after the call (extra). */
    uint32_t rows_at[3], rows_was[3], own_at[3], down[MAX_DOWN];
    int nrows = 0, ndown = 0, nextra = 0, n;
    uint32_t extra[MAX_DOWN];
    uint32_t ft = st_table_of(st_league_of(KEY_FIRST), &n);
    for (int i = 0; on && i < count; i++) {
        if (was[i] != 1 || !barred[i]) continue;
        uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
        if (ndown == MAX_DOWN) {
            if (log) fprintf(stderr, "recomp: " TAG ": %.51s stays in the First Division one more season (more than %d "
                             "second teams)\n", win_str(club + ST_CLUB_NAME), MAX_DOWN);
            continue;
        }
        down[ndown++] = club;
        int at = -1;
        for (int r = n - 3; ft && n >= 3 && r < n && at < 0; r++)
            if (rd32(g_mem, ft + (uint32_t)r * ST_ROW_SIZE) == club) at = r;
        if (at >= 0) continue;                  /* the original sends it down */
        int slot = -1;
        for (int r = n - 3; ft && n >= 3 && r < n && slot < 0; r++) {
            uint32_t row = ft + (uint32_t)r * ST_ROW_SIZE, c = rd32(g_mem, row);
            int used = 0;
            for (int k = 0; k < nrows; k++) used |= rows_at[k] == row;
            int ci = club_index(c);
            if (!used && ci >= 0 && !barred[ci]) slot = r;
        }
        if (slot < 0) {
            extra[nextra++] = club;
            continue;
        }
        uint32_t row = ft + (uint32_t)slot * ST_ROW_SIZE, displaced = rd32(g_mem, row);
        rows_at[nrows] = row;
        rows_was[nrows] = displaced;
        /* The team's own row (none when it came down from the Premier League) takes the displaced club. */
        own_at[nrows] = 0;
        for (int r = 0; r < n && !own_at[nrows]; r++)
            if (rd32(g_mem, ft + (uint32_t)r * ST_ROW_SIZE) == club) {
                own_at[nrows] = ft + (uint32_t)r * ST_ROW_SIZE;
                wr32(g_mem, own_at[nrows], displaced);
            }
        wr32(g_mem, row, club);
        nrows++;
        if (log) fprintf(stderr, "recomp: " TAG ": %.51s takes row %d of the First Division for the zone exchange in "
                         "place of %.51s\n", win_str(club + ST_CLUB_NAME), slot + 1, win_str(displaced + ST_CLUB_NAME));
    }
    o_007ebe20();
    /* The rows back as they were (neither move of 0x7ebe20 writes the table): the team to its own row, the displaced
     * club to the row it lent. */
    for (int k = nrows - 1; k >= 0; k--) {
        if (own_at[k]) wr32(g_mem, own_at[k], rd32(g_mem, rows_at[k]));
        wr32(g_mem, rows_at[k], rows_was[k]);
    }
    for (int k = 0; k < nheld; k++) {
        uint32_t club = table + (uint32_t)held[k] * ST_CLUB_SIZE;
        wr32(g_mem, club + ST_CLUB_DIVISION, div_of[k]);
        if (log) {
            uint32_t parent = season_rus_parent(club), pd = rd32(g_mem, parent + ST_CLUB_DIVISION);
            fprintf(stderr, "recomp: " TAG ": %.51s stays in %.51s (its club %.51s plays in %.51s)\n",
                    win_str(club + ST_CLUB_NAME), win_str(div_of[k] + 4), win_str(parent + ST_CLUB_NAME),
                    st_readable(pd) ? win_str(pd + 4) : "no division");
        }
    }
    if (!rt_unwinding) {
        /* Second teams beyond the three rows: down to the zone of 0x7ec100, and the next club of the zones up. */
        uint32_t zone = st_comp_of(KEY_ZONE_DOWN);
        for (int k = 0; k < nextra && !rt_unwinding; k++) {
            if (rd32(g_mem, extra[k] + ST_CLUB_DIVISION) != first) continue;
            uint32_t a[3] = {extra[k], zone, 1};
            st_call_this(F_MOVE_DOWN, self, 3, a);
            int best = -1;
            for (int i = 0; i < count; i++) {
                uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
                if (was[i] == 1 || level_of_club(club) != 2 || !russian(club) || barred[i] ||
                    (best >= 0 && !before(club, table + (uint32_t)best * ST_CLUB_SIZE)))
                    continue;
                best = i;
            }
            if (best < 0) continue;
            uint32_t b[3] = {table + (uint32_t)best * ST_CLUB_SIZE, first, 1};
            st_call_this(F_MOVE_UP, self, 3, b);
            if (log) fprintf(stderr, "recomp: " TAG ": %.51s goes up to the First Division in place of %.51s\n",
                             win_str(b[0] + ST_CLUB_NAME), win_str(extra[k] + ST_CLUB_NAME));
        }
        for (int k = 0; log && k < ndown; k++)
            fprintf(stderr, "recomp: " TAG ": %.51s %s\n", win_str(down[k] + ST_CLUB_NAME),
                    rd32(g_mem, down[k] + ST_CLUB_DIVISION) == first ?
                    "stays in the First Division (a barred second team)" : "went down from the First Division");
        for (int i = 0; log && i < count; i++) {
            uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
            if (was[i] == 1 || rd32(g_mem, club + ST_CLUB_DIVISION) != first) continue;
            uint32_t parent = season_rus_parent(club);
            fprintf(stderr, "recomp: " TAG ": %.51s goes up to the First Division%s\n", win_str(club + ST_CLUB_NAME),
                    !parent ? "" : barred_first(club) ? " (a barred second team)" : " (a second team)");
        }
        if (log && !on)
            for (int i = 0; i < count; i++) {
                uint32_t club = table + (uint32_t)i * ST_CLUB_SIZE;
                if (rd32(g_mem, club + ST_CLUB_DIVISION) == first && barred_first(club))
                    fprintf(stderr, "recomp: " TAG ": %.51s stays in the First Division (a barred second team)\n",
                            win_str(club + ST_CLUB_NAME));
            }
        census("after");
        if (old_left > 0) old_left--;
    }
    free(held);
    free(div_of);
    free(was);
    free(barred);
}
