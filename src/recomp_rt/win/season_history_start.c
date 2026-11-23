/* The start-year clear of the history rows of the data (season_history_start.h).
 *
 * The game writes the row of its start-year edition at the creation (League::record_season_honours 0x684640, the
 * call of 0x4afce0 at 0x684771), and the data row of that edition goes. One row of the label goes; two or more
 * stay (the halves of a split season under one label).
 *
 * The Brazilian region records (season_bra_states.c) can hold the champions of several states. When the data holds
 * a row of the state that the game plays on the record (Autumn 2013: Ceara / Guarany under the Cearense, with
 * Botafogo-PB / Treze and CRB / ASA), only that row is the game's edition: it goes and the other states' rows stay,
 * neither cleared nor counted (open item 176). The same holds when the label has a row whose state is not known.
 * When every row of the label is of another state (3.9.68: the Paraense record holds Alagoas, ASA / CSA, and the
 * Cearense record Amazonas, Atletico Rio Negro / Nacional AM, one row a year), the record's rows are its edition of
 * that year, whatever the state, and the rule above applies to them (open item 189: else the start year held two
 * champions). */
#include "season_history_start.h"

int season_history_start_clear(int n, const int *state, unsigned char *clear)
{
    int pass = 0, same = 0, k = 0;       /* pass: the other states' rows stay out of the test */
    for (int i = 0; i < n; i++) pass |= state[i] != 1;
    for (int i = 0; i < n; i++) same += !(pass && state[i] == 1);
    for (int i = 0; i < n; i++) {
        clear[i] = same < 2 && !(pass && state[i] == 1);
        k += clear[i];
    }
    return k;
}
