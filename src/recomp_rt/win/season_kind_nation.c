/* Rule kind "nation" of the season change (season_change.h; 0.6.0 slice 17): the transfer windows, the foreign
 * player limits and the work permit rule of a nation, from the structured table that tools/seasons/encode_rules.py
 * makes (season_nation.h). Evidence and the texts the encoder cannot read: docs/research/2026-10-17-season-rules-17.md.
 *
 * The game's objects (docs/research/2026-10-16-sweep-rulebooks.md section 3, sweep-season-end.md section 4):
 * - TRANSFER_MANAGER [0xde1f54] holds 28 rules objects at +0x8ac (setup_transfer_rules 0x8d2e27..0x8d2f78): the
 *   European one first, then one for each nation. A rules object (TRANSFER_RULES::TRANSFER_RULES 0x8e84a0):
 *   +4 the window records (12 bytes each), +8 their count (byte), +9 the rules id, +0xa the nation record, +0x12
 *   the scope (1 nation), +0x13 the cap of foreign players a club may hold, +0x14 the cap of non-EU players
 *   (can_sign_player 0x8e8560 refuses a signing at the cap; 0xff turns the check off, 0x8e8614).
 * - A window record is a start bound and an end bound of 6 bytes: rules id, window index, weekday (0xff none),
 *   day, month (0 to 11), 1 for the start bound and 0 for the end bound (England's constructor 0x579c03..0x579c2f).
 * - The manager keeps a sorted table of pointers to every bound (+0x8a4, count +0x8a8; qsort 0x945333 with the
 *   comparator 0x8e5590) for the deadline news (update_transfer_window_news 0x8c6bc0). The kind makes it again
 *   as setup_transfer_rules does, after it changes a window.
 * - The match squad limits are in a 42-byte record that the lookup 0x8cf6b0 (thiscall: fixture, record, flag)
 *   fills through the nation's rules object: bytes +0..+2 foreign, +3..+5 non-EU, 0xff for none
 *   (2026-10-16-foreign-and-tactics-positions.md). The kind runs after the lookup and gives the slot that is set
 *   the number of the table, only for a fixture of a first division. It adds no limit that the game does not
 *   have: a limit in a slot that is not set stays as the game has it.
 * - Work permits: TRANSFER_MANAGER::player_needs_work_permit 0x8c7560 (thiscall: player, club, flag; ret 0xc)
 *   asks 0x53d740 (player, 0) whether the player is of the EU and stops when he is. With the flag set it returns 1
 *   for every other player at once (0x8c760d); with the flag clear it looks for a permit in the list (0x8c7650).
 *   Inference: the flag marks a new signing, the other calls ask about a player who is already at the club.
 *   After Brexit (Q249 keeps the real rules, R10) the kind answers 1 for a player of the EU who is not a citizen
 *   of the UK or Ireland, when the flag is set and the club is of a nation with the rule, from 1 January 2021.
 *   The players already in a squad are not asked again with the flag, so they stay, as the real transition did.
 *
 * It acts only in a season game (start year not 2001, or changing rules on); the base game and
 * RECOMP_SEASON_CODE=off keep the original's rules (Q342). The rules are those of the start year with changing
 * rules off, else of the last season point. RECOMP_SEASON_NATION_LOG=1 prints each change. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_nation.h"
#include "season_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MANAGER 0xde1f54u               /* TRANSFER_MANAGER * */
#define RULES_AT 0x8acu                 /* the 28 rules objects */
#define RULES_COUNT 28
#define EDGES_AT 0x8a4u                 /* sorted pointers to the window bounds */
#define EDGES_COUNT 0x8a8u
#define R_WINDOWS 4u
#define R_COUNT 8u                      /* byte */
#define R_ID 9u
#define R_NATION 0xau
#define R_SCOPE 0x12u
#define R_CAP_FOREIGN 0x13u
#define R_CAP_NON_EU 0x14u
#define WINDOW_SIZE 12u
#define GAME_DATE 0xae2c90u
#define COMP_TABLE 0xadadfcu
#define COMP_SLOTS 0xadadf8u
#define MALLOC 0x00944e46u              /* cdecl (size): the allocation of setup_transfer_rules, 0x8d2e62 */
#define QSORT 0x00945333u               /* cdecl (base, count, size, compare), 0x8d2f73 */
#define WINDOW_COMPARE 0x008e5590u
#define IS_EU 0x0053d740u               /* cdecl (player, club nation or 0) -> 1 when of the EU */
#define NEEDS_PERMIT 0x008c7560u
#define BREXIT_YEAR 2021

static int readable(uint32_t p) { return rt_readable(p); }

static const char *const uk_names[] = {"England", "Scotland", "Wales", "Northern Ireland"};

static int log_on(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SEASON_NATION_LOG") && *getenv("RECOMP_SEASON_NATION_LOG");
    return on;
}

static int active(void)
{
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

/* The season whose rules hold now. */
static int current_season(void)
{
    if (!season_changing_rules()) return season_start_year();
    int p = season_change_point();
    return p > season_start_year() ? p : season_start_year();
}

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* ---- The rules objects ---- */

static uint32_t manager(void)
{
    uint32_t m = rd32(g_mem, MANAGER);
    return readable(m) ? m : 0;
}

/* The rules object of a nation (scope 1), or 0. The nation's record id is in the global of the table. */
static uint32_t rules_of(const SeasonNation *n)
{
    uint32_t m = manager();
    int32_t id = (int32_t)rd32(g_mem, n->key_global);
    if (!m || id < 0) return 0;
    for (int i = 0; i < RULES_COUNT; i++) {
        uint32_t r = rd32(g_mem, m + RULES_AT + 4u * (uint32_t)i);
        if (!readable(r) || rd8(g_mem, r + R_SCOPE) != 1) continue;
        uint32_t nat = rd32(g_mem, r + R_NATION);
        if (readable(nat) && (int32_t)rd32(g_mem, nat) == id) return r;
    }
    return 0;
}

/* ---- The windows ---- */

static void put_bound(uint8_t *p, int id, int index, int day, int month, int start)
{
    p[0] = (uint8_t)id;
    p[1] = (uint8_t)index;
    p[2] = 0xff;                                  /* no weekday */
    p[3] = (uint8_t)day;
    p[4] = (uint8_t)(month - 1);
    p[5] = (uint8_t)start;
}

/* The records the nation should have: regular windows from the table, and for a nation with a partial flag the
 * window of index 2 that the game keeps (Italy: free transfers only, 0x661520). Returns the count. */
static int build_records(uint32_t rules, const SeasonNationState *st, int partial, uint8_t *out, int room)
{
    int count = 0;
    uint32_t old = rd32(g_mem, rules + R_WINDOWS);
    int old_count = (int8_t)rd8(g_mem, rules + R_COUNT);
    /* Byte 0 of a bound is the index of the rules object in the manager (update_transfer_window_news 0x8c6d17,
     * 0x8c6d1c: [manager + byte * 4 + 0x8ac]), also the rules id +9 of the object. */
    int id = old_count > 0 && readable(old) ? (int)rd8(g_mem, old) : (int)rd8(g_mem, rules + R_ID);
    const uint8_t *kept = NULL;
    for (int i = 0; partial && readable(old) && i < old_count; i++)
        if (rd8(g_mem, old + 12u * (uint32_t)i + 1) == 2) kept = g_mem + old + 12u * (uint32_t)i;
    for (int w = 0; w < st->windows && count < room; w++) {
        if (kept && count == 2) {                 /* index 2 belongs to the kept window */
            memcpy(out + 12 * count, kept, 12);
            count++;
            if (count >= room) break;
        }
        int index = kept && w >= 2 ? w + 1 : w;
        put_bound(out + 12 * count, id, index, st->win[w].sd, st->win[w].sm, 1);
        put_bound(out + 12 * count + 6, id, index, st->win[w].ed, st->win[w].em, 0);
        count++;
    }
    if (kept && count <= 2 && count < room) {     /* fewer than two regular windows: the kept one follows */
        memcpy(out + 12 * count, kept, 12);
        count++;
    }
    return count;
}

/* The manager's sorted table of window bounds, made again as setup_transfer_rules does (0x8d2e27..0x8d2f78). */
static int rebuild_edges(uint32_t m)
{
    uint32_t total = 0;
    for (int i = 0; i < RULES_COUNT; i++) {
        uint32_t r = rd32(g_mem, m + RULES_AT + 4u * (uint32_t)i);
        if (readable(r) && (int8_t)rd8(g_mem, r + R_COUNT) > 0) total += 2u * (uint32_t)(int8_t)rd8(g_mem, r + R_COUNT);
    }
    uint32_t size[1] = {total * 4u ? total * 4u : 4u};
    uint32_t table = call_cdecl(MALLOC, 1, size);
    if (rt_unwinding || !readable(table)) return 0;
    uint32_t j = 0;
    for (int i = 0; i < RULES_COUNT; i++) {
        uint32_t r = rd32(g_mem, m + RULES_AT + 4u * (uint32_t)i);
        int n = readable(r) ? (int8_t)rd8(g_mem, r + R_COUNT) : 0;
        uint32_t arr = readable(r) ? rd32(g_mem, r + R_WINDOWS) : 0;
        for (int k = 0; k < n && readable(arr); k++) {
            wr32(g_mem, table + 4u * j++, arr + WINDOW_SIZE * (uint32_t)k);
            wr32(g_mem, table + 4u * j++, arr + WINDOW_SIZE * (uint32_t)k + 6u);
        }
    }
    wr32(g_mem, m + EDGES_AT, table);
    wr32(g_mem, m + EDGES_COUNT, j);
    uint32_t args[4] = {table, j, 4, WINDOW_COMPARE};
    call_cdecl(QSORT, 4, args);
    return !rt_unwinding;
}

/* Gives the nation's rules object the windows of the state. Returns 1 when it changed them. */
static int apply_windows(const SeasonNation *n, uint32_t rules, const SeasonNationState *st)
{
    if (st->windows == SEASON_NATION_KEEP) return 0;
    uint8_t want[WINDOW_SIZE * (SEASON_NATION_MAX_WINDOWS + 1)];
    int count = 0, partial = (n->flags & SEASON_NATION_PARTIAL_WINDOWS) != 0;
    if (st->windows != SEASON_NATION_WINDOWS_NONE) count = build_records(rules, st, partial, want, SEASON_NATION_MAX_WINDOWS + 1);
    else if (partial) return 0;                   /* the nation keeps a window: no table row gives none for it */
    uint32_t old = rd32(g_mem, rules + R_WINDOWS);
    int old_count = (int8_t)rd8(g_mem, rules + R_COUNT);
    if (old_count == count && (count == 0 || (readable(old) && !memcmp(g_mem + old, want, WINDOW_SIZE * (size_t)count))))
        return 0;
    /* The old window array and the old sorted table of bounds are not freed: the old table points into the old
     * arrays, the manager's destructor is not known to free them from these pointers, and a few hundred bytes a
     * season are left. */
    if (count > 0) {
        uint32_t size[1] = {WINDOW_SIZE * (uint32_t)count};
        uint32_t arr = call_cdecl(MALLOC, 1, size);
        if (rt_unwinding || !readable(arr)) return 0;
        memcpy(g_mem + arr, want, WINDOW_SIZE * (size_t)count);
        wr32(g_mem, rules + R_WINDOWS, arr);
    }
    wr8(g_mem, rules + R_COUNT, (uint32_t)count);
    if (log_on()) {
        fprintf(stderr, "recomp: season nation: %s windows %d -> %d:", n->name, old_count, count);
        uint32_t arr = rd32(g_mem, rules + R_WINDOWS);
        for (int w = 0; w < count && readable(arr); w++)
            fprintf(stderr, " %d.%d-%d.%d(#%d)", rd8(g_mem, arr + 12u * (uint32_t)w + 3), rd8(g_mem, arr + 12u * (uint32_t)w + 4) + 1,
                    rd8(g_mem, arr + 12u * (uint32_t)w + 9), rd8(g_mem, arr + 12u * (uint32_t)w + 10) + 1,
                    rd8(g_mem, arr + 12u * (uint32_t)w + 1));
        fputc('\n', stderr);
    }
    return 1;
}

static void apply_caps(const SeasonNation *n, uint32_t rules, const SeasonNationState *st)
{
    if (st->sign_foreign != SEASON_NATION_KEEP && rd8(g_mem, rules + R_CAP_FOREIGN) != (uint32_t)st->sign_foreign) {
        if (log_on())
            fprintf(stderr, "recomp: season nation: %s foreign cap %u -> %d\n", n->name, rd8(g_mem, rules + R_CAP_FOREIGN),
                    st->sign_foreign);
        wr8(g_mem, rules + R_CAP_FOREIGN, (uint32_t)st->sign_foreign);
    }
    if (st->sign_non_eu != SEASON_NATION_KEEP && rd8(g_mem, rules + R_CAP_NON_EU) != (uint32_t)st->sign_non_eu) {
        if (log_on())
            fprintf(stderr, "recomp: season nation: %s non-EU cap %u -> %d\n", n->name, rd8(g_mem, rules + R_CAP_NON_EU),
                    st->sign_non_eu);
        wr8(g_mem, rules + R_CAP_NON_EU, (uint32_t)st->sign_non_eu);
    }
}

/* The rules of every nation for a season. */
static void apply_all(int season)
{
    uint32_t m = manager();
    if (!m) {
        if (log_on()) fprintf(stderr, "recomp: season nation: no transfer manager yet\n");
        return;
    }
    int changed = 0;
    for (int i = 0; i < season_nation_count && !rt_unwinding; i++) {
        const SeasonNation *n = &season_nations[i];
        uint32_t rules = rules_of(n);
        SeasonNationState st;
        if (!rules || !season_nation_state(n->name, season, &st)) continue;
        apply_caps(n, rules, &st);
        changed |= apply_windows(n, rules, &st);
    }
    if (changed && !rt_unwinding) rebuild_edges(m);
}

/* ---- The news ---- */

static const char *const month_names[] = {"January", "February", "March", "April", "May", "June", "July", "August",
                                          "September", "October", "November", "December"};

static void windows_text(const SeasonNationState *st, char *out, size_t room)
{
    out[0] = 0;
    for (int w = 0; w < st->windows && w < SEASON_NATION_MAX_WINDOWS; w++) {
        char one[96];
        snprintf(one, sizeof one, "%s%d %s to %d %s", w == 0 ? "" : (w == st->windows - 1 ? " and " : ", "), st->win[w].sd,
                 month_names[st->win[w].sm - 1], st->win[w].ed, month_names[st->win[w].em - 1]);
        strncat(out, one, room - strlen(out) - 1);
    }
}

static int windows_same(const SeasonNationState *a, const SeasonNationState *b)
{
    if (a->windows != b->windows) return 0;
    for (int w = 0; w < a->windows && w < SEASON_NATION_MAX_WINDOWS; w++)
        if (memcmp(&a->win[w], &b->win[w], sizeof a->win[w])) return 0;
    return 1;
}

static void limit_text(int v, char *out, size_t room, const char *kind)
{
    if (v == SEASON_NATION_NONE) snprintf(out, room, "no limit on %s players", kind);
    else snprintf(out, room, "at most %d %s players", v, kind);
}

/* The nations that have a competition in the game: only their changes are news. */
static int nation_in_game(const char *nation, int season)
{
    uint32_t comps[512];
    int n = season_change_comps(comps, 512);
    for (int i = 0; i < n; i++) {
        const char *file = season_change_file(comps[i]);
        const SeasonRule *r = strcmp(file, "?") ? season_rules_find(season, file, 0) : NULL;
        if (r && !strcmp(r->nation, nation)) return 1;
    }
    return 0;
}

static void news_for(const SeasonNation *n, int season)
{
    SeasonNationState was, now;
    season_nation_state(n->name, season - 1, &was);
    if (!season_nation_state(n->name, season, &now) || !nation_in_game(n->name, season)) return;
    char a[160], b[160];
    if (!windows_same(&was, &now) && now.windows != SEASON_NATION_KEEP) {
        if (now.windows == SEASON_NATION_WINDOWS_NONE) {
            season_change_news("%s: clubs can sign players at any time.", n->name);
        } else {
            windows_text(&now, a, sizeof a);
            season_change_news("%s: the transfer windows are now %s.", n->name, a);
        }
    }
    if (was.sign_non_eu != now.sign_non_eu && now.sign_non_eu != SEASON_NATION_KEEP) {
        limit_text(now.sign_non_eu, a, sizeof a, "non-EU");
        season_change_news("%s: a club may now hold %s.", n->name, a);
    }
    if (was.sign_foreign != now.sign_foreign && now.sign_foreign != SEASON_NATION_KEEP) {
        limit_text(now.sign_foreign, a, sizeof a, "foreign");
        season_change_news("%s: a club may now hold %s.", n->name, a);
    }
    if (was.squad_non_eu != now.squad_non_eu && now.squad_non_eu != SEASON_NATION_KEEP) {
        limit_text(now.squad_non_eu, a, sizeof a, "non-EU");
        season_change_news("%s: a match squad in the top division may now hold %s.", n->name, a);
    }
    if (was.squad_foreign != now.squad_foreign && now.squad_foreign != SEASON_NATION_KEEP) {
        limit_text(now.squad_foreign, b, sizeof b, "foreign");
        season_change_news("%s: a match squad in the top division may now hold %s.", n->name, b);
    }
    if (!was.brexit && now.brexit)
        season_change_news("%s: from 1 January 2021 players from the EU need a work permit to sign for a club.", n->name);
}

/* ---- The hooks ---- */

static void nation_new_game(int start_year)
{
    if (log_on()) fprintf(stderr, "recomp: season nation: new game %d, rules %s\n", start_year, active() ? "on" : "off");
    if (log_on() && manager())
        for (int i = 0; i < 5; i++) {
            const char *nm = i < 4 ? uk_names[i] : "Republic of Ireland";
            const SeasonNation *n = season_nation_find(nm);
            uint32_t r = n ? rules_of(n) : 0, nat = r ? rd32(g_mem, r + R_NATION) : 0;
            fprintf(stderr, "recomp: season nation: %s permit group byte %d\n", nm,
                    readable(nat) ? (int)rd8(g_mem, nat + 0x76u) : -1);
        }
    if (active()) apply_all(start_year);
}

static void nation_loaded(int start_year)
{
    (void)start_year;
    if (active()) apply_all(current_season());
}

static void nation_season(int season)
{
    if (!active()) return;
    apply_all(season);
    for (int i = 0; i < season_nation_count && !rt_unwinding; i++) news_for(&season_nations[i], season);
}

/* ---- The match squad limits ---- */

typedef struct {
    const char *file;
    int season;
    const SeasonNation *nation;     /* NULL: the file is a competition of no nation of the table in that season */
    int top;                        /* 1: a first division */
} FileNation;
static FileNation file_cache[256];
static int file_cached;

/* The nation of the competition class file in the season (the first table row of the file), and whether it is
 * the nation's first division. A limit with a number acts in the first division only; "no limit" acts in every
 * competition of the nation, as the English filler 0x579c50 sets the limit of 3 for all of them. */
static const SeasonNation *file_nation(const char *file, int season, int *top)
{
    for (int i = 0; i < file_cached; i++)
        if (file_cache[i].file == file && file_cache[i].season == season) {
            *top = file_cache[i].top;
            return file_cache[i].nation;
        }
    const SeasonRule *r = season_rules_find(season, file, 0);
    const SeasonNation *n = r ? season_nation_find(r->nation) : NULL;
    int t = r && r->tier == 1;
    if (file_cached < (int)(sizeof file_cache / sizeof file_cache[0])) file_cache[file_cached++] = (FileNation){file, season, n, t};
    *top = t;
    return n;
}

static void set_slots(uint32_t rec, int first, int v)
{
    if (v == SEASON_NATION_KEEP) return;
    for (int s = first; s < first + 3; s++)
        if (rd8(g_mem, rec + (uint32_t)s) != 0xff) wr8(g_mem, rec + (uint32_t)s, (uint32_t)v);
}

/* TRANSFER_MANAGER restriction lookup 0x8cf6b0, thiscall (fixture, record, collapse flag), ret 0xc. */
void o_008cf6b0(void);
RT_WRAP(008cf6b0, restriction_lookup)
{
    uint32_t fixture = rd32(g_mem, cpu.esp + 4), rec = rd32(g_mem, cpu.esp + 8);
    o_008cf6b0();
    if (rt_unwinding || !active() || !readable(fixture) || !readable(rec)) return;
    uint32_t comp_rec = rd32(g_mem, fixture + 0x14);
    if (!readable(comp_rec)) return;
    int32_t id = (int32_t)rd32(g_mem, comp_rec);
    uint32_t table = rd32(g_mem, COMP_TABLE);
    if (id < 0 || id >= (int32_t)rd32(g_mem, COMP_SLOTS) || !readable(table)) return;
    uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)id);
    if (!readable(obj)) return;
    const char *file = season_change_file(obj);
    int season = current_season();
    int top = 0;
    const SeasonNation *n = strcmp(file, "?") ? file_nation(file, season, &top) : NULL;
    SeasonNationState st;
    if (!n || !season_nation_state(n->name, season, &st)) return;
    uint8_t before[6];
    memcpy(before, g_mem + rec, 6);
    if (top || st.squad_non_eu == SEASON_NATION_NONE) set_slots(rec, 3, st.squad_non_eu);
    if (top || st.squad_foreign == SEASON_NATION_NONE) set_slots(rec, 0, st.squad_foreign);
    if (log_on() && memcmp(before, g_mem + rec, 6)) {
        static unsigned shown;
        if (shown++ < 12)
            fprintf(stderr, "recomp: season nation: %s %s %d squad record %02x %02x %02x %02x %02x %02x -> %02x %02x %02x %02x %02x %02x\n",
                    n->name, file, season, before[0], before[1], before[2], before[3], before[4], before[5], g_mem[rec],
                    g_mem[rec + 1], g_mem[rec + 2], g_mem[rec + 3], g_mem[rec + 4], g_mem[rec + 5]);
    }
}

/* ---- Work permits after Brexit ---- */

static int32_t nation_id_of(const char *name)
{
    const SeasonNation *n = season_nation_find(name);
    return n ? (int32_t)rd32(g_mem, n->key_global) : -2;
}

/* The nation of the club has the work permit group of the game: player_needs_work_permit answers 0 at once for
 * a club whose nation byte +0x76 is not 14 (0x8c75ed), so no permit exists there. */
#define NATION_GROUP 0x76u
#define PERMIT_GROUP 14

/* The club's nation is one of the four UK nations and holds the permit group: the name, else NULL. */
static const char *uk_permit_nation(uint32_t club)
{
    if (!readable(club)) return NULL;
    uint32_t nat = rd32(g_mem, club + 0x53);
    if (!readable(nat) || rd8(g_mem, nat + NATION_GROUP) != PERMIT_GROUP) return NULL;
    int32_t id = (int32_t)rd32(g_mem, nat);
    for (int i = 0; i < 4; i++)
        if (id == nation_id_of(uk_names[i])) return uk_names[i];
    return NULL;
}

/* The year from which the Brexit rule holds, from 1 January: 2021. The test variable RECOMP_SEASON_BREXIT_YEAR
 * moves it, so a control run has signings before the date (season_rules17_run.py). */
static int brexit_year(void)
{
    static int year;
    if (!year) {
        const char *v = getenv("RECOMP_SEASON_BREXIT_YEAR");
        year = v && atoi(v) >= 2001 ? atoi(v) : BREXIT_YEAR;
    }
    return year;
}


/* The table name of a player's nationality (k 0: +0x1a, k 1: the second one at +0x1e), or NULL. */
static const char *nationality_of(uint32_t player, int k)
{
    uint32_t nat = rd32(g_mem, player + (k ? 0x1eu : 0x1au));
    if (!readable(nat)) return NULL;
    int32_t id = (int32_t)rd32(g_mem, nat);
    for (int i = 0; i < season_nation_count; i++)
        if (id == (int32_t)rd32(g_mem, season_nations[i].key_global)) return season_nations[i].name;
    return NULL;
}

/* The callers of player_needs_work_permit 0x8c7560 (call site, flag; return address = site + 5), read from the
 * .text of 3.9.68. The rule acts where a signing is decided:
 *   offer checks, flag 0 (the player has no permit in the list): transfer_manager_offer_needs_work_permit
 *     0x8c7470 at 0x8c74a9 and 0x8c7539, TRANSFER_MANAGER::check_offer_work_permits 0x8c7690 at 0x8c7704 and
 *     0x8c7872. The first is called from 0x8c31a4, the second from 0x8c6951 and 0x8cbcb3.
 *   complete_accepted_contract_offer 0x8c38b0 at 0x8c3902, flag 1: when it answers 1 the manager asks
 *     is_work_permit_granted 0x8c7950 (0x8c391b), the game's own permit decision, and a refusal ends the signing.
 * Left alone: update_work_permit_on_move 0x8c7e20 (0x8c7e5d, flag 1: the permit is stamped when a player moves);
 * the AI scout and shortlist loops (0x7b4450, 0x7b495d, 0x7b4f43, 0x7b5095, 0x7b6e99, 0x83fddd, 0x843249,
 * 0x843a4d, flag 1); the screens (0x45ccd4, 0x45eb85, 0x484bec, 0x87d9f2, 0x88939a); the contract manager
 * (0x4ceb41, 0x4d24bb); and match selection scoring (0x6dcb32, 0x6e5d60, 0x6e5ebc, 0x6e834c, 0x6e8456,
 * 0x72639f), which must not drop players who are already at a club. */
extern const uint32_t rt_site_permit_offer_a, rt_site_permit_offer_b, rt_site_permit_check_a, rt_site_permit_check_b,
    rt_site_permit_complete, rt_site_permit_decide;      /* tools/recomp/callsites.py: the build checks each call */

static int signing_return(uint32_t ret)
{
    return ret == rt_site_permit_offer_a || ret == rt_site_permit_offer_b || ret == rt_site_permit_check_a ||
           ret == rt_site_permit_check_b || ret == rt_site_permit_complete;
}

static unsigned permit_signings[2];          /* log lines so far: the rule off, the rule on */

/* TRANSFER_MANAGER::player_needs_work_permit 0x8c7560, thiscall (player, club, flag), ret 0xc. */
void o_008c7560(void);
RT_WRAP(008c7560, player_needs_work_permit)
{
    uint32_t ret = rd32(g_mem, cpu.esp), player = rd32(g_mem, cpu.esp + 4), club = rd32(g_mem, cpu.esp + 8);
    const char *nation;
    if (active() && signing_return(ret) && readable(player) && (nation = uk_permit_nation(club)) != NULL) {
        uint32_t args[2] = {player, 0};
        Cpu saved = cpu;
        uint32_t eu = rt_callback(IS_EU, 2, args);
        if (!rt_unwinding) cpu = saved;
        if (rt_unwinding) return;
        if (eu) {
            int rule = season_brexit_blocks(nation, current_season(), (int16_t)rd16(g_mem, GAME_DATE + 2), brexit_year(), 1,
                                            nationality_of(player, 0), nationality_of(player, 1));
            if (!rule && (nationality_of(player, 0) && !strcmp(nationality_of(player, 0), "Republic of Ireland")) &&
                log_on())
                fprintf(stderr, "recomp: season nation: Brexit: an Irish player signs for a club of %s: goes through\n", nation);
            if (log_on() && permit_signings[rule != 0]++ < 8)
                fprintf(stderr, "recomp: season nation: Brexit: %s of an EU player by a club of %s, %s, rule %s\n",
                        ret == rt_site_permit_complete ? "completion" : "offer check", nation,
                        (int16_t)rd16(g_mem, GAME_DATE + 2) >= brexit_year() ? "from the date of the rule" : "before the date of the rule",
                        rule ? "on: sent to the permit decision" : "off: goes through");
            if (rule) {
                rt_ret_stdcall(1, 3);
                return;
            }
        }
    }
    o_008c7560();
}

/* TRANSFER_MANAGER::is_work_permit_granted 0x8c7950, thiscall (player, nation, record, 0, 0), ret 0x14; the third
 * argument is the record that 0x4cc9e0 gives. It answers 1 when the permit is granted: the completion goes on
 * (0x8c3920 jne). The wrap logs the answer at the completion of a signing. */
void o_008c7950(void);
RT_WRAP(008c7950, is_work_permit_granted)
{
    uint32_t ret = rd32(g_mem, cpu.esp);
    o_008c7950();
    if (!rt_unwinding && log_on() && ret == rt_site_permit_decide) {
        static unsigned shown[2];
        int after = (int16_t)rd16(g_mem, GAME_DATE + 2) >= brexit_year();
        if (shown[after]++ < 8)
            fprintf(stderr, "recomp: season nation: permit decision at a completion %s: answer %u (%s)\n",
                    after ? "from the date of the rule" : "before the date of the rule", cpu.eax,
                    cpu.eax ? "granted" : "refused");
    }
}

const SeasonRuleKind season_kind_nation = {"nation", nation_new_game, nation_loaded, nation_season, NULL, NULL, NULL};
