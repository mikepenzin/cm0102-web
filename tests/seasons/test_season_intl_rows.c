/* The original's host rows (0.6.2 open item 103, src/recomp_rt/win/season_intl_rows.c).
 *
 * The constructor HostCountry 0x5f97c0 takes the 57 rows from malloc(0x792) (0x5f98b8..0x5f98c3), which does not
 * clear them, and HostCountry::init_default_table 0x5f9970 writes all six host fields of the first 16 rows (World
 * Cup and Euro) but only the first two of rows 16 to 56, which are all fixed (choice 0xfd). find_hosts 0x5fa730
 * reads those two of a fixed row, so the original never sees the other four. season_intl_host_ids reads all six
 * of a fixed row. In the a2001esb game the other four held 0, which is Afghanistan, so Afghanistan played the
 * Africa Cup of Nations 2002 as a third host. The clean sets those four fields of each fixed row to -1 (no host)
 * and leaves every other byte. The game calls it only while the table holds the original's 57 rows (season_intl.c
 * hosts_unset_clean). ctest season_intl_rows. */
#include "season_intl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

#define ROW SEASON_INTL_ROW_SIZE
#define CHOICE SEASON_INTL_ROW_CHOICE
#define FIXED SEASON_INTL_CHOICE_FIXED

static void put32(uint8_t *p, int32_t v) { memcpy(p, &v, 4); }
static int32_t get32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }

/* A row: key, year, six host fields, choice, decision day, cycle. */
static void row(uint8_t *r, int32_t key, int16_t year, const int32_t pairs[6], uint8_t choice, uint16_t day,
                uint8_t cycle)
{
    put32(r, key);
    memcpy(r + 4, &year, 2);
    for (int k = 0; k < 6; k++) put32(r + 6 + 4 * k, pairs[k]);
    r[CHOICE] = choice;
    memcpy(r + 0x1f, &day, 2);
    r[0x21] = cycle;
}

int main(void)
{
    uint8_t rows[4 * ROW], before[sizeof rows];
    /* The rows of the a2001esb save (July 2011): the Africa Cup of Nations 2002 (Egypt, then 0 for Afghanistan),
     * the OFC club row of every year (New Zealand twice), a World Cup row as drawn rows have it, and a fixed row of
     * the a2001s1 save with other leftovers. */
    row(rows, 0x1c3, 2002, (const int32_t[6]){59, -1, 0, 0, 0, 0}, FIXED, 169, 0x10);
    row(rows + ROW, 0x1b5, 0, (const int32_t[6]){134, 134, 0, 0, 0, 0}, FIXED, 13, 0);
    row(rows + 2 * ROW, 0x1c1, 2006, (const int32_t[6]){74, -1, 70, -1, 192, -1}, 0xfe, 200, 0x20);
    row(rows + 3 * ROW, 0x147, 0, (const int32_t[6]){74, -1, 28428, -224, 134742271, 28429}, FIXED, 8, 0);
    memcpy(before, rows, sizeof rows);

    int changed = season_intl_rows_clean(rows, 4);
    CHECK(changed == 3, "%d rows changed, want the 3 fixed rows", changed);
    for (int i = 0; i < 4; i++) {
        const uint8_t *r = rows + i * ROW, *b = before + i * ROW;
        int fixed = b[CHOICE] == FIXED;
        CHECK(!memcmp(r, b, 0xe), "row %d: key, year and the first two host fields stay", i);
        CHECK(!memcmp(r + CHOICE, b + CHOICE, ROW - CHOICE), "row %d: choice, day and cycle stay", i);
        for (int k = 2; k < 6; k++) {
            int32_t v = get32(r + 6 + 4 * k), want = fixed ? -1 : get32(b + 6 + 4 * k);
            CHECK(v == want, "row %d host field %d: %d, want %d", i, k, v, want);
        }
    }
    CHECK(season_intl_rows_clean(rows, 4) == 0, "a second clean changes nothing");
    CHECK(season_intl_rows_clean(rows, 0) == 0, "no rows");

    /* 0.6.2 item 181: the draw of the 2002 Africa Cup of Nations in a 2001 game. The original's row (Egypt 59,
     * fixed, day 169, move 16, cleaned) takes the table's host, Mali (118), in pair 2 with choice 1: find_hosts
     * 0x5fa730 then gives Mali (0x5fa788), and pair 1 keeps Egypt for the copy at the first season change. */
    uint8_t acn[ROW], orig[ROW];
    row(acn, 0x1c3, 2002, (const int32_t[6]){59, -1, -1, -1, -1, -1}, FIXED, 169, 0x10);
    memcpy(orig, acn, ROW);
    const int32_t mali[1] = {118}, egypt[1] = {59};
    CHECK(season_intl_row_draw_hosts(acn, egypt, 1) == 0, "a row of the table's host stays");
    CHECK(!memcmp(acn, orig, ROW), "a row of the table's host: no byte changes");
    CHECK(season_intl_row_draw_hosts(acn, mali, 1) == 1, "the row of another host takes the table's host");
    CHECK(acn[CHOICE] == 1, "choice %d, want pair 2 (1)", acn[CHOICE]);
    CHECK(get32(acn + 6) == 59 && get32(acn + 10) == -1, "pair 1 keeps the original's host");
    CHECK(get32(acn + 14) == 118 && get32(acn + 18) == -1, "pair 2 holds the table's host");
    CHECK(get32(acn + 22) == -1 && get32(acn + 26) == -1, "pair 3 holds none");
    CHECK(!memcmp(acn, orig, 6) && !memcmp(acn + 0x1f, orig + 0x1f, ROW - 0x1f), "key, year, day and move stay");
    CHECK(season_intl_row_draw_hosts(acn, mali, 1) == 0, "a second draw changes nothing");
    CHECK(season_intl_rows_clean(acn, 1) == 0, "the clean of open item 103 leaves the drawn row");
    /* The copy at the first season change (hosts_apply) takes the original's row back. */
    uint8_t copy[ROW];
    memcpy(copy, acn, ROW);
    CHECK(season_intl_row_undo_draw_hosts(copy, mali, 1) == 1, "the copy takes the original's row");
    CHECK(!memcmp(copy, orig, ROW), "the copy equals the original's row");
    CHECK(season_intl_row_undo_draw_hosts(copy, mali, 1) == 0, "a second undo changes nothing");
    CHECK(season_intl_row_undo_draw_hosts(orig, mali, 1) == 0, "the original's row stays");

    /* The World Cup 2002 row (0x5f9a1f..0x5f9a55): South Korea 70 and Japan 74 drawn as pair 1 (choice 0); the table
     * names both, in another order. */
    uint8_t wc[ROW], wc0[ROW];
    row(wc, 0x1c1, 2002, (const int32_t[6]){70, 74, 12, -1, 192, -1}, 0, 200, 0x20);
    memcpy(wc0, wc, ROW);
    const int32_t korea_japan[2] = {74, 70};
    CHECK(season_intl_row_draw_hosts(wc, korea_japan, 2) == 0, "the World Cup 2002 row stays");
    CHECK(season_intl_row_undo_draw_hosts(wc, korea_japan, 2) == 0, "the World Cup 2002 row is no drawn row");
    CHECK(!memcmp(wc, wc0, ROW), "the World Cup 2002 row: no byte changes");
    /* A drawn row whose pair 2 is chosen (choice 1) by the original's draw, not by the table, stays at the copy. */
    row(wc, 0x1c1, 2006, (const int32_t[6]){70, -1, 74, -1, 192, -1}, 1, 200, 0x20);
    memcpy(wc0, wc, ROW);
    CHECK(season_intl_row_undo_draw_hosts(wc, (const int32_t[1]){74}, 1) == 0, "a row of three pairs stays");
    CHECK(!memcmp(wc, wc0, ROW), "a row of three pairs: no byte changes");
    /* No room: a fixed row with more hosts, or a table edition of more than two hosts, stays. */
    row(wc, 0x1c3, 2002, (const int32_t[6]){59, 60, 61, -1, -1, -1}, FIXED, 169, 0x10);
    CHECK(season_intl_row_draw_hosts(wc, mali, 1) == 0, "a fixed row of three hosts stays");
    row(wc, 0x1c3, 2002, (const int32_t[6]){59, -1, -1, -1, -1, -1}, FIXED, 169, 0x10);
    CHECK(season_intl_row_draw_hosts(wc, (const int32_t[3]){1, 2, 3}, 3) == 0, "three table hosts: no room");
    CHECK(season_intl_row_draw_hosts(wc, mali, 0) == 0, "no table host: the row stays");

    if (failures) fprintf(stderr, "season_intl_rows: %d failures\n", failures);
    else printf("season_intl_rows: ok\n");
    return failures ? 1 : 0;
}
