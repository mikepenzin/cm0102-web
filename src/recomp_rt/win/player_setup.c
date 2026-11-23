/* The dates of a season database's player_setup.cfg in a season game (open item 229, tools/recomp/player_setup.py).
 *
 * The Update Team databases keep their loans, bans, retirements and future transfers in player_setup.cfg, one line
 * each, with the dates of a game that starts in July 2001: October 2026 has "LOAN" "Jack" "" "Grealish" "Manchester
 * City" "Everton" 1 6 2001 15 6 2002 for a loan of 2026/27. The game reads the file at a new game:
 * player_setup_cfg_load 0x7a30c0 calls player_setup_apply_line 0x7a3a10 for each line. The parser takes years 1980 to
 * 2010 only (LOAN 0x7a4f97..0x7a4fa3, FUTURE_TRANSFER 0x7a4cac..0x7a4cb6; at most 2010 for the bans, 0x7a3d62), so
 * the file cannot hold the real years. In a game that starts after 2001 every date lies in the past:
 *   - LOAN: TRANSFER_MANAGER::add_config_loan 0x8bf400 (called at 0x7a500c) signs the loan only when its start is on
 *     or before the game date [0xae2c90] and its end on or after it. No loan was signed.
 *   - the bans, DRUGS (type 0x100), MISCONDUCT (0x1000) and FALSE_PASSPORT (0x800): the parser takes the end only when
 *     its year is at least the game's year [0xae2c92] (0x7a3d6c..0x7a3d7a, 0x7a3fd5..0x7a3fe3, 0x7a422c..0x7a423a).
 *     No ban was given.
 *   - RETIREMENT (the Date at 0x7a48b8 for PLAYER_REGEN::add_retirement 0x7a8510) and FUTURE_TRANSFER (the Date at
 *     0x7a4cd0 for 0x8bf060) were dated before the start.
 * The lines without a date (INJURY, INT_RETIREMENT) are not touched.
 *
 * The fix (tools/recomp/player_setup.py) moves the year of each of those dates after the parser's checks of the
 * file's values and before the checks against the game date, through rt_player_setup_year. A year moves by the rule
 * of the database's contract ends (season_dates_contract_shift, season_dates.c): a year of the data season D or later
 * and 2010 or later is a real year and moves by N - D; an earlier one is a year of a 2001 start and moves by the
 * database's contract shift (D - 2001 for the Update Team databases) plus N - D. So October 2026 in 2026/27 moves 2001
 * to 2026, and December 2010 SIM, whose dated lines carry the real year 2010 (its bans and retirements), moves
 * nothing in a 2010 start (its LOAN lines end in 2011, which the parser refuses with or without the fix).
 * Only the year moves. No dated line of the test-fixtures databases holds a 29 February; a future one that moved
 * into a year that is not a leap year would reach Date::Date 0x549f70 as 29 February of that year.
 *
 * It acts only in a season game (start year after 2001, season code on) whose data set has its own player_setup.cfg
 * (win_data_set_has): without it the game reads its own Data/player_setup.cfg (the 3.9.68 file of 2001 players, files.c
 * data_dir_swap), whose dates do not move. So the base game and our own databases (Summer 2002, 2004, 2006) play as
 * before. The official data sets 3.9.60 and 3.9.65 have their own file; they start only in 2001/02, where nothing
 * moves, and in a later test start (RECOMP_START_SEASON) they take N - 2001, the rule of their contract ends.
 *
 * The result is cached by start year and data set folder (a Custom database edited in place between two games of one
 * program run keeps the first plan); the folder check and the plan run once for each. Logged once for each. Test switch RECOMP_PLAYER_SETUP_DATES=original: no date moves. */
#include "win.h"
#include "../rt.h"
#include "season.h"
#include "season_dates.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int original(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("RECOMP_PLAYER_SETUP_DATES"); v = e && !strcmp(e, "original"); }
    return v;
}

/* The plan of the game that is made, 0 when nothing moves. Cached by start year and folder. */
static int plan_now(SeasonShiftPlan *out)
{
    static int cached_year = -1, cached_ok;
    static char cached_dir[1100];
    static SeasonShiftPlan cached;
    int N = season_start_year();
    if (N <= SEASON_FIRST_YEAR || !season_code_on() || original()) return 0;
    const char *dir = win_data_set_dir();
    if (N != cached_year || strcmp(dir, cached_dir)) {
        cached_year = N;
        snprintf(cached_dir, sizeof cached_dir, "%s", dir);
        cached_ok = dir[0] && win_data_set_has("player_setup.cfg") && season_dates_plan(dir, N, &cached);
        if (cached_ok) {
            const char *slash = strrchr(dir, '/');
            fprintf(stderr, "recomp: player setup: the dates of player_setup.cfg move by %d years (years before %d by "
                    "%d, real years by %d; %s, the game in %d)\n", cached.contract_old,
                    cached.D > 2010 ? cached.D : 2010, cached.contract_old, cached.contract_real,
                    slash ? slash + 1 : dir, N);
        }
    }
    *out = cached;
    return cached_ok;
}

uint32_t rt_player_setup_year(uint32_t value)
{
    SeasonShiftPlan p;
    if (!plan_now(&p)) return value;
    int y = (int16_t)value;
    return (value & 0xffff0000u) | (uint16_t)(y + season_dates_contract_shift(&p, y));
}
