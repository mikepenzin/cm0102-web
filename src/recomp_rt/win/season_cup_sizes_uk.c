/* Cups of Britain and Ireland follow the divisions (0.7.2 league sizes, slice 4; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D14 and D19). Each cup takes its list size and counts at run
 * time from the divisions, as rt_cup_room (season_sizes.c) does for the Welsh Cup. Only a season game with the sizes
 * on (season_sizes_on) gets any of it: the base game and RECOMP_LEAGUE_SIZES=off keep the original's counts.
 *
 * - The League Cup, the Scottish League Cup and the Scottish Challenge Cup take every club of their divisions
 *   (rt_cup_clubs, the count and list size patches of tools/recomp/league_sizes.py CUP_INSNS):
 *   EngCcCup::collect_entrants 0x56ccc0 allocates 92 places (push 0x228 at 0x56cccd, count 0x5c at 0x56ccd4) for
 *   the four League divisions, and returns 0 when it finds another number (0x56d0f9..0x56d104).
 *   ScoLgeCup::collect_entrants 0x7f0400 sets 40 (1999, 0x7f0417) or 42 (0x7f041f) and allocates from that count
 *   (0x7f0425..0x7f0436); its loops of the Third, Second and First Divisions have no bound (0x7f04cd, 0x7f054b,
 *   0x7f05d5). ScoChalCup::collect_entrants 0x7ed010 allocates 30 places (push 0xb4 at 0x7ed01b, count 0x1e at
 *   0x7ed024) for the Third, Second and First Divisions (0x7ed0c2..0x7ed0d2).
 * - The Irish Cup, the Welsh League Cup and the FA Cup keep their place counts (32, 18, 152): their round tables
 *   are made for them. A wrap of collect_entrants gives the original the clubs it can hold, and then puts the
 *   others in, so the original's own order, sort and random numbers stay:
 *   NirCup::collect_entrants 0x78ac90 puts the Premier and First Division clubs ([0x9cf8b0], [0x9cf8b4]) from place
 *   12 with no bound (0x78ad93..0x78adb0, byte4 1) and 12 clubs of the lower pool ([0x9cf8b8], at most 30,
 *   0x78ad57..0x78ad75) at places 0 to 11 (0x4afbd0 at 0x78addc, byte4 2 at 0x78ae0a). With more than 20 such
 *   clubs it wrote past its 32 places (push 0xc0). The wrap hides the clubs after the first 20 in table order
 *   (club +0x57 = 0 for the call), then puts each of them, byte4 1, in place of the last lower places. With fewer
 *   than 12 lower clubs ([0x9cf8b8]) it lends the lower division to Premier and First Division clubs for the call
 *   (lend_nir_lower), else 0x4afbd0 boxes "comp_util..cpp 241".
 *   WelLgeCup::collect_entrants 0x927010 makes 6 groups of 3 from 18 pointers (calloc at 0x92702a, +0xf2 = 18 at
 *   0x927020): every Welsh Premier club ([0x9cf94c]) with no bound (0x9270cf), then a box at 0x92716b
 *   (wel_lge_cup.cpp 743) when fewer than 18. With a Premier of 12 (2010/11 to 2025/26) the wrap lends the
 *   Premier division to the best Welsh clubs (+0x53 the nation [0x9cf510], +0x80 reputation) of the Welsh lower
 *   division ([0x9cf960]) for the call; with more than 18 it hides those after the first 18.
 *   EngFaCup::collect_entrants 0x56fab0 puts the Conference clubs ([0x9cf69c]) in a stack array of 22 at
 *   [esp + 0x3c] with no bound (0x56fd98), then copies 38 lower places (0x56fffc..0x57003e) and 22 Conference places
 *   (0x570046..0x57008b), then the FA Trophy finalists and the League clubs up to 152. The wrap hides the
 *   Conference clubs after the first 22 in table order, then puts each of them in place of the last places of the
 *   first block. Fewer than 22 is rt_fa_cup_takes (season_sizes.c).
 * - The Welsh Cup (WelCup::collect_entrants 0x924ab0) already takes 32 less the Premier clubs from the lower
 *   division (rt_cup_room at 0x924b81).
 * A place that a short pool leaves empty takes a club of the nation before the draw (season_cup_entrants.c). */
#include "win.h"
#include "season_sizes.h"

#include <stdio.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_COUNT_LESS 0xae2350u       /* NirCup counts [0xae235c] - 2 * [0xae2350] clubs (0x78ad2b..0x78ad42) */
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define CLUB_REPUTATION 0x80u
#define CUP_CLUBS 0x3eu
#define CUP_LIST 0xa7u
#define ENTRY 6u

#define KEY_ENG_PRM 0x9cf5bcu
#define KEY_ENG_FIRST 0x9cf5c0u
#define KEY_ENG_SECOND 0x9cf5c4u
#define KEY_ENG_THIRD 0x9cf5c8u
#define KEY_ENG_CONF 0x9cf69cu
#define KEY_NIR_PRM 0x9cf8b0u
#define KEY_NIR_FIRST 0x9cf8b4u
#define KEY_WEL_PRM 0x9cf94cu
#define KEY_WEL_LOWER 0x9cf960u
#define KEY_WALES 0x9cf510u

#define KEY_NIR_LOWER 0x9cf8b8u
#define NIR_TOP 20                      /* 32 places less the 12 lower places of 0x78ae11 */
#define NIR_LOWER 12                    /* the lower places: need 12 at 0x78add4, the copy to 0x48 at 0x78ae11 */
#define WEL_LGE_PLACES 18
#define FA_CONF 22
#define MOST_HELD 64

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t club_at(int i) { return rd32(g_mem, CLUBS) + CLUB_SIZE * (uint32_t)i; }

static int all_clubs(void) { return (int)rd32(g_mem, CLUB_COUNT); }

/* 1 when the club's division has the record id of the key (as each loop of these classes compares it). */
static int in_key(uint32_t club, uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t div = rd32(g_mem, club + CLUB_DIVISION);
    return id >= 0 && readable(div) && (int32_t)rd32(g_mem, div) == id;
}

/* The clubs of a key's division among the first range clubs, or -1 when the game has no such division. */
static int clubs_of(uint32_t key, int range)
{
    if ((int32_t)rd32(g_mem, key) < 0 || !readable(rd32(g_mem, CLUBS))) return -1;
    int n = 0;
    for (int i = 0; i < range; i++) n += in_key(club_at(i), key);
    return n;
}

uint32_t rt_cup_clubs(uint32_t original, uint32_t key_a, uint32_t key_b, uint32_t key_c, uint32_t key_d)
{
    if (!season_sizes_on()) return original;
    const uint32_t keys[4] = {key_a, key_b, key_c, key_d};
    int sum = 0;
    for (int k = 0; k < 4; k++) {
        if (!keys[k]) continue;
        int n = clubs_of(keys[k], all_clubs());
        if (n < 0) return original;
        sum += n;
    }
    if (sum < 2) return original;
    if ((uint32_t)sum != original)
        fprintf(stderr, "recomp: league sizes: a cup of %d clubs takes %d, its divisions\n", (int)original, sum);
    return (uint32_t)sum;
}

/* ---- Clubs held out of a call, or lent a division for it ---- */

typedef struct {
    uint32_t club, division;            /* the club and its own division (+0x57) */
} Held;

/* Hides the clubs of key a or b after the first keep in table order (the original's own scan order): +0x57 = 0, so
 * the original skips them. Returns the count. */
static int hide_after(uint32_t key_a, uint32_t key_b, int keep, int range, Held *held)
{
    int seen = 0, n = 0;
    for (int i = 0; i < range && n < MOST_HELD; i++) {
        uint32_t c = club_at(i);
        if (!in_key(c, key_a) && !(key_b && in_key(c, key_b))) continue;
        if (seen++ < keep) continue;
        held[n].club = c;
        held[n].division = rd32(g_mem, c + CLUB_DIVISION);
        wr32(g_mem, c + CLUB_DIVISION, 0);
        n++;
    }
    return n;
}

static void give_back(const Held *held, int n)
{
    for (int i = 0; i < n; i++) wr32(g_mem, held[i].club + CLUB_DIVISION, held[i].division);
}

static int listed(uint32_t list, int places, uint32_t club)
{
    for (int i = 0; i < places; i++)
        if (rd32(g_mem, list + ENTRY * (uint32_t)i) == club) return 1;
    return 0;
}

/* Puts each held club that is not in the list (of places) in place of the last of the first end places that lower()
 * accepts (byte4 as given, byte5 0). Returns the clubs that found no place. */
static int put_held(uint32_t list, int places, int end, const Held *held, int n, int (*lower)(uint32_t at), int byte4)
{
    int at = end - 1, left = 0;
    for (int k = 0; k < n; k++) {
        if (listed(list, places, held[k].club)) continue;
        while (at >= 0 && !lower(list + ENTRY * (uint32_t)at)) at--;
        if (at < 0) {
            left++;
            continue;
        }
        uint32_t p = list + ENTRY * (uint32_t)at--;
        wr32(g_mem, p, held[k].club);
        g_mem[p + 4] = (uint8_t)byte4;
        g_mem[p + 5] = 0;
    }
    return left;
}

/* ---- NirCup::collect_entrants 0x78ac90 ---- */

static int nir_lower_place(uint32_t at) { return g_mem[at + 4] == 2; }

/* The lower pool needs 12 clubs: 0x4afbd0 (need 12 at 0x78add4) boxes "comp_util..cpp 241" with fewer (0x4afbef
 * to 0x4afc64), and then 0x78ade4..0x78ae14 copies 12 entries of the stack list all the same, the last ones stale.
 * The rules table can leave fewer (Northern Ireland 2008/09: a Premiership of 12 and a Championship of 17 take
 * clubs of the lower division; release set row ta2001). Lends the lower division to Premier and First Division
 * clubs for the call: first the held ones (put_held would put them in lower places), then the last of the others
 * in table order. Returns the count; lent[] keeps each club's own division. */
static int lend_nir_lower(int range, const Held *held, int n, Held *lent)
{
    int lower = 0, k = 0;
    uint32_t div = 0;
    for (int i = 0; i < range; i++)
        if (in_key(club_at(i), KEY_NIR_LOWER)) {
            lower++;
            if (!div) div = rd32(g_mem, club_at(i) + CLUB_DIVISION);
        }
    if (!div || lower >= NIR_LOWER) return 0;
    for (int h = 0; h < n && lower + k < NIR_LOWER; h++, k++) {
        lent[k] = held[h];
        wr32(g_mem, held[h].club + CLUB_DIVISION, div);
    }
    for (int i = range - 1; i >= 0 && lower + k < NIR_LOWER; i--) {
        uint32_t c = club_at(i);
        if (!in_key(c, KEY_NIR_PRM) && !in_key(c, KEY_NIR_FIRST)) continue;
        lent[k].club = c;
        lent[k].division = rd32(g_mem, c + CLUB_DIVISION);
        wr32(g_mem, c + CLUB_DIVISION, div);
        k++;
    }
    fprintf(stderr, "recomp: league sizes: Irish Cup: %d lower clubs, %d Premier and First Division clubs lent to "
                    "the lower places\n", lower, k);
    return k;
}

void o_0078ac90(void);

RT_WRAP(0078ac90, nir_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    Held held[MOST_HELD], lent[NIR_LOWER];
    int n = 0, m = 0;
    if (season_sizes_on() && readable(rd32(g_mem, CLUBS))) {
        int range = all_clubs() - 2 * (int)rd32(g_mem, CLUB_COUNT_LESS);
        n = hide_after(KEY_NIR_PRM, KEY_NIR_FIRST, NIR_TOP, range, held);
        m = lend_nir_lower(range, held, n, lent);
    }
    o_0078ac90();
    give_back(lent, m);
    give_back(held, n);
    if ((!n && !m) || rt_unwinding) return;
    uint32_t list = rd32(g_mem, cup + CUP_LIST);
    int places = (int16_t)rd16(g_mem, cup + CUP_CLUBS);
    if (!readable(list) || places <= 0) return;
    for (int i = 0; i < places; i++)        /* a lent club is a Premier or First Division club: byte4 1 */
        for (int k = 0; k < m; k++)
            if (rd32(g_mem, list + ENTRY * (uint32_t)i) == lent[k].club) g_mem[list + ENTRY * (uint32_t)i + 4] = 1;
    if (!n) return;
    int left = put_held(list, places, places, held, n, nir_lower_place, 1), lower = 0;
    for (int i = 0; i < places; i++) lower += nir_lower_place(list + ENTRY * (uint32_t)i);
    fprintf(stderr, "recomp: league sizes: Irish Cup: %d Premier and First Division clubs, %d lower places\n",
            NIR_TOP + n, lower);
    if (left) fprintf(stderr, "recomp: league sizes: fault: Irish Cup: %d clubs found no place\n", left);
}

/* ---- WelLgeCup::collect_entrants 0x927010 ---- */

void o_00927010(void);

/* Lends the Premier division (div) to the best Welsh clubs of the lower division, at most want. */
static int lend_welsh(uint32_t div, int want, Held *held)
{
    int32_t wales = (int32_t)rd32(g_mem, KEY_WALES);
    int n = 0;
    while (n < want && n < MOST_HELD) {
        uint32_t best = 0;
        for (int i = 0; i < all_clubs(); i++) {
            uint32_t c = club_at(i), nation = rd32(g_mem, c + CLUB_NATION);
            if (!in_key(c, KEY_WEL_LOWER) || !readable(nation) || (int32_t)rd32(g_mem, nation) != wales) continue;
            if (!best || (int16_t)rd16(g_mem, c + CLUB_REPUTATION) > (int16_t)rd16(g_mem, best + CLUB_REPUTATION))
                best = c;
        }
        if (!best) break;
        held[n].club = best;
        held[n].division = rd32(g_mem, best + CLUB_DIVISION);
        wr32(g_mem, best + CLUB_DIVISION, div);
        n++;
    }
    return n;
}

RT_WRAP(00927010, wel_lge_cup_entrants)
{
    Held held[MOST_HELD];
    int n = 0;
    if (season_sizes_on() && readable(rd32(g_mem, CLUBS))) {
        int prem = clubs_of(KEY_WEL_PRM, all_clubs());
        uint32_t div = 0;
        for (int i = 0; prem > 0 && !div && i < all_clubs(); i++)
            if (in_key(club_at(i), KEY_WEL_PRM)) div = rd32(g_mem, club_at(i) + CLUB_DIVISION);
        if (prem > WEL_LGE_PLACES) {
            n = hide_after(KEY_WEL_PRM, 0, WEL_LGE_PLACES, all_clubs(), held);
            fprintf(stderr, "recomp: league sizes: Welsh League Cup: %d Premier clubs, the last %d in the table "
                            "order sit out\n", prem, n);
        } else if (prem > 0 && prem < WEL_LGE_PLACES) {
            n = lend_welsh(div, WEL_LGE_PLACES - prem, held);
            fprintf(stderr, "recomp: league sizes: Welsh League Cup: %d Premier clubs and %d of the lower division\n",
                    prem, n);
        }
    }
    o_00927010();
    give_back(held, n);
}

/* ---- EngFaCup::collect_entrants 0x56fab0 ---- */

static int fa_league_club(uint32_t c)
{
    return in_key(c, KEY_ENG_PRM) || in_key(c, KEY_ENG_FIRST) || in_key(c, KEY_ENG_SECOND) || in_key(c, KEY_ENG_THIRD);
}

/* A place of the first block, the clubs below the Conference (0x56fffc..0x57003e). put_held takes only places before
 * fa_lower_end, so the FA Trophy finalists after the Conference places (0x57008d, 0x5700bb) stay. */
static int fa_lower_place(uint32_t at) { return readable(rd32(g_mem, at)); }

/* The end of the first block: the first place that holds a League or Conference club. */
static int fa_lower_end(uint32_t list, int places)
{
    int i = 0;
    while (i < places) {
        uint32_t c = rd32(g_mem, list + ENTRY * (uint32_t)i);
        if (readable(c) && (fa_league_club(c) || in_key(c, KEY_ENG_CONF))) break;
        i++;
    }
    return i;
}

void o_0056fab0(void);

RT_WRAP(0056fab0, fa_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    Held held[MOST_HELD + 8];
    int n = 0;
    if (season_sizes_on() && readable(rd32(g_mem, CLUBS)))
        n = hide_after(KEY_ENG_CONF, 0, FA_CONF, all_clubs(), held);
    o_0056fab0();
    give_back(held, n);
    if (!n || rt_unwinding) return;
    uint32_t list = rd32(g_mem, cup + CUP_LIST);
    int places = (int16_t)rd16(g_mem, cup + CUP_CLUBS);
    if (!readable(list) || places <= 0) return;
    /* A hidden FA Trophy finalist ([esp + 0x1c], [esp + 0x30]) has no division during the call, so the original does
     * not take its place from the 22 Conference places (0x56fdc4) and still adds it (0x57008d): the list is one
     * over, and its last League club finds no place (the loop ends at 152). That League club goes back in too. */
    int extra = 0;
    for (int i = 0; i < all_clubs() && n + extra < MOST_HELD + 8; i++) {
        uint32_t c = club_at(i);
        if (!fa_league_club(c) || listed(list, places, c)) continue;
        held[n + extra].club = c;
        held[n + extra].division = rd32(g_mem, c + CLUB_DIVISION);
        extra++;
    }
    int left = put_held(list, places, fa_lower_end(list, places), held, n + extra, fa_lower_place, 0);
    fprintf(stderr, "recomp: league sizes: FA Cup: %d Conference clubs; %d clubs in place of clubs below it\n",
            FA_CONF + n, n + extra - left);
    if (left) fprintf(stderr, "recomp: league sizes: fault: FA Cup: %d clubs found no place\n", left);
}
