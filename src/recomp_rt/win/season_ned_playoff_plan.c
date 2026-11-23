/* The Dutch nacompetitie by season (issue 150). Host code with no game state: tests/seasons/test_season_ned_playoff.c.
 *
 * Real life, read on 18 November 2026 on the RSSSF season pages https://rsssf.org/tablesn/ned05.html, ned06.html,
 * ned2012.html, ned2018.html, ned2019.html and ned2021.html to ned2026.html, and the KNVB play-off rules of 2026/27
 * (https://www.knvb.nl/downloads/bestand/30197/doc, rows hol-bel-315 and hol-bel-328 of the rules table):
 * - To 2004/05: two groups of four. The class plays them.
 * - 2005/06 to 2018/19: eight Eerste Divisie clubs and the Eredivisie 16th and 17th for two places, all two legs. Four
 *   clubs play round 1, the winners, four more clubs and the two Eredivisie clubs play round 2, and the four winners
 *   play two ties of round 3. The rounds form two brackets of five, each for one place.
 * - 2019/20: abandoned, no play-off.
 * - 2020/21: six Eerste Divisie clubs, three ties, then semi-finals with the Eredivisie 16th, then a final. Single
 *   matches. 2021/22 to 2024/25 and from 2026/27 the same rounds in two legs.
 * - 2025/26: four clubs play round 1, the winners meet the two best clubs in round 2, a semi-final, then the final
 *   with the Eredivisie 16th. All two legs.
 * Seeds count the Eerste Divisie entrants in table order from 0. */
#include "season_ned_playoff_plan.h"

NedPoForm ned_po_form(int season)
{
    if (season <= 2004) return NED_PO_GROUPS;
    if (season <= 2018) return NED_PO_TEN;
    if (season == 2019) return NED_PO_NONE;
    return season == 2025 ? NED_PO_LADDER : NED_PO_SEVEN;
}

int ned_po_legs(int season) { return season == 2020 ? 1 : 2; }

int ned_po_brackets(NedPoForm form)
{
    return form == NED_PO_TEN ? 2 : form == NED_PO_SEVEN || form == NED_PO_LADDER ? 1 : 0;
}

int ned_po_rounds(NedPoForm form)
{
    return form == NED_PO_LADDER ? 4 : form == NED_PO_TEN || form == NED_PO_SEVEN ? 3 : 0;
}

int ned_po_lower_clubs(NedPoForm form)
{
    return form == NED_PO_TEN ? 8 : form == NED_PO_SEVEN || form == NED_PO_LADDER ? 6 : 0;
}

int ned_po_upper_clubs(NedPoForm form) { return ned_po_brackets(form); }

/* Round r is drawn on day 1 + 7r and plays on day 3 + 7r, the second leg 4 days later. */
int ned_po_days(NedPoForm form, int legs)
{
    int rounds = ned_po_rounds(form);
    return rounds ? 3 + 7 * (rounds - 1) + (legs == 2 ? 4 : 0) : 0;
}

static const NedPoRound TEN[3] = {{110, 2, 1, 2, 0}, {120, 4, 2, 3, 2}, {150, 2, 1, 0, 0}};
static const NedPoRound SEVEN[3] = {{120, 6, 3, 6, 0}, {130, 4, 2, 1, 6}, {150, 2, 1, 0, 0}};
static const NedPoRound LADDER[4] = {{110, 4, 2, 4, 0}, {120, 4, 2, 2, 4}, {130, 2, 1, 0, 0}, {150, 2, 1, 1, 6}};

int ned_po_round(NedPoForm form, int round, NedPoRound *out)
{
    if (round < 0 || round >= ned_po_rounds(form)) return 0;
    *out = form == NED_PO_TEN ? TEN[round] : form == NED_PO_SEVEN ? SEVEN[round] : LADDER[round];
    return 1;
}

int ned_po_entrants(NedPoForm form, int bracket, int out[NED_PO_MAX_LIST])
{
    static const int ten[2][5] = {{7, 4, NED_PO_UPPER + 1, 3, 0}, {6, 5, NED_PO_UPPER, 2, 1}};
    static const int seven[7] = {5, 0, 4, 1, 3, 2, NED_PO_UPPER};
    static const int ladder[7] = {4, 3, 5, 2, 0, 1, NED_PO_UPPER};
    if (bracket < 0 || bracket >= ned_po_brackets(form)) return 0;
    int n = form == NED_PO_TEN ? 5 : 7;
    for (int i = 0; i < n; i++)
        out[i] = form == NED_PO_TEN ? ten[bracket][i] : form == NED_PO_SEVEN ? seven[i] : ladder[i];
    return n;
}

#define W(t) (-(t) - 1)

int ned_po_tie(NedPoForm form, int round, int tie, int *first, int *second)
{
    static const int ten[3][2][2] = {{{0, 1}}, {{W(0), 2}, {3, 4}}, {{W(0), W(1)}}};
    static const int seven[3][3][2] = {{{0, 1}, {2, 3}, {4, 5}}, {{W(0), W(1)}, {W(2), 6}}, {{W(0), W(1)}}};
    static const int ladder[4][2][2] = {{{0, 1}, {2, 3}}, {{W(0), 4}, {W(1), 5}}, {{W(0), W(1)}}, {{W(0), 6}}};
    NedPoRound r;
    if (!ned_po_round(form, round, &r) || tie < 0 || tie >= r.ties) return 0;
    const int *p = form == NED_PO_TEN ? ten[round][tie] : form == NED_PO_SEVEN ? seven[round][tie] : ladder[round][tie];
    *first = p[0];
    *second = p[1];
    return 1;
}
