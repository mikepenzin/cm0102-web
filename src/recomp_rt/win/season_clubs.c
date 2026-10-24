/* Club moves at a new game (season_clubs.h). The table lives in host memory. The moves change only the
 * division pointer (+0x57) of club records in the game's memory, and only in a season game. */
#include "win.h"
#include "season.h"
#include "season_clubs.h"
#include "season_names.h"
#include "season_records.h"
#include "season_rules.h"
#include "season_sizes.h"
#include "season_divisions.h"
#include "../host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FORMAT 1
#define MAX_ROWS 200000
#define MAX_BYTES (32u << 20)

#define CLUBS 0xae23b4u             /* the club table (0x245 bytes a club), clubs then national teams */
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u      /* the last 2 x nations rows are national teams (0x53b900) */
#define CLUB_SIZE 0x245u
#define CLUB_NAME 4u                /* long name, 52 bytes */
#define CLUB_NATION 0x53u           /* nation pointer */
#define CLUB_DIVISION 0x57u         /* club_comp pointer */
#define CLUB_REPUTATION 0x80u      /* s16 */
#define CLUB_LAST_DIVISION 0x5bu    /* club_comp pointer: the division of the last season */
#define COMPS 0xae23d0u             /* the club_comp table (0x6b bytes a record) */
#define COMP_COUNT 0xae2378u
#define COMP_SIZE 0x6bu
#define COMP_NAME 4u
#define COMP_NATION 0x5du           /* nation pointer */
#define COMP_RANK 0x69u             /* s16: a higher division has a higher value (English Premier 18, Third 4) */
#define KEY_LOWER 0x9cf764u         /* "A Lower Division" (0x60efd0 at 0x610141) */
#define NATIONS 0xae23a8u           /* the nation table, 0x122 bytes a nation */
#define DEN_NATION 0x9cf2c0u        /* index of Denmark (DenCup::collect_entrants 0x551240 at 0x5512ee) */
#define KEY_PREMIER 0x9cf6b8u       /* "A Premier Division" (0x60efd0 at 0x60fb7f) */

typedef struct {
    int season, club_id;
    const char *division, *club, *real;
} ClubRow;

static char *text;
static ClubRow *rows;
static int nrows;

static int fail(const char *path, int line, const char *why)
{
    fprintf(stderr, "recomp: club moves table %s, line %d: %s; no club moves\n", path, line, why);
    free(text);
    free(rows);
    text = NULL;
    rows = NULL;
    nrows = 0;
    return 0;
}

static char *end_line(char *line)
{
    char *nl = strchr(line, '\n');
    if (!nl) return NULL;
    *nl = 0;
    if (nl > line && nl[-1] == '\r') nl[-1] = 0;
    return nl;
}

static int split(char *line, char **out, int n)
{
    int k = 0;
    out[k++] = line;
    for (char *p = line; *p; p++)
        if (*p == '\t') {
            if (k == n) return 0;
            *p = 0;
            out[k++] = p + 1;
        }
    return k == n;
}

static int number(const char *s, int lo, int hi)
{
    if (!*s) return -1;
    long v = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10 + (*p - '0');
        if (v > hi) return -1;
    }
    return v < lo ? -1 : (int)v;
}

int season_clubs_load(const char *path)
{
    free(text);
    free(rows);
    text = NULL;
    rows = NULL;
    nrows = 0;
    FILE *f = host_fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > (long)MAX_BYTES) { fclose(f); return fail(path, 0, "the file is too large"); }
    text = malloc((size_t)size + 1);
    if (!text) { fclose(f); return fail(path, 0, "no memory"); }
    size_t got = fread(text, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) return fail(path, 0, "a read failed");
    text[size] = 0;
    char *line = text, *next = end_line(line);
    int want = -1, version = 0, lineno = 1;
    if (!next || sscanf(line, "#bosman-clubs\t%d\t%d", &version, &want) != 2 || version != FORMAT || want < 0 ||
        want > MAX_ROWS)
        return fail(path, 1, "not a club moves table of format 1");
    line = next + 1;
    lineno = 2;
    next = end_line(line);
    if (!next || strcmp(line, "season\tdivision\tclub_id\tclub\treal"))
        return fail(path, 2, "the columns are not those of format 1");
    rows = calloc((size_t)want + 1, sizeof *rows);
    if (!rows) return fail(path, 0, "no memory");
    for (line = next + 1; *line; line = next + 1) {
        lineno++;
        next = end_line(line);
        if (!next) return fail(path, lineno, "the last line has no end");
        if (nrows == want) return fail(path, lineno, "more rows than line 1 says");
        char *c[5];
        if (!split(line, c, 5)) return fail(path, lineno, "a wrong number of columns");
        ClubRow *r = &rows[nrows];
        r->season = number(c[0], SEASON_RULE_FIRST, SEASON_RULE_LAST);
        r->division = c[1];
        r->club_id = !strcmp(c[2], "-1") ? -1 : number(c[2], 0, 10000000);
        r->club = c[3];
        r->real = c[4];
        if (r->season < 0 || (r->club_id < 0 && strcmp(c[2], "-1")) || !*r->division || !*r->club || strlen(r->division) > 51 ||
            strlen(r->club) > 51)
            return fail(path, lineno, "a bad season, club id or name");
        nrows++;
    }
    if (nrows != want) return fail(path, lineno, "fewer rows than line 1 says");
    return nrows;
}

int season_clubs_init(const char *game_dir)
{
    char path[1200];
    season_data_path(game_dir, "clubs.tsv", path, sizeof path);
    int n = season_clubs_load(path);
    if (n) fprintf(stderr, "recomp: club moves table: %d clubs from %s\n", n, path);
    else fprintf(stderr, "recomp: club moves table: none (%s)\n", path);
    return n;
}

/* One club of the game. */
typedef struct {
    uint32_t at, nation, db_div, div;
    int id, national;
    int kept;                       /* 1 when a list keeps it in its division */
} Club;

/* One division of the game. */
typedef struct {
    uint32_t at, nation;
    int rank, n0, want, tier, target, spare, sized;
    int fresh;          /* slice 20: a record that the module adds, with no club in the database */
    int listed, found, kept, missing, missing_reserve, filled_own, filled_left, filled_up, filled_pool, filled_spare, filled_lower;
    int open, stand_in;
    const char *name;
} Division;

static int same_name(uint32_t at, const char *name)
{
    return !strncmp(win_str(at), name, 52);
}

/* The club count that the class of a division takes in create_table (the write of +0x3e). The second count is
 * that of the first season, where the class has a year branch (+0x40 == 2001). A division keeps this count,
 * not the count of the database. The class takes the first clubs of the division in the order of the club
 * table and stops at its count (EngConf::create_table 0x56ecb0 at 0x56ed54..0x56ed7f). With fewer the table
 * is not made (0x56ed95..0x56edab), and the constructor stops at its assert (fra_first.cpp 108 for FraFirst).
 * The address is that of the write. These classes are not here, so their divisions keep the database count:
 * - classes that count the clubs of the data: IrePrm 0x63a740, IreFirst 0x635910, PolFirst 0x7c9450,
 *   NorFirst 0x7909a0;
 * - classes that take groups: BraNatThird 0x42dc90, FraLower 0x5be230, SpaLower 0x8533d0,
 *   and the Belgian, Portuguese, Spanish, Swedish, Italian C2 and Turkish group classes;
 * - classes that take no club by the division pointer: the Brazilian state classes (BraRegSp::create_table
 *   0x43cc40 and the others take the clubs of a region, BraRegCentral::load_region_clubs 0x430ef0), and
 *   FraCfa 0x5b9fa0, a stub with no table (the French CFA clubs go to "A Lower Division", see the doc). */
typedef struct { const char *name, *file; int size, first_season; } ClassSize;
static const ClassSize CLASS_SIZE[] = {
    {"American Major League", "usa_mls", 12, 12},                   /* UsaMls::create_table 0x908572: 3 groups of 4 */
    {"Argentine Premier Division", "arg_prm", 20, 20},              /* ArgPrm::create_table 0x405552 */
    {"Argentine Second Division", "arg_second", 25, 25},               /* ArgSecond::fill_league_table 0x407ce2 */
    {"Australian National Soccer League", "aus_nsl", 13, 13},       /* AusNsl 0x411b82 */
    {"Belgian First Division", "bel_first", 18, 18},                  /* 0x41e8c2 */
    {"Belgian Second Division", "bel_second", 18, 18},                 /* 0x41fd82 */
    {"Brazilian National First Division", "bra_nat_first", 28, 28},       /* 0x42a492 */
    {"Brazilian National Second Division", "bra_nat_second", 28, 28},      /* 0x42ca82 */
    {"Croatian First Division", "cro_a1", 16, 16},                 /* 0x5147c2 */
    {"Croatian Second Division North", "cro_a2a", 16, 16},          /* 0x515cc2 */
    {"Croatian Second Division South", "cro_a2b", 16, 16},          /* 0x516de2 */
    {"Danish First Division", "den_first", 16, 16},                   /* 0x5522a2 */
    {"Danish Premier Division", "den_prm", 12, 12},                 /* 0x5538e2 */
    {"Danish Second Division", "den_second", 16, 16},                  /* 0x554a52 */
    {"English Conference", "eng_conf", 22, 22},                      /* 0x56ecc2 */
    {"English First Division", "eng_first", 24, 24},                  /* 0x572c01 */
    {"English Premier Division", "eng_prm", 20, 20},                /* 0x574a32 */
    {"English Second Division", "eng_second", 24, 24},                 /* 0x576642 */
    {"English Third Division", "eng_third", 24, 24},                  /* 0x577f82 */
    {"Finnish First Division North", "fin_first", 10, 10},            /* 0x593a82 */
    {"Finnish First Division South", "fin_first", 10, 10},            /* 0x593a82 */
    {"Finnish Premier Division", "fin_prm", 12, 12},                /* 0x595b82 */
    {"French First Division", "fra_first", 20, 18},                   /* 0x5bc2bd: 18 at 0x5bc2c5 when +0x40 is 2001 */
    {"French Second Division", "fra_second", 20, 20},                  /* 0x5bef21 */
    {"French National", "fra_third", 20, 20},                         /* 0x5c05e2 */
    {"German First Division", "ger_first", 18, 18},                   /* 0x5db244 */
    {"German Second Division", "ger_second", 18, 18},                  /* 0x5df2c2 */
    {"German Regional Division North", "ger_regional", 18, 18},          /* 0x5dd809; 19 only when +0x40 is 2000 */
    {"German Regional Division South", "ger_regional", 18, 18},
    {"Greek National A Division", "gre_prm", 14, 14},               /* 0x5ead72 */
    {"Greek National B Division", "gre_second", 14, 14},               /* 0x5ebcc2; the databases hold 16 */
    {"Dutch First Division", "hol_first", 18, 18},                    /* 0x5f6252 */
    {"Dutch Premier Division", "hol_prm", 18, 18},                  /* 0x5f8442 */
    {"Italian Serie A", "ita_ser_a", 18, 18},                         /* 0x641522; the later databases hold 20 */
    {"Italian Serie B", "ita_ser_b", 20, 20},                         /* 0x646792 */
    {"Italian Serie C1/A", "ita_ser_c1a", 18, 18},                      /* 0x64b682 */
    {"Italian Serie C1/B", "ita_ser_c1b", 18, 18},                      /* 0x64f832 */
    {"Japanese J-League 1", "jap_j1", 16, 16},                     /* 0x6634f2 */
    {"Japanese J-League 2", "jap_j2", 12, 12},                     /* 0x665042 */
    {"Korean League", "kor_league", 10, 10},                           /* 0x66bbb2 */
    {"Northern Irish League First Division", "nir_first", 10, 10},    /* 0x78bc62 */
    {"Northern Irish League Premier Division", "nir_prm", 10, 10},  /* 0x78ec72 */
    {"Norwegian Premier Division", "nor_prm", 14, 14},              /* 0x7925f2 */
    {"Polish Second Division", "pol_second", 18, 20},                  /* 0x7cbdfd: 20 at 0x7cbe03 when +0x40 is 2001 */
    {"Portuguese Premier League", "por_prm", 18, 18},               /* 0x7cf812 */
    {"Portuguese Second League", "por_second", 18, 18},                /* 0x7d1532 */
    {"Russian First Division", "rus_first", 18, 18},                  /* 0x7ea6d3 */
    {"Russian Premier Division", "rus_prm", 16, 16},                /* 0x7eb622 */
    {"Scottish First Division", "sco_first", 10, 10},                 /* 0x7eed42 */
    {"Scottish Premier Division", "sco_prm", 12, 12},               /* 0x7f283d; 10 only when +0x40 is 1999 */
    {"Scottish Second Division", "sco_second", 10, 10},                /* 0x7f3e92 */
    {"Scottish Third Division", "sco_third", 10, 10},                 /* 0x7f4e32 */
    {"Spanish First Division", "spa_first", 20, 20},                  /* 0x84fc82 */
    {"Spanish Second Division", "spa_second", 22, 22},                 /* 0x855b52 */
    {"Swedish First Division", "swe_first", 16, 16},                  /* 0x88efa2 */
    {"Swedish Premier Division", "swe_prm", 14, 14},                /* 0x890442 */
    {"Turkish Premier Division", "tur_first", 18, 18},                /* 0x8fddd4 */
    {"Turkish 2. Division Category A", "tur_second", 20, 20},          /* 0x8ff162 */
    {"Welsh Premier Division", "wel_first", 18, 18},                  /* 0x925b42 */
};

/* The count of a division in a season: the count of the rules table row of its class (slice 18,
 * season_sizes_clubs: create_table then takes the clubs that the division holds), else the count of the class. */
static int class_size(const char *name, int season)
{
    int new_division = season_divisions_class_size(name, season);   /* slice 20 */
    if (new_division >= 0) return new_division;
    for (size_t k = 0; k < sizeof CLASS_SIZE / sizeof CLASS_SIZE[0]; k++)
        if (!strncmp(name, CLASS_SIZE[k].name, 52)) {
            int table = season_sizes_clubs(CLASS_SIZE[k].file, season);
            if (table > 0) return table;
            return season == SEASON_FIRST_YEAR ? CLASS_SIZE[k].first_season : CLASS_SIZE[k].size;
        }
    return -1;
}

/* The file of a division's class, or NULL. */
static const char *class_file(const char *name)
{
    for (size_t k = 0; k < sizeof CLASS_SIZE / sizeof CLASS_SIZE[0]; k++)
        if (!strncmp(name, CLASS_SIZE[k].name, 52)) return CLASS_SIZE[k].file;
    return NULL;
}

/* The count of a class after 2001. */
static int class_size_of(const char *file)
{
    for (size_t k = 0; file && k < sizeof CLASS_SIZE / sizeof CLASS_SIZE[0]; k++)
        if (!strcmp(file, CLASS_SIZE[k].file)) return CLASS_SIZE[k].size;
    return 0;
}

/* A club name in a plain form, to find a club whose name in the research differs a little from the database
 * ("Operário Futebol Clube Ltda" and "Operário Futebol Clube"): the letters of Windows-1252 without accents,
 * lower case, words of letters and digits, and (when drop is set) the words that only say "club" (fc, futebol,
 * clube, ltda, ...) left out. */
static void plain_words(const char *in, char *out, size_t room, int drop_words)
{
    static const char *const drop[] = {"fc", "cf", "sc", "ac", "ec", "ca", "cd", "sd", "ud", "if", "fk", "sk",
                                       "bk", "afc", "club", "clube", "futebol", "football", "ltda", "de", "da",
                                       "do", "das", "dos", "the", "e", "y", "and"};
    static const char fold[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";   /* 0xc0.. */
    char word[64];
    size_t n = 0, w = 0;
    out[0] = 0;
    for (const unsigned char *p = (const unsigned char *)in;; p++) {
        int ch = *p;
        if (ch >= 0xc0) ch = (unsigned char)fold[ch - 0xc0];
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        int letter = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
        if (letter && w < sizeof word - 1) {
            word[w++] = (char)ch;
            continue;
        }
        if (w) {
            word[w] = 0;
            int skip = 0;
            for (size_t k = 0; drop_words && k < sizeof drop / sizeof drop[0] && !skip; k++) skip = !strcmp(word, drop[k]);
            if (!skip && n + w + 2 < room) {
                if (n) out[n++] = ' ';
                memcpy(out + n, word, w);
                n += w;
                out[n] = 0;
            }
            w = 0;
        }
        if (!*p) break;
    }
}

static void plain_name(const char *in, char *out, size_t room)
{
    plain_words(in, out, room, 1);
}

/* 1 when the words of a are the first words of b, or those of b the first words of a. */
static int word_prefix(const char *a, const char *b)
{
    size_t na = strlen(a), nb = strlen(b), n = na < nb ? na : nb;
    if (strncmp(a, b, n)) return 0;
    return na == nb || (na < nb ? b[n] == ' ' : a[n] == ' ');
}

/* 1 for the name of a reserve or B side: a last word B, II, 2 or Reserves, a word such as U21, Jong at the
 * start, or "(reserve team)" (the research form). */
static int reserve_name(const char *name)
{
    char plain[128];
    plain_name(name, plain, sizeof plain);
    const char *last = strrchr(plain, ' ');
    last = last ? last + 1 : plain;
    if (!strcmp(last, "b") || !strcmp(last, "ii") || !strcmp(last, "2") || !strcmp(last, "reserves") ||
        !strcmp(last, "reserve") || !strncmp(plain, "jong ", 5) || strstr(name, "(reserve team)"))
        return 1;
    for (const char *p = plain; *p; p++)
        if ((p == plain || p[-1] == ' ') && p[0] == 'u' && p[1] >= '1' && p[1] <= '2' && p[2] >= '0' && p[2] <= '9')
            return 1;
    return 0;
}

static int find_division(const Division *div, int ncomps, const char *name)
{
    for (int t = 0; t < ncomps; t++)
        if ((div[t].n0 > 0 || div[t].fresh) && !div[t].spare && same_name(div[t].at + COMP_NAME, name)) return t;
    return -1;
}

/* 1 when a club can take a place of division d in a fill group of move_clubs (the groups are named there). */
static int fill_takes(const Club *c, const Division *div, int ncomps, uint32_t comps, const Division *d, int group,
                      uint32_t spare1, uint32_t spare2, uint32_t cup_pool_nation)
{
    if (c->kept || c->national || c->nation != d->nation || c->div == d->at) return 0;
    int t = c->db_div >= comps ? (int)((c->db_div - comps) / COMP_SIZE) : -1;
    int reserve_ok = !reserve_name(win_str(c->at + CLUB_NAME)) || d->missing_reserve;
    if (group == 0)                     /* a club that left a division of a higher rank */
        return !c->div && t >= 0 && t < ncomps && div[t].rank > d->rank && reserve_ok;
    if (group == 4)                     /* a club that left a lower division with a list */
        return !c->div && t >= 0 && t < ncomps && div[t].target && !div[t].spare && div[t].rank < d->rank && reserve_ok;
    if (group == 1) return !c->div && !c->db_div && reserve_ok;
    if (group == 2) return c->div && (c->div == spare1 || c->div == spare2) && c->nation != cup_pool_nation;
    if (c->div >= comps && c->div < comps + (uint32_t)ncomps * COMP_SIZE) {
        const Division *from = &div[(c->div - comps) / COMP_SIZE];
        return !from->spare && !from->target && from->rank < d->rank;
    }
    return 0;
}

/* FILL_BY_REP: the club of the group with the highest reputation (+0x80), or -1. A list of a lower division (the
 * English Conference) sends the clubs that it does not keep out of that division. Its former clubs are the clubs
 * of the nation nearest to the division above, so they take its open places first, best first. The spare
 * divisions (A Lower Division, A Premier Division) hold the nation's small clubs in alphabetical order; the club
 * table trim 0x5267c0 keeps clubs by reputation (0x526afd..0x526b9e, inference), so a fill that took the first of
 * them lost the weakest at the trim. A 2025/26 start on data of 2002 (England, changing rules on) gave League Two
 * seven clubs of A Lower Division (Abbey Hey to Almondsbury Town). The trim then dropped four, League Two
 * played with 20 clubs, and the FA Cup and the League Cup filled 4 empty places each season. */
static int best_fill(const Club *club, int count, const Division *div, int ncomps, uint32_t comps, const Division *d,
                     int group, uint32_t spare1, uint32_t spare2, uint32_t cup_pool_nation)
{
    int best = -1, best_rep = 0;
    for (int i = 0; i < count; i++) {
        if (!fill_takes(&club[i], div, ncomps, comps, d, group, spare1, spare2, cup_pool_nation)) continue;
        int rep = (int16_t)rd16(g_mem, club[i].at + CLUB_REPUTATION);
        if (best < 0 || rep > best_rep) best = i, best_rep = rep;
    }
    return best;
}

/* The moves of a season. */
static void move_clubs(int season)
{
    int first = -1, last = -1;
    for (int i = 0; i < nrows; i++)
        if (rows[i].season == season) {
            if (first < 0) first = i;
            last = i;
        }
    if (first < 0) first = 0, last = -1;        /* no list: only the counts of CLASS_SIZE */
    int log = getenv("RECOMP_CLUB_LOG") && *getenv("RECOMP_CLUB_LOG");
    uint32_t table = rd32(g_mem, CLUBS), comps = rd32(g_mem, COMPS);
    int count = (int)rd32(g_mem, CLUB_COUNT), ncomps = (int)rd32(g_mem, COMP_COUNT);
    int national_from = count - 2 * (int)rd32(g_mem, NATION_COUNT);
    if (!table || !comps || count <= 0 || ncomps <= 0) return;
    int32_t key_lower = (int32_t)rd32(g_mem, KEY_LOWER), key_premier = (int32_t)rd32(g_mem, KEY_PREMIER);
    uint32_t spare1 = key_lower >= 0 ? comps + (uint32_t)key_lower * COMP_SIZE : 0;
    uint32_t spare2 = key_premier >= 0 ? comps + (uint32_t)key_premier * COMP_SIZE : 0;

    /* DenCup::collect_entrants 0x551240 takes its lower clubs from A Lower Division (record [0x9cf764], 0x5512fa)
     * when their nation is Denmark (0x551302..0x55131a: the nation table [0xae23a8], 0x122 bytes a nation, at the
     * index [0x9cf2c0]), up to 48, then three whole divisions, and stops at den_cup.cpp 75 (0x550a62) without 92. A
     * Danish club taken from that division leaves the cup short, so the moves take none (a 2021/22 start with the
     * October 2021 database stopped there; main did not). */
    uint32_t nations = rd32(g_mem, NATIONS), cup_pool_nation = nations ? nations + rd32(g_mem, DEN_NATION) * 0x122u : 0;
    Club *club = calloc((size_t)count, sizeof *club);
    Division *div = calloc((size_t)ncomps, sizeof *div);
    int *by_list = calloc((size_t)(last - first + 2), sizeof *by_list);
    int *to_div = calloc((size_t)(last - first + 2), sizeof *to_div);
    Division **order = calloc((size_t)ncomps + 1, sizeof *order);
    if (!club || !div || !by_list || !to_div || !order) {
        free(club);
        free(div);
        free(by_list);
        free(to_div);
        free(order);
        return;
    }
    for (int i = 0; i < count; i++) {
        Club *c = &club[i];
        c->at = table + (uint32_t)i * CLUB_SIZE;
        c->id = (int)rd32(g_mem, c->at);
        c->nation = rd32(g_mem, c->at + CLUB_NATION);
        c->db_div = c->div = rd32(g_mem, c->at + CLUB_DIVISION);
        c->national = c->id >= national_from;
    }
    /* The divisions: every club_comp record, its club count in the database and the count it keeps. */
    for (int i = 0; i < ncomps; i++) {
        Division *d = &div[i];
        d->at = comps + (uint32_t)i * COMP_SIZE;
        d->nation = rd32(g_mem, d->at + COMP_NATION);
        d->rank = (int16_t)rd16(g_mem, d->at + COMP_RANK);
        d->spare = d->at == spare1 || d->at == spare2;
        d->name = win_str(d->at + COMP_NAME);
        d->fresh = season_divisions_new_record(d->name);
    }
    for (int i = 0; i < count; i++)
        if (club[i].db_div >= comps && club[i].db_div < comps + (uint32_t)ncomps * COMP_SIZE)
            div[(club[i].db_div - comps) / COMP_SIZE].n0++;
    int resized = 0;
    for (int i = 0; i < ncomps; i++) {
        int size = (div[i].n0 > 0 || div[i].fresh) && !div[i].spare ? class_size(div[i].name, season) : -1;
        const char *file = class_file(div[i].name);
        if (size > 0 && div[i].n0 > size && season_sizes_keeps_database(file)) {     /* a cup takes it whole */
            int most = class_size_of(file);
            size = div[i].n0 < most ? div[i].n0 : most;
        }
        div[i].want = size >= 0 ? size : div[i].n0;     /* 0: a division that ends (slice 20, Q290) */
        if (div[i].want != div[i].n0) {
            div[i].sized = 1;
            resized++;
            if (log) fprintf(stderr, "recomp: club moves: %s takes %d clubs, the database gives it %d\n", div[i].name,
                             div[i].want, div[i].n0);
        }
    }
    for (int i = 0; i < ncomps; i++) {             /* tier: 1 + the divisions of the nation with a higher rank */
        if ((div[i].n0 <= 0 && !div[i].fresh) || div[i].spare) continue;
        div[i].tier = 1;
        for (int j = 0; j < ncomps; j++)
            if (j != i && div[j].n0 > 0 && !div[j].spare && div[j].nation == div[i].nation && div[j].rank > div[i].rank)
                div[i].tier++;
    }

    /* The lists: find each club and mark the target divisions. by_list holds the club index or -1. */
    int unknown_division = 0, unmatched = 0, by_plain = 0;
    for (int r = first; r <= last; r++) {
        by_list[r - first] = -1;
        to_div[r - first] = -1;
        if (rows[r].season != season) continue;
        if (!season_divisions_list_on(rows[r].division, season)) continue;     /* slice 20 */
        int t = find_division(div, ncomps, rows[r].division);
        if (t < 0) {
            unknown_division++;
            continue;
        }
        to_div[r - first] = t;
        div[t].target = 1;
        div[t].listed++;
        int k = -1;
        /* Slice 15b: a club that the database lacks can be a new club record of this load (season_records.c). */
        int record = rows[r].club_id < 0 ? season_records_club(season, rows[r].real) : -1;
        for (int i = 0; i < count && k < 0 && record >= 0; i++)
            if (club[i].id == record && !club[i].national) k = i;
        for (int i = 0; i < count && k < 0 && rows[r].club_id >= 0; i++)
            if (club[i].id == rows[r].club_id && !club[i].national && club[i].nation == div[t].nation &&
                same_name(club[i].at + CLUB_NAME, rows[r].club))
                k = i;
        for (int i = 0; i < count && k < 0; i++)
            if (!club[i].national && club[i].nation == div[t].nation && same_name(club[i].at + CLUB_NAME, rows[r].club))
                k = i;
        if (k < 0) {
            /* The plain form, when exactly one club of the nation has it. When several have it, the one whose
             * full words (with fc, clube, ...) begin the research name or begin with it: "Operário Futebol
             * Clube" for "Operário Futebol Clube Ltda", not "Operário (SC)" (both "operario"). */
            char want[128], have[128], want_full[128], have_full[128];
            plain_name(rows[r].club, want, sizeof want);
            plain_words(rows[r].club, want_full, sizeof want_full, 0);
            int hits = 0, hit = -1, close = 0, close_hit = -1;
            for (int i = 0; i < count && *want; i++) {
                if (club[i].national || club[i].nation != div[t].nation) continue;
                plain_name(win_str(club[i].at + CLUB_NAME), have, sizeof have);
                if (strcmp(have, want)) continue;
                hits++, hit = i;
                plain_words(win_str(club[i].at + CLUB_NAME), have_full, sizeof have_full, 0);
                if (word_prefix(have_full, want_full)) close++, close_hit = i;
            }
            if (hits == 1) k = hit, by_plain++;
            else if (hits > 1 && close == 1) k = close_hit, by_plain++;
        }
        if (k < 0) {
            unmatched++;
            div[t].missing++;
            if (reserve_name(rows[r].real) || reserve_name(rows[r].club)) div[t].missing_reserve++;
            if (log) fprintf(stderr, "recomp: club moves: missing: %d/%02d\t%s\ttier %d\t%s\n", season,
                             (season + 1) % 100, rows[r].division, div[t].tier, rows[r].real);
            continue;
        }
        div[t].found++;
        by_list[r - first] = k;
    }
    /* Keep: in each target, the listed clubs that the database has there first, then the others, up to the
     * count of the division. A club in two lists keeps the first. */
    for (int pass = 0; pass < 2; pass++)
        for (int r = first; r <= last; r++) {
            int k = by_list[r - first], t = to_div[r - first];
            if (k < 0 || t < 0 || club[k].kept) continue;
            if ((pass == 0) != (club[k].db_div == div[t].at)) continue;
            if (div[t].kept >= div[t].want) continue;
            club[k].kept = 1;
            club[k].div = div[t].at;
            div[t].kept++;
        }
    /* Leave: a club in a target that no list keeps there. */
    int left = 0;
    for (int i = 0; i < count; i++) {
        Club *c = &club[i];
        if (c->kept || c->national || !c->db_div) continue;
        int t = (int)((c->db_div - comps) / COMP_SIZE);
        if (c->db_div >= comps && t < ncomps && div[t].target && c->div == c->db_div) {
            c->div = 0;
            left++;
        }
    }
    /* A shell: a club with no nation (+0x53 NULL) and fewer than 15 squad slots (+0xd7, 50 slots) in a division
     * of a nation leaves it. VIRTUAL_STAFF::top_up 0x90dae0 gives a club with no nation no virtual players
     * (0x90db33..0x90db38), and fill_fixture_squads 0x90d2f0 stops at virtual_staff.cpp 195 (0x90d4bc) when a club
     * of a fixture has fewer than 15 filled slots after the top-up (0x90d45f..0x90d465). The fill below never
     * takes such a club (it takes clubs of the division's nation only), so its place goes to a real club. A club
     * with a nation and few players gets virtual players and stays. */
    int shells = 0;
    for (int i = 0; i < count; i++) {
        Club *c = &club[i];
        if (c->kept || c->national || c->nation || !c->div) continue;
        if (c->div < comps || c->div >= comps + (uint32_t)ncomps * COMP_SIZE) continue;
        Division *d = &div[(c->div - comps) / COMP_SIZE];
        if (d->spare || !d->nation) continue;
        int slots = 0;
        for (uint32_t j = 0; j < 50; j++) slots += rd32(g_mem, c->at + 0xd7u + 4 * j) != 0;
        if (slots >= 15) continue;
        c->div = 0;
        shells++;
        if (log) fprintf(stderr, "recomp: club moves: %s has no nation and %d squad slots: it leaves %s\n",
                         win_str(c->at + CLUB_NAME), slots, d->name);
    }
    if (shells) fprintf(stderr, "recomp: club moves: %d clubs with no nation and no squad leave their division\n",
                        shells);
    /* The divisions to cut or fill: those of the nations with a list or with a division of another count, from
     * the highest rank down. */
    int norder = 0;
    for (int i = 0; i < ncomps; i++) {
        if ((div[i].n0 <= 0 && !div[i].fresh) || div[i].spare) continue;
        int nation_listed = 0;
        for (int j = 0; j < ncomps && !nation_listed; j++)
            nation_listed = (div[j].target || div[j].sized) && div[j].nation == div[i].nation;
        if (nation_listed) order[norder++] = &div[i];
    }
    for (int i = 1; i < norder; i++)               /* insertion sort: rank high first, then record */
        for (int j = i; j > 0 && (order[j]->rank > order[j - 1]->rank ||
                                  (order[j]->rank == order[j - 1]->rank && order[j]->at < order[j - 1]->at)); j--) {
            Division *x = order[j];
            order[j] = order[j - 1];
            order[j - 1] = x;
        }
    int short_after = 0, cut = 0;
    for (int o = 0; o < norder; o++) {
        Division *d = order[o];
        int now = 0;
        for (int i = 0; i < count; i++) now += club[i].div == d->at;
        /* Too many (a class that takes fewer than the database holds): the last clubs of the table leave, as the
         * class would leave them out. */
        for (int i = count - 1; i >= 0 && now > d->want; i--)
            if (club[i].div == d->at && !club[i].kept) {
                club[i].div = 0;
                now--;
                cut++;
            }
        /* Too few. First the clubs that the database had there and that no list keeps. Then two kinds of place:
         * - the place of a real club of the list that the database lacks (Q359): below the top two tiers it takes
         *   one of the nation's clubs that the database has in no division (no national team, and no reserve or
         *   B side unless the list lacks one). The place of a missing reserve or B side takes such a club in
         *   every tier. In the top two tiers the place of another missing club stays open for slice 15b when the
         *   class counts the clubs of the data. A class with a fixed count stops at its assert with an open
         *   place, so there it takes a club with no division too, and the log calls it a stand-in;
         * - any other place (a club that moved up, or a class that takes more clubs than the real division has,
         *   slice 18): first a club that left a division of a higher rank (step "Leave"; no reserve or B side,
         *   and no move between two divisions of one rank such as German Regional North and South), so the
         *   clubs of the nation stay in its divisions (SweCup::build_club_pool 0x88da20 needs 244 Swedish clubs in
         *   them and loops for ever with fewer). Then a club that left a lower division with a list, then the
         *   nation's clubs with no division, then the clubs of the spare divisions, then the clubs of lower
         *   divisions that no list keeps. */
        for (int i = 0; i < count && now < d->want; i++) {
            Club *c = &club[i];
            if (c->kept || c->national || c->nation != d->nation || c->div || c->db_div != d->at) continue;
            c->div = d->at;
            now++;
            d->filled_own++;
        }
        int need = d->want - now;
        int missing_places = need < d->missing ? need : d->missing;
        int fixed = class_size(d->name, season) > 0;
        int reserve_places = missing_places < d->missing_reserve ? missing_places : d->missing_reserve;
        int places[2] = {d->tier > 2 || fixed ? missing_places : reserve_places, need - missing_places};
        /* The groups of a kind 1 place, in this order: 0 a club that left a higher division, 4 a club that left a
         * lower division with a list, 1 a club with no division, 2 a club of a spare division, 3 a club of a lower
         * division with no list. Groups 4 and 2 take the club with the highest reputation first (see FILL_BY_REP). */
        static const int kind1_groups[] = {0, 4, 1, 2, 3};
        for (int kind = 0; kind < 2; kind++)
            for (int g = 0; g < (kind ? 5 : 1) && places[kind] > 0; g++) {
                int group = kind ? kind1_groups[g] : 1;
                int by_rep = group == 4 || group == 2;
                for (int i = 0; i < count && places[kind] > 0; i++) {
                    int pick = by_rep ? best_fill(club, count, div, ncomps, comps, d, group, spare1, spare2,
                                                  cup_pool_nation)
                                      : (fill_takes(&club[i], div, ncomps, comps, d, group, spare1, spare2,
                                                    cup_pool_nation) ? i : -1);
                    if (pick < 0) {
                        if (by_rep) break;
                        continue;
                    }
                    Club *c = &club[pick];
                    c->div = d->at;         /* a lower division that gives a club fills in its turn, later */
                    now++;
                    places[kind]--;
                    if (group == 0) d->filled_left++;
                    else if (group == 4) d->filled_up++;
                    else if (group == 1) d->filled_pool++;
                    else if (group == 2) d->filled_spare++;
                    else d->filled_lower++;
                    if (kind == 0 && d->tier <= 2 && reserve_places-- <= 0) d->stand_in++;
                }
            }
        d->open = d->want - now;
        if (now < d->want) {
            short_after++;
            fprintf(stderr, "recomp: club moves: %s (tier %d) holds %d clubs, its class takes %d\n", d->name, d->tier,
                    now, d->want);
        }
    }
    /* Slice 20: a club that leaves a group division of a tier in its new form (Liga 3, Primera Federación) goes to the
     * tier's division of relegated clubs, as the season change sends it (season_divisions_leaver_division), not to
     * no division (O260 A24: the Portuguese Cup stopped at por_cup.cpp 74). A club with no nation stays out (the
     * shells above). */
    int to_lower = 0;
    for (int i = 0; i < count; i++) {
        Club *c = &club[i];
        if (c->div || !c->db_div || c->national || !c->nation) continue;
        uint32_t d = season_divisions_leaver_division(c->db_div, season);
        if (!d) continue;
        c->div = d;
        to_lower++;
    }
    if (to_lower)
        fprintf(stderr, "recomp: club moves: %d clubs that leave a group division of a new form go to its lower division\n",
                to_lower);
    /* Write the moves. */
    int moved = 0;
    for (int i = 0; i < count; i++) {
        if (club[i].div != club[i].db_div) {
            wr32(g_mem, club[i].at + CLUB_DIVISION, club[i].div);
            /* The division of the last season (+0x5b, resolve_database_indexes 0x5315a0 at 0x531657) becomes the
             * database division: the club comes from there. FraLgeCup::collect_entrants 0x5bd160 takes 14 French
             * National clubs whose last division is set and is not CFA (0x5bd222..0x5bd249) and stops at
             * fra_lge_cup.cpp 73 (0x5bca3a) without 54 entrants. A club that had no division keeps its value. */
            if (club[i].db_div) wr32(g_mem, club[i].at + CLUB_LAST_DIVISION, club[i].db_div);
            moved++;
        }
    }
    int lists = 0, whole = 0;
    for (int t = 0; t < ncomps; t++) {
        if (!div[t].target && !div[t].sized) continue;
        lists += div[t].target;
        whole += div[t].target && div[t].found == div[t].listed && div[t].kept == div[t].listed &&
                 div[t].listed == div[t].want;
        if (log)
            fprintf(stderr, "recomp: club moves: list: %d/%02d\t%s\ttier %d\treal %d\tfound %d\tkept %d\town %d\t"
                            "left %d\tup %d\tno division %d\tspare %d\tlower %d\tstand-in %d\topen %d\tclass %d\tdatabase %d\n",
                    season, (season + 1) % 100, div[t].name, div[t].tier, div[t].listed, div[t].found, div[t].kept,
                    div[t].filled_own, div[t].filled_left, div[t].filled_up, div[t].filled_pool, div[t].filled_spare, div[t].filled_lower, div[t].stand_in,
                    div[t].open, div[t].want, div[t].n0);
    }
    /* Test switch RECOMP_CLUB_DUMP=<file>: after the moves, one line per club (no national team) with its id, name
     * and division, tab-separated. A tab or line end in a name is written as a space (the data holds such names).
     * Nothing is written when the moves do not run. */
    const char *dump = getenv("RECOMP_CLUB_DUMP");
    FILE *df = dump && *dump ? host_fopen(dump, "w") : NULL;
    for (int i = 0; df && i < count; i++) {
        if (club[i].national) continue;
        char name[64], division[64];
        snprintf(name, sizeof name, "%s", win_str(club[i].at + CLUB_NAME));
        snprintf(division, sizeof division, "%s", club[i].div ? win_str(club[i].div + COMP_NAME) : "none");
        for (char *q = name; *q; q++) if (*q == '\t' || *q == '\n' || *q == '\r') *q = ' ';
        for (char *q = division; *q; q++) if (*q == '\t' || *q == '\n' || *q == '\r') *q = ' ';
        fprintf(df, "%d\t%s\t%s\n", club[i].id, name, division);
    }
    if (df) fclose(df);
    fprintf(stderr, "recomp: club moves of %d/%02d: %d divisions with a list (%d whole), %d clubs moved, %d left "
                    "their division, %d cut to the class count, %d listed clubs not in this database (%d found by "
                    "the plain name), %d divisions not in it, %d divisions short, %d divisions with another class "
                    "count\n", season, (season + 1) % 100, lists, whole, moved, left, cut, unmatched, by_plain,
            unknown_division, short_after, resized);
    free(order);
    free(club);
    free(div);
    free(by_list);
    free(to_div);
}

void o_00524d70(void);

/* database_trim_staff_tables 0x524d70, cdecl (char database_size): called once, at 0x828389 in create_game,
 * after the load (0x8282a7) and before the trim of the club table (0x5267c0 at 0x828395). The moves run
 * before it, on the whole data of every nation: the trims then keep the clubs and staff of the moved data, as
 * they would for a database that held the real divisions. (After the trims the clubs of the nations that the
 * player did not select are cut down to a few per nation, so a later point would see broken divisions.) */
RT_WRAP(00524d70, trim_staff_tables)
{
    const char *off = getenv("RECOMP_CLUB_MOVES");     /* test switch: off moves no club */
    season_divisions_test_extra();                     /* slice 20: test switch RECOMP_TEST_EXTRA */
    if (season_game_changes() && nrows && !(off && !strcmp(off, "off"))) move_clubs(season_start_year());
    if (season_game_changes()) season_records_fill(season_start_year());     /* slice 15b: after the moves */
    o_00524d70();
}
