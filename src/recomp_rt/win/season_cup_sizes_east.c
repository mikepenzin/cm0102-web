/* The cups of eastern Europe and Asia follow the divisions (0.7.2 league sizes, slice 6, D14; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md). The counts are in season_cup_sizes_east.h, the instruction rows
 * in tools/recomp/cup_sizes_east.py.
 *
 * Each cup keeps its entrant count and its round table. Every club of the cup's divisions plays, and the lower
 * clubs take the places that are left (lead decision of slice 6). A place that stays empty in a list of 6 bytes a
 * place (+0xa7) gets a club of the nation before the draw (season_cup_entrants.c), so a list that is short moves its
 * clubs to the last places: the clubs of the top division keep the places of the later rounds.
 *   GreCup::collect_entrants 0x5e9a80: 44 pointers at +0xee (+0xf2 = 0x2c at 0x5e9a90). Up to 100 clubs of the
 *     lower division [0x9cf874] (0x5e9b2d..0x5e9b5a), 16 of them by 0x4afbd0 (push 0x10 at 0x5e9b60, box
 *     comp_util.cpp when the pool is smaller) and a copy of 16 (0x5e9b75..0x5e9b89), then [0x9cf870] and [0x9cf86c]
 *     up to 44 (0x5e9bb0, 0x5e9bf6), box gre_cup.cpp 802 when the list is not 44 (0x5e9c24). Here the picks are 44
 *     less both divisions (rt_gre_cup_lower), and when the pool is smaller, Greek clubs of no division of the cup
 *     join the lower division for the call, by reputation.
 *   CroCup::collect_entrants 0x517bb0: 48 places (0x517bc5). Every club of [0x9cf8a8] and [0x9cf8ac] from place 0
 *     (0x517c71..0x517cbc), then every club of [0x9cf8a4] (0x517d08..0x517d19), with no bound. The round table
 *     0x517740 plays places 0 to 31 in round 1 and adds places 32 to 47 in round 2. Here a list of more than 48
 *     clubs leaves out the second-tier clubs of the lowest reputation, and the clubs move to the last places, so
 *     the First Division clubs enter in round 2.
 *   RusCup::select_entrants 0x7e99a0: 48 places. The First Division [0x9cf92c] from place 31 down (0x7e9bc2), 14
 *     lower picks (push 0xe at 0x7e9b74, copy to 0x7e9c73) while places are left below 32 (0x7e9c30), then the
 *     Premier League [0x9cf930] from the place after the last (0x7e9c7f..0x7e9cb9), 1 when the list holds 48
 *     (0x7e9cf2). Here the picks are 32 less the First Division (rt_rus_cup_lower) and the Premier League starts
 *     at place 32 (rt_rus_cup_premier_place).
 *   TurCup::collect_entrants 0x8fcb90: 64 places. 26 of up to 30 clubs of [0x9cf8e0] (copy from [esp + 0x7c] down,
 *     0x8fcd30..0x8fcd60), up to 20 of [0x9cf8dc] from place 26 (0x8fcd71), every club of [0x9cf8d8] from place 63
 *     down (0x8fccbb), with no bound. Here the lower picks are 64 less the other two (rt_tur_cup_lower).
 *   PolLgeCup::collect_entrants 0x7cad30: 36 places in 2001, else 34 (0x7cad47, 0x7cad4f). Second-tier clubs
 *     [0x9cf85c] up to the count (0x7cae01, 0x7cae84), then the Ekstraklasa [0x9cf858] up to the count, 1 when the
 *     list is full (sete at 0x7cafd0). Here the second tier stops at the count less the Ekstraklasa.
 *   JapJCup::collect_entrants 0x665a00: 28 places (0x665a14). J2 [0x9cf768] up to the count (0x665aba, 0x665ae5),
 *     then J1 [0x9cf65c], 1 when full (sete at 0x665bad). Here J2 stops at the count less J1.
 *   JapEmpCup::collect_entrants 0x6624f0: 80 places. 47 lower picks of [0x9cf678] (pool check 0x66268b, push 0x2f
 *     at 0x6626b9, copy 0x662711), 2 of [0x9cf674] at place 47 (0x662740), 3 of [0x9cf670] at place 49 (0x662778),
 *     then J2 from place 52 (0x662789, 0x6627cc) and J1, 1 when full (sete at 0x6628c4). Here the lower picks are
 *     75 less J2 and J1 (rt_jap_emp_lower), and the places after them move with them.
 *   KorLeagueCup::select_teams 0x66cf20: 10 pointers at +0xee (push 0xa at 0x66cf2a, +0xf2 at 0x66cf30) take every
 *     club of [0x9cfa00] with no bound (0x66cfdf). Its two groups of 5 (create_group_a_table 0x66c770,
 *     create_group_b_stage 0x66cb60), the 10 group rounds (create_schedule 0x66c240) and the knockout of 4 stay.
 *     A K League of more than 10 leaves out its best clubs by last season's place, as the real cups of 2009 to
 *     2011 played their groups without the AFC Champions League clubs (lead decision of slice 6).
 *
 * The test seam RECOMP_CUP_SIZE_TEST ("gre_prm:+2,jap_j2:-3", season_cup_sizes_east.h) changes the clubs that a
 * cup sees in a division while it collects: +n clubs of the nation from no division of the cup, by reputation,
 * -n of its clubs. They are back in their own divisions after the call. With RECOMP_SIZE_LOG=1 each cup logs its
 * places and the last winner in the history.
 *
 * The base game and RECOMP_LEAGUE_SIZES=off keep every cup as the original (Q152). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_names.h"
#include "season_cup_sizes_east.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define CLUB_LAST_DIVISION 0x5bu
#define CLUB_LAST_PLACE 0x5fu
#define CLUB_REPUTATION 0x80u
#define RECORDS 0xae23d0u               /* club competition records, 0x6b bytes each (RusCup 0x7e99ab..0x7e9a6c) */
#define RECORD_SIZE 0x6bu
#define COMP_RECORD 4u
#define COMP_YEAR 0x40u
#define CUP_TEAMS 0x3eu
#define CUP_LIST 0xa7u                  /* 6 bytes a place, the club at +0 */
#define ENTRY 6u
#define POINTERS 0xeeu                  /* GreCup and KorLeagueCup: pointers */
#define POINTER_COUNT 0xf2u
#define HISTORY 0xae23d4u               /* club_comp_history rows, 0x1a bytes: +4 record, +8 year, +0xa winner */
#define HISTORY_COUNT 0xae237cu
#define HISTORY_SIZE 0x1au

#define GRE_PRM 0x9cf86cu
#define GRE_SECOND 0x9cf870u
#define GRE_LOWER 0x9cf874u
#define CRO_A1 0x9cf8a4u
#define CRO_A2A 0x9cf8a8u
#define CRO_A2B 0x9cf8acu
#define RUS_FIRST 0x9cf92cu
#define RUS_PRM 0x9cf930u
#define TUR_FIRST 0x9cf8d8u             /* the Super Lig */
#define TUR_SECOND 0x9cf8dcu
#define TUR_LOWER 0x9cf8e0u
#define POL_FIRST 0x9cf858u             /* the Ekstraklasa */
#define POL_SECOND 0x9cf85cu
#define JAP_J1 0x9cf65cu
#define JAP_J2 0x9cf768u
#define KOR_LEAGUE 0x9cfa00u

static int readable(uint32_t p) { return rt_readable(p); }

/* A season game with the sizes on, as sizes_on of season_sizes.c (RECOMP_LEAGUE_SIZES=off keeps the original's
 * counts). */
static int sizes_on(void)
{
    static int off = -1;
    if (off < 0) off = getenv("RECOMP_LEAGUE_SIZES") && !strcmp(getenv("RECOMP_LEAGUE_SIZES"), "off");
    return !off && season_game_changes();
}

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SIZE_LOG") && *getenv("RECOMP_SIZE_LOG");
    return on;
}

static uint32_t club_at(int i) { return rd32(g_mem, CLUBS) + (uint32_t)i * CLUB_SIZE; }
static int club_count(void) { return (int)rd32(g_mem, CLUB_COUNT); }

/* The record id of a club's division, -1 for none. */
static int32_t division_id(uint32_t club)
{
    uint32_t d = rd32(g_mem, club + CLUB_DIVISION);
    return readable(d) ? (int32_t)rd32(g_mem, d) : -1;
}

static int32_t nation_of(uint32_t club)
{
    uint32_t n = rd32(g_mem, club + CLUB_NATION);
    return readable(n) ? (int32_t)rd32(g_mem, n) : -1;
}

/* The clubs of the division of a key id global. */
static int clubs_of_key(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    int n = 0;
    if (id < 0 || !readable(rd32(g_mem, CLUBS))) return 0;
    for (int i = 0; i < club_count(); i++) n += division_id(club_at(i)) == id;
    return n;
}

/* ---- The test seam, and the clubs that join or leave a division for one call ---- */

#define MAX_MOVED 128
static struct { uint32_t club, division; } moved[MAX_MOVED];
static int nmoved;

static void move_club(uint32_t club, uint32_t division)
{
    if (nmoved >= MAX_MOVED) return;
    moved[nmoved].club = club;
    moved[nmoved++].division = rd32(g_mem, club + CLUB_DIVISION);
    wr32(g_mem, club + CLUB_DIVISION, division);
}

static void restore_moved(void)
{
    while (nmoved > 0) {
        nmoved--;
        wr32(g_mem, moved[nmoved].club + CLUB_DIVISION, moved[nmoved].division);
    }
}

static int in_keys(int32_t id, const uint32_t *keys, int nkeys)
{
    for (int k = 0; k < nkeys; k++)
        if (keys[k] && (int32_t)rd32(g_mem, keys[k]) == id) return 1;
    return 0;
}

/* The nation of a division: that of its first club, -1 when it has none. */
static int32_t nation_of_key(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    for (int i = 0; id >= 0 && i < club_count(); i++)
        if (division_id(club_at(i)) == id) return nation_of(club_at(i));
    return -1;
}

/* n clubs of the nation whose division is none of the cup's keys join the division of `key`, the best reputation
 * first. Returns the clubs that joined. */
static int join(uint32_t key, int32_t nation, int n, const uint32_t *keys, int nkeys)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t records = rd32(g_mem, RECORDS);
    if (id < 0 || nation < 0 || !readable(records)) return 0;
    uint32_t record = records + (uint32_t)id * RECORD_SIZE;
    int done = 0;
    for (; done < n && nmoved < MAX_MOVED; done++) {
        uint32_t best = 0;
        for (int i = 0; i < club_count(); i++) {
            uint32_t c = club_at(i);
            int32_t d = division_id(c);
            if (d < 0 || nation_of(c) != nation || in_keys(d, keys, nkeys)) continue;
            if (!best || (int16_t)rd16(g_mem, c + CLUB_REPUTATION) > (int16_t)rd16(g_mem, best + CLUB_REPUTATION))
                best = c;
        }
        if (!best) break;
        move_club(best, record);
    }
    return done;
}

/* The last n clubs of the division of `key` leave it (no division) for the call. */
static void leave(uint32_t key, int n)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    for (int i = club_count() - 1; i >= 0 && n > 0 && nmoved < MAX_MOVED; i--)
        if (division_id(club_at(i)) == id) {
            move_club(club_at(i), 0);
            n--;
        }
}

typedef struct { const char *file; uint32_t key; } CupDivision;

/* The seam's changes for the divisions of a cup. keys: every key the cup reads (no club joins from those). */
static void seam(const CupDivision *divs, int ndivs, const uint32_t *keys, int nkeys)
{
    const char *spec = getenv("RECOMP_CUP_SIZE_TEST");
    if (!spec || !*spec) return;
    for (int k = 0; k < ndivs; k++) {
        int delta = east_seam_delta(spec, divs[k].file);
        if (delta > 0) join(divs[k].key, nation_of_key(divs[k].key), delta, keys, nkeys);
        else if (delta < 0) leave(divs[k].key, -delta);
    }
}

/* ---- Logs ---- */

/* The club of place i of a cup's list: the pointers at +0xee (GreCup, KorLeagueCup) or the list at +0xa7. */
static uint32_t place_club(uint32_t cup, int pointers, int i)
{
    uint32_t list = rd32(g_mem, cup + (pointers ? POINTERS : CUP_LIST));
    return readable(list) ? rd32(g_mem, list + (uint32_t)i * (pointers ? 4u : ENTRY)) : 0;
}

static int place_count(uint32_t cup, int pointers)
{
    int n = pointers ? (int32_t)rd32(g_mem, cup + POINTER_COUNT) : (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    return n < 0 ? 0 : n > 512 ? 512 : n;
}

/* The latest row of a cup in the history (club_comp_history, as season_dates.c log_comp reads it): its year, and
 * the winner's name in name (or "none"). */
static int last_winner(uint32_t cup, char *name, size_t size)
{
    uint32_t rec = rd32(g_mem, cup + COMP_RECORD), rows = rd32(g_mem, HISTORY);
    uint32_t n = rd32(g_mem, HISTORY_COUNT), table = rd32(g_mem, CLUBS), winner = 0;
    int year = 0;
    for (uint32_t i = 0; readable(rows) && i < n && i < 1000000u; i++) {
        uint32_t r = rows + i * HISTORY_SIZE;
        if (rd32(g_mem, r + 4) != rec || (int16_t)rd16(g_mem, r + 8) < year) continue;
        year = (int16_t)rd16(g_mem, r + 8);
        winner = rd32(g_mem, r + 0xa);
    }
    int known = readable(table) && winner >= table && winner < table + (uint32_t)club_count() * CLUB_SIZE;
    snprintf(name, size, "%.50s", known ? win_str(winner + 4) : "none");
    return year;
}

/* "recomp: cup sizes: <file> <year>: <clubs> of <places> places (<division> <in the cup>/<clubs>, ...)<more>; last
 * winner <club> (history <year>)". A collect starts an edition, so the history holds the winner of the edition
 * before: tests/seasons/cup_sizes_run.py reads that each edition of the run ends with a winner. */
static void log_counts(uint32_t cup, int pointers, const CupDivision *divs, int ndivs, const char *more)
{
    if (!logging()) return;
    int places = place_count(cup, pointers), filled = 0;
    for (int i = 0; i < places; i++) filled += readable(place_club(cup, pointers, i));
    char text[256];
    int at = 0;
    for (int k = 0; k < ndivs && at < (int)sizeof text - 48; k++) {
        int32_t id = (int32_t)rd32(g_mem, divs[k].key);
        int in = 0;
        for (int i = 0; i < places; i++) {
            uint32_t c = place_club(cup, pointers, i);
            in += readable(c) && division_id(c) == id;
        }
        at += snprintf(text + at, sizeof text - (size_t)at, "%s%s %d/%d", k ? ", " : "", divs[k].file, in,
                       clubs_of_key(divs[k].key));
    }
    char winner[64];
    int history = last_winner(cup, winner, sizeof winner);
    fprintf(stderr, "recomp: cup sizes: %s %d: %d of %d places (%s)%s; last winner %s (history %d)\n",
            season_change_file(cup), (int16_t)rd16(g_mem, cup + COMP_YEAR), filled, places, text, more ? more : "",
            winner, history);
}

/* The places of a list (+0xa7) that hold a club. */
static int list_filled(uint32_t cup)
{
    uint32_t list = rd32(g_mem, cup + CUP_LIST);
    int count = (int16_t)rd16(g_mem, cup + CUP_TEAMS), n = 0;
    for (int i = 0; readable(list) && i < count; i++) n += readable(rd32(g_mem, list + (uint32_t)i * ENTRY));
    return n;
}

/* The clubs of a list (+0xa7) move to its last places in their order, so the empty places come first, for
 * season_cup_entrants.c. A list with a hole (a sort over a range with empty places, JapJCup 0x665b9b) closes it
 * too. Returns the clubs of the list. */
static int clubs_to_tail(uint32_t cup)
{
    uint32_t list = rd32(g_mem, cup + CUP_LIST);
    int count = (int16_t)rd16(g_mem, cup + CUP_TEAMS), used = 0;
    if (!readable(list) || count < 1 || count > 512) return 0;
    static uint8_t keep[512 * ENTRY];
    for (int i = 0; i < count; i++)
        if (readable(rd32(g_mem, list + (uint32_t)i * ENTRY)))
            memcpy(keep + (size_t)used++ * ENTRY, g_mem + list + (uint32_t)i * ENTRY, ENTRY);
    if (used == 0 || used == count) return used;
    memset(g_mem + list, 0, (size_t)(count - used) * ENTRY);
    memcpy(g_mem + list + (uint32_t)east_tail_place(count, used, 0) * ENTRY, keep, (size_t)used * ENTRY);
    return used;
}


/* The n clubs of the lowest reputation of the divisions of keys leave them for the call. */
static void leave_weakest(const uint32_t *keys, int nkeys, int n)
{
    for (; n > 0 && nmoved < MAX_MOVED; n--) {
        uint32_t worst = 0;
        for (int i = 0; i < club_count(); i++) {
            uint32_t c = club_at(i);
            int32_t d = division_id(c);
            if (d < 0 || !in_keys(d, keys, nkeys)) continue;
            if (!worst || (int16_t)rd16(g_mem, c + CLUB_REPUTATION) < (int16_t)rd16(g_mem, worst + CLUB_REPUTATION))
                worst = c;
        }
        if (!worst) return;
        move_club(worst, 0);
    }
}

/* ---- GreCup ---- */

static const CupDivision gre_divs[] = {{"gre_prm", GRE_PRM}, {"gre_second", GRE_SECOND}};
static const uint32_t gre_keys[] = {GRE_PRM, GRE_SECOND, GRE_LOWER};

/* The lower picks of GreCup::collect_entrants (push at 0x5e9b60, copy bound at 0x5e9b86): pool is bl, the lower
 * clubs found (at most 100). The original's 16 in the base game. */
uint32_t rt_gre_cup_lower(uint32_t pool)
{
    if (!sizes_on()) return 0x10u;
    return (uint32_t)east_lower_picks(44, clubs_of_key(GRE_PRM) + clubs_of_key(GRE_SECOND), (int8_t)pool, 1);
}

void o_005e9a80(void);

RT_WRAP(005e9a80, gre_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    if (!sizes_on()) {
        o_005e9a80();
        return;
    }
    seam(gre_divs, 2, gre_keys, 3);
    /* The loop finds at most 100 lower clubs (cmp bl, 0x64 at 0x5e9b3e). When they are fewer than the places left,
     * Greek clubs of no division of the cup join the lower division for the call, so the list holds 44 and the box
     * at 0x5e9c24 does not come. */
    int need = 44 - clubs_of_key(GRE_PRM) - clubs_of_key(GRE_SECOND), pool = clubs_of_key(GRE_LOWER), joined = 0;
    if (pool > 100) pool = 100;
    /* At least one: the copy loop (0x5e9b75..0x5e9b89) copies one place before its test. */
    int want = need < 1 ? 1 : need;
    if (want > pool && want <= 100) joined = join(GRE_LOWER, nation_of_key(GRE_PRM), want - pool, gre_keys, 3);
    o_005e9a80();
    char more[96];
    snprintf(more, sizeof more, ", %d lower picks%s", want,
             joined ? ", clubs of no cup division in the pool" : "");
    log_counts(cup, 1, gre_divs, 2, more);
    restore_moved();
}

/* ---- CroCup ---- */

static const CupDivision cro_divs[] = {{"cro_a1", CRO_A1}, {"cro_a2a", CRO_A2A}, {"cro_a2b", CRO_A2B}};
static const uint32_t cro_keys[] = {CRO_A1, CRO_A2A, CRO_A2B};
static const uint32_t cro_second[] = {CRO_A2A, CRO_A2B};

void o_00517bb0(void);

RT_WRAP(00517bb0, cro_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    if (!sizes_on()) {
        o_00517bb0();
        return;
    }
    seam(cro_divs, 3, cro_keys, 3);
    int over = east_skip(clubs_of_key(CRO_A1) + clubs_of_key(CRO_A2A) + clubs_of_key(CRO_A2B), 48);
    leave_weakest(cro_second, 2, over);
    o_00517bb0();
    if ((int32_t)cpu.eax == 1) {
        clubs_to_tail(cup);
        log_counts(cup, 0, cro_divs, 3, over ? ", the clubs in the last places, second-tier clubs left out"
                                             : ", the clubs in the last places");
    }
    restore_moved();
}

/* ---- RusCup ---- */

static const CupDivision rus_divs[] = {{"rus_prm", RUS_PRM}, {"rus_first", RUS_FIRST}};
static const uint32_t rus_keys[] = {RUS_PRM, RUS_FIRST};

/* The lower picks of RusCup::select_entrants (push at 0x7e9b74, copy bound at 0x7e9c73): pool is the byte at
 * [esp + 0x13], the lower clubs found. The original's 14 in the base game. */
uint32_t rt_rus_cup_lower(uint32_t pool)
{
    if (!sizes_on()) return 0xeu;
    return (uint32_t)east_lower_picks(32, clubs_of_key(RUS_FIRST), (int8_t)pool, 1);
}

/* push 0xe at 0x7e9b74: the lower picks. With an empty pool the copy loop (0x7e9c2c..0x7e9c7d) still copies one slot
 * before its test: that slot ([esp + 0x230] before the push, lea ecx, [esp + 0x234] at 0x7e9b76) is cleared, so
 * the place stays empty and season_cup_entrants.c fills it. */
uint32_t rt_rus_cup_picks(uint32_t esp)
{
    if (!sizes_on()) return 0xeu;
    if ((int8_t)rd8(g_mem, esp + 0x13u) <= 0) wr32(g_mem, esp + 0x230u, 0);
    return rt_rus_cup_lower(rd8(g_mem, esp + 0x13u));
}

/* The first place of the Premier League (0x7e9c7f): the places taken so far, at least 32 in a season game, so the
 * Premier League enters in round 2 when the First Division and the lower picks leave a place of round 1 empty. */
uint32_t rt_rus_cup_premier_place(uint32_t placed)
{
    int p = (int8_t)placed;
    return sizes_on() && p < 32 ? 32u : (placed & 0xffu);
}

void o_007e99a0(void);

RT_WRAP(007e99a0, rus_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    if (!sizes_on()) {
        o_007e99a0();
        return;
    }
    seam(rus_divs, 2, rus_keys, 2);
    o_007e99a0();
    log_counts(cup, 0, rus_divs, 2, NULL);
    restore_moved();
}

/* ---- TurCup ---- */

static const CupDivision tur_divs[] = {{"tur_first", TUR_FIRST}, {"tur_second", TUR_SECOND}};
static const uint32_t tur_keys[] = {TUR_FIRST, TUR_SECOND, TUR_LOWER};

/* The lower picks of TurCup::collect_entrants: the word at [esp + 0x10] holds the clubs of the lower pool (up to
 * 30), the word at [esp + 0x14] the second-tier clubs (up to 20), the Super Lig fills from place 63 down. */
static int tur_lower(uint32_t esp)
{
    int pool = (int16_t)rd16(g_mem, esp + 0x10u), second = (int16_t)rd16(g_mem, esp + 0x14u);
    int k = east_lower_picks(64, clubs_of_key(TUR_FIRST) + second, pool, 1);
    return k < 1 ? 1 : k;
}

/* lea ecx, [esp + 0x7c] at 0x8fcd30: the last lower pick to copy (26 at [esp + 0x18] in the original). With an
 * empty pool the one copy takes a cleared slot: an empty place. */
uint32_t rt_tur_cup_copy_from(uint32_t esp)
{
    if (!sizes_on()) return esp + 0x7cu;
    if ((int16_t)rd16(g_mem, esp + 0x10u) <= 0) wr32(g_mem, esp + 0x18u, 0);
    return esp + 0x18u + 4u * (uint32_t)(tur_lower(esp) - 1);
}

/* mov edx, 0x1a at 0x8fcd34: the lower picks. */
uint32_t rt_tur_cup_lower(uint32_t esp)
{
    return sizes_on() ? (uint32_t)tur_lower(esp) : 0x1au;
}

/* mov eax, 0x9c at 0x8fcd71: the list offset of the first second-tier club, after the lower picks. */
uint32_t rt_tur_cup_second_place(uint32_t esp)
{
    return sizes_on() ? (uint32_t)tur_lower(esp) * ENTRY : 0x9cu;
}

void o_008fcb90(void);

RT_WRAP(008fcb90, tur_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    if (!sizes_on()) {
        o_008fcb90();
        return;
    }
    seam(tur_divs, 2, tur_keys, 3);
    o_008fcb90();
    log_counts(cup, 0, tur_divs, 2, NULL);
    restore_moved();
}

/* ---- PolLgeCup and JapJCup: a division up to the places that the division after it leaves ---- */

static const CupDivision pol_divs[] = {{"pol_first", POL_FIRST}, {"pol_second", POL_SECOND}};
static const uint32_t pol_keys[] = {POL_FIRST, POL_SECOND};
static const CupDivision jcup_divs[] = {{"jap_j1", JAP_J1}, {"jap_j2", JAP_J2}};
/* Both Japanese cups: no club joins a division from J1, J2 or JapEmpCup's three lower pools. */
static const uint32_t jemp_keys[] = {JAP_J1, JAP_J2, 0x9cf678u, 0x9cf674u, 0x9cf670u};

/* movsx edx, word ptr [esi + 0x3e] at 0x7cae01 and 0x7cae84: the limit of the second-tier loops. */
uint32_t rt_pol_lge_second(uint32_t cup)
{
    int count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    return (uint32_t)(sizes_on() ? east_bound(count, clubs_of_key(POL_FIRST)) : count);
}

/* movsx edx, word ptr [esi + 0x3e] at 0x665aba and 0x665ae5: the limit of the J2 loop. */
uint32_t rt_jap_j_cup_j2(uint32_t cup)
{
    int count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    return (uint32_t)(sizes_on() ? east_bound(count, clubs_of_key(JAP_J1)) : count);
}

/* After a collect that returns 1 only for a full list (sete): a short list moves its clubs to the last places and
 * counts as made, as the empty places get clubs before the draw (season_cup_entrants.c). */
static void short_list(uint32_t cup, const CupDivision *divs, int ndivs, const char *more)
{
    int used = list_filled(cup), count = (int16_t)rd16(g_mem, cup + CUP_TEAMS);
    if (cpu.eax == 0 && used > 0 && used < count) {
        clubs_to_tail(cup);
        cpu.eax = 1;
    }
    log_counts(cup, 0, divs, ndivs, used < count ? ", the clubs in the last places" : more);
}

void o_007cad30(void);

RT_WRAP(007cad30, pol_lge_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    if (!sizes_on()) {
        o_007cad30();
        return;
    }
    seam(pol_divs, 2, pol_keys, 2);
    o_007cad30();
    short_list(cup, pol_divs, 2, NULL);
    restore_moved();
}

void o_00665a00(void);

RT_WRAP(00665a00, jap_j_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    if (!sizes_on()) {
        o_00665a00();
        return;
    }
    seam(jcup_divs, 2, jemp_keys, 5);
    /* The original sorts places 11 to 26 (0x665b88..0x665b9b), so a short list would sort empty places: Japanese
     * clubs of no cup division join J2 for the call until J2 and J1 fill the 28 places. */
    int count = 28, short_by = count - clubs_of_key(JAP_J1) - clubs_of_key(JAP_J2);
    int joined = short_by > 0 ? join(JAP_J2, nation_of_key(JAP_J1), short_by, jemp_keys, 5) : 0;
    o_00665a00();
    /* cup_sizes_run.py reads the count of this text. */
    char more[64] = "";
    if (joined) snprintf(more, sizeof more, ", %d clubs of no cup division join jap_j2", joined);
    short_list(cup, jcup_divs, 2, more);
    restore_moved();
}

/* ---- JapEmpCup ---- */

static int jemp_lower = 0x2f;

/* cmp bl, 0x2f at 0x66268b: the lower picks, 75 less J2 and J1 (47 in the original), at most the pool (bl). The
 * later sites read the value kept here, because bl changes in the copy loop (0x66271c). */
uint32_t rt_jap_emp_lower(uint32_t pool)
{
    if (!sizes_on()) return 0x2fu;
    int k = east_lower_picks(75, clubs_of_key(JAP_J2) + clubs_of_key(JAP_J1), (int8_t)pool, 1);
    jemp_lower = k < 1 ? 1 : k;      /* an empty pool fails the check, as in the original */
    return (uint32_t)jemp_lower;
}

/* The constants after the check (0x6626b9, 0x662711, 0x662740, 0x662778, 0x662789, 0x6627cc): the lower picks
 * times mult plus add. */
uint32_t rt_jap_emp_place(uint32_t original, uint32_t mult, uint32_t add)
{
    return sizes_on() ? (uint32_t)jemp_lower * mult + add : original;
}

void o_006624f0(void);

RT_WRAP(006624f0, jap_emp_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    if (!sizes_on()) {
        o_006624f0();
        return;
    }
    seam(jcup_divs, 2, jemp_keys, 5);
    o_006624f0();
    short_list(cup, jcup_divs, 2, NULL);
    restore_moved();
}

/* ---- KorLeagueCup ---- */

static const CupDivision kor_divs[] = {{"kor_league", KOR_LEAGUE}};
static const uint32_t kor_keys[] = {KOR_LEAGUE};

/* The n best clubs of the K League by last season's place leave it for the call: a club whose last division
 * (+0x5b) is the K League by its place (+0x5f), the others after. */
static void leave_best(int n)
{
    int32_t id = (int32_t)rd32(g_mem, KOR_LEAGUE);
    for (; n > 0 && nmoved < MAX_MOVED; n--) {
        uint32_t best = 0;
        int best_rank = 0;
        for (int i = 0; i < club_count(); i++) {
            uint32_t c = club_at(i), last = rd32(g_mem, c + CLUB_LAST_DIVISION);
            if (division_id(c) != id) continue;
            int rank = readable(last) && (int32_t)rd32(g_mem, last) == id ? (int8_t)g_mem[c + CLUB_LAST_PLACE] : 1000;
            if (!best || rank < best_rank) {
                best = c;
                best_rank = rank;
            }
        }
        if (!best) return;
        move_club(best, 0);
    }
}

void o_0066cf20(void);

RT_WRAP(0066cf20, kor_league_cup_teams)
{
    uint32_t cup = cpu.ecx;
    if (!sizes_on()) {
        o_0066cf20();
        return;
    }
    seam(kor_divs, 1, kor_keys, 1);
    int skip = east_skip(clubs_of_key(KOR_LEAGUE), 10);
    leave_best(skip);
    o_0066cf20();
    char more[64] = "";
    /* cup_sizes_run.py reads the count of this text. */
    if (skip) snprintf(more, sizeof more, ", the %d best by last place left out", skip);
    log_counts(cup, 1, kor_divs, 1, more);
    restore_moved();
}
