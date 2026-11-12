/* The match grid guard: MATCH_PLAYER::move_to_grid_cell 0x6f46b0, thiscall (x, y), ret 8. A player at a cell
 * (+0x102 x, +0x103 y, slot +0x28 >= 0) leaves it: the cell of the engine (+0x19e) at 0x215e + (x * 12 + y) * 90
 * counts the players of each side (+0x27) at +0x58. With a count of 0 the original shows match_pl.cpp 4885
 * (0x6f4731..0x6f4786) and returns without the move. Gate (b) met it once (2021 start, 14 October 2023, after
 * the July 2023 load); three replays of that save and 20 short games with five substitutes forced in every match
 * (about 81,000 matches, RECOMP_TEST_SUBS_USED below) gave no such cell. No 0.6.0 code moves a player on the
 * grid: the engine makes every substitution itself, and the season rules only set the counts, which init_match
 * caps at 9. In a season game the guard logs the match and the caller. For a player of the match it moves him
 * as one in no cell (slot -1, the original's own path at 0x6f4819), so the counts stay right and no modal box
 * stops the game. The full play of gate (b) run 7 (2021 start, day 287 of 2023) met two calls whose object is no
 * player of a match (side 49 and 41, slot 100 and 101, no fixture): for such an object the guard does what the
 * original does after its box (no move, ret 8), with no write through its fields. */

#include "win.h"
#include "season.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void o_006f46b0(void);

static void name_at(uint32_t at, char *out)
{
    out[0] = 0;
    if (rt_readable(at)) memcpy(out, g_mem + at + 4, 51), out[51] = 0;
}

static int readable(uint32_t p) { return rt_readable(p); }

RT_WRAP(006f46b0, match_grid_guard)
{
    uint32_t pl = cpu.ecx;
    if ((int8_t)g_mem[pl + 0x28] > -1 && season_code_on() &&
        (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules())) {
        int x = (int8_t)g_mem[pl + 0x102], y = (int8_t)g_mem[pl + 0x103], side = (int8_t)g_mem[pl + 0x27];
        int slot = (int8_t)g_mem[pl + 0x28];
        uint32_t engine = rd32(g_mem, pl + 0x19e);
        uint32_t fixture = readable(engine) ? rd32(g_mem, engine + 0x4792) : 0;
        /* A player of a match: side 0 or 1, a cell with x >= 0 and y from 0 to 11 (x * 12 + y, 0x6f46f7), a slot
         * in the cell below 12 (a cell holds at most 11 a side, 0x6f484d), and an engine with its fixture. */
        int player = (side == 0 || side == 1) && x >= 0 && y >= 0 && y < 12 && slot < 12 && readable(engine) &&
                     readable(fixture);
        uint32_t cell = engine + 0x215eu + (uint32_t)((x * 12 + y) * 90);
        if (!player || !g_mem[cell + (uint32_t)side + 0x58]) {
            char comp[52], home[52], away[52];
            name_at(readable(fixture) ? rd32(g_mem, fixture + 0x14) : 0, comp);
            name_at(readable(fixture) ? rd32(g_mem, fixture + 0x1c) : 0, home);
            name_at(readable(fixture) ? rd32(g_mem, fixture + 0x20) : 0, away);
            fprintf(stderr, "recomp: match grid: %s, %s v %s, word +0x4d %04x: %s of side %d (slot %d) leaves "
                            "cell %d,%d (to %d,%d) whose count is 0, from %08x, object %08x; bytes", comp, home, away,
                    readable(fixture) ? rd16(g_mem, fixture + 0x4d) : 0, player ? "a player" : "no player",
                    side, slot, x, y, (int8_t)rt_arg(0), (int8_t)rt_arg(1), rd32(g_mem, cpu.esp), pl);
            for (uint32_t i = 0; i < 0x30; i += 4) fprintf(stderr, " %08x", rd32(g_mem, pl + i));
            fputc('\n', stderr);
            if (!player) {                     /* as the original after its box: no move, ret 8 */
                rt_ret_stdcall(0, 2);
                return;
            }
            g_mem[pl + 0x28] = 0xff;           /* in no cell: the original adds him to the new cell */
        }
    }
    o_006f46b0();
}

/* Test switch RECOMP_TEST_SUBS_USED=n (1 to 9): CM3_MATCH_ENGINE::init_match 0x6b7f20, thiscall (fixture, ...),
 * sets the substitutes used of every match (the high nibble of fixture +0x4d, read and capped at 9 at
 * 0x6b8006..0x6b802a) before the original, for the grid test of match_pl.cpp 4885. With RECOMP_CLUB_LOG the
 * program prints the count of matches at exit. */
void o_006b7f20(void);

static int matches;

static void matches_at_exit(void) { fprintf(stderr, "recomp: match grid: %d matches started\n", matches); }

RT_WRAP(006b7f20, match_subs_test)
{
    static int used = -1;
    if (used < 0) {
        const char *v = getenv("RECOMP_TEST_SUBS_USED");
        used = v ? atoi(v) : 0;
        if (getenv("RECOMP_CLUB_LOG")) atexit(matches_at_exit);
    }
    uint32_t fixture = rt_arg(0);
    matches++;
    /* A fixture made before the MLS fix (an older save's round table, season_mls.c) still holds flag 0x20 at +0x38
     * (the run-in shootout, period_completed 0x6bdb1c): in a season game it plays as extra time and penalties. */
    if (fixture >= 0x10000u && (rd16(g_mem, fixture + 0x38) & 0x20u))
        wr16(g_mem, fixture + 0x38, rt_mls_round_flags(rd16(g_mem, fixture + 0x38)));
    if (used > 0 && used <= 9 && fixture >= 0x10000u)
        wr16(g_mem, fixture + 0x4d, (uint16_t)((rd16(g_mem, fixture + 0x4d) & 0xff0fu) | (uint32_t)(used << 4)));
    o_006b7f20();
}

/* Test log (RECOMP_CLUB_LOG=1): the penalty takers of a shootout, CM3_MATCH_ENGINE 0x6be520, thiscall (). It sets
 * the 11 takers of each side to -1 (+0x1af, 0x6be538..0x6be55e), then fills them from the tactics of a human's side
 * (0x898040) or the best player left (0x6ceb90). run_penalty_shootout_step takes the player of order entry
 * kick % 11 (+0x1af + side * 11, 0x6be931..0x6be96a): an entry still -1 gives an object before the side's players.
 * The line names a shootout that leaves such an entry. It only reads. */
void o_006be520(void);

RT_WRAP(006be520, shootout_order_log)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_CLUB_LOG") != NULL;
    uint32_t engine = cpu.ecx;
    o_006be520();
    if (!on || rt_unwinding) return;
    uint32_t fixture = rd32(g_mem, engine + 0x4792);
    char comp[52], home[52], away[52];
    name_at(readable(fixture) ? rd32(g_mem, fixture + 0x14) : 0, comp);
    name_at(readable(fixture) ? rd32(g_mem, fixture + 0x1c) : 0, home);
    name_at(readable(fixture) ? rd32(g_mem, fixture + 0x20) : 0, away);
    fprintf(stderr, "recomp: match grid: shootout of %s, %s v %s, engine %08x, word +0x4d %04x; order", comp, home,
            away, engine, readable(fixture) ? rd16(g_mem, fixture + 0x4d) : 0);
    for (uint32_t k = 0; k < 22; k++) fprintf(stderr, " %d", (int8_t)g_mem[engine + 0x1af + k]);
    fputc('\n', stderr);
}

/* Test log (RECOMP_CLUB_LOG=1): CM3_MATCH_ENGINE::run_penalty_shootout_step 0x6be710, thiscall (int), ret 4. The
 * kicks of each side count in signed bytes (+0x8e9f, +0x8ea0, 0x6be984..0x6be998). The line shows a shootout that
 * passes 30 kicks a side, every 10 kicks. It only reads. */
void o_006be710(void);

RT_WRAP(006be710, shootout_step_log)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_CLUB_LOG") != NULL;
    uint32_t engine = cpu.ecx;
    int kicks = (int8_t)g_mem[engine + 0x8e9f];
    if (on && ((kicks >= 30 || kicks < 0) ? kicks % 10 == 0 : getenv("RECOMP_TEST_SHOOTOUT_ALL") != NULL) &&
        g_mem[engine + 0x8e9e] == 0) {
        fprintf(stderr, "recomp: match grid: shootout at %d kicks, engine %08x, state", kicks, engine);
        for (uint32_t i = 0x8e9e; i <= 0x8eb4; i++) fprintf(stderr, " %02x", g_mem[engine + i]);
        fprintf(stderr, ", goals %d %d, taker positions", g_mem[engine + 0xf5bc], g_mem[engine + 0xf5f2]);
        for (uint32_t s = 0; s < 2; s++) {
            fprintf(stderr, " side %u:", s);
            for (uint32_t k = 0; k < 11; k++) {
                int idx = (int8_t)g_mem[engine + 0x1af + s * 11 + k];
                uint32_t pl = engine + 0x4796u + (uint32_t)((int)s * 20 + idx) * 0x1beu;
                fprintf(stderr, " %d", (int8_t)g_mem[pl + 0x2d]);
            }
        }
        fputc('\n', stderr);
    }
    o_006be710();
}
