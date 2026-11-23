/* Background matches take their own numbers (open item 155, original-faults group match_numbers).
 *
 * The fault of the original: the speed of the machine changes the game. The game's random function 0x90cfc0 takes
 * each number from one pool of 51,000 words at 0xaa2454: the pointer 0xde1f60 moves one word per call, and the offset
 * 0xde1f5c is added (docs/research/2026-10-14-start-screen-helpers.md). The matches of a day that the human does not
 * watch play in steps of real time:
 *   - Between clicks. game_main_day_loop 0x5cccd0 sets the hook [0xdd6956] to 0x5d61b0 (0x5ccd05). It calls
 *     MATCH_DAY::step_live_match 0x6b55c0 with no human match (0x5d61ba) on the match day [0xb67a50] while the human
 *     is on the game's screens: each call runs one engine step of the current background match,
 *     MATCH_DAY::step_background_match 0x6b5ce0, and when that match ends it starts the next one,
 *     start_next_background_match 0x6b5a70 (0x6b55d4..0x6b55e3). A log of the random calls saw them (return
 *     0x5d61c1) on 18 and 20 July 2001 of the match check, about 145,000 steps.
 *   - While a watched match plays. 0x6b55c0 runs the next step of the human match only when the line's delay has
 *     passed on the millisecond clock 0x947299 (+0x10d plus +0x109 of the live block). In every other pass of the
 *     screen loop it runs one background step (0x6b5982).
 * Both kinds of step take numbers from the one pool, as do the human match and the screens (the background picture
 * 0x41bfb0 takes three numbers at each new picture when Background Changes is on). So the count of passes between
 * two clicks, or in a line's delay, decides which numbers the human match and the rest of the game get. On a busy
 * machine a pass takes longer, and the game plays another way. tests/smoke/match_timing_run.py shows it with a
 * throttle: two slow runs of 1242c594 played other matches (6 November 2026).
 *
 * The fix: each match day has a second pointer and offset into the same pool, and every call of 0x6b5ce0 and
 * 0x6b5a70 for that match day takes numbers from it. play_matches 0x6b4360 plays the background matches that are
 * left at the end of the day (0x5cd2f9) through the same two functions (its loop from 0x6b4383: 0x6b5a70, then
 * 0x6b5ce0(1)), so the background matches take the same numbers in the same order however the steps fall between
 * the clicks, and the main stream does not depend on them. The human match and all other work keep the main stream.
 * The second stream starts when the match day is built, MATCH_DAY::MATCH_DAY 0x6b3c10: the day loop calls it at its
 * entry (0x5cc957) and for each new day (0x5cd2d9), at the same place of the main stream on every machine. Its pointer
 * and offset are a hash of the main pointer, the main offset and the date (0xae2c90), so the second streams of two days
 * start at unrelated places of the pool. No number of the main stream is taken for it.
 *
 * The C library's numbers move with the timing too: a background step that starts a match can fill a club's squad,
 * start_next_background_match 0x6b5a70 calls VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0 (0x6b5bc1), and its chain
 * through 0x90dae0 and 0x90e0a0 calls rand() 0x946a04 (0x90e150, 0x90e17b) and srand() 0x9469fa (0x90de58, 0x90dfff).
 * Main work calls rand() too (0x5263bc, 0x86367f, 0x8656b4), and the random function takes a new offset from rand()
 * at the end of the pool (0x90cff1). So the second stream has its own seed of the C library (0xad6c30) as well, and
 * the original's wrap at the end of the pool works for both streams as it is. RECOMP_ORIGINAL_FAULTS=keep:match_numbers
 * turns the fix off (original_faults.c). */
#include "win.h"

#include <stdint.h>

#define RND_PTR 0xde1f60u          /* the pointer into the pool (one word on per call) */
#define RND_OFF 0xde1f5cu          /* the offset added to each word */
#define CRT_SEED 0xad6c30u         /* the seed of the C library's rand() 0x946a04 and srand() 0x9469fa */
#define POOL 0xaa2454u             /* the first word of the pool */
#define POOL_WORDS 51000u
#define DATE 0xae2c90u             /* the game date: day of the year (word), year (word) */

/* The second stream of the match day. The game has one match day at a time ([0xb67a50]); a step of another one
 * keeps the main stream. */
static struct {
    uint32_t day;                  /* the MATCH_DAY object, 0: none */
    uint32_t ptr, off, seed;       /* the second stream: pool pointer, offset, C library seed */
} second;

static int depth;                  /* > 0 while a background step runs with the second stream */

/* A 32-bit hash step (the finaliser of MurmurHash3). */
static uint32_t mix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    return h ^ (h >> 16);
}

void o_006b3c10(void);
void o_006b5ce0(void);
void o_006b5a70(void);

/* MATCH_DAY::MATCH_DAY, thiscall (date, slice): the second stream of the new match day starts here. */
RT_WRAP(006b3c10, match_day_new)
{
    uint32_t day = cpu.ecx;
    o_006b3c10();
    if (!win_match_numbers_fix_on()) return;
    uint32_t h = mix(rd32(g_mem, RND_PTR) * 0x9e3779b1u ^ rd32(g_mem, RND_OFF) * 0x85ebca6bu ^
                     rd32(g_mem, DATE) * 0xc2b2ae35u);
    second.day = day;
    second.ptr = POOL + 4u * (h % POOL_WORDS);
    second.off = (h >> 8) & 0x7fffu;           /* rand() & 0xffff, as at 0x90cff1: at most 0x7fff */
    second.seed = mix(h ^ 0x9e3779b9u);
}

/* Runs the original with the second stream in place of the main stream. */
static void with_stream(void (*original)(void))
{
    if (!depth && (!second.day || cpu.ecx != second.day)) {
        original();
        return;
    }
    uint32_t main_ptr = 0, main_off = 0, main_seed = 0;
    if (!depth++) {
        main_ptr = rd32(g_mem, RND_PTR);
        main_off = rd32(g_mem, RND_OFF);
        main_seed = rd32(g_mem, CRT_SEED);
        wr32(g_mem, RND_PTR, second.ptr);
        wr32(g_mem, RND_OFF, second.off);
        wr32(g_mem, CRT_SEED, second.seed);
    }
    original();
    if (!--depth) {
        second.ptr = rd32(g_mem, RND_PTR);
        second.off = rd32(g_mem, RND_OFF);
        second.seed = rd32(g_mem, CRT_SEED);
        wr32(g_mem, RND_PTR, main_ptr);
        wr32(g_mem, RND_OFF, main_off);
        wr32(g_mem, CRT_SEED, main_seed);
    }
}

/* MATCH_DAY::step_background_match, thiscall (MATCH_DAY, run_to_end). */
RT_WRAP(006b5ce0, step_background_match)
{
    with_stream(o_006b5ce0);
}

/* MATCH_DAY::start_next_background_match, fastcall (MATCH_DAY in ecx). */
RT_WRAP(006b5a70, start_next_background_match)
{
    with_stream(o_006b5a70);
}
