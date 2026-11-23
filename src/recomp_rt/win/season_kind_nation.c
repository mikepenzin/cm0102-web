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
 *   have: a limit in a slot that is not set stays as the game has it. A table rule of non-EU players with no
 *   foreign limit changes the kind of the game's foreign limit (Croatia from 2013/14, issue 135;
 *   season_nation_squad_record).
 * - Work permits: TRANSFER_MANAGER::player_needs_work_permit 0x8c7560 (thiscall: player, club, flag; ret 0xc)
 *   asks 0x53d740 (player, 0) whether the player is of the EU and stops when he is. With the flag set it returns 1
 *   for every other player at once (0x8c760d); with the flag clear it looks for a permit in the list (0x8c7650).
 *   Inference: the flag marks a new signing, the other calls ask about a player who is already at the club.
 *   After Brexit (Q249 keeps the real rules, R10) the kind answers 1 for a player of the EU who is not a citizen
 *   of the UK or Ireland, when the flag is set and the club is of a nation with the rule, from 1 January 2021.
 *   The players already in a squad are not asked again with the flag, so they stay, as the real transition did.
 * - Portugal keeps its own non-EU signing limit (6, 5, 4 by division) in PORTUGAL_RULES::can_sign_player 0x7d3ef0
 *   and can_make_offer 0x7d4040, not in +0x14. With sign_non_eu "none" (from 2006/07, issue 121) the kind lifts it
 *   for clubs of the Liga and the Liga de Honra (the section "Portugal's own non-EU limit" below).
 *
 * It acts only in a season game (start year not 2001, or changing rules on); the base game and
 * RECOMP_SEASON_CODE=off keep the original's rules (Q342). The rules are those of the start year with changing
 * rules off, else of the last season point. RECOMP_SEASON_NATION_LOG=1 prints each change. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_nation.h"
#include "season_rules.h"
#include "season_calendar.h"

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

/* The state of a nation in a season of the game: Ireland after its calendar move reads the row of its real season
 * (season_nation_row_season, open item 258). */
static int state_in(const char *nation, int season, SeasonNationState *out)
{
    return season_nation_state(nation, season_nation_row_season(nation, season, season_rules_moved("ire_prm")), out);
}

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
/* The rules of every nation for a season, of one nation (only), or of all but one ("!" and its name). */
static void apply_some(int season, const char *only)
{
    uint32_t m = manager();
    if (!m) {
        if (log_on()) fprintf(stderr, "recomp: season nation: no transfer manager yet\n");
        return;
    }
    int changed = 0;
    for (int i = 0; i < season_nation_count && !rt_unwinding; i++) {
        const SeasonNation *n = &season_nations[i];
        if (only && (only[0] == '!' ? !strcmp(n->name, only + 1) : strcmp(n->name, only) != 0)) continue;
        uint32_t rules = rules_of(n);
        SeasonNationState st;
        if (!rules || !state_in(n->name, season, &st)) continue;
        apply_caps(n, rules, &st);
        changed |= apply_windows(n, rules, &st);
    }
    if (changed && !rt_unwinding) rebuild_edges(m);
}

static void apply_all(int season) { apply_some(season, NULL); }

/* Ireland's season for its rules after a load: the edition of IrePrm when its February change came after the last
 * season point (open item 258), else 0. Only with changing rules on: off, the start season's rules stay (Q232). */
static int ireland_ahead(int season)
{
    int ie = season_calendar_edition();
    return season_changing_rules() && season_rules_moved("ire_prm") && ie > season ? ie : 0;
}

/* Open item 258: after its calendar move Ireland changes season on 20 February (season_calendar.h). Its rules of the
 * real season S then hold from that change, so the first window of S (2005: 7 January to 4 March) is open from it and
 * the July window follows; the season point in June gives the same row again. */
static void nation_entry(const SeasonChange *c)
{
    if (!active() || !season_changing_rules() || c->entry != 20 || !season_rules_moved("ire_prm") || c->season < 2003)
        return;
    apply_some(c->season, "Republic of Ireland");
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
        const SeasonRule *r = strcmp(file, "?") ? season_rules_row_of(file, season) : NULL;
        if (r && !strcmp(r->nation, nation)) return 1;
    }
    return 0;
}

static void news_for(const SeasonNation *n, int season)
{
    SeasonNationState was, now;
    state_in(n->name, season - 1, &was);
    if (!state_in(n->name, season, &now) || !nation_in_game(n->name, season)) return;
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
    int kind_change = now.squad_foreign == SEASON_NATION_NONE && now.squad_non_eu != SEASON_NATION_KEEP &&
                      now.squad_non_eu != SEASON_NATION_NONE;     /* season_nation_squad_record: Croatia, issue 135 */
    if (kind_change && was.squad_foreign != SEASON_NATION_NONE)
        season_change_news("%s: players of the EU and EEA no longer count as foreign players in a match squad.", n->name);
    else if (!kind_change && was.squad_foreign != now.squad_foreign && now.squad_foreign != SEASON_NATION_KEEP) {
        limit_text(now.squad_foreign, b, sizeof b, "foreign");
        season_change_news("%s: a match squad in the top division may now hold %s.", n->name, b);
    }
    if (!was.brexit && now.brexit)
        season_change_news("%s: from 1 January 2021 players from the EU need a work permit to sign for a club.", n->name);
}

/* ---- The hooks ---- */

static void portugal_log_counts(const char *when);
static void portugal_test_sign(void);

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
    if (active() && log_on()) portugal_log_counts("start");
    if (active() && getenv("RECOMP_TEST_PT_SIGN") && *getenv("RECOMP_TEST_PT_SIGN")) portugal_test_sign();
}

static void nation_loaded(int start_year)
{
    (void)start_year;
    if (!active()) return;
    /* A load between Ireland's February change and the season point: Ireland takes the rules of its new edition (open
     * item 258), the other nations those of the season. */
    int season = current_season(), ie = ireland_ahead(season);
    if (!ie) {
        apply_all(season);
        return;
    }
    apply_some(season, "!Republic of Ireland");
    apply_some(ie, "Republic of Ireland");
}

static void nation_season(int season)
{
    if (!active()) return;
    if (log_on()) {
        char when[24];
        snprintf(when, sizeof when, "season %d", season);
        portugal_log_counts(when);
    }
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

/* The nation of the competition class file in the season (the first table row of the file, after the table its
 * last row: the after-table rule, 0.6.2 fault 60), and whether it is the nation's first division. A limit with a
 * number acts in the first division only; "no limit" acts in every competition of the nation, as the English
 * filler 0x579c50 sets the limit of 3 for all of them. */
static const SeasonNation *file_nation(const char *file, int season, int *top)
{
    for (int i = 0; i < file_cached; i++)
        if (file_cache[i].file == file && file_cache[i].season == season) {
            *top = file_cache[i].top;
            return file_cache[i].nation;
        }
    const SeasonRule *r = season_rules_row_of(file, season);
    const SeasonNation *n = r ? season_nation_find(r->nation) : NULL;
    int t = r && r->tier == 1;
    if (file_cached < (int)(sizeof file_cache / sizeof file_cache[0])) file_cache[file_cached++] = (FileNation){file, season, n, t};
    *top = t;
    return n;
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
    if (!n || !state_in(n->name, season, &st)) return;
    uint8_t before[6];
    memcpy(before, g_mem + rec, 6);
    int changed = season_nation_squad_record(g_mem + rec, &st, top);
    if (log_on()) {
        /* By nation and by answer, so that the lines of one nation or of kept records do not hide the rest. */
        static unsigned shown[64][2];
        int k = (int)(n - season_nations);
        if (k >= 0 && k < 64 && shown[k][changed]++ < (changed ? 12u : 4u))
            fprintf(stderr, "recomp: season nation: %s %s %d %s squad record %02x %02x %02x %02x %02x %02x -> %02x %02x %02x %02x %02x %02x\n",
                    n->name, file, season, top ? "top" : "lower", before[0], before[1], before[2], before[3], before[4],
                    before[5], g_mem[rec], g_mem[rec + 1], g_mem[rec + 2], g_mem[rec + 3], g_mem[rec + 4], g_mem[rec + 5]);
    }
}

/* ---- Portugal's own non-EU limit at a signing (issue 121) ---- */

/* PORTUGAL_RULES::can_sign_player 0x7d3ef0 and PORTUGAL_RULES::can_make_offer 0x7d4040 keep their own non-EU
 * limit in a local, not in the rules object's cap +0x14: 6 when the club's division record (club +0x57) is the
 * competition of [0x9cf60c] (0x7d3f35, 0x7d41aa, 0x7d42ca), 5 for [0x9cf610] (0x7d3f4a), else 4. Both first ask
 * TRANSFER_RULES::check_window 0x8e8e50 (closed: code 9, 0x7d4027, 0x7d4384), then count the non-EU players of
 * the club's 50 squad slots and refuse with code 0x15 when the count passes the limit (0x7d4010, 0x7d436b).
 * Every other path ends in the base check, 0x8e8560 for a signing and 0x8e8800 for an offer, with the same
 * arguments; the offer path with no extra players and another club (0x7d4395) calls the base check alone.
 * From 2006/07 the Liga and the Liga de Honra have no limit (sign_non_eu "none"). The wraps call the original;
 * when it refuses a club of those two divisions and the window is open, its answer was the Portuguese limit or the
 * base check's own, so the answer under the rule is the base check's. The base check resets the offer's player
 * iterator (0x8e888d) and check_window writes only its out argument, so a second call changes no state. */
#define PT_LIGA 0x009cf60cu
#define PT_HONRA 0x009cf610u
#define CLUB_DIVISION 0x57u
#define CLUBS 0x00ae23b4u               /* club records, 0x245 bytes each (0x7d40a0) */
#define CLUB_SIZE 0x245u
#define CHECK_WINDOW 0x008e8e50u        /* thiscall (date, out or 0), ret 8: 1 when the date is in a window */
#define BASE_CAN_SIGN 0x008e8560u       /* thiscall (club, player, arg, date, out code), ret 0x14 */
#define BASE_CAN_OFFER 0x008e8800u      /* thiscall (offer, club, date, out code), ret 0x10 */

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* "liga", "honra", or NULL for a club of another division. */
static const char *portugal_division(uint32_t club)
{
    if (!readable(club)) return NULL;
    uint32_t div = rd32(g_mem, club + CLUB_DIVISION);
    if (!readable(div)) return NULL;
    uint32_t id = rd32(g_mem, div);
    return id == rd32(g_mem, PT_LIGA) ? "liga" : id == rd32(g_mem, PT_HONRA) ? "honra" : NULL;
}

/* 1 when the season rule lifts the Portuguese limit for the club: rules on, sign_non_eu "none", and the club in
 * the Liga or the Liga de Honra (the divisions of the 30 June 2006 decision). */
static int portugal_no_limit(uint32_t club)
{
    if (!active() || !portugal_division(club)) return 0;
    SeasonNationState st;
    return season_nation_state("Portugal", current_season(), &st) && st.sign_non_eu == SEASON_NATION_NONE;
}

/* The answer under the rule after a refusal: the base check's when the window is open, else the refusal. */
static void portugal_recheck(uint32_t self, uint32_t date, uint32_t base, int n, const uint32_t *args, uint32_t out,
                             int out_was, const char *what)
{
    uint32_t w[2] = {date, 0};
    if (!call_this(CHECK_WINDOW, self, 2, w) || rt_unwinding) return;
    if (out_was >= 0) wr16(g_mem, out, (uint16_t)out_was);
    uint32_t r = call_this(base, self, n, args);
    if (rt_unwinding) return;
    if (log_on()) {
        static unsigned shown[2];          /* by answer: refusals of the base check must not hide the rest */
        if (shown[r != 0]++ < 12)
            fprintf(stderr, "recomp: season nation: Portugal %s %d: no non-EU limit, base check answers %u\n", what,
                    current_season(), r);
    }
    cpu.eax = r;
}

/* Test log (RECOMP_SEASON_NATION_LOG) only: a refusal with the window open that the base check would let through
 * was the Portuguese limit's. The base check writes no global; it may stop part way through the offer's list of
 * extra players (0x8e88c7), and every reader of that list resets it first (0x7d40c4, 0x8e888d). The out code is put
 * back. */
static void portugal_log_kept(uint32_t self, uint32_t date, uint32_t base, int n, const uint32_t *args, uint32_t out,
                              const char *what)
{
    static unsigned shown;
    uint32_t w[2] = {date, 0};
    if (shown >= 12 || !call_this(CHECK_WINDOW, self, 2, w) || rt_unwinding) return;
    int code = readable(out) ? (int)rd16(g_mem, out) : -1;
    uint32_t saved_eax = cpu.eax, r = call_this(base, self, n, args);
    if (rt_unwinding) return;
    if (code >= 0) wr16(g_mem, out, (uint16_t)code);
    cpu.eax = saved_eax;
    if (r && shown++ < 12)
        fprintf(stderr, "recomp: season nation: Portugal %s %d: refused by the Portuguese non-EU limit\n", what,
                current_season());
}

/* Test log only: the non-EU players of each club of the Liga and the Liga de Honra, counted as 0x7d3f8b does
 * (the 50 squad slots +0xd7, a person of id below [0xae2364], foreign 0x53d980 and not of the EU 0x53d740). */
#define CLUB_COUNT 0x00ae235cu
#define PERSON_COUNT 0x00ae2364u
#define IS_FOREIGN 0x0053d980u
#define SQUAD 0xd7u                     /* 50 person pointers */
#define DATE_NEW 0x00549ef0u            /* Date::Date (day of the year, year), ret 8 */

/* 1 when the person counts against the Portuguese limit of the club, as 0x7d3f62..0x7d3f82 asks. */
static int portugal_non_eu(uint32_t club, uint32_t p)
{
    if (!readable(p) || (int32_t)rd32(g_mem, p) >= (int32_t)rd32(g_mem, PERSON_COUNT)) return 0;
    uint32_t f[3] = {p, rd32(g_mem, club + 0x53u), 1}, e[2] = {p, 0};
    uint32_t foreign = rt_callback(IS_FOREIGN, 3, f);
    if (rt_unwinding || !foreign) return 0;
    uint32_t eu = rt_callback(IS_EU, 2, e);
    return !rt_unwinding && !eu;
}

static int portugal_count(uint32_t club)
{
    int n = 0;
    for (int k = 0; k < 50 && !rt_unwinding; k++) n += portugal_non_eu(club, rd32(g_mem, club + SQUAD + 4u * (uint32_t)k));
    return n;
}

static void portugal_log_counts(const char *when)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int clubs = (int)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; i < clubs && readable(table) && !rt_unwinding; i++) {
        uint32_t club = table + CLUB_SIZE * (uint32_t)i;
        const char *div = portugal_division(club);
        if (!div) continue;
        Cpu saved = cpu;
        int n = portugal_count(club);
        if (rt_unwinding) return;
        cpu = saved;
        fprintf(stderr, "recomp: season nation: Portugal non-EU %s: %s %s %d\n", when, win_str(club + 4u), div, n);
    }
}

/* Test only (RECOMP_TEST_PT_SIGN=1, issue 121): the signing check of a 7th non-EU player. The Liga club with the
 * most non-EU players gets non-EU players of other clubs in its empty squad slots until it holds 6, the rules
 * object's can_sign_player (vtable +0x10, 0x7d3ef0) is asked about one more on day 200 (in the window of 1 June to
 * 1 September, 0x7d3df0), and the slots are put back. The original refuses with code 0x15; under a season rule of
 * no limit the base check answers. The game's state is the same after the test, except what the checks write. */
static void portugal_test_sign(void)
{
    const SeasonNation *pt = season_nation_find("Portugal");
    uint32_t rules = pt ? rules_of(pt) : 0, table = rd32(g_mem, CLUBS);
    int clubs = (int)rd32(g_mem, CLUB_COUNT);
    if (!readable(rules) || !readable(table)) {
        fprintf(stderr, "recomp: season nation: Portugal test: no rules object\n");
        return;
    }
    Cpu saved = cpu;
    uint32_t club = 0;
    int best = -1;
    for (int i = 0; i < clubs && !rt_unwinding; i++) {
        uint32_t c = table + CLUB_SIZE * (uint32_t)i;
        const char *div = portugal_division(c);
        if (!div || strcmp(div, "liga")) continue;
        int n = portugal_count(c);
        if (n > best) best = n, club = c;
    }
    if (rt_unwinding) return;
    uint32_t want[7], slots[50];
    int got = 0, need = best >= 6 ? 1 : 7 - best;      /* the players to put in, and the one to sign */
    for (int i = 0; i < clubs && got < need && !rt_unwinding; i++) {
        uint32_t c = table + CLUB_SIZE * (uint32_t)i;
        if (c == club) continue;
        for (int k = 0; k < 50 && got < need && !rt_unwinding; k++) {
            uint32_t p = rd32(g_mem, c + SQUAD + 4u * (uint32_t)k);
            if (portugal_non_eu(club, p)) want[got++] = p;
        }
    }
    if (rt_unwinding) return;
    memcpy(slots, g_mem + club + SQUAD, sizeof slots);
    int put = 0;
    for (int k = 0; k < 50 && put < need - 1; k++) {
        uint32_t p = rd32(g_mem, club + SQUAD + 4u * (uint32_t)k);
        if (!readable(p)) wr32(g_mem, club + SQUAD + 4u * (uint32_t)k, want[put++]);
    }
    int held = portugal_count(club);
    uint32_t esp = cpu.esp;
    cpu.esp -= 0x40u;
    uint32_t date = cpu.esp + 0x20u, out = cpu.esp + 0x30u;
    uint32_t d[2] = {200, (uint32_t)(int16_t)rd16(g_mem, GAME_DATE + 2)};
    call_this(DATE_NEW, date, 2, d);
    wr16(g_mem, out, 0xffffu);
    uint32_t fn = rd32(g_mem, rd32(g_mem, rules) + 0x10u);
    uint32_t a[5] = {club, want[need - 1], 0, date, out};
    uint32_t r = rt_unwinding || got < need ? 0 : call_this(fn, rules, 5, a);
    unsigned code = rd16(g_mem, out);
    cpu.esp = esp;
    memcpy(g_mem + club + SQUAD, slots, sizeof slots);
    if (rt_unwinding) return;
    cpu = saved;
    fprintf(stderr, "recomp: season nation: Portugal test %d: %s held %d non-EU (had %d, %d found), can_sign_player 0x%x "
            "for one more answers %u code 0x%x\n", current_season(), win_str(club + 4u), held, best, got, fn, r, code);
}

void o_007d3ef0(void);
RT_WRAP(007d3ef0, portugal_can_sign_player)
{
    uint32_t self = cpu.ecx, a[5];
    for (int i = 0; i < 5; i++) a[i] = rd32(g_mem, cpu.esp + 4u + 4u * (uint32_t)i);
    int out_was = readable(a[4]) ? (int)rd16(g_mem, a[4]) : -1;
    o_007d3ef0();
    if (rt_unwinding || cpu.eax) return;
    if (portugal_no_limit(a[0])) portugal_recheck(self, a[3], BASE_CAN_SIGN, 5, a, a[4], out_was, "signing");
    else if (log_on() && active() && portugal_division(a[0])) portugal_log_kept(self, a[3], BASE_CAN_SIGN, 5, a, a[4], "signing");
}

void o_007d4040(void);
RT_WRAP(007d4040, portugal_can_make_offer)
{
    uint32_t self = cpu.ecx, a[4];
    for (int i = 0; i < 4; i++) a[i] = rd32(g_mem, cpu.esp + 4u + 4u * (uint32_t)i);
    int out_was = readable(a[3]) ? (int)rd16(g_mem, a[3]) : -1;
    /* The club whose division the rule reads: the club argument. When it is the offer's club (offer +8, 0x7d4142)
     * the original takes that club's division too. When it is another club, the original counts the squad of the
     * player's own club (0x7d42ef) but takes the division of the offer's club (0x7d42b3); the rule lifts the limit
     * by the division of the club the check is about, so a lower division seller keeps its limit. */
    uint32_t club = a[1];
    o_007d4040();
    if (rt_unwinding || cpu.eax) return;
    if (portugal_no_limit(club)) portugal_recheck(self, a[2], BASE_CAN_OFFER, 4, a, a[3], out_was, "offer");
    else if (log_on() && active() && portugal_division(club)) portugal_log_kept(self, a[2], BASE_CAN_OFFER, 4, a, a[3], "offer");
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

const SeasonRuleKind season_kind_nation = {"nation", nation_new_game, nation_loaded, nation_season, nation_entry, NULL,
                                            NULL};
