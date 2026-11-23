/* Germany: a reserve side plays the third tier only while its first team plays the Bundesliga or the 2. Bundesliga,
 * and never goes up into the 2. Bundesliga (open item 239, 0.7.2).
 *
 * The real rule by season. Sources, read on 14 November 2026 (notes: run-logs/0.7.2-nor-b/research-ger.md):
 * - 2008/09 to 2026/27 (the 3. Liga). DFB-Spielordnung (edition of 1 June 2024, https://dfb.de/fileadmin/_dfbdam/
 *   304263-Heft_04_Spielordnung_Schiedsrichterordnung_20240601.pdf) § 55 Nr. 3.1: the right to go up into the 2.
 *   Bundesliga lapses for a club that already plays the Lizenzligen with a team; Nr. 4: "an seiner Stelle der
 *   nächste aufstiegsberechtigte Amateurverein der 3. Liga". § 55b: reserve sides are treated as amateur sides for
 *   the way up into the 3. Liga, and the second teams of 3. Liga clubs may not play the fourth tier. DFB.de, "Wo
 *   dürfen zweite Mannschaften spielen?" (13 April 2026): a reserve side of a Lizenzverein may play the 3. Liga; when
 *   its first team goes down from the Bundesliga it stays; when its first team comes down from the 2. Bundesliga
 *   into the 3. Liga it leaves the 3. Liga (two teams of one club may not play one league) and goes to the Oberliga.
 *   de.wikipedia "3. Fußball-Liga": a reserve side in a promotion place in 2020 and 2023, the places moved one down.
 * - 2001/02 to 2007/08 (the two Regionalligen as the third tier). en.wikipedia "Regionalliga Süd (1994–2012)": the
 *   first two go up "unless they are reserve teams"; 2003/04, 1. FC Saarbrücken (3rd) went up as FC Bayern Munich
 *   II was not eligible; Karlsruher SC II went down to the Oberliga in 2000 as its first team came down into the
 *   Regionalliga. en.wikipedia "German reserve football teams": reserve sides play every level except the top two.
 *   The DFB text of those seasons was not found (I don't know its wording); the cases show the same rule.
 * So in every season the game plays (2001/02 to 2026/27) the rule in the game's divisions is the same: a reserve side
 * may play German Regional (the Regionalliga, from 2008/09 the 3. Liga in the same object, season_divisions.c 20a)
 * only while its first team plays the Bundesliga or the 2. Bundesliga, it never takes a place that goes up, and the
 * next club of the table takes that place. A youth side (last word U17, U19) or a third team (III) plays none of the
 * game's divisions (§ 55b: no third team of a Lizenzverein in the fourth tier, so none in the third).
 *
 * The original already does this for the key pairs of 2001 (club_get_linked_b_team 0x540a50: a club of Spain,
 * Germany or Portugal, nation test 0x540af6..0x540b12, then fixed keys from 0x540b1b; a first team gets its B side
 * and *out = 1 at 0x54187c, a B side gets its first team and *out stays 0):
 * - GerRegional::create_table 0x5dd7f0 gives every linked B side of North the mark 0xfc (0x5dd916..0x5dd942; the
 *   South stage likewise near 0x5ddf85), so 0x684cd0 gives it no title, promotion or play-off (season_second_teams.c).
 * - ger_first_relegate_b_teams_of_relegated_clubs 0x5dbf50 (called at 0x5db4c1, before the chain at 0x5db4cc):
 *   for each row of status 3 of the 2. Bundesliga whose club has a B side in German Regional (0x5dbfba..0x5dbfec),
 *   the B side's row of North or South (0x68ab70 at 0x5dc019) gets status 3 (0x5dc095), so the exchange sends it down.
 * - ger_first_exchange_regional_lower_division_clubs 0x5db980 (at 0x5db4e6): the pool of new clubs is the German
 *   clubs with no division or in A Lower Division [0x9cf764] (0x5db9bc..0x5db9d8); a linked B side joins only when
 *   its first team plays neither German Regional nor A Lower Division (0x5db9e2..0x5dba11). season_divisions.c's wrap
 *   of 0x5db980 (3. Liga seasons) keeps that test (pool_club).
 * The newer II, U23, U21 and Amateure sides of the season databases are no key pairs, so none of that touches them:
 * they enter the pool by reputation, can win German Regional or the 3. Liga and go up into the 2. Bundesliga.
 *
 * The fix (season games only, season_game_changes; the base 3.9.68 game keeps the original). A side of this rule is a
 * German club (club +0x53, [0x9cf314]) whose last word is II, U23, U-23, U21, U-21, Amateure or Reserves (a reserve
 * side), U17 or U19 (a youth side) or III (a third team), and that 0x540a50 does not link to a first team (so the key
 * pairs keep the original's code alone, and the two never mark or move the same club). Its first team: a row of
 * GER_PAIRS, else the German club named as the side less its last word (case aside), not itself such a side.
 * - Before each 0x684cd0 of German Regional (North or the 3. Liga), its South stage and the 2. Bundesliga
 *   (season_second_teams.c hook before_status): each reserve side with a first team, each youth side and third team
 *   gets 0xfc (st_mark), the original's own mark, so the next club of the table takes the place.
 * - The wrap of 0x5dbf50 (before the chain): when a save comes from the days after a table was complete, the places
 *   are handed on (st_shift_up; it also counts the key pairs' B sides as barred, so it never gives them a place).
 *   Old saves: a side in the 2. Bundesliga or the Bundesliga gets status 3 and the best placed club of status 3 stays
 *   up in exchange (the chain 0x689c80 moves rows in pairs), so the side goes down one division at each change.
 * - The wrap of GerRegional::create_table 0x5dd7f0 at the next season start (GerRegional::start_new_season 0x5dda90
 *   at 0x5ddb3c, after GerFirst::start_new_season 0x5db4a0 made the chain, the exchange 0x5db980 and the split
 *   0x5dbbd0): each side in German Regional that may not play there (a youth side or third team; a reserve side
 *   whose first team does not play the Bundesliga or the 2. Bundesliga, so also one whose club just came down) goes
 *   to A Lower Division (League::relegate_club_to_division 0x6831a0, as 0x5dba9e, and +0x60 cleared as 0x5dbaad),
 *   and the club of the original's pool with the highest reputation that may play there takes its place and its
 *   group +0x60 (League::move_club_to_division 0x6830b0, as 0x5dbbae). The original sorts by reputation and shuffles
 *   the top band (0x4afbd0); this takes no random number. The count of the division stays.
 * - The club moves of a new game (season_clubs.c, through season_second_barred) put no such side in the Bundesliga or
 *   the 2. Bundesliga, and none in German Regional that may not play there.
 * A reserve side with no first team found keeps the original's behaviour (none in the season databases, see the
 * check run-logs/0.7.2-nor-b/ger/check_names.py).
 *
 * Switches. RECOMP_GER_SECOND=off (tests): the original. RECOMP_GER_SECOND_LOG=1: the marks, the moves and a census
 * before and after each German season change. RECOMP_TEST_GER_SECOND_REP=1 (tests): each side of this rule in the
 * pool gets the highest reputation before the change, so the original's pool would take them first.
 * RECOMP_TEST_GER_SECOND_UP=1 (tests): before the change the best placed side of German Regional's table takes a
 * promotion status from the lowest placed club that has one, as an old save. RECOMP_TEST_GER_SECOND_OLD=<n> (tests):
 * the original until n German season changes are done, as a game of an older build. */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_second_teams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define KEY_GERMANY 0x9cf314u
#define KEY_FIRST 0x9cf54cu             /* the Bundesliga */
#define KEY_SECOND 0x9cf550u            /* the 2. Bundesliga */
#define KEY_NORTH 0x9cf55cu             /* group tags at club +0x60 (0x5dbff5, 0x5dc00a) */
#define KEY_SOUTH 0x9cf560u
#define KEY_REGIONAL 0x9cf564u          /* German Regional: the Regionalliga, from 2008/09 the 3. Liga */
#define KEY_LOWER 0x9cf764u             /* A Lower Division */
#define NATION_COUNT 0xae2350u          /* the last 2 x nations rows of the club table are national teams */
#define CLUB_GROUP 0x60u
#define OBJ_STAGES 0xcu
#define OBJ_YEAR 0x40u
#define F_LINKED 0x00540a50u            /* club_get_linked_b_team, cdecl (club, int *out, need) */
#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_MOVE_UP 0x006830b0u           /* League::move_club_to_division, thiscall (club, comp, 1) ret 0xc */
#define F_MOVE_DOWN 0x006831a0u         /* League::relegate_club_to_division, thiscall (club, comp, 1) ret 0xc */

#define readable st_readable
#define env_is st_env_is

/* First teams that are not the side's name less its last word (the season databases, check_names.py). A side may
 * have more than one row: the first row whose club exists is taken. */
static const char *const GER_PAIRS[][2] = {
    {"1.FC Dynamo Dresden II", "SG Dynamo Dresden"},
    {"Dynamo Dresden U19", "1.FC Dynamo Dresden"},
    {"Dynamo Dresden U19", "SG Dynamo Dresden"},
    {"1.FSV Mainz 05 U19", "FSV Mainz 05"},
    {"Bor. M'gladbach Amateure", "Borussia M'gladbach"},
    {"Borussia Dortmund Amateure", "Borussia Dortmund"},
    {"Borussia Dortmund Amateure", "BV Borussia Dortmund"},
    {"FC Bayern Munich U17", "FC Bayern M\xfcnchen"},
    {"FC Bayern Munich U19", "FC Bayern M\xfcnchen"},
    {"Hertha BSC Amateure", "Hertha BSC Berlin"},
    {"KSV Holstein Kiel II", "Holstein Kiel"},
    {"LR Ahlen II", "Rot Weiss Ahlen"},
    {"LR Ahlen II", "Rot-Weiss Ahlen"},
    {"RW Oberhausen Amateure", "Rot-Wei\xdf Oberhausen"},
    {"SSV Jahn 2000 Regensburg II", "SSV 2000 Jahn Regensburg"},
    {"SSV Jahn 2000 Regensburg II", "SSV Jahn Regensburg"},
    {"SpVgg Gr. F\xfcrth Amateure", "SpVgg Greuther F\xfcrth"},
};

enum { KIND_NONE, KIND_RESERVE, KIND_YOUTH };   /* KIND_YOUTH: a youth side or a third team */

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = env_is("RECOMP_GER_SECOND_LOG", "1");
    return log;
}

/* RECOMP_TEST_GER_SECOND_OLD=<n> (tests): the German season changes left to play as the original. Read once. */
static int old_left = -1;

static int rule_on(void)
{
    static int off = -1;
    if (off < 0) off = env_is("RECOMP_GER_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_GER_SECOND_OLD");
        old_left = v && atoi(v) > 0 ? atoi(v) : 0;
    }
    return !off && !old_left && season_game_changes();
}

static const char *name_of(uint32_t club) { return win_str(club + ST_CLUB_NAME); }

static int german(uint32_t club) { return readable(club) && st_nation_is(club, KEY_GERMANY); }

/* The kind of a name by its last word (case aside); a one-word name is none. */
static int name_kind(const char *name)
{
    size_t n = strnlen(name, 51);
    while (n && name[n - 1] == ' ') n--;
    size_t end = n;
    while (n && name[n - 1] != ' ') n--;
    if (!n || end - n > 9) return KIND_NONE;
    char last[10];
    memcpy(last, name + n, end - n);
    last[end - n] = 0;
    static const char *const reserve[] = {"ii", "u23", "u-23", "u21", "u-21", "amateure", "reserves"};
    for (size_t i = 0; i < sizeof reserve / sizeof *reserve; i++)
        if (!strcasecmp(last, reserve[i])) return KIND_RESERVE;
    if (!strcasecmp(last, "iii")) return KIND_YOUTH;
    const char *d = last + ((last[0] | 0x20) == 'u' ? 1 : 0);
    if (d == last) return KIND_NONE;
    if (*d == '-') d++;
    if (d[0] == '1' && d[1] >= '0' && d[1] <= '9' && !d[2]) return KIND_YOUTH;
    return KIND_NONE;
}

/* A cdecl call of the original from C: the registers stay as they were unless the program unwinds. */
static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* 0x540a50 (club, &out, 1): the linked club, and *out (1 for a first team, 0 for a B side). */
static uint32_t linked(uint32_t club, int *is_first)
{
    *is_first = 0;
    uint32_t a1[1] = {4};
    uint32_t out = call_cdecl(F_MALLOC, 1, a1);
    if (rt_unwinding || !readable(out)) return 0;
    wr32(g_mem, out, 0);
    uint32_t a[3] = {club, out, 1};
    uint32_t r = call_cdecl(F_LINKED, 3, a);
    if (rt_unwinding) return 0;
    *is_first = rd32(g_mem, out) != 0;
    uint32_t f[1] = {out};
    call_cdecl(F_FREE, 1, f);
    return readable(r) ? r : 0;
}

/* The cache of the sides: their kind and first team, for one club table. */
typedef struct {
    uint32_t club, first;
    int8_t kind;       /* KIND_NONE also for a key pair's B side (the original's) */
    int8_t pair_b;     /* 1 for a B side of the key pairs */
} Side;
#define CACHE 1024
static Side cache[CACHE];
static int ncache;
static uint32_t cache_table;
static int cache_count;

static uint32_t find_club(const char *want, uint32_t self)
{
    int count;
    uint32_t table = st_clubs(&count);
    for (int i = 0; table && i < count; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
        if (c == self || !german(c)) continue;
        const char *n = name_of(c);
        if (!strncasecmp(n, want, 52) && name_kind(n) == KIND_NONE) return c;
    }
    return 0;
}

static uint32_t first_team_by_name(uint32_t club)
{
    const char *name = name_of(club);
    for (size_t r = 0; r < sizeof GER_PAIRS / sizeof *GER_PAIRS; r++) {
        if (strncmp(name, GER_PAIRS[r][0], 52)) continue;
        uint32_t c = find_club(GER_PAIRS[r][1], club);
        if (c) return c;
    }
    size_t at = strnlen(name, 51);
    while (at && name[at - 1] == ' ') at--;
    while (at && name[at - 1] != ' ') at--;
    while (at && name[at - 1] == ' ') at--;
    if (!at) return 0;
    char want[64];
    snprintf(want, sizeof want, "%.*s", (int)at, name);
    return find_club(want, club);
}

/* The side of a club (kind KIND_NONE for a club of no concern). */
static Side side_of(uint32_t club)
{
    Side none = {club, 0, KIND_NONE, 0};
    if (!german(club)) return none;
    int kind = name_kind(name_of(club));
    if (kind == KIND_NONE) return none;
    int count;
    uint32_t table = st_clubs(&count);
    if (table != cache_table || count != cache_count) {
        ncache = 0;
        cache_table = table;
        cache_count = count;
    }
    for (int i = 0; i < ncache; i++)
        if (cache[i].club == club) return cache[i];
    Side s = {club, 0, (int8_t)kind, 0};
    int is_first;
    uint32_t link = linked(club, &is_first);
    if (rt_unwinding) return none;
    if (link && !is_first) {
        s.kind = KIND_NONE;
        s.pair_b = 1;
        s.first = link;
    } else {
        s.first = first_team_by_name(club);
    }
    if (ncache < CACHE) cache[ncache++] = s;
    return s;
}

static int id_is(uint32_t div, uint32_t key)
{
    return readable(div) && (int32_t)rd32(g_mem, div) == (int32_t)rd32(g_mem, key);
}

static int top_two(uint32_t div) { return id_is(div, KEY_FIRST) || id_is(div, KEY_SECOND); }

static int regional_div(uint32_t div)
{
    return id_is(div, KEY_REGIONAL) || id_is(div, KEY_NORTH) || id_is(div, KEY_SOUTH);
}

/* 1 for a side that takes no place that goes up: a youth side or third team, or a reserve side with a first team. */
static int no_way_up(uint32_t club)
{
    Side s = side_of(club);
    return s.kind == KIND_YOUTH || (s.kind == KIND_RESERVE && s.first);
}

/* The same, and the key pairs' B sides (which the original marks 0xfc at 0x5dd942). */
static int no_way_up_any(uint32_t club) { return no_way_up(club) || side_of(club).pair_b; }

/* The rule of a season year y (both eras give the same answer in the game's divisions, see the head of the file). */
static const char *era(int y) { return y <= 2007 ? "Regionalliga rule to 2007/08" : "3. Liga rule from 2008/09"; }

/* 1 when a side may not play German Regional under the rule of year y. */
static int barred_regional(uint32_t club, int y)
{
    (void)y;
    Side s = side_of(club);
    if (s.kind == KIND_YOUTH) return 1;
    if (s.kind != KIND_RESERVE || !s.first) return 0;
    return !top_two(season_clubs_planned_division(s.first));   /* the moves' plan at a new game, else +0x57 */
}

static uint32_t south_stage(uint32_t reg)
{
    if (!readable(reg)) return 0;
    uint32_t stages = rd32(g_mem, reg + OBJ_STAGES);
    if (!readable(stages)) return 0;
    uint32_t st = rd32(g_mem, stages);
    return readable(st) ? st : 0;
}

/* The census of the sides of this rule in the three divisions, for the log. */
static void log_census(const char *when, int season, int y)
{
    if (!log_on()) return;
    int count, n1 = 0, n2 = 0, n3 = 0, bad = 0;
    uint32_t table = st_clubs(&count);
    for (int i = 0; table && i < count; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE, d = rd32(g_mem, c + ST_CLUB_DIVISION);
        if (!german(c) || name_kind(name_of(c)) == KIND_NONE) continue;
        Side s = side_of(c);
        if (s.kind == KIND_NONE) continue;
        if (id_is(d, KEY_FIRST)) n1++;
        else if (id_is(d, KEY_SECOND)) n2++;
        else if (regional_div(d)) {
            n3++;
            bad += barred_regional(c, y);
        }
    }
    fprintf(stderr, "recomp: ger second: census %s (season %d, rule of %d): Bundesliga %d sides, 2. Bundesliga %d "
            "sides, German Regional %d sides, %d of them barred there\n", when, season, y, n1, n2, n3, bad);
}

/* Before 0x684cd0 (season_second_teams.c): the sides of German Regional, its South stage and the 2. Bundesliga
 * get 0xfc. */
static void ger_before_status(uint32_t league)
{
    if (!rule_on() || !readable(league)) return;
    uint32_t reg = st_league_of(KEY_REGIONAL), second = st_league_of(KEY_SECOND);
    if (league == reg || (reg && league == south_stage(reg)))
        st_mark(league, no_way_up, "ger second", "German Regional", log_on());
    else if (second && league == second)
        st_mark(league, no_way_up, "ger second", "the 2. Bundesliga", log_on());
}

/* RECOMP_TEST_GER_SECOND_UP=1 (tests): the best placed side of the table that has no promotion status takes status
 * 0 from the lowest placed club that has it, which takes the side's status (0xff for 0xfc). */
static void test_up(uint32_t league)
{
    static int on = -1;
    if (on < 0) on = env_is("RECOMP_TEST_GER_SECOND_UP", "1");
    int n;
    uint32_t table = on && league ? st_table_of(league, &n) : 0;
    if (!table) return;
    int side = -1, other = -1;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        uint8_t s = g_mem[row + ST_ROW_STATUS];
        int barred = no_way_up(rd32(g_mem, row));
        if (barred && (s == ST_UP || s == ST_TITLE)) return;
        if (barred && side < 0 && s != ST_DOWN) side = i;
        if (!barred && s == ST_UP && !side_of(rd32(g_mem, row)).pair_b) other = i;
    }
    if (side < 0 || other < 0) return;
    uint32_t a = table + (uint32_t)side * ST_ROW_SIZE, b = table + (uint32_t)other * ST_ROW_SIZE;
    uint8_t was = g_mem[a + ST_ROW_STATUS];
    g_mem[b + ST_ROW_STATUS] = was == ST_BARRED ? ST_NONE : was;
    g_mem[a + ST_ROW_STATUS] = ST_UP;
    fprintf(stderr, "recomp: ger second: test: %.51s takes status 0 from %.51s\n", name_of(rd32(g_mem, a)),
            name_of(rd32(g_mem, b)));
}

/* Old saves: each side of the league gets status 3, and the best placed club of status 3 (not a side) stays up. */
static void season_down(uint32_t league, const char *what)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    if (!table) return;
    int next = 0;
    for (int i = n - 1; i >= 0 && !rt_unwinding; i--) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] == ST_DOWN || !no_way_up(club)) continue;   /* no first team: original */
        int partner = -1;
        for (; next < n; next++) {
            uint32_t r = table + (uint32_t)next * ST_ROW_SIZE;
            if (g_mem[r + ST_ROW_STATUS] == ST_DOWN && side_of(rd32(g_mem, r)).kind == KIND_NONE) {
                partner = next++;
                break;
            }
        }
        if (partner < 0) {
            if (log_on()) fprintf(stderr, "recomp: ger second: %.51s stays in %s one more season (no place goes "
                                  "down for it)\n", name_of(club), what);
            continue;
        }
        g_mem[row + ST_ROW_STATUS] = ST_DOWN;
        uint32_t up = table + (uint32_t)partner * ST_ROW_SIZE;
        g_mem[up + ST_ROW_STATUS] = ST_NONE;
        if (log_on()) fprintf(stderr, "recomp: ger second: %.51s goes down from %s, %.51s stays up\n", name_of(club),
                              what, name_of(rd32(g_mem, up)));
    }
}

/* Set at the German season change (0x5dbf50) for the next GerRegional::create_table. */
static int pending, pending_season, pending_year;

/* ger_first_relegate_b_teams_of_relegated_clubs 0x5dbf50 (thiscall: GerFirst; ret), at 0x5db4c1. */
void o_005dbf50(void);
RT_WRAP(005dbf50, ger_second_change)
{
    uint32_t self = cpu.ecx;
    int count;
    if (!season_game_changes() || !st_clubs(&count) || !readable(self)) {
        o_005dbf50();
        return;
    }
    int season = (int16_t)rd16(g_mem, self + OBJ_YEAR) + 1, y = season_rule_year(season);
    uint32_t reg = st_league_of(KEY_REGIONAL), second = st_league_of(KEY_SECOND), first = st_league_of(KEY_FIRST);
    if (env_is("RECOMP_TEST_GER_SECOND_REP", "1")) {
        uint32_t table = st_clubs(&count);
        for (int i = 0; i < count; i++) {
            uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE, d = rd32(g_mem, c + ST_CLUB_DIVISION);
            if ((!d || id_is(d, KEY_LOWER)) && german(c) && side_of(c).kind != KIND_NONE)
                wr16(g_mem, c + ST_CLUB_REPUTATION, 10000);
        }
    }
    test_up(reg);
    log_census("before", season, y);
    if (rule_on()) {
        st_shift_up(reg, no_way_up_any, "ger second", "at the season change", log_on());
        if (south_stage(reg)) st_shift_up(south_stage(reg), no_way_up_any, "ger second", "at the season change",
                                          log_on());
        st_shift_up(second, no_way_up_any, "ger second", "at the season change", log_on());
        season_down(second, "the 2. Bundesliga");
        season_down(first, "the Bundesliga");
    }
    pending = 1;                         /* the log runs with the switch off too */
    pending_season = season;
    pending_year = y;
    o_005dbf50();
}

/* The best club of the original's pool (0x5db9bc..0x5dba13) that may play German Regional, by reputation. */
static uint32_t best_of_pool(int y)
{
    int count;
    uint32_t table = st_clubs(&count);
    count -= 2 * (int)rd32(g_mem, NATION_COUNT);
    uint32_t best = 0;
    int best_rep = -1;
    for (int i = 0; table && i < count && !rt_unwinding; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE, d = rd32(g_mem, c + ST_CLUB_DIVISION);
        if (!german(c) || (d && !id_is(d, KEY_LOWER))) continue;
        int rep = (int16_t)rd16(g_mem, c + ST_CLUB_REPUTATION);
        if (rep <= best_rep) continue;
        Side s = side_of(c);
        if (s.pair_b) {                 /* the original's test of a B side */
            uint32_t sd = rd32(g_mem, s.first + ST_CLUB_DIVISION);
            if (!sd || regional_div(sd) || id_is(sd, KEY_LOWER)) continue;
        } else if (barred_regional(c, y)) {
            continue;
        }
        best = c;
        best_rep = rep;
    }
    return best;
}

/* GerRegional::create_table 0x5dd7f0 (thiscall). At the season start after a German season change: each side that
 * may not play German Regional goes to A Lower Division and the best club of the pool takes its place. */
void o_005dd7f0(void);
RT_WRAP(005dd7f0, ger_second_table)
{
    uint32_t self = cpu.ecx;
    if (pending && readable(self) && self == st_league_of(KEY_REGIONAL)) {
        pending = 0;
        int y = pending_year, count, on = rule_on();
        uint32_t reg_rec = st_comp_of(KEY_REGIONAL), lower_rec = st_comp_of(KEY_LOWER);
        uint32_t table = st_clubs(&count);
        for (int i = 0; on && table && i < count && !rt_unwinding; i++) {
            uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
            if (rd32(g_mem, c + ST_CLUB_DIVISION) != reg_rec || !barred_regional(c, y)) continue;
            uint32_t group = rd32(g_mem, c + CLUB_GROUP);
            Side s = side_of(c);
            /* No club to take its place: the side stays, so German Regional keeps its size. */
            uint32_t in = best_of_pool(y);
            if (!in || rt_unwinding) {
                if (log_on()) fprintf(stderr, "recomp: ger second: %.51s stays in German Regional (no club of A Lower "
                                      "Division to take its place)\n", name_of(c));
                continue;
            }
            uint32_t a[3] = {c, lower_rec, 1};
            st_call_this(F_MOVE_DOWN, self, 3, a);
            wr32(g_mem, c + CLUB_GROUP, 0);
            if (in) {
                uint32_t m[3] = {in, reg_rec, 1};
                st_call_this(F_MOVE_UP, self, 3, m);
                wr32(g_mem, in + CLUB_GROUP, group);
            }
            if (log_on()) {
                uint32_t d = s.first ? rd32(g_mem, s.first + ST_CLUB_DIVISION) : 0;
                fprintf(stderr, "recomp: ger second: %.51s goes down to A Lower Division (season %d, %s: %s %.51s "
                        "plays %.51s); %.51s takes its place\n", name_of(c), pending_season, era(y),
                        s.kind == KIND_YOUTH ? "a youth side or third team," : "its first team",
                        s.first ? name_of(s.first) : "", readable(d) ? win_str(d + 4) : "none",
                        in ? name_of(in) : "no club");
            }
        }
        if (!rt_unwinding && log_on()) {
            for (int i = 0; table && i < count; i++) {
                uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
                Side s = rd32(g_mem, c + ST_CLUB_DIVISION) == reg_rec ? side_of(c) : (Side){0, 0, 0, 0};
                if (s.kind != KIND_NONE)
                    fprintf(stderr, "recomp: ger second: %.51s plays German Regional, %s (season %d, %s: %.51s "
                            "plays %.51s)\n", name_of(c), barred_regional(c, y) ? "barred" : "allowed",
                            pending_season, era(y),
                            s.first ? name_of(s.first) : "no first team found",
                            s.first && readable(rd32(g_mem, s.first + ST_CLUB_DIVISION))
                                ? win_str(rd32(g_mem, s.first + ST_CLUB_DIVISION) + 4) : "none");
            }
            log_census("after", pending_season, y);
        }
        if (old_left > 0) old_left--;
        cpu.ecx = self;
    }
    o_005dd7f0();
}

/* The club moves of a new game (season_clubs.c): no side in the top two divisions, none in German Regional that may
 * not play there (its first team's division at the time of the call). */
static int ger_barred_at_start(uint32_t club, uint32_t div)
{
    if (!rule_on() || !german(club) || name_kind(name_of(club)) == KIND_NONE) return 0;
    if (top_two(div)) return no_way_up(club);
    if (regional_div(div)) return barred_regional(club, season_rule_year(st_season_year(KEY_FIRST)));
    return 0;
}

static const SecondTeamHooks ger_hooks = {ger_before_status, NULL, NULL, ger_barred_at_start};
SEASON_SECOND_HOOKS(ger_hooks)
