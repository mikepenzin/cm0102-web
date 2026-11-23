/* International competitions in the original (0.6.0 slice 27): the editions and the rules of the code sites
 * (season_intl.h). This file reads no game memory, so the unit test tests/seasons/test_season_intl.c builds
 * it with its own season state. Research and evidence: docs/research/2026-10-18-international.md. */
#include "season.h"
#include "season_intl.h"

int season_change_is_creating(void);    /* season_change.c: 1 while create_game 0x828110 runs */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* The source file of each class of the code sites (enum of season_intl.h). */
static const char *const class_files[INTL_CLASSES] = {
    "world_cup", "euro_champ", "fifa_confed", "goldcup", "african_nations", "asia_nations", "oceania_nations",
    "olympics",
};

/* The year that an ended class holds (the Confederations Cup after 2017): the object waits and plays no more
 * finals. The same year as SEASON_CUPS_NEVER of the ended cups (season_cups.h), which the history checker reads
 * as ended (tests/seasons/future/history_check.py, ENDED_YEAR). It is also an edition year of the classes that
 * continue (the Africa Cup of Nations holds every odd year from 2027), so a game near 2999 would show such an
 * edition as ended there; the date jump limit is 2300 (season.h). Up to 0.6.1 every class waited for 2200 after
 * its table, which a game to 2300 reaches (0.6.2 open items 1, 13 and 34). */
#define NEVER_YEAR 2999u

/* The cycle of each class after the last edition of its table, in years (0.6.2 decision A: each final continues
 * on its last cycle, with no end year; the OFC Nations Cup every 4 years from 2028, Max on 30 October 2026; the
 * Africa Cup of Nations every 2 years, the original's step, where the table's last two editions are one year
 * apart). 0: the class has ended (the Confederations Cup). Order of the enum of season_intl.h. */
static const int class_cycles[INTL_CLASSES] = {4, 4, 0, 4, 2, 4, 4, 4};

/* The last season with a golden goal (int-world: the 2003 Confederations Cup used it, "golden goal ceased in
 * 2004"; Q367: the silver goal of Euro 2004, season 2003/04, becomes full extra time and penalties). */
#define LAST_GOLDEN_SEASON 2002
#define GOLDEN_GOAL 0x80u

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_INTL_LOG");
        on = v && *v == '1';
    }
    return on;
}

int season_intl_on(void)
{
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

const SeasonIntlEdition *season_intl_table_edition(const char *file, int year)
{
    for (int i = 0; i < season_intl_edition_count; i++)
        if (season_intl_editions[i].year == year && !strcmp(season_intl_editions[i].file, file))
            return &season_intl_editions[i];
    return NULL;
}

/* ---- The editions after the table (0.6.2 decision A) ---- */

/* The last table edition of a class, and its cycle after the table (0 for an ended class or another file). */
static const SeasonIntlEdition *table_last(const char *file, int *cycle)
{
    const SeasonIntlEdition *last = NULL;
    for (int i = 0; i < season_intl_edition_count; i++)
        if (!strcmp(season_intl_editions[i].file, file)) last = &season_intl_editions[i];
    *cycle = 0;
    for (int c = 0; last && c < INTL_CLASSES; c++)
        if (!strcmp(class_files[c], file)) *cycle = class_cycles[c];
    return last;
}

/* The year field of a class is a 16-bit signed word (+0x40): no edition after this year. */
#define LAST_EDITION_YEAR 32767

/* ---- Hosts after the table (0.6.2 slice 5) ---- */

/* Q5: the hosts announced for editions after the table, each checked against its source
 * (docs/research/2026-10-31-hosts-after-table.md): FIFA Congress of 11 December 2024, UEFA Executive Committee of
 * 10 October 2023, the IOC elections of 2017 and 2021. */
static const struct { const char *file; int year; const char *hosts; } announced[] = {
    {"world_cup", 2034, "Saudi Arabia"},
    {"euro_champ", 2032, "Italy;Turkey/T\xfcrkiye"},
    {"olympics", 2028, "United States/USA"},
    {"olympics", 2032, "Australia"},
};

const char *season_intl_announced_hosts(const char *file, int year)
{
    for (size_t i = 0; i < sizeof announced / sizeof announced[0]; i++)
        if (announced[i].year == year && !strcmp(announced[i].file, file)) return announced[i].hosts;
    return NULL;
}

/* The Euros of 2020 (eleven host nations) and 2028 (five) have no host in the table, so that no host takes an
 * automatic place (UEFA gave none in 2020). The history row names the nation of the final: Wembley, England, in
 * both (UEFA, 2021 and 10 October 2023). The World Cup of 2026 has three hosts, and its row names the nation of the
 * final too: MetLife Stadium, United States (issue 145, https://en.wikipedia.org/wiki/2026_FIFA_World_Cup). */
const char *season_intl_history_host(const char *file, int year)
{
    if (!strcmp(file, "euro_champ") && (year == 2020 || year == 2028)) return "England";
    if (!strcmp(file, "world_cup") && year == 2026) return "United States/USA";
    return NULL;
}

/* The news of the qualified nations (NationalTeams::write_qualified_nations_text 0x777ca0) has room for two hosts
 * (parts 4 and 5, 0x777ce7..0x777cfb). Euro 2020 names the nation of the final, as its history row does. Euro 2028
 * names the first two of the four hosts that stage matches: UEFA's decision of 10 October 2023 names five hosts, and
 * Northern Ireland stages no match (season_nl_euro.c, kept_places). */
const char *season_intl_news_hosts(const char *file, int year)
{
    if (strcmp(file, "euro_champ")) return NULL;
    return year == 2020 ? "England" : year == 2028 ? "England;Republic of Ireland" : NULL;
}

/* Q4: the original's rows (host-rule.md, read in HostCountry::init_default_table 0x5f9970): the World Cup 1998 to
 * 2026 (0x5f99bb), the Euro (0x5f9c09) and the Olympics (0x5f9ff2) 2000 to 2028, every 4 years, moving on 32; the
 * Africa Cup of Nations 2000 to 2014 every 2 years, moving on 16 (0x5f9eb7). The Copa America and Asian Cup rows
 * move on by a step that does not fit their cycle, and the Gold Cup and OFC Nations Cup have none: they rotate
 * (Q32). */
static const struct { const char *file; int base, step, rows; } slots[] = {
    {"world_cup", 1998, 4, 8}, {"euro_champ", 2000, 4, 8}, {"olympics", 2000, 4, 8}, {"african_nations", 2000, 2, 8},
};

int season_intl_host_slots(const char *file, int *base, int *step, int *rows)
{
    for (size_t i = 0; i < sizeof slots / sizeof slots[0]; i++)
        if (!strcmp(slots[i].file, file)) {
            *base = slots[i].base;
            *step = slots[i].step;
            *rows = slots[i].rows;
            return 1;
        }
    return 0;
}

int season_intl_host_slot(const char *file, int year)
{
    int base, step, rows;
    if (!season_intl_host_slots(file, &base, &step, &rows)) return -1;
    int k = (year - base) / step;                       /* the editions of odd years after 2027 take the rows in order */
    return ((k % rows) + rows) % rows;
}

static int (*host_alive)(const char *file, const char *name);

void season_intl_set_alive(int (*alive)(const char *file, const char *name)) { host_alive = alive; }

const char *season_intl_rotation_host(const char *file, int k, int (*alive)(const char *file, const char *name))
{
    static char names[64][64];
    int n = 0;
    for (int i = 0; i < season_intl_edition_count && n < 64; i++) {
        const SeasonIntlEdition *e = &season_intl_editions[i];
        if (strcmp(e->file, file)) continue;
        for (const char *p = e->hosts; *p && n < 64;) {
            size_t len = strcspn(p, ";");
            char one[64];
            snprintf(one, sizeof one, "%.*s", (int)len, p);
            int seen = 0;
            for (int j = 0; j < n && !seen; j++) seen = !strcmp(names[j], one);
            if (!seen && (!alive || alive(file, one))) snprintf(names[n++], sizeof names[0], "%s", one);
            p += len;
            if (*p == ';') p++;
        }
    }
    return n && k >= 0 ? names[k % n] : NULL;
}

int season_intl_rotation_count(const char *file, int (*alive)(const char *file, const char *name))
{
    int n = 0;
    while (n < 64 && season_intl_rotation_host(file, n, alive)) {
        char first[64], now[64];
        snprintf(now, sizeof now, "%s", season_intl_rotation_host(file, n, alive));
        snprintf(first, sizeof first, "%s", season_intl_rotation_host(file, 0, alive));
        if (n && !strcmp(now, first)) break;
        n++;
    }
    return n;
}

int season_intl_rotation_step(const char *file, int year)
{
    int cycle;
    const SeasonIntlEdition *last = table_last(file, &cycle);
    return last && cycle > 0 && year > last->year ? (year - last->year) / cycle - 1 : -1;
}

int season_intl_pick_pair(const int valid[3], int r, int redraw)
{
    if (r >= 0 && r < 3 && valid[r]) return r;
    int others[3], n = 0;
    for (int k = 0; k < 3; k++)
        if (valid[k] && k != r) others[n++] = k;
    return n ? others[((redraw % n) + n) % n] : -1;
}

/* The edition of a year after the table: a copy of the last table edition (teams, first day of the year, no
 * golden goal) on the class's cycle. Its hosts: the announced ones (Q5), the rotation host of a class whose hosts
 * rotate (Q32), else none here: the host row of the year gives them (Q4, season_intl.c). The copies live in a
 * ring, so a pointer stays good for the next 31 calls; every caller reads it at once. */
static const SeasonIntlEdition *continued(const char *file, int year)
{
    static SeasonIntlEdition ring[32];
    static char ring_hosts[32][128];
    static int next;
    int cycle;
    const SeasonIntlEdition *last = table_last(file, &cycle);
    if (!last || cycle <= 0 || year <= last->year || year > LAST_EDITION_YEAR || (year - last->year) % cycle)
        return NULL;
    SeasonIntlEdition *e = &ring[next];
    char *hosts = ring_hosts[next];
    next = (next + 1) % (int)(sizeof ring / sizeof ring[0]);
    *e = *last;
    e->year = year;
    e->season = year - (last->year - last->season);
    e->start_year = year + (last->start_year - last->year);
    e->golden = 0;
    e->hosts = season_intl_announced_hosts(file, year);
    if (!e->hosts) {
        int base, step, rows;
        e->hosts = season_intl_host_slots(file, &base, &step, &rows) ? NULL
                   : season_intl_rotation_host(file, season_intl_rotation_step(file, year), host_alive);
    }
    snprintf(hosts, sizeof ring_hosts[0], "%s", e->hosts ? e->hosts : "");
    e->hosts = hosts;                       /* the rotation's buffer changes with the next call */
    e->row = "after-table";
    return e;
}

const SeasonIntlEdition *season_intl_edition(const char *file, int year)
{
    const SeasonIntlEdition *e = season_intl_table_edition(file, year);
    return e ? e : continued(file, year);
}

/* The table is sorted by file, then year (make_intl.py). */
const SeasonIntlEdition *season_intl_first(const char *file, int start_season)
{
    for (int i = 0; i < season_intl_edition_count; i++)
        if (!strcmp(season_intl_editions[i].file, file) && season_intl_editions[i].season >= start_season)
            return &season_intl_editions[i];
    int cycle;
    const SeasonIntlEdition *last = table_last(file, &cycle);
    if (!last || cycle <= 0) return NULL;
    int k = (start_season - last->season + cycle - 1) / cycle;      /* start_season > last->season here */
    if (k < 1) k = 1;
    long year = (long)last->year + (long)k * cycle;
    return year <= LAST_EDITION_YEAR ? continued(file, (int)year) : NULL;
}

const SeasonIntlEdition *season_intl_next(const char *file, int year)
{
    for (int i = 0; i < season_intl_edition_count; i++)
        if (!strcmp(season_intl_editions[i].file, file) && season_intl_editions[i].year > year)
            return &season_intl_editions[i];
    int cycle;
    const SeasonIntlEdition *last = table_last(file, &cycle);
    if (!last || cycle <= 0) return NULL;
    long k = year < last->year ? 1 : ((long)year - last->year) / cycle + 1;
    long next = (long)last->year + k * cycle;
    return next <= LAST_EDITION_YEAR ? continued(file, (int)next) : NULL;
}

static const char *class_file(uint32_t cls)
{
    return cls < INTL_CLASSES ? class_files[cls] : "?";
}

const char *season_intl_class_file(uint32_t cls) { return class_file(cls); }

/* The constructor of a class: the original rounds the game year up to its cycle. A game that starts in
 * another season takes the first edition of the table from its start season. The base game and a 2001 start
 * keep the original's year (the table holds the same editions for 2001/02). */
uint32_t rt_intl_first_year(uint32_t cls, uint32_t year)
{
    if (!season_intl_on() || season_start_year() == SEASON_FIRST_YEAR) return year;
    const SeasonIntlEdition *e = season_intl_first(class_file(cls), season_start_year());
    uint32_t now = e ? (uint32_t)e->year : NEVER_YEAR;
    if (logging())
        fprintf(stderr, "recomp: season intl: %s first edition %u (the original %u)\n", class_file(cls), now,
                year & 0xffffu);
    return now;
}

/* start_new_season of a class: the original adds its step. With changing rules on, the next edition of the
 * table, and after the table the next edition on the class's last cycle (0.6.2 decision A). With changing rules
 * off, the rules of the start season stay: the original's step from the first edition. An ended class waits for
 * NEVER_YEAR (Q248: a competition that ended does not come back). */
uint32_t rt_intl_next_year(uint32_t cls, uint32_t year, uint32_t step)
{
    uint32_t old = year & 0xffffu;
    if (!season_intl_on() || !season_changing_rules()) return old + step;
    const SeasonIntlEdition *e = season_intl_next(class_file(cls), (int)(int16_t)old);
    uint32_t now = e ? (uint32_t)e->year : (old >= NEVER_YEAR ? old : NEVER_YEAR);
    if (logging())
        fprintf(stderr, "recomp: season intl: %s next edition %u after %u (the original %u)\n", class_file(cls),
                now, old, old + step);
    return now;
}

/* The rule flags of a knockout round (sweep-international section 7): bit 0x80 is the golden goal. With
 * changing rules on, the edition of the table decides, or the season of the year for a year that the table
 * has no edition for. With changing rules off, the rule of the start season stays. */
/* 1 for the edition of a class that a game started in 2001/02 builds at the new game: the original's first
 * edition (the table's first edition from 2001/02 is the same year). It keeps the original's dates and round
 * flags, because its stages are made at the new game (Q152: a 2001 game with changing rules on equals the base
 * game at the start). */
static int start_edition_2001(uint32_t cls, int year)
{
    if (season_start_year() != SEASON_FIRST_YEAR || cls >= INTL_CLASSES) return 0;
    const SeasonIntlEdition *e = season_intl_first(class_files[cls], SEASON_FIRST_YEAR);
    return e && e->year == year;
}

uint32_t rt_intl_round_flags(uint32_t cls, uint32_t year, uint32_t flags)
{
    if (!season_intl_on() || !(flags & GOLDEN_GOAL)) return flags;
    if (start_edition_2001(cls, (int)(int16_t)(year & 0xffffu))) return flags;
    int golden;
    if (season_changing_rules()) {
        int y = (int)(int16_t)(year & 0xffffu);
        const SeasonIntlEdition *e = season_intl_edition(class_file(cls), y);
        golden = e ? e->golden : y <= LAST_GOLDEN_SEASON + 1;
    } else {
        golden = season_start_year() <= LAST_GOLDEN_SEASON;
    }
    return golden ? flags : flags & ~GOLDEN_GOAL;
}

/* The first World Cup at which the holder had to qualify (int-world: "2002 is the last World Cup with an
 * automatic place for the holders"). */
#define FIRST_WC_WITHOUT_HOLDER 2006

/* The World Cup holder that a qualifying section leaves out of its pool. In a season game every section plays
 * for the 2006 World Cup or later, or for 2002 without the holder in its pool (season_intl.h, seasons.py). */
uint32_t rt_intl_wc_holder(uint32_t holder)
{
    /* A game started in 2001/02 makes its sections at the new game as the base game does (Q152). */
    if (season_start_year() == SEASON_FIRST_YEAR && season_change_is_creating()) return holder;
    return season_intl_on() ? 0u : holder;
}

/* Slot 0 of the World Cup teams: the holder, to 2002. */
uint32_t rt_intl_wc_holder_slot(uint32_t year)
{
    return !season_intl_on() || (int)(int16_t)(year & 0xffffu) < FIRST_WC_WITHOUT_HOLDER;
}

/* ---- Finals dates ---- */

/* The first group day of each class in its template function (vtable +0x3c), the first call of
 * League_Stage date 0x68a160 there: day, then the month from 0 (read at each address). */
static const struct { int day, month0, code; } class_first[INTL_CLASSES] = {
    {12, 5, 4},     /* World Cup 12 June: 0x92c335 */
    {9, 5, 6},      /* European Championship 9 June: 0x57ac35 */
    {29, 6, 5},     /* Confederations Cup 29 July: 0x58b601 */
    {6, 6, 3},      /* Copa America 6 July: 0x5e09a5 */
    {8, 1, 6},      /* African Cup of Nations 8 February: 0x401385 */
    {13, 9, 4},     /* Asian Cup 13 October: 0x40eba5 */
    {10, 10, 4},    /* Oceania Nations Cup 10 November when the year % 4 is 2 (0x798b70, 0x798c23), else 19 June with
                       code 0 (0x798f7e) */
    {13, 8, 2},     /* Olympics 13 September: 0x79f875 */
};
/* code: the weekday code of that first group day (0 Monday to 6 Sunday, the sixth argument of League_Stage's date
 * 0x68a160), to which Date::Date moves the day (0x54a2d0: up to 3 days either way, season_uefa_days.h). */

static long days_from_civil(long y, int m, int d)    /* m 1 to 12; days from 1 January 1970 */
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

/* The edition whose format a class plays in an edition year. With changing rules on, the edition of that year.
 * With changing rules off, the rules of the start season stay (Q232): the first edition from the start season
 * gives the size, the golden goal and the time of year of every edition. NULL outside a season game. */
const SeasonIntlEdition *season_intl_rule_edition(const char *file, int year)
{
    if (!season_intl_on()) return NULL;
    if (season_changing_rules()) return season_intl_edition(file, year);
    return season_intl_first(file, season_start_year());
}

/* The Confederations Cup in June (0.6.2 open items 157 to 159, original-faults group confed, original_faults.c).
 * The original's template plays it from 29 July (League_Stage date 0x68a160 at 0x58b601) to 9 August (0x521eb0 at
 * 0x58b984), in the year +0x40 that steps by 2 for ever (0x58c050), and its draw on 20 February (Date::Date 0x549f70
 * at 0x58be40). Those days are the original's Club World Championship (late July to August in every odd year) and
 * the European club qualifiers: in the changing-rules-off games of run set pass 3, 12 clubs of the Champions Cup,
 * the Inter-Toto Cup and the Club World Championship played the day before, of or after a match of a side that held
 * 3 to 15 of their players (NationalTeams::get_international_duty_club 0x7757d0 holds a player from the day before
 * his side's match to the day after). Every real edition started in mid-June, after the June FIFA days: 2003 18 to
 * 29 June (https://en.wikipedia.org/wiki/2003_FIFA_Confederations_Cup), 2005 15 to 29 June
 * (https://en.wikipedia.org/wiki/2005_FIFA_Confederations_Cup), 2009 14 to 28 June
 * (https://en.wikipedia.org/wiki/2009_FIFA_Confederations_Cup), 2013 15 to 30 June
 * (https://en.wikipedia.org/wiki/2013_FIFA_Confederations_Cup), 2017 17 June to 2 July
 * (https://en.wikipedia.org/wiki/2017_FIFA_Confederations_Cup). So an edition that keeps the original's dates (every
 * edition of a game with the season code's international rules off, and the 2003 edition that a 2001 game builds
 * at the new game) moves its first group day to the Saturday (the template's weekday code 5) on or before 18 June
 * of its year, the first day of the 2003 edition. A whole shift keeps every template date on its own code
 * (Date::Date moves each date to its code, 0x54a2d0): the groups on 29 July, 1 and 4 August (codes 5, 1, 4: 0x58b601,
 * 0x58b62c, 0x58b658) and the knockout up to 9 August (0x58b984) come 3, 6 and up to 11 days after the first. So
 * the edition plays from 12 to 18 June to 23 to 29 June (2003: 14 to 25 June), all in June as the real editions
 * mostly did. */
static int confed_june_shift(int year)
{
    const SeasonIntlEdition *e = season_intl_table_edition(class_files[INTL_CONFED], 2003);
    if (!e || e->month < 1) {                   /* the rules table always holds it (int-world-22): a broken table */
        static int said;
        if (!said++) fprintf(stderr, "recomp: season intl: no date of the Confederations Cup 2003 in the table, so "
                                     "group confed keeps the original's dates\n");
        return 0;
    }
    long from = days_from_civil(year, class_first[INTL_CONFED].month0 + 1, class_first[INTL_CONFED].day);
    long to = days_from_civil(year, e->month, e->day > 0 ? e->day : 1);
    int code = class_first[INTL_CONFED].code;
    int wd = (int)(((to + 3) % 7 + 7) % 7), back = ((wd - code) % 7 + 7) % 7;    /* 1 January 1970: a Thursday */
    return (int)(to - back - from);
}

int season_intl_date_shift(uint32_t cls, int year)
{
    if (cls >= INTL_CLASSES) return 0;
    int confed = cls == INTL_CONFED && season_intl_confed_june();
    if (!season_intl_on()) return confed ? confed_june_shift(year) : 0;
    const SeasonIntlEdition *e = season_intl_rule_edition(class_files[cls], year);
    if (!e || e->month < 1) return 0;
    /* A game that starts in 2001/02 keeps the dates of the editions it builds at the new game (Q152: the World
     * Cup 2002, the African Cup of Nations 2002, Euro 2004 and the others of a 2001 game with changing rules on play
     * as in the base game). The Confederations Cup 2003 plays as in the base game too: in June with group confed. */
    if (start_edition_2001(cls, year)) return confed ? confed_june_shift(year) : 0;
    int day = class_first[cls].day, month0 = class_first[cls].month0, code = class_first[cls].code;
    if (cls == INTL_OFC && year % 4 != 2) {
        day = 19;
        month0 = 5;
        code = 0;
    }
    long from = days_from_civil(year, month0 + 1, day);
    long to = days_from_civil(year + (e->start_year - e->year), e->month, e->day > 0 ? e->day : 1);
    /* An edition of another year's date (after the table: the copy of the last table edition; changing rules off:
     * the start season's edition) gets that day in its own year, on any weekday. Date::Date moves every template date
     * up to 3 days to its weekday code, and the codes of neighbouring rounds differ by a day in places (the
     * knockout of 48, season_wc48.c, keeps the original's codes 8 days later), so in some years one date moved 3
     * days forward and the next 3 back. The World Cup of 2066 (a 2006 game of run set pass 2) then met its groups
     * with its round of 32, and the game moved matches a week at a time to 4 August, into the UEFA club qualifiers
     * (0.6.2 open item 96). Such an edition moves up to 3 days more, so its first group day is on the template's code
     * and every date stays within a day of its own. */
    if (e->year != year || !season_intl_table_edition(class_files[cls], year)) {
        int wd = (int)(((to + 3) % 7 + 7) % 7), k = ((code - wd) % 7 + 7) % 7;   /* 1 January 1970: a Thursday */
        to += k > 3 ? k - 7 : k;
    }
    return (int)(to - from);
}

/* ---- The days of the finals (0.6.2 slice 6) ----
 *
 * The qualifiers keep the original's calendar moved to each cycle, while the finals take the table's years and
 * days. In long game seed 6 (a 2001 game, changing rules on, 0.6.0) the CAF World Cup qualifiers of 26 January 2013
 * met the African Cup of Nations of 19 January 2013. The finalists' group matches of that week moved to later days
 * (add_fixture 0x5aa680 moves a clashing fixture at 0x5aa96e and adds it again at 0x5aa978), the last group ended
 * on 5 February, the knockout rounds came a week apart, and no final was set. The class still held that edition
 * in the save of 2022 (run-logs/release-test-0.6.0/long-games/soak-seed6). The Copa America of 2004 in the same
 * game met CONMEBOL qualifiers on 17 and 24 July (the fixtures of its save of July 2004) and stopped the same way.
 * season_friendly_dates.c keeps a qualifier off these days. */

/* The step of each class's start_new_season (tools/recomp/seasons.py, the sites 0x92d409 to 0x7a04b0). */
static const int class_step[INTL_CLASSES] = {4, 4, 2, 2, 2, 4, 2, 4};

/* The confederations whose nations play each class (bits 1 << SEASON_CONF_...). */
#define ALL_CONFS ((1 << SEASON_CONFS) - 1)
static const int class_confs[INTL_CLASSES] = {
    ALL_CONFS, 1 << SEASON_CONF_EUROPE, 0, (1 << SEASON_CONF_SOUTH_AMERICA) | (1 << SEASON_CONF_NORTH_AMERICA),
    1 << SEASON_CONF_AFRICA, 1 << SEASON_CONF_ASIA, 1 << SEASON_CONF_OCEANIA, 0,
};

/* The days from the first group day to the final for a team count: the real spans (2013 ACN of 16 teams 22 days,
 * Copa America Centenario of 16 23, Euro 2016 of 24 30, 2014 World Cup of 32 31, 2026 World Cup of 48 38;
 * int-continental and int-world rows) and some days more. Inference: the game's own finals fit in these spans
 * (the 2012 ACN of seed 6 played 21 January to 11 February, 21 days). The ACN and Asian Cup of 24
 * play their knockout 12 days after the original's and up to 14 days more (season_intl_cups.c LATER_ROUNDS and
 * MAX_DELAY), and Date::Date moves a date up to 3 days to its weekday: the ACN final can come 31 + 12 + 3 days after
 * the first group day, and 14 more. */
static int finals_span(uint32_t cls, int teams)
{
    if (teams > 16 && teams <= 24 && (cls == INTL_ACN || cls == INTL_ASIAN)) return 60;
    return teams <= 16 ? 30 : teams <= 24 ? 37 : teams <= 32 ? 38 : 45;
}

/* The first and last day (days from 1 January 1970) of the finals of class cls in edition year `year`, when the
 * class plays one in this game: with changing rules on, an edition of the table from the start season; with it
 * off, the first edition from the start season and every step after it (rt_intl_next_year). */
static int finals_days(uint32_t cls, int year, long *from, long *to)
{
    const char *file = class_files[cls];
    const SeasonIntlEdition *rule = season_intl_rule_edition(file, year);
    if (!rule) return 0;
    if (season_changing_rules()) {
        if (rule->season < season_start_year()) return 0;
    } else if (year < rule->year || (year - rule->year) % class_step[cls]) {
        return 0;
    }
    int day = class_first[cls].day, month0 = class_first[cls].month0;
    if (cls == INTL_OFC && year % 4 != 2) {
        day = 19;
        month0 = 5;
    }
    *from = days_from_civil(year, month0 + 1, day) + season_intl_date_shift(cls, year);
    *to = *from + finals_span(cls, rule->teams);
    return 1;
}

int season_intl_finals_first_day(uint32_t cls, int year, long *day)
{
    long to;
    return cls < INTL_CLASSES && finals_days(cls, year, day, &to);
}

long season_intl_abs_day(int y, int doy) { return days_from_civil(y, 1, 1) + doy; }

int season_intl_finals_busy(int conf, int y, int doy)
{
    if (!season_intl_on() || conf < 0 || conf >= SEASON_CONFS) return 0;
    long day = days_from_civil(y, 1, 1) + doy, from, to;
    for (uint32_t cls = 0; cls < INTL_CLASSES; cls++) {
        if (!(class_confs[cls] & (1 << conf))) continue;
        for (int year = y - 1; year <= y + 1; year++)
            if (finals_days(cls, year, &from, &to) && day >= from - 2 && day <= to) return 1;
    }
    return 0;
}

const SeasonNlGoldCup *season_intl_gold_edition(int year)
{
    static SeasonNlGoldCup repeat;
    const SeasonNlGoldCup *base = NULL;
    if (season_changing_rules()) {
        for (int i = 0; i < season_nl_gold_cup_count; i++) {
            if (season_nl_gold_cups[i].year == year) return &season_nl_gold_cups[i];
            if (season_nl_gold_cups[i].year < year) base = &season_nl_gold_cups[i];
        }
    } else {
        for (int i = 0; i < season_nl_gold_cup_count && !base; i++)
            if (season_nl_gold_cups[i].year > season_start_year()) base = &season_nl_gold_cups[i];
        if (!base && season_nl_gold_cup_count) base = &season_nl_gold_cups[season_nl_gold_cup_count - 1];
        if (base && base->year == year) return base;
        if (base && base->year > year) base = NULL;
    }
    if (!base || (year - base->year) % 2) return NULL;
    repeat = *base;
    repeat.year = year;
    return &repeat;
}

/* The Gold Cup's first day is the edition's (season_gold_cup_date_shift moves the class's first group day, 6 July,
 * to it). Only an edition after the start year is played (gold_first_after in season_gold_cup.c). */
int season_intl_gold_busy(int y, int doy)
{
    if (!season_intl_on()) return 0;
    long day = days_from_civil(y, 1, 1) + doy;
    for (int year = y - 1; year <= y + 1; year++) {
        const SeasonNlGoldCup *g = year > season_start_year() ? season_intl_gold_edition(year) : NULL;
        if (!g || g->start.month < 1) continue;
        long from = days_from_civil(year + g->start.year, g->start.month, g->start.day > 0 ? g->start.day : 1);
        if (day >= from - 2 && day <= from + finals_span(INTL_COPA, g->teams)) return 1;
    }
    return 0;
}

void season_intl_shift_date(int shift, int *day, int *month0, int *year)
{
    if (!shift) return;
    int y, m, d;
    civil_from_days(days_from_civil(*year, *month0 + 1, *day) + shift, &y, &m, &d);
    *day = d;
    *month0 = m - 1;
    *year = y;
}

/* ---- Nation records ---- */

/* The renamed nations. A nation record (nation.dat, 0x122 bytes) holds the long name at +4 (51 bytes), the short
 * name at +0x38 (26), the code at +0x53 (4) and the adjective at +0x57 (26; the continent follows at +0x71), read
 * from the 3.9.68 nation.dat (Yugoslavia, row 209). Every season database keeps the record "Yugoslavia" and adds
 * Serbia, Montenegro and North Macedonia as new records when it has them (the nation.dat of the 49 databases),
 * so a rename applies only when the data holds the old name and no record with a later name. */
const SeasonIntlRename season_intl_renames[] = {
    {2002, "Yugoslavia", "Serbia and Montenegro", "Serbia & Montenegro", "SCG", "Serbian", "int-world-13"},
    {2006, "Serbia and Montenegro", "Serbia", "Serbia", "SRB", "Serbian", "int-world-61"},
    {2011, "Netherlands Antilles", "Cura\xe7" "ao", "Cura\xe7" "ao", "CUW", "Cura\xe7" "aoan", "int-world-325"},
    {2016, "Czech Republic", "Czechia", "Czechia", "CZE", "Czech", "int-world-181"},
    {2017, "Swaziland", "Eswatini", "Eswatini", "SWZ", "Swazi", "int-world-193"},
    {2018, "FYR of Macedonia", "North Macedonia", "North Macedonia", "MKD", "Macedonian", "int-world-205"},
    {2022, "Turkey", "T\xfcrkiye", "T\xfcrkiye", "TUR", "Turkish", "int-world-253"},
};
const int season_intl_rename_count = (int)(sizeof season_intl_renames / sizeof season_intl_renames[0]);

int season_intl_later_held(int i, int (*held)(const char *name, void *ctx), void *ctx)
{
    if (i < 0 || i >= season_intl_rename_count) return 0;
    const char *n = season_intl_renames[i].name;
    for (int step = 0; n && step <= season_intl_rename_count; step++) {
        if (held(n, ctx)) return 1;
        const char *next = NULL;
        for (int k = 0; k < season_intl_rename_count; k++)
            if (!strcasecmp(season_intl_renames[k].old, n)) next = season_intl_renames[k].name;
        n = next;
    }
    return 0;
}

int season_intl_uefa_from(const char *name)
{
    /* UEFA Congress of 3 May 2016 (docs/research/rules-0.6.0/uefa-access.tsv, the 2016/17 rows: "Kosovo (55):
     * admitted May 2016"); the 2018-19 Nations League has Kosovo in League D (nations-league-tiers.tsv). */
    if (!name) return 0;
    if (!strcasecmp(name, "Kosovo")) return 2016;
    /* UEFA Congress of 25 April 2002 in Stockholm: Kazakhstan moves from the AFC (tests/seasons/future/nations.tsv;
     * https://en.wikipedia.org/wiki/Kazakhstan_national_football_team, read 2026-10-30). The 2002/03 season is
     * the first after it. */
    if (!strcasecmp(name, "Kazakhstan")) return 2002;
    return 0;
}

int season_intl_afc_from(const char *name)
{
    /* Timor-Leste: AFC member from 2005 (the-afc.com, member associations; FIFA member from 12 September 2005). The
     * 3.9.68 data holds it as "Timor" with no continent (original_faults.c rt_national_vacancy), so it plays no
     * match (item 200 of 0.6.2). From 2005/06 it plays in the AFC. */
    if (!name) return 0;
    if (!strcasecmp(name, "Timor") || !strcasecmp(name, "Timor-Leste") || !strcasecmp(name, "East Timor")) return 2005;
    return 0;
}

int season_intl_concacaf_from(const char *name)
{
    /* Martinique: a CONCACAF member, not a FIFA member (an overseas department of France). Associate member from
     * 1991 and full member from 2013 (https://concacaf.com/inside-concacaf/member-associations/martinique; Wikipedia
     * gives 1964 or 1978 for the associate membership, read 2026-11-05): a member in every season of the game. It
     * played the Gold Cup of 2002, 2003, 2013, 2017, 2019, 2021 and 2023 and League A of the CONCACAF Nations League
     * in 2019-20, 2022-23 and 2023-24. The databases from October 2016 hold it with no continent (open item 141). */
    if (!name) return 0;
    if (!strcasecmp(name, "Martinique")) return 1991;
    /* The other CONCACAF members that are not FIFA members (Wikipedia, CONCACAF; read 2026-11-05). The congress of
     * 19 April 2013 made French Guiana, Guadeloupe, Martinique, Saint-Martin and Sint Maarten full members.
     * Guadeloupe: associate member from 1964. French Guiana: associate member for more than two decades before
     * 2013 (CONCACAF), so before the game's first season (the year 2001 is the game's first, not the joining year).
     * Saint-Martin and Sint Maarten: associate members from 21 April 2002, so from 2002/03. Bonaire: associate member
     * from April 2013, full member from 10 June 2014, so from 2013/14. The databases from October 2016 hold all five
     * with no continent. */
    if (!strcasecmp(name, "Guadeloupe")) return 1964;
    if (!strcasecmp(name, "French Guiana")) return 2001;
    if (!strcasecmp(name, "Saint-Martin") || !strcasecmp(name, "Sint Maarten")) return 2002;
    if (!strcasecmp(name, "Bonaire")) return 2013;
    return 0;
}

/* ---- The real draws of finals whose qualifying began before the game (O259 rows A9 and A16) ---- */

/* The finalists of each World Cup and European Championship that a game can start into after its qualifying began (a
 * start in the finals' season or the one before, Q388: fill_allowed of season_intl.c). Names of the data; '/' gives
 * the other names of a nation, first the one of its season (the renames of season_intl_renames). Sources (read
 * 2026-10-23): https://en.wikipedia.org/wiki/2006_FIFA_World_Cup_qualification and the pages of 2010, 2014, 2018 and
 * 2022 (the qualified teams), https://en.wikipedia.org/wiki/UEFA_Euro_2004_qualifying and the pages of 2008, 2012,
 * 2016, 2020 and 2024; for 2026 https://en.wikipedia.org/wiki/2026_FIFA_World_Cup_qualification (the 48 qualified
 * teams, read 2026-10-23; O274). Every name is a live record with a continent in each database of the seasons two
 * before to one after its edition (the O188 sweep script, natinv/real.py). Euro 2028 is not here: its qualifying
 * starts in 2027, after every start season, so the game plays it in full; a start that meets it short keeps the logged
 * reputation fill (a data default).
 */
static const struct { const char *file; int year; const char *teams; } real_draws[] = {
    {"world_cup", 2006, "Germany;Argentina;Brazil;Ecuador;Paraguay;Costa Rica;Mexico;United States;"
                        "Trinidad & Tobago/Trinidad and Tobago;Angola;Ivory Coast;Ghana;Togo;Tunisia;Australia;"
                        "Iran;Japan;Saudi Arabia;South Korea;Croatia;Czech Republic/Czechia;England;France;Italy;"
                        "Holland/Netherlands;Poland;Portugal;Serbia and Montenegro/Serbia/Yugoslavia;Spain;"
                        "Sweden;Switzerland;Ukraine"},
    {"world_cup", 2010, "South Africa;Algeria;Cameroon;Ghana;Ivory Coast;Nigeria;Australia;Japan;"
                        "North Korea/Korea DPR;South Korea;New Zealand;Honduras;Mexico;United States;Argentina;"
                        "Brazil;Chile;Paraguay;Uruguay;Denmark;England;France;Germany;Greece;Italy;"
                        "Holland/Netherlands;Portugal;Serbia/Serbia and Montenegro/Yugoslavia;Slovakia;Slovenia;"
                        "Spain;Switzerland"},
    {"world_cup", 2014, "Brazil;Algeria;Cameroon;Ghana;Ivory Coast;Nigeria;Australia;Iran;Japan;South Korea;"
                        "Costa Rica;Honduras;Mexico;United States;Argentina;Chile;Colombia;Ecuador;Uruguay;"
                        "Belgium;Bosnia-Herzegovina/Bosnia and Herzegovina;Croatia;England;France;Germany;Greece;"
                        "Italy;Holland/Netherlands;Portugal;Russia;Spain;Switzerland"},
    {"world_cup", 2018, "Russia;Egypt;Morocco;Nigeria;Senegal;Tunisia;Australia;Iran;Japan;Saudi Arabia;"
                        "South Korea;Costa Rica;Mexico;Panama;Argentina;Brazil;Colombia;Peru;Uruguay;Belgium;"
                        "Croatia;Denmark;England;France;Germany;Iceland;Poland;Portugal;"
                        "Serbia/Serbia and Montenegro/Yugoslavia;Spain;Sweden;Switzerland"},
    {"world_cup", 2022, "Qatar;Cameroon;Ghana;Morocco;Senegal;Tunisia;Australia;Iran;Japan;Saudi Arabia;"
                        "South Korea;Canada;Costa Rica;Mexico;United States;Argentina;Brazil;Ecuador;Uruguay;"
                        "Belgium;Croatia;Denmark;England;France;Germany;Holland/Netherlands;Poland;Portugal;"
                        "Serbia/Serbia and Montenegro/Yugoslavia;Spain;Switzerland;Wales"},
    {"world_cup", 2026, "Canada;Mexico;United States;Japan;Iran;Uzbekistan;Jordan;South Korea;Australia;Qatar;"
                        "Saudi Arabia;Iraq;Morocco;Tunisia;Egypt;Algeria;Ghana;Cape Verde Islands/Cape Verde;Senegal;"
                        "South Africa;Ivory Coast;Democratic Republic of Congo/DR Congo;Panama;Cura\xe7" "ao;Haiti;"
                        "Argentina;Brazil;Ecuador;Paraguay;Uruguay;Colombia;New Zealand;England;France;Croatia;"
                        "Portugal;Norway;Germany;Holland/Netherlands;Switzerland;Scotland;Spain;Austria;Belgium;"
                        "Bosnia-Herzegovina/Bosnia and Herzegovina;Sweden;T\xfcrkiye/Turkey;Czechia/Czech Republic"},
    {"euro_champ", 2004, "Portugal;Bulgaria;Croatia;Czech Republic/Czechia;Denmark;England;France;Germany;Greece;"
                         "Italy;Latvia;Holland/Netherlands;Russia;Spain;Sweden;Switzerland"},
    {"euro_champ", 2008, "Austria;Switzerland;Croatia;Czech Republic/Czechia;France;Germany;Greece;Italy;"
                         "Holland/Netherlands;Poland;Portugal;Romania;Russia;Spain;Sweden;Turkey/T\xfcrkiye"},
    {"euro_champ", 2012, "Poland;Ukraine;Croatia;Czech Republic/Czechia;Denmark;England;France;Germany;Greece;"
                         "Italy;Holland/Netherlands;Portugal;Republic of Ireland;Russia;Spain;Sweden"},
    {"euro_champ", 2016, "France;Albania;Austria;Belgium;Croatia;Czech Republic/Czechia;England;Germany;Hungary;"
                         "Iceland;Italy;Northern Ireland;Poland;Portugal;Republic of Ireland;Romania;Russia;"
                         "Slovakia;Spain;Sweden;Switzerland;Turkey/T\xfcrkiye;Ukraine;Wales"},
    {"euro_champ", 2020, "Austria;Belgium;Croatia;Czech Republic/Czechia;Denmark;England;Finland;France;Germany;"
                         "Hungary;Italy;Holland/Netherlands;North Macedonia/FYR of Macedonia;Poland;Portugal;"
                         "Russia;Scotland;Slovakia;Spain;Sweden;Switzerland;Turkey/T\xfcrkiye;Ukraine;Wales"},
    {"euro_champ", 2024, "Germany;Albania;Austria;Belgium;Croatia;Czech Republic/Czechia;Denmark;England;France;"
                         "Georgia;Hungary;Italy;Holland/Netherlands;Poland;Portugal;Romania;Scotland;"
                         "Serbia/Serbia and Montenegro/Yugoslavia;Slovakia;Slovenia;Spain;Switzerland;"
                         "Turkey/T\xfcrkiye;Ukraine"},
};

/* The inter-confederation play-offs of the same World Cups: the AFC team, its partner, then the other tie in
 * the order of season_intl_qual.c's ties table (the Oceania stage 2 pair). Sources: the qualification pages
 * above, section "Inter-confederation play-offs". 2026 is the play-off tournament of six (season_wcq26.c,
 * tournament_six): the two seeds, then the semi-finals 2 v 5 and 3 v 4 (New Caledonia 0-1 Jamaica, Bolivia 2-1
 * Suriname, 26 March 2026; finals DR Congo 1-0 Jamaica, Iraq 2-1 Bolivia, 31 March 2026;
 * https://en.wikipedia.org/wiki/2026_FIFA_World_Cup_qualification_(inter-confederation_play-offs), read
 * 2026-10-23). */
static const struct { int year; const char *teams[6]; } real_ties[] = {
    {2006, {"Bahrain", "Trinidad & Tobago/Trinidad and Tobago", "Australia", "Uruguay"}},
    {2010, {"Bahrain", "New Zealand", "Costa Rica", "Uruguay"}},
    {2014, {"Jordan", "Uruguay", "New Zealand", "Mexico"}},
    {2018, {"Australia", "Honduras", "New Zealand", "Peru"}},
    {2022, {"Australia", "Peru", "New Zealand", "Costa Rica"}},
    {2026, {"Democratic Republic of Congo/DR Congo", "Iraq", "New Caledonia", "Bolivia", "Suriname/Surinam",
            "Jamaica"}},
};

const char *season_intl_real_draw(const char *file, int year)
{
    for (size_t i = 0; i < sizeof real_draws / sizeof real_draws[0]; i++)
        if (real_draws[i].year == year && file && !strcmp(real_draws[i].file, file)) return real_draws[i].teams;
    return NULL;
}

int season_intl_late_start(const char *file, int year)
{
    return season_intl_on() && season_start_year() >= year - 1 && season_intl_real_draw(file, year) != NULL;
}

const char *season_intl_real_tie(int year, int k)
{
    for (size_t i = 0; i < sizeof real_ties / sizeof real_ties[0]; i++)
        if (real_ties[i].year == year && k >= 0 && k < 6) return real_ties[i].teams[k];
    return NULL;
}
