/* Closed leagues: the clubs that fold and join in their real seasons (season_closed.h).
 *
 * MLS: docs/research/rules-0.6.0/arg-usa.tsv (rows arg-usa-217 and on, the notes of each season name the clubs that
 * join and fold) and docs/research/club-records-0.6.0/closed-leagues.tsv (one source for each new club). San Jose
 * Earthquakes stopped after 2005, when the club's players and staff moved to Houston, and came back in 2008.
 *
 * A-League: docs/research/rules-0.6.0/aus-jap-kor.tsv (rows aus-jap-kor-28 to -49, the notes of each season name the
 * clubs that join and leave). Edition Y is the season Y/Y+1. Football Kingz became New Zealand Knights in 2004, and
 * Newcastle United became Newcastle Jets, so the clubs of the older databases keep their place. */
#include <stdlib.h>
#include <string.h>
#include "season_closed.h"

#define FOREVER 9999
#define A_LEAGUE_FIRST 2005            /* row aus-jap-kor-28; the National Soccer League plays before it */
#define MAX_WORDS 4
#define A_LEAGUE_CAP 13                 /* AusNsl::create_table stores 13 rows (0x411b82) */

typedef struct {
    const char *league, *real;
    int from, to, from2, to2;           /* the seasons that it plays; a second spell when from2 is set */
    const char *words[MAX_WORDS];       /* lower case parts of its names in the databases */
} ClosedRow;

static const ClosedRow ROWS[] = {
    {"usa_mls", "Chicago Fire", 1998, FOREVER, 0, 0, {"chicago fire"}},
    {"usa_mls", "Colorado Rapids", 1996, FOREVER, 0, 0, {"colorado rapids"}},
    {"usa_mls", "Columbus Crew", 1996, FOREVER, 0, 0, {"columbus crew"}},
    {"usa_mls", "D.C. United", 1996, FOREVER, 0, 0, {"dc united", "d.c. united"}},
    {"usa_mls", "FC Dallas", 1996, FOREVER, 0, 0, {"dallas burn", "fc dallas"}},
    {"usa_mls", "Los Angeles Galaxy", 1996, FOREVER, 0, 0, {"galaxy"}},
    {"usa_mls", "New England Revolution", 1996, FOREVER, 0, 0, {"new england rev"}},
    {"usa_mls", "New York Red Bulls", 1996, FOREVER, 0, 0, {"metrostars", "red bull"}},
    {"usa_mls", "San Jose Earthquakes", 1996, 2005, 2008, FOREVER, {"san jose earthquakes", "san jos\xe9 earthquakes"}},
    {"usa_mls", "Sporting Kansas City", 1996, FOREVER, 0, 0, {"kansas city wizards", "sporting kansas", "sporting kc"}},
    {"usa_mls", "Miami Fusion", 1998, 2001, 0, 0, {"miami fusion"}},
    {"usa_mls", "Tampa Bay Mutiny", 1996, 2001, 0, 0, {"tampa bay mutiny"}},
    {"usa_mls", "Real Salt Lake", 2005, FOREVER, 0, 0, {"real salt lake"}},
    {"usa_mls", "Chivas USA", 2005, 2014, 0, 0, {"chivas usa"}},
    {"usa_mls", "Houston Dynamo", 2006, FOREVER, 0, 0, {"houston dynamo"}},
    {"usa_mls", "Toronto FC", 2007, FOREVER, 0, 0, {"toronto fc"}},
    {"usa_mls", "Seattle Sounders", 2009, FOREVER, 0, 0, {"seattle sounders"}},
    {"usa_mls", "Philadelphia Union", 2010, FOREVER, 0, 0, {"philadelphia union"}},
    {"usa_mls", "Portland Timbers", 2011, FOREVER, 0, 0, {"portland timbers"}},
    {"usa_mls", "Vancouver Whitecaps", 2011, FOREVER, 0, 0, {"vancouver whitecaps"}},
    {"usa_mls", "Montreal Impact", 2012, FOREVER, 0, 0, {"montreal impact", "montr\xe9" "al impact", "cf montr"}},
    {"usa_mls", "New York City FC", 2015, FOREVER, 0, 0, {"new york city"}},
    {"usa_mls", "Orlando City SC", 2015, FOREVER, 0, 0, {"orlando city"}},
    {"usa_mls", "Atlanta United FC", 2017, FOREVER, 0, 0, {"atlanta united"}},
    {"usa_mls", "Minnesota United FC", 2017, FOREVER, 0, 0, {"minnesota united"}},
    {"usa_mls", "Los Angeles FC", 2018, FOREVER, 0, 0, {"los angeles fc", "los angeles football", "lafc"}},
    {"usa_mls", "FC Cincinnati", 2019, FOREVER, 0, 0, {"fc cincinnati"}},
    {"usa_mls", "Inter Miami CF", 2020, FOREVER, 0, 0, {"inter miami"}},
    {"usa_mls", "Nashville SC", 2020, FOREVER, 0, 0, {"nashville sc"}},
    {"usa_mls", "Austin FC", 2021, FOREVER, 0, 0, {"austin fc"}},
    {"usa_mls", "Charlotte FC", 2022, FOREVER, 0, 0, {"charlotte fc"}},
    {"usa_mls", "St. Louis City SC", 2023, FOREVER, 0, 0, {"st. louis city", "st louis city", "saint louis city"}},
    {"usa_mls", "San Diego FC", 2025, FOREVER, 0, 0, {"san diego fc"}},
    {"aus_nsl", "Adelaide United", 2005, FOREVER, 0, 0, {"adelaide united"}},
    {"aus_nsl", "Brisbane Roar", 2005, FOREVER, 0, 0, {"queensland roar", "brisbane roar"}},
    {"aus_nsl", "Central Coast Mariners", 2005, FOREVER, 0, 0, {"central coast mariners"}},
    {"aus_nsl", "Melbourne Victory", 2005, FOREVER, 0, 0, {"melbourne victory"}},
    {"aus_nsl", "Newcastle Jets", 2005, FOREVER, 0, 0, {"newcastle united", "newcastle jets"}},
    {"aus_nsl", "New Zealand Knights", 2005, 2006, 0, 0, {"new zealand knights", "football kingz"}},
    {"aus_nsl", "Perth Glory", 2005, FOREVER, 0, 0, {"perth glory"}},
    {"aus_nsl", "Sydney FC", 2005, FOREVER, 0, 0, {"sydney fc", "sydney football club"}},
    {"aus_nsl", "Wellington Phoenix", 2007, FOREVER, 0, 0, {"wellington phoenix"}},
    {"aus_nsl", "Gold Coast United", 2009, 2011, 0, 0, {"gold coast united"}},
    {"aus_nsl", "North Queensland Fury", 2009, 2010, 0, 0, {"north queensland fury"}},
    {"aus_nsl", "Melbourne City", 2010, FOREVER, 0, 0, {"melbourne heart", "melbourne city"}},
    {"aus_nsl", "Western Sydney Wanderers", 2012, FOREVER, 0, 0, {"western sydney wanderers"}},
    {"aus_nsl", "Western United", 2019, 2024, 0, 0, {"western united"}},
    {"aus_nsl", "Macarthur FC", 2020, FOREVER, 0, 0, {"macarthur fc", "macarthur bulls"}},
    {"aus_nsl", "Auckland FC", 2024, FOREVER, 0, 0, {"auckland fc"}},
};
#define NROWS ((int)(sizeof ROWS / sizeof ROWS[0]))

static int lower(int ch) { return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch; }

static int has_part(const char *name, const char *part)
{
    size_t n = strlen(name), w = strlen(part);
    for (size_t i = 0; i + w <= n; i++) {
        size_t k = 0;
        while (k < w && lower((unsigned char)name[i + k]) == (unsigned char)part[k]) k++;
        if (k == w) return 1;
    }
    return 0;
}

static int row_names(const ClosedRow *r, const char *name)
{
    for (int k = 0; k < MAX_WORDS && r->words[k]; k++)
        if (has_part(name, r->words[k])) return 1;
    return 0;
}

static int row_plays(const ClosedRow *r, int season)
{
    return (season >= r->from && season <= r->to) || (r->from2 && season >= r->from2 && season <= r->to2);
}

int season_closed_row(const char *league, const char *name)
{
    for (int i = 0; league && name && i < NROWS; i++)
        if (!strcmp(ROWS[i].league, league) && row_names(&ROWS[i], name)) return i;
    return -1;
}

/* The short names of the update databases that hold no name part of their club (one has the spelling "Philadephia").
 * They count only for a record that the table names: "Toronto" is also the short name of Toronto Lynx. */
static const struct {
    const char *league, *short_name, *real;
} SHORTS[] = {
    {"usa_mls", "houston", "Houston Dynamo"},          {"usa_mls", "seattle", "Seattle Sounders"},
    {"usa_mls", "philadelphia", "Philadelphia Union"}, {"usa_mls", "philadephia", "Philadelphia Union"},
    {"usa_mls", "toronto", "Toronto FC"},              {"usa_mls", "nycfc", "New York City FC"},
    {"usa_mls", "atlanta utd", "Atlanta United FC"},   {"usa_mls", "portland", "Portland Timbers"},
};

static int same_text(const char *a, const char *lower_b)
{
    while (*a && lower((unsigned char)*a) == (unsigned char)*lower_b) a++, lower_b++;
    return !*a && !*lower_b;
}

int season_closed_club(const char *league, const char *name, const char *short_name)
{
    int row = season_closed_row(league, name);
    if (row < 0 || !short_name || !short_name[0]) return row;
    int by_short = season_closed_row(league, short_name);
    if (by_short >= 0) return by_short;
    for (size_t k = 0; k < sizeof SHORTS / sizeof SHORTS[0]; k++) {
        if (strcmp(SHORTS[k].league, league) || !same_text(short_name, SHORTS[k].short_name)) continue;
        for (int i = 0; i < NROWS; i++)
            if (!strcmp(ROWS[i].league, league) && !strcmp(ROWS[i].real, SHORTS[k].real)) return i;
    }
    return row;
}

const char *season_closed_real_name(const char *league, const char *name, const char *short_name)
{
    int row = season_closed_club(league, name, short_name);
    return row < 0 ? name : ROWS[row].real;
}

const char *season_closed_league(const char *division)
{
    if (division && !strncmp(division, "American Major League", 52)) return "usa_mls";
    if (division && !strncmp(division, "Australian National Soccer League", 52)) return "aus_nsl";
    return NULL;
}

int season_closed_active(const char *league, int season)
{
    return strcmp(league, "aus_nsl") || season >= A_LEAGUE_FIRST;
}

int season_closed_abroad(const char *league, const char *name, const char *short_name)
{
    static const char *const ABROAD[] = {"New Zealand Knights", "Wellington Phoenix", "Auckland FC"};
    int row = strcmp(league, "aus_nsl") ? -1 : season_closed_club(league, name, short_name);
    for (size_t k = 0; row >= 0 && k < sizeof ABROAD / sizeof ABROAD[0]; k++)
        if (!strcmp(ROWS[row].real, ABROAD[k])) return 1;
    return 0;
}

int season_closed_waits(const char *name, const char *short_name, int season)
{
    return season_closed_member("usa_mls", name, short_name, season) == 0 ||
           season_closed_member("aus_nsl", name, short_name, season) == 0;
}

int season_closed_member(const char *league, const char *name, const char *short_name, int season)
{
    int i = season_closed_club(league, name, short_name);
    return i < 0 ? -1 : row_plays(&ROWS[i], season);
}

int season_closed_count(const char *league, int season)
{
    int n = 0;
    for (int i = 0; i < NROWS; i++) n += !strcmp(ROWS[i].league, league) && row_plays(&ROWS[i], season);
    return n;
}

static const ClosedRow *league_row(const char *league, int i)
{
    for (int k = 0; k < NROWS; k++)
        if (!strcmp(ROWS[k].league, league) && i-- == 0) return &ROWS[k];
    return NULL;
}

const char *season_closed_real(const char *league, int i)
{
    const ClosedRow *r = league_row(league, i);
    return r ? r->real : NULL;
}

int season_closed_first(const char *league, int i)
{
    const ClosedRow *r = league_row(league, i);
    return r ? r->from : 0;
}

void season_closed_plan(const char *league, int season, int want, const ClosedClub *clubs, int n, int *actions,
                        ClosedResult *result)
{
    ClosedResult r = {0, 0, 0, 0, 0, 0};
    int active = season_closed_active(league, season);
    for (int i = 0; i < n; i++) {
        actions[i] = CLOSED_STAY;
        if (!clubs[i].in_league) continue;
        r.count++;
        if (!active) continue;
        if (season_closed_member(league, clubs[i].name, clubs[i].short_name, season) != 0) continue;
        if (clubs[i].human) {
            r.human_stays++;
            continue;
        }
        actions[i] = CLOSED_FOLD;
        r.folded++;
        r.count--;
    }
    if (!active) {
        *result = r;
        return;
    }
    /* Each club of the season that the league lacks: the club of the data with the highest reputation that has one
     * of its names. A reserve side never joins. */
    for (int k = 0; k < NROWS; k++) {
        const ClosedRow *row = &ROWS[k];
        if (strcmp(row->league, league) || !row_plays(row, season)) continue;
        int have = 0, best = -1;
        for (int i = 0; i < n && !have; i++) {
            if (season_closed_club(league, clubs[i].name, clubs[i].short_name) != k || clubs[i].reserve) continue;
            if (clubs[i].in_league) have = 1;
            else if (best < 0 || clubs[i].reputation > clubs[best].reputation) best = i;
        }
        if (have) continue;
        if (best < 0) {
            r.missing++;
            continue;
        }
        actions[best] = CLOSED_JOIN;
        r.joined++;
        r.count++;
    }
    /* A stand-in of the data leaves when the real clubs fill the league. */
    while (want > 0 && r.count > want) {
        int worst = -1;
        for (int i = 0; i < n; i++) {
            if (!clubs[i].in_league || actions[i] != CLOSED_STAY || clubs[i].human) continue;
            if (season_closed_member(league, clubs[i].name, clubs[i].short_name, season) >= 0) continue;
            if (worst < 0 || clubs[i].reputation < clubs[worst].reputation) worst = i;
        }
        if (worst < 0) break;
        actions[worst] = CLOSED_CUT;
        r.cut++;
        r.count--;
    }
    /* A human manager can keep a club beside the real clubs. The class makes no table row past its count, so a club
     * that would join waits a season, and then the club with the lowest reputation leaves. */
    int cap = strcmp(league, "aus_nsl") ? 0 : A_LEAGUE_CAP;
    while (cap > 0 && r.count > cap) {
        int drop = -1, joining = 0;
        for (int i = 0; i < n; i++)
            if (actions[i] == CLOSED_JOIN && (!joining || clubs[i].reputation < clubs[drop].reputation)) drop = i, joining = 1;
        for (int i = 0; i < n && !joining; i++)
            if (clubs[i].in_league && actions[i] == CLOSED_STAY && !clubs[i].human &&
                (drop < 0 || clubs[i].reputation < clubs[drop].reputation))
                drop = i;
        if (drop < 0) break;
        actions[drop] = joining ? CLOSED_STAY : CLOSED_CUT;
        r.joined -= joining;
        r.cut += !joining;
        r.count--;
    }
    *result = r;
}

int season_closed_places(const ClosedClub *clubs, const int *actions, const unsigned *from, const int *pool, int n,
                         unsigned *to)
{
    int filled = 0;
    for (int i = 0; i < n; i++) to[i] = 0;
    for (int j = 0; j < n; j++) {
        if (actions[j] != CLOSED_JOIN || !from[j]) continue;
        int best = -1;
        for (int i = 0; i < n; i++)
            if (actions[i] == CLOSED_CUT && !to[i] && (best < 0 || clubs[i].reputation > clubs[best].reputation)) best = i;
        int cut = best >= 0;
        for (int i = 0; !cut && pool && i < n; i++)
            if (pool[i] && actions[i] == CLOSED_STAY && !to[i] && (best < 0 || clubs[i].reputation > clubs[best].reputation))
                best = i;
        if (best < 0) break;
        to[best] = from[j];
        filled++;
    }
    return filled;
}
