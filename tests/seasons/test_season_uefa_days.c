/* The UEFA group and league phase days against the FIFA days (row A18, lead 4, 26 October 2026): for every running
 * season from 2002/03 to 2030/31, each Champions League, UEFA Cup / Europa League and Conference League group or
 * league phase matchday the game uses (season_uefa_days.c, with each season's format from the UEFA table), and
 * each day a qualifier can move to (season_fifa_day: every FIFA day of the list). No matchday meets a FIFA day
 * (season_fifa_meets: within a day of one, or inside a window). With --list it prints every matchday and every
 * FIFA day. The first knockout draw of each competition (season_uefa_ko_date) comes after its last group or league
 * phase day. ctest season_uefa_days. */
#include "season_uefa.h"
#include "season_uefa_days.h"

#include <stdio.h>
#include <string.h>

int season_code_on(void) { return 1; }
int season_start_year(void) { return 2001; }
int season_changing_rules(void) { return 1; }

static int failures, checked, list;
static int last_day[3];                 /* the last group day of 'c', 'u' and 'k' in the season, as y * 400 + doy */

static int comp_index(const char *name)
{
    return strstr(name, "Champions") ? 0 : strstr(name, "Conference") ? 2 : 1;
}

static void day(int season, const char *comp, int md, int y, int m, int d)
{
    checked++;
    int meets = season_fifa_meets(y, m + 1, d);
    if (list || meets)
        printf("%s%d/%02d\t%s\tmatchday %d\t%04d-%02d-%02d\n", meets ? "FAIL meets a FIFA day: " : "", season,
               (season + 1) % 100, comp, md, y, m + 1, d);
    failures += meets;
    int k = comp_index(comp), key = y * 400 + season_day_of_year(y, m + 1, d);
    if (key > last_day[k]) last_day[k] = key;
}

/* The first knockout draw of comp after its last group day (no row: the format's own dates stay). */
static void ko_draw(int season, int k)
{
    static const char comps[3] = {'c', 'u', 'k'};
    if (!last_day[k]) return;
    const SeasonUefaKoDate *r = NULL;
    for (int kind = SEASON_UEFA_KO_PO; kind <= SEASON_UEFA_KO_QF && !r; kind++)
        r = season_uefa_ko_date(comps[k], season, kind);
    if (!r) return;
    int y = season + r->draw_year, key = y * 400 + season_day_of_year(y, r->draw_month + 1, r->draw_day);
    if (key <= last_day[k]) {
        printf("FAIL %d/%02d %c: the first knockout draw (%04d-%02d-%02d) is not after the last group day\n", season,
               (season + 1) % 100, comps[k], y, r->draw_month + 1, r->draw_day);
        failures++;
    }
}

static void league(int season, char comp, const char *name)
{
    int wd, n = comp == 'k' ? 6 : 8;
    const SeasonLeagueDay *t = season_uefa_league_days(comp, season, &wd);
    for (int i = 0; i < n; i++) {
        int y, m, d;
        season_weekday_date(season + t[i].year_off, t[i].month, t[i].day, wd, &y, &m, &d);
        day(season, name, i + 1, y, m, d);
    }
}

static void groups(int season, int days, const char *name)
{
    const SeasonLeagueDay *t = season_uefa_group_days(season, days);
    for (int i = 0; i < days; i++) {
        int y, m, d;
        season_weekday_date(season + t[i].year_off, t[i].month, t[i].day, 3, &y, &m, &d);
        day(season, name, i + 1, y, m, d);
    }
}

static int has_round(const SeasonUefaRound *rs, int n, const char *name)
{
    for (int i = 0; i < n && i < SEASON_UEFA_ROUNDS; i++)
        if (rs[i].name && !strcmp(rs[i].name, name)) return 1;
    return 0;
}

int main(int argc, char **argv)
{
    list = argc > 1 && !strcmp(argv[1], "--list");
    for (int s = 2002; s <= 2030; s++) {
        memset(last_day, 0, sizeof last_day);
        const SeasonUefa *r = season_uefa_table_row(s);
        if (!r) {
            printf("FAIL no UEFA row for %d\n", s);
            failures++;
            continue;
        }
        if (r->cl_league) league(s, 'c', "Champions League league phase");
        else
            for (int phase = 1; phase <= 2; phase++) {
                if (phase == 2 && !has_round(r->cl_rounds, r->cl_round_count, "Groups 2")) break;
                for (int set = 0; set < 2; set++) {
                    SeasonLeagueDay t[6];
                    int wd[6];
                    if (!season_uefa_cl_group_days(s, phase, set, t, wd)) {
                        printf("FAIL %d/%02d: no real Champions League days (phase %d)\n", s, (s + 1) % 100, phase);
                        failures++;
                    }
                    char name[64];
                    snprintf(name, sizeof name, "Champions League %sgroups %s", phase == 2 ? "second " : "",
                             set ? "E-H" : "A-D");
                    for (int i = 0; i < 6; i++) {
                        int y, m, d;
                        season_weekday_date(s + t[i].year_off, t[i].month, t[i].day, wd[i], &y, &m, &d);
                        day(s, name, i + 1, y, m, d);
                    }
                }
            }
        const char *uc = s >= 2009 ? "Europa League" : "UEFA Cup";
        if (r->uc_league) league(s, 'e', "Europa League league phase");
        else if (r->uc_groups > 0)
            groups(s, (r->uc_group_clubs % 2 ? r->uc_group_clubs : r->uc_group_clubs - 1) * r->uc_meetings, uc);
        if (r->ecl_league) league(s, 'k', "Conference League league phase");
        else if (r->ecl_round_count && r->ecl_groups > 0)
            groups(s, (r->ecl_group_clubs % 2 ? r->ecl_group_clubs : r->ecl_group_clubs - 1) * r->ecl_meetings,
                   "Conference League");
        for (int k = 0; k < 3; k++) ko_draw(s, k);
        if (s <= 2008 && r->uc_groups > 0) {                /* Q2 and R1 of the UEFA Cup before its groups */
            void (*first[2])(int, int *, int *, int *) = {season_uefa_cup_q2_day, season_uefa_cup_r1_day};
            const char *names[2] = {"UEFA Cup second qualifying round", "UEFA Cup first round"};
            long end = 0;
            for (int k = 0; k < 2; k++)
                for (int leg = 0; leg < 2; leg++) {
                    int y, m, d;
                    first[k](s, &y, &m, &d);
                    long z = season_abs_day(y, m, d) + 14 * leg;
                    season_from_abs_day(z, &y, &m, &d);
                    day(s, names[k], leg + 1, y, m - 1, d);
                    end = z;
                }
            const SeasonLeagueDay *g = season_uefa_group_days(s, (r->uc_group_clubs % 2 ? r->uc_group_clubs :
                                                                   r->uc_group_clubs - 1) * r->uc_meetings);
            long g1 = season_abs_day(s + g[0].year_off, g[0].month + 1, g[0].day) - 3;   /* before any weekday move */
            checked++;
            if (end >= g1) {
                printf("FAIL %d/%02d: the UEFA Cup first round ends on or after the first group day\n", s,
                       (s + 1) % 100);
                failures++;
            }
        }
    }
    /* The real days (UEFA Cup with groups, item 125 batch Q): Q2 first leg, R1 draw, R1 first leg. */
    static const int real[][7] = {{2004, 8, 12, 8, 27, 9, 16}, {2005, 8, 11, 8, 26, 9, 15}, {2006, 8, 10, 8, 25, 9, 14},
                                  {2007, 8, 16, 8, 31, 9, 20}, {2008, 8, 14, 8, 29, 9, 18}};
    for (size_t i = 0; i < sizeof real / sizeof real[0]; i++) {
        int got[6], y;
        season_uefa_cup_q2_day(real[i][0], &y, &got[0], &got[1]);
        season_uefa_cup_r1_draw(real[i][0], &y, &got[2], &got[3]);
        season_uefa_cup_r1_day(real[i][0], &y, &got[4], &got[5]);
        checked++;
        if (memcmp(got, real[i] + 1, sizeof got)) {
            printf("FAIL %d/%02d: UEFA Cup Q2 %02d-%02d, R1 draw %02d-%02d, R1 %02d-%02d; real %02d-%02d, %02d-%02d, "
                   "%02d-%02d\n", real[i][0], (real[i][0] + 1) % 100, got[0], got[1], got[2], got[3], got[4], got[5],
                   real[i][1], real[i][2], real[i][3], real[i][4], real[i][5], real[i][6]);
            failures++;
        }
    }
    /* Item 298: the UEFA Cup before its groups (2001/02 to 2003/04) takes the same rule in a season game. The real
     * qualifying round first legs and first round draws (en.wikipedia.org "2001-02 UEFA Cup", "2002-03 UEFA Cup",
     * "2003-04 UEFA Cup", rsssf.org ec200102, ec200203, ec200304, kassiesa.net dates2001 to dates2003, read 19
     * November 2026): 9 and 24 August 2001, 15 and 30 August 2002, 14 and 29 August 2003. The real first rounds
     * (20 September 2001 after the postponement from 13 September, 19 September 2002, 24 September 2003) are not
     * matched: the rule gives 13, 12 and 18 September, whose two legs meet no FIFA day. */
    static const int pre[][5] = {{2001, 8, 9, 8, 24}, {2002, 8, 15, 8, 30}, {2003, 8, 14, 8, 29}};
    for (size_t i = 0; i < sizeof pre / sizeof pre[0]; i++) {
        int got[4], y, m, d;
        season_uefa_cup_q2_day(pre[i][0], &y, &got[0], &got[1]);
        season_uefa_cup_r1_draw(pre[i][0], &y, &got[2], &got[3]);
        checked++;
        if (memcmp(got, pre[i] + 1, sizeof got)) {
            printf("FAIL %d/%02d: UEFA Cup qualifying round %02d-%02d, R1 draw %02d-%02d; real %02d-%02d, %02d-%02d\n",
                   pre[i][0], (pre[i][0] + 1) % 100, got[0], got[1], got[2], got[3], pre[i][1], pre[i][2], pre[i][3],
                   pre[i][4]);
            failures++;
        }
        season_uefa_cup_r1_day(pre[i][0], &y, &m, &d);
        for (int leg = 0; leg < 2; leg++) {
            int y2, m2, d2;
            season_from_abs_day(season_abs_day(y, m, d) + 14 * leg, &y2, &m2, &d2);
            checked++;
            if (season_fifa_meets(y2, m2, d2)) {
                printf("FAIL %d/%02d: UEFA Cup first round leg %d on %04d-%02d-%02d meets a FIFA day\n", pre[i][0],
                       (pre[i][0] + 1) % 100, leg + 1, y2, m2, d2);
                failures++;
            }
        }
    }
    if (list)
        for (int y = 2002; y <= 2031; y++)
            for (int m = 1; m <= 12; m++)
                for (int want = 1; want <= 31; want++) {
                    int f = season_fifa_day(y, m, want);
                    if (!f) break;
                    if (f >= want) printf("FIFA day\t%04d-%02d-%02d\n", y, m, f), want = f;
                }
    printf("%d matchdays checked, %d failures\n", checked, failures);
    return failures ? 1 : 0;
}
