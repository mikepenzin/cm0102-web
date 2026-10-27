/* Test runs: a save at a fixed game date, for the same-save check (tests/smoke/smoke_run.py
 * --same-save). Only the environment switches it on. Play is unchanged without it.
 *
 * RECOMP_SAVE_AFTER_DAYS=n: at the first human turn n or more game days after the first human turn,
 * the runtime saves the game as samesave.sav in the game folder of the original (the write goes to the
 * user folder, files.c) and ends the program with code 0.
 *
 * The human turn is FUN_005cde40 (game.cpp). The day loop FUN_005cccd0 calls it once per slice, after
 * the day's processing (docs/research/2026-10-14-assistant-and-days-inventory.md, 2a). The save is
 * FUN_0082e430(path, 0x16), the call of the Auto Save at 0x7617d8. The folder is the save folder of
 * the game options, [0xae3138] + 0xf, as the Auto Save uses it at 0x7617bb.
 *
 * In such a run the save writes zeros in place of the 0x1cc bytes that the save 0x52dad0 copies from its
 * stack into index.dat without setting them (call site index_pad, 0x53142b). Those bytes are left over
 * from earlier code, so two runs that draw different screens could save different bytes with the same
 * game state. In the same way it writes zeros in the word at the end of each row of the save's table
 * (+0x108 of the row), after the name of each row and in the unused rows, which the open of a part 0x9326e0 leaves as they were in
 * the heap (docs/research/2026-10-15-attribute-groups.md). */
#include "win.h"
#include "season.h"
#include "season_dates.h"
#include "season_history.h"
#include "season_rules.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_DATE 0xae2c90u        /* s16 day of the year, s16 year */
#define GAME_OPTIONS 0xae3138u     /* pointer to the game options; +0xf is the save folder */
#define SAVE_GAME 0x0082e430u      /* cdecl (path, flags). The meaning of the result is unknown: the
                                    * test compares the files, so the runtime only prints it. */

static int save_after = -2;        /* -2: not read yet, -1: off */
static int first_day = -1;

static int game_day(void)
{
    int day = (int16_t)rd16(g_mem, GAME_DATE), year = (int16_t)rd16(g_mem, GAME_DATE + 2);
    return year * 366 + day;       /* only differences of a few months are used */
}

void o_005cde40(void);

/* RECOMP_SCREEN_LOG: a line with the game date at a human turn 7 or more game days after the last such line. The
 * screen log (waits.c) prints a line only when the screen changes, and the stall check of the test runs reads the
 * date from those lines (tests/seasons/full_play_run.py). A manager with no club on holiday sees one news screen a
 * week, the same text when no new headline came: in the 2025 gate (b) start (leg 1, December 2026) the days went
 * on but no line came for 20 minutes, and the run stopped as a stall. Only the log changes. */
static void date_log_turn(void)
{
    static int on = -1, last = -1000000;
    if (on < 0) on = getenv("RECOMP_SCREEN_LOG") != NULL;
    int today = game_day();
    if (!on || today - last < 7) return;
    last = today;
    fprintf(stderr, "recomp: game date %d/%d at a human turn\n", (int16_t)rd16(g_mem, GAME_DATE),
            (int16_t)rd16(g_mem, GAME_DATE + 2));
}

/* The human turn, cdecl (short *last date): 1 while the turn goes on. */
RT_WRAP(005cde40, human_turn)
{
    date_log_turn();
    holiday_next_turn();
    season_log_turn();
    season_dates_log_turn();
    season_history_log_turn();
    season_report_turn();
    screen_sweep_turn();           /* RECOMP_SCREEN_SWEEP (screen_sweep.c, slice 30a) */
    if (picker_check_turn()) {     /* RECOMP_PICK_CHECK: the dump is written */
        if (rt_on_fatal) rt_on_fatal();
        SDL_Quit();
        exit(0);
    }
    if (save_after == -2) {
        const char *v = getenv("RECOMP_SAVE_AFTER_DAYS");
        save_after = v && *v ? atoi(v) : -1;
    }
    if (save_after >= 0) {
        int today = game_day();
        if (first_day < 0) first_day = today;
        if (today - first_day >= save_after) {
            char path[512];
            uint32_t options = rd32(g_mem, GAME_OPTIONS);
            snprintf(path, sizeof path, "%ssamesave.sav", (const char *)(g_mem + options + 0xf));
            uint32_t args[2] = {rt_alloc_str(path), 0x16};
            uint32_t r = rt_callback(SAVE_GAME, 2, args);
            fprintf(stderr, "recomp: saved %s after %d game days (result %u)\n", path, today - first_day, r);
            if (rt_on_fatal) rt_on_fatal();
            SDL_Quit();
            exit(0);
        }
    }
    o_005cde40();
}

void o_00932e10(void);
extern const uint32_t rt_site_index_pad;

/* The write of a part of a save, cdecl (data, size, count, file). */
RT_WRAP(00932e10, save_write)
{
    if (save_after >= 0 && rd32(g_mem, cpu.esp) == rt_site_index_pad && rt_arg(1) == 0x1cc)
        memset(g_mem + rt_arg(0), 0, 0x1cc);
    o_00932e10();
}

void o_009326e0(void);

/* The open of a part of a save, cdecl (folder, name, write, ...): the save object. It holds the count of
 * rows at +0x21 and the table of 1000 rows of 0x10c bytes at +0x25, which the close 0x932b00 writes to
 * the file whole. Unused rows are heap bytes too: they get zeros. */
RT_WRAP(009326e0, save_open)
{
    uint32_t write = rt_arg(2);
    o_009326e0();
    if (rt_unwinding || save_after < 0 || write != 1 || !cpu.eax) return;
    uint32_t count = rd32(g_mem, cpu.eax + 0x21), table = cpu.eax + 0x25;
    for (uint32_t row = 0; row < 1000; row++) {
        uint32_t r = table + row * 0x10c;
        if (row < count) {
            size_t name = strnlen(win_str(r + 8), 0x100);   /* the bytes after the name */
            memset(g_mem + r + 8 + name, 0, 0x100 - name);
            wr32(g_mem, r + 0x108, 0);
        } else {
            memset(g_mem + r, 0, 0x10c);
        }
    }
}
