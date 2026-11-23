/* The places of each confederation in World Cup qualifying for a World Cup of 48 teams with hosts (0.6.2 slice 5,
 * open item 59). Kept apart from season_wcq26.c, which uses it at its sites, so the unit test
 * (tests/seasons/test_season_wcq26_places.c) reads the same rule. The rule of O265 and slice 4a's fault 32: each
 * confederation keeps its 2026 total with its hosts counted in it. */
#include "season_wcq26.h"

static int at_least(int v, int floor) { return v < floor ? floor : v; }

void season_wcq26_places(const int hosts[WCQ_CONFS], int direct[WCQ_CONFS])
{
    int h[WCQ_CONFS];
    for (int c = 0; c < WCQ_CONFS; c++) h[c] = at_least(hosts[c], 0);
    /* UEFA: 12 group winners and 4 paths less its hosts (europe_paths), at least 1 path. */
    direct[WCQ_UEFA] = 12 + (h[WCQ_UEFA] >= 4 ? 1 : 4 - h[WCQ_UEFA]);
    /* CONMEBOL: the league's direct places 6 less the hosts in +0xee (set_up_rules 0x921040). */
    direct[WCQ_CONMEBOL] = at_least(6 - h[WCQ_CONMEBOL], 0);
    /* CONCACAF: 6 less its hosts of the final group of 6 (0x91785c). */
    direct[WCQ_CONCACAF] = at_least(6 - h[WCQ_CONCACAF], 0);
    /* CAF: one group winner each of 9 groups less its hosts (africa_groups), at least 5 groups. */
    direct[WCQ_CAF] = h[WCQ_CAF] >= 4 ? 5 : 9 - h[WCQ_CAF];
    /* OFC: the final's winner, none with a host (the winner goes to the play-off tournament). Each OFC host after
     * the first has no OFC place to take, so it takes one of the AFC, the confederation that shares the region. */
    direct[WCQ_OFC] = h[WCQ_OFC] ? 0 : 1;
    int ofc_more = h[WCQ_OFC] > 1 ? h[WCQ_OFC] - 1 : 0;
    /* AFC: 4 of each of its two final groups less its hosts. */
    direct[WCQ_AFC] = at_least(8 - h[WCQ_AFC] - ofc_more, 0);
}

int season_wcq26_asia_group(int places, int g)
{
    if (places < 0) places = 0;
    if (places > 8) places = 8;
    return g == 0 ? (places + 1) / 2 : places / 2;
}
