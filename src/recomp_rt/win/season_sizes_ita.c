/* Italy at its real league sizes (0.7.2 league sizes, slice 9; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md,
 * D22). Serie A plays 20 from 2004/05, Serie B 24, 22 and 19, and the Serie C1 groups 19, 17 and 16 (the tables and
 * the date templates of the size engine: league_sizes.py GROWN). The code of the classes that reads a fixed count
 * takes the count of the table here (tools/recomp/league_sizes_ita.py names each site):
 *
 * - The tie-break zones of Serie A and Serie B. ItaSerA::update_for_date 0x642880 and
 *   ItaSerA::create_tiebreak_playoffs 0x641a70 build a stack array of 18 zone bytes (1 the title, 2 and 3 the European
 *   places of 0x905c60, 4 the word 0x404 after them, 5 the relegation places +0xc1 counting down from entry 17 at
 *   0x64299f and 0x641b6d) and start a tie-break when rows i and i + 1 have equal points and different zones
 *   (0x6429e3, 0x6429e7; 0x641bba, 0x641bbe). ItaSerB::update_for_date 0x647050 and
 *   ItaSerB::create_tiebreak_playoff_stage 0x646bb0 build 20 fixed entries (1, then 2 for entries 1 to 3, 3 for
 *   entries 16 to 19: 0x647057..0x647077, 0x646bd0..0x646bee) and compare at 0x647123, 0x647127 and 0x646c6d,
 *   0x646c71. A table of 20 in Serie A read 2 unwritten bytes and checked the relegation line at 15th; a Serie B of
 *   22 or 24 read the return address of 0x647050. Here each read gives the zone of the row for the count. Serie B
 *   with the sizes on takes the zones of the season's places to 2004/05 and none from 2005/06, when the head-to-head
 *   table replaced the tie-break match (season_ita_rules.h, gap row 30).
 * - The play-out rows of the C1 groups. ItaSerC1a::create_playoff_stages 0x64ba70 and
 *   ItaSerC1b::create_play_off_stages 0x64fc30 put the rows 16, 13, 15 and 14 (17th, 14th, 16th, 15th: +0x3b0,
 *   +0x2ff, +0x375, +0x33a at 0x64bc2c..0x64bc58 and 0x64fdec..0x64fe18, seeds 0x10, 0xd, 0xf, 0xe at
 *   0x64bc61..0x64bc70 and 0x64fe21..0x64fe30) into the play-out. Here they are the four rows above the last row of
 *   the count (with 20: 16th to 19th, as the Serie C play-out of 2014/15 on). set_club_progress 0x64bd80 and
 *   0x64ff40 give the clubs of those places the range 14th to 17th (0x64bdbe, 0x64bded, 0x64be29, 0x64be2d).
 * - The C1 regroup. ita_ser_a_regroup_serie_c1_clubs 0x642630 collects the clubs of C1/A and C1/B into a stack array
 *   of 36 (cmp dl, 0x24 at 0x64269c), sorts them by latitude (0x945333 with 0x53fd40 at 0x6426c2), gives the first 18
 *   to C1/B (cmp ax, 0x12 at 0x6426d4) and writes 36 (cmp ax, 0x24 at 0x64275f). With 33 clubs it wrote through 3
 *   slots that held no club. Here the array is a block of 64 and the loop writes the clubs it collected, the first
 *   ones to C1/B up to the count of its next row.
 * - The C2 exchange. ItaSerA::exchange_serie_c2_clubs_unplayed 0x641fc0 reads the rows of C1/A and C1/B to C1/A's
 *   count (movsx edi, word ptr [ecx + 0x3e] at 0x64210c; status at 0x642122 and 0x642161; League::get_table_row
 *   0x6826d0 has no bound). Here it reads each table to its own count.
 * - The fixed fixture lists. ItaSerA::create_fixed_fixtures 0x642a70, ItaSerB::set_up_fixed_fixtures 0x647230 and
 *   set_up_fixed_fixtures of C1/A 0x64be90 and C1/B 0x650100 allocate (clubs / 2) x match days entries of 16 bytes
 *   into +0xea (0x642a90..0x642aae, 0x647250..0x64726e, 0x64beb9, 0x650120..0x650130) and write the real 2001/02
 *   fixtures by club name (fixed_fixture_set_by_club_names 0x68aff0): 306, 380, 306 and 306 entries at fixed offsets
 *   (the last at +0x1310, +0x17b0). A Serie B of 19 (9 x 38 = 342 entries) wrote 38 entries past its block: a 2018
 *   game overwrote the heap header of the club records pool and stopped at file_llist.CPP 101. create_fixtures uses
 *   the list only when the year of its first date is the league's (0x68382e..0x68383d), a 2001 game at the real
 *   counts. Here a block too small for the list makes none (+0xea 0, as the builders leave it when a club is missing),
 *   as season_divisions_ita.c does for C2/B and C2/C.
 * - The Coppa Italia seeding pots. compare_clubs_by_seeding_pot 0x640590 ranks Serie B's places 3rd to 6th, 7th to
 *   14th and 15th to 18th, the 3rd-last of 20 (0x6405fe, 0x64068e). Here the last range ends at the 3rd-last of the
 *   last Serie B's count (gap row 32).
 *
 * The base game and a table at the class's count (18 in Serie A and C1, 20 in Serie B) keep the original's bytes
 * and paths (rule Q152). The regroup and the C2 exchange take the new code only in a season game with the sizes on. */
#include "win.h"
#include "../rt_host.h"
#include "season.h"
#include "season_sizes.h"
#include "season_sizes_ita.h"
#include "season_ita_rules.h"
#include "season_divisions_ita.h"

#include <stdio.h>
#include <string.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u
#define COMP_RECORDS 0xae23d0u          /* club_comp records, 0x6b bytes each (as 0x642659..0x642672) */
#define COMP_RECORD_SIZE 0x6bu
#define COMP_OBJECTS 0xadadfcu
#define KEY_A 0x9cf570u
#define KEY_C1A 0x9cf578u
#define KEY_C1B 0x9cf57cu
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u
#define CLUBS_AT 0x3eu
#define UP 0xbeu                        /* promotion places (League::has_automatic_promotion 0x684790) */
#define DOWN 0xc1u                      /* relegation places (ItaSerA +0xc1 at 0x6416a4, read at 0x642992) */
#define YEAR 0x40u                      /* s16: the season of the edition */
#define F_PROGRESS 0x004a4850u          /* thiscall (club) on [league + 8]: the club's progress record (0x64bd89) */

#define SERIE_A_CLASS 18
#define SERIE_B_CLASS 20
#define C1_CLASS 18
#define REGROUP_MAX 64

static int readable(uint32_t p) { return rt_readable(p); }

static int count_of(uint32_t obj) { return readable(obj) ? (int16_t)rd16(g_mem, obj + CLUBS_AT) : 0; }

/* ---- Tie-break zones ---- */

uint32_t rt_ita_zone(uint32_t obj, uint32_t serie_b, uint32_t row, uint32_t at)
{
    int n = count_of(obj), cls = serie_b ? SERIE_B_CLASS : SERIE_A_CLASS, i = (int)row;
    /* With the sizes on, the zones of the season's rules (season_rule_year: the start season's with changing rules
     * off). Serie B: the zones of the season's places to 2004/05, none from 2005/06, when the head-to-head table
     * settles a tie (season_ita_rules.h; gap row 30). At 20 clubs with 4 up and 4 down (2001/02 and 2002/03) these
     * are the original's bytes. Serie A: open item 254. */
    if (season_sizes_on() && n >= 2 && i >= 0 && i < n) {
        int year = season_rule_year((int16_t)rd16(g_mem, obj + YEAR));
        if (serie_b) return (uint32_t)ita_serie_b_zone(year, n, (int8_t)g_mem[obj + UP], (int8_t)g_mem[obj + DOWN], i);
        /* Serie A with the sizes on: the zones of its era (open item 254). The original wrote bytes for its 18 rows
         * only, so a row past them has no class byte. */
        return (uint32_t)ita_serie_a_zone(year, n, (int8_t)g_mem[obj + DOWN], i, i < SERIE_A_CLASS ? g_mem[at] : 0);
    }
    if (n == cls || n < 2 || i < 0 || i >= n) return g_mem[at];
    if (!serie_b) {
        /* Serie A: the European entries of the original stay (rows 0 to 8 at most), the relegation entries count up
         * from the last row of the table. An entry 5 that the original wrote for 18 rows goes back to 0. */
        int down = (int8_t)g_mem[obj + DOWN];
        if (down > 0 && i >= n - down) return 5;
        if (i < cls) {
            uint8_t z = g_mem[at];
            return z == 5 ? 0u : z;
        }
        return 0;
    }
    /* Serie B with the sizes off (a grown table needs them on): the original's layout, its 4 last entries at the
     * last 4 rows of the table. */
    if (i == 0) return 1;
    if (i < 4) return 2;
    if (i >= n - 4) return 3;
    return 0;
}

/* ---- C1 play-out ---- */

/* The play-out of the builder that runs now (season_sizes_ita_playout_set, from the builder wraps in
 * season_divisions_ita.c), or none. */
static ItaPlayout po;
static uint32_t po_obj;

#define PLAYOFF_PLACES 0xbfu            /* the play-off clubs, also the club count of both stages (0x64bb99, 0x64bcb1) */
#define DOWN_PLAYOFF 0xc0u              /* the play-out places (League::update_positions_and_status 0x684cd0) */

void season_sizes_ita_playout_shape(uint32_t obj, ItaPlayout *p)
{
    ita_c1_playout(season_rule_year((int16_t)rd16(g_mem, obj + YEAR)), count_of(obj), p);
}

void season_sizes_ita_playout_set(uint32_t obj, const ItaPlayout *p)
{
    po_obj = p ? obj : 0;
    if (p) po = *p;
}

/* The slot (0 to 3) of a site of the builders: the row offsets +0x3b0, +0x2ff, +0x375, +0x33a and the seeds 0x10,
 * 0xd, 0xf, 0xe in the order of the play-out list (0x64bc2c..0x64bc70). */
static int slot_of(uint32_t original_row)
{
    static const int rows[4] = {16, 13, 15, 14};
    for (int k = 0; k < 4; k++)
        if ((int)original_row == rows[k]) return k;
    return -1;
}

static uint32_t playout_row(uint32_t obj, uint32_t original_row)
{
    int k = slot_of(original_row);
    if (obj != po_obj || k < 0) return original_row;
    if (k >= 2 * po.ties) return 0;     /* a slot past the stage's clubs (or no stage): not read by the stage */
    return (uint32_t)po.slot[k];
}

/* The row offset of a play-out place (the original's for 18 clubs), as the play-out of the season. */
uint32_t rt_ita_c1_row(uint32_t obj, uint32_t original)
{
    return playout_row(obj, original / ROW_SIZE) * ROW_SIZE;
}

/* The seed (a row index) of a play-out place. */
uint32_t rt_ita_c1_seed(uint32_t obj, uint32_t original) { return playout_row(obj, original); }

/* The club count of the play-out stage (0x64bcb1, 0x64fe71, 0x6540a6: the play-off count +0xbf): 2 for each tie. */
uint32_t rt_ita_playout_count(uint32_t obj, uint32_t original)
{
    return obj == po_obj ? (uint32_t)(2 * po.ties) : original;
}

/* A builder run of no tie (2013/14, or a points gap that drops both ties): the builder makes no play-out stage. The
 * stage constructor 0x522e00 fails on 0 clubs (Cup.cpp 149), so the stage's block (operator new 0xb2 at 0x64bc75,
 * kept at 0x64bc7d) reads as 0 and the builder skips the constructor (je at 0x64bc8e). The block is kept here and
 * freed by the builder wrap after the builder returns (season_sizes_ita_playout_block). */
static uint32_t po_block;

int rt_ita_playout_none(uint32_t obj) { return obj == po_obj && po.ties <= 0; }

uint32_t rt_ita_playout_block(uint32_t obj, uint32_t block)
{
    if (!rt_ita_playout_none(obj)) return block;
    po_block = block;
    return 0;
}

uint32_t season_sizes_ita_playout_block(void)
{
    uint32_t b = po_block;
    po_block = 0;
    return b;
}

/* The play-out template (create_schedule stage 1: one round of 4 clubs in 2 ties, +0x18 4, +0x1a 2, +0x1c 4 at
 * 0x64b629..0x64b635) for a play-out of one tie or none. */
void season_sizes_ita_playout_template(uint32_t obj, int stage, uint32_t t)
{
    if (stage != 1 || obj != po_obj || po.ties == 2 || !readable(t)) return;
    wr16(g_mem, t + 0x18, (uint16_t)(2 * po.ties));
    wr16(g_mem, t + 0x1a, (uint16_t)po.ties);
    wr16(g_mem, t + 0x1c, (uint16_t)(2 * po.ties));
}

/* The places of a C1 group (and of group C in the Serie C era) in the season: the play-out places +0xc0 (2 for each
 * tie; the original 4 at 0x64b7c9) and the direct places +0xc1 (the original 1 at 0x64b7cf), so that
 * League::update_positions_and_status 0x684cd0 marks the season's rows. */
void season_sizes_ita_playout_places(uint32_t obj)
{
    if (!season_sizes_on() || !readable(obj) || count_of(obj) < 2) return;
    ItaPlayout p;
    season_sizes_ita_playout_shape(obj, &p);
    int was_po = (int8_t)g_mem[obj + DOWN_PLAYOFF], was_down = (int8_t)g_mem[obj + DOWN];
    g_mem[obj + DOWN_PLAYOFF] = (uint8_t)(2 * p.ties);
    g_mem[obj + DOWN] = (uint8_t)p.direct;
    if (was_po != 2 * p.ties || was_down != p.direct)
        fprintf(stderr, "recomp: league sizes: %.50s %d: play-out of %d ties, %d down directly\n",
                win_str(rd32(g_mem, obj + 4) + 4), (int16_t)rd16(g_mem, obj + YEAR), p.ties, p.direct);
}

/* set_club_progress of C1/A 0x64bd80 and C1/B 0x64ff40 (the same code), thiscall (club, stage, place, best, worst)
 * ret 0x14: the progress record of the club (0x4a4850 on [this + 8]) gets the place +0x73, the best +0x74 and the
 * worst +0x75. In the league (stage -1) the places 2 to 5 range from 2 to 5 (the promotion play-off) and the
 * places 14 to 17 from 14 to 17 (the play-out); in the play-off (stage 0) all three go one down; in the play-out
 * (stage 1) the best is 14 for a winner (1) and 16 for a loser (2). With another count the play-out places move
 * with it. */
static void c1_progress(void (*original)(void))
{
    uint32_t self = cpu.ecx;
    ItaPlayout p;
    int gone[2], safe[2];
    if (season_sizes_on() && count_of(self) >= 6) {
        season_ita_playout_final(self, &p, gone, safe);   /* the ties played, after the points gap */
        if (rt_unwinding) return;
    }
    if (!season_sizes_on() || count_of(self) < 6 || (count_of(self) == C1_CLASS && p.ties == 2 && p.direct == 1 &&
                                                     p.slot[0] == 16 && p.slot[1] == 13)) {
        original();
        return;
    }
    uint32_t club = rt_arg(0);
    int stage = (int8_t)(rt_arg(1) & 0xff);
    int place = (int)(rt_arg(2) & 0xff), best = (int)(rt_arg(3) & 0xff), worst = (int)(rt_arg(4) & 0xff);
    Cpu saved = cpu;
    cpu.ecx = rd32(g_mem, self + 8);
    uint32_t a[1] = {club};
    uint32_t rec = rt_callback(F_PROGRESS, 1, a);
    if (rt_unwinding) return;
    cpu = saved;
    /* The play-out places of the season (1st = 1): from the highest to the lowest row of its ties. */
    int lo = 0, hi = 0;
    for (int k = 0; k < 2 * p.ties; k++) {
        int r = p.slot[k] + 1;
        if (!lo || r < lo) lo = r;
        if (r > hi) hi = r;
    }
    if (stage == -1) {
        if (best >= 2 && best <= 5) best = 2;
        else if (best >= 14 && best <= 17 && !lo) best = place;
        else if (lo && best >= lo && best <= hi) best = lo;
        if (worst >= 2 && worst <= 5) worst = 5;
        else if (worst >= 14 && worst <= 17 && !lo) worst = place;
        else if (lo && worst >= lo && worst <= hi) worst = hi;
    } else if (stage == 0) {
        place++;
        best++;
        worst++;
    } else if (stage == 1 && lo) {
        if (best == 1) best = lo;
        else if (best == 2) best = p.ties == 2 ? lo + 2 : hi;
    }
    if (readable(rec)) {
        g_mem[rec + 0x73] = (uint8_t)place;
        g_mem[rec + 0x74] = (uint8_t)best;
        g_mem[rec + 0x75] = (uint8_t)worst;
    }
    rt_ret_stdcall((rec & 0xffffff00u) | (uint32_t)(worst & 0xff), 5);   /* al: the worst (0x64be39) */
}

void o_0064bd80(void);
void o_0064ff40(void);
RT_WRAP(0064bd80, c1a_progress) { c1_progress(o_0064bd80); }
RT_WRAP(0064ff40, c1b_progress) { c1_progress(o_0064ff40); }
/* ItaSerC2a::set_club_progress 0x6544b0, the same 63 instructions as 0x64bd80: group C of the Serie C era only (the
 * C2 groups before it keep the original). */
void o_006544b0(void);
RT_WRAP(006544b0, c2a_progress)
{
    if (!season_ita_serie_c(cpu.ecx)) o_006544b0();
    else if (!rt_unwinding) c1_progress(o_006544b0);
}

/* ---- Coppa Italia seeding pots ---- */

#define KEY_B 0x9cf574u
#define KEY_SERIE_D 0x9cf58cu
#define CLUB_LAST_DIVISION 0x5bu
#define CLUB_LAST_PLACE 0x5fu

static int cup_b_count;                 /* the count of the last Serie B, 0: not known */

void season_sizes_ita_cup_pick(int season)
{
    cup_b_count = 0;
    uint32_t table = rd32(g_mem, CLUBS), b = rd32(g_mem, KEY_B);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    if (!season_sizes_on() || !readable(table) || count <= 0 || count > 100000) return;
    for (int i = 0; i < count; i++) {
        uint32_t club = table + CLUB_SIZE * (uint32_t)i, last = rd32(g_mem, club + CLUB_LAST_DIVISION);
        uint32_t now = rd32(g_mem, club + CLUB_DIVISION), key = readable(now) ? rd32(g_mem, now) : 0;
        int place = (int8_t)g_mem[club + CLUB_LAST_PLACE];
        /* Only a club in an Italian division now (Serie A to Serie D, the keys 0x9cf570 to 0x9cf58c): a club out of
         * the leagues keeps an old last division and place. */
        int italian = 0;
        for (uint32_t k = KEY_A; k <= KEY_SERIE_D && !italian; k += 4) italian = key && key == rd32(g_mem, k);
        if (!italian) continue;
        if (readable(last) && rd32(g_mem, last) == b && place > cup_b_count) cup_b_count = place;
    }
    cup_b_count = ita_cup_b_count(cup_b_count, season, season_start_year());
    if (cup_b_count > 0 && cup_b_count != SERIE_B_CLASS)
        fprintf(stderr, "recomp: league sizes: Coppa Italia %d pots: the last Serie B had %d clubs, its places 15 to "
                        "%d share a pot\n", season, cup_b_count, cup_b_count - 2);
}

static int cup_pot(uint32_t club)
{
    uint32_t a = rd32(g_mem, KEY_A), b = rd32(g_mem, KEY_B);
    uint32_t last = rd32(g_mem, club + CLUB_LAST_DIVISION), now = rd32(g_mem, club + CLUB_DIVISION);
    ItaCupClub c = {0};
    c.has_last = last != 0;
    c.last_is_b = readable(last) && rd32(g_mem, last) == b;
    c.last_is_a = readable(last) && rd32(g_mem, last) == a;
    c.cur_is_b = readable(now) && rd32(g_mem, now) == b;
    c.last_is_cur = last == now;
    c.place = (int8_t)g_mem[club + CLUB_LAST_PLACE];
    return ita_cup_pot(&c, cup_b_count);
}

/* compare_clubs_by_seeding_pot 0x640590, cdecl (Club **a, Club **b): the pot of a less the pot of b. The original
 * ends Serie B's third range at 18th, the 3rd-last of its 20 (0x6405fe, 0x64068e): with a last Serie B of 22, 24 or
 * 19 the range ends at its own 3rd-last (season_ita_rules.h, gap row 32). A last Serie B of 20 (or not known) keeps
 * the original. */
void o_00640590(void);
RT_WRAP(00640590, ita_cup_pot_compare)
{
    if (!season_sizes_on() || cup_b_count <= 0 || cup_b_count == SERIE_B_CLASS) {
        o_00640590();
        return;
    }
    uint32_t pa = rt_arg(0), pb = rt_arg(1);
    uint32_t a = readable(pa) ? rd32(g_mem, pa) : 0, b = readable(pb) ? rd32(g_mem, pb) : 0;
    if (!readable(a) || !readable(b)) {
        o_00640590();
        return;
    }
    rt_ret_cdecl((uint32_t)(cup_pot(a) - cup_pot(b)));
}

/* ---- Fixed fixture lists ---- */

#define MATCH_DAYS 0xa9u                /* s16: the match days of the template (the builders' 0x642a99, 0x647259) */
#define FIXED_LIST 0xeau                /* the fixed fixture list (0x642aea) */

static void fixed_list(void (*original)(void), int writes)
{
    uint32_t self = cpu.ecx;
    int n = count_of(self), days = readable(self) ? (int16_t)rd16(g_mem, self + MATCH_DAYS) : 0;
    int room = n / 2 * days;           /* as cdq, sub, sar, imul */
    if (!season_sizes_on() || room >= writes) {
        original();
        return;
    }
    wr32(g_mem, self + FIXED_LIST, 0);
    fprintf(stderr, "recomp: league sizes: %.50s %d: no fixed fixture list (%d clubs and %d match days hold %d of its "
                    "%d)\n", win_str(rd32(g_mem, self + 4) + 4), (int16_t)rd16(g_mem, self + 0x40), n, days,
            room < 0 ? 0 : room, writes);
    rt_ret_cdecl(cpu.eax);
}

void o_00642a70(void);
void o_00647230(void);
void o_0064be90(void);
void o_00650100(void);
RT_WRAP(00642a70, ita_a_fixed_list) { fixed_list(o_00642a70, 306); }
RT_WRAP(00647230, ita_b_fixed_list) { fixed_list(o_00647230, 380); }
RT_WRAP(0064be90, c1a_fixed_list) { fixed_list(o_0064be90, 306); }
RT_WRAP(00650100, c1b_fixed_list) { fixed_list(o_00650100, 306); }

/* ---- C1 regroup ---- */

static struct {
    int on;             /* the regroup of this season change takes the block and the counts */
    uint32_t block;     /* game memory, REGROUP_MAX club pointers */
    int collected, to_b;
} rg;

static uint32_t record_of(uint32_t key)
{
    uint32_t idx = rd32(g_mem, key), base = rd32(g_mem, COMP_RECORDS);
    return readable(base) ? base + idx * COMP_RECORD_SIZE : 0;
}

static int clubs_in(uint32_t rec)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    for (int i = 0; rec && readable(table) && i < count; i++)
        n += rd32(g_mem, table + CLUB_SIZE * (uint32_t)i + CLUB_DIVISION) == rec;
    return n;
}

int season_sizes_ita_regroup_begin(uint32_t serie_a)
{
    rg.on = 0;
    if (!season_sizes_on() || !readable(serie_a)) return 1;
    /* No club in C1/A or C1/B: the loop of 0x6426d4..0x64276a runs at least once and would write through an empty
     * slot (0x642763), so the regroup does nothing. */
    if (!clubs_in(record_of(KEY_C1A)) && !clubs_in(record_of(KEY_C1B))) return 0;
    if (!rg.block) {
        Cpu saved = cpu;
        uint32_t a[1] = {REGROUP_MAX * 4u};
        uint32_t p = rt_callback(0x00944e46u, 1, a);     /* the game's allocator (0x944e46) */
        if (rt_unwinding) return 0;
        cpu = saved;
        if (!readable(p)) return 1;
        rg.block = p;
    }
    memset(g_mem + rg.block, 0, REGROUP_MAX * 4u);
    /* C1/B's clubs for the season that ItaSerA starts (start_new_season adds 1 to +0x40 at 0x641789, after the
     * regroup at 0x641743): its row with changing rules on, else the count of its last table (+0x3e of the C1/B
     * object: the start season's count, Q232). Not the clubs it holds now: the C2 exchange before the regroup
     * (0x64173c) relegates C1/B's clubs and puts all 6 C2 clubs into C1/A (0x6420db..0x642104). */
    int year = (int16_t)rd16(g_mem, serie_a + 0x40) + 1;
    int b = season_changing_rules() ? season_sizes_clubs("ita_ser_c1b", year) : -1;
    if (b < 0) {
        uint32_t objs = rd32(g_mem, COMP_OBJECTS), c1b = readable(objs) ? rd32(g_mem, objs + rd32(g_mem, KEY_C1B) * 4u) : 0;
        b = count_of(c1b);
    }
    rg.to_b = b > 0 ? b : C1_CLASS;
    rg.collected = 0;
    rg.on = 1;
    return 1;
}

void season_sizes_ita_regroup_end(void)
{
    if (rg.on && rg.collected >= REGROUP_MAX)
        fprintf(stderr, "recomp: league sizes: ita regroup: %d clubs of Serie C1 or more, any others keep their "
                        "group\n", REGROUP_MAX);
    if (rg.on && season_sizes_on())
        fprintf(stderr, "recomp: league sizes: ita regroup: %d clubs of Serie C1, %d to C1/B\n", rg.collected,
                rg.collected < rg.to_b ? rg.collected : rg.to_b);
    rg.on = 0;
}

/* The limit of the collection (cmp dl, 0x24 at 0x64269c). */
uint32_t rt_ita_regroup_max(uint32_t original) { return rg.on ? REGROUP_MAX : original; }

/* The array of the collection, the sort and the loop (0x64269f, 0x6426bc, 0x6426d0): the block in place of the stack. */
uint32_t rt_ita_regroup_buf(uint32_t stack) { return rg.on ? rg.block : stack; }

/* The collected count (movsx eax, dl at 0x6426b2). */
uint32_t rt_ita_regroup_count(uint32_t n)
{
    if (rg.on) rg.collected = (int)n;
    return n;
}

/* The clubs that go to C1/B (cmp ax, 0x12 at 0x6426d4). */
uint32_t rt_ita_regroup_split(uint32_t original)
{
    if (!rg.on) return original;
    return (uint32_t)(rg.collected < rg.to_b ? rg.collected : rg.to_b);
}

/* The end of the loop (cmp ax, 0x24 at 0x64275f): the clubs collected. */
uint32_t rt_ita_regroup_end(uint32_t original)
{
    return rg.on && rg.collected > 0 ? (uint32_t)rg.collected : original;
}

/* ---- C2 exchange ---- */

/* The rows that the loop of 0x641fc0 reads (0x64210c): the larger count of C1/A and C1/B. */
uint32_t rt_ita_c1_rows(uint32_t c1a, uint32_t c1b)
{
    int a = count_of(c1a);
    if (!season_sizes_on()) return (uint32_t)a;
    int b = count_of(c1b);
    return (uint32_t)(b > a ? b : a);
}

/* The status of row i of a C1 table (cmp byte ptr [eax + 0x37], 3 at 0x642122 and 0x642161): none past its count. */
uint32_t rt_ita_row_status(uint32_t obj, uint32_t i, uint32_t row)
{
    if (season_sizes_on() && (int)i >= count_of(obj)) return 0;
    return g_mem[row + ROW_STATUS];
}
