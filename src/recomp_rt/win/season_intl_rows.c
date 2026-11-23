/* The host fields that the original's fixed host rows leave unset (0.6.2 open item 103).
 *
 * The constructor HostCountry 0x5f97c0 takes the 57 rows of 0x22 bytes from malloc(0x792) (0x5f98b8..0x5f98c3),
 * which does not clear them. HostCountry::init_default_table 0x5f9970 writes all six host fields of the World Cup
 * and Euro rows (rows 0 to 15) and only the first two (+6 and +0xa) of rows 16 to 56, which are all fixed (choice
 * 0xfd from 0x5f9eef). find_hosts 0x5fa730 reads those two of a fixed row (0x5fa7a0..0x5fa7a9), and daily_update
 * 0x5fa850 and post_host_news 0x5faec0 read no other, so the original never sees the other four. They hold what
 * the heap held before: 0 (Afghanistan) in the England, Spain and Brazil game of the run set, other values in the
 * games with all nations. season_intl_host_ids reads all six fields of a fixed row (slice 29), so a 0 there
 * made Afghanistan a third host of the Africa Cup of Nations 2002. ctest season_intl_rows. */
#include "season_intl.h"

#include <string.h>

int season_intl_rows_clean(uint8_t *rows, int count)
{
    int changed = 0;
    for (int i = 0; i < count; i++) {
        uint8_t *row = rows + (size_t)i * SEASON_INTL_ROW_SIZE;
        if (row[SEASON_INTL_ROW_CHOICE] != SEASON_INTL_CHOICE_FIXED) continue;
        uint8_t *rest = row + SEASON_INTL_ROW_PAIRS + 2 * 4, none[4 * 4];
        memset(none, 0xff, sizeof none);
        if (!memcmp(rest, none, sizeof none)) continue;
        memcpy(rest, none, sizeof none);
        changed++;
    }
    return changed;
}

/* ---- The table's host at the draw of an edition of 2001/02 (0.6.2 item 181) ----
 *
 * A 2001 game keeps the original's rows until its first season change (Q152). The row of the Africa Cup of Nations
 * 2002 names Egypt (key 0x9cf2d8 at 0x5f9f19, fixed 0xfd at 0x5f9f2f), where Mali hosted (en.wikipedia.org, "2002
 * African Cup of Nations": Mali, 19 January to 10 February 2002). At the draw the row takes the table's hosts in
 * pair 2 with choice 1. find_hosts 0x5fa730 gives the pair of a choice 0 to 2 (0x5fa763..0x5fa79e), as the venue
 * (0x85c61c) and the entrants (0x40233e) ask it, and season_intl_host_ids reads that pair too. daily_update 0x5fa850
 * draws only rows of choice 0xff or 0xfe (0x5faa96..0x5faaae), so it keeps the pair. Pair 1 keeps the original's
 * hosts, which hosts_apply copies as the row of a later edition (Q4): the undo gives that copy the original's form. */

static int32_t field(const uint8_t *row, int k)
{
    int32_t v;
    memcpy(&v, row + SEASON_INTL_ROW_PAIRS + 4 * k, 4);
    return v;
}

static void put_field(uint8_t *row, int k, int32_t v) { memcpy(row + SEASON_INTL_ROW_PAIRS + 4 * k, &v, 4); }

/* 1 when the valid hosts of fields from..to of a row are the n hosts of ids, in any order. */
static int same_hosts(const uint8_t *row, int from, int to, const int32_t *ids, int n)
{
    int count = 0;
    for (int k = from; k < to; k++) {
        int32_t v = field(row, k), in = 0;
        if (v < 0) continue;
        for (int j = 0; j < n && !in; j++) in = ids[j] == v;
        if (!in) return 0;
        count++;
    }
    return count == n;
}

static int unset(const uint8_t *row, int from, int to)
{
    for (int k = from; k < to; k++)
        if (field(row, k) != -1) return 0;
    return 1;
}

int season_intl_row_draw_hosts(uint8_t *row, const int32_t *ids, int n)
{
    if (n < 1 || n > 2 || row[SEASON_INTL_ROW_CHOICE] != SEASON_INTL_CHOICE_FIXED || !unset(row, 2, 6) ||
        same_hosts(row, 0, 2, ids, n))
        return 0;
    for (int k = 0; k < 2; k++) put_field(row, 2 + k, k < n ? ids[k] : -1);
    row[SEASON_INTL_ROW_CHOICE] = 1;
    return 1;
}

int season_intl_row_undo_draw_hosts(uint8_t *row, const int32_t *ids, int n)
{
    if (n < 1 || n > 2 || row[SEASON_INTL_ROW_CHOICE] != 1 || !unset(row, 4, 6) || !same_hosts(row, 2, 4, ids, n) ||
        unset(row, 0, 2) || same_hosts(row, 0, 2, ids, n))
        return 0;
    for (int k = 2; k < 4; k++) put_field(row, k, -1);
    row[SEASON_INTL_ROW_CHOICE] = SEASON_INTL_CHOICE_FIXED;
    return 1;
}
