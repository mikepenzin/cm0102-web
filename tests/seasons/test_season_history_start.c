/* The start-year clear of the history rows of the data (open items 176 and 189, season_history_start.c). Real life:
 * one champion and one runner-up for each edition of a state championship. ctest season_history_start. */
#include "season_history_start.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int n, const int *state, const char *want, const char *text)
{
    unsigned char clear[8];
    char got[9];
    memset(clear, 0, sizeof clear);
    season_history_start_clear(n, state, clear);
    for (int i = 0; i < n; i++) got[i] = clear[i] ? 'x' : '.';
    got[n] = 0;
    int ok = strcmp(got, want) == 0;
    printf("%s  %s (cleared %s, want %s)\n", ok ? "ok  " : "FAIL", text, got, want);
    if (!ok) failures++;
}

int main(void)
{
    /* Autumn 2013, Cearense 2013: Botafogo-PB / Treze, CRB / ASA (other states) and Ceara / Guarany (Ceara). The
     * game's Ceara row replaces only the Ceara row (item 176). */
    check(3, (const int[]){1, 1, 0}, "..x", "2013: the own state's row goes, the other states' rows stay");
    /* 3.9.68, a 2001 start: the Paraense record holds one row a year, of Alagoas (ASA / CSA 2001), and the
     * Cearense record one of Amazonas (Atletico Rio Negro / Nacional AM 2001). The data has no row of the state
     * that the game plays: that row is the record's edition of 2001 and goes (item 189). */
    check(1, (const int[]){1}, "x", "2001: the record's one row, of another state, goes");
    /* A record that is no state championship: the row of the label goes (the original rule). */
    check(1, (const int[]){-1}, "x", "one row of no state: it goes");
    /* The halves of a split season (two rows of one label): both stay. */
    check(2, (const int[]){-1, -1}, "..", "two rows of no state: a split season, both stay");
    /* Two rows of other states and none of the own state: as a split season, both stay. */
    check(2, (const int[]){1, 1}, "..", "two rows of other states, none of the own state: both stay");
    check(0, (const int[]){0}, "", "no row: nothing goes");
    /* A row of no known state with a row of another state: the unknown row is the edition and goes (as item 176). */
    check(2, (const int[]){-1, 1}, "x.", "a row of no state and one of another state: the first goes");
    check(2, (const int[]){0, -1}, "..", "a row of the own state and one of no state: a split season, both stay");
    check(2, (const int[]){0, 0}, "..", "two rows of the own state: a split season, both stay");
    check(3, (const int[]){1, 0, 0}, "...", "two rows of the own state and one of another: all stay");
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("season_history_start: all checks passed\n");
    return 0;
}
