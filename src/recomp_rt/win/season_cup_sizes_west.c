/* The cups of western Europe take the real divisions (0.7.2 league sizes, slice 5; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D14). Each cup keeps its entrant count and the class's round
 * table, as rt_cup_room (season_sizes.c) does: every club of its divisions takes part, and the lower clubs take the
 * room that is left. A place that the lower clubs cannot fill takes a club of the nation from below the divisions
 * of the cup. The base game, and a season game with RECOMP_LEAGUE_SIZES=off, keep every cup as the original (D19,
 * Q152). Read in the original, 3.9.68:
 *
 * - BelFaCup::collect_entrants 0x41d8d0: 112 places (0x70 at 0x41d8e1). A pool of Belgian lower clubs (keys
 *   [0x9cf7d4] to [0x9cf7e0], at most 150, 0x41d9be), comp_util_sort_and_shuffle_top_clubs 0x4afbd0 of the top 44
 *   (push 0x2c at 0x41d9e4), a copy of 44 (mov edi, 0x2c at 0x41d9f4), then the three third-tier groups
 *   [0x9cf7c4], [0x9cf5a4], [0x9cf5a8] from place 44 (mov eax, 0x108 at 0x41da43), the Second Division
 *   [0x9cf5a0] and the First Division [0x9cf59c], each bounded by 112. It returns 1 only with 112 (sete at
 *   0x41db7b). A First Division of 16 left 2 places empty. Here the pool clubs are 112 less the five divisions.
 * - HolCup::collect_entrants 0x5f4820: 86 places (+0xf2, 0x5f4830). The Eredivisie [0x9cf56c] and the Eerste
 *   Divisie [0x9cf568] clubs that are not a second side (+0x1db == -1) first, then a copy of the first 50 places
 *   (mov edx, 0x32 at 0x5f49e1, no bound) whose sort and dedupe make 50 empty places, filled by 50 pool clubs
 *   (push 0x32 at 0x5f4a62, mov ebp, 0x32 at 0x5f4a7c), then the second sides (keys [0x9cf6f8], [0x9cf6f0],
 *   [0x9cf6e8]) and a fill of empty places from A Lower Division. 18 + 18 + 50 = 86: an Eerste Divisie of 20 wrote
 *   past the list. Here the three 50s are 86 less both divisions.
 * - DenCup::collect_entrants 0x551240: 92 places (0x5c at 0x551254), Danish clubs of A Lower Division [0x9cf764] up
 *   to the room of rt_cup_room (0x55131c), then the Second [0x9cf5b4], First [0x9cf5b0] and Premier [0x9cf5ac]
 *   Divisions. With a First Division of 12 (2012/13 on) the clubs that the club moves cut have no division, so
 *   the cup was short and its constructor stopped at den_cup.cpp 75 (0x550a62). Before slice 5 the First Division
 *   kept its database count. Here the cup takes Danish clubs with no division after the lower clubs.
 * - ItaCCup::collect_entrants 0x63dfe0 (Coppa Italia Serie C): 90 places (+0xf2, 0x63dff4) of a block it does not
 *   clear (0x944e46). Italian clubs of the C1 groups [0x9cf578], [0x9cf57c] and the C2 groups [0x9cf580],
 *   [0x9cf584], [0x9cf588], bounded by 90; then a second pass reads places 0 to 79 (0x63e109..0x63e19f) and swaps
 *   clubs into places 80 on. Smaller C2 groups left places that held no club. Here the empty places take Italian
 *   clubs below Serie C before that pass.
 * - ItaCup::select_entrants 0x63faf0 (Coppa Italia): 48 places, a pool of 32 (current Serie B clubs, clubs of last
 *   division B placed 3, 4 or above 16, C1 clubs placed 3 to 5, then C1 clubs by position) and 16 more (B 1 and 2,
 *   A 9 to 14 at 0x63fe28..0x63fe30, A 1 to 8 at 0x63feb0). The pool skips a club of last division A placed below
 *   15 (0x63fbfe), so with a Serie A of 20 the clubs placed 15 to 17, which stay in Serie A, enter no loop. Here
 *   every club of Serie A and Serie B takes part: a missing one takes the place of the weakest pool club that is
 *   in neither division.
 * - FraCup::collect_entrants 0x5baa50: 222 places; its three lower passes (0x5bab90, 0x5bac3c, 0x5bacf0) take the
 *   room left by Ligue 1 and Ligue 2 (rt_cup_add). When the list stays short (sete at 0x5bae36 gives 0 and
 *   FraCup::FraCup stops at fra_cup.cpp 76, 0x5ba2d1), other French clubs take the empty places before Ligue 2.
 * - FraLgeCup::collect_entrants 0x5bd160: 54 places, National clubs up to the room that Ligue 2 [0x9cf5d0] and
 *   Ligue 1 [0x9cf5cc] leave (rt_cup_room at 0x5bd2ac, rt_fra_lge_first), then both. A National of 17 (2024/25)
 *   with two Ligues of 18 left one place empty, and the constructor stopped at fra_lge_cup.cpp 73 (0x5bcb21).
 *   The empty places take French clubs from below the National (season_cup_sizes_fra_lge, from the wrap in
 *   season_sizes.c).
 * The fills take only clubs with 15 players or more in the 50 slots at club +0xd7, the count that
 * VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0 wants (0x90d43a..0x90d469, box virtual_staff.cpp 195). */
#include "win.h"
#include "../rt.h"
#include "season.h"
#include "season_change.h"
#include "season_cup_sizes_west.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u             /* the club table (0x245 bytes a club) */
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u      /* the clubs are [0xae235c] - 2 * [0xae2350]; the national sides follow */
#define NATIONS 0xae23a8u           /* the nation table, 0x122 bytes a nation */
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define CLUB_LAST_DIVISION 0x5bu
#define CLUB_LAST_POSITION 0x5fu
#define CLUB_REPUTATION 0x80u
#define CLUB_SQUAD 0xd7u            /* 50 player pointers (0x90d43a) */
#define SQUAD_SLOTS 50
#define SQUAD_LEAST 15              /* fill_fixture_squads 0x90d45f */
#define RECORD_LEVEL 0x69u
#define LOWER 0x9cf764u             /* A Lower Division */
#define NO_LEVEL (-1000)

static int readable(uint32_t p) { return rt_readable(p); }

/* As season_sizes.c: a season game with the sizes on (RECOMP_LEAGUE_SIZES=off: the original's counts). */
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

static int club_count(void) { return (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT); }

static uint32_t club_at(int i) { return rd32(g_mem, CLUBS) + (uint32_t)i * CLUB_SIZE; }

/* The nation pointer of a nation key id global, as the cups compare it (0x5512fa..0x551318). */
static uint32_t nation_of_key(uint32_t key) { return rd32(g_mem, NATIONS) + rd32(g_mem, key) * 0x122u; }

/* The record id of a club's division, -1 for none. */
static int32_t division_id(uint32_t club)
{
    uint32_t d = rd32(g_mem, club + CLUB_DIVISION);
    return readable(d) ? (int32_t)rd32(g_mem, d) : -1;
}

static int level_of(uint32_t club)
{
    uint32_t d = rd32(g_mem, club + CLUB_DIVISION);
    return readable(d) ? (int16_t)rd16(g_mem, d + RECORD_LEVEL) : NO_LEVEL;
}

static int in_key(uint32_t club, uint32_t key) { return key && division_id(club) == (int32_t)rd32(g_mem, key); }

/* The players of a club (the slots that fill_fixture_squads 0x90d2f0 counts). */
static int squad_of(uint32_t club)
{
    int n = 0;
    for (int i = 0; i < SQUAD_SLOTS; i++) n += rd32(g_mem, club + CLUB_SQUAD + 4u * (uint32_t)i) != 0;
    return n;
}

/* The clubs of up to five divisions (key id globals, 0 for none). */
static int clubs_of(const uint32_t *keys, int nkeys)
{
    int n = 0, count = club_count();
    if (!readable(rd32(g_mem, CLUBS))) return -1;
    for (int i = 0; i < count; i++)
        for (int k = 0; k < nkeys; k++)
            if (in_key(club_at(i), keys[k])) {
                n++;
                break;
            }
    return n;
}

/* The best club of a nation for an empty place that is not taken yet: not in a division of skip, with 15 players,
 * the highest level first (a club with no division last), then the highest reputation; with none_only, only a
 * club with no division. When no such club has 15 players, the club with the most players (the original's own
 * lower pools take clubs with no test of the squad). taken(club, ctx) is 1 for a club that the cup holds. 0 when
 * none is left. */
static uint32_t best_club(uint32_t nation, const uint32_t *skip, int nskip, int none_only,
                          int (*taken)(uint32_t, void *), void *ctx)
{
    uint32_t best = 0, most = 0;
    int count = club_count(), most_players = -1;
    for (int i = 0; i < count; i++) {
        uint32_t c = club_at(i);
        if (rd32(g_mem, c + CLUB_NATION) != nation) continue;
        if (none_only && readable(rd32(g_mem, c + CLUB_DIVISION))) continue;
        int skipped = 0;
        for (int k = 0; k < nskip && !skipped; k++) skipped = in_key(c, skip[k]);
        if (skipped || taken(c, ctx)) continue;
        int players = squad_of(c);
        if (players < SQUAD_LEAST) {
            if (players > most_players) most = c, most_players = players;
            continue;
        }
        if (best) {
            int lc = level_of(c), lb = level_of(best);
            if (lc < lb || (lc == lb && (int16_t)rd16(g_mem, c + CLUB_REPUTATION) <=
                                         (int16_t)rd16(g_mem, best + CLUB_REPUTATION)))
                continue;
        }
        best = c;
    }
    return best ? best : most;
}

/* ---- Belgian Cup ---- */

static const uint32_t BEL_KEYS[] = {0x9cf7c4u, 0x9cf5a4u, 0x9cf5a8u, 0x9cf5a0u, 0x9cf59cu};
#define BEL_PLACES 112

uint32_t rt_bel_cup_lower(uint32_t original, uint32_t pool)
{
    if (!sizes_on()) return original;
    int top = clubs_of(BEL_KEYS, 5), want = BEL_PLACES - top, have = (int16_t)(pool & 0xffffu);
    if (top < 0 || want < 1) return original;
    if (want > have) want = have;
    if (want < 1) return original;
    static int said = -1;
    if (logging() && want != said) {
        said = want;
        fprintf(stderr, "recomp: cup sizes: Belgian Cup: %d lower clubs of %d in the pool, %d of the divisions "
                        "(the original takes 44)\n", want, have, top);
    }
    return (uint32_t)want;
}

uint32_t rt_bel_cup_place(uint32_t original, uint32_t taken)
{
    return sizes_on() ? taken * 6u : original;
}

/* ---- Dutch Cup ---- */

static const uint32_t HOL_KEYS[] = {0x9cf56cu, 0x9cf568u};
#define HOL_PLACES 86

uint32_t rt_hol_cup_lower(uint32_t original, uint32_t esp)
{
    if (!sizes_on()) return original;
    int top = clubs_of(HOL_KEYS, 2), want = HOL_PLACES - top, pool = (int16_t)rd16(g_mem, esp + 0x10u);
    if (top < 0) return original;
    if (want > pool) want = pool;
    if (want < 1) want = 1;                   /* the copy loop (dec edx, jne at 0x5f49f7) needs 1 or more */
    static int said = -1;
    if (logging() && want != said) {
        said = want;
        fprintf(stderr, "recomp: cup sizes: Dutch Cup: %d lower clubs of %d in the pool, %d of the two divisions "
                        "(the original takes 50)\n", want, pool, top);
    }
    return (uint32_t)want;
}

/* ---- Danish Cup ---- */

#define DEN_NATION 0x9cf2c0u
#define DEN_PLACES 92
#define ENTRY 6u

struct list6 { uint32_t list; int n; };

static int in_list6(uint32_t club, void *ctx)
{
    struct list6 *l = ctx;
    for (int i = 0; i < l->n; i++)
        if (rd32(g_mem, l->list + ENTRY * (uint32_t)i) == club) return 1;
    return 0;
}

#define MOST_PLACES 256

/* A cup with a list of 6-byte places (+0xa7, count +0x3e) that its collect_entrants left short: clubs of the
 * nation of nation_key that the cup does not hold (with none_only, only clubs with no division) take the empty
 * places, put before the first club of a division of keys, so those divisions keep the end of the list as the
 * original orders it. Entry bytes 0, as the original's lower clubs. Returns 1 when the list is full. */
static int fill_short(uint32_t cup, int places, uint32_t nation_key, const uint32_t *keys, int nkeys, int none_only,
                      const char *name)
{
    uint32_t list = rd32(g_mem, cup + 0xa7u);
    int count = (int16_t)rd16(g_mem, cup + 0x3eu), placed = 0, lower = 0;
    if (!readable(list) || count != places || count > MOST_PLACES || !readable(rd32(g_mem, CLUBS))) return 0;
    while (placed < count && readable(rd32(g_mem, list + ENTRY * (uint32_t)placed))) placed++;
    for (int top = 0; lower < placed && !top; lower += !top) {
        uint32_t c = rd32(g_mem, list + ENTRY * (uint32_t)lower);
        for (int k = 0; k < nkeys && !top; k++) top = in_key(c, keys[k]);
    }
    struct list6 l = {list, placed};
    uint32_t nation = nation_of_key(nation_key), add[MOST_PLACES];
    int k = 0;
    while (placed + k < count) {
        uint32_t c = best_club(nation, keys, nkeys, none_only, in_list6, &l);
        if (!c) break;
        add[k++] = c;
        wr32(g_mem, list + ENTRY * (uint32_t)(placed + k - 1), c);    /* the next search sees it in the list */
        l.n = placed + k;
    }
    /* move the divisions up by k and put the new clubs before them */
    for (int i = placed - 1; i >= lower; i--) {
        uint32_t from = list + ENTRY * (uint32_t)i, to = list + ENTRY * (uint32_t)(i + k);
        wr32(g_mem, to, rd32(g_mem, from));
        g_mem[to + 4] = g_mem[from + 4];
        g_mem[to + 5] = g_mem[from + 5];
    }
    for (int j = 0; j < k; j++) {
        uint32_t at = list + ENTRY * (uint32_t)(lower + j);
        wr32(g_mem, at, add[j]);
        g_mem[at + 4] = 0;
        g_mem[at + 5] = 0;
    }
    if (logging() || placed + k < count)
        fprintf(stderr, "recomp: cup sizes: %s: %d lower clubs, %d of the divisions, %d more clubs of the nation%s\n",
                name, lower, placed - lower, k, placed + k < count ? " (still short)" : "");
    if (placed + k < count) {          /* why: the nation's clubs that the cup does not hold */
        int none = 0, none_squad = 0, other = 0, other_squad = 0, n = club_count();
        for (int i = 0; i < n; i++) {
            uint32_t c = club_at(i);
            if (rd32(g_mem, c + CLUB_NATION) != nation || in_list6(c, &l)) continue;
            int sq = squad_of(c) >= SQUAD_LEAST, nd = !readable(rd32(g_mem, c + CLUB_DIVISION));
            none += nd, none_squad += nd && sq, other += !nd, other_squad += !nd && sq;
        }
        fprintf(stderr, "recomp: cup sizes: %s: the nation's clubs out of the cup: %d with no division (%d with 15 "
                        "players), %d in a division (%d with 15 players)\n", name, none, none_squad, other, other_squad);
    }
    return placed + k == count;
}

void o_00551240(void);

/* DenCup::collect_entrants 0x551240, thiscall (cup), returns 1 with 92 clubs. When the lower clubs leave places
 * empty, the cup takes Danish clubs with no division (the clubs that the club moves cut) after them. */
RT_WRAP(00551240, den_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    o_00551240();
    if (!sizes_on() || cpu.eax) return;
    static const uint32_t keys[] = {0x9cf5b4u, 0x9cf5b0u, 0x9cf5acu};
    cpu.eax = (uint32_t)fill_short(cup, DEN_PLACES, DEN_NATION, keys, 3, 1, "Danish Cup");
}

/* ---- Coppa Italia Serie C ---- */

#define ITA_NATION 0x9cf360u
#define ITA_A 0x9cf570u
#define ITA_B 0x9cf574u
#define ITA_C_PLACES 90

struct list4 { uint32_t list; int n; };

static int in_list4(uint32_t club, void *ctx)
{
    struct list4 *l = ctx;
    for (int i = 0; i < l->n; i++)
        if (rd32(g_mem, l->list + 4u * (uint32_t)i) == club) return 1;
    return 0;
}

/* At `mov eax, 0x50` (0x63e109), after the first pass: esi the cup, edi the clubs it placed. The empty places take
 * Italian clubs below Serie C, so the second pass reads clubs only. */
uint32_t rt_ita_c_cup_fill(uint32_t original, uint32_t cup, uint32_t placed)
{
    if (!sizes_on()) return original;
    uint32_t list = rd32(g_mem, cup + 0xeeu);
    int count = (int)rd32(g_mem, cup + 0xf2u), n = (int)placed, from = n;
    if (!readable(list) || count != ITA_C_PLACES || n >= count || n < 1) return original;
    static const uint32_t skip[] = {ITA_A, ITA_B, 0x9cf578u, 0x9cf57cu, 0x9cf580u, 0x9cf584u, 0x9cf588u};
    struct list4 l = {list, n};
    uint32_t nation = nation_of_key(ITA_NATION);
    while (n < count) {
        uint32_t c = best_club(nation, skip, 7, 0, in_list4, &l);
        if (!c) break;
        wr32(g_mem, list + 4u * (uint32_t)n++, c);
        l.n = n;
    }
    if (n < count) {                     /* none left: the places hold a club, never a value of the block */
        for (int i = n; i < count; i++) wr32(g_mem, list + 4u * (uint32_t)i, rd32(g_mem, list + 4u * (uint32_t)(i % from)));
        fprintf(stderr, "recomp: cup sizes: Coppa Italia Serie C: only %d of %d clubs\n", n, count);
    }
    if (logging())
        fprintf(stderr, "recomp: cup sizes: Coppa Italia Serie C: %d clubs of Serie C, %d from below\n", from, n - from);
    return original;
}

/* ---- Coppa Italia ---- */

#define ITA_CUP_PLACES 48
#define ITA_CUP_POOL 32

static int required(uint32_t club) { return in_key(club, ITA_A) || in_key(club, ITA_B); }

/* After ItaCup::select_entrants 0x63faf0 (its wrap in season_divisions_ita.c) has made the list: each Italian club
 * of Serie A or Serie B that is not in it takes the pool place of the club of neither division with the highest
 * last position (then the lowest reputation). */
void season_cup_sizes_ita_cup(uint32_t cup)
{
    if (!sizes_on()) return;
    uint32_t list = rd32(g_mem, cup + 0xeeu), nation = nation_of_key(ITA_NATION);
    int count = (int)rd32(g_mem, cup + 0xf2u), moved = 0, missing = 0, clubs = club_count();
    if (!readable(list) || count != ITA_CUP_PLACES || !readable(rd32(g_mem, CLUBS))) return;
    for (int i = 0; i < clubs; i++) {
        uint32_t c = club_at(i);
        if (rd32(g_mem, c + CLUB_NATION) != nation || !required(c)) continue;
        int in = 0;
        for (int j = 0; j < count && !in; j++) in = rd32(g_mem, list + 4u * (uint32_t)j) == c;
        if (in) continue;
        int weakest = -1;
        for (int j = 0; j < ITA_CUP_POOL; j++) {
            uint32_t o = rd32(g_mem, list + 4u * (uint32_t)j);
            if (readable(o) && required(o)) continue;
            if (!readable(o)) {
                weakest = j;
                break;
            }
            if (weakest >= 0) {
                uint32_t w = rd32(g_mem, list + 4u * (uint32_t)weakest);
                int po = g_mem[o + CLUB_LAST_POSITION], pw = g_mem[w + CLUB_LAST_POSITION];
                if (po < pw || (po == pw && (int16_t)rd16(g_mem, o + CLUB_REPUTATION) >=
                                              (int16_t)rd16(g_mem, w + CLUB_REPUTATION)))
                    continue;
            }
            weakest = j;
        }
        if (weakest < 0) {
            missing++;
            continue;
        }
        wr32(g_mem, list + 4u * (uint32_t)weakest, c);
        moved++;
    }
    if ((moved && logging()) || missing)
        fprintf(stderr, "recomp: cup sizes: Coppa Italia: %d clubs of Serie A and B take pool places%s\n", moved,
                missing ? " (some find no place)" : "");
}

/* ---- French Cup ---- */

void o_005baa50(void);

/* FraCup::collect_entrants 0x5baa50, thiscall (cup), returns 1 with 222 clubs (sete at 0x5bae36). The lower passes
 * take the room that Ligue 1 and Ligue 2 leave; when the French clubs below the National are too few, the list stays
 * short, other French clubs take the empty places before Ligue 2 and Ligue 1. */
RT_WRAP(005baa50, fra_cup_entrants)
{
    uint32_t cup = cpu.ecx;
    o_005baa50();
    if (!sizes_on() || cpu.eax) return;
    static const uint32_t keys[] = {0x9cf5d0u, 0x9cf5ccu};
    cpu.eax = (uint32_t)fill_short(cup, 222, 0x9cf300u, keys, 2, 0, "French Cup");
}

/* After FraLgeCup::collect_entrants 0x5bd160 (its wrap in season_sizes.c) left the list short: French clubs from below
 * the National take the empty places. Returns 1 when the list is full. */
int season_cup_sizes_fra_lge(uint32_t cup)
{
    if (!sizes_on()) return 0;
    static const uint32_t keys[] = {0x9cf5d4u, 0x9cf5d0u, 0x9cf5ccu};
    return fill_short(cup, 54, 0x9cf300u, keys, 3, 0, "French League Cup");
}

/* ---- French background leagues: the CFA and the French Lower Division ---- */

#define CFA_PLACES 0x48
#define LOWER_PLACES 0x28
#define CLUB_ZONE 0x60u
#define CFA_ZONE 0x9cf5d8u

struct zoned { struct list4 l; int want_zone; uint32_t zone; };

/* Taken: in the array, or (in the first pass) not in the zone that the league's own loop takes. */
static int not_zoned(uint32_t club, void *ctx)
{
    struct zoned *z = ctx;
    if (in_list4(club, &z->l)) return 1;
    if (!z->want_zone) return 0;
    uint32_t r = rd32(g_mem, club + CLUB_ZONE);
    return z->zone ? !(readable(r) && rd32(g_mem, r) == rd32(g_mem, z->zone)) : r != 0;
}

/* Fills a league's club array (4 bytes a club, `count` placed) to `places` with French clubs from below the National:
 * first the clubs of the zone that the league's loop takes (zone_key, or no zone for 0), then any. Returns the new
 * count, the high half of count kept. */
static uint32_t fill_french_league(uint32_t array, uint32_t count, int places, uint32_t zone_key, const char *name)
{
    int n = (int16_t)(count & 0xffffu), from = n;
    if (!sizes_on() || n >= places || n < 0) return count;
    static const uint32_t skip[] = {0x9cf5d4u, 0x9cf5d0u, 0x9cf5ccu};
    struct zoned z = {{array, n}, 1, zone_key};
    uint32_t nation = nation_of_key(0x9cf300u);
    for (int pass = 0; pass < 2 && n < places; pass++) {
        z.want_zone = pass == 0;
        while (n < places) {
            uint32_t c = best_club(nation, skip, 3, 0, not_zoned, &z);
            if (!c) break;
            wr32(g_mem, array + 4u * (uint32_t)n++, c);
            z.l.n = n;
        }
    }
    fprintf(stderr, "recomp: cup sizes: %s: %d clubs, %d more from below the National%s\n", name, from, n - from,
            n < places ? " (still short)" : "");
    return (count & 0xffff0000u) | (uint32_t)n;
}

/* FraCfa::FraCfa 0x5b91c0, at `lea eax, [esp + 0x21c]` (0x5b9320) after its loop: the array at esp + 0x21c holds
 * edi clubs, every club of Ligue 1 and Ligue 2 and the A Lower Division clubs of the CFA zone up to the cap of
 * rt_fra_cfa_lower_cap (0x5b92e8). The four groups of 18 read 72 places of an array that is never cleared
 * (0x5b933b..0x5b9362). When the zone holds fewer clubs, the empty places take French clubs from below the
 * National, of the CFA zone first, so no group takes a stack value. Returns the count for the shuffle (0x5b9327). */
uint32_t rt_fra_cfa_fill(uint32_t array, uint32_t count)
{
    return fill_french_league(array, count, CFA_PLACES, CFA_ZONE, "CFA");
}

/* The constructor of the French Lower Division 0x5bd600 (vtable 0x96ab78), at `lea edx, [esp + 0x1c]` (0x5bd752)
 * after its loop: the array at esp + 0x1c holds edi clubs, every National club ([0x9cf5d4], 0x5bd6dd) and up to 20
 * French A Lower Division clubs with no zone (+0x60 == 0, `cmp word ptr [esp + 0x10], 0x14` at 0x5bd71d), at most
 * 40 (0x5bd6e9). FraLower::create_table 0x5be230 then reads two groups of 20 (0x14 at 0x5be242; 0x5bd766..0x5bd78c)
 * of an array that is never cleared. A National of 17 (2024/25) left 3 places with stack values: 156 fixtures of the
 * division named no club, and a Coupe de France match on 28 December 2024 stopped at match_eng..cpp 652. The empty
 * places take French clubs from below the National, with no zone first. Returns the count for the shuffle. */
uint32_t rt_fra_lower_fill(uint32_t array, uint32_t count)
{
    return fill_french_league(array, count, LOWER_PLACES, 0, "French Lower Division");
}
