/* The lookups of the UEFA formats table (season_uefa.h). It reads only the table and the season of the game
 * (season.h), so the ctest season_uefa links it with stand-ins of those three functions. */
#include "season.h"
#include "season_uefa.h"

#include <string.h>

const SeasonUefa *season_uefa_table_row(int season)
{
    if (season_uefa_row_count <= 0) return NULL;
    const SeasonUefa *best = &season_uefa_rows[0];
    for (int i = 0; i < season_uefa_row_count; i++)
        if (season_uefa_rows[i].season <= season) best = &season_uefa_rows[i];
    return best;
}

const SeasonUefa *season_uefa_row(int season)
{
    if (!season_code_on()) return NULL;
    int start = season_start_year();
    int changing = season_changing_rules();
    if (start == SEASON_FIRST_YEAR && !changing) return NULL;    /* the base game */
    return season_uefa_table_row(changing ? season : start);
}

static int round_clubs(const SeasonUefaRound *rs, int n, const char *name)
{
    for (int i = 0; i < n && i < SEASON_UEFA_ROUNDS; i++)
        if (rs[i].name && !strcmp(rs[i].name, name)) return rs[i].clubs;
    return 0;
}

static int level_clubs(const SeasonUefaRound *rs, int n, const char *name)
{
    size_t k = strlen(name);
    int sum = 0;
    for (int i = 0; i < n && i < SEASON_UEFA_ROUNDS; i++)
        if (rs[i].name && !strncmp(rs[i].name, name, k) && (rs[i].name[k] == 0 || rs[i].name[k] == ' '))
            sum += rs[i].clubs;
    return sum;
}

int season_uefa_cl_level(const SeasonUefa *r, const char *name)
{
    return r ? level_clubs(r->cl_rounds, r->cl_round_count, name) : 0;
}

int season_uefa_uc_level(const SeasonUefa *r, const char *name)
{
    return r ? level_clubs(r->uc_rounds, r->uc_round_count, name) : 0;
}

int season_uefa_cl_round(const SeasonUefa *r, const char *name)
{
    return r ? round_clubs(r->cl_rounds, r->cl_round_count, name) : 0;
}

int season_uefa_uc_round(const SeasonUefa *r, const char *name)
{
    return r ? round_clubs(r->uc_rounds, r->uc_round_count, name) : 0;
}

int season_uefa_ecl_round(const SeasonUefa *r, const char *name)
{
    return r ? round_clubs(r->ecl_rounds, r->ecl_round_count, name) : 0;
}

int season_uefa_ecl_level(const SeasonUefa *r, const char *name)
{
    return r ? level_clubs(r->ecl_rounds, r->ecl_round_count, name) : 0;
}

int season_uefa_suspended(const SeasonUefa *r, const char *name)
{
    size_t k = name ? strlen(name) : 0;
    for (const char *p = r && r->suspended ? r->suspended : ""; k && *p;) {
        const char *e = p;
        while (*e && *e != ',') e++;
        if ((size_t)(e - p) == k && !strncmp(p, name, k)) return 1;
        p = *e ? e + 1 : e;
    }
    return 0;
}

int season_uefa_cl_round_of_16(const SeasonUefa *r)
{
    return r && season_uefa_cl_round(r, "Groups") == 32 && season_uefa_cl_round(r, "Groups 2") == 0 &&
           season_uefa_cl_round(r, "KO") == 16;
}

/* The season of the access list for S: S itself, the last season after the table, -1 before it. */
static int access_season(int season)
{
    int first = 1 << 30, last = -1;
    for (int i = 0; i < season_uefa_access_count; i++) {
        if (season_uefa_access[i].season < first) first = season_uefa_access[i].season;
        if (season_uefa_access[i].season > last) last = season_uefa_access[i].season;
    }
    if (last < 0 || season < first) return -1;
    return season > last ? last : season;
}

const char *season_uefa_access_places(int season, char comp, int rank)
{
    int s = access_season(season);
    const char *best = NULL;
    int best_from = 1 << 30, last_to = -1;
    const char *last = NULL;
    for (int i = 0; s >= 0 && i < season_uefa_access_count; i++) {
        const SeasonUefaAccess *a = &season_uefa_access[i];
        if (a->season != s || a->comp != comp) continue;
        if (rank >= a->rank_from && rank <= a->rank_to) return a->places;
        if (a->rank_from > rank && a->rank_from < best_from) best = a->places, best_from = a->rank_from;
        if (a->rank_to > last_to) last = a->places, last_to = a->rank_to;
    }
    return best ? best : last;
}

const char *season_uefa_access_rule(int season, char comp)
{
    int s = access_season(season);
    for (int i = 0; s >= 0 && i < season_uefa_access_count; i++)
        if (season_uefa_access[i].season == s && season_uefa_access[i].comp == comp) return season_uefa_access[i].places;
    return NULL;
}

int season_uefa_cl_cap(int season)
{
    return season >= 2024 ? 7 : season >= 2015 ? 5 : 4;
}

static int league_cup_row(int season, char comp, const char *name)
{
    int s = access_season(season);
    for (int i = 0; s >= 0 && i < season_uefa_access_count; i++)
        if (season_uefa_access[i].season == s && season_uefa_access[i].comp == comp &&
            !strcmp(season_uefa_access[i].places, name))
            return 1;
    return 0;
}

int season_uefa_access_league_cup(int season, const char *name) { return league_cup_row(season, 'L', name); }

int season_uefa_access_ecl_league_cup(int season, const char *name) { return league_cup_row(season, 'M', name); }

/* ---- Entry codes and round sizes (slice 23) ---- */

/* The code of a token of the access list. Champions League: the plain Q1, Q2 and PR rounds are for champions
 * only, so they are the Champions Path. A plain PO or Q3 too. */
int season_uefa_code(char comp, const char *tok, int n)
{
    static const struct { const char *t; int cl, el, ecl; } map[] = {
        {"G", SEASON_UEFA_CL_G, SEASON_UEFA_EL_G, SEASON_UEFA_ECL_G},
        {"PO-LP", SEASON_UEFA_CL_PO_LP, SEASON_UEFA_EL_PO, SEASON_UEFA_ECL_PO},
        {"PO-CP", SEASON_UEFA_CL_PO_CP, SEASON_UEFA_EL_PO, SEASON_UEFA_ECL_PO},
        {"PO", SEASON_UEFA_CL_PO_CP, SEASON_UEFA_EL_PO, SEASON_UEFA_ECL_PO},
        {"Q3-LP", SEASON_UEFA_CL_Q3_LP, SEASON_UEFA_EL_Q3, SEASON_UEFA_ECL_Q3},
        {"Q3-CP", SEASON_UEFA_CL_Q3_CP, SEASON_UEFA_EL_Q3, SEASON_UEFA_ECL_Q3},
        {"Q3", SEASON_UEFA_CL_Q3_CP, SEASON_UEFA_EL_Q3, SEASON_UEFA_ECL_Q3},
        {"Q2-LP", SEASON_UEFA_CL_Q2_LP, SEASON_UEFA_EL_Q2, SEASON_UEFA_ECL_Q2},
        {"Q2-CP", SEASON_UEFA_CL_Q2_CP, SEASON_UEFA_EL_Q2, SEASON_UEFA_ECL_Q2},
        {"Q2", SEASON_UEFA_CL_Q2_CP, SEASON_UEFA_EL_Q2, SEASON_UEFA_ECL_Q2},
        {"Q1", SEASON_UEFA_CL_Q1, SEASON_UEFA_EL_Q1, SEASON_UEFA_ECL_Q1},
        {"PR", SEASON_UEFA_CL_PR, SEASON_UEFA_EL_PR, 0},
    };
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if ((int)strlen(map[i].t) == n && !strncmp(map[i].t, tok, (size_t)n))
            return comp == 'c' ? map[i].cl : comp == 'k' ? map[i].ecl : map[i].el;
    return 0;
}


static const char *const CL_ROUND[SEASON_UEFA_CL_CODES][2] = {
    {NULL, NULL}, {"Groups", NULL}, {"PO LP", NULL}, {"PO CP", "PO"}, {"Q3 LP", NULL}, {"Q3 CP", "Q3"},
    {"Q2 LP", NULL}, {"Q2 CP", "Q2"}, {"Q1", NULL}, {"PR", NULL}};
static const char *const EL_LEVEL[SEASON_UEFA_EL_CODES - SEASON_UEFA_EL_G + 1] = {"Groups", "PO", "Q3", "Q2", "Q1", "PR",
                                                                               NULL};

int season_uefa_code_clubs(const SeasonUefa *r, char comp, int code)
{
    if (!r) return 0;
    if (comp == 'c') {
        if (code < 1 || code >= SEASON_UEFA_CL_CODES) return 0;
        int n = season_uefa_cl_round(r, CL_ROUND[code][0]);
        return n || !CL_ROUND[code][1] ? n : season_uefa_cl_round(r, CL_ROUND[code][1]);
    }
    if (comp == 'k') {
        if (code < SEASON_UEFA_ECL_G || code >= SEASON_UEFA_ECL_CODES) return 0;
        return season_uefa_ecl_level(r, EL_LEVEL[code - SEASON_UEFA_ECL_G]);
    }
    if (code < SEASON_UEFA_EL_G || code >= SEASON_UEFA_EL_CODES) return 0;
    return season_uefa_uc_level(r, EL_LEVEL[code - SEASON_UEFA_EL_G]);
}

/* The winners of a Champions League round of clubs clubs. The preliminary round is a tournament of semi-finals
 * and a final (rows uefa-138, uefa-139: "PR 4", one winner to Q1), so a quarter of its clubs go through. */
static int cl_winners(int code, int clubs) { return code == SEASON_UEFA_CL_PR ? clubs / 4 : clubs / 2; }

/* The Champions League losers of round tok ("Q3", "Q3-LP", "PR") of the row. */
static int cl_losers(const SeasonUefa *r, const char *tok, int n)
{
    char name[8];
    if (n <= 0 || n >= (int)sizeof name) return 0;
    memcpy(name, tok, (size_t)n);
    name[n] = 0;
    if (!strcmp(name, "PR")) {
        int clubs = season_uefa_cl_round(r, "PR");
        return clubs - cl_winners(SEASON_UEFA_CL_PR, clubs);
    }
    if (name[2] == '-') {                       /* a path: "Q3-LP" is the round "Q3 LP" */
        name[2] = ' ';
        return season_uefa_cl_round(r, name) / 2;
    }
    return season_uefa_cl_level(r, name) / 2;
}

/* Slice 24: the losers of the row's Champions League ("c:") and Europa League ("e:") rounds that enter Conference
 * League round ecl_code (ecl_drops). A Europa League round of all paths gives half its clubs. */
static int ecl_dropped(const SeasonUefa *r, int ecl_code)
{
    int sum = 0;
    for (const char *p = r->ecl_drops ? r->ecl_drops : ""; *p;) {
        while (*p == ' ') p++;
        const char *gt = strchr(p, '>');
        if (!gt || gt - p < 3 || p[1] != ':') break;
        const char *e = gt + 1;
        while (*e && *e != ' ') e++;
        if (season_uefa_code('k', gt + 1, (int)(e - gt - 1)) == ecl_code) {
            if (p[0] == 'c') {
                sum += cl_losers(r, p + 2, (int)(gt - p - 2));
            } else if (p[0] == 'e' && gt - p - 2 < 8) {
                char name[8];
                memcpy(name, p + 2, (size_t)(gt - p - 2));
                name[gt - p - 2] = 0;
                sum += season_uefa_uc_level(r, name) / 2;
            }
        }
        p = e;
    }
    return sum;
}

int season_uefa_dropped(const SeasonUefa *r, int el_code)
{
    if (r && el_code >= SEASON_UEFA_ECL_G && el_code < SEASON_UEFA_ECL_CODES) return ecl_dropped(r, el_code);
    if (!r || !r->drops || el_code < SEASON_UEFA_EL_G || el_code >= SEASON_UEFA_EL_CODES) return 0;
    int sum = 0;
    for (const char *p = r->drops; *p;) {
        while (*p == ' ') p++;
        const char *gt = strchr(p, '>');
        if (!gt) break;
        const char *e = gt + 1;
        while (*e && *e != ' ') e++;
        int to = season_uefa_code('e', gt + 1, (int)(e - gt - 1));
        if (to == el_code) sum += cl_losers(r, p, (int)(gt - p));
        p = e;
    }
    return sum;
}

void season_uefa_new_clubs(const SeasonUefa *r, char comp, int *out)
{
    for (int i = 0; i < SEASON_UEFA_CODE_SLOTS; i++) out[i] = 0;
    if (!r) return;
    if (comp == 'c') {
        static const int champions[] = {SEASON_UEFA_CL_G, SEASON_UEFA_CL_PO_CP, SEASON_UEFA_CL_Q3_CP,
                                        SEASON_UEFA_CL_Q2_CP, SEASON_UEFA_CL_Q1, SEASON_UEFA_CL_PR};
        static const int league[] = {SEASON_UEFA_CL_G, SEASON_UEFA_CL_PO_LP, SEASON_UEFA_CL_Q3_LP, SEASON_UEFA_CL_Q2_LP};
        const int *paths[2] = {champions, league};
        int lens[2] = {6, 4};
        out[SEASON_UEFA_CL_G] = season_uefa_code_clubs(r, 'c', SEASON_UEFA_CL_G);
        for (int p = 0; p < 2; p++)
            for (int i = 0; i < lens[p]; i++) {
                int code = paths[p][i], below = 0, won = 0;
                for (int j = i + 1; j < lens[p] && !below; j++) {
                    below = season_uefa_code_clubs(r, 'c', paths[p][j]);
                    won = cl_winners(paths[p][j], below);
                }
                if (code == SEASON_UEFA_CL_G) out[code] -= won;
                else out[code] = season_uefa_code_clubs(r, 'c', code) - won;
            }
        return;
    }
    int lo = comp == 'k' ? SEASON_UEFA_ECL_G : SEASON_UEFA_EL_G, hi = comp == 'k' ? SEASON_UEFA_ECL_CODES : SEASON_UEFA_EL_CODES;
    for (int code = lo; code < hi; code++) {
        int size = season_uefa_code_clubs(r, comp, code), below = 0;
        for (int j = code + 1; j < hi && !below; j++) below = season_uefa_code_clubs(r, comp, j);
        out[code] = size ? size - below / 2 - season_uefa_dropped(r, code) : 0;
    }
}

static int same_venue(const SeasonUefa *a, const SeasonUefa *b)
{
    return a->sc_stadium && b->sc_stadium && a->sc_city && b->sc_city && !strcmp(a->sc_stadium, b->sc_stadium) &&
           !strcmp(a->sc_city, b->sc_city);
}

/* Q28. After the last table row the venue of each year is drawn from the venues of the table (the past finals),
 * never the venue of the year before: from the venue of year Y - 1, a step of 1 to n - 1 places along the list
 * of the n venues. The step is a fixed mix of the year, so the draw takes no game random number and a loaded game
 * gets the same venue again. */
int season_uefa_super_cup_venue(int year, const char **stadium, const char **city)
{
    const SeasonUefa *r = season_uefa_row(year);
    if (!r || !r->sc_stadium || !r->sc_city) return 0;
    const SeasonUefa *last = &season_uefa_rows[season_uefa_row_count - 1];
    if (season_changing_rules() && r == last && year > last->season) {
        const SeasonUefa *venues[64];
        int n = 0, k = -1;
        for (int i = 0; i < season_uefa_row_count && n < 64; i++) {
            int seen = !season_uefa_rows[i].sc_stadium || !season_uefa_rows[i].sc_city;
            for (int j = 0; j < n && !seen; j++) seen = same_venue(venues[j], &season_uefa_rows[i]);
            if (!seen) venues[n++] = &season_uefa_rows[i];
        }
        for (int j = 0; j < n && k < 0; j++)
            if (same_venue(venues[j], last)) k = j;
        if (n > 1 && k >= 0) {
            for (int y = last->season + 1; y <= year; y++) {
                uint32_t h = (uint32_t)y * 2654435761u;
                h ^= h >> 15;
                k = (k + 1 + (int)(h % (uint32_t)(n - 1))) % n;
            }
            r = venues[k];
        }
    }
    *stadium = r->sc_stadium;
    *city = r->sc_city;
    return 1;
}

int season_uefa_cup_outcome(int stage, int outcome)
{
    return stage < 0 && outcome == 1 ? 0 : outcome;
}
