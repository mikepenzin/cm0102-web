/* Belgium: U23 sides stay below their first team and out of the top tier (open item 115, 0.7.0), and from 2015/16
 * the Second Division's 16th goes down directly (open item 124).
 *
 * The real rule (Wikipedia season pages of the Challenger Pro League 2020/21 to 2025/26 and the Belgian Competition
 * Authority, press release 19/2026, all read 3 November 2026; no federation text was found):
 * - from 2020/21 a U23 side never goes up to the top tier, and its promotion or play-off place goes to the next
 *   eligible club (2024/25: Club NXT 6th, Lokeren-Temse 7th took the play-off place);
 * - a U23 side always plays at least one level below its first team, in every tier;
 * - when its first team goes down from the Pro League, the U23 side goes down "whatever its place", and from
 *   2024/25 it "would automatically take up one of the relegation spots";
 * - the relegation shield: 2020/21 Club NXT was "not eligible for promotion or relegation", and no club went down
 *   at all, as National Division 1 was cancelled (the game keeps its relegation and uses the way of 2024/25 below
 *   for a U23 side only; 2021/22 had no U23 side in the division); 2024/25 no U23 side went down by the
 *   table and no club went down in its place (Jong Genk 15th and Seraing 14th stayed up, only Deinze went down,
 *   the third tier sent its two clubs up, so the division grew from 16 to 17); 2025/26 the Belgian Competition
 *   Authority abolished the shield in May 2026 (Club NXT stayed up only as RWDM lost its licence), so no shield
 *   from 2025/26 (Max, 4 November 2026: "copy real life");
 * - 2001/02 to 2019/20: no source has a U23 side in a national division, so those seasons keep the game.
 * Open item 124 (lead decision, 3 November 2026, "as in real life"): from 2015/16 no second-tier club played the
 * third tier's promotion play-off (sources: run-logs/0.6.2-s113/research-belgian-third-playoff.tsv).
 *
 * The original. BelFirst::start_new_season 0x41ea90 calls the chain BelFirst::process_promotion_relegation_chain
 * 0x41ee80 (vtable +0xb0 of BelFirst 0x967940). It records the last places of the four tables (vtable +0xa4,
 * 0x68aa30, which writes club +0x5b and +0x5f only) and calls league_promote_and_relegate 0x689c80 three times, in
 * pairs: First and Second (0x41ef03), Second and Third group A (0x41ef2b), Second and Third group B (0x41ef58). It
 * moves the rows of status 3 of the upper table down and the rows of status 0 or 5 of the lower table up. Then
 * bel_first_move_third_division_clubs 0x41ec90 sends the rows of status 3 of the two Third groups to Fourth A and B
 * (0x41ed1a..0x41ed76) and brings up the play-off winners and the four clubs that BelThird::create_relegation_
 * playoff_stage 0x423ba0 stored at +0xee..+0xfa. League::update_positions_and_status 0x684cd0 gives no title,
 * promotion or play-off to a row of status 0xfc (the original's mark for a German B side, 0x5dd942), so the next
 * row takes the place; in the zone of relegation such a row gets 3 (see season_nor_second.c). BelSecond::
 * update_positions_and_status 0x420680 (vtable +0x94 of 0x9679f4) calls 0x684cd0 and then gives status 1 to the
 * winner of each period only when its row has 0xff (0x42070e). BelSecond::create_playoff_group 0x420230 tops the
 * play-off group up to four with the rows of 0xff from the top (0x42028e..0x4202a6). BelThird::update_positions_
 * and_status 0x422f70 (vtable +0x94 of 0x967b48) does the same for groups A (this) and B (stage 0, this +0xc). The
 * original links no Belgian U23 side to its club (club_get_linked_b_team 0x540a50 holds Spain, Germany and
 * Portugal only).
 *
 * The fix (season games only, season_game_changes, from the season 2020/21):
 * - The link: a Belgian club whose name is a reserve name (season_clubs_reserve_name: a last word B, II, 2,
 *   Reserve(s), a word U21 or U23, Jong at the start) or ends in " 16 FC" (Standard de Liège 16 FC), less the
 *   first teams "Jong Westel". Its first team is the Belgian club of the name without that word, or the name of a
 *   small exception table (RSC Charleroi B, Zulte-Waregem B, Beerschot V.A U23, Standard de Liège 16 FC).
 * - In the season: before 0x420680 every U23 side of the Second Division gets 0xfc; before 0x422f70 each U23 side
 *   of the Third Division whose first team plays in the Second Division or lower next season (its level now, moved
 *   by its status once its table sets one) gets 0xfc, and gets 0xff back when that no longer holds. So the next
 *   clubs take the title, the promotion places, the periods and the play-offs. A row that already holds such a
 *   place (a save of an older build made after the table was complete) hands it to the best placed row of 0xff
 *   (hand_on); the play-off builders 0x420230 and 0x423880 do the same first.
 * - The Fourth Division list: for the call of 0x423ba0 a U23 side of the four Fourth Divisions whose first team
 *   plays in the Third Division or lower next season reads as no division, so it is no part of the four that go up
 *   or of the relegation play-off (the original's own way for Spain, 0x8504c9..0x8504ec; see season_nor_second.c).
 * - At the season change, before 0x41ee80, in this order: (1) a U23 side in the First Division (an old save) gets
 *   3 and the best placed club of 3 there stays up (0xff), as the moves go in pairs; (2) a U23 side of the Third
 *   Division that goes up (0 or 5) to the division of its first team next season hands the place to the best
 *   placed row of 0xff of its group; (3) the shield (2020/21 and 2024/25): a U23 side of the Second Division with 3
 *   by the table gets 0xff and no club goes down in its place; the division holds 18 in the game (0x41fd82) and
 *   0x689c80 moves in pairs, so one promotion of the Third Division drops (the promotion play-off's winner, else
 *   the group winner with the fewest points, gets 0xfe); a U23 side whose first team goes down is no part of the
 *   shield, it goes down by step 4; (4) a U23 side of the Second Division whose first team plays there or lower next
 *   season (its first team relegated from the First Division, or an old save) gets 3 and the best placed club of 3
 *   stays up; (5) a U23 side of the Third Division whose first team plays in the Third Division or lower next season
 *   gets 3 and the best placed club of 3 of its group stays up. A U23 side with more such sides than clubs of 3
 *   stays one more season (logged). A U23 side sent down from the First Division whose first team plays in the
 *   Second Division shares its division for one season, until step 4 of the next change.
 * - Open item 124, from 2015/16: for the call of BelThird::create_promotion_playoff_stage 0x423880 the row 15 that
 *   it reads (+0x375, 0x423a80) holds the next club of the Third Division (the best placed row of 0xff that the
 *   original does not put in, the more points of the two groups, group A on a tie), and the Second Division's 16th
 *   gets 3. Its place is that of the relegation play-off: status 2, or 0xfc for a U23 side (another status is a
 *   fault: logged, and the original plays). The results go through BelThird::update_staff_records_for_status
 *   0x4230d0, which gives a club of this division 0 when it wins (0x4231b2) and 0xfe when it loses (0x423204).
 * Accepted effects: a club that stays up or takes a place keeps the news of its earlier status, and the 16th gets
 * no news of its direct relegation (0x684cd0 and 0x4230d0 send them through vtable +0x40 when they set a status).
 * The Third Division club whose promotion the shield drops keeps the news of its promotion; a group winner (5)
 * drops only when no play-off winner (0) is left, and stays champion in the table.
 * Not built (no rule text or no way in the game): the quota of four U23 sides (2025/26) and the 2026/27
 * replacement rule, the one-division third tier of 2016/17 on (the game keeps two groups), the 2015/16 reform year
 * (nine clubs down) and the real sizes of the Second Division (16, 17 in 2025/26, 15 in 2026/27: the game holds 18).
 *
 * Test switches: RECOMP_BEL_SECOND=off plays as the original (the log still works). RECOMP_BEL_SECOND_LOG=1 logs
 * the marks, the moves, the play-off and a census of the U23 sides at each season change. Tests only:
 * RECOMP_TEST_BEL_POINTS=<name>=<+|->;... moves the named clubs to the top (+) or the bottom (-) of their table by
 * points (+0x19) before each update of the Second and Third Divisions; RECOMP_TEST_BEL_DOWN=<name>;... gives the
 * named clubs of the First Division 3 before the chain (and the best placed club of 3 0xff), as a relegated first
 * team; RECOMP_TEST_BEL_SECOND_OLD=<n> plays as the original until n Belgian season changes are done. */
#include "win.h"
#include "season.h"
#include "season_bel_playoffs.h"
#include "season_bel_second.h"
#include "season_change.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_sizes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_BELGIUM 0x9cf244u
#define KEY_FIRST 0x9cf59cu
#define KEY_SECOND 0x9cf5a0u
#define KEY_THIRD 0x9cf7c4u             /* BelThird, group A (+0x57 of every Third Division club in a season) */
#define KEY_THIRD_A 0x9cf5a4u           /* the data divisions Third A and B (club +0x60, 0x423102) */
#define KEY_THIRD_B 0x9cf5a8u
#define KEY_FOURTH 0x9cf7d4u            /* Fourth A to D, 0x9cf7d4..0x9cf7e0 (0x423c37..0x423c51) */
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define COMPS 0xae23d0u
#define CLUB_SIZE 0x245u
#define COMP_SIZE 0x6bu
#define CLUB_NAME 0x04u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define LEAGUES 0xadadfcu
#define LEAGUE_STAGES 0x0cu             /* BelThird: stage 0 is group B (0x422f92) */
#define LEAGUE_CLUBS 0x3eu
#define LEAGUE_TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROW_POINTS 0x19u                /* s16 */
#define ROW_STATUS 0x37u
#define ROW_POS 0x39u                   /* s8: the place, from 1 (0x685225) */
#define DOWN_PLAYOFF 0xc0u              /* s8: relegation play-off places of a league (0x6852b3) */
#define DOWN_PLACES 0xc1u               /* s8: relegation places (League::has_relegation 0x6847b0, 0x6852d4) */
/* The Second Division row that 0x423880 reads (+0x375, 0x423a80): row 15, the count less 3 with the size engine
 * (0.7.2 slice 10, season_sizes_bel_third_row). */
#define GAME_DATE 0xae2c90u             /* s16 day of the year (0 = 1 January), s16 year */
#define ST_TITLE 5
#define ST_UP 0
#define ST_PLAYOFF 1
#define ST_DOWN_PLAYOFF 2
#define ST_DOWN 3
#define ST_BARRED 0xfc
#define ST_NONE 0xff
#define ST_STAYS 0xfe              /* stays: the loser of the promotion play-off (0x4230d0, 0x423204) */
#define BAR_FROM 2020                   /* the first season with a U23 side in a national division */
#define DIRECT_FROM 2015                /* open item 124: no second-tier club in the third tier's play-off */
#define LEVELS 5                        /* 1 First, 2 Second, 3 Third, 4 Fourth, 5 any other division or none */

static int readable(uint32_t p) { return rt_readable(p); }

static int env_is(const char *name, const char *value)
{
    const char *v = getenv(name);
    return v && !strcmp(v, value);
}

static int old_left = -1;

static int switched_off(void)
{
    static int off = -1;
    if (off < 0) off = env_is("RECOMP_BEL_SECOND", "off");
    if (old_left < 0) {
        const char *v = getenv("RECOMP_TEST_BEL_SECOND_OLD");
        old_left = v ? atoi(v) : 0;
        if (old_left < 0) old_left = 0;
    }
    return off || old_left > 0;
}

static int log_on(void)
{
    static int log = -1;
    if (log < 0) log = env_is("RECOMP_BEL_SECOND_LOG", "1");
    return log;
}

static int rule_on(void) { return !switched_off() && season_game_changes(); }

static const char *name_of(uint32_t club) { return win_str(club + CLUB_NAME); }

/* The season that the game plays now: it starts in July (day 181 or later), so January to June belong to the
 * season of the year before. */
static int season_now(void)
{
    int day = (int16_t)rd16(g_mem, GAME_DATE), year = (int16_t)rd16(g_mem, GAME_DATE + 2);
    return day >= 181 ? year : year - 1;
}

static uint32_t league_of(uint32_t key)
{
    uint32_t leagues = rd32(g_mem, LEAGUES);
    int32_t id = (int32_t)rd32(g_mem, key);
    if (!readable(leagues) || id < 0 || id > 0xffff) return 0;
    uint32_t league = rd32(g_mem, leagues + 4u * (uint32_t)id);
    return readable(league) ? league : 0;
}

static uint32_t comp_of(uint32_t key) { return rd32(g_mem, COMPS) + rd32(g_mem, key) * COMP_SIZE; }

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

static uint32_t row_at(uint32_t table, int i) { return table + (uint32_t)i * ROW_SIZE; }

/* The group B stage of BelThird (0 when none). */
static uint32_t group_b(uint32_t third)
{
    if (!readable(third)) return 0;
    uint32_t stages = rd32(g_mem, third + LEAGUE_STAGES);
    uint32_t b = readable(stages) ? rd32(g_mem, stages) : 0;
    return readable(b) ? b : 0;
}

/* The league tables of a level: 1 First, 2 Second, 3 the two Third groups. Returns the count. */
static int leagues_of(int level, uint32_t out[2])
{
    int k = 0;
    if (level == 1) out[k] = league_of(KEY_FIRST), k += out[k] != 0;
    if (level == 2) out[k] = league_of(KEY_SECOND), k += out[k] != 0;
    if (level == 3) {
        uint32_t third = league_of(KEY_THIRD);
        if (third) out[k++] = third;
        uint32_t b = group_b(third);
        if (b) out[k++] = b;
    }
    return k;
}

static int belgian(uint32_t club)
{
    uint32_t nation = rd32(g_mem, club + CLUB_NATION);
    return readable(nation) && (int32_t)rd32(g_mem, nation) == (int32_t)rd32(g_mem, KEY_BELGIUM);
}

/* First teams whose name reads as a reserve name. */
static const char *const FIRST_TEAMS[] = {"Jong Westel"};

/* U23 sides whose first team has another name (the databases of April 2023 to May 2026). */
static const struct { const char *u23, *first; } PARENTS[] = {
    {"RSC Charleroi B", "Royal Charleroi SC"},
    {"RSC Charleroi B", "R Charleroi SC"},
    {"Zulte-Waregem B", "Zulte Waregem"},
    {"Zulte-Waregem B", "SV Zulte Waregem"},
    {"Beerschot V.A U23", "Beerschot V.A."},
    {"Beerschot V.A U23", "Beerschot VA"},
    {"Standard de Li\xe8ge 16 FC", "Standard Li\xe8ge"},
};

static int ends_with(const char *s, const char *tail)
{
    size_t n = strlen(s), t = strlen(tail);
    return n > t && !strcmp(s + n - t, tail);
}

static int u23_name(const char *name)
{
    for (size_t i = 0; i < sizeof FIRST_TEAMS / sizeof *FIRST_TEAMS; i++)
        if (!strcmp(name, FIRST_TEAMS[i])) return 0;
    return season_clubs_reserve_name(name) || ends_with(name, " 16 FC");
}

static int u23(uint32_t club) { return readable(club) && belgian(club) && u23_name(name_of(club)); }

/* The Belgian club (not a U23 side) of that exact name, or 0. */
static uint32_t club_named(const char *want)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; readable(table) && want[0] && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (!strncmp(name_of(club), want, 52) && belgian(club) && !u23_name(name_of(club))) return club;
    }
    return 0;
}

/* The first team of a U23 side, or 0 when the data has none. */
static uint32_t parent_of(uint32_t club)
{
    char base[64];
    snprintf(base, sizeof base, "%.51s", name_of(club));
    static const char *const TAILS[] = {" II", " B", " 2", " U23", " U21", " U19", " Reserve", " Reserves"};
    for (size_t i = 0; i < sizeof TAILS / sizeof *TAILS; i++)
        if (ends_with(base, TAILS[i])) {
            base[strlen(base) - strlen(TAILS[i])] = 0;
            break;
        }
    uint32_t first = club_named(!strncmp(base, "Jong ", 5) ? base + 5 : base);
    for (size_t i = 0; !first && i < sizeof PARENTS / sizeof *PARENTS; i++)
        if (!strncmp(name_of(club), PARENTS[i].u23, 52)) first = club_named(PARENTS[i].first);
    return first;
}

/* The level of a division record: 1 First, 2 Second, 3 Third, 4 Fourth, 5 any other or none. */
static int level_of_div(uint32_t div)
{
    if (!readable(div)) return LEVELS;
    if (div == comp_of(KEY_FIRST)) return 1;
    if (div == comp_of(KEY_SECOND)) return 2;
    if (div == comp_of(KEY_THIRD) || div == comp_of(KEY_THIRD_A) || div == comp_of(KEY_THIRD_B)) return 3;
    for (uint32_t k = 0; k < 4; k++)
        if (div == comp_of(KEY_FOURTH + 4u * k)) return 4;
    return LEVELS;
}

static int level_of(uint32_t club) { return level_of_div(rd32(g_mem, club + CLUB_DIVISION)); }

/* The table row of a club in the leagues of its level (0 when none). */
static uint32_t row_of(uint32_t club, int level)
{
    uint32_t leagues[2];
    int nl = leagues_of(level, leagues);
    for (int l = 0; l < nl; l++) {
        int n;
        uint32_t table = table_of(leagues[l], &n);
        for (int i = 0; i < n; i++)
            if (rd32(g_mem, row_at(table, i)) == club) return row_at(table, i);
    }
    return 0;
}

/* The level at which a club plays next season, from its status now. */
static int next_level(uint32_t club)
{
    int level = level_of(club);
    uint32_t row = level <= 3 ? row_of(club, level) : 0;
    int s = row ? g_mem[row + ROW_STATUS] : ST_NONE;
    if (s == ST_DOWN && level < 4) return level + 1;
    if ((s == ST_UP || s == ST_TITLE) && level > 1) return level - 1;
    return level;
}

/* 1 when a row of this level may not go up: in the Second Division every U23 side (never into the top tier), in
 * the Third Division a U23 side whose first team plays in the Second Division or lower next season (next_level:
 * its level now, moved by a status of promotion or relegation once its table sets one). */
static int barred_up(uint32_t club, int level)
{
    if (!u23(club)) return 0;
    if (level <= 2) return 1;
    uint32_t first = parent_of(club);
    return first && next_level(first) >= level - 1;
}

static int up_status(int s) { return s == ST_TITLE || s == ST_UP || s == ST_PLAYOFF; }

/* The best placed row of 0xff that may go up (-1 when none). */
static int next_free(uint32_t table, int n, int level)
{
    for (int i = 0; i < n; i++) {
        uint32_t row = row_at(table, i);
        if (g_mem[row + ROW_STATUS] == ST_NONE && !barred_up(rd32(g_mem, row), level)) return i;
    }
    return -1;
}

/* A barred row that holds the title, promotion or the play-off (a save of an older build): it gets 0xfc, and the
 * best placed row of 0xff takes the title or promotion. A play-off place is filled by the builder's own top-up. */
static void hand_on(uint32_t league, int level, const char *when)
{
    int n;
    uint32_t table = table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = row_at(table, i), club = rd32(g_mem, row);
        int s = g_mem[row + ROW_STATUS];
        if (!up_status(s) || !barred_up(club, level)) continue;
        g_mem[row + ROW_STATUS] = ST_BARRED;
        int j = s == ST_PLAYOFF ? -1 : next_free(table, n, level);
        if (j >= 0) g_mem[row_at(table, j) + ROW_STATUS] = (uint8_t)s;
        if (log_on())
            fprintf(stderr, "recomp: bel second: %.51s may not take status %d %s%s%.51s\n", name_of(club), s, when,
                    j >= 0 ? ", it goes to " : "", j >= 0 ? name_of(rd32(g_mem, row_at(table, j))) : "");
    }
}

/* Each barred row gets 0xfc (one with 3 keeps it), so 0x684cd0 gives its places to the next rows. A U23 side that
 * is no longer barred (its first team now goes up) gets 0xff back. No other Belgian row gets 0xfc. */
static void mark(uint32_t league, int level)
{
    hand_on(league, level, "in the season");
    int n;
    uint32_t table = table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = row_at(table, i), club = rd32(g_mem, row);
        int s = g_mem[row + ROW_STATUS];
        if (s == ST_BARRED && u23(club) && !barred_up(club, level)) {
            g_mem[row + ROW_STATUS] = ST_NONE;
            if (log_on()) fprintf(stderr, "recomp: bel second: %.51s may go up from level %d again\n", name_of(club),
                                  level);
            continue;
        }
        if (s == ST_DOWN || s == ST_BARRED || !barred_up(club, level)) continue;
        g_mem[row + ROW_STATUS] = ST_BARRED;
        if (log_on()) fprintf(stderr, "recomp: bel second: %.51s may not go up from level %d\n", name_of(club), level);
    }
}

/* RECOMP_TEST_BEL_POINTS (tests only): name=+ puts the club on top of its table by points, name=- at the bottom. */
static void test_points(uint32_t league)
{
    const char *spec = getenv("RECOMP_TEST_BEL_POINTS");
    int n;
    uint32_t table = spec ? table_of(league, &n) : 0;
    for (int i = 0; table && i < n; i++) {
        uint32_t row = row_at(table, i);
        const char *name = name_of(rd32(g_mem, row));
        size_t len = strlen(name);
        for (const char *p = len ? spec : ""; (p = strstr(p, name)) != NULL; p++) {
            if ((p != spec && p[-1] != ';') || p[len] != '=') continue;
            int pts = (int16_t)rd16(g_mem, row + ROW_POINTS);
            if (p[len + 1] == '+' && pts < 150) wr16(g_mem, row + ROW_POINTS, (uint16_t)(pts + 300));
            if (p[len + 1] == '-' && pts > -150) wr16(g_mem, row + ROW_POINTS, (uint16_t)(pts - 300));
            break;
        }
    }
}

/* ---- The season ---- */

/* BelSecond::update_positions_and_status 0x420680, thiscall, no arguments. */
void o_00420680(void);
RT_WRAP(00420680, bel_second_status)
{
    uint32_t self = cpu.ecx;
    if (self && self == league_of(KEY_SECOND) && season_game_changes()) {
        test_points(self);
        if (rule_on() && season_now() >= BAR_FROM) mark(self, 2);
        season_periods_winners(self);   /* 0.7.2 slice 10: a row with no periods */
    }
    o_00420680();
}

/* The row index of each zone at the bottom of a table, as League::update_positions_and_status 0x684cd0 lays them out
 * (0x684e7d..0x684ef5): from the last row up, +0xc1 rows of the direct relegation, then +0xc0 rows of the relegation
 * play-off, and a row of status 0, 1 or 5 (title, promotion, a period winner) holds no zone and is skipped
 * (0x684ec2..0x684ece). 0x685225 then gives each row the zone at its place +0x39 (zone[place - 1]), so rows that
 * tie at one place (all of them at the start of a season) share its zone. zone[i] is 3, 2 or 0. */
static void bottom_zones(uint32_t table, int n, int down, int playoff, uint8_t *zone)
{
    memset(zone, 0, (size_t)n);
    int want = down, z = ST_DOWN;
    for (int i = n - 1; i >= 0 && z >= ST_DOWN_PLAYOFF; i--) {
        while (!want && z >= ST_DOWN_PLAYOFF) want = --z == ST_DOWN_PLAYOFF ? playoff : 0;
        if (z < ST_DOWN_PLAYOFF) break;
        int s = g_mem[row_at(table, i) + ROW_STATUS];
        if (s == ST_UP || s == ST_PLAYOFF || s == ST_TITLE) continue;
        zone[i] = (uint8_t)z;
        want--;
    }
}

/* A barred U23 side (0xfc) in the zone of the relegation play-off: 0x684cd0 gives 2 only to a row of 0xff or 0xfe
 * (0x685274..0x68527f), so the group would have no row of 2 and BelThird::create_relegation_playoff_stage 0x423ba0
 * stops with an error box (bel_third.cpp 1642, 0x423d68..0x423da5: it needs a row of 2 in each group). The side
 * plays the relegation play-off: it gets 2. The zone is the original's (bottom_zones): a period winner (1) in the
 * place of the play-off moves the zone one row up (open item 223: pass 6, a2026s2, May 2070, SK Beveren won a
 * period of group A and ended 14th, and Union Saint-Gilloise U23 (0xfc) 13th held the zone; item 115 looked at the
 * place only and the box came). It runs with the rule off too (a save made with it on); a row of 0xfc exists only
 * in a season game. A side that holds the zone above its place logs one line always (item 223). */
static void playoff_place(uint32_t league)
{
    int n;
    uint32_t table = table_of(league, &n);
    int down = (int8_t)g_mem[league + DOWN_PLACES], playoff = (int8_t)g_mem[league + DOWN_PLAYOFF];
    if (!table || down < 0 || playoff <= 0) return;
    uint8_t zone[64];
    bottom_zones(table, n, down, playoff, zone);
    for (int i = 0; i < n; i++) {
        uint32_t row = row_at(table, i);
        int pos = (int8_t)g_mem[row + ROW_POS];
        if (pos < 1 || pos > n || zone[pos - 1] != ST_DOWN_PLAYOFF || g_mem[row + ROW_STATUS] != ST_BARRED) continue;
        g_mem[row + ROW_STATUS] = ST_DOWN_PLAYOFF;
        int moved = pos <= n - down - playoff;
        if (log_on() || moved)
            fprintf(stderr, "recomp: bel second: %.51s (place %d) plays the relegation play-off of level 3%s\n",
                    name_of(rd32(g_mem, row)), pos, moved ? " (the zone moved up past a period winner, item 223)" : "");
    }
}

/* Open item 223, the real rule: a Third Division period winner that ends in a place of relegation. BelThird::
 * update_positions_and_status 0x422f70 gives 1 to the winner of each period when its row has 0xff (0x423036..
 * 0x42303c), and the row keeps 1: 0x684cd0 skips it when it lays out the zones of relegation (bottom_zones), so the
 * next row up takes its place in the relegation play-off or the direct relegation, and the period winner plays the
 * promotion play-off. Real life (fr.wikipedia "Championnat de Belgique de football de troisieme division", section
 * on the periods, read 12 November 2026): a club that ends in a place of direct relegation or as "barragiste" may
 * not play the Tour final, and the next club of the final table takes its place. In a season game, at the call of
 * BelThird::create_promotion_playoff_stage 0x423880 (the tables are final: the stage chain 0x423640 calls it at
 * 0x4236f1, before 0x423ba0 at 0x4236f8), when a row of 1 holds a place of relegation, the bottom of the group goes
 * by place: the places of direct relegation (+0xc1) get 3, the places of the play-off (+0xc0) get 2, and a row above
 * them that the move gave 2 or 3 gets back 0xff (0xfc for a barred U23 side, as mark gives it). The original's top-up
 * of 0x423880 (group A 0x4238bd..0x42393c, group B 0x42395a..0x4239d9: the rows of 1 count, then rows of 0xff from
 * the top) gives the free promotion
 * play-off place to the next club. A game with the rules off (or RECOMP_BEL_SECOND=off) keeps the original. Each
 * change logs one line. Ties: BelThird gives tied rows their own places (2067 and 2070 dumps: equal points at
 * places 13, 14 and 15), so each place holds one row. The
 * Second Division has the same way (0x420680 gives 1 at 0x42070e) and no source for its rule: not changed. */
static void period_winner_down(uint32_t league, const char *division)
{
    int n;
    uint32_t table = table_of(league, &n);
    int down = (int8_t)g_mem[league + DOWN_PLACES], playoff = (int8_t)g_mem[league + DOWN_PLAYOFF];
    if (!table || down < 0 || playoff < 0 || down + playoff > n) return;
    int bottom = n - down - playoff, hit = 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = row_at(table, i);
        hit |= (int8_t)g_mem[row + ROW_POS] > bottom && g_mem[row + ROW_STATUS] == ST_PLAYOFF;
    }
    if (!hit) return;
    for (int i = 0; i < n; i++) {
        uint32_t row = row_at(table, i), club = rd32(g_mem, row);
        int s = g_mem[row + ROW_STATUS], to = s, pos = (int8_t)g_mem[row + ROW_POS];
        if (pos > n - down) to = ST_DOWN;
        else if (pos > bottom) to = ST_DOWN_PLAYOFF;
        else if (s == ST_DOWN || s == ST_DOWN_PLAYOFF)
            to = rule_on() && season_now() >= BAR_FROM && barred_up(club, 3) ? ST_BARRED : ST_NONE;
        if (to == s) continue;
        g_mem[row + ROW_STATUS] = (uint8_t)to;
        fprintf(stderr, "recomp: bel second: %.51s, %d of %d in the %s: status %d -> %d (%s; item 223)\n",
                name_of(club), pos, n, division, s, to,
                s == ST_PLAYOFF ? "a period winner in a place of relegation, the next club takes its play-off place"
                                : to == ST_NONE || to == ST_BARRED ? "its place is above the relegation" : "its place");
    }
}

/* BelThird::update_positions_and_status 0x422f70, thiscall, no arguments: groups A (this) and B. */
void o_00422f70(void);
RT_WRAP(00422f70, bel_third_status)
{
    uint32_t self = cpu.ecx;
    if (self && self == league_of(KEY_THIRD) && season_game_changes()) {
        uint32_t b = group_b(self);
        test_points(self);
        if (b) test_points(b);
        if (rule_on() && season_now() >= BAR_FROM) {
            mark(self, 3);
            if (b) mark(b, 3);
        }
    }
    o_00422f70();
    if (!rt_unwinding && self && self == league_of(KEY_THIRD) && season_game_changes()) {
        uint32_t b = group_b(self);
        playoff_place(self);
        if (b) playoff_place(b);
    }
}

/* BelSecond::create_playoff_group 0x420230, thiscall, no arguments. */
void o_00420230(void);
RT_WRAP(00420230, bel_second_playoff)
{
    if (cpu.ecx == league_of(KEY_SECOND) && rule_on() && season_now() >= BAR_FROM)
        hand_on(cpu.ecx, 2, "before the play-off");
    season_bel_po_entrants_in(cpu.ecx);         /* issue 150: First Division clubs in the final round */
    o_00420230();
    season_bel_po_entrants_out();               /* also on an unwind: the rows get their own clubs back */
}

/* The row index that the original's top-up of 0x423880 leaves as the best placed row of 0xff (0x4238c9..0x42393c:
 * the rows of 1 count, then rows of 0xff from the top until 3). -1 when none. */
static int next_after_topup(uint32_t table, int n)
{
    int have = 0;
    for (int i = 0; i < n; i++) have += g_mem[row_at(table, i) + ROW_STATUS] == ST_PLAYOFF;
    for (int i = 0; i < n; i++) {
        if (g_mem[row_at(table, i) + ROW_STATUS] != ST_NONE) continue;
        if (have < 3) {
            have++;
            continue;
        }
        return i;
    }
    return -1;
}

/* BelThird::create_promotion_playoff_stage 0x423880, thiscall, no arguments. */
void o_00423880(void);
RT_WRAP(00423880, bel_third_playoff)
{
    uint32_t self = cpu.ecx, second = league_of(KEY_SECOND), b = group_b(self);
    int season = season_now();
    if (self && self == league_of(KEY_THIRD) && rule_on()) {
        period_winner_down(self, "Third Division group A");
        if (b) period_winner_down(b, "Third Division group B");
    }
    if (self != league_of(KEY_THIRD) || !season_game_changes() || !b || !second) {
        o_00423880();
        return;
    }
    int ns, na, nb;
    uint32_t st = table_of(second, &ns), ta = table_of(self, &na), tb = table_of(b, &nb);
    int playoff_row = season_sizes_bel_third_row(second);
    uint32_t slot = ns > playoff_row ? row_at(st, playoff_row) : 0, sixteenth = slot ? rd32(g_mem, slot) : 0;
    if (!rule_on() || season < DIRECT_FROM || !slot || !ta || !tb) {
        if (log_on() && slot)
            fprintf(stderr, "recomp: bel second: %.51s (status %d) plays the third tier's promotion play-off\n",
                    name_of(sixteenth), g_mem[slot + ROW_STATUS]);
        o_00423880();
        return;
    }
    if (season >= BAR_FROM) {
        hand_on(self, 3, "before the play-off");
        hand_on(b, 3, "before the play-off");
    }
    int ia = next_after_topup(ta, na), ib = next_after_topup(tb, nb);
    int pa = ia >= 0 ? (int16_t)rd16(g_mem, row_at(ta, ia) + ROW_POINTS) : -32768;
    int pb = ib >= 0 ? (int16_t)rd16(g_mem, row_at(tb, ib) + ROW_POINTS) : -32768;
    uint32_t next = ia >= 0 && pa >= pb ? row_at(ta, ia) : ib >= 0 ? row_at(tb, ib) : 0;
    if (!next) {
        fprintf(stderr, "recomp: bel second: fault: no third-tier club is left for the play-off place of %.51s; "
                        "it plays as in the original\n", name_of(sixteenth));
        o_00423880();
        return;
    }
    uint32_t club = rd32(g_mem, next);
    /* The place of the relegation play-off: 2, or 0xfc for a U23 side, which that zone never gives 2 (0x685274..
     * 0x68527f: only a row of 0xff or 0xfe). With the size engine (0.7.2 slice 10) the row of the season may give
     * that place no relegation play-off (+0xc0 0: 0xff) or a direct relegation (3): the club keeps its status and the
     * third-tier club still takes its place in the play-off. A title, promotion or period row is a fault. This is
     * real life in every season from 2015/16 (gap 36 of the 0.7.2 sizes, checked 15 November 2026): no second-tier
     * club played the third tier's promotion play-off (2015/16 none was played; 2016/17 to 2018/19 the bottom four
     * of First Division B played a relegation play-off among themselves; 2019/20 none, COVID; from 2020/21 the last
     * one or two went down directly; the Division 1 play-offs list third-tier clubs only: en.wikipedia.org pages
     * 2015-16_Belgian_Second_Division, Challenger_Pro_League and Belgian_Division_1). */
    int s15 = g_mem[slot + ROW_STATUS];
    int sized = season_sizes_on() && (s15 == ST_NONE || s15 == ST_DOWN || s15 == ST_STAYS);
    if (s15 != ST_DOWN_PLAYOFF && s15 != ST_BARRED && !sized) {
        fprintf(stderr, "recomp: bel second: fault: %.51s (row %d) has status %d, not the place of the relegation "
                        "play-off; it plays as in the original\n", name_of(sixteenth), playoff_row + 1, s15);
        o_00423880();
        return;
    }
    if (!sized) g_mem[slot + ROW_STATUS] = ST_DOWN;
    if (log_on())
        fprintf(stderr, "recomp: bel second: %.51s goes down directly (status %d), %.51s (%d points) takes its "
                        "play-off place\n", name_of(sixteenth), g_mem[slot + ROW_STATUS], name_of(club),
                (int16_t)rd16(g_mem, next + ROW_POINTS));
    wr32(g_mem, slot, club);
    o_00423880();
    wr32(g_mem, slot, sixteenth);
}

/* BelThird::create_relegation_playoff_stage 0x423ba0, thiscall, no arguments: lists the clubs of the four Fourth
 * Divisions. A U23 side whose first team plays in the Third Division or lower next season is no part of the list. */
void o_00423ba0(void);
RT_WRAP(00423ba0, bel_fourth_list)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!rule_on() || season_now() < BAR_FROM || !readable(table) || count <= 0) {
        o_00423ba0();
        return;
    }
    int *held = malloc(sizeof *held * (size_t)count), nheld = 0, listed = 0;
    for (int i = 0; held && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, club + CLUB_NATION) && level_of(club) == 4) listed++;
    }
    for (int i = 0; held && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (level_of(club) != 4 || !u23(club)) continue;
        uint32_t first = parent_of(club);
        if (!first || next_level(first) < 3) continue;       /* its first team plays above the Third Division */
        if (listed - nheld <= 16) {             /* the list needs 16 clubs (0x423c7d) */
            fprintf(stderr, "recomp: bel second: %.51s stays in the Fourth Division list (16 clubs left)\n",
                    name_of(club));
            continue;
        }
        held[nheld++] = i;
    }
    uint32_t *div_of = malloc(sizeof *div_of * (size_t)(nheld ? nheld : 1));
    for (int k = 0; div_of && k < nheld; k++) {
        uint32_t club = table + (uint32_t)held[k] * CLUB_SIZE;
        div_of[k] = rd32(g_mem, club + CLUB_DIVISION);
        wr32(g_mem, club + CLUB_DIVISION, 0);
    }
    o_00423ba0();
    for (int k = 0; div_of && k < nheld; k++) {
        uint32_t club = table + (uint32_t)held[k] * CLUB_SIZE;
        wr32(g_mem, club + CLUB_DIVISION, div_of[k]);
        if (log_on()) fprintf(stderr, "recomp: bel second: %.51s may not go up into its first team's division\n",
                              name_of(club));
    }
    free(held);
    free(div_of);
}

/* ---- The season change ---- */

/* Up to k clubs of 3 that may stay up (not U23 sides), the best placed first, get 0xff. Returns how many. */
static int stay_up(uint32_t league, int k, const char *what)
{
    int n, done = 0;
    uint32_t table = table_of(league, &n);
    for (int i = 0; i < n && done < k; i++) {
        uint32_t row = row_at(table, i);
        if (g_mem[row + ROW_STATUS] != ST_DOWN || u23(rd32(g_mem, row))) continue;
        g_mem[row + ROW_STATUS] = ST_NONE;
        done++;
        if (log_on()) fprintf(stderr, "recomp: bel second: %.51s stays up in %s\n", name_of(rd32(g_mem, row)), what);
    }
    return done;
}

/* Each U23 side of the league that must go down (too_high) gets 3 in exchange for the best placed club of 3,
 * which stays up; with no such club left it stays one more season. */
static void send_down(uint32_t league, int level, const char *what)
{
    int n;
    uint32_t table = table_of(league, &n);
    for (int i = n - 1; i >= 0; i--) {
        uint32_t row = row_at(table, i), club = rd32(g_mem, row);
        if (g_mem[row + ROW_STATUS] == ST_DOWN || !u23(club)) continue;
        uint32_t first = parent_of(club);
        int must = level == 1 || (first && next_level(first) >= level);
        if (!must) continue;
        g_mem[row + ROW_STATUS] = ST_DOWN;
        if (!stay_up(league, 1, what)) {
            g_mem[row + ROW_STATUS] = ST_NONE;
            if (log_on()) fprintf(stderr, "recomp: bel second: %.51s stays in %s one more season (no place goes "
                                  "down for it)\n", name_of(club), what);
            continue;
        }
        if (log_on()) fprintf(stderr, "recomp: bel second: %.51s goes down from %s (its first team %.51s)\n",
                              name_of(club), what, first ? name_of(first) : "none");
    }
}

/* The shield of 2020/21 and 2024/25: each U23 side of the Second Division with 3 gets 0xff and no club goes down in
 * its place (2024/25: Jong Genk 15th stayed up, Seraing 14th too, only Deinze went down). The game holds the
 * Second Division at 18 (0x41fd82) and 0x689c80 moves in pairs, so one promotion of the Third Division drops in
 * exchange: a row of 0 (the winner of the promotion play-off, 0x4231b2), else the row of 5 with the fewest points,
 * gets 0xfe (stays). With no such row the U23 side goes down (logged). */
static void shield(uint32_t league, int season)
{
    int n;
    uint32_t table = table_of(league, &n), third[2];
    int nt = leagues_of(3, third);
    for (int i = 0; i < n; i++) {
        uint32_t row = row_at(table, i), club = rd32(g_mem, row);
        if (g_mem[row + ROW_STATUS] != ST_DOWN || !u23(club)) continue;
        uint32_t parent = parent_of(club);
        if (parent && next_level(parent) >= 2) continue;    /* its first team goes down: step 4 sends it down */
        uint32_t drop = 0;
        int drop_s = 0, drop_pts = 0;
        for (int l = 0; l < nt; l++) {
            int m;
            uint32_t t = table_of(third[l], &m);
            for (int k = 0; k < m; k++) {
                uint32_t r = row_at(t, k);
                int s = g_mem[r + ROW_STATUS], pts = (int16_t)rd16(g_mem, r + ROW_POINTS);
                if (s != ST_UP && s != ST_TITLE) continue;
                /* A row of 0 before any row of 5, then the fewest points, group A on a tie. */
                int better = !drop || (s == ST_UP && drop_s != ST_UP) || (s == drop_s && pts < drop_pts);
                if (better) {
                    drop = r;
                    drop_s = s;
                    drop_pts = pts;
                }
            }
        }
        if (!drop) {
            fprintf(stderr, "recomp: bel second: the shield of %d/%02d finds no promotion of the Third Division to "
                            "drop; %.51s goes down\n", season, (season + 1) % 100, name_of(club));
            continue;
        }
        g_mem[row + ROW_STATUS] = ST_NONE;
        g_mem[drop + ROW_STATUS] = ST_STAYS;
        if (log_on())
            fprintf(stderr, "recomp: bel second: the shield of %d/%02d keeps %.51s up, no club goes down in its place, "
                            "%.51s (status %d) stays in the Third Division\n", season, (season + 1) % 100,
                    name_of(club), name_of(rd32(g_mem, drop)), drop_s);
    }
}

/* A U23 side of the Third Division that goes up into the division of its first team hands the place on. */
static void third_up(uint32_t league)
{
    int n;
    uint32_t table = table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = row_at(table, i), club = rd32(g_mem, row);
        int s = g_mem[row + ROW_STATUS];
        if ((s != ST_UP && s != ST_TITLE) || !u23(club)) continue;
        uint32_t first = parent_of(club);
        if (!first || next_level(first) < 2) continue;         /* its first team plays in the First Division */
        g_mem[row + ROW_STATUS] = ST_BARRED;
        int j = next_free(table, n, 3);
        if (j >= 0) g_mem[row_at(table, j) + ROW_STATUS] = (uint8_t)s;
        if (log_on())
            fprintf(stderr, "recomp: bel second: %.51s may not go up to its first team's division%s%.51s\n",
                    name_of(club), j >= 0 ? ", the place goes to " : "",
                    j >= 0 ? name_of(rd32(g_mem, row_at(table, j))) : "");
    }
}

/* RECOMP_TEST_BEL_DOWN (tests only): the named clubs of the First Division get 3, as a relegated first team, and
 * as many clubs of 3 (not named) stay up first, so the moves stay in pairs. */
static int test_named(const char *spec, const char *name)
{
    size_t len = strlen(name);
    for (const char *p = len ? spec : ""; (p = strstr(p, name)) != NULL; p++)
        if ((p == spec || p[-1] == ';') && (!p[len] || p[len] == ';')) return 1;
    return 0;
}

static void test_down(void)
{
    const char *spec = getenv("RECOMP_TEST_BEL_DOWN");
    int n;
    uint32_t first = league_of(KEY_FIRST), table = spec ? table_of(first, &n) : 0;
    int named[64], k = 0;
    for (int i = 0; table && i < n; i++)
        if (g_mem[row_at(table, i) + ROW_STATUS] != ST_DOWN && test_named(spec, name_of(rd32(g_mem, row_at(table, i)))))
            named[k++] = i;
    k = stay_up(first, k, "the First Division (test)");
    for (int j = 0; j < k; j++) {
        g_mem[row_at(table, named[j]) + ROW_STATUS] = ST_DOWN;
        fprintf(stderr, "recomp: bel second: test: %.51s goes down from the First Division\n",
                name_of(rd32(g_mem, row_at(table, named[j]))));
    }
}

static void census(const char *when)
{
    if (!log_on()) return;
    int clubs[LEVELS + 1] = {0}, sides[LEVELS + 1] = {0};
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; readable(table) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (!belgian(club)) continue;
        int level = level_of(club);
        clubs[level]++;
        if (!u23(club)) continue;
        sides[level]++;
        if (level <= 4) {
            uint32_t first = parent_of(club);
            fprintf(stderr, "recomp: bel second: %s: %.51s at level %d, first team %.51s at level %d\n", when,
                    name_of(club), level, first ? name_of(first) : "none", first ? level_of(first) : 0);
        }
    }
    fprintf(stderr, "recomp: bel second: census %s: First %d clubs, %d U23; Second %d clubs, %d U23; Third %d clubs, "
            "%d U23; Fourth %d clubs, %d U23\n", when, clubs[1], sides[1], clubs[2], sides[2], clubs[3], sides[3],
            clubs[4], sides[4]);
}

/* BelFirst::process_promotion_relegation_chain 0x41ee80 (vtable +0xb0), thiscall ret 4. */
void o_0041ee80(void);
RT_WRAP(0041ee80, bel_second_chain)
{
    if (!season_game_changes() || season_change_is_creating()) {
        o_0041ee80();
        return;
    }
    int season = (int16_t)rd16(g_mem, GAME_DATE + 2) - 1;      /* the summer after the season that ended */
    census("before");
    season_bel_po_settle();                     /* issue 150: the result of the final round */
    if (rt_unwinding) return;
    test_down();
    if (rule_on() && season >= BAR_FROM) {
        uint32_t first = league_of(KEY_FIRST), second = league_of(KEY_SECOND), third[2];
        int nt = leagues_of(3, third);
        hand_on(second, 2, "at the season change");
        for (int l = 0; l < nt; l++) hand_on(third[l], 3, "at the season change");
        send_down(first, 1, "the First Division");
        for (int l = 0; l < nt; l++) third_up(third[l]);
        if (season == 2020 || season == 2024) shield(second, season);
        send_down(second, 2, "the Second Division");
        for (int l = 0; l < nt; l++) send_down(third[l], 3, l ? "Third Division group B" : "Third Division group A");
    }
    o_0041ee80();
    if (!rt_unwinding) {
        season_bel_po_moved();
        census("after");
        if (old_left > 0) old_left--;
    }
}

int season_bel_barred(uint32_t club, uint32_t div)
{
    int season = season_change_is_creating() ? season_start_year() : season_now();
    if (switched_off() || season < BAR_FROM || !readable(club) || !readable(div) || !u23(club)) return 0;
    int level = level_of_div(div);
    if (level == 1) return 1;
    uint32_t first = parent_of(club);
    return first && level <= 4 && level_of(first) >= level;
}
