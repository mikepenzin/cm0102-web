/* Spain and Portugal: B and C sides newer than the key pairs of 0x540a50 (open items 121 and 122, 0.7.0).
 *
 * The real rule. Spain (RFEF Reglamento General art. 41, Circular 34 of 2025/26; Bases de Primera y Segunda División
 * 2025/26 art. 9 and 16; Bases de Primera Federación 2021/22, 2023/24 and 2025/26, all read on 3 November 2026): a
 * B or C side never plays in the division of a team above it in its club (its "cadena"), it does not go up into
 * that division and the next club takes the place, and when the first team comes down into the B side's division
 * the B side goes down one division. Before 2021 only one secondary source was found (it says the same). Portugal
 * (press and Wikipedia only, the league texts were not read): from 2012/13 a B side may play the Segunda Liga but
 * never goes up into the Primeira Liga (Porto B, champion 2015/16, stayed down and the next clubs went up), and it
 * goes down when its first team comes down into its division. Sources and the seasons with no text:
 * run-logs/0.6.2-s111b/research/ESP.md and POR.md.
 *
 * The original. Every reserve check of the Spanish and Portuguese league code goes through club_get_linked_b_team
 * 0x540a50, cdecl (club, int *out, need_league). It returns 0 for a club of a nation other than Spain [0x9cf488],
 * Germany [0x9cf314] or Portugal [0x9cf434] (0x540af6..0x540b12), and with need_league 1 for a club whose +0x244 is
 * 0 (0x540ad8..0x540af5; +0x244 is 1 for a club with a pair, set at the start of a game by 0x541e60 at 0x541ea2).
 * Then it compares the club id with 83 senior key ids (0x540b1b..0x54129c, each a jump to 0x541871, which writes
 * *out = 1 at 0x54187c and returns the club of the paired B key) and 82 B side key ids (jumps to 0x54129c, which
 * writes *out = 0 at 0x5412a7 and returns the paired senior club). With no match it returns 0 (0x541868). The key ids
 * come from exact names of 2001 (src/cm3/key_ids_table.c). So *out tells the role: 1 when the club returned is the
 * reserve side of the club asked. There is no C side and no Villarreal, Getafe, Granada, Almería, Zaragoza or
 * Oviedo B side, and no Portuguese B side but Benfica, Braga, Marítimo, Porto and Sporting B.
 *
 * Its callers in the league code:
 * - SpaFirst::resolve_b_team_clashes 0x850970 and PorPrm::apply_b_team_promotion_limits 0x7d0320 (start_new_season
 *   0x84feb0 at 0x84fed1 and 0x7cf9e0 at 0x7cfa01, when the third tier is loaded, before the chain at +0xb0). Each
 *   asks for the reserve side of a club (it reads the result only when *out is 1: 0x850a1b, 0x850c8f, 0x7d03c3,
 *   0x7d0618). First the clubs of the top division (0x850a09, 0x7d03b1): a reserve side in the second division
 *   whose first team goes down (row status 3) gets 0xfb and goes down, and a row of status 3 of the second
 *   division gets 0xfe and stays (0x850ab6, 0x850e99..0x850ebf); a reserve side in the second division whose first
 *   team stays gets 0xfe, and when it held a promotion place (status 0) the first row of 0xff takes it
 *   (0x850c2a, 0x850e61..0x850e93); a reserve side in a group of the third tier is held the same way when its first
 *   team goes down (0x850b28, 0x850c2a). Then the clubs of the second division (0x850c7d, 0x7d0606): a reserve side
 *   in the third tier goes down (status 3, 0x850dd1) when the club goes down, else it is held (0xfe, 0x850e36).
 * - The callers that ask for the senior side of a club and read the result only when *out is 0: SpaSecond::
 *   create_table 0x855b40 (0x855c17: the row of a B side whose senior plays La Liga gets 0xfc, 0x855c55, so it
 *   gets no promotion place in the season, League::update_positions_and_status 0x684cd0 at 0x684dea..0x684e53),
 *   PorSecond::create_table 0x7d1520 (0x7d15f7, 0xfc at 0x7d1635), the exchanges with no third tier
 *   SpaFirst 0x8500c0 (0x8500fe, 0x8501de, 0x85029e) and PorPrm 0x7cfd30 (0x7cfe5c), and the pool of the
 *   Spanish third tier spa_first_exchange_second_division_b_clubs 0x850300 (0x8504c9: a B side whose senior plays
 *   in the tier stays out, 0x8504db..0x8504ec). The season code asks the same through the runtime
 *   (season_playoffs.c barred, season_divisions.c group_pool_club, return address RT_RETURN_ADDR).
 * So the original holds the whole rule; it only lacks the pairs of the newer sides.
 *
 * The fix (season games only, season_game_changes). The wrap of 0x540a50 adds a link by name when the original
 * finds none, for a Spanish or Portuguese club, and only for the calls of the league code above (by their return
 * addresses). The other callers of 0x540a50 (about 120: youth, staff, finance, the squad trim 0x541e60 that sets
 * +0x244) see the original. need_league is not read for the new links (no new side has +0x244 set; setting it
 * would let 0x541e60 trim squads). The link: a B side "<name> B" belongs to the club named "<name>"; a C side
 * "<name> C" to "<name> B", or to "<name>" when the club has no B side. The names are compared whole, without case,
 * within the nation (the data has no field that names a reserve side's club). Over the databases of the research
 * (run-logs/0.6.2-s111b/research/db) this gives every Spanish or Portuguese B or C side its club, except the sides of
 * a small table (NAMED below: CF Peralada-Girona B, C.D. Numancia B, Real Sociedad C and others, whose names do not
 * hold their club's name) and "Deportivo Alavés B" (club "C.D. Alavés"), which the key pairs hold. No fuzzy match.
 * - A call that asks for the senior side (the role *out 0) gets the club above: the B side's first team, the C
 *   side's B side. *out is written 0.
 * - A call of 0x850970 or 0x7d0320 (the role *out 1) gets the club below: a first team's B side (or its C side
 *   with no B side), a B side's C side. *out is written 1. A B side of the key pairs with a C side gets its C side
 *   there too (the original gives the B side's senior, which those calls skip).
 * Left out: SpaLower::create_table 0x8533d0 (the Spanish Lower Division, a tier the season code does not play) reads
 * the pairs at 0x85351e, 0x8535e4 and 0x8536cb as the original; the cups (0x84d975.., 0x7ce308..) as well.
 * So a newer side gets the 0xfc mark of the season, is held from promotion and from the play-off, and goes down
 * with its first team; a C side the same against its B side.
 *
 * Portugal's pool. por_prm_exchange_second_b_with_lower 0x7cff70 (the form with the database's zones, before
 * 2021/22) moves the Portuguese clubs of the Third Division [0x9cf620] and A Lower Division [0x9cf764]
 * (0x7d0022..0x7d0065) into the Second Division B by reputation with no pair test (no call of 0x540a50 in it).
 * Around that call (season_divisions.c group_refill) a B or C side whose club above plays the Second Division B or
 * lower has no division, so it is no part of the pool, as the Spanish pool rule 0x8504c9; then it gets its division
 * back. From 2021/22 group_pool_club holds the same test.
 *
 * Old saves (Max, 3 November 2026: a second team already above its allowed level moves down at the next season
 * change, as Norway). A game of 0.7.0 or earlier can hold a newer B side in its first team's division, or above it.
 * At the season change, around 0x850970 and 0x7d0320:
 * - before: a reserve side of the second division whose club above plays the top or the second division and that
 *   holds the title, promotion or the play-off (a save made after the table was complete, with no 0xfc) gets 0xfc
 *   and the best placed row of 0xff takes its place;
 * - after: a reserve side that shares a division with a club above it, or plays above it, goes down one division:
 *   in the top division status 3, and the best placed club of status 3 stays up (0xff), so the chain (pairwise)
 *   keeps the sizes; in the second division status 3 (the original's 0xfb, which it turns into 3 at its end), and
 *   the best placed row of status 3 gets 0xfe, as the original (0x850ab6, 0x850ebc); in a group of the third tier
 *   status 3 (the original's 0x850dd1; the refill fills the group). A place of promotion that it held goes to the
 *   first row of 0xff, as 0x850e8e.
 * A C side counts against the lowest of its B side and its first team. A reserve side whose club above goes up out
 * of the same division at this change stays. A reserve side two divisions too high goes down one division at each
 * season change. The tiers below the third are not checked.
 *
 * Club moves of a new game (season_clubs.c): a B or C side does not take a place of the top three tiers in the
 * division of a club above it or higher, by the divisions that the moves plan (season_esp_barred).
 *
 * Switches: RECOMP_ESP_SECOND=off (test switch) gives the original everywhere. RECOMP_ESP_SECOND_LOG=1 logs each new
 * link, each move, and a census of the reserve sides at each season change. RECOMP_TEST_ESP_SECOND_PUT="<club>:<row>
 * [@<k>][,...]" (tests only) swaps the club of each name with the club of that row (1 = top) of its own table at the
 * first (or the k-th) season change of its nation, before the original reads the table: the statuses stay with the
 * rows.
 * RECOMP_TEST_ESP_SECOND_REP=1 (tests only) gives the Portuguese B and C sides of the pool of 0x7cff70 the highest
 * reputation before the call.
 * RECOMP_TEST_ESP_SECOND_OLD=<n> (tests only) plays as the original until n season changes of each nation are done,
 * as a game of an older build. */
#include "win.h"
#include "season_esp_second.h"
#include "season_names.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_NAME 0x04u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define CLUB_REPUTATION 0x80u
#define LEAGUES 0xadadfcu               /* the competition objects, by competition id */
#define LEAGUE_CLUBS 0x3eu              /* s16 rows */
#define LEAGUE_TABLE 0xb1u              /* rows of 0x3b bytes: +0 the club, +0x37 the status */
#define LEAGUE_STAGES 0x0cu             /* the stage objects of a tier of groups (0x8509a3, 0x7d0343) */
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define ST_TITLE 5
#define ST_UP 0
#define ST_PLAYOFF 1
#define ST_DOWN 3
#define ST_HELD 0xfeu                   /* 0x850c2a: stays where it is */
#define ST_BARRED 0xfcu
#define ST_NONE 0xffu
#define F_LINKED 0x00540a50u
#define KEY_LOWER 0x9cf764u             /* A Lower Division */

typedef struct {
    const char *name;
    uint32_t nation, top, second, third, below;     /* keys: the nation, the four tiers (record of the third) */
    int changes;                                     /* season changes seen (RECOMP_TEST_ESP_SECOND_OLD) */
} Nation;

static Nation SPAIN = {"Spain", 0x9cf488u, 0x9cf624u, 0x9cf628u, 0x9cf7b8u, KEY_LOWER, 0};
static Nation PORTUGAL = {"Portugal", 0x9cf434u, 0x9cf60cu, 0x9cf610u, 0x9cf7c0u, 0x9cf620u, 0};

/* The return addresses of the calls of 0x540a50 that the link serves. */
static const uint32_t ASK_SENIOR[] = {0x855c1cu, 0x850103u, 0x8501e3u, 0x8502a3u, 0x8504ceu, 0x7cfe61u, 0x7d15fcu,
                                      RT_RETURN_ADDR};
static const uint32_t ASK_RESERVE[] = {0x850a0eu, 0x850c82u, 0x7d03b6u, 0x7d060bu};

static int readable(uint32_t p) { return rt_readable(p); }

static int env_is(const char *name, const char *value)
{
    const char *v = getenv(name);
    return v && !strcmp(v, value);
}

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = env_is("RECOMP_ESP_SECOND_LOG", "1");
    return log;
}

static int old_changes(void)
{
    static int n = -1;
    if (n < 0) {
        const char *v = getenv("RECOMP_TEST_ESP_SECOND_OLD");
        n = v ? atoi(v) : 0;
        if (n < 0) n = 0;
    }
    return n;
}

static Nation *nation_of(uint32_t club)
{
    uint32_t nation = rd32(g_mem, club + CLUB_NATION);
    if (!readable(nation)) return NULL;
    int32_t id = (int32_t)rd32(g_mem, nation);
    if (id == (int32_t)rd32(g_mem, SPAIN.nation)) return &SPAIN;
    if (id == (int32_t)rd32(g_mem, PORTUGAL.nation)) return &PORTUGAL;
    return NULL;
}

static int rule_on(const Nation *n)
{
    static int off = -1;
    if (off < 0) off = env_is("RECOMP_ESP_SECOND", "off");
    /* RECOMP_TEST_ESP_SECOND_OLD=<k>: off from the start to the end of the k-th season change of the nation (the
     * season after it plays with the fix, as a game of an older build loaded then). */
    return !off && n && (!old_changes() || n->changes > old_changes()) && season_game_changes();
}

static const char *name_of(uint32_t club) { return win_str(club + CLUB_NAME); }

/* The length of a name without trailing spaces (at most 51). */
static size_t name_len(const char *s)
{
    size_t n = 0;
    while (n < 51 && s[n]) n++;
    while (n && s[n - 1] == ' ') n--;
    return n;
}

/* 'b' or 'c' when the last word of a name is B or C, with *base the length of the name before " B"; else 0. */
static char reserve_kind(uint32_t club, size_t *base)
{
    const char *s = name_of(club);
    size_t n = name_len(s);
    if (n < 3 || s[n - 2] != ' ') return 0;
    char k = (char)tolower((unsigned char)s[n - 1]);
    if (k != 'b' && k != 'c') return 0;
    *base = n - 2;
    return k;
}

/* The Spanish and Portuguese clubs by name (lower case), sorted: built at the first look-up after index_reset (each
 * season change, each exchange of the pool, the club moves of a new game) or when the club table moves. */
typedef struct {
    char name[52];
    const Nation *n;
    uint32_t club;
} Named;

static Named *index_rows;
static int index_count = -1;
static uint32_t index_table;

static void index_reset(void) { index_count = -1; }

static void lower_copy(char *out, const char *in, size_t len)
{
    for (size_t i = 0; i < len; i++) out[i] = (char)tolower((unsigned char)in[i]);
    out[len] = 0;
}

static int by_name(const void *a, const void *b) { return strcmp(((const Named *)a)->name, ((const Named *)b)->name); }

static void index_build(void)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (index_count >= 0 && table == index_table) return;
    free(index_rows);
    index_rows = NULL;
    index_count = 0;
    index_table = table;
    if (!readable(table) || count <= 0) return;
    index_rows = malloc(sizeof *index_rows * (size_t)count);
    for (int i = 0; index_rows && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        const Nation *n = nation_of(club);
        if (!n) continue;
        Named *e = &index_rows[index_count++];
        lower_copy(e->name, name_of(club), name_len(name_of(club)));
        e->n = n;
        e->club = club;
    }
    qsort(index_rows, (size_t)index_count, sizeof *index_rows, by_name);
}

/* The club of the nation n named name[0..len) followed by suffix (NULL: none, else " B" or " C"), or 0. Names
 * compare whole and without case. */
static uint32_t find_named(const Nation *n, const char *name, size_t len, const char *suffix)
{
    char want[64];
    size_t extra = suffix ? strlen(suffix) : 0;
    if (len + extra > 51) return 0;
    lower_copy(want, name, len);
    if (suffix) lower_copy(want + len, suffix, extra);
    index_build();
    int lo = 0, hi = index_count;
    while (lo < hi) {                                   /* the first row not below want */
        int mid = (lo + hi) / 2;
        if (strcmp(index_rows[mid].name, want) < 0) lo = mid + 1;
        else hi = mid;
    }
    for (int i = lo; i < index_count && !strcmp(index_rows[i].name, want); i++)
        if (index_rows[i].n == n) return index_rows[i].club;
    return 0;
}

/* Reserve sides whose name does not hold their club's name (the databases of the research, run-logs/0.6.2-s111b/
 * research/db: in a division that the game plays or in a pool of one), with the club above each, in the order
 * looked for. No fuzzy match. */
static const struct { const char *club, *above[2]; } NAMED[] = {
    {"CF Peralada-Girona B", {"Girona FC", NULL}},              /* March 2018, Second Division B3 */
    {"C.D. Numancia B", {"C.D. Numancia de Soria", NULL}},      /* 3.9.68 and 2005 to 2013, Lower Division */
    {"C.Polideportivo Ejido B", {"Polideportivo Ejido", NULL}},
    {"UCAM Murcia B", {"UCAM Murcia CF", NULL}},
    {"Real Sociedad C", {"Real Sociedad C.F. B", NULL}},        /* March 2020 on, Lower Division */
    {"Sevilla FC C", {"Sevilla C.F. B", NULL}},
    {"UD Las Palmas C", {"U.D. Las Palmas B", "UD Las Palmas B"}},
};

static uint32_t find_whole(const Nation *n, const char *name) { return find_named(n, name, strlen(name), NULL); }

/* The club above a B or C side by name (its first team, or the C side's B side), or 0. */
static uint32_t named_above(const Nation *n, uint32_t club)
{
    size_t base;
    const char *s = name_of(club);
    for (size_t i = 0; i < sizeof NAMED / sizeof *NAMED; i++) {
        if (strlen(NAMED[i].club) != name_len(s) || strncasecmp(s, NAMED[i].club, name_len(s))) continue;
        for (int j = 0; j < 2 && NAMED[i].above[j]; j++) {
            uint32_t up = find_whole(n, NAMED[i].above[j]);
            if (up) return up;
        }
        return 0;
    }
    char k = reserve_kind(club, &base);
    if (!k) return 0;
    if (k == 'c') {
        uint32_t b = find_named(n, s, base, " B");
        if (b) return b;
    }
    return find_named(n, s, base, NULL);
}

/* The club below a club by name (a first team's B side, or its C side with none; a B side's C side), or 0. */
static uint32_t named_below(const Nation *n, uint32_t club)
{
    size_t base;
    const char *s = name_of(club);
    for (size_t i = 0; i < sizeof NAMED / sizeof *NAMED; i++)
        for (int j = 0; j < 2 && NAMED[i].above[j]; j++)
            if (strlen(NAMED[i].above[j]) == name_len(s) && !strncasecmp(s, NAMED[i].above[j], name_len(s))) {
                uint32_t down = find_whole(n, NAMED[i].club);
                if (down) return down;
            }
    char k = reserve_kind(club, &base);
    if (k == 'c') return 0;
    if (k == 'b') return find_named(n, s, base, " C");
    size_t len = name_len(s);
    uint32_t b = find_named(n, s, len, " B");
    return b ? b : find_named(n, s, len, " C");
}

static int listed(uint32_t ret, const uint32_t *list, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (list[i] == ret) return 1;
    return 0;
}

static int quiet;                      /* the calls of above_of log no link */

static void log_link(const char *role, uint32_t club, uint32_t link, uint32_t ret)
{
    if (log_on() && !quiet)
        fprintf(stderr, "recomp: esp second: %.51s: %s %.51s (call at 0x%x)\n", name_of(club), role, name_of(link),
                ret);
}

/* club_get_linked_b_team 0x540a50, cdecl (club, int *out, need_league). */
void o_00540a50(void);
RT_WRAP(00540a50, esp_second_link)
{
    uint32_t ret = rd32(g_mem, cpu.esp), club = rd32(g_mem, cpu.esp + 4), out = rd32(g_mem, cpu.esp + 8);
    int senior = listed(ret, ASK_SENIOR, sizeof ASK_SENIOR / sizeof *ASK_SENIOR);
    int reserve = !senior && listed(ret, ASK_RESERVE, sizeof ASK_RESERVE / sizeof *ASK_RESERVE);
    o_00540a50();
    if (rt_unwinding || (!senior && !reserve) || !readable(club)) return;
    Nation *n = nation_of(club);
    if (!rule_on(n)) return;
    uint32_t r = cpu.eax, link = 0;
    if (senior && !r) {
        link = named_above(n, club);
        if (link) log_link("plays below", club, link, ret);
    } else if (reserve) {
        size_t base;
        if (reserve_kind(club, &base) == 'b') link = named_below(n, club);   /* a B side's C side, even when paired */
        else if (!r) link = named_below(n, club);
        if (link) log_link("plays above", club, link, ret);
    }
    if (!link) return;
    if (readable(out)) wr32(g_mem, out, reserve ? 1u : 0u);
    cpu.eax = link;
}

/* The clubs above a reserve side (B or C by name, or a B side of the key pairs): up to two (a C side's B side and
 * first team). Asked through 0x540a50 (the original's pairs, then the link by name). */
static int above_of(uint32_t club, uint32_t up[2])
{
    size_t base;
    if (!reserve_kind(club, &base)) return 0;
    int k = 0;
    Cpu saved = cpu;
    quiet++;
    uint32_t a[3] = {club, 0, 0};
    uint32_t first = rt_callback(F_LINKED, 3, a);
    if (!rt_unwinding && readable(first)) {
        up[k++] = first;
        if (reserve_kind(first, &base)) {               /* a C side: its B side, then the B side's first team */
            uint32_t b[3] = {first, 0, 0};
            uint32_t top = rt_callback(F_LINKED, 3, b);
            if (!rt_unwinding && readable(top)) up[k++] = top;
        }
    }
    quiet--;
    if (rt_unwinding) return 0;
    cpu = saved;
    return k;
}

/* The tier of a division of a nation: 1 to 3 for the divisions the code plays (a club of the third tier has its
 * record as division and its group at +0x60, as 0x850b09..0x850b30 reads it; a group record of the database that
 * the new form does not play, such as Spanish Second Division B3 from 2021/22, is no part of it), 4 for any other,
 * 5 for none. */
static int tier_of(const Nation *n, uint32_t div)
{
    if (!readable(div)) return 5;
    int32_t id = (int32_t)rd32(g_mem, div);
    if (id == (int32_t)rd32(g_mem, n->top)) return 1;
    if (id == (int32_t)rd32(g_mem, n->second)) return 2;
    if (id == (int32_t)rd32(g_mem, n->third)) return 3;
    return 4;
}

/* The division of a club: as the club moves of a new game plan it while they run (season_esp_barred; 0 for a club
 * that they take out of its division), else its own. */
static uint32_t (*planned_div)(uint32_t club);

static uint32_t div_of(uint32_t club)
{
    return planned_div ? planned_div(club) : rd32(g_mem, club + CLUB_DIVISION);
}

/* The lowest tier (1 to 4) of the clubs above a reserve side that play in a division, or 5 for none: a reserve side
 * plays below each of them (a C side below its B side and its first team). */
static int above_tier(const Nation *n, uint32_t club)
{
    uint32_t up[2];
    int k = above_of(club, up), low = 0;
    for (int i = 0; i < k; i++) {
        int t = tier_of(n, div_of(up[i]));
        if (t <= 4 && t > low) low = t;
    }
    return low ? low : 5;
}

/* 1 when a reserve side plays in the division of a club above it, or above that club, within the top three tiers. */
static int too_high(const Nation *n, uint32_t club)
{
    int t = tier_of(n, rd32(g_mem, club + CLUB_DIVISION)), a = above_tier(n, club);
    return t <= 3 && a <= 4 && a >= t;
}

int season_esp_barred(uint32_t club, uint32_t div, uint32_t (*planned)(uint32_t club))
{
    if (!readable(club) || !readable(div)) return 0;
    Nation *n = nation_of(club);
    if (!rule_on(n)) return 0;
    int t = tier_of(n, div);
    if (t > 3) return 0;
    planned_div = planned;
    int a = above_tier(n, club);
    planned_div = NULL;
    return a <= 4 && a >= t;
}

void season_esp_second_moves(void) { index_reset(); }

/* The league object of a key (0 when none). */
static uint32_t league_of(uint32_t key)
{
    uint32_t leagues = rd32(g_mem, LEAGUES);
    int32_t id = (int32_t)rd32(g_mem, key);
    if (!readable(leagues) || id < 0 || id > 0xffff) return 0;
    uint32_t league = rd32(g_mem, leagues + 4u * (uint32_t)id);
    return readable(league) ? league : 0;
}

static uint32_t table_of(uint32_t league, int *n)
{
    *n = 0;
    if (!readable(league)) return 0;
    uint32_t table = rd32(g_mem, league + LEAGUE_TABLE);
    int rows = (int16_t)rd16(g_mem, league + LEAGUE_CLUBS);
    if (!readable(table) || rows <= 0 || rows > 64) return 0;
    *n = rows;
    return table;
}

/* The tables of the third tier: its object and its stage objects (the groups, as 0x8509a3..0x8509ca). */
static int third_tables(const Nation *n, uint32_t *out, int max)
{
    uint32_t obj = league_of(n->third);
    int k = 0, rows;
    if (!obj) return 0;
    if (table_of(obj, &rows)) out[k++] = obj;
    uint32_t stages = rd32(g_mem, obj + LEAGUE_STAGES);
    for (int i = 0; readable(stages) && i < 4 && k < max; i++) {
        uint32_t st = rd32(g_mem, stages + 4u * (uint32_t)i);
        if (readable(st) && table_of(st, &rows)) out[k++] = st;
    }
    return k;
}

static int up_status(int s) { return s == ST_TITLE || s == ST_UP || s == ST_PLAYOFF; }

/* The first row of a table with status from, the best placed first, gets status to (not a row in skip). Returns 1
 * when one did. */
static int first_row(uint32_t league, uint8_t from, uint8_t to, const char *skip)
{
    int rows;
    uint32_t table = table_of(league, &rows);
    for (int i = 0; i < rows; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        if (g_mem[row + ROW_STATUS] != from || (skip && skip[i])) continue;
        g_mem[row + ROW_STATUS] = to;
        if (log_on())
            fprintf(stderr, "recomp: esp second: %.51s takes status %d in place of a reserve side\n",
                    name_of(rd32(g_mem, row)), to == ST_NONE ? -1 : to);
        return 1;
    }
    return 0;
}

/* The status of a club's row in a table, or -1 when it has none there. */
static int row_status(uint32_t table, int rows, uint32_t club)
{
    for (int i = 0; i < rows; i++)
        if (rd32(g_mem, table + (uint32_t)i * ROW_SIZE) == club) return g_mem[table + (uint32_t)i * ROW_SIZE + ROW_STATUS];
    return -1;
}

/* 1 when a reserve side of a table of the second (tier 2) or third tier (tier 3) may not go up at this change: from
 * the second division none may (it would share the top division with its club or play above it); from the third tier
 * one whose club above plays the second division or lower, or comes down into it from the top division (status 3). */
static int may_not_go_up(const Nation *n, uint32_t club, int tier)
{
    if (!readable(club)) return 0;
    if (tier == 2) return above_tier(n, club) <= 4;
    uint32_t up[2];
    int k = above_of(club, up), top_rows;
    uint32_t top = table_of(league_of(n->top), &top_rows);
    for (int i = 0; i < k; i++) {
        int t = tier_of(n, rd32(g_mem, up[i] + CLUB_DIVISION));
        if ((t >= 2 && t <= 4) || (t == 1 && row_status(top, top_rows, up[i]) == ST_DOWN)) return 1;
    }
    return 0;
}

/* Before the original: a reserve side that may not go up and holds the title, promotion or the play-off gets 0xfc,
 * and its place goes to the best placed row of 0xff that is no such side, as the original hands on a place
 * (0x850e8e). This covers a save made after the table was complete (no 0xfc of the season), and a third-tier side
 * that holds the title (status 5): 0x850970 and 0x7d0320 hold such a side (0xfe, 0x850c2a, 0x7d05a7) but count a
 * place to hand on only for status 0 (0x850be9..0x850c26, 0x7d0574..0x7d05a3), so the place would stay empty. A play-off already played keeps
 * its winner. */
static void shift_up(const Nation *n, uint32_t league, int tier)
{
    int rows;
    uint32_t table = table_of(league, &rows);
    char barred[64] = {0};
    for (int i = 0; i < rows; i++) barred[i] = may_not_go_up(n, rd32(g_mem, table + (uint32_t)i * ROW_SIZE), tier);
    for (int i = 0; i < rows; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        uint8_t s = g_mem[row + ROW_STATUS];
        if (!barred[i] || s == ST_DOWN) continue;
        g_mem[row + ROW_STATUS] = ST_BARRED;
        if (up_status(s)) {
            if (log_on())
                fprintf(stderr, "recomp: esp second: %.51s held status %d of tier %d\n", name_of(rd32(g_mem, row)), s,
                        tier);
            first_row(league, ST_NONE, s, barred);
        }
    }
}

/* After the original: each reserve side that shares a division with a club above it, or plays above it, goes down
 * one division. tier 1, 2 or 3 (a group of the third tier). 0x850970 and 0x7d0320 end by turning their 0xfb rows
 * of the second division into 3 (0x850fd4..0x851005, 0x7d0931), and the chain 0x689c80 moves only rows of status 3
 * down (0x689d26, 0x689e52), so a reserve side gets 3 here. The club that stays up in its place (0xff in the top
 * division, 0xfe below, as 0x850ebc) and a club that takes a place of promotion that it held (0, as 0x850e8e) are
 * no reserve side with a club above it (it may be one that goes down with its club; and one whose club plays the
 * tier above or lower may not go up). */
/* 1 when a reserve side of the table (tier) shares the tier with a club above it, or plays above it, and that club
 * does not go up out of this table at this change (a club above that goes up leaves it below). */
static int stays_too_high(const Nation *n, uint32_t table, int rows, uint32_t club, int tier)
{
    if (tier_of(n, rd32(g_mem, club + CLUB_DIVISION)) != tier) return 0;
    uint32_t up[2];
    int k = above_of(club, up);
    for (int i = 0; i < k; i++) {
        int t = tier_of(n, rd32(g_mem, up[i] + CLUB_DIVISION));
        if (t > 4 || t < tier) continue;
        int s = row_status(table, rows, up[i]);
        if (s == ST_TITLE || s == ST_UP) continue;
        return 1;
    }
    return 0;
}

static void old_save_down(const Nation *n, uint32_t league, int tier)
{
    int rows;
    uint32_t table = table_of(league, &rows);
    char stay_skip[64] = {0}, up_skip[64] = {0}, high[64] = {0};
    for (int i = 0; i < rows; i++) {
        uint32_t club = rd32(g_mem, table + (uint32_t)i * ROW_SIZE);
        int a = readable(club) ? above_tier(n, club) : 5;
        stay_skip[i] = a <= 4;
        up_skip[i] = a <= 4 && a >= tier - 1;
        high[i] = readable(club) && stays_too_high(n, table, rows, club, tier);
    }
    int down = 0;
    for (int i = 0; i < rows; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE, club = rd32(g_mem, row);
        uint8_t s = g_mem[row + ROW_STATUS];
        if (!high[i] || s == ST_DOWN) continue;
        g_mem[row + ROW_STATUS] = ST_DOWN;
        down++;
        if (log_on())
            fprintf(stderr, "recomp: esp second: %.51s goes down from %.51s (a club above it plays there or "
                    "higher)\n", name_of(club), win_str(rd32(g_mem, club + CLUB_DIVISION) + 4));
        if (tier > 1 && up_status(s)) first_row(league, ST_NONE, s, up_skip);
    }
    for (int k = 0; k < down && tier < 3; k++)          /* the best placed club of status 3 stays up */
        if (!first_row(league, ST_DOWN, tier == 1 ? ST_NONE : ST_HELD, stay_skip) && log_on())
            fprintf(stderr, "recomp: esp second: no club of status 3 stays up for a reserve side in tier %d\n", tier);
}

/* The reserve sides of the top three tiers, and those too high. */
static void census(const Nation *n, const char *when)
{
    if (!log_on()) return;
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), per[4] = {0}, high = 0;
    for (int i = 0; readable(table) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        size_t base;
        if (nation_of(club) != n || !reserve_kind(club, &base)) continue;
        int t = tier_of(n, rd32(g_mem, club + CLUB_DIVISION));
        if (t > 3) continue;
        per[t]++;
        if (too_high(n, club)) {
            high++;
            fprintf(stderr, "recomp: esp second: %s: %.51s is too high in %.51s\n", n->name, name_of(club),
                    win_str(rd32(g_mem, club + CLUB_DIVISION) + 4));
        }
    }
    fprintf(stderr, "recomp: esp second: census %s %s: reserve sides tier 1 %d, tier 2 %d, tier 3 %d; too high %d\n",
            n->name, when, per[1], per[2], per[3], high);
}

/* RECOMP_TEST_ESP_SECOND_PUT: each named club changes places with the club of a row of its own table, at the first
 * season change of its nation, or at the k-th with "<club>:<row>@<k>". */
static void test_put(const Nation *n)
{
    const char *v = getenv("RECOMP_TEST_ESP_SECOND_PUT");
    if (!v) return;
    char buf[512];
    snprintf(buf, sizeof buf, "%s", v);
    char *save = NULL;
    for (char *item = strtok_r(buf, ",", &save); item; item = strtok_r(NULL, ",", &save)) {
        char *at = strrchr(item, '@');
        int when = 1;
        if (at) {
            *at = 0;
            when = atoi(at + 1);
        }
        char *colon = strrchr(item, ':');
        if (!colon || when != n->changes) continue;
        *colon = 0;
        int want = atoi(colon + 1) - 1;
        uint32_t club = find_named(n, item, strlen(item), NULL);
        if (!club) continue;
        uint32_t leagues[8] = {league_of(n->top), league_of(n->second)};
        int nl = 2 + third_tables(n, leagues + 2, 6);
        for (int l = 0; l < nl; l++) {
            int rows;
            uint32_t t = table_of(leagues[l], &rows);
            for (int i = 0; i < rows && want >= 0 && want < rows; i++) {
                if (rd32(g_mem, t + (uint32_t)i * ROW_SIZE) != club) continue;
                uint32_t a = t + (uint32_t)i * ROW_SIZE, b = t + (uint32_t)want * ROW_SIZE;
                wr32(g_mem, a, rd32(g_mem, b));
                wr32(g_mem, b, club);
                fprintf(stderr, "recomp: esp second: test: %.51s takes row %d (status %d) from %.51s\n", item, want + 1,
                        g_mem[b + ROW_STATUS], name_of(rd32(g_mem, a)));
                break;
            }
        }
    }
}

/* SpaFirst::resolve_b_team_clashes 0x850970 and PorPrm::apply_b_team_promotion_limits 0x7d0320: thiscall, no
 * arguments, ret 0. */
static void season_change(Nation *n, void (*original)(void))
{
    uint32_t self = cpu.ecx;
    n->changes++;
    index_reset();
    test_put(n);
    int on = rule_on(n);
    if (on) {
        census(n, "before");
        shift_up(n, league_of(n->second), 2);
        uint32_t groups[8];
        int ng = third_tables(n, groups, 8);
        for (int i = 0; i < ng; i++) shift_up(n, groups[i], 3);
    }
    original();
    if (rt_unwinding || !on) return;
    old_save_down(n, self, 1);
    old_save_down(n, league_of(n->second), 2);
    uint32_t groups[8];
    int ng = third_tables(n, groups, 8);
    for (int i = 0; i < ng; i++) old_save_down(n, groups[i], 3);
}

void o_00850970(void);
RT_WRAP(00850970, esp_second_spain) { season_change(&SPAIN, o_00850970); }
void o_007d0320(void);
RT_WRAP(007d0320, esp_second_portugal) { season_change(&PORTUGAL, o_007d0320); }

/* RECOMP_TEST_ESP_SECOND_REP=1 (tests only): before the pool of 0x7cff70 each Portuguese B or C side of the Third
 * Division and A Lower Division gets the highest reputation (club +0x80), so that the original would take it first. */
static void test_rep(void)
{
    if (!env_is("RECOMP_TEST_ESP_SECOND_REP", "1")) return;
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; readable(table) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
        size_t base;
        if (!readable(div) || nation_of(club) != &PORTUGAL || !reserve_kind(club, &base)) continue;
        int32_t id = (int32_t)rd32(g_mem, div);
        if (id == (int32_t)rd32(g_mem, PORTUGAL.below) || id == (int32_t)rd32(g_mem, KEY_LOWER))
            wr16(g_mem, club + CLUB_REPUTATION, 10000);
    }
}

/* The B and C sides that leave the pool of 0x7cff70 for the call, with their divisions. */
static uint32_t *hidden_club, *hidden_div;
static int nhidden;

void season_esp_second_pool(uint32_t nation_key, int phase)
{
    if (nation_key != PORTUGAL.nation) return;
    if (phase == 0) {
        nhidden = 0;
        index_reset();
        test_rep();
        if (!rule_on(&PORTUGAL)) return;
        uint32_t table = rd32(g_mem, CLUBS);
        int count = (int)rd32(g_mem, CLUB_COUNT);
        hidden_club = malloc(sizeof *hidden_club * (size_t)(count > 0 ? count : 1));
        hidden_div = malloc(sizeof *hidden_div * (size_t)(count > 0 ? count : 1));
        for (int i = 0; hidden_club && hidden_div && readable(table) && i < count; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
            if (!readable(div) || nation_of(club) != &PORTUGAL) continue;
            int32_t id = (int32_t)rd32(g_mem, div);
            if (id != (int32_t)rd32(g_mem, PORTUGAL.below) && id != (int32_t)rd32(g_mem, KEY_LOWER)) continue;
            int a = above_tier(&PORTUGAL, club);         /* a club above it in the Second Division B or lower */
            if (a < 3 || a > 4) continue;
            hidden_club[nhidden] = club;
            hidden_div[nhidden++] = div;
            wr32(g_mem, club + CLUB_DIVISION, 0);
            if (log_on())
                fprintf(stderr, "recomp: esp second: %.51s stays out of the Second Division B pool (a club above "
                        "it plays there)\n", name_of(club));
        }
        return;
    }
    for (int i = 0; i < nhidden; i++) wr32(g_mem, hidden_club[i] + CLUB_DIVISION, hidden_div[i]);
    nhidden = 0;
    free(hidden_club);
    free(hidden_div);
    hidden_club = hidden_div = NULL;
    if (!rt_unwinding) census(&PORTUGAL, "after the exchange");
}
