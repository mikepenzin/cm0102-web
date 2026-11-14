/* The rules report and the season jump (0.6.0 slice 12, Q265). Test runs only: without the variables below
 * the human turn calls season_report_turn, which returns at once and changes nothing.
 *
 * RECOMP_RULES_REPORT=<file>: the runtime writes the competitions that the game holds, twice for each season:
 *   start  at the first human turn of the game, then at the first human turn on or after 1 August;
 *   end    at the first human turn on or after 1 June of the next year (before the update days of the league
 *          table entries, mostly 20 or 21 June: 2026-10-16-sweep-season-end.md, section 2.1). A national team
 *          tournament makes its team list after the start (the World Cup holds 0 clubs on 14 July 2001).
 * It only reads the game's memory.
 * At each start it also writes the clubs of each division (club lines, slice 15). RECOMP_RULES_REPORT_STOP=start
 * ends the program with code 0 after the first start report (the club moves and names of a start season).
 * RECOMP_DATA_SWEEP=1 (0.6.0 slice 30a, check 4 in data form; with RECOMP_RULES_REPORT): at each start and end
 * report the runtime also checks every competition object and each built stage of it (stage array +0xc, slots 0
 * to the current stage +0x30, at most the count +0x2c) and writes a "sweep" line for each fault, and one
 * "sweepok" line a report with what it read:
 *   - date: a round whose year is outside the edition (the year +0x40 of the object that holds the round table,
 *     less one to plus two), or whose day is not a day of the year. Cup rounds: table +0xa3, +0x3c rounds of 0x68
 *     bytes, the draw day word +0 and its year offset byte +2 (cup_set_round_draw_date 0x521e60 at 0x521ea6,
 *     0x521ea9), the match day word +3 and its year offset byte +5 (cup_set_round_match_date 0x521eb0). League
 *     rounds: table +0xba, +0xa9 rounds (word, 0x68b158) of 0x41 bytes, the day word +0 and the year offset word
 *     +2 (league_round_set_date 0x68a160 at 0x68a1a5, 0x68a1ac). These are the dates that the Schedule and
 *     Fixtures tabs of the competition screen show. An object waiting for an edition in 2999 or later (slices 21b,
 *     26) is left out;
 *   - empty: a league (kind league, not the Club World Championship in a form of slice 26) that has drawn its
 *     fixtures (+0x45 1, Comp::Comp 0x48c5bb) while the rules table gives its season clubs, with no club in its
 *     table (+0x3e 0) or an empty row (table +0xb1, rows of 0x3b bytes, the club at +0, 0x6826d0).
 * It only reads memory.
 * RECOMP_SEASON_JUMP=<year>: the game plays on with no match engine, and the program ends with code 0 after
 * the end report of the season that starts in that year. The test steps put the manager on holiday
 * (tests/seasons/rules_report_check.py), so the days pass with no input. A value that is not a year from 2001
 * to 2099 stops the program at the start (season_report_init).
 *
 * No match engine: the original has a switch for it, ENABLE_MATCH_ENGINE=NO in its setup file
 * (create_game_from_setup_file 0x827110 compares with "YES" 0xa812c0 and "NO" 0xa812bc and writes the byte
 * 0xa5a390 at 0x8277a6 and 0x8277cf; it is 1 in the data of the exe). With the byte 0, MATCH_DAY::play_matches
 * 0x6b4360 plays the human club's match with the others (the test at 0x6b4a29 skips the deferred path at
 * 0x6b4df5), and CM3_MATCH_ENGINE::init_match 0x6b7f20 sets its quick flag (0x6b85fb..0x6b8604), so every
 * match is played by the quick result engine CM3_MATCH_ENGINE::quick_simulate_match 0x6beeb0. The results go
 * through match_day_apply_match_result 0x6b5f00 into each competition as in a normal day, so the tables,
 * prize money, history, records and the end-of-season step run (docs/research/2026-10-16-sweep-season-end.md,
 * section 2.4). The fifth reader, match_stats_get_record_bucket 0x729bd0, returns bucket 3 at 0x729c32 only
 * with the byte 1. The jump writes the byte at the first human turn, after the game is made.
 *
 * What the report reads, for each competition object (docs/research/2026-10-17-rules-report.md):
 * - The objects: the table g_comp_object_table [0xadadfc] with [0xadadf8] slots, one per club competition
 *   record (built by 0x8386f0 from the count 0xae2378), and the competition arrays of the 35 league table
 *   entries at 0xb63d60 (0x48 bytes each; +0xc count, +0x10 array; game_season_update_pass 0x5d5cab..0x5d5cbb).
 *   An object in both is written once.
 * - +0 the vtable: the class and its source file (season_comp_classes, tools/seasons/make_comp_classes.py).
 * - +4 the competition record (club_comp.dat or nation_comp.dat, 0x6b bytes): +0 its id, +4 its long name
 *   (docs/research/2026-10-13-database-inventory.md; the record is read at 0x689b66 and 0x48f2df).
 * - +0x40 the season year (s16; League start_new_season adds 1, for example 0x574c10).
 * - +0x3e the club count (s16): a league compares each table row with it (League::check_complete 0x689b3d);
 *   a cup sets it in its constructor (EngCcCup 0x56c32d: 92).
 * - +0x3c (s16): for a league, the times that each pair of clubs meets ((clubs - 1) * +0x3c = matches per
 *   club, 0x689b27..0x689b36); for a cup, the round count (Cup::map_fixture_tree 0x51879c).
 * - +0x4a substitutes used and +0x49 substitutes named (bytes; Comp::Comp 0x48c5c9 sets 5 and 3;
 *   Comp::set_fixture_substitutes 0x48f2fa..0x48f32d packs them into the fixture).
 * Leagues only (set_up_rules, for example EngFirst 0x572d90 and EngPrm 0x574b70):
 * - +0xa7 (s16) matches per club (0x689b1d).
 * - +0xbe promotion places: League::has_automatic_promotion 0x684790 reads it (EngFirst 2, EngPrm 0).
 * - +0xbf promotion play-off places (EngFirst 4, EngThird 4 at 0x578104; inference from the values).
 * - +0xc0 places beside both ends that are neither safe nor direct (League::update_positions_and_status
 *   0x6852b3..0x6852e4 adds it to +0xbe at the top and to +0xc1 at the bottom; 0 in England; inference:
 *   relegation play-off places).
 * - +0xc1 relegation places: League::has_relegation 0x6847b0 reads it (EngPrm 3, EngFirst 3).
 * - +0xd9 (u16) rule flags: bit 1 automatic promotion (0x68479a), bit 0x20 status marks (0x6852a0).
 * - +0x1c and +0x20 (s32) the record ids of the divisions above and below: EngFirst 0x572dea and 0x572df6
 *   store the key ids [0x9cf5bc] (English Premier Division) and [0x9cf5c4]; EngPrm 0x574bd0 stores -1. */
#include "win.h"
#include "season_comps.h"
#include "season.h"
#include "season_rules.h"
#include "season_cups.h"
#include "season_comps.h"
#include "season_uefa.h"
#include "season_comps.h"
#include "../host.h"
#include "../rt_host.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATCH_ENGINE_ON 0xa5a390u   /* u8, 1 in the exe data */
#define COMP_TABLE 0xadadfcu        /* Comp ** */
#define COMP_SLOTS 0xadadf8u        /* s32 */
#define ENTRIES 0xb63d60u           /* league table entries */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35u
#define MAX_COMPS 4096

static int jump_to = -1;            /* -1: off */
static int started;                 /* the first human turn has run */
static FILE *report;
static int last_start = -1;         /* the last season with a start report */
static int last_end = -1;           /* the last season with an end report */
static int stop_at_start;           /* RECOMP_RULES_REPORT_STOP=start: end after the first start report */

static const SeasonCompClass *class_of(uint32_t vtable)
{
    int lo = 0, hi = season_comp_class_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (season_comp_classes[mid].vtable == vtable) return &season_comp_classes[mid];
        if (season_comp_classes[mid].vtable < vtable) lo = mid + 1;
        else hi = mid - 1;
    }
    return season_comps_class(vtable);     /* slice 28: a vtable copy of a new competition (season_comps.h) */
}

/* A pointer of the game that can be read: inside the 32-bit space and not near 0. */
static int readable(uint32_t p) { return rt_readable(p); }

static void write_cup_rounds(FILE *f, int season, const char *phase, const char *slot, const char *file,
                             uint32_t obj);

/* ---- RECOMP_DATA_SWEEP (slice 30a) ---- */

static int sweep_on;
static int sweep_objects, sweep_tables, sweep_rounds, sweep_faults;

static void sweep_fault(FILE *f, int season, const char *phase, const char *file, uint32_t obj, const char *what)
{
    uint32_t rec = rd32(g_mem, obj + 4);
    char name[64] = "?";
    if (readable(rec)) {
        snprintf(name, sizeof name, "%.51s", win_str(rec + 4));
        for (char *p = name; *p; p++)
            if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';
    }
    fprintf(f, "sweep\t%d\t%s\t%s\t%s\t%s\n", season, phase, file, name, what);
    sweep_faults++;
}

/* The rounds of one round table: cup (0x68 bytes, two dates) or league (0x41 bytes, one date). */
static void sweep_rounds_of(FILE *f, int season, const char *phase, const char *file, uint32_t owner, uint32_t holder,
                            int cup)
{
    int base = (int16_t)rd16(g_mem, holder + 0x40);
    int n = cup ? (int16_t)rd16(g_mem, holder + 0x3c) : (int16_t)rd16(g_mem, holder + 0xa9);
    uint32_t table = rd32(g_mem, holder + (cup ? 0xa3u : 0xbau)), size = cup ? 0x68u : 0x41u;
    if (base < 1990 || base >= 2999 || n < 1 || n > 128 || !readable(table) || !readable(table + size * (uint32_t)n))
        return;
    sweep_tables++;
    for (int i = 0; i < n; i++) {
        uint32_t r = table + size * (uint32_t)i;
        int day[2] = {(int16_t)rd16(g_mem, r), cup ? (int16_t)rd16(g_mem, r + 3) : 0};
        int off[2] = {cup ? (int8_t)g_mem[r + 2] : (int16_t)rd16(g_mem, r + 2), cup ? (int8_t)g_mem[r + 5] : 0};
        for (int k = 0; k < (cup ? 2 : 1); k++) {
            int year = base + off[k];
            sweep_rounds++;
            if (day[k] < 0 || day[k] > 365 || year < base - 1 || year > base + 2) {
                char what[160];
                snprintf(what, sizeof what, "date: %s round %d of %d, %s day %d of %d (edition %d)",
                         holder == owner ? "own" : "stage", i, n, k ? "match" : (cup ? "draw" : "match"), day[k],
                         year, base);
                sweep_fault(f, season, phase, file, owner, what);
            }
        }
    }
}

static void sweep_comp(FILE *f, int season, const char *phase, const char *file, const SeasonCompClass *c,
                       uint32_t obj)
{
    sweep_objects++;
    if (c && (c->kind == SEASON_KIND_CUP || c->kind == SEASON_KIND_LEAGUE))
        sweep_rounds_of(f, season, phase, file, obj, obj, c->kind == SEASON_KIND_CUP);
    /* Only the built stages, slots 0 to +0x30, as the game reads them (the save, 0x689918). start_new_season
     * deletes those stages and leaves their pointers in the slots (EuropeanCup 0x584e0d..0x584e23, EngFirst
     * 0x572e76..0x572e8a) before it sets +0x30 to -1 (0x584e28, 0x572e96): a slot up to the count +0x2c can hold
     * a freed stage, whose round table gave false date faults at the 2004/05 start of a 2003 game. */
    uint32_t stages = rd32(g_mem, obj + 0xc);
    int32_t slots = (int32_t)rd32(g_mem, obj + 0x2c), built = (int32_t)rd32(g_mem, obj + 0x30) + 1;
    if (built < slots) slots = built;
    for (int32_t i = 0; readable(stages) && i < slots && i < 64; i++) {
        uint32_t st = rd32(g_mem, stages + 4u * (uint32_t)i);
        if (!readable(st) || st == obj) continue;
        if (g_mem[st + 0x43] == 1) sweep_rounds_of(f, season, phase, file, obj, st, 1);       /* a Cup_Stage */
        else if (g_mem[st + 0x43] == 2) sweep_rounds_of(f, season, phase, file, obj, st, 0);  /* a League_Stage */
    }
    if (!c || c->kind != SEASON_KIND_LEAGUE || rd32(g_mem, obj + 0x45) != 1) return;
    /* The Club World Championship in Form K (one knockout stage) or Form G (8 stages) of slice 26: its parent
     * League holds no club by design, club count +0x3e 0 and one zeroed row (season_kind_club_world.c, part b).
     * Only the original form has 3 stage slots (+0x2c). */
    if (rd32(g_mem, obj) == 0x97119cu && (int32_t)rd32(g_mem, obj + 0x2c) != 3) return;
    /* A league the game never plays: no year at +0x40 and no club count at +0x3e, as the French Lower Division (FraFirst
     * record, vtable 0x96a904), whose clubs the game keeps in A Lower Division (2026-10-17-club-moves-and-names.md,
     * "The French CFA and Lower Division"). It shares the file name of Ligue 1, so the rule of fra_first is not its rule.
     * The rules report skips such objects too (2026-10-21-expected-files-060.md). */
    if ((int16_t)rd16(g_mem, obj + 0x40) == 0 && (int16_t)rd16(g_mem, obj + 0x3e) == 0) return;
    const SeasonRule *r = season_rules_find(season, file, 0);
    if (!r || r->clubs <= 0) return;
    int n = (int16_t)rd16(g_mem, obj + 0x3e);
    uint32_t table = rd32(g_mem, obj + 0xb1);
    int empty = n <= 0 || !readable(table) ? -1 : 0;
    for (int i = 0; !empty && i < n && i < 128; i++)
        if (!readable(rd32(g_mem, table + 0x3bu * (uint32_t)i))) empty = i + 1;
    if (empty) {
        char what[160];
        if (empty < 0) snprintf(what, sizeof what, "empty: no club in the table, the rules give %d", r->clubs);
        else snprintf(what, sizeof what, "empty: row %d of %d has no club, the rules give %d", empty, n, r->clubs);
        sweep_fault(f, season, phase, file, obj, what);
    }
}

static void write_comp(FILE *f, int season, const char *phase, const char *slot, uint32_t obj)
{
    uint32_t vt = rd32(g_mem, obj), rec = rd32(g_mem, obj + 4);
    const SeasonCompClass *c = class_of(vt);
    /* Slice 21c: a new competition with its def's vtable copy reports the class of the copy and the def's file. */
    SeasonCompClass copy;
    if (!c && season_comps_base_vtable(vt) && class_of(season_comps_base_vtable(vt))) {
        copy = *class_of(season_comps_base_vtable(vt));
        copy.file = season_comps_file_of(obj);
        c = &copy;
    }
    char name[64] = "?";
    int id = -1;
    if (readable(rec)) {
        id = (int)rd32(g_mem, rec);
        snprintf(name, sizeof name, "%.51s", win_str(rec + 4));
        for (char *p = name; *p; p++)
            if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';
    }
    int league = c && c->kind == SEASON_KIND_LEAGUE;
    char rules[128] = "-\t-\t-\t-\t-\t-\t-\t-";
    if (league)
        snprintf(rules, sizeof rules, "%d\t%d\t%d\t%d\t%d\t0x%x\t%d\t%d", (int16_t)rd16(g_mem, obj + 0xa7),
                 (int8_t)g_mem[obj + 0xbe], (int8_t)g_mem[obj + 0xbf], (int8_t)g_mem[obj + 0xc0],
                 (int8_t)g_mem[obj + 0xc1], rd16(g_mem, obj + 0xd9), (int32_t)rd32(g_mem, obj + 0x1c),
                 (int32_t)rd32(g_mem, obj + 0x20));
    /* Slice 23: a UEFA cup of the access list counts its clubs, not its entry slots (season_uefa_qual.c). */
    int clubs = season_uefa_report_clubs(obj);
    if (clubs < 0) clubs = (int16_t)rd16(g_mem, obj + 0x3e);
    /* Slice 24: a new competition of an original class has the file of its def (season_comps.h). */
    const char *file = season_comps_file(obj);
    if (!file) file = c ? c->file : "?";
    fprintf(f, "comp\t%d\t%s\t%s\t%d\t%s\t%s\t%s\t0x%x\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n", season, phase, slot, id,
            file, c ? c->cls : "?",
            c ? (league ? "league" : c->kind == SEASON_KIND_CUP ? "cup" : "comp") : "?", vt,
            (int16_t)rd16(g_mem, obj + 0x40), clubs, (int16_t)rd16(g_mem, obj + 0x3c),
            g_mem[obj + 0x4a], g_mem[obj + 0x49], rules, name);
    if (c && c->kind == SEASON_KIND_CUP) write_cup_rounds(f, season, phase, slot, file, obj);
    if (sweep_on) sweep_comp(f, season, phase, file, c, obj);
}

/* A cup line (slice 21): the rounds of a cup as the original's round table holds them (cup+0x3c rounds, table at
 * cup+0xa3, 0x68 bytes a round; docs/research/2026-10-17-cup-round-record.md). One word for each round:
 * legs (+0x21), replays (+0x20), the match flags of the first match (+0x0d) and of the deciding match (+0x0f),
 * as legs:replays:first:deciding with the flags in hexadecimal. */
static void write_cup_rounds(FILE *f, int season, const char *phase, const char *slot, const char *file, uint32_t obj)
{
    int n = (int16_t)rd16(g_mem, obj + 0x3c);
    uint32_t table = rd32(g_mem, obj + 0xa3);
    if (n < 1 || n > 32 || !readable(table) || !readable(table + 0x68u * (uint32_t)n)) return;
    fprintf(f, "cup\t%d\t%s\t%s\t%s\t%d\t", season, phase, slot, file, n);
    for (int i = 0; i < n; i++) {
        uint32_t r = table + 0x68u * (uint32_t)i;
        fprintf(f, "%s%d:%d:%x:%x", i ? " " : "", g_mem[r + 0x21], g_mem[r + 0x20], rd16(g_mem, r + 0x0d),
                rd16(g_mem, r + 0x0f));
    }
    fprintf(f, "\t0x%x\n", obj);                  /* the object, for a test that follows one cup */
}

/* The clubs of each division at a season start (0.6.0 slice 15): one line per club whose division pointer
 * (club +0x57, resolve_database_indexes 0x5315a0 at 0x531625) is set: the record id of the division, the club
 * id (+0) and its long name (+4). The club table is [0xae23b4] with [0xae235c] clubs of 0x245 bytes. */
static void write_clubs(int season, const char *phase)
{
    uint32_t table = rd32(g_mem, 0xae23b4u);
    int32_t count = (int32_t)rd32(g_mem, 0xae235cu);
    for (int32_t i = 0; readable(table) && i < count && i < 1000000; i++) {
        uint32_t club = table + (uint32_t)i * 0x245u, div = rd32(g_mem, club + 0x57);
        if (!readable(div)) continue;
        char name[64];
        snprintf(name, sizeof name, "%.51s", win_str(club + 4));
        for (char *p = name; *p; p++)
            if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';
        fprintf(report, "club\t%d\t%s\t%d\t%d\t%s\n", season, phase, (int)rd32(g_mem, div), (int)rd32(g_mem, club),
                name);
    }
}

static void write_season(int season, const char *phase)
{
    int y = 0, m = 0, d = 0;
    win_game_date(&y, &m, &d);
    fprintf(report, "season\t%d\t%s\t%04d-%02d-%02d\t%d\t%s\n", season, phase, y, m, d, season_start_year(),
            season_changing_rules() ? "on" : "off");
    static uint32_t seen[MAX_COMPS];
    int nseen = 0;
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int32_t slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    char slot[32];
    for (int32_t i = 0; readable(table) && i < slots && i < MAX_COMPS; i++) {
        uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)i);
        if (!readable(obj)) continue;
        if (nseen < MAX_COMPS) seen[nseen++] = obj;
        snprintf(slot, sizeof slot, "t%d", i);
        write_comp(report, season, phase, slot, obj);
    }
    for (uint32_t e = 0; e < ENTRY_COUNT; e++) {
        uint32_t entry = ENTRIES + e * ENTRY_SIZE;
        int32_t n = (int32_t)rd32(g_mem, entry + 0xc);
        uint32_t arr = rd32(g_mem, entry + 0x10);
        for (int32_t k = 0; readable(arr) && k < n && k < 256; k++) {
            uint32_t obj = rd32(g_mem, arr + 4u * (uint32_t)k);
            if (!readable(obj)) continue;
            int dup = 0;
            for (int s = 0; s < nseen && !dup; s++) dup = seen[s] == obj;
            if (dup) continue;
            if (nseen < MAX_COMPS) seen[nseen++] = obj;
            snprintf(slot, sizeof slot, "e%u.%d", e, k);
            write_comp(report, season, phase, slot, obj);
        }
    }
    season_uefa_report(report, season, phase);      /* slice 22: the stages of the Champions League */
    season_cups_more_report(report, season, phase); /* slice 21c: super cups, group stages, new cups */
    if (*phase == 's') write_clubs(season, phase);
    if (sweep_on) {
        fprintf(report, "sweepok\t%d\t%s\t%d objects\t%d round tables\t%d dates\t%d faults\n", season, phase,
                sweep_objects, sweep_tables, sweep_rounds, sweep_faults);
        sweep_objects = sweep_tables = sweep_rounds = sweep_faults = 0;
    }
    fflush(report);
    fprintf(stderr, "recomp: rules report: season %d/%02d %s on %02d.%02d.%04d\n", season, (season + 1) % 100, phase,
            d, m, y);
    /* The game's heap at each report (the arena from RT_HEAP_BASE): the top, the free bytes below it,
     * and the bytes in use. A text line only. */
    uint32_t top = rt_heap_top() - RT_HEAP_BASE, freed = rt_heap_free_bytes();
    fprintf(stderr, "recomp: heap at season %d/%02d %s: top %u MB of %u MB, free below the top %u MB, in use %u MB\n",
            season, (season + 1) % 100, phase, top >> 20, rt_heap_arena_bytes() >> 20, freed >> 20,
            (top - freed) >> 20);
    uint32_t sb, sn, spans;
    rt_heap_free_parts(&sb, &sn, &spans);
    fprintf(stderr, "recomp: heap free parts: small blocks %u MB in %u blocks, large spans %u MB in %u spans\n", sb >> 20,
            sn, (freed - sb) >> 20, spans);
}

void season_report_init(void)
{
    const char *v = getenv("RECOMP_SEASON_JUMP");
    if (v && *v) {
        char *end;
        long y = strtol(v, &end, 10);
        if (*end || y < 2001 || y > 2099) {
            fprintf(stderr, "cm0102_recomp: RECOMP_SEASON_JUMP=%s is not a year from 2001 to 2099.\n", v);
            exit(2);
        }
        jump_to = (int)y;
    }
    const char *sweep = getenv("RECOMP_DATA_SWEEP");
    sweep_on = sweep && *sweep == '1';
    const char *stop = getenv("RECOMP_RULES_REPORT_STOP");
    stop_at_start = stop && !strcmp(stop, "start");
    const char *path = getenv("RECOMP_RULES_REPORT");
    if (path && *path) {
        report = host_fopen(path, "w");
        if (!report) {
            fprintf(stderr, "cm0102_recomp: RECOMP_RULES_REPORT: cannot write %s.\n", path);
            exit(2);
        }
        fprintf(report, "#bosman-rules-report\t2\n"
                "columns\tseason\tphase\tslot\trecord\tfile\tclass\tkind\tvtable\tyear\tclubs\trounds\tsubs_used"
                "\tbench\tmatches\tup\tplayoff_up\tplayoff_down\tdown\tflags\tabove\tbelow\tname\n");
    }
}

void season_report_turn(void)
{
    if (jump_to < 0 && !report) return;
    if (!started) {
        started = 1;
        if (jump_to >= 0) {
            g_mem[MATCH_ENGINE_ON] = 0;
            fprintf(stderr, "recomp: season jump to the end of %d/%02d with no match engine\n", jump_to,
                    (jump_to + 1) % 100);
        }
    }
    int y = 0, m = 0, d = 0;
    if (!win_game_date(&y, &m, &d)) return;
    int season = -1;
    const char *phase = NULL;
    if (last_start < 0) season = season_start_year(), phase = "start";
    else if (m >= 8 && y > last_start) season = y, phase = "start";
    else if (m >= 6 && m < 8 && y - 1 > last_end && y - 1 >= season_start_year())
        season = y - 1, phase = "end";
    if (phase) {
        if (report) write_season(season, phase);
        else fprintf(stderr, "recomp: season jump: season %d/%02d %s\n", season, (season + 1) % 100, phase);
        if (*phase == 's') last_start = season;
        else last_end = season;
    }
    if (stop_at_start && last_start >= 0) last_end = jump_to = last_start;   /* RECOMP_RULES_REPORT_STOP=start */
    if (jump_to >= 0 && last_end >= jump_to) {
        if (report) fclose(report);
        report = NULL;
        fprintf(stderr, "recomp: season jump: done\n");
        if (rt_on_fatal) rt_on_fatal();
        SDL_Quit();
        exit(0);
    }
}
