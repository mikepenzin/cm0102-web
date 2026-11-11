/* The season change (0.6.0 slice 16; spec docs/specs/2026-10-16-seasons-0.6.0-spec.md, Q232, Q245, Q248, Q258).
 *
 * With changing rules on, each competition takes the rules of its new season at its season change, and the
 * player gets one news item for each season in which the rules change. With changing rules off, nothing
 * changes after the start (Q232). Research and evidence: docs/research/2026-10-17-season-change.md.
 *
 * When a season changes. The original has no single "new season" step. game_season_update_pass 0x5d5ae0 runs
 * at the end of every day. On the update day of a league table entry (+0x15, compared at 0x5d5bc8) it may move
 * the entry's season year (+0x35) and then calls start_new_season (vtable +0x8, at 0x5d5d1c) of each finished
 * competition of the entry. A top league also starts its lower leagues. The runtime wraps the pass and every
 * start_new_season (season_starts.c, made by tools/seasons/make_start_wraps.py). Hooks run only inside the
 * pass, only with changing rules on, and only for a season after the start year (the run from 1 January to
 * the start date at a new game, 0x5cc850, is not a change).
 *
 * The season S of a change comes from the game date: a change from January to September of year Y starts
 * season Y (2009 for 2009/10), a change from 1 October starts season Y + 1 (calendar-year leagues with update
 * days 27 November and 27 December start their next edition then; the rules table puts an edition in the row
 * of the year it is played).
 *
 * The season point of S is the first pass from 1 June to 30 September of year S in which an entry has its
 * update day (20 June in a game with England). The season hook runs there, and the news item of S is posted
 * there. A calendar-year league that changes before (January or February of S, or December of S - 1) gets its
 * rules on its own date; its sentences wait for the item of S. A sentence after the point goes into the item
 * of S that is already posted (the text is read when the item is shown).
 *
 * The registry. A rule kind is a table of functions; NULL leaves a hook out. Each later slice (17 to 29) adds
 * its kinds to the list season_rule_kinds in season_change.c, one line each. The hooks, in the order they run:
 *   new_game(Y)   after a new game is made in start year Y (any changing rules; after the start season hooks
 *                 of the run to the start date, see "The start season"). Example: build a kind's own
 *                 state for the game, such as the season of each rule it has applied.
 *   loaded(Y)     after a saved game is loaded. Example: rebuild that state from the game's memory.
 *   season(S)     once at the season point of S. Example: transfer windows of the new season.
 *   entry(c)      once per league table entry on its update day, before its first competition starts
 *                 (c->comp is 0). Scope by nation with c->nation, by entry with c->entry. The pass checks
 *                 the nation's selected flag (+0x11c, 0x5d5be5) only for the step of the entry's season year,
 *                 so this hook runs for an entry of a nation that is not selected too: a kind that adds a
 *                 competition checks that flag itself. Example: a
 *                 competition that starts in season S gets its object with season_change_add_comp.
 *   before(c)     before the start_new_season of a competition (c->started is 1). The original's own
 *                 set_up_rules (vtable +0x8c) and create_fixtures (League::create_fixtures 0x6835c0) run
 *                 inside start_new_season after it, so fields they set are overwritten. Example: a club count
 *                 that set_up_rules reads.
 *   after(c)      after the start_new_season of a competition (c->started 1), and on the update day for each
 *                 competition of the entry that did not start (c->started 0: an unfinished competition, or an
 *                 international one that waits for its next edition, such as the 2010 qualifiers on 25 July
 *                 2009). Example: the name of the season (season_kind_names.c).
 *
 * The base game. No kind is called at all (no hook of any kind) in the base game, a game that starts in 2001
 * with changing rules off, and with the test switch RECOMP_SEASON_CODE=off. So the base game and its saves stay
 * as they are (Q152, Q342), whatever a kind does.
 *
 * The start season. A game that starts after 2001 runs the entry, before, after and season hooks for its start
 * season too (S equal to the start year, c->start 1, season_change_is_start() 1), with changing rules on or off.
 * A game that starts in 2001 does not: the 2001/02 rules are the original's. Each league table entry gets the
 * hooks of the start season once:
 *   - an entry with its update day from 1 January to the start date (entries 5, 4, 11, 22, 2, 14, 3 and others):
 *     in the run to the start date of a new game (game_run_to_start_date 0x5cc850, called from create_game
 *     0x828110 at 0x829fd9; c->creating 1), before the first fixtures of the game are made;
 *   - an entry with its update day after the start date and before 1 October (entries 7 on 21 July, 0 on 25 July,
 *     8 on 31 July, 1 on 15 August, 6 on 28 August and others): on that day in the game (c->creating 0);
 *   - an entry with its update day from 1 October (entries 24 on 27 November, 15 on 27 December and others): its
 *     start season began in the year before the start year, before the game. At the end of the new game, its
 *     entry hook and the after hook of each of its competitions run (c->started 0, c->creating 1); there is no
 *     start_new_season, so no before hook. A kind that changes fixtures fixes the ones that are made.
 * So a kind applies the rules of the start season, for example 5 substitutes in a 2010/11 game. The start season
 * gives no news: season_change_news does nothing in its hooks. A later season (S after the start year) runs the
 * hooks only with changing rules on. The hooks of the start season also run after a load in the start year (the
 * in-game days above), so a kind writes only a real change, as for every season.
 *
 * A kind applies the rule of season S from its own table (the rules table, season_rules.h, or another). It
 * writes the game's memory only for a real change, so a season with no rule change leaves the game as it is.
 * For each change the player sees, it calls season_change_news with one sentence. After the last row of a
 * table the rule stays (Q248): a kind that finds no row changes nothing. */
#ifndef RECOMP_SEASON_CHANGE_H
#define RECOMP_SEASON_CHANGE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int season;             /* S: 2009 for 2009/10 */
    uint32_t comp;          /* the competition object, 0 in entry() */
    const char *file;       /* the source file of its class (eng_first), "?" when not known, "" in entry() */
    int entry;              /* its league table entry, 0 to 34, or -1 when not known */
    uint32_t nation;        /* the entry's nation record (entry +0), 0 for a continental or world entry */
    int started;            /* 1: start_new_season runs for it now; 0: see after() above */
    int start;              /* 1: S is the start season of the game (see "The start season" above) */
    int creating;           /* 1: the game is being made now (the run to the start date of a new game) */
} SeasonChange;

typedef struct {
    const char *name;                                         /* for the log: "names" */
    void (*new_game)(int start_year);
    void (*loaded)(int start_year);
    void (*season)(int season);
    void (*entry)(const SeasonChange *c);
    void (*before)(const SeasonChange *c);
    void (*after)(const SeasonChange *c);
} SeasonRuleKind;

/* Adds one sentence to the news item of the season of the hook that runs ("The UEFA Cup is now called ...").
 * Only a kind calls it, inside a season, entry, before or after hook. Plain words, British English, no
 * exclamation marks, no "<" (the game's text code reads it as the start of a token), in the game's code
 * (season_change_latin1). The text of one season holds at most 8 KB; later sentences are left out. */
void season_change_news(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Turns UTF-8 text (the rules table is UTF-8) into the game's one-byte code (Windows-1252, as the data files):
 * each two-byte sequence of a character up to U+00FF becomes one byte. Other bytes stay. */
void season_change_latin1(char *s);

/* Changes the name of a competition. Every change of a competition's name in the season code goes through
 * this function: the names kind of this slice, and slice 15 at a new game.
 *
 * rec is the address of a competition record in the game's memory: a club_comp.dat or nation_comp.dat record
 * (0x6b bytes) of the array at [0xae23d0], the club comps first and then the nation comps. A competition object
 * holds the address of its record at +4. name is the new long name in the game's code (Windows-1252: call
 * season_change_latin1 first on UTF-8 text). The function writes at most 50 characters and the 0 to the
 * record +4 and clears the rest of the 51 bytes. A rec that is not a record of that array (an address before
 * it, inside a record, or at or after its count [0xae2378]) changes nothing.
 *
 * The game finds its key competitions by name at every load of the data, also of a saved game
 * (set_key_ids 0x601a40, match_club_comp_key_id 0x60efd0, the box "index.cpp 5809" when a name is missing).
 * So the function keeps the record's name in the data by the record's index, and the wrap of 0x60efd0 gives
 * that name to the match. The first change of a record keeps the name in the data: a second change (a chain
 * such as UEFA Cup, UEFA Europa League, then another name) does not overwrite it. The names go into the season
 * part of the save (version 2), so a load knows them before the original reads the data.
 *
 * A game with a changed name always writes the season part, also a 2001/02 game with changing rules off
 * (season.c, part_needed). Call it while a game is made or on: after season_change_reset, which the runtime calls before a new game,
 * a load and the end of a game (season.c). The list is cleared there, so call it again for each new game.
 * It adds no news. A kind that wants a news sentence calls season_change_news inside its hook. */
void season_change_rename(uint32_t rec, const char *name);

/* Slice 26 (season_kind_club_world.c): the Club World Championship's part of the shared history thunk of slot 12,
 * 0x40f590. The one wrap of 0x40f590 calls it first; it returns 1 when it handled the call (it then did the jump or
 * the return), 0 when the wrap goes on. */
int season_club_world_history_thunk(void);

/* Slice 26 (season_comps.c): the long name of a competition record in the data, before any season_change_rename
 * (the name that the game's name matchers see, 0x60efd0), or its current name when it has no rename. NULL for
 * an address that is not a record. */
const char *season_change_data_name(uint32_t rec);

/* The number of records with a changed name in this game (slice 15): at a load, the names of the season part. */
int season_change_renames(void);

/* The competition objects that the game holds: the table g_comp_object_table [0xadadfc] and the
 * competition arrays of the 35 league table entries (as the rules report reads them, season_report.c).
 * Fills out with at most max distinct objects and returns their count. It only reads memory. */
int season_change_comps(uint32_t *out, int max);

/* The competition objects of one league table entry (its array, +0xc count, +0x10 array). */
int season_change_entry_comps(int entry, uint32_t *out, int max);

/* Keeps a competition idle for the season that starts (a league cup that ends, the Intertoto Cup after 2008).
 * Only inside a before hook. The start_new_season of that competition does not run: it keeps its finished
 * season, makes no fixtures and plays no matches. The object stays in its entry's array, so the game saves it,
 * loads it and frees it as before (the arrays own the objects: save 0x6692f0 from 0x82f139, the load rebuilds
 * [0xadadfc] from them at 0x82c5f6, the free 0x669290 from 0x82ffa3), and its record and history stay. The after
 * hook then runs with c->started 0. The pass asks again each season (the competition stays finished), so a kind
 * calls this in each season in which the competition does not run; nothing is kept in the save. Not tested in a
 * game yet: which screens show an idle competition is unknown, so the first kind that uses it checks that. */
void season_change_skip(void);

/* 1 while the hooks of the start season run (see "The start season" above). For the season hook, which gets
 * no SeasonChange. */
int season_change_is_start(void);

/* 1 while create_game 0x828110 runs, so in the run to the start date of a new game. */
int season_change_is_creating(void);

/* The source file of a competition object's class (eng_first), or "?" for a class that is not known. */
const char *season_change_file(uint32_t obj);

/* Puts a competition object that a kind has made (in the game's heap) into the array of a league table
 * entry, so the pass starts its seasons from then on. Only inside an entry hook of that entry, before the
 * pass reads the array. The array is made again with the game's operator new 0x944cf1, and the old one goes
 * back with operator delete 0x944c94. Returns 1, or 0 when it cannot. */
int season_change_add_comp(int entry, uint32_t obj);

/* The last season whose season point ran, 0 before any (a saved game of slice 11 has none). A kind that rebuilds
 * its state after a load takes the season of the game from it (the larger of this and the start year). */
int season_change_point(void);

/* season_starts.c calls it around each start_new_season (this, 0 before and 1 after). */
int season_change_start(uint32_t comp, int after);   /* 1 before: a kind skips this season */

/* season.c calls these. */
void season_change_new_game(void);                            /* a new game is made */
void season_change_creating(int on);                          /* 1 before create_game runs, 0 after it */
void season_change_loaded(void);                              /* a saved game is loaded */
void season_change_reset(void);                               /* before a new game, a load, the end of a game */

/* The season part of the save (season.c). Version 2 adds after the 16 bytes of version 1:
 *   u32 count, then count times: u32 season, u32 posted (1 when the item is sent), u32 length n, n bytes;
 *   u32 count, then count times: u32 record index (from [0xae23d0], 0x6b bytes a record), 51 bytes name in
 *       the data (the name, its 0 and 0 to the end of the field: two runs with the same clicks write the same
 *       bytes; docs/research/2026-11-11-season-part-unset-bytes.md);
 *   u32 the last season point;
 *   from row A18, when qualifiers wait for the first running season: u32 count, then count times 20 bytes
 *       (season_friendly_dates.c). No bytes at all when none wait.
 * season_change_part_size gives 0 when the game has none of these (the part stays version 1). A reader takes
 * these fields of a later version too and leaves what follows them. */
uint32_t season_change_part_size(void);
void season_change_part_write(uint8_t *out);                  /* season_change_part_size bytes */
int season_change_part_read(const uint8_t *in, uint32_t n);   /* 0 for damaged bytes */

#endif
