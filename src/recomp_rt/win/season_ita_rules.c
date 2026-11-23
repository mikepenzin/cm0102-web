/* The Italian rules of the league sizes that depend only on numbers (season_ita_rules.h). */
#include "season_ita_rules.h"

int ita_serie_b_zone(int year, int clubs, int up, int down, int row)
{
    if (year > ITA_SPAREGGIO_LAST || row < 0 || row >= clubs) return 0;
    if (row == 0) return 1;
    if (row < up) return 2;
    if (down > 0 && row >= clubs - down) return 3;
    return 0;
}

int ita_cup_pot(const ItaCupClub *c, int b_count)
{
    int last_b_low = b_count > 2 ? b_count - 2 : 18;
    if (!c->has_last) return 1;                                                 /* 0x6405ab */
    if (c->last_is_b && c->place >= 3 && c->place <= 6) return 4;               /* 0x6405af..0x6405be */
    if (c->last_is_a && c->cur_is_b) return 4;                                  /* 0x6405c0..0x6405cd */
    if (c->last_is_b && c->place >= 7 && c->place <= 14) return 3;              /* 0x6405d7..0x6405e6 */
    if (c->last_is_b && c->place >= 15 && c->place <= last_b_low) return 2;     /* 0x6405f2..0x640601 */
    if (!c->cur_is_b || c->last_is_cur || c->last_is_a) return 1;               /* 0x640603..0x640614 */
    return 2;
}

int ita_cup_b_count(int scanned, int season, int start)
{
    if (scanned <= 0) return 0;
    if (season <= start && scanned < 20) return 0;
    return scanned;
}

void ita_c1_playout(int year, int clubs, ItaPlayout *p)
{
    p->ties = 0;
    p->direct = clubs > 0 ? 1 : 0;
    for (int k = 0; k < 4; k++) p->slot[k] = -1;
    if (clubs < 6) return;
    int n = clubs;
    if (year == 2013) {
        p->direct = 0;
        return;
    }
    if (year == 2017) {
        p->ties = 2;
        p->direct = 0;
        p->slot[0] = n - 1, p->slot[1] = n - 4, p->slot[2] = n - 2, p->slot[3] = n - 3;
        return;
    }
    if (year == 2018) {
        p->ties = 1;
        p->direct = n >= 20 ? 1 : 0;
        p->slot[0] = n - 1 - p->direct, p->slot[1] = n - 2 - p->direct;
        return;
    }
    p->ties = 2;
    p->slot[0] = n - 2, p->slot[1] = n - 5, p->slot[2] = n - 3, p->slot[3] = n - 4;
}

int ita_c1_gap_rule(int year) { return year >= 2016 && year != 2019; }

int ita_c1_playout_gap(ItaPlayout *p, const int *points, int *down, int *safe)
{
    int kept[4], nk = 0, out = 0;
    for (int t = 0; t < p->ties; t++) {
        int lo = p->slot[2 * t], hi = p->slot[2 * t + 1];
        if (points[hi] - points[lo] > ITA_PLAYOUT_GAP) {
            down[out] = lo;
            safe[out++] = hi;
            continue;
        }
        kept[nk++] = lo;
        kept[nk++] = hi;
    }
    for (int k = 0; k < 4; k++) p->slot[k] = k < nk ? kept[k] : -1;
    p->ties = nk / 2;
    return out;
}

int ita_serie_a_zone(int year, int clubs, int down, int row, int class_zone)
{
    if (row < 0 || row >= clubs) return 0;
    int relegated = down > 0 && row >= clubs - down;
    if (year <= ITA_SPAREGGIO_LAST) return relegated ? 5 : class_zone == 5 ? 0 : class_zone;
    if (year < ITA_SERIE_A_SPAREGGIO_BACK) return 0;
    return row == 0 ? 1 : relegated ? 5 : 0;
}
