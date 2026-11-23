/* The list of row A18 in the season part of a save (season_friendly_dates.c): the write puts exactly the bytes
 * that the size gives. The save (season.c add_part) allocates the size of the part and the write fills it, so
 * a write past the size overruns that heap block. In 0.6.0 the write of an empty list stored its count of 4 bytes
 * while the size was 0 (forum report, 0.6.1 item 4: a hard crash at the first save of a 2002/03 game on
 * Windows, and the save left without its season part). ctest season_friendly_part. */
#include "win.h"
#include "season.h"
#include "season_uefa_days.h"
#include "season_names.h"

#include <stdio.h>
#include <string.h>

/* The game side of season_friendly_dates.c: never called by the part functions. */
Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
uint32_t rt_callback(uint32_t target, int nargs, const uint32_t *args) { (void)target, (void)nargs, (void)args; return 0; }
void o_005cc510(void) {}
void o_00685640(void) {}
int season_start_year(void) { return 2002; }
int season_game_changes(void) { return 1; }
int season_changing_rules(void) { return 1; }
int season_day_of_year(int y, int m, int d) { (void)y, (void)m; return d; }
void season_from_day_of_year(int y, int doy, int *m, int *d) { (void)y; *m = 1, *d = doy; }
int season_fifa_day(int y, int m, int want) { (void)y, (void)m; return want; }
int season_intl_finals_busy(int conf, int y, int doy) { (void)conf, (void)y, (void)doy; return 0; }
long season_abs_day(int y, int m, int d) { return (long)y * 372 + m * 31 + d; }
int season_conmebol_real_day(long z) { (void)z; return 0; }
void season_from_abs_day(long z, int *y, int *m, int *d)
{
    *y = (int)(z / 372), *m = (int)(z % 372 / 31), *d = (int)(z % 31);
}
int season_gold_cup_busy(int y, int doy) { (void)y, (void)doy; return 0; }

void season_friendly_dates_reset(void);
uint32_t season_friendly_part_size(void);
void season_friendly_part_write(uint8_t *out);
int season_friendly_part_read(const uint8_t *in, uint32_t n);

#define ROOM 256
#define MARK 0xa5

static int failures;

static void check(int ok, const char *text)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", text);
    if (!ok) failures++;
}

/* The write leaves every byte from the size on as it was. */
static int writes_within_size(uint32_t *size)
{
    uint8_t buf[ROOM];
    memset(buf, MARK, sizeof buf);
    *size = season_friendly_part_size();
    if (*size > ROOM) return 0;
    season_friendly_part_write(buf);
    for (uint32_t i = *size; i < ROOM; i++)
        if (buf[i] != MARK) return 0;
    return 1;
}

int main(void)
{
    uint32_t size;
    char text[160];

    /* A new game with no qualifier that waits (every save of a 2002/03 start on its first day). */
    season_friendly_dates_reset();
    int within = writes_within_size(&size);
    snprintf(text, sizeof text, "an empty list: size %u, and the write puts no byte at or after it", size);
    check(size == 0 && within, text);
    check(season_friendly_part_read(NULL, 0) == 1 && season_friendly_part_size() == 0,
          "a part with no bytes for the list reads as an empty list");

    /* Two entries of 20 bytes: they read back, and the write gives the same bytes, no more. */
    uint8_t in[4 + 2 * 20];
    uint32_t count = 2;
    memcpy(in, &count, 4);
    for (uint32_t i = 0; i < 2 * 20; i++) in[4 + i] = (uint8_t)(i + 1);
    check(season_friendly_part_read(in, sizeof in) == 1, "a list of two entries reads");
    uint8_t out[ROOM];
    memset(out, MARK, sizeof out);
    size = season_friendly_part_size();
    if (size <= ROOM) season_friendly_part_write(out);
    within = writes_within_size(&size);
    snprintf(text, sizeof text, "two entries: size %u, the write gives the bytes it read and none after them", size);
    check(size == sizeof in && within && !memcmp(out, in, sizeof in), text);

    /* The reset after a load or a new game empties the list again. */
    season_friendly_dates_reset();
    within = writes_within_size(&size);
    check(size == 0 && within, "after a reset: size 0 and no byte written");

    printf("%s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
