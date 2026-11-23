/* The Irish League (Northern Ireland) at the places of its rows (0.7.2 league sizes, slice 13, items 271 and 272;
 * spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md).
 *
 * The classes write fixed places in set_up_rules:
 * - NirPrm::set_up_rules 0x78ed80: 0 up (+0xbe at 0x78edce), the relegation play-off place +0xc0 = 1 (0x78edda) and
 *   1 down (+0xc1 at 0x78ede0), the First Division below (+0x20 = [0x9cf8b4] at 0x78ee0d);
 * - NirFirst::set_up_rules 0x78bd70: 1 up (+0xbe at 0x78bdb0), the promotion play-off place +0xbf = 1 (0x78bdb6),
 *   0 down (+0xc1 at 0x78bdc2) and no division below (+0x20 = -1 at 0x78bdec).
 * The play-off is one stage of NirFirst: its constructor 0x78b010 sets the stage count +0x2c to 1 (0x78b07d) and
 * makes the stage array +0xc (0x78b08e), and create_playoff_stage 0x78c0a0 pairs the First Division row with status 1
 * (cmp byte [row + 0x37], 1 at 0x78c117; none found shows the box at 0x78c14a) with the Premier Division row with
 * status 2 (0x78c1bc). So the rows' places are written here, in the set_up_rules wrap (season_sizes_rules), as
 * season_sizes_irl.c does for the League of Ireland: the direct places of the row, and in a season whose Premier
 * Division row has no play-off both play-off places 0 and the stage count 0 (the season ends at the table).
 *
 * The First Division is the lowest division of the game. Its row's relegation places (from 2003/04) go to the
 * Northern Irish lower division of the database ("northern ireland lower division", key [0x9cf8b8]), where the
 * original keeps the nation's small clubs: NirCup::collect_entrants 0x78ac90 takes its lower entrants from there
 * (cmp ebp, [0x9cf8b8] at 0x78ad61). The size step takes clubs from there too (season_sizes.c), so the First Division
 * reaches its row (12 in 2003/04 after 4 went up, 17 in 2008/09).
 *
 * With the sizes off, or outside a season game, the original's places stand (rule Q152). */
#include "win.h"
#include "season_sizes.h"

#include <stdio.h>
#include <string.h>

#define COMP_STAGES 0xcu        /* the stage array (NirFirst::NirFirst 0x78b010 stores it at 0x78b08e) */
#define COMP_STAGE_COUNT 0x2cu  /* s32: NirFirst::NirFirst sets 1 (0x78b07d) */
#define COMP_STAGE_NOW 0x30u    /* s32: -1 before the play-off (NirFirst::start_new_season 0x78be76) */
#define COMP_YEAR 0x40u         /* s16: the edition */
#define UP 0xbeu
#define UP_PLAYOFF 0xbfu
#define DOWN_PLAYOFF 0xc0u
#define DOWN 0xc1u
#define MOST_PLACES 30          /* season_sizes.c set_places: the most places of a row */

/* The record of a key id: [0xae23d0] + id x 0x6b (as season_arg_third_record, 0x40689d..0x4068b6). */
#define RECORDS 0xae23d0u
#define RECORD_SIZE 0x6bu
#define KEY_NIR_LOWER 0x9cf8b8u /* "northern ireland lower division" (NirCup::collect_entrants 0x78ad61) */

static int readable(uint32_t p) { return rt_readable(p); }   /* as season_sizes.c */

/* The lower division of the nation of a lowest division of the game, by file. */
static const struct {
    const char *file;
    uint32_t key;
} lowers[] = {
    {"nir_first", KEY_NIR_LOWER},
};

static uint32_t record_of(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t table = rd32(g_mem, RECORDS);
    return id >= 0 && readable(table) ? table + (uint32_t)id * RECORD_SIZE : 0;
}

uint32_t season_sizes_nation_lower(const char *file)
{
    if (!file || !season_sizes_on()) return 0;
    for (size_t i = 0; i < sizeof lowers / sizeof lowers[0]; i++)
        if (!strcmp(file, lowers[i].file)) return record_of(lowers[i].key);
    return 0;
}

int season_sizes_in_nation_lower(uint32_t record)
{
    if (!season_sizes_on() || !readable(record)) return 0;
    for (size_t i = 0; i < sizeof lowers / sizeof lowers[0]; i++) {
        int32_t id = (int32_t)rd32(g_mem, lowers[i].key);
        if (id >= 0 && rd32(g_mem, record) == (uint32_t)id) return 1;
    }
    return 0;
}

/* 1 when the Premier Division's row of the edition of obj has no play-off. */
static int no_playoff(uint32_t obj)
{
    const char *p = season_sizes_row_playoffs(obj, "nir_prm");
    return p && !strcmp(p, "none");
}

static void put(uint32_t obj, uint32_t at, int n)
{
    if (n >= 0 && n <= MOST_PLACES) g_mem[obj + at] = (uint8_t)n;
}

void season_sizes_nir_rules(uint32_t obj, const char *file)
{
    if (!file) return;
    int prm = !strcmp(file, "nir_prm");
    if (!prm && strcmp(file, "nir_first")) return;
    /* With the sizes off the original's places stand. Only a save of a season with no play-off, loaded with the sizes
     * off, holds the stage count 0: it gets the class's 1 back below (no_playoff is 0, the row reads give -1). */
    if (!season_sizes_on() && prm) return;
    int none = no_playoff(obj);
    if (prm) {
        /* The row's "down" is the direct places ("1 (pos 16)"), the play-off apart ("pos 15: promotion/relegation
         * play-off"). */
        put(obj, DOWN, season_sizes_row_down(obj, file));
        if (none) g_mem[obj + DOWN_PLAYOFF] = 0;
        return;
    }
    put(obj, UP, season_sizes_row_up(obj, file));
    /* The relegation places only with changing rules on, and only as many as the step can replace from the lower
     * division (review: with changing rules off no club goes down, so the class's 0 stays). */
    put(obj, DOWN, season_sizes_nation_lower(file) ? season_sizes_lower_shown(obj, file) : -1);
    if (none) g_mem[obj + UP_PLAYOFF] = 0;
    /* Only before the play-off stage: the constructor and start_new_season call set_up_rules with +0x30 -1. A season
     * after one with no play-off gets the class's 1 back. A save of a season with no play-off holds +0x2c 0, and
     * League::load 0x687b10 then makes no stage array, so it is made here as the constructor does. */
    if ((int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) != -1) return;
    if (!none && !readable(rd32(g_mem, obj + COMP_STAGES))) {
        uint32_t arr = season_sizes_game_malloc(4);
        if (!arr) return;
        wr32(g_mem, arr, 0);
        wr32(g_mem, obj + COMP_STAGES, arr);
    }
    wr32(g_mem, obj + COMP_STAGE_COUNT, none ? 0u : 1u);
    if (none)
        fprintf(stderr, "recomp: league sizes: nir_first %d: no play-off with the Premier Division\n",
                (int16_t)rd16(g_mem, obj + COMP_YEAR));
}
