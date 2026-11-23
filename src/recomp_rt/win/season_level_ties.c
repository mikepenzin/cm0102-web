/* Level clubs whose head-to-head order is still open (0.6.2 open item 167): the pure test. The game part, with the
 * reasons and the addresses, is season_level_ties_game.c. */
#include "season_level_ties.h"

int level_ties_open(const LevelRow *rows, int n, int matches, int win)
{
    if (!rows || n < 3) return 0;
    for (int i = 0; i < n; i++) {
        if (rows[i].played < matches) continue;
        int p = rows[i].points, pair = 0;
        for (int j = i + 1; j < n && !pair; j++)
            pair = rows[j].played >= matches && rows[j].points == p;
        if (!pair) continue;
        /* A club with matches left on p can still leave p; one below p can still end on p with w wins and d draws
         * (a draw is 1 point) from its matches left. */
        for (int k = 0; k < n; k++) {
            int left = matches - rows[k].played, need = p - rows[k].points;
            if (left <= 0 || need < 0) continue;
            if (need == 0) return 1;
            for (int w = 0; w <= left && w * win <= need; w++)
                if (need - w * win <= left - w) return 1;
        }
    }
    return 0;
}
