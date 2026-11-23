/* England: League Two and the Conference exchange the clubs of the places of their rows (0.7.2 league sizes, slice 8,
 * D15; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md).
 *
 * The original. EngPrm::start_new_season 0x574c10 runs the chain (vtable +0xb0, 0x574c23) and then one of two
 * Conference exchanges (0x574c3a: the Conference object of [0x9cf69c] in the object table [0xadadfc]):
 * - A Conference that the game plays: the chain 0x5752e0 calls league_promote_and_relegate 0x689c80 for League
 *   Two and the Conference (0x57548d, pairs 1) when the Conference champion may go up (0x599e00 at 0x57541a). Else the
 *   last row of League Two gets status 0xfe (0x575438) and stays, and no club moves (0x57545f, 0x575468). In pairs
 *   the call moves as many clubs as the places of both divisions give, so this path takes the places of the rows once
 *   both divisions have them (season_sizes.c sizes_rules). exchange_conference_feeder_clubs 0x5750a0 then swaps 3
 *   clubs of three feeder leagues (push 3 at 0x5751f8, cmp bl, 3 at 0x575289) with the Conference rows of status 3.
 * - A Conference that the game does not play (the default: the setup screen has it as an extra division):
 *   exchange_conference_clubs_unplayed 0x574e60 lists the clubs of the Conference record (0x574ed9..0x574f01), sorts
 *   them and shuffles the top 1 (comp_util_sort_and_shuffle_top_clubs 0x4afbd0, push 1 at 0x574f7c), moves the first
 *   to League Two when it may go up (0x599e00 at 0x574fb7; League::move_club_to_division 0x6830b0 at 0x57502e), and
 *   then sends every League Two row of status 3 to the Conference record (0x6831a0 at 0x575070). An ineligible first
 *   club leaves the last League Two row at 0xfe and moves nothing (0x574fcd, 0x575002). So it is 1 up and every row of
 *   status 3 down: League Two relegates 1 in the class (EngThird::set_up_rules 0x578111), and with the 2 of the rows
 *   from 2002/03 it fell to 23 clubs (slice 4).
 *
 * The fix. League Two takes the relegation places of its row in a season game whether or not the Conference object
 * exists (season_sizes_eng_below): the object of a played Conference can be made after League Two at a new game, and
 * an unplayed one has none. The wrap of 0x574e60 runs the original and then moves up as many more clubs of the
 * Conference record as went down beyond the one it moved. It picks them as the original picks its one
 * (comp_util_sort_and_shuffle_top_clubs 0x4afbd0, cdecl (list, count, need): qsort by reputation from high to low,
 * compare 0x4b07d0 on club +0x80 at 0x4afc02, then random swaps among the first 3 * need, 0x4afc13..0x4afc57), the
 * first that may go up (0x599e00 as 0x574f85..0x574fb7). With no table to place them, this pick is the nearest form
 * of the real champion and play-off winner (gap row 27). When none is left, the best placed of the clubs that went
 * down stay in League Two. The base game and a League Two of one relegation place keep the original alone.
 *
 * Gap fixes (15 November 2026, lead-notes/sizes-gaps.md rows 25 to 29): the feeder exchange takes the places of the
 * Conference's row (eng_conf_feeders), a champion that may not go up gives its place to the next club (eng_chain),
 * and the unplayed exchange picks as the original picks (above). */
#include "win.h"
#include "../rt_host.h"
#include "season_change.h"
#include "season_sizes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u             /* the club table */
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u         /* club_comp pointer; the record's first dword is its id (0x574ee0) */
#define COMP_OBJECTS 0xadadfcu      /* the competition objects by record id */
#define COMP_SLOTS 0xadadf8u
#define COMP_RECORD 4u              /* object +4: the competition record */
#define COMP_CLUBS 0x3eu
#define COMP_TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define KEY_THIRD 0x9cf5c8u         /* League Two's record id (0x574e74) */
#define KEY_CONF 0x9cf69cu          /* the Conference's record id (0x574ee2) */
#define RECORDS 0xae23d0u           /* the competition records, 0x6b bytes each (0x575012..0x575028) */
#define RECORD_SIZE 0x6bu
#define CHECKER 0xae2a38u           /* thiscall object of 0x5a0590 (0x574fa9) */
#define F_CHECKER_OF 0x005a0590u    /* thiscall (club): the object whose 0x599e00 says whether the club may go up */
#define F_MAY_GO_UP 0x00599e00u     /* thiscall (+0xe4, +0xe2 of League Two, its record id, 1): al 1 when it may */
#define F_PICK 0x004afbd0u          /* comp_util_sort_and_shuffle_top_clubs, cdecl (list, count, need) */
#define F_MOVE_CLUB 0x006830b0u     /* League::move_club_to_division, thiscall (club, record, 1) (0x57502e) */
#define F_RELEGATE 0x006831a0u      /* League::relegate_club_to_division, thiscall (club, record, 1) (0x5752b7) */
#define F_EXCHANGE 0x00689c80u      /* league_promote_and_relegate, thiscall (upper, lower, 1, arg, -1, -1) */
#define CLUB_NATION 0x53u           /* nation pointer; its first dword is its id (0x5751ab) */
#define KEY_NATION 0x9cf2e4u        /* England's nation id (0x5751ad) */
#define STATUS_UP 5                 /* a row that the exchange 0x689c80 sends up (0x689cc1) */
#define STATUS_WINNER 0             /* the play-off winner (0x576d9e), sent up too (0x689cc5) */
#define STATUS_STAYS 0xfe           /* the last League Two row that stays (0x575438) */
#define STATUS_DOWN 3

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SIZE_LOG") && !strcmp(getenv("RECOMP_SIZE_LOG"), "1");
    return on;
}

int season_sizes_eng_below(const char *file)
{
    return file && (!strcmp(file, "eng_third") || !strcmp(file, "eng_conf"));
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t object_of(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key), slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    uint32_t table = rd32(g_mem, COMP_OBJECTS);
    if (id < 0 || id >= slots || !readable(table)) return 0;
    uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(obj) ? obj : 0;
}

/* 1 when the club's division is the record of the id at key. */
static int in_division(uint32_t club, uint32_t key)
{
    uint32_t d = rd32(g_mem, club + CLUB_DIVISION);
    return readable(d) && rd32(g_mem, d) == rd32(g_mem, key);
}

static const char *name_of(uint32_t club) { return win_str(club + 4); }

/* The clubs of the Conference record in the order of the original's pick for need clubs (0x4afbd0 on a list in
 * the game's heap, as 0x574f7c calls it). Returns the count put back in conf. */
static int pick_order(uint32_t *conf, int nconf, int need)
{
    uint32_t list = season_sizes_game_malloc(4u * (uint32_t)nconf);
    if (!list) return 0;
    int n = 0;
    for (int k = 0; k < nconf; k++)
        if (in_division(conf[k], KEY_CONF)) wr32(g_mem, list + 4u * (uint32_t)n++, conf[k]);
    if (n > 0) {
        uint32_t a[3] = {list, (uint32_t)n, (uint32_t)(need < n ? need : n)};
        Cpu saved = cpu;
        rt_callback(F_PICK, 3, a);
        if (!rt_unwinding) cpu = saved;
    }
    for (int k = 0; k < n && !rt_unwinding; k++) conf[k] = rd32(g_mem, list + 4u * (uint32_t)k);
    season_sizes_game_free(list);
    return rt_unwinding ? 0 : n;
}

/* As 0x574f85..0x574fbc: the club may go up to League Two. */
static int may_go_up(uint32_t third, uint32_t club)
{
    uint32_t a1[1] = {club};
    uint32_t checker = call_this(F_CHECKER_OF, rd32(g_mem, CHECKER), 1, a1);
    if (rt_unwinding || !readable(checker)) return 0;
    uint32_t rec = rd32(g_mem, third + COMP_RECORD);
    uint32_t a2[4] = {(uint32_t)(int32_t)(int16_t)rd16(g_mem, third + 0xe4),
                      (uint32_t)(int32_t)(int16_t)rd16(g_mem, third + 0xe2), readable(rec) ? rd32(g_mem, rec) : 0, 1};
    return (call_this(F_MAY_GO_UP, checker, 4, a2) & 0xff) != 0;
}

void o_00574e60(void);

/* EngPrm::exchange_conference_clubs_unplayed 0x574e60, thiscall (EngPrm), no arguments. */
RT_WRAP(00574e60, eng_conf_unplayed)
{
    uint32_t self = cpu.ecx, third = object_of(KEY_THIRD);
    uint32_t clubs = rd32(g_mem, CLUBS);
    int count = (int32_t)rd32(g_mem, CLUB_COUNT), rows = third ? (int16_t)rd16(g_mem, third + COMP_CLUBS) : 0;
    uint32_t table = third ? rd32(g_mem, third + COMP_TABLE) : 0;
    int down = 0;
    for (int i = 0; readable(table) && i < rows && i < 64; i++)
        down += g_mem[table + (uint32_t)i * ROW_SIZE + ROW_STATUS] == STATUS_DOWN;
    uint32_t *conf = NULL, *went = NULL;
    if (season_sizes_on() && down > 1 && readable(clubs) && count > 0 && count < 100000) {
        conf = malloc(sizeof *conf * (size_t)count);
        went = malloc(sizeof *went * (size_t)down);
    }
    if (!conf || !went) {
        free(conf);
        free(went);
        o_00574e60();
        return;
    }
    /* Before the original: the clubs of the Conference record and the League Two rows of status 3. */
    int nconf = 0, nwent = 0;
    for (int i = 0; i < count; i++) {
        uint32_t club = clubs + (uint32_t)i * CLUB_SIZE;
        if (in_division(club, KEY_CONF)) conf[nconf++] = club;
    }
    for (int i = 0; i < rows && i < 64 && nwent < down; i++) {
        uint32_t row = table + (uint32_t)i * ROW_SIZE;
        if (g_mem[row + ROW_STATUS] == STATUS_DOWN) went[nwent++] = rd32(g_mem, row);
    }
    o_00574e60();
    if (rt_unwinding) {
        free(conf);
        free(went);
        return;
    }
    int moved_down = 0, moved_up = 0;
    for (int k = 0; k < nwent; k++) moved_down += readable(went[k]) && in_division(went[k], KEY_CONF);
    for (int k = 0; k < nconf; k++) moved_up += in_division(conf[k], KEY_THIRD);
    int need = moved_down - moved_up;
    if (logging())
        fprintf(stderr, "recomp: league sizes: eng: the unplayed Conference exchange moved %d down and %d up (%d rows "
                        "of status 3)\n", moved_down, moved_up, down);
    if (need > 0) {
        nconf = pick_order(conf, nconf, need);
        uint32_t rec = rd32(g_mem, KEY_THIRD);
        for (int k = 0; k < nconf && need > 0 && !rt_unwinding; k++) {
            uint32_t club = conf[k];
            if (!in_division(club, KEY_CONF)) continue;
            if (!may_go_up(third, club)) {
                if (logging()) fprintf(stderr, "recomp: league sizes: eng: %.51s may not go up\n", name_of(club));
                continue;
            }
            uint32_t a[3] = {club, rd32(g_mem, RECORDS) + rec * RECORD_SIZE, 1};
            call_this(F_MOVE_CLUB, self, 3, a);
            need--;
            if (logging())
                fprintf(stderr, "recomp: league sizes: eng: %.51s goes up to League Two (the original's pick)\n",
                        name_of(club));
        }
        /* No Conference club left that may go up: the best placed League Two clubs that went down stay up, as the
         * original keeps its last row (0xfe at 0x574fcd) when its one club may not go up. Real life (gap row 28):
         * the FA's National League System regulations 2025/26, 5.5.2, fill a vacancy at step 1 or 2 with the best
         * placed relegated club
         * (thefa.com/-/media/files/thefaportal/governance-docs/rules-of-the-association/2025-26/
         * 21---national-league-system-regulations.ashx). League Two keeps its count
         * also with changing rules off, where no size step runs. went[] is in the order of the table. */
        for (int k = 0; k < nwent && need > 0 && !rt_unwinding; k++) {
            if (!readable(went[k]) || !in_division(went[k], KEY_CONF)) continue;
            uint32_t a[3] = {went[k], rd32(g_mem, RECORDS) + rec * RECORD_SIZE, 1};
            call_this(F_MOVE_CLUB, self, 3, a);
            need--;
            fprintf(stderr, "recomp: league sizes: eng: %.51s stays in League Two (no Conference club may go up)\n",
                    name_of(went[k]));
        }
    }
    free(conf);
    free(went);
}

/* ---- The played Conference (gap rows 25 and 26, 15 November 2026) ---- */

/* The club of a league row, or 0. */
static uint32_t row_club(uint32_t league, int i)
{
    uint32_t table = rd32(g_mem, league + COMP_TABLE);
    int rows = (int16_t)rd16(g_mem, league + COMP_CLUBS);
    if (!readable(table) || i < 0 || i >= rows || i >= 64) return 0;
    uint32_t club = rd32(g_mem, table + (uint32_t)i * ROW_SIZE);
    return readable(club) ? club : 0;
}

static uint8_t *row_status(uint32_t league, int i)
{
    return &g_mem[rd32(g_mem, league + COMP_TABLE) + (uint32_t)i * ROW_SIZE + ROW_STATUS];
}

static uint32_t record_ptr(uint32_t key)
{
    return rd32(g_mem, RECORDS) + rd32(g_mem, key) * RECORD_SIZE;
}

/* Test seam: RECOMP_TEST_REFUSE_CHAMPION=1 makes the Conference champion one that may not go up. */
static int refuse_champion(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_TEST_REFUSE_CHAMPION") && !strcmp(getenv("RECOMP_TEST_REFUSE_CHAMPION"), "1");
    return on;
}

void o_00599e00(void);

/* The check 0x599e00, thiscall (club object: +8 the club's index, 0x599e0f..0x599e22), (+0xe4, +0xe2 of a league,
 * its record id, 1), ret 0x10: al 1 when the club may go up. Only the test seam changes it. */
RT_WRAP(00599e00, may_go_up_check)
{
    if (!refuse_champion() || !season_sizes_on()) {
        o_00599e00();
        return;
    }
    uint32_t obj = cpu.ecx, rec_id = rt_arg(2), conf = object_of(KEY_CONF);
    uint32_t club = rd32(g_mem, CLUBS) + rd32(g_mem, obj + 8) * CLUB_SIZE;
    if (conf && rec_id == rd32(g_mem, KEY_THIRD) && club == row_club(conf, 0)) {
        cpu.eax = (cpu.eax & 0xffffff00u);
        cpu.esp += 4 + 0x10;   /* ret 0x10 */
        return;
    }
    o_00599e00();
}

void o_005752e0(void);

/* EngPrm::process_promotion_relegation_chain 0x5752e0, thiscall (EngPrm), one argument, ret 4. With a played
 * Conference it checks the Conference champion (row 0) with 0x599e00 (0x575400..0x57541a). A champion that may not
 * go up gets the original's refusal: the last League Two row gets 0xfe (0x575438), a news item (0x48ece0 at
 * 0x57545f), and no club moves (return at 0x575468, before the call 0x689c80 at 0x57548d).
 *
 * Real life (gap row 26): the FA's National League System regulations 2025/26, 5.2: a champion that fails the
 * criteria is not promoted and the club placed next takes its place. A play-off winner is not replaced. So after
 * the refusal the first row below that may go up and that the exchange does not send up already gets status 5,
 * the champion's row loses it, League Two's last row is again a relegation row, and the exchange runs as at
 * 0x57546b..0x57548d. With no such row the original's refusal stays. */
RT_WRAP(005752e0, eng_chain)
{
    uint32_t self = cpu.ecx, arg = rt_arg(0);
    o_005752e0();
    if (rt_unwinding || !season_sizes_on()) return;
    uint32_t conf = object_of(KEY_CONF), third = object_of(KEY_THIRD);
    int last = third ? (int16_t)rd16(g_mem, third + COMP_CLUBS) - 1 : -1;
    uint32_t champion = conf ? row_club(conf, 0) : 0;
    if (!champion || last < 0 || !row_club(third, last) || *row_status(third, last) != STATUS_STAYS) return;
    if (may_go_up(third, champion) || rt_unwinding) return;
    int rows = (int16_t)rd16(g_mem, conf + COMP_CLUBS), next = -1;
    /* Not past the relegation places: a row of status 3 goes down (review of 16 November). */
    for (int i = 1; i < rows && i < 64 && next < 0 && !rt_unwinding && *row_status(conf, i) != STATUS_DOWN; i++)
        if (row_club(conf, i) && *row_status(conf, i) != STATUS_UP && *row_status(conf, i) != STATUS_WINNER &&
            may_go_up(third, row_club(conf, i)))
            next = i;
    if (next < 0 || rt_unwinding) {
        fprintf(stderr, "recomp: league sizes: eng: %.51s may not go up, and no club below may: no club moves\n",
                name_of(champion));
        return;
    }
    *row_status(conf, 0) = STATUS_STAYS;    /* 0 and 5 both go up (0x689cc1..0x689cc7) */
    *row_status(conf, next) = STATUS_UP;
    *row_status(third, last) = STATUS_DOWN;
    fprintf(stderr, "recomp: league sizes: eng: %.51s may not go up: %.51s (place %d) goes up in its place\n",
            name_of(champion), name_of(row_club(conf, next)), next + 1);
    uint32_t a[6] = {third, conf, 1, arg, 0xffffffffu, 0xffffffffu};
    call_this(F_EXCHANGE, self, 6, a);
}

/* A club of England below the Conference: the pool of 0x575158..0x5751dd (a division and a nation, the division
 * none of the records [0x9cf764], [0x9cf5bc] to [0x9cf5c8] and [0x9cf69c], the nation's id [0x9cf2e4]). */
static int feeder_club(uint32_t club)
{
    static const uint32_t keys[] = {0x9cf764u, 0x9cf5bcu, 0x9cf5c0u, 0x9cf5c4u, 0x9cf5c8u, KEY_CONF};
    uint32_t d = rd32(g_mem, club + CLUB_DIVISION), n = rd32(g_mem, club + CLUB_NATION);
    if (!readable(d) || !readable(n)) return 0;
    for (size_t k = 0; k < sizeof keys / sizeof *keys; k++)
        if (rd32(g_mem, d) == rd32(g_mem, keys[k])) return 0;
    return rd32(g_mem, n) == rd32(g_mem, KEY_NATION);
}

/* The clubs whose division is the Conference record. */
static int conf_count(void)
{
    uint32_t clubs = rd32(g_mem, CLUBS);
    int count = (int32_t)rd32(g_mem, CLUB_COUNT), n = 0;
    for (int i = 0; readable(clubs) && i < count && i < 100000; i++)
        n += in_division(clubs + (uint32_t)i * CLUB_SIZE, KEY_CONF);
    return n;
}

void o_005750a0(void);

/* EngPrm::exchange_conference_feeder_clubs 0x5750a0, thiscall (EngPrm), no arguments. The original takes the clubs
 * of England below the Conference (0x575158..0x5751dd), orders them with 0x4afbd0 for 3 (push 3 at 0x5751f8),
 * moves up the first club of each of three divisions (0x57520f..0x575268), and sends the Conference rows of status
 * 3 down, one to each of those divisions, at most 3 (cmp bl, 3 at 0x575289; 0x6831a0 at 0x5752b7).
 *
 * Real life (gap row 25): the Conference relegates the count of its row (4 from 2006/07; season_sizes.c
 * sizes_rules sets it), and as many clubs come up (2 from each of Conference North and South). So the rows of
 * status 3 that the original leaves go down too, each to the division of a club that comes up in its place, picked
 * as the original picks (0x4afbd0, then 0x6830b0). The base game and a Conference of 3 or fewer relegation places
 * keep the original alone. A Conference of fewer than 3 takes 3 (the original); the size step (D13) brings it to
 * its row's count. */
RT_WRAP(005750a0, eng_conf_feeders)
{
    uint32_t self = cpu.ecx, conf = object_of(KEY_CONF);
    int rows = conf ? (int16_t)rd16(g_mem, conf + COMP_CLUBS) : 0, down = 0;
    uint32_t went[64];
    for (int i = 0; conf && i < rows && i < 64; i++)
        if (*row_status(conf, i) == STATUS_DOWN && row_club(conf, i)) went[down++] = row_club(conf, i);
    int before = conf_count();
    o_005750a0();
    if (rt_unwinding || !season_sizes_on() || down <= 3) return;
    int left = 0;
    uint32_t stay[64];
    for (int k = 0; k < down; k++)
        if (in_division(went[k], KEY_CONF)) stay[left++] = went[k];
    uint32_t clubs = rd32(g_mem, CLUBS);
    int count = (int32_t)rd32(g_mem, CLUB_COUNT), n = 0;
    uint32_t list = left > 0 && readable(clubs) && count > 0 && count < 100000 ?
                    season_sizes_game_malloc(4u * (uint32_t)count) : 0;
    for (int i = 0; list && i < count; i++) {
        uint32_t club = clubs + (uint32_t)i * CLUB_SIZE;
        int gone = 0;
        for (int k = 0; k < down; k++) gone |= went[k] == club;
        if (!gone && feeder_club(club)) wr32(g_mem, list + 4u * (uint32_t)n++, club);
    }
    int up = 0, sent = 0;   /* beyond the original's */
    if (n > 0) {
        uint32_t a[3] = {list, (uint32_t)n, (uint32_t)(left < n ? left : n)};
        Cpu saved = cpu;
        rt_callback(F_PICK, 3, a);
        if (!rt_unwinding) cpu = saved;
        for (int k = 0; k < n && up < left && !rt_unwinding; k++) {
            uint32_t club = rd32(g_mem, list + 4u * (uint32_t)k), from = rd32(g_mem, club + CLUB_DIVISION);
            uint32_t m[3] = {club, record_ptr(KEY_CONF), 1};
            call_this(F_MOVE_CLUB, self, 3, m);
            if (rt_unwinding) break;
            uint32_t r[3] = {stay[up], from, 1};
            call_this(F_RELEGATE, self, 3, r);
            up++;
            sent++;
            if (logging())
                fprintf(stderr, "recomp: league sizes: eng: %.51s comes up to the Conference, %.51s goes down\n",
                        name_of(club), name_of(stay[up - 1]));
        }
    }
    if (list) season_sizes_game_free(list);
    int gone = 0;
    for (int k = 0; k < down; k++) gone += !in_division(went[k], KEY_CONF);
    if (logging() || sent < left)
        fprintf(stderr, "recomp: league sizes: eng: the feeder exchange sent %d down and %d up (%d rows of status "
                        "3, %d beyond the original's 3)\n", gone, conf_count() - before + gone, down, left);
}
