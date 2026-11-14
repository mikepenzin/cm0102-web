/* New competitions (0.6.0; season_comps.h, design docs/research/2026-10-18-new-comps.md). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_rules.h"
#include "season_uefa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_RECORDS 0xae23d0u          /* the club comp records, then the nation comp records */
#define COMP_RECORD_COUNT 0xae2378u     /* s32: the club comp records ([0xadadfc] has one slot each, 0x8386fb) */
#define COMP_TABLE 0xadadfcu            /* Comp **, indexed by record id (record +0) */
#define COMP_SLOTS 0xadadf8u            /* s32 */
#define RECORD_SIZE 0x6bu
#define NAME_AT 4u
#define ENTRIES 0xb63d60u               /* league table entries, 0x48 bytes */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35
#define GAME_YEAR 0xae2c92u             /* s16 */
#define NEVER 2999

/* ---- The competitions: each slice adds its lines here, in its own block ---- */

/* Slice 28 (season_nl.c, season_gold_cup.c): the UEFA and CONCACAF Nations Leagues, the CONCACAF Gold Cup and the
 * Finalissima. */
extern const SeasonCompDef season_nl_comp, season_nl_concacaf_comp, season_gold_cup_comp, season_finalissima_comp;

/* Test only (RECOMP_SEASON_COMPS_TEST=1, tests/seasons/season_comps_check.py): a new record "Test New Cup" with an
 * object of the Inter-American Cup class (Inter_Amer_Cup 0x632080, thiscall (year, record), ret 8, new 0xb2,
 * sweep-europe 2.1) in entry 2. It takes the rows of the Intercontinental Cup, so it is made in 2001/02 to
 * 2004/05. Without the variable the module skips it. */
static uint32_t test_make(const SeasonCompDef *d, uint32_t rec, int year, int loading);
static const SeasonCompDef test_new_cup = {"intercontinental_cup", "test_new_cup", "Test New Cup", 2, test_make,
                                           "UEFA Cup", "Test Cup", 10};

/* Slice 21c: the new domestic cups (season_new_cups.c). */
extern const SeasonCompDef season_def_tur_super, season_def_rus_super, season_def_nor_super, season_def_rus_lge_cup,
    season_def_conf_cup, season_def_all_ireland;

/* Slice 26c: the new club cups (season_club_cups.c). */
extern const SeasonCompDef season_cup_sudamericana, season_cup_recopa, season_cup_suruga_bank,
    season_cup_intercontinental, season_cup_acl_two;

/* Slice 20d (season_divisions.c): the second group of the Greek second tier, a GreSecond object. */
extern const SeasonCompDef season_div_gre_group;

/* Slice 24, part b (U8, Q364): the Conference League from 2021/22, a new record with a second object of the
 * Uefa_Cup class in the Europe entry 3 (docs/research/2026-10-18-conference-league-object.md). Its record copies
 * the UEFA Cup's continent, nation and colours, and its level (+0x69) is the UEFA Cup's 18: the fixture order
 * (0x48da90), the team pick (MATCH_MAN::pick_team 0x6dc154) and the referee order (0x79d410) read it, so the
 * cup counts as much as the Europa League (doc, decision 5). season_uefa_cup_kind tells it from the Europa League by
 * its record, so the wraps of season_uefa_cup.c and season_uefa_qual.c give it its own rules. */
static uint32_t ecl_make(const SeasonCompDef *d, uint32_t rec, int year, int loading);
static const SeasonCompDef conference_league = {"Conference League", SEASON_UEFA_ECL_FILE, SEASON_UEFA_ECL_RECORD, 3,
                                                ecl_make, "UEFA Cup", "Conference League", 18};

/* Slice 20e: the state championships of 12 states of the North, Northeast and Central regions of Brazil, objects
 * of the North class (season_bra_states.c, Q332). */
extern const SeasonCompDef season_bra_state_ma, season_bra_state_am, season_bra_state_pi, season_bra_state_ac,
    season_bra_state_rn, season_bra_state_pb, season_bra_state_al, season_bra_state_se, season_bra_state_df,
    season_bra_state_es, season_bra_state_ms, season_bra_state_to;

/* Slice 20b: group C of the single Serie C (2014/15 on) on the C2/A object (season_divisions_ita.c). The original
 * makes C2/A only with the extra division flag; without it the module makes the object. */
extern const SeasonCompDef season_ita_group_c;

const SeasonCompDef *const season_comp_defs[] = {
    &test_new_cup,                      /* test only, see above */
    /* slice 26c */
    &season_cup_sudamericana, &season_cup_recopa, &season_cup_suruga_bank, &season_cup_intercontinental,
    &season_cup_acl_two,
    &conference_league,                 /* slice 24 */
    &season_nl_comp,                    /* slice 28 */
    &season_nl_concacaf_comp,           /* slice 28 part b */
    &season_gold_cup_comp,              /* slice 28 part b */
    &season_finalissima_comp,           /* slice 28 part b */
    &season_div_gre_group,              /* slice 20d: the second group of the Greek second tier */
    /* slice 20e */
    &season_bra_state_ma, &season_bra_state_am, &season_bra_state_pi, &season_bra_state_ac, &season_bra_state_rn,
    &season_bra_state_pb, &season_bra_state_al, &season_bra_state_se, &season_bra_state_df, &season_bra_state_es,
    &season_bra_state_ms, &season_bra_state_to,
    &season_def_tur_super,              /* slice 21c */
    &season_def_rus_super,
    &season_def_nor_super,
    &season_def_rus_lge_cup,
    &season_def_conf_cup,
    &season_def_all_ireland,
    &season_ita_group_c,                /* slice 20b */
};
const int season_comp_def_count = (int)(sizeof season_comp_defs / sizeof season_comp_defs[0]);

static uint32_t test_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    (void)loading;
    Cpu saved = cpu;
    uint32_t size[1] = {0xb2};
    uint32_t obj = rt_callback(0x944cf1u, 1, size);
    if (rt_unwinding || !obj) return 0;
    uint32_t args[2] = {(uint32_t)year, rec};
    cpu.ecx = obj;
    rt_callback(0x632080u, 2, args);
    if (rt_unwinding) return 0;
    cpu = saved;
    /* A vtable copy, as each real def's object has one (season_club_cups.c): with the original's vtable the names
     * kind of slice 15 sees a second Inter-American Cup (class_of in season_names.c) and gives the record that
     * class's name ("Copa Interamericana"), not the name of the def's rows. */
    uint32_t vt = season_comps_vtable(d, rd32(g_mem, obj), 40);
    if (vt) wr32(g_mem, obj, vt);
    return obj;
}

/* operator new 0x944cf1 with the class size 0xb3 (as 0x8314bf), then Uefa_Cup::Uefa_Cup 0x9022e0, thiscall
 * (year, record) ret 8. It writes the vtable 0x970790 (0x90232d), runs the seeding of the year (0x902377) and
 * takes its entrants (0x90237e); at a load it reads its state from its saved file (0x90235f). */
static uint32_t ecl_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    (void)d;
    (void)loading;
    Cpu saved = cpu;
    uint32_t size[1] = {0xb3};
    uint32_t obj = rt_callback(0x944cf1u, 1, size);
    if (rt_unwinding || !obj) return 0;
    uint32_t args[2] = {(uint32_t)year, rec};
    cpu.ecx = obj;
    rt_callback(0x9022e0u, 2, args);
    if (rt_unwinding) return 0;
    cpu = saved;
    return obj;
}

static int test_on(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SEASON_COMPS_TEST") && *getenv("RECOMP_SEASON_COMPS_TEST");
    return on;
}

/* The defs of this game: the test def only with its variable. */
static const SeasonCompDef *def_at(int i)
{
    const SeasonCompDef *d = season_comp_defs[i];
    return d == &test_new_cup && !test_on() ? NULL : d;
}

/* ---- State of the game that runs ---- */

#define MAX_DEFS 64
static uint32_t objects[MAX_DEFS];      /* the object of each def, 0 when not made */

static void reset(void) { memset(objects, 0, sizeof objects); }

int season_comps_on(void)
{
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

/* ---- The rules table ---- */

/* Slice 21c: a comp "Nation:comp" names the rows of that nation only (several nations have a comp "super cup"). */
static int comp_is(const SeasonRule *r, const char *comp)
{
    const char *colon = strchr(comp, ':');
    if (!colon) return !strcmp(r->comp, comp);
    size_t n = (size_t)(colon - comp);
    return strlen(r->nation) == n && !strncmp(r->nation, comp, n) && !strcmp(r->comp, colon + 1);
}

static const SeasonRule *row_of(const char *comp, int s)
{
    const SeasonRule *best = NULL;
    for (int i = 0; i < season_rules_count(); i++) {
        const SeasonRule *r = season_rules_row(i);
        if (!comp_is(r, comp) || r->season > s) continue;
        if (!best || r->season > best->season) best = r;
    }
    return best;
}

static int is_held(const SeasonRule *r)
{
    return r->clubs != SEASON_RULE_NOT_HELD && strcmp(r->format, "not_held") != 0;
}

int season_comps_held(const char *comp, int s)
{
    const SeasonRule *r = comp ? row_of(comp, s) : NULL;
    return r ? is_held(r) : -1;
}

int season_comps_next_edition(const char *comp, int s)
{
    int h = season_comps_held(comp, s);
    if (h < 0) return 0;
    if (h) return s;
    int last = 0;
    for (int i = 0; i < season_rules_count(); i++) {
        const SeasonRule *r = season_rules_row(i);
        if (comp_is(r, comp) && r->season > last) last = r->season;
    }
    for (int y = s + 1; y <= last; y++)
        if (season_comps_held(comp, y) == 1) return y;
    return NEVER;
}

/* Slice 21c: a def with its own rule (its `held`). */
int season_comps_def_held(const SeasonCompDef *d, int s)
{
    if (!d) return -1;
    return d->held ? d->held(d, s) : season_comps_held(d->comp, s);
}

static int def_next_edition(const SeasonCompDef *d, int s)
{
    if (!d->held) return season_comps_next_edition(d->comp, s);
    int h = d->held(d, s);
    if (h < 0) return 0;
    if (h) return s;
    for (int y = s + 1; y <= SEASON_LAST_YEAR; y++)
        if (d->held(d, y) == 1) return y;
    return NEVER;
}

/* ---- Records, objects and vtables ---- */

static int def_index(const SeasonCompDef *d)
{
    for (int i = 0; i < season_comp_def_count && i < MAX_DEFS; i++)
        if (season_comp_defs[i] == d) return i;
    return -1;
}

uint32_t season_comps_object(const SeasonCompDef *d)
{
    int i = def_index(d);
    return i < 0 ? 0 : objects[i];
}

uint32_t season_comps_record(const char *name)
{
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    int32_t count = (int32_t)rd32(g_mem, COMP_RECORD_COUNT);
    if (!name || !base || count <= 0 || count > 100000) return 0;
    for (int32_t i = 0; i < count; i++) {
        uint32_t rec = base + (uint32_t)i * RECORD_SIZE;
        const char *n = season_change_data_name(rec);
        if (n && !strcmp(n, name)) return rec;
    }
    return 0;
}

const char *season_comps_file(uint32_t obj)
{
    if (!rt_readable(obj)) return NULL;
    uint32_t rec = rd32(g_mem, obj + 4);
    if (!rt_readable(rec)) return NULL;
    const char *n = season_change_data_name(rec);
    for (int i = 0; n && i < season_comp_def_count; i++) {
        const SeasonCompDef *d = def_at(i);
        if (d && d->record && !strcmp(d->record, n)) return d->file;
    }
    return NULL;
}

int season_comps_is_def_record(uint32_t rec)
{
    for (int i = 0; rec && i < season_comp_def_count && i < MAX_DEFS; i++) {
        const SeasonCompDef *d = def_at(i);
        if (d && d->record && season_comps_record(d->record) == rec) return 1;
    }
    return 0;
}

uint32_t season_comps_top_club(uint32_t continent_key, const uint32_t *skip, int n)
{
    uint32_t clubs = rd32(g_mem, 0xae23b4u), want = rd32(g_mem, continent_key), best = 0;
    int32_t count = (int32_t)rd32(g_mem, 0xae235cu) - 2 * (int32_t)rd32(g_mem, 0xae2350u);
    int best_rep = -100000;
    for (int32_t i = 0; clubs && i < count; i++) {
        uint32_t c = clubs + (uint32_t)i * 0x245u, nation = rd32(g_mem, c + 0x53);
        uint32_t cont = nation ? rd32(g_mem, nation + 0x71) : 0;
        if (!cont || rd32(g_mem, cont) != want) continue;
        int rep = (int16_t)rd16(g_mem, c + 0x80);
        if (rep <= best_rep) continue;
        int taken = 0;
        for (int j = 0; j < n && !taken; j++) taken = skip[j] == c;
        if (!taken) best = c, best_rep = rep;
    }
    return best;
}

/* One copy per def for the whole run (runtime memory from win_static_alloc, never freed): a later call for the
 * same def gives the same copy, made again from the class's vtable. Slice 28: each copy and the class of its
 * original are kept for the rules report (season_comps_class). */
#define MAX_COPIES MAX_DEFS
static struct { uint32_t copy, original; } copies[MAX_COPIES];
static int copy_count;

uint32_t season_comps_vtable(const SeasonCompDef *d, uint32_t vtable, int slots)
{
    static uint32_t made[MAX_DEFS], sizes[MAX_DEFS];
    int i = def_index(d);
    if (i < 0 || i >= MAX_DEFS || slots <= 0 || slots > 128) return 0;
    if (!made[i] || sizes[i] < (uint32_t)slots) {
        made[i] = win_static_alloc(4u * (uint32_t)slots);
        sizes[i] = made[i] ? (uint32_t)slots : 0;
    }
    if (!made[i]) return 0;
    memcpy(g_mem + made[i], g_mem + vtable, 4u * (size_t)slots);
    int k = 0;
    while (k < copy_count && copies[k].copy != made[i]) k++;
    if (k < MAX_COPIES) {
        copies[k].copy = made[i];
        copies[k].original = vtable;
        if (k == copy_count) copy_count++;
    }
    return made[i];
}

/* Slice 28: the class of a vtable copy for the rules report and the season change: the original's class and
 * kind, with the file of the def whose object holds the copy (season_comps.h, "Rules for the slices"). */
const SeasonCompClass *season_comps_class(uint32_t vtable)
{
    static SeasonCompClass out;
    for (int i = 0; i < copy_count; i++) {
        if (copies[i].copy != vtable) continue;
        const SeasonCompClass *c = NULL;
        for (int k = 0; k < season_comp_class_count; k++)
            if (season_comp_classes[k].vtable == copies[i].original) c = &season_comp_classes[k];
        if (!c) return NULL;
        out = *c;
        out.vtable = vtable;
        for (int d = 0; d < season_comp_def_count && d < MAX_DEFS; d++)
            if (season_comp_defs[d] && objects[d] && rd32(g_mem, objects[d]) == vtable) out.file = season_comp_defs[d]->file;
        return &out;
    }
    return NULL;
}

/* Slice 21c: one vtable copy per def, kept for the run (static memory). */
static uint32_t def_vtables[MAX_DEFS], def_bases[MAX_DEFS];

uint32_t season_comps_def_vtable(const SeasonCompDef *d, uint32_t vtable, int slots)
{
    int i = def_index(d);
    if (i < 0) return 0;
    if (!def_vtables[i]) {
        def_vtables[i] = season_comps_vtable(d, vtable, slots);
        def_bases[i] = def_vtables[i] ? vtable : 0;
    }
    return def_vtables[i];
}

const char *season_comps_file_of(uint32_t obj)
{
    if (!rt_readable(obj)) return NULL;
    uint32_t vt = rd32(g_mem, obj);
    for (int i = 0; vt && i < season_comp_def_count && i < MAX_DEFS; i++)
        if (def_vtables[i] == vt) return season_comp_defs[i]->file;
    return NULL;
}

uint32_t season_comps_base_vtable(uint32_t vtable)
{
    for (int i = 0; vtable && i < season_comp_def_count && i < MAX_DEFS; i++)
        if (def_vtables[i] == vtable) return def_bases[i];
    return 0;
}

void season_comps_set_slot(uint32_t vtable, int slot, const char *name, void (*fn)(void))
{
    if (vtable && slot >= 0) wr32(g_mem, vtable + 4u * (uint32_t)slot, rt_host_fn(name, fn));
}

/* The entry exists in the game (season_comps.h): a national entry with its nation selected, a continental or
 * world entry with at least one object of the original. */
static int entry_in_game(int entry)
{
    if (entry < 0 || entry >= ENTRY_COUNT) return 0;
    uint32_t e = ENTRIES + (uint32_t)entry * ENTRY_SIZE;
    uint32_t nation = rd32(g_mem, e);
    if (nation) return g_mem[nation + 0x11c] != 0;     /* the selected flag, as the pass reads it (0x5d5be5) */
    return (int32_t)rd32(g_mem, e + 0xc) > 0;
}

/* [0xadadfc][record id] = obj. The daily update walks the table, not the entry arrays (0x5ce060), so an object
 * added after build_comp_object_table 0x8386f0 needs its slot (new-comps-load doc, section 1, point 5). */
static void set_table_slot(uint32_t rec, uint32_t obj)
{
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int32_t slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    int32_t id = (int32_t)rd32(g_mem, rec);
    if (table && id >= 0 && id < slots) wr32(g_mem, table + 4u * (uint32_t)id, obj);
}

/* The name of the competition in season s from the table, in the game's code, or "" with none. */
static void table_name(const SeasonCompDef *d, int s, char *out, size_t room)
{
    const SeasonRule *r = row_of(d->comp, s);
    snprintf(out, room, "%s", r && r->name && strcmp(r->name, "n/a") ? r->name : "");
    season_change_latin1(out);
    out[room > 51 ? 50 : room - 1] = 0;
}

static uint32_t make(const SeasonCompDef *d, int i, int s, int loading)
{
    uint32_t rec = season_comps_record(d->record);
    if (!rec || !d->make) {
        fprintf(stderr, "recomp: new competition %s: no record \"%s\" in the data\n", d->file,
                d->record ? d->record : "(none)");
        return 0;
    }
    uint32_t obj = d->make(d, rec, s, loading);
    if (!obj || rt_unwinding) return 0;
    if (!season_change_add_comp(d->entry, obj)) {
        fprintf(stderr, "recomp: new competition %s: not added to entry %d\n", d->file, d->entry);
        return 0;
    }
    objects[i] = obj;
    if (!loading) set_table_slot(rec, obj);
    fprintf(stderr, "recomp: new competition %s made for %d (entry %d, record %d)%s\n", d->file, s, d->entry,
            (int)rd32(g_mem, rec), loading ? " from the save" : "");
    return obj;
}

/* ---- The rule kind ---- */

static void comps_entry(const SeasonChange *c)
{
    if (!season_comps_on() || !entry_in_game(c->entry)) return;
    for (int i = 0; i < season_comp_def_count && i < MAX_DEFS; i++) {
        const SeasonCompDef *d = def_at(i);
        if (!d || d->entry != c->entry || objects[i]) continue;
        if (season_comps_def_held(d, c->season) != 1 &&
            !(d->early && season_changing_rules() && season_comps_def_held(d, c->season + 1) == 1))
            continue;   /* slice 28: `early`, only with changing rules on (Q398: with them off a cup that the start
                         * season does not hold stays off); slice 21c: the def's own `held` */
        uint32_t obj = make(d, i, c->season, 0);
        if (!obj) continue;
        char name[64];
        table_name(d, c->season, name, sizeof name);
        if (*name) season_change_rename(rd32(g_mem, obj + NAME_AT), name);
        if (*name) season_change_news("The %s is played for the first time.", name);
    }
}

/* A later name of the table (Q226, Q238). */
static void comps_after(const SeasonChange *c)
{
    if (!season_comps_on()) return;
    for (int i = 0; i < season_comp_def_count && i < MAX_DEFS; i++) {
        const SeasonCompDef *d = def_at(i);
        if (!d || !objects[i] || objects[i] != c->comp) continue;
        char name[64];
        table_name(d, c->season, name, sizeof name);
        uint32_t rec = rd32(g_mem, c->comp + NAME_AT);
        char old[52];
        snprintf(old, sizeof old, "%.50s", win_str(rec + NAME_AT));
        if (!*name || !strcmp(old, name)) continue;
        season_change_rename(rec, name);
        season_change_news("%s is now called %s.", old, name);
    }
}

const SeasonRuleKind season_kind_comps = {"comps", NULL, NULL, NULL, comps_entry, NULL, comps_after};

void season_bra_states_reset(void);    /* slice 20e: season_bra_states.h */
void season_comps_reset(void) { reset(); season_bra_states_reset(); }

/* ---- The load: build_comp_object_table 0x8386f0 ---- */

/* The file name that the object's save writes (Cup 0x5210fe..0x521162, League 0x6891ce..0x68923a): the long name
 * of the record with "/" and "." made "_", then "_<record id>.tmp". */
static void save_file(uint32_t rec, char *out, size_t room)
{
    char name[64];
    snprintf(name, sizeof name, "%.50s", win_str(rec + NAME_AT));
    for (char *p = name; *p; p++)
        if (*p == '/' || *p == '.') *p = '_';
    snprintf(out, room, "%s_%d.tmp", name, (int)rd32(g_mem, rec));
}

/* build_comp_object_table, cdecl, called by create_game (0x829640) and the load (0x82c5f6). Its one wrap is in
 * season_names.c (slices 15 and 26 both need it); it calls this before the original. At a load the
 * creators made only the original's objects (key_nation_create_all_comps 0x669180 at 0x82c5aa), so each def
 * whose object's file came out of the save is made again here, before the original fills [0xadadfc] from the
 * entry arrays. The constructor reads its state from that file. Inference: the temp files are in the user
 * folder, as the game's own *.tmp files of a run are. */
void season_comps_before_comp_table(void)
{
    if (season_comps_on() && !season_change_is_creating()) {
        reset();
        int year = (int16_t)rd16(g_mem, GAME_YEAR);
        for (int i = 0; i < season_comp_def_count && i < MAX_DEFS; i++) {
            const SeasonCompDef *d = def_at(i);
            if (!d || !entry_in_game(d->entry)) continue;
            uint32_t rec = season_comps_record(d->record);
            if (!rec) continue;
            char file[128], host[2048];
            save_file(rec, file, sizeof file);
            /* Only the file of the user folder, where the load extracts the save's *.tmp files: a stale file of
             * the game folder (win_existing_file gives that form too) is not this game's. */
            if (win_existing_file(file, host, sizeof host) && !strncmp(host, win_user_dir, strlen(win_user_dir)))
                make(d, i, year, 1);
            if (rt_unwinding) return;
        }
    }
}

/* ---- New records (docs/research/2026-10-18-new-comp-records.md) ----
 *
 * The comp records are one array of 0x6b-byte records at [0xae23d0]: C club comps, N nation comps, then F = 0x7f
 * friendly records. [0xae2378] = C + N + F, [0xae2380] = N, [0xae23d8] = the first nation comp (the loader's
 * sizing 0x529a8a..0x529aca and layout 0x529f6f..0x529fba). Record +0 is its index (doc 1.5). A new record goes
 * at C + N, before the friendly records: only the friendly records move, and they are blank at a new game
 * (0x52a060..0x52a1ab). The array lies in the loader's one block [0xae23a4], so the module copies it into a new
 * block (the game's malloc 0x944e46) and frees that block itself (database_free_all_tables 0x528080 only clears
 * the pointer, 0x528234). It does this only at the database load of a new game (database_load 0x528540 with the
 * file source, stream 0), before the id pass 0x5315a0 turns the ids of the records into pointers. A save holds
 * the records in its nation_comp.dat part (writer 0x52fb65), so a load gets them back with no change. */

#define NATION_COUNT 0xae2380u          /* s32: N */
#define NATION_RECORDS 0xae23d8u        /* the first nation comp */
#define FRIENDLY_RECORDS 0x7f
/* The most new records of one game. 13 defs have `like` on main at slice 21c's merge (2026-10-20, with slices 24, 26c
 * and 28); the ctest season_comps_new_records (tests/seasons/comps_new_records_check.py) fails when the defs with
 * `like` outgrow it, and a game logs every def that finds no room (wanted). */
#define MAX_NEW 32

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static int loading_files;               /* 1 inside database_load 0x528540 of a new game's files */
static int grown;                       /* 1 when the id pass of this load ran */
static uint32_t new_block;              /* the module's copy of the array, 0 when none */

/* The records the next new game needs: each def with `like` whose record the data lacks, and that the game can
 * make. wanted fills names (at most MAX_NEW) and returns the count. It needs the data (the record names). */
static int wanted(const char (*have)[52], int have_count, char (*names)[52])
{
    int start = season_start_year(), rules = season_next_rules(), n = 0;
    if (!season_code_on() || (start == SEASON_FIRST_YEAR && !rules)) return 0;   /* the base game, Q152 */
    for (int i = 0; i < season_comp_def_count; i++) {
        const SeasonCompDef *d = def_at(i);
        if (!d || !d->like || !d->record) continue;
        int h = season_comps_def_held(d, start);
        if (h == 0 && d->early && rules && season_comps_def_held(d, start + 1) == 1) h = 1;   /* slice 28 (Q398) */
        if (h < 0 || (!h && (!rules || def_next_edition(d, start) == NEVER))) continue;  /* Q232 */
        int known = 0;
        for (int j = 0; j < have_count; j++)
            if (!strcmp(have[j], d->record)) known = 1;
        if (known) continue;
        if (n >= MAX_NEW) {                 /* never silent: its competition would never come */
            fprintf(stderr, "recomp: new competitions: no room for the record of %s (more than %d new records)\n",
                    d->file, MAX_NEW);
            continue;
        }
        snprintf(names[n++], 52, "%.50s", d->record);
    }
    return n;
}

static void put_name(uint32_t at, const char *name, uint32_t room)
{
    memset(g_mem + at, 0, room);
    size_t len = strlen(name);
    if (len > room - 1) len = room - 1;
    memcpy(g_mem + at, name, len);
}

static const SeasonCompDef *def_of_record(const char *name)
{
    for (int i = 0; i < season_comp_def_count; i++)
        if (season_comp_defs[i] && def_at(i) && def_at(i)->record && !strcmp(def_at(i)->record, name))
            return def_at(i);
    return NULL;
}

/* Before the id pass of a new game's load: the array gets the new records. */
static void grow(void)
{
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    int32_t count = (int32_t)rd32(g_mem, COMP_RECORD_COUNT), nations = (int32_t)rd32(g_mem, NATION_COUNT);
    int32_t cn = count - FRIENDLY_RECORDS;
    if (!base || cn <= 0 || nations < 0 || nations > cn || count > 100000) return;
    static char have[4096][52];
    int have_count = cn < 4096 ? cn : 4096;
    for (int i = 0; i < have_count; i++)
        snprintf(have[i], 52, "%.50s", win_str(base + (uint32_t)i * RECORD_SIZE + NAME_AT));
    char names[MAX_NEW][52];
    int k = wanted((const char (*)[52])have, have_count, names);
    if (!k) return;
    uint32_t size[1] = {(uint32_t)(count + k) * RECORD_SIZE};
    uint32_t block = call_cdecl(0x944e46u, 1, size);
    if (rt_unwinding || !block) return;
    memcpy(g_mem + block, g_mem + base, (size_t)cn * RECORD_SIZE);
    for (int j = 0; j < k; j++) {
        uint32_t r = block + (uint32_t)(cn + j) * RECORD_SIZE;
        const SeasonCompDef *d = def_of_record(names[j]);
        uint32_t like = 0;
        for (int i = 0; d && i < have_count && !like; i++)
            if (!strcmp(have[i], d->like)) like = base + (uint32_t)i * RECORD_SIZE;
        memset(g_mem + r, 0, RECORD_SIZE);
        if (like) memcpy(g_mem + r + 0x59, g_mem + like + 0x59, 0x10);   /* continent, nation, two colours: ids */
        else for (uint32_t f = 0x59; f <= 0x65; f += 4) wr32(g_mem, r + f, (uint32_t)-2);
        wr32(g_mem, r, (uint32_t)(cn + j));                             /* id = index (doc 1.5) */
        put_name(r + NAME_AT, names[j], 0x34);
        put_name(r + 0x38, d && d->short_name ? d->short_name : names[j], 0x1b);
        /* Slice 20: level -1 copies the level of `like` (a division of the nation takes the rank of the division
         * it sits beside: the club moves count the tier by it, season_clubs.c). */
        if (d && d->level < 0 && like) wr16(g_mem, r + 0x69, rd16(g_mem, like + 0x69));
        else wr16(g_mem, r + 0x69, (uint32_t)(d && d->level > 0 ? d->level : 1));
    }
    /* The friendly records again, blank, as the loader writes them (0x52a060..0x52a1ab). */
    for (int i = 0; i < FRIENDLY_RECORDS; i++) {
        uint32_t r = block + (uint32_t)(cn + k + i) * RECORD_SIZE;
        memset(g_mem + r, 0, RECORD_SIZE);
        wr32(g_mem, r, (uint32_t)(cn + k + i));
        for (uint32_t f = 0x59; f <= 0x65; f += 4) wr32(g_mem, r + f, 0xffffffffu);
    }
    if (new_block) {
        uint32_t b[1] = {new_block};
        call_cdecl(0x9452cau, 1, b);
    }
    new_block = block;
    wr32(g_mem, COMP_RECORDS, block);
    wr32(g_mem, NATION_RECORDS, block + (uint32_t)(cn - nations) * RECORD_SIZE);
    wr32(g_mem, COMP_RECORD_COUNT, (uint32_t)(count + k));
    wr32(g_mem, NATION_COUNT, (uint32_t)(nations + k));
    fprintf(stderr, "recomp: new competitions: %d new records from index %d\n", k, cn);
}

/* The added records in the data that runs: the last records of the nation block whose name is the record of a def
 * with `like` (at a load they come from the save). For the hiding wraps below. */
static int added_in_data(void)
{
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    int32_t cn = (int32_t)rd32(g_mem, COMP_RECORD_COUNT) - FRIENDLY_RECORDS;
    int k = 0;
    for (int32_t i = cn - 1; base && i >= 0 && k < MAX_NEW; i--) {
        const char *n = season_change_data_name(base + (uint32_t)i * RECORD_SIZE);
        const SeasonCompDef *d = n ? def_of_record(n) : NULL;
        if (!d || !d->like) break;
        k++;
    }
    return k;
}

int season_comps_records_stale(void)
{
    if (!rd32(g_mem, COMP_RECORDS)) return 0;
    uint32_t base = rd32(g_mem, COMP_RECORDS);
    int now = added_in_data();
    int32_t all = (int32_t)rd32(g_mem, COMP_RECORD_COUNT) - FRIENDLY_RECORDS, cn = all - now;
    if (cn <= 0) return 0;
    static char have[4096][52];
    int have_count = cn < 4096 ? cn : 4096;
    for (int i = 0; i < have_count; i++)
        snprintf(have[i], 52, "%.50s", win_str(base + (uint32_t)i * RECORD_SIZE + NAME_AT));
    char names[MAX_NEW][52];
    int k = wanted((const char (*)[52])have, have_count, names);
    if (k != now) return 1;
    for (int j = 0; j < k; j++)
        if (strcmp(names[j], win_str(base + (uint32_t)(cn + j) * RECORD_SIZE + NAME_AT))) return 1;
    return 0;
}

void o_00528540(void);
void o_005315a0(void);
void o_00528080(void);
void o_00601a40(void);
void o_005339a0(void);
void o_00544fc0(void);

/* database_load, cdecl (path, mode, stream): stream 0 is the files of a new game (0x524d50), 1 a save (0x524d10). */
RT_WRAP(00528540, database_load)
{
    int was = loading_files, files = rt_arg(2) == 0;
    loading_files = files;
    grown = 0;
    o_00528540();
    loading_files = was;
    /* The .lng files that the load copied, in the rows of the new game's data (season_lng.c). The names are still
     * English here: the language names read the files after the load (Config::apply_and_save 0x5d8480). */
    if (!rt_unwinding && files && cpu.eax) season_lng_fit_temp();
}

/* The id pass of the records (called at 0x52d1aa in the load, and by the writer at each save). */
RT_WRAP(005315a0, database_id_pass)
{
    if (loading_files && !grown) {
        grown = 1;
        Cpu saved = cpu;
        grow();
        if (rt_unwinding) return;
        cpu = saved;
    }
    o_005315a0();
}

/* database_free_all_tables: the module's copy of the record array goes too. */
RT_WRAP(00528080, database_free_all_tables)
{
    o_00528080();
    if (rt_unwinding) return;
    if (new_block) {
        uint32_t eax = cpu.eax, b[1] = {new_block};
        new_block = 0;
        call_cdecl(0x9452cau, 1, b);
        if (rt_unwinding) return;
        cpu.eax = eax;
    }
}

int season_comps_added_records(void) { return added_in_data(); }

/* Three functions take [0xae2378] - 0x7f as C + N: the key id matcher loop 0x604241 and the language checks
 * 0x533e94 and 0x5451c1. They run with the added records hidden, as the original sees the data (doc 2.1, 2.4). */
static void hidden(void (*fn)(void))
{
    int k = added_in_data();
    if (k) wr32(g_mem, COMP_RECORD_COUNT, rd32(g_mem, COMP_RECORD_COUNT) - (uint32_t)k);
    fn();
    if (k) wr32(g_mem, COMP_RECORD_COUNT, rd32(g_mem, COMP_RECORD_COUNT) + (uint32_t)k);
}


/* The language names call themselves first with language 0 (0x5339f6 and the next cases of the jump table at
 * 0x5339ec). When the names are in another language that inner call reads eng.lng and checks the counts too, so
 * only the outer call hides the added records: a hide in both took them off twice. */
static int lng_depth;

static void hidden_once(void (*fn)(void))
{
    if (lng_depth++) fn();
    else hidden(fn);
    lng_depth = rt_unwinding ? 0 : lng_depth - 1;
}

RT_WRAP(00601a40, set_key_ids) { hidden_once(o_00601a40); }
RT_WRAP(005339a0, database_load_language_names) { hidden_once(o_005339a0); }
RT_WRAP(00544fc0, eng_lng_load_tables) { hidden_once(o_00544fc0); }
