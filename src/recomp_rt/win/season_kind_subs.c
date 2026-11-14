/* Rule kind "subs" of the season change (season_change.h; 0.6.0 slice 17): the substitutes used and the named
 * substitutes (the bench) of a competition, from the rules table (columns subs_used and bench, season_rules.h).
 *
 * How the original holds them (docs/research/2026-10-17-season-rules-17.md, section 1):
 * - A competition object keeps the named substitutes at +0x49 (Comp::Comp 0x48c5c9 sets 5) and the substitutes
 *   used at +0x4a (0x48c5cd sets 3). A fixture holds both in the word at +0x4d: the named substitutes in bits 0
 *   to 3, the substitutes used in bits 4 to 7. Comp::set_fixture_substitutes 0x48f2d0 (vtable +0x7c) packs them
 *   when a fixture is made (0x48f2fa..0x48f32d).
 * - Three classes override the slot and still pack the two fields: AusNsl 0x4124e0, FraCup and FraLgeCup 0x5bb100
 *   (a final round sets the named substitutes to 5, 0x5bb10c). GreCup 0x5ea3d0 does not read them: it writes
 *   0x77 (round 10) or 0x37 into the word. The kind changes the nibbles after it with the table's counts.
 * - The match engine and the team screens read the nibbles: the squad is 11 plus the low nibble (for example
 *   0x459aa8, 0x474e49, 0x6d0f8c, 0x6db9ad, 0x8cf8cf), and CM3_MATCH_ENGINE::init_match 0x6b7fe6..0x6b8015 sets a
 *   nibble above 9 to 9. So a count up to 9 works wherever the original reads them (the Greek cup already names 7).
 *
 * The kind writes +0x49 and +0x4a of the competition at the start of the four functions, so the original's own
 * packing, including its fixed rounds, does the rest. It acts only in a season game (start year not 2001, or
 * changing rules on, as slice 15 defines it); the base game and RECOMP_SEASON_CODE=off keep the original's
 * values (Q342). The season is the start year with changing rules off (Q232), else the season of the pass that
 * starts the competition (the before hook), else the season year of the competition (+0x40). A competition with
 * no row, or a row with no count, keeps what it has.
 *
 * The kind has no new_game, loaded or season hook: the fixtures of the first season are made inside the game's
 * creation, after the start year is set, and the fixtures of each later season in start_new_season (before hook
 * of this file), so the wraps see every fixture. RECOMP_SEASON_SUBS_LOG=1 prints each change. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_rules.h"
#include "season_subs_rule.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_YEAR 0x40u              /* s16: the season year of the competition */
#define COMP_NAMED 0x49u             /* byte: named substitutes */
#define COMP_USED 0x4au              /* byte: substitutes used */

static int pass_season;              /* the season of the last before hook, valid on the day it was set */
static int pass_day, pass_year;

static int readable(uint32_t p) { return rt_readable(p); }

static int active(void)
{
    return season_code_on() && (season_start_year() != SEASON_FIRST_YEAR || season_changing_rules());
}

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SEASON_SUBS_LOG") && *getenv("RECOMP_SEASON_SUBS_LOG");
    return on;
}

static int season_of(uint32_t comp)
{
    if (!season_changing_rules()) return season_start_year();
    /* start_new_season runs inside one day's pass, so the season of the before hook holds for that date only: a
     * competition that is skipped, or an unwind between hooks, cannot leave it set. */
    if (pass_season && (int16_t)rd16(g_mem, 0xae2c90) == pass_day && (int16_t)rd16(g_mem, 0xae2c92) == pass_year)
        return pass_season;
    int y = (int16_t)rd16(g_mem, comp + COMP_YEAR);
    return y >= SEASON_FIRST_YEAR && y <= SEASON_LAST_YEAR ? y : season_start_year();
}

/* A fixture is made for every match, so the lookup is cached by the class file and the season. The file
 * strings are the constants of the class table, so the pointer is the key. */
typedef struct {
    const char *file;
    int season, mask, used, bench;
} Cached;
static Cached cache[512];
static int cached;

static int lookup(const char *file, int season, int *used, int *bench)
{
    for (int i = 0; i < cached; i++)
        if (cache[i].file == file && cache[i].season == season) {
            *used = cache[i].used;
            *bench = cache[i].bench;
            return cache[i].mask;
        }
    int mask = season_subs_rule(season, file, used, bench);
    if (!(mask & SEASON_SUBS_USED)) *used = 0;
    if (!(mask & SEASON_SUBS_BENCH)) *bench = 0;
    if (cached < (int)(sizeof cache / sizeof cache[0]))
        cache[cached++] = (Cached){file, season, mask, *used, *bench};
    return mask;
}

/* The competition's own fields, before the original packs them into a fixture. */
static void apply(uint32_t comp)
{
    if (!active() || !readable(comp)) return;
    const char *file = season_change_file(comp);
    if (!strcmp(file, "?")) return;
    int season = season_of(comp), used, bench;
    int mask = lookup(file, season, &used, &bench);
    if ((mask & SEASON_SUBS_USED) && rd8(g_mem, comp + COMP_USED) != (uint32_t)used) {
        if (logging())
            fprintf(stderr, "recomp: season subs: %s %d/%02d used %u -> %d\n", file, season, (season + 1) % 100,
                    rd8(g_mem, comp + COMP_USED), used);
        wr8(g_mem, comp + COMP_USED, (uint32_t)used);
    }
    if ((mask & SEASON_SUBS_BENCH) && rd8(g_mem, comp + COMP_NAMED) != (uint32_t)bench) {
        if (logging())
            fprintf(stderr, "recomp: season subs: %s %d/%02d bench %u -> %d\n", file, season, (season + 1) % 100,
                    rd8(g_mem, comp + COMP_NAMED), bench);
        wr8(g_mem, comp + COMP_NAMED, (uint32_t)bench);
    }
}

/* Comp::set_fixture_substitutes 0x48f2d0, thiscall (fixture), ret 4. */
void o_0048f2d0(void);
RT_WRAP(0048f2d0, comp_set_fixture_subs)
{
    apply(cpu.ecx);
    o_0048f2d0();
}

/* AusNsl::set_fixture_substitutes 0x4124e0 (same arguments). */
void o_004124e0(void);
RT_WRAP(004124e0, aus_nsl_set_fixture_subs)
{
    apply(cpu.ecx);
    o_004124e0();
}

/* FraCup and FraLgeCup set_fixture_substitutes 0x5bb100. */
void o_005bb100(void);
RT_WRAP(005bb100, fra_cup_set_fixture_subs)
{
    apply(cpu.ecx);
    o_005bb100();
}

/* GreCup::set_fixture_substitutes 0x5ea3d0, thiscall (fixture), ret 4: the original writes the word (0x77 for
 * round 10, else 0x37) and ignores +0x49 and +0x4a, so the table's counts go into the nibbles afterwards. */
void o_005ea3d0(void);
RT_WRAP(005ea3d0, gre_cup_set_fixture_subs)
{
    uint32_t comp = cpu.ecx, fixture = rd32(g_mem, cpu.esp + 4);
    o_005ea3d0();
    if (rt_unwinding || !active() || !readable(comp) || !readable(fixture)) return;
    const char *file = season_change_file(comp);
    int used = 0, bench = 0;
    int mask = strcmp(file, "?") ? lookup(file, season_of(comp), &used, &bench) : 0;
    uint32_t word = rd16(g_mem, fixture + 0x4d);
    /* Round 10 (0x5ea3d4: the word at fixture +0x30): the original names 7. The table has one count for the season,
     * not one for a round, so round 10 keeps the original's bench. */
    if ((mask & SEASON_SUBS_BENCH) && (int16_t)rd16(g_mem, fixture + 0x30) != 10)
        word = (word & ~0xfu) | (uint32_t)bench;
    if (mask & SEASON_SUBS_USED) word = (word & ~0xf0u) | ((uint32_t)used << 4);
    wr16(g_mem, fixture + 0x4d, word);
}

static void subs_before(const SeasonChange *c)
{
    pass_season = c->season;
    pass_day = (int16_t)rd16(g_mem, 0xae2c90);
    pass_year = (int16_t)rd16(g_mem, 0xae2c92);
}

/* A new game, a load and the end of a game: no start_new_season runs. */
static void subs_new_game(int start_year)
{
    if (logging()) fprintf(stderr, "recomp: season subs: new game %d, rules %s\n", start_year, active() ? "on" : "off");
    pass_season = 0;
}

const SeasonRuleKind season_kind_subs = {"subs", subs_new_game, subs_new_game, NULL, NULL, subs_before, NULL};
