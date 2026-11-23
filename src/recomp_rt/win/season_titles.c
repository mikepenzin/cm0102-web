/* Titles taken away after the season (0.6.2 item 108, Max 3 November 2026: do what real life does). The research rows
 * of docs/research/history-0.6.0 give each title as the official record holds it today, so a title revoked and given
 * to no club has no row ("not awarded"), and the newest row of that competition is the season before. Each row here is
 * a start season that began before the ruling: real life then still took the club of the pitch as the holder (the
 * Supercoppa, the UEFA place of the cup winner), so the original's holder read gives it in that
 * season (wrap of 0x4afd80 in season_history.c). The names are those of the one database of that season whose plan
 * clears the row (plan.tsv): another database keeps its own row and does not match.
 * - Italian Serie A 2004/05: Juventus 1st, AC Milan 2nd. The FIGC appeal court revoked the title on 25 July 2006
 *   and gave it to no club. The Supercoppa of 20 August 2005 was Juventus v Internazionale
 *   (it.wikipedia.org/wiki/Serie_A_2004-2005, rsssf.org/tablesi/ital05.html, it.wikipedia.org/wiki/
 *   Supercoppa_italiana_2005, read 3 November 2026).
 *   September 2005 database (start 2005): newest row 2003/04, AC Milan.
 * - Polish FA Cup 2004/05: Groclin Dyskobolia beat Zaglebie Lubin. The PZPN took the cup away on 2 September 2020 and
 *   gave it to no club (pl.wikipedia.org, Puchar Polski 2004/2005, and en.wikipedia.org/wiki/2004-05_Polish_Cup,
 *   read 3 November 2026). September 2005 database: newest row 2004, Lech Poznan.
 * - Chinese First Division A 2003: Shanghai Shenhua 1st, Shanghai International 2nd (Shanghai Zhongyuan in the
 *   October 2003 database). The CFA revoked the title on 18 February 2013 and gave it to no club
 *   (zh.wikipedia.org, 2003 Jia-A page, and rsssf.org/tablesc/china03.html, read 3 November 2026).
 *   October 2003 database (start 2003): newest row 2002, Dalian Shide.
 * Serie A 2005/06 needs no row: its research row holds Internazionale (given on 26 July 2006), and every start after
 * it reads that row. */
#include "season_titles.h"

#include <stddef.h>
#include <string.h>

static const struct {
    const char *comp;
    int start;                  /* the start season */
    int label;                  /* the label year of the newest history row in that start */
    const char *row;            /* its winner */
    const char *then[2];        /* the winner and the runner-up of that time */
    const char *nation;
} titles[] = {
    {"Italian Serie A", 2005, 2004, "AC Milan", {"Juventus", "AC Milan"}, "Italy"},
    {"Polish FA Cup", 2005, 2004, "KKS Lech Poznan", {"SSA Groclin Dyskobolia", "SSA Zaglebie Lubin"}, "Poland"},
    {"Chinese First Division A", 2003, 2002, "Dalian Shide", {"Shanghai Shenhua", "Shanghai Zhongyuan"}, "China PR"},
};

const char *season_title_then(const char *comp, int start, int label, const char *row, int k, const char **nation)
{
    if (!comp || !row || k < 0 || k > 1) return NULL;
    for (size_t i = 0; i < sizeof titles / sizeof titles[0]; i++)
        if (titles[i].start == start && titles[i].label == label && !strcmp(titles[i].comp, comp) &&
            !strcmp(titles[i].row, row)) {
            if (nation) *nation = titles[i].nation;
            return titles[i].then[k];
        }
    return NULL;
}

int season_title_has(const char *comp, int start)
{
    for (size_t i = 0; comp && i < sizeof titles / sizeof titles[0]; i++)
        if (titles[i].start == start && !strcmp(titles[i].comp, comp)) return 1;
    return 0;
}
