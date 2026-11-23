/* The editions of the club cups of the rules table (0.6.2 slice 9a): season_club_editions.h. The date reader and the
 * edition of a row come from season_club_cups.c (0.6.0 part 26c), unchanged. */
#include "season_club_editions.h"
#include "season_uefa_days.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Dates of the table's text ---- */

static const char *const month_names[12] = {"January", "February", "March", "April", "May", "June", "July",
                                            "August", "September", "October", "November", "December"};

int season_club_is_leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

int season_club_day_of_year(int day, int month, int year)
{
    static const short start[12] = {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333};
    return start[month - 1] + day + (month > 2 && season_club_is_leap(year));
}

typedef struct { int day, month, year; } Day;

static int day_key(Day d) { return d.year * 400 + d.month * 32 + d.day; }

/* The dates of a text part: each "<day>" with its own month, or followed by "and", "-" or an en dash and another
 * day; a missing month or year comes from the next date that has one. Returns the first and last date, 0 when
 * none. Numbers of other meaning (club counts, "2007 J.League Cup") have no month after them and are not days. */
static int text_dates(const char *t, size_t len, int default_year, Day *first, Day *last, int *open_end)
{
    *open_end = 0;
    Day d[32];
    int n = 0, joined[32] = {0};
    const char *end = t + len;
    for (const char *p = t; p < end && n < 32; p++) {
        if (*p < '0' || *p > '9' || (p > t && ((p[-1] >= '0' && p[-1] <= '9') || p[-1] == '-' || p[-1] == '.')))
            continue;
        const char *q = p;
        int v = 0, digits = 0;
        while (q < end && *q >= '0' && *q <= '9') v = v * 10 + (*q++ - '0'), digits++;
        if (digits > 2 || v < 1 || v > 31) { p = q - 1; continue; }
        Day x = {v, 0, 0};
        if (q < end && *q == ' ')
            for (int m = 0; m < 12; m++) {
                size_t ml = strlen(month_names[m]);
                if ((size_t)(end - q - 1) >= ml && !strncmp(q + 1, month_names[m], ml)) { x.month = m + 1; q += 1 + ml; break; }
            }
        if (x.month && q + 5 <= end && q[0] == ' ' && q[1] >= '1' && q[1] <= '2' && q[2] >= '0' && q[2] <= '9' &&
            q[3] >= '0' && q[3] <= '9' && q[4] >= '0' && q[4] <= '9') {
            x.year = (q[1] - '0') * 1000 + (q[2] - '0') * 100 + (q[3] - '0') * 10 + (q[4] - '0');
            q += 5;
        }
        /* A connector to another day: " and ", " - ", " – " (UTF-8 en dash). */
        const char *r = q;
        while (r < end && *r == ' ') r++;
        int link = 0;
        if (r + 3 <= end && !strncmp(r, "and", 3)) link = 3;
        else if (r < end && *r == '-') link = 1;
        else if (r + 3 <= end && (unsigned char)r[0] == 0xe2 && (unsigned char)r[1] == 0x80 && (unsigned char)r[2] == 0x93)
            link = 3;
        int linked = 0;
        if (link) {
            const char *s = r + link;
            while (s < end && *s == ' ') s++;
            linked = s < end && *s >= '0' && *s <= '9';
            /* "26 August – December 2026": the end has a month and no day, so the last date is not known. */
            for (int m = 0; x.month && !linked && m < 12; m++)
                if ((size_t)(end - s) >= strlen(month_names[m]) && !strncmp(s, month_names[m], strlen(month_names[m])))
                    *open_end = 1;
        }
        if (!x.month && !linked) { p = q - 1; continue; }
        joined[n] = linked;
        d[n++] = x;
        p = q - 1;
    }
    /* A day with no month must be joined to the next day (the one that gives the month). */
    for (int i = n - 2; i >= 0; i--) {
        if (!d[i].month && joined[i]) d[i].month = d[i + 1].month;
        if (!d[i].year && d[i].month) d[i].year = d[i + 1].year;
    }
    int k = 0;
    for (int i = 0; i < n; i++)
        if (d[i].month) {
            if (!d[i].year) d[i].year = default_year;
            d[k++] = d[i];
        }
    if (!k) return 0;
    *first = *last = d[0];
    for (int i = 1; i < k; i++) {
        if (day_key(d[i]) < day_key(*first)) *first = d[i];
        if (day_key(d[i]) > day_key(*last)) *last = d[i];
    }
    return 1;
}

/* ---- The edition of a year ---- */

static const SeasonRule *row_of(const char *comp, int s)
{
    const SeasonRule *best = NULL;
    for (int i = 0; i < season_rules_count(); i++) {
        const SeasonRule *r = season_rules_row(i);
        if (strcmp(r->comp, comp) || r->season > s) continue;
        if (!best || r->season > best->season) best = r;
    }
    return best;
}

static int is_held(const SeasonRule *r) { return r->clubs != SEASON_RULE_NOT_HELD && strcmp(r->format, "not_held"); }

/* Part `i` of a text split by `sep`: start and length, 0 when there is no such part. */
static int part_of(const char *t, const char *sep, int i, const char **start, size_t *len)
{
    const char *p = t;
    for (int k = 0; k < i; k++) {
        p = strstr(p, sep);
        if (!p) return 0;
        p += strlen(sep);
    }
    const char *e = strstr(p, sep);
    *start = p;
    *len = e ? (size_t)(e - p) : strlen(p);
    return 1;
}

static int has(const char *t, size_t len, const char *word)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%.*s", (int)(len < sizeof buf - 1 ? len : sizeof buf - 1), t);
    return strstr(buf, word) != NULL;
}

/* The edition of row r, part i: 1 when its dates are known. */
static int edition_of_part(const SeasonClubCup *k, const SeasonRule *r, int i, int default_year, SeasonClubEdition *e)
{
    const char *t, *c;
    size_t len, clen;
    if (!r->format_text || !part_of(r->format_text, "||", i, &t, &len)) return 0;
    if (k->skip_first_clause) {
        const char *semi = memchr(t, ';', len);
        if (semi) { len -= (size_t)(semi + 1 - t); t = semi + 1; }
    }
    Day a, b;
    int open_end;
    if (!text_dates(t, len, default_year, &a, &b, &open_end)) return 0;
    /* A late edition (the Sudamericana of 2020, final 23 January 2021) plays at the normal time (Q277): its final on
     * 20 December, so the edition ends before the entry's update day of 10 January and its season start runs. */
    if (k->calendar && b.year > a.year) b = (Day){20, 12, a.year};
    e->year = a.year;
    e->last_year = b.year;
    e->first = season_club_day_of_year(a.day, a.month, a.year);
    e->last = season_club_day_of_year(b.day, b.month, b.year);
    e->clubs = r->clubs > 0 ? r->clubs : 0;
    if (r->clubs_text && part_of(r->clubs_text, ";", i, &c, &clen)) e->clubs = atoi(c);   /* "47; 44+10" */
    e->legs = has(t, len, "two-legged") || has(t, len, "two legs") ? 2 : 1;
    e->final_legs = e->legs == 2 && !has(t, len, "single-match final") ? 2 : 1;
    e->row = r;
    e->open_end = open_end;
    return e->clubs >= 2;
}

static int edition_in_table(const SeasonClubCup *k, int y, SeasonClubEdition *e);

/* An edition whose text gives no last day ("26 August – December 2026", the FIFA Intercontinental Cup of 2026/27)
 * takes the days and months of the edition before, moved to its year; with none before it is not known. */
int season_club_edition_in_table(const SeasonClubCup *k, int y, SeasonClubEdition *e)
{
    if (!edition_in_table(k, y, e)) return 0;
    if (!e->open_end) return 1;
    SeasonClubEdition before;
    for (int x = y - 1; x >= SEASON_RULE_FIRST && x >= y - 4; x--)
        if (edition_in_table(k, x, &before) && !before.open_end) {
            season_club_edition_move(&before, y - x);
            e->first = before.first, e->last = before.last, e->year = before.year, e->last_year = before.last_year;
            e->open_end = 0;
            return 1;
        }
    return 0;
}

static int edition_in_table(const SeasonClubCup *k, int y, SeasonClubEdition *e)
{
    if (!k->calendar) {
        const SeasonRule *r = row_of(k->comp, y);
        if (!r || !is_held(r)) return 0;
        return edition_of_part(k, r, 0, y, e);
    }
    for (int s = y; s >= y - 1; s--) {
        const SeasonRule *r = row_of(k->comp, s);
        if (!r || r->season != s || !is_held(r)) continue;
        for (int i = 0; i < 4; i++)
            if (edition_of_part(k, r, i, y, e) && e->year == y) return 1;
    }
    return 0;
}

/* The last season of a row of comp, 0 when none. */
static int last_row_season(const char *comp)
{
    const SeasonRule *r = row_of(comp, 1 << 30);
    return r ? r->season : 0;
}

int season_club_last_edition(const SeasonClubCup *k)
{
    int last = last_row_season(k->comp);
    SeasonClubEdition e;
    /* A row of season s holds editions of s and s + 1 at most (a calendar cup's part, a season cup's spring). */
    for (int y = last + 1; last && y >= SEASON_RULE_FIRST; y--)
        if (season_club_edition_in_table(k, y, &e)) return y;
    return 0;
}

int season_club_continues(const SeasonClubCup *k)
{
    const SeasonRule *r = row_of(k->comp, 1 << 30);
    return r && is_held(r);
}

void season_club_edition_move(SeasonClubEdition *e, int years)
{
    Day a = {0}, b = {0};
    for (int m = 12; m >= 1 && !a.month; m--)
        if (e->first >= season_club_day_of_year(1, m, e->year))
            a = (Day){e->first - season_club_day_of_year(1, m, e->year) + 1, m, e->year};
    for (int m = 12; m >= 1 && !b.month; m--)
        if (e->last >= season_club_day_of_year(1, m, e->last_year))
            b = (Day){e->last - season_club_day_of_year(1, m, e->last_year) + 1, m, e->last_year};
    e->year += years;
    e->last_year += years;
    e->first = season_club_day_of_year(a.day, a.month, e->year);
    e->last = season_club_day_of_year(b.day, b.month, e->last_year);
}

int season_club_edition(const SeasonClubCup *k, int y, SeasonClubEdition *e)
{
    if (season_club_edition_in_table(k, y, e)) return 1;
    int last = season_club_last_edition(k);
    if (!last || y <= last || !season_club_continues(k) || !season_club_edition_in_table(k, last, e)) return 0;
    season_club_edition_move(e, y - last);
    return 1;
}

int season_club_next_edition(const SeasonClubCup *k, int y)
{
    SeasonClubEdition e;
    int last = season_club_last_edition(k);
    for (int x = y; x <= last; x++)
        if (season_club_edition_in_table(k, x, &e)) return x;
    if (!last || !season_club_continues(k)) return 0;
    return y > last ? y : last + 1;
}

/* ---- The Club World Cup after the table (Q3) ---- */

static const SeasonRule *latest_row(const char *file, int s, int held_only)
{
    for (int y = s; y >= SEASON_RULE_FIRST; y--) {
        const SeasonRule *r = season_rules_find(y, file, 0);
        if (r && (!held_only || is_held(r))) return r;
    }
    return NULL;
}

const SeasonRule *season_club_world_row(const char *file, int s)
{
    const SeasonRule *r = latest_row(file, s, 0);
    if (!r || strcmp(file, "world_club_champ") || is_held(r)) return r;
    const SeasonRule *last = latest_row(file, SEASON_RULE_LAST, 0);
    if (s <= last->season) return r;                    /* inside the table: its own row */
    const SeasonRule *held = latest_row(file, last->season, 1);
    return held && (s - held->season) % SEASON_CWC_CYCLE == 0 ? held : r;
}

/* ---- The rounds (from season_club_cups.c, 0.6.0 part 26c; the FIFA days from 0.6.2 open item 95) ---- */

int season_club_plan_knockout(const SeasonClubEdition *e, int n, SeasonClubRound *r)
{
    int count = 0;
    int last = e->last + (e->last_year > e->year ? 365 + season_club_is_leap(e->year) : 0);
    int p = 1;
    while (p * 2 <= n) p *= 2;
    int teams = p;
    if (n > p) r[count++] = (SeasonClubRound){0x14, 2 * (n - p), n - p, 2 * (n - p), 0, e->legs, 0};
    for (; teams >= 2; teams /= 2) {
        int fresh = count == 0 ? teams : (n > p && count == 1 ? 2 * p - n : 0);
        int first = count == 1 && n > p ? 2 * (n - p) : 0;
        int id = teams == 2 ? 0x96 : teams == 4 ? 0x82 : teams == 8 ? 0x78 : 0x14;
        r[count++] = (SeasonClubRound){id, teams, teams / 2, fresh, first, teams == 2 ? e->final_legs : e->legs, 0};
    }
    /* Days: the final (its first leg) on the last day, or 7 days before for two legs; the rounds before it from
     * the first day, evenly. */
    int final_day = last - (r[count - 1].legs == 2 ? 7 : 0);
    for (int i = 0; i < count; i++)
        r[i].match = count == 1 ? e->first : i == count - 1 ? final_day : e->first + i * (final_day - e->first) / (count - 1);
    if (count == 1 && r[0].legs == 2 && last > e->first) r[0].match = e->first;
    return count;
}

int season_club_second_leg(const SeasonClubEdition *e, const SeasonClubRound *r, int count)
{
    if (r->legs != 2) return 0;
    return count == 1 && e->last > e->first ? e->last - e->first : 7;
}

/* 1 when day doy of year y (it may lie before or after the year) meets a FIFA day. */
static int meets_fifa_day(int y, int doy)
{
    while (doy < 0) doy += 365 + season_club_is_leap(--y);
    while (doy >= 365 + season_club_is_leap(y)) doy -= 365 + season_club_is_leap(y++);
    int m, d;
    season_from_day_of_year(y, doy, &m, &d);
    return season_fifa_meets(y, m, d);
}

int season_club_meets_duty(int y, int doy) { return meets_fifa_day(y, doy) || meets_fifa_day(y, doy + 1); }


int season_club_plan_rounds(const char *comp, const SeasonClubEdition *e, int n, SeasonClubRound *r)
{
    int count = 0;
    int last = e->last + (e->last_year > e->year ? 365 + season_club_is_leap(e->year) : 0);
    if (!strcmp(comp, "fifa_intercontinental_cup") && n == 6) {
        /* Club list: OFC, AFC, CAF, CONMEBOL, CONCACAF, UEFA (pick_intercontinental). The play-off of OFC and AFC,
         * then the African-Asian-Pacific Cup and the Derby of the Americas with CAF, CONMEBOL and CONCACAF new
         * (the play-off winner takes the empty slot), the Challenger Cup, the final with UEFA new. */
        int span = last - e->first;
        r[count++] = (SeasonClubRound){0x14, 2, 1, 2, 0, 1, e->first};
        r[count++] = (SeasonClubRound){0x78, 4, 2, 3, 2, 1, e->first + span / 3};
        r[count++] = (SeasonClubRound){0x82, 2, 1, 0, 0, 1, e->first + 2 * span / 3};
        r[count++] = (SeasonClubRound){0x96, 2, 1, 1, 5, 1, last};
    } else {
        count = season_club_plan_knockout(e, n, r);
    }
    /* Open item 95: the players of a club are on duty from the day before their side's match to the day after
     * (NationalTeams::get_international_duty_club 0x7757d0), so a match next to a FIFA day loses them. The original
     * adds a linked Under 21 or 'B' match on the day before each senior national match that is not a tournament
     * match, between nations of reputation above 2499 (ReserveComp::add_b_or_under21_fixture 0x7e6720, called from add_fixture 0x5aa680 at 0x5aab09), so the
     * players of those sides are on duty from two days before the FIFA day (open item 163: the Copa Sudamericana
     * of 8 August 2011, two days before the FIFA day of 10 August, lost 3 players of 12 de Octubre to Paraguay's
     * Under 21 match with Cameroon on 9 August). season_club_meets_duty holds both. Each round
     * (both legs) takes the nearest day that meets none, at most 21 days either way, the later one first, and never
     * before the round before it and its draw, nor before the first day. With no such day it keeps its day. */
    int after = e->first;                       /* round 0 never before the edition's first day */
    for (int i = 0; i < count; i++) {
        int gap = season_club_second_leg(e, &r[i], count), want = r[i].match < after ? after : r[i].match, m = want;
        for (int k = 0; k <= 21; k++) {
            int up = want + k, down = want - k;
            if (!season_club_meets_duty(e->year, up) && !(gap && season_club_meets_duty(e->year, up + gap))) { m = up; break; }
            if (k && down >= after && !season_club_meets_duty(e->year, down) && !(gap && season_club_meets_duty(e->year, down + gap))) {
                m = down;
                break;
            }
        }
        r[i].match = m;
        after = m + gap + 2;                    /* the next round's draw on the day after, its match the day after that */
    }
    return count;
}

/* ---- The Libertadores places ---- */

const SeasonRule *season_club_places_row(const char *comp, int s)
{
    const SeasonRule *best = NULL;
    for (int i = 0; i < season_rules_count(); i++) {
        const SeasonRule *r = season_rules_row(i);
        if (strcmp(r->comp, comp) || r->season > s || !r->format_text || !strstr(r->format_text, "places:")) continue;
        if (!best || r->season > best->season) best = r;
    }
    return best;
}
