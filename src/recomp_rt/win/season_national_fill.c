/* The fill of a short national side before its match (season games only).
 *
 * VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0 tops up both sides of a fixture (0x90dae0 at 0x90d3bd and 0x90d3f4).
 * The top-up gives no virtual players to the side of a nation with a reputation above 500 (0x90dd5d..0x90dd73),
 * so a national side with few players stays short. The original then shows virtual_staff.cpp 195 (0x90d4bc) and
 * plays on with no check (0x90d4dc); in a season game the wrap in season_records.c skips the box. With fewer than
 * 11 the team pick stops the match at match_eng.cpp 612 or 652 (init_match 0x6b7f20, 0x6b89be): Holland U21 had 10
 * players in an Under 21 International in August 2024 (2023 start, October 2023 database, after a load).
 *
 * season_national_fill fills such a side after the top-up, as the top-up does for clubs: a national side (club id
 * from clubs - 2 x nations, database_is_national_team 0x53b900) with fewer than FILL_TO players it can field (in
 * its squad slots, not injured and in no other national squad) takes the
 * players of its nation (staff +0x1a, or the second nationality +0x1e, as 0x53d980 reads both) by current ability (player row +0x05), those not injured (status 2 of
 * Injury::get_player_injury_status 0x62c400, as count_available_players 0x90da0f skips), and for an under-21 side
 * (id from clubs - nations, 0x5409b0) only those born in the year 21 before the game year or later, and none in
 * the squad of another national side (called_up). */

#include "win.h"
#include "season.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define STAFF 0xae23bcu
#define STAFF_COUNT 0xae2364u
#define STAFF_SIZE 0x6eu
#define GAME_YEAR 0xae2c92u
#define INJURIES 0xb63d28u
#define INJURY_STATUS 0x62c400u     /* thiscall INJURIES (player), 2 = injured */
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define CLUB_SQUAD 0xd7u
#define SQUAD 50
#define FILL_TO 15                  /* the squad check of 0x90d2f0 (0x90d469) */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static int injured(uint32_t player)
{
    Cpu saved = cpu;
    uint32_t a[1] = {player};
    cpu.ecx = rd32(g_mem, INJURIES);
    uint32_t r = rt_callback(INJURY_STATUS, 1, a);
    if (rt_unwinding) return 1;
    cpu = saved;
    return (r & 0xff) == 2;
}

static int by_address(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

/* The players in the squad of another national side (sorted, for bsearch): the senior side and the under-21 side
 * of a nation often play on the same days, and a player called up there is on international duty for the other
 * (NationalTeams::get_international_duty_club 0x7757d0 in the team pick): Belgium U21 had 15 squad slots after the
 * fill but fielded 9 on 2 September 2024 (2022 start, after the second load). Returns the count; *out is malloced. */
static int called_up(uint32_t club, uint32_t **out)
{
    int clubs = (int)rd32(g_mem, CLUB_COUNT), nations = (int)rd32(g_mem, NATION_COUNT), n = 0;
    uint32_t base = rd32(g_mem, CLUBS);
    uint32_t *v = malloc(sizeof *v * (size_t)(2 * nations * SQUAD + 1));
    if (!v) rt_fatal("national fill: no memory");
    for (int i = clubs - 2 * nations; i >= 0 && i < clubs; i++) {
        uint32_t c = base + (uint32_t)i * CLUB_SIZE;
        if (c == club) continue;
        for (uint32_t j = 0; j < SQUAD; j++) {
            uint32_t sp = rd32(g_mem, c + CLUB_SQUAD + 4 * j);
            if (sp) v[n++] = sp;
        }
    }
    qsort(v, (size_t)n, sizeof *v, by_address);
    *out = v;
    return n;
}

static void fill_side(uint32_t club)
{
    if (!readable(club)) return;
    int id = (int)rd32(g_mem, club), clubs = (int)rd32(g_mem, CLUB_COUNT), nations = (int)rd32(g_mem, NATION_COUNT);
    if (id < clubs - 2 * nations || id >= clubs) return;
    int u21 = id >= clubs - nations, n = 0;
    uint32_t nation = rd32(g_mem, club + CLUB_NATION);
    if (!readable(nation)) return;
    uint32_t *busy = NULL;
    int nbusy = called_up(club, &busy);
    for (uint32_t j = 0; j < SQUAD && !rt_unwinding; j++) {     /* the players it can field */
        uint32_t sp = rd32(g_mem, club + CLUB_SQUAD + 4 * j);
        n += sp && !bsearch(&sp, busy, (size_t)nbusy, sizeof *busy, by_address) && !injured(sp);
    }
    if (n >= FILL_TO || rt_unwinding) {
        free(busy);
        return;
    }
    int before = n, born_from = (int)rd16(g_mem, GAME_YEAR) - 21;
    uint32_t staff = rd32(g_mem, STAFF), count = rd32(g_mem, STAFF_COUNT);
    while (n < FILL_TO && !rt_unwinding) {
        uint32_t best = 0;
        int best_ca = -1;
        for (uint32_t i = 0; readable(staff) && i < count; i++) {
            uint32_t sp = staff + i * STAFF_SIZE, pr = rd32(g_mem, sp + 0x61);
            if (!readable(pr) || (rd32(g_mem, sp + 0x1a) != nation && rd32(g_mem, sp + 0x1e) != nation)) continue;
            if (u21 && (int16_t)rd16(g_mem, sp + 0x12) < born_from) continue;
            int ca = (int16_t)rd16(g_mem, pr + 5);
            if (ca <= best_ca) continue;
            int in = 0;
            for (uint32_t j = 0; j < SQUAD && !in; j++) in = rd32(g_mem, club + CLUB_SQUAD + 4 * j) == sp;
            if (in || bsearch(&sp, busy, (size_t)nbusy, sizeof *busy, by_address) || injured(sp)) continue;
            best = sp, best_ca = ca;
        }
        if (!best) break;
        uint32_t j = 0;
        while (j < SQUAD && rd32(g_mem, club + CLUB_SQUAD + 4 * j)) j++;
        if (j == SQUAD) break;
        wr32(g_mem, club + CLUB_SQUAD + 4 * j, best);
        n++;
    }
    char name[52];
    memcpy(name, g_mem + club + 4, 51);
    name[51] = 0;
    /* Why short: the squad slots it had, the players of its nation in the data (by age for an under-21 side), how
     * many of them sit in another national squad, and the nation's reputation (the top-up gives no virtual
     * players above 500, 0x90dd5d). */
    int slots = 0, data = 0, elsewhere = 0;
    for (uint32_t j = 0; j < SQUAD; j++) slots += rd32(g_mem, club + CLUB_SQUAD + 4 * j) != 0;
    for (uint32_t i = 0; readable(staff) && i < count; i++) {
        uint32_t sp = staff + i * STAFF_SIZE;
        if (!readable(rd32(g_mem, sp + 0x61)) || (rd32(g_mem, sp + 0x1a) != nation && rd32(g_mem, sp + 0x1e) != nation)) continue;
        if (u21 && (int16_t)rd16(g_mem, sp + 0x12) < born_from) continue;
        data++;
        elsewhere += bsearch(&sp, busy, (size_t)nbusy, sizeof *busy, by_address) != NULL;
    }
    fprintf(stderr, "recomp: national fill: %s%s had %d players it can field, now %d (squad slots %d, players of the nation "
                    "in the data %d, in another national squad %d, nation +0x88 %d)\n", name, u21 ? " (under-21)" : "",
            before, n, slots, data, elsewhere, (int)rd32(g_mem, nation + 0x88));
    free(busy);
}

/* Before the top-up: a national side whose nation has no squad sizes in NationalTeams (the byte pairs at
 * [[0xb74318] + 4] + 110 x the nation id + 20 x kind + 0x42 and + 0x43 that 0x7754c0 and 0x775520 read, kinds 0 to 2,
 * 0x7754fa..0x775516) gets no players from the top-up (0x90dcb7..0x90dcd9 for a senior side): the data keeps
 * nations with no players of either nationality, such as Netherlands Antilles (2022 start, the 2023-24 CONCACAF
 * Nations League) and Saint-Martin (2018 start, 2019-20). Such a nation takes the sizes of the other side's nation,
 * or of the first nation with sizes, so the top-up makes its virtual players as for any other side. */
#define NATIONAL_TEAMS 0xb74318u

static uint32_t size_at(uint32_t table, uint32_t nation_id, int k)
{
    return table + 110u * nation_id + 20u * (uint32_t)(k / 2) + 0x42u + (uint32_t)(k % 2);
}

static int has_sizes(uint32_t table, uint32_t nation_id)
{
    for (int k = 0; k < 6; k++)
        if (g_mem[size_at(table, nation_id, k)]) return 1;
    return 0;
}

static uint32_t nation_id_of(uint32_t club)
{
    uint32_t nat = readable(club) ? rd32(g_mem, club + CLUB_NATION) : 0;
    return readable(nat) ? rd32(g_mem, nat) : 0xffffffffu;
}

static void give_sizes(uint32_t club, uint32_t other)
{
    int id = readable(club) ? (int)rd32(g_mem, club) : -1;
    int clubs = (int)rd32(g_mem, CLUB_COUNT), nations = (int)rd32(g_mem, NATION_COUNT);
    if (id < clubs - 2 * nations || id >= clubs) return;
    uint32_t nt = rd32(g_mem, NATIONAL_TEAMS), table = readable(nt) ? rd32(g_mem, nt + 4) : 0;
    uint32_t nid = nation_id_of(club), from = nation_id_of(other);
    if (!readable(table) || nid >= (uint32_t)nations || has_sizes(table, nid)) return;
    if (from >= (uint32_t)nations || !has_sizes(table, from))
        for (from = 0; from < (uint32_t)nations && !has_sizes(table, from); from++) {}
    if (from >= (uint32_t)nations) return;
    for (int k = 0; k < 6; k++) g_mem[size_at(table, nid, k)] = g_mem[size_at(table, from, k)];
    char name[52];
    memcpy(name, g_mem + club + 4, 51);
    name[51] = 0;
    fprintf(stderr, "recomp: national fill: %s had no squad sizes, takes those of nation %u\n", name, from);
}

void season_national_sizes(uint32_t fixture)
{
    if (!readable(fixture)) return;
    uint32_t home = rd32(g_mem, fixture + 0x1c), away = rd32(g_mem, fixture + 0x20);
    give_sizes(home, away);
    give_sizes(away, home);
}

void season_national_fill(uint32_t fixture)
{
    if (!readable(fixture)) return;
    fill_side(rd32(g_mem, fixture + 0x1c));
    if (!rt_unwinding) fill_side(rd32(g_mem, fixture + 0x20));
}

/* ---- The under-21 squad keeps out the senior squad (rule O304, season games) ----
 *
 * NationalTeams::select_squad 0x76c120, thiscall (Nation *nation, slot, Date *, Fixture *, forced), ret 0x14, picks
 * the squad of one national side of the nation (slot 0 senior, 1 B, 2 under-21; get_squad_limits 0x76d430) into
 * its squad slots +0xd7. The under-21 pick does not avoid the senior squad, and the senior and under-21 sides
 * often play in the same window: then the team pick drops the players of both squads for international duty
 * (get_international_duty_club 0x7757d0), and the under-21 side fields too few. In a 2023 start (October 2023
 * database, the July 2024 save) Holland U21 had 15 squad slots and 10 players it could field, 32 of its eligible
 * players in another national squad; Sweden U21 7 and 25. A log of the picks showed the cause: the original
 * pick gives many under-21 squads few players (same save: Uruguay 11, Mali 9, Senegal 6, Morocco 5, several 0),
 * and the top-up 0x90dae0 gives a nation with +0x88 above 500 no virtual players (0x90dd5d), so the strong
 * nations reach the squad check short (Belgium 11, Denmark 12, Republic of Ireland 13 squad slots). After the original picks
 * either squad, the under-21 squad of the nation loses every player of the senior squad and is made up to
 * U21_SQUAD at once (u21_without_seniors), so it never reaches its match short.
 *
 * Players with a second nationality (staff +0x1e) can also sit in the squad of another nation: the senior pick of
 * that nation does not look at France's under-21 squad. Gate (a) run 8, 2020 start (November 2020): on day 241 of
 * 2020 France U21 had 27 squad slots and 14 players it could field; 12 of the others were in the senior squads of
 * Cameroon, Morocco, Burkina Faso, Comoros, Togo, Egypt, The Congo and Algeria (the fill net of fill_side acted). So
 * the under-21 squad also loses the players of any other national squad, and it is made up until U21_SQUAD of its
 * players can play (not injured), as fill_side counts them. */

static uint32_t side_of(uint32_t nation, int u21)
{
    int clubs = (int)rd32(g_mem, CLUB_COUNT), nations = (int)rd32(g_mem, NATION_COUNT);
    uint32_t base = rd32(g_mem, CLUBS);
    int from = u21 ? clubs - nations : clubs - 2 * nations, to = u21 ? clubs : clubs - nations;
    for (int i = from; i >= 0 && i < to; i++) {
        uint32_t c = base + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, c + CLUB_NATION) == nation) return c;
    }
    return 0;
}

static int in_squad(uint32_t club, uint32_t sp)
{
    for (uint32_t j = 0; j < SQUAD; j++)
        if (rd32(g_mem, club + CLUB_SQUAD + 4 * j) == sp) return 1;
    return 0;
}

/* The rule O304 acts in a season game: a start after 2001/02, or a 2001 start with changing rules on from the
 * 2002/03 season (the date's year, less one before 1 July: in 2001/02 the rules of that game are the original's,
 * so its first season plays as the base game). */
static int u21_rule_on(void)
{
    if (!season_code_on()) return 0;
    if (season_start_year() != SEASON_FIRST_YEAR) return 1;
    if (!season_changing_rules()) return 0;
    int day = (int16_t)rd16(g_mem, GAME_YEAR - 2u), year = (int16_t)rd16(g_mem, GAME_YEAR);
    return (day >= 181 ? year : year - 1) > SEASON_FIRST_YEAR;   /* day 181 is 1 July */
}

#define U21_SQUAD 22                /* the second limit of get_squad_limits 0x76d430 for slot 2 (0x76d4e2) */

/* The under-21 squad of the nation: the players of the senior squad leave it, and it takes players up to
 * U21_SQUAD: players of the nation (first or second nationality) born in the year 21 before the game year or
 * later, by current ability, not injured and in no other national squad. */
static void u21_without_seniors(uint32_t nation)
{
    uint32_t senior = side_of(nation, 0), u21 = side_of(nation, 1);
    if (!readable(senior) || !readable(u21)) return;
    uint32_t *busy = NULL;
    int nbusy = called_up(u21, &busy), in = 0, born_from = (int)rd16(g_mem, GAME_YEAR) - 21;
    int out = 0, out_other = 0, have = 0;
    for (uint32_t j = 0; j < SQUAD && !rt_unwinding; j++) {
        uint32_t sp = rd32(g_mem, u21 + CLUB_SQUAD + 4 * j);
        if (!sp) continue;
        if (in_squad(senior, sp) || bsearch(&sp, busy, (size_t)nbusy, sizeof *busy, by_address)) {
            wr32(g_mem, u21 + CLUB_SQUAD + 4 * j, 0);
            if (in_squad(senior, sp)) out++;
            else out_other++;
        } else if (!injured(sp))
            have++;
    }
    if (have >= U21_SQUAD || rt_unwinding) {
        free(busy);
        if (out || out_other)
            fprintf(stderr, "recomp: national fill: %.50s under-21 squad: %d players of the senior squad and %d of "
                            "other national squads leave (%d can play)\n", (char *)(g_mem + u21 + 4), out, out_other,
                    have);
        return;
    }
    uint32_t staff = rd32(g_mem, STAFF), count = rd32(g_mem, STAFF_COUNT);
    while (have + in < U21_SQUAD && !rt_unwinding) {
        uint32_t best = 0;
        int best_ca = -1;
        for (uint32_t i = 0; readable(staff) && i < count; i++) {
            uint32_t sp = staff + i * STAFF_SIZE, pr = rd32(g_mem, sp + 0x61);
            if (!readable(pr) || (rd32(g_mem, sp + 0x1a) != nation && rd32(g_mem, sp + 0x1e) != nation)) continue;
            if ((int16_t)rd16(g_mem, sp + 0x12) < born_from) continue;
            int ca = (int16_t)rd16(g_mem, pr + 5);
            if (ca <= best_ca || in_squad(u21, sp)) continue;
            if (bsearch(&sp, busy, (size_t)nbusy, sizeof *busy, by_address) || injured(sp)) continue;
            best = sp, best_ca = ca;
        }
        if (!best) break;
        uint32_t j = 0;
        while (j < SQUAD && rd32(g_mem, u21 + CLUB_SQUAD + 4 * j)) j++;
        if (j == SQUAD) break;
        wr32(g_mem, u21 + CLUB_SQUAD + 4 * j, best);
        in++;
    }
    free(busy);
    if (out || out_other || in)
        fprintf(stderr, "recomp: national fill: %.50s under-21 squad: %d players of the senior squad and %d of other "
                        "national squads leave, %d others join (now %d can play)\n", (char *)(g_mem + u21 + 4), out,
                out_other, in, have + in);
}

/* Called by the wrap of NationalTeams::select_squad 0x76c120 (season_intl_euro.c) after the original. */
void season_national_after_pick(uint32_t nation, int slot)
{
    if (rt_unwinding || (slot != 0 && slot != 2) || !readable(nation) || !u21_rule_on()) return;
    Cpu saved = cpu;
    u21_without_seniors(nation);
    if (!rt_unwinding) cpu = saved;
}

/* NationalTeams::select_squads_for_fixture 0x76bab0, thiscall (Fixture *), ret 4: a squad still picked for a
 * window that ends at most 14 days before this fixture stays and its window grows (0x76bb9e..0x76bc09), with no
 * call of select_squad, so an under-21 squad picked short (or before a load) would play short. After the
 * original, each national side of the fixture gets the same treatment as after a pick (u21_without_seniors). */
void o_0076bab0(void);

RT_WRAP(0076bab0, u21_squads_for_fixture)
{
    uint32_t fixture = rt_arg(0);
    o_0076bab0();
    if (rt_unwinding || !readable(fixture) || !u21_rule_on()) return;
    Cpu saved = cpu;
    int clubs = (int)rd32(g_mem, CLUB_COUNT), nations = (int)rd32(g_mem, NATION_COUNT);
    for (uint32_t k = 0; k < 2 && !rt_unwinding; k++) {
        uint32_t club = rd32(g_mem, fixture + 0x1c + 4 * k);
        int id = readable(club) ? (int)rd32(g_mem, club) : -1;
        if (id >= clubs - 2 * nations && id < clubs) u21_without_seniors(rd32(g_mem, club + CLUB_NATION));
    }
    if (!rt_unwinding) cpu = saved;
}
