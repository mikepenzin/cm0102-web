/* The Tier A switch (0.6.2 slice 3, decisions Q9, Q10, Q16 and Q31 of the future play spec). RECOMP_TIER_A=1 is a
 * test switch, read once at the start and off by default. Off, every wrap below runs the original at once, so the
 * game is the same (Q152). On, a game plays only the competitions, to reach far years quickly: the calendar, draws,
 * entrants, league tables, season changes, history, finances, retirements, new players, contracts, and save and
 * load stay. The AI transfer market, background matches, new injuries and AI manager moves stop, and each match gets
 * an instant result from the two sides' reputation. Research: run-logs of slice 3 (research-match.md and
 * research-skips.md, read again for each address cited here) and run-logs/future-3/profile.md (the share of a
 * normal year of each area).
 *
 * The skips, each a full no-op that pops the original's arguments (no caller reads eax, cited per wrap). They act
 * only when the game is made (0xdd711c), so a Tier A game is made as the normal game is:
 * - The AI transfer market: update_club_shortlist 0x83a730 (ret 4 at 0x83a772, called at 0x5cdd15),
 *   make_club_bids 0x84a020 (ret 4 at 0x84abbf, called at 0x5cdd6d), the day loop's calls of the position lists
 *   0x839e60 (ret 4 at 0x83a12c, called at 0x5cda46) and 0x83a780 (ret at 0x83a9c4, called at 0x5cda56; the
 *   constructor 0x839cf8, the load 0x83b229 and 0x83a6b3 still build them), and add_player_to_club_shortlists
 *   0x842c00 (ret 8 at 0x842dfe, 17 callers, none reads eax). The day loop's
 *   club loop 0x5cdc4e..0x5cdd9a still runs, so the media calls 0x754a30 and 0x757c70 in it (their only callers)
 *   keep the story log short. The original's flag 0x9cd1f8 would skip them too, so it is not used.
 * - Background matches: SimulatedStats::play_daily_matches 0x84be20 (ret at 0x84c4fb, called at 0x5cdac9). It
 *   writes no fixture, table or history row, only player stats of clubs with no loaded league.
 * - Not original code, for the AI sales that Tier A lacks: a full AI club lets its weakest players over 21 go
 *   (release_surplus, open item 78).
 * - The other way, for the AI signings that Tier A lacks: an AI club with fewer than SIGN_BELOW players in its squad
 *   slots runs its own market, the original's shortlist and bids (short_club, open item 92).
 * Contracts stay. A skip of the contract pass 0x4c7ac0 (contract ends, called at 0x5cd406) and of the daily staff
 * pass 0x5ce2d0 (the AI contract offers, 0x4cda70 at 0x5d19d7 and five more) left each player at his club after his
 * contract ended, and such a player cannot play (an England game from 2025: Aston Villa had 50 players and 4 who
 * could play its Champions League match in August 2028, and init_match 0x6b7f20 stopped at match_eng.cpp 652,
 * 0x6b8aca). The top-up of match squads (0x90d2f0) counts the players at the club, not the ones who can play, so it
 * did not help. With the contract code kept, a player whose contract ends leaves the club, and a club below 15
 * gets virtual players for its matches. The shortlist offers of the daily staff pass (0x5cfaf6, 0x5d585f) are
 * add_player_to_club_shortlists 0x842c00 and stop with the market.
 * - Injuries: Injury::add_injury 0x62d250 (ret 0x20, wrapped in injury_init.c) returns -1 after the game is made.
 *   The injuries of the database and of game creation stay, and heal by date when read (0x62c680 from 0x62c400
 *   and 0x62ed70), with no daily code.
 * - AI manager moves: MANAGER_MANAGER::consider_sacking_manager 0x69aba0 (ret 8 at 0x69acb8, callers 0x696525,
 *   0x696539, 0x6a59b4, 0x6aa0c1, none reads eax), so no sacking or ultimatum. An appointed manager can still
 *   leave his club for a vacancy. The vacancy pass 0x68f9c0 stays: retirements (0x7aaeb0 calls add_job_vacancy 0x69ba50) and the season change
 *   (league_promote_and_relegate 0x689c80) open vacancies, and only it fills them. Without it more clubs and
 *   national teams would have no manager each year, as Q31 found for players.
 *
 * The instant result. match_day_can_quick_simulate 0x6b66a0 (cdecl, ret, callers 0x6b4ae3 and 0x6b5c9a push eax
 * as the quick argument of init_match 0x6b7f20) gives 1, after the original has run for its side effect on the
 * human clubs' flags (0x6b66b1..0x6b66df). Then run_match_steps 0x6b98c0 calls quick_simulate_match 0x6beeb0
 * (thiscall, ret at 0x6c0661, one caller 0x6b98d7, eax not read) for every match. Its wrap writes the score that
 * 0x6beeb0 writes, with the same tie rules, and skips the per-minute play, scorers, cards, injuries and ratings:
 * - the fixture copy F is [engine + 0x4792] (0x6bfe2b); its clubs are F+0x1c and F+0x20 (0x6c021c..0x6c0225),
 *   each with the reputation s16 at +0x80 (0x6c0228, 0x6c022f; a national side is a club row with half its
 *   nation's reputation)
 * - half time F+0x45/+0x46 at minute 45 (0x6bfe40, 0x6bfe4f), full time F+0x43/+0x44 at 90 (0x6bfe6d,
 *   0x6bfe7c), the side goals at engine +0xf5bd and +0xf5f3 (0x6bfe67, 0x6bfe76), the minute word +0x8ed0
 *   (0x6bfe01)
 * - level at 90 (one leg: F+0x43 == F+0x44 at 0x6bfed0; two legs, F+0x4b not -1: the aggregates with the first
 *   leg F+0x4b/+0x4c at 0x6bfe96..0x6bfea9): flags F+0x38 0x80 or 0x02 play extra time (0x6bfeb1..0x6bfec5).
 *   Else two legs with flag 4 clear and away goals F+0x44 != F+0x4b decide, unless flag 0x4000 (0x6c001c..
 *   0x6c0049); else flag 0x01 or 0x20 gives penalties (0x6c004f, 0x6c0060)
 * - after 120 the totals go to F+0x47/+0x48 (0x6c0085); still level (one leg 0x6c0102; two legs the aggregate,
 *   and with 0x4000 also F+0x48 == F+0x4b, 0x6c00a5..0x6c00ec): flag 0x01 or 0x20 gives penalties
 * - penalties F+0x49/+0x4a, never equal (0x6c0116..0x6c018f); then two legs add the result to F+0x4b/+0x4c
 *   (0x6c0192..0x6c01c9)
 * A draw with no flag stays a draw: the cup plans its replay (Cup::decide_tie_winner 0x51f2f0). The man of the
 * match 0x6d0f80, the reputation update 0x6d1cc0 and the result apply 0x6b5f00 (table, cup tie, gate money,
 * records, FIFA ranking) run as before. The goals come from a random number of the host, not utils_random
 * 0x90cfc0, so the instant result itself takes no draw of the game. */
#include "win.h"
#include "tier_a.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void o_0083a730(void);
void o_0084a020(void);
void o_00839e60(void);
void o_0083a780(void);
void o_00842c00(void);
void o_0084be20(void);
void o_0069aba0(void);
void o_006b66a0(void);
void o_006beeb0(void);

int tier_a(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_TIER_A");
        on = v && !strcmp(v, "1");
        if (on) fprintf(stderr, "recomp: Tier A switch on: competitions only, instant results from reputation\n");
    }
    return on;
}

#define GAME_MADE 0xdd711cu   /* not 0 when the game is made: create_game 0x828110 sets it at 0x829fe3,
                                 load_game_data 0x82a9c0 at 0x82cd6a, free_game_data 0x82ff40 clears it at 0x830509 */

int tier_a_in_play(void) { return tier_a() && rd32(g_mem, GAME_MADE) != 0; }

/* A skip pops the arguments as the original's ret does, in play only: game creation stays as in the normal game.
 * from: the return address of the one call to skip, or 0 for every call. (The recomp tool finds each wrap by its
 * RT_WRAP line.) */
static int skip(int nargs, uint32_t from)
{
    if (!tier_a_in_play() || (from && rd32(g_mem, cpu.esp) != from)) return 0;
    rt_ret_stdcall(0, nargs);
    return 1;
}

#define TRANSFERS 0xde1f54u          /* TRANSFER_MANAGER, the this of 0x8c16b0 and 0x8c1870 (0x83db1c, 0x83db60) */
#define SLOTS 50                     /* club squad slots +0xd7 (VIRTUAL_STAFF 0x90d8f0 counts 0x32) */
#define RELEASE_TO 37                /* a club that reaches 50 slots and goes down to N spends about 5 / (50 - N)
                                        of its time at 45 or more. Down to 45, 405 clubs had 45 or more in July
                                        2108, the normal game 153: 5 / 13 = 0.38 = 153 / 405, so N = 37 */
#define SIGN_BELOW 15                /* open item 92: a club with fewer players in its squad slots runs its market
                                        (the top-up of match squads 0x90d2f0 fills a club below 15 with virtual
                                        players, 0x90d45f..0x90d469) */
#define LISTS_EVERY 30               /* the original rebuilds the position lists on each day of the year that 30
                                        divides (0x5cda25..0x5cda36 before the call at 0x5cda46) */
#define CONTRACTS 0xae1050u          /* CONTRACT_MANAGER, the this of 0x4cf6d0 and 0x4cf5f0 (0x83d8cd, 0x83d901) */
#define CLUB_TABLE 0xae23b4u         /* the club rows (0x245 bytes each), national sides in the last 2 x nations */
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define CLUB_SIZE 0x245u

static uint32_t call_this(uint32_t fn, uint32_t self, int n, uint32_t *args)
{
    cpu.ecx = self;
    return rt_callback(fn, n, args);
}

/* Not original code: a Tier A step (0.6.2 open item 92), the other side of release_surplus. Tier A has no AI
 * signings, so a club whose players' contracts ended or who retired was never refilled, while the normal game's
 * market refills it. The MLS All-Star game takes at least 30 free players of each conference from the first rows of
 * the MLS groups (0x90a980: the players of the club slots +0xd7 that are not injured, 0x62c400, not on international
 * duty, 0x7757d0, and at that club, +0x39 at 0x90ab3c, counted at 0x90ab41 and 0x90ad3a; fewer than 30 stop at usa_mls_all_stars.cpp 476,
 * 0x90abe4, or 545, 0x90addd). In a Tier A game from 2001 the MLS clubs held 1 to 48 players in July 2068 (Kansas
 * City 1, NYNJ 3, Dallas 4) and the game stopped at line 545. The normal game from the same save of July 2061 had
 * 16 to 41 a year later, where Tier A had 6 to 44. So an AI club of a played league with fewer than SIGN_BELOW
 * players runs the original's market for itself: update_club_shortlist 0x83a730 (the day loop's random call at
 * 0x5cdd15) and make_club_bids 0x84a020 (0x5cdd6d), with the position lists they read, 0x839e60 and 0x83a780. The
 * transfers then go through TRANSFER_MANAGER::daily_update 0x8c5880 as in the normal game. From the save of July
 * 2061: 440 such clubs below 15 then, about 20 from 2063 on; the MLS clubs held 14 to 46 in July 2068 and the game
 * played on to 2072 with no box. A Tier A year took about a fifth longer (2062 to 2068: 3,078 s against 2,253 s,
 * both at once on one machine); with a floor of 20 (about 80 clubs) it took two fifths longer. */
static int short_clubs;              /* AI clubs below SIGN_BELOW at the last daily update */
static int lists_day = -1;           /* the game day of the last position lists, -1 none in this program */

static int squad_size(uint32_t club)
{
    int n = 0;
    for (uint32_t j = 0; j < SLOTS; j++) n += rd32(g_mem, club + 0xd7 + 4 * j) != 0;
    return n;
}

/* An AI club of a nation, as release_surplus picks it: not a national side (0x53b900), not a human's (0x600a50). */
static int ai_club(uint32_t club)
{
    if (!rd32(g_mem, club + 0x53)) return 0;                          /* as 0x83cbd4: a club with a nation */
    uint32_t a1c[1] = {club}, a5[5] = {club, 0, 1, 0, 0};
    if (rt_callback(0x53b900u, 1, a1c) || rt_unwinding) return 0;
    return !rt_callback(0x600a50u, 5, a5) && !rt_unwinding;
}

/* 1 when the club's division is a league that the game plays: its record id (+0x57 points at the record, the id at
 * +0, as the day loop reads it at 0x5cdd46..0x5cdd4d) has an object in g_comp_object_table [0xadadfc] (as
 * League::process_promotion_relegation_chain reads it at 0x689c42), below the table's slot count [0xadadf8]. The clubs of a nation whose leagues the game did not load
 * (most of the world: 5,294 of 8,139 AI clubs held fewer than 5 players in the normal game of July 2062) have none. */
static int plays_league(uint32_t club)
{
    uint32_t div = rd32(g_mem, club + 0x57), table = rd32(g_mem, 0xadadfcu);
    if (div < 0x10000u || table < 0x10000u) return 0;
    int32_t id = (int32_t)rd32(g_mem, div);
    return id >= 0 && id < (int32_t)rd32(g_mem, 0xadadf8u) && rd32(g_mem, table + 4u * (uint32_t)id) != 0;
}

static int short_club(uint32_t club)
{
    Cpu saved = cpu;
    int is = squad_size(club) < SIGN_BELOW && plays_league(club) && ai_club(club);
    if (!rt_unwinding) cpu = saved;
    return is;
}

static int game_day(void)
{
    return (int16_t)rd16(g_mem, 0xae2c92u) * 366 + (int16_t)rd16(g_mem, 0xae2c90u);   /* year and day of the year */
}

/* Not original code: a Tier A step (0.6.2 open item 78). Tier A has no AI sales, so squads only grew: new players
 * and youth joined, and nobody left before his contract ended. In July 2108 335 of 8,107 clubs had all 50 squad
 * slots (+0xd7) filled against 29 in the normal game (which sells about 18 % of a big squad a year: clubs at 45
 * slots or more, 2020/21). The top-up 0x90dae0 then has no slot for a virtual player, and a reserve side under the
 * rule "3 over 21" (0x8cf7e0 type 3, the age byte +0x18 compared at 0x8cf9a8) fielded 10 or fewer: Albion Rovers in
 * January 2045 of a 2013 start (match_eng.cpp 652), Motherwell in September 2041 of a 2001 start (612). So an AI
 * club with all its squad slots filled lets its weakest players over 21 go (current ability, player row +0x05, as
 * the national fill reads it) until RELEASE_TO remain. The release is the original's, with the checks that
 * Shortlist_Manager::assess_club_squad 0x83cb40 makes before its own release (cited in the loop):
 * queue_free_transfer 0x8c16b0 (staff, 0, -1, as at 0x83db6b) unless already queued (0x8c1870), and
 * TRANSFER_MANAGER::daily_update 0x8c5880 releases the queue the same day (process_free_transfer_queue 0x8c18c0 at
 * 0x8c599c: the payoff, news, history, the squad slots and the contract end, 0x8c1af0). It runs for every club
 * before each daily update (the wrap below), only in play. */
static void release_surplus(uint32_t club)
{
    uint32_t sp[SLOTS];
    int n = 0;
    for (uint32_t j = 0; j < SLOTS; j++) {
        uint32_t s = rd32(g_mem, club + 0xd7 + 4 * j);
        if (s) sp[n++] = s;
    }
    if (n < SLOTS || !ai_club(club)) return;                          /* as 0x83cbd4, 0x83cbc8 and 0x83cbdf */
    uint32_t tm = rd32(g_mem, TRANSFERS), contracts = rd32(g_mem, CONTRACTS);
    int left = n, queued = 0;
    while (left > RELEASE_TO && n > 0 && !rt_unwinding) {
        int best = -1, best_ca = 0x7fffffff;
        for (int k = 0; k < n; k++) {
            uint32_t pr = rd32(g_mem, sp[k] + 0x61);
            if (!pr || (int8_t)g_mem[sp[k] + 0x18] <= 21) continue;    /* the age byte, as 0x8cf9a8 reads it */
            int ca = (int16_t)rd16(g_mem, pr + 5);
            if (ca < best_ca) best = k, best_ca = ca;
        }
        if (best < 0) break;
        uint32_t s = sp[best], a1[1] = {s};
        sp[best] = sp[--n];
        /* The checks of assess_club_squad before its release: no loan contract (0x4cf6d0 at 0x83d8d4), at this club
         * (+0x39, 0x83d8f9) with a contract there (0x4cf5f0 at 0x83d909), no offer for him (0x8c5cf0 at 0x83dafa,
         * 0x8c5e40 at 0x83db0f). queue_free_transfer shows an error box for a loanee (0x8c16c8, 0x8c1722). */
        if (call_this(0x4cf6d0u, contracts, 1, a1) || rt_unwinding || rd32(g_mem, s + 0x39) != club) continue;
        uint32_t a2[2] = {s, club}, a3o[3] = {s, 0, 0}, a2o[2] = {s, 0};
        if (!call_this(0x4cf5f0u, contracts, 2, a2) || rt_unwinding) continue;
        if (call_this(0x8c5cf0u, tm, 3, a3o) || rt_unwinding || call_this(0x8c5e40u, tm, 2, a2o) || rt_unwinding)
            continue;
        left--;                                                        /* he leaves, queued now or before */
        if (call_this(0x8c1870u, tm, 1, a1) || rt_unwinding) continue;
        uint32_t a3[3] = {s, 0, 0xffffffffu};
        call_this(0x8c16b0u, tm, 3, a3);
        queued++;
    }
    if (queued && !rt_unwinding)
        fprintf(stderr, "recomp: Tier A: %.50s has %d squad slots filled, lets %d players over 21 go\n",
                (char *)(g_mem + club + 4), SLOTS, queued);
}

/* TRANSFER_MANAGER::daily_update 0x8c5880, thiscall (date, slice), ret 8, called at 0x5cd3ee in the day loop. */
void o_008c5880(void);

RT_WRAP(008c5880, tier_a_transfer_daily_update)
{
    if (tier_a_in_play()) {
        Cpu saved = cpu;
        uint32_t base = rd32(g_mem, CLUB_TABLE);
        int clubs = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT);  /* national sides last */
        short_clubs = 0;
        for (int i = 0; i < clubs && !rt_unwinding; i++) {
            uint32_t club = base + (uint32_t)i * CLUB_SIZE;
            release_surplus(club);
            if (!rt_unwinding && squad_size(club) < SIGN_BELOW && plays_league(club) && ai_club(club)) short_clubs++;
        }
        if (rt_unwinding) return;
        cpu = saved;
    }
    o_008c5880();
}

/* update_club_shortlist and make_club_bids, thiscall (club), ret 4: only for a short club (short_club). */
RT_WRAP(0083a730, tier_a_update_club_shortlist)
{
    int run = tier_a_in_play() && short_club(rt_arg(0));
    if (rt_unwinding) return;
    if (run || !skip(1, 0)) o_0083a730();
}

RT_WRAP(0084a020, tier_a_make_club_bids)
{
    int run = tier_a_in_play() && short_club(rt_arg(0));
    if (rt_unwinding) return;
    if (run || !skip(1, 0)) o_0084a020();
}

/* The position lists of the day loop, only while a club is short. The build of 0x5cda46 keeps its days. The sort of
 * 0x5cda56 runs only on the day of a build: it first builds the lists when they are older than LISTS_EVERY days
 * (none in this program, a load of an earlier save, or no club was short on the day of a build). A short club so
 * reads lists at most LISTS_EVERY days old, as in the normal game, and the daily sort of the whole world (most of
 * the market's time, run-logs/future-3/profile.md) runs once in LISTS_EVERY days. */
RT_WRAP(00839e60, tier_a_build_position_lists)
{
    if (tier_a_in_play() && short_clubs && rd32(g_mem, cpu.esp) == 0x5cda4bu) lists_day = game_day();
    else if (skip(1, 0x5cda4b)) return;
    o_00839e60();
}

RT_WRAP(0083a780, tier_a_sort_position_lists)
{
    if (tier_a_in_play() && short_clubs && rd32(g_mem, cpu.esp) == 0x5cda5bu) {
        int age = game_day() - lists_day;              /* below 0 after a load of an earlier save in this program */
        if (lists_day < 0 || age < 0 || age >= LISTS_EVERY) {
            Cpu saved = cpu;
            uint32_t a[1] = {0};
            cpu.ecx = rd32(g_mem, rd32(g_mem, TRANSFERS) + 0x856);   /* the this of 0x5cda38..0x5cda40 */
            rt_callback(0x839e60u, 1, a);
            if (rt_unwinding) return;
            cpu = saved;
            lists_day = game_day();
        }
        if (lists_day == game_day()) {
            o_0083a780();
            return;
        }
    }
    if (!skip(0, 0x5cda5b)) o_0083a780();
}

RT_WRAP(00842c00, tier_a_add_to_shortlists)
{
    if (!skip(2, 0)) o_00842c00();
}

RT_WRAP(0084be20, tier_a_background_matches)
{
    if (!skip(0, 0)) o_0084be20();
}

RT_WRAP(0069aba0, tier_a_consider_sacking)
{
    if (!skip(2, 0)) o_0069aba0();
}

RT_WRAP(006b66a0, tier_a_can_quick_simulate)
{
    o_006b66a0();
    if (tier_a_in_play()) cpu.eax = 1;
}

/* The random numbers of the instant result: splitmix64, the same sequence in each program. The state is not in the
 * save, so each leg of a soak starts the sequence again after its load. */
static uint64_t rng_state = 0x7469657241ull;

static double rng_unit(void)
{
    uint64_t z = (rng_state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    z ^= z >> 31;
    return (double)(z >> 11) / 9007199254740992.0;
}

static int poisson(double mean)
{
    double l = exp(-mean), p = 1.0;
    int k = 0;
    do { k++; p *= rng_unit(); } while (p > l && k < 15);
    return k - 1;
}

static int binomial(int n, double p)
{
    int k = 0;
    for (int i = 0; i < n; i++) k += rng_unit() < p;
    return k;
}

/* The mean goals of each side from the two reputations (s16 at club +0x80): about 1.4 and 1.1 between equal
 * sides, and the stronger side scores more as the gap grows. */
static void means(uint32_t f, double *home, double *away)
{
    uint32_t hc = rd32(g_mem, f + 0x1c), ac = rd32(g_mem, f + 0x20);
    int hr = hc ? (int16_t)rd16(g_mem, hc + 0x80) : 0, ar = ac ? (int16_t)rd16(g_mem, ac + 0x80) : 0;
    double d = (hr - ar) / 2500.0;
    if (d > 2.0) d = 2.0;
    if (d < -2.0) d = -2.0;
    *home = 1.4 * exp(0.55 * d);
    *away = 1.1 * exp(-0.55 * d);
}

static int s8(uint32_t a) { return (int8_t)rd8(g_mem, a); }

/* Penalties F+0x49/+0x4a as 0x6c0116..0x6c018f: two numbers from 0 to 4, and a coin when they are equal. */
static void penalties(uint32_t f)
{
    int h = (int)(rng_unit() * 5), a = (int)(rng_unit() * 5);
    if (h == a) { if (rng_unit() < 0.5) h++; else a++; }
    wr8(g_mem, f + 0x49, (uint32_t)h);
    wr8(g_mem, f + 0x4a, (uint32_t)a);
}

RT_WRAP(006beeb0, tier_a_quick_simulate_match)
{
    if (!tier_a_in_play()) { o_006beeb0(); return; }
    uint32_t eng = cpu.ecx, f = rd32(g_mem, eng + 0x4792);
    double mh, ma;
    means(f, &mh, &ma);
    int h = poisson(mh), a = poisson(ma);
    wr8(g_mem, f + 0x45, (uint32_t)binomial(h, 0.45));
    wr8(g_mem, f + 0x46, (uint32_t)binomial(a, 0.45));
    wr8(g_mem, f + 0x43, (uint32_t)h);
    wr8(g_mem, f + 0x44, (uint32_t)a);
    int minute = 90;
    uint32_t flags = rd16(g_mem, f + 0x38);
    int legs2 = s8(f + 0x4b) != -1;
    int level = legs2 ? h + s8(f + 0x4b) == a + s8(f + 0x4c) : h == a;
    if (level) {
        if (flags & 0x82) {                         /* extra time */
            minute = 120;
            int eh = poisson(mh / 3), ea = poisson(ma / 3);
            if ((flags & 0x80) && eh + ea > 1) {    /* golden goal (0x6bfeb9 sets the flag of it): the first goal
                                                       ends the game */
                int home_first = rng_unit() * (eh + ea) < eh;
                eh = home_first;
                ea = !home_first;
            }
            h += eh;
            a += ea;
            wr8(g_mem, f + 0x47, (uint32_t)h);
            wr8(g_mem, f + 0x48, (uint32_t)a);
            if (legs2) {
                level = h + s8(f + 0x4b) == a + s8(f + 0x4c);
                if (level && (flags & 0x4000)) level = a == s8(f + 0x4b);
            } else
                level = h == a;
            if (level && (flags & 0x21)) penalties(f);
        } else if (legs2 && !(flags & 4) && a != s8(f + 0x4b) && !(flags & 0x4000)) {
            /* away goals decide */
        } else if (flags & 0x21)
            penalties(f);
    }
    if (legs2) {                                    /* the aggregate, 0x6c0192..0x6c01c9 */
        int last_h = s8(f + 0x47) != -1 ? s8(f + 0x47) : s8(f + 0x43);
        int last_a = s8(f + 0x47) != -1 ? s8(f + 0x48) : s8(f + 0x44);
        wr8(g_mem, f + 0x4b, (uint32_t)(s8(f + 0x4b) + last_h));
        wr8(g_mem, f + 0x4c, (uint32_t)(s8(f + 0x4c) + last_a));
    }
    wr8(g_mem, eng + 0xf5bd, (uint32_t)h);
    wr8(g_mem, eng + 0xf5f3, (uint32_t)a);
    wr16(g_mem, eng + 0x8ed0, (uint32_t)minute);
    rt_ret_stdcall(0, 0);
}
