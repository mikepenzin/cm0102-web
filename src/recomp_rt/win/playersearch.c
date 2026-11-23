/* The option full_player_search (0.5.0, #57).
 *
 * The player search list builder 0x7afa70 adds a player only when the human manager knows him: the score of
 * 0x5407e0 is 4000 or more (jge at 0x7afc4b), or a scout report exists, or he is from the same region
 * and one of 0x544520, 0x544590 and 0x76d9b0 is true (docs/research/2026-10-16-crash-fixes.md, 1c).
 * Changing the jge to a jmp lists every player. Here every original call still runs, so that the
 * game state is the same with the option on and off. tools/recomp/widescreen.py makes the three places that
 * reject a player ask rt_full_player_search instead. With the option off, the original's path runs.
 *
 * The limit (forum report of 2026-10-21). The screen counts the players it shows in 16 bits: 0x7af160 adds
 * each player that passes the filters to a list with the word count inc at 0x68d191, and the found text of
 * 0x808690 (0x80b6f6) reads that word as signed. Above 32767 players the word is negative and the screen
 * says "1 player found (N filtered out)" over an empty list. Two places add players after the extra ones:
 * the players the manager knows that come later in the staff order (the original's accept path), and
 * 0x7af870 (scout lists, 0x7affe2) and 0x7af720 (0x7af31b, at most 50). So:
 *   - extra players (the ones the original rejects) join only while the list holds fewer than LIST_MAX;
 *   - a known player that comes when the list is full takes the place of the last extra player
 *     (rt_full_player_search_slot), so the list keeps every player that the option off lists;
 *   - LIST_MAX leaves room for the later adds, and rt_full_player_search_shown caps the players that
 *     0x7af160 passes to the screen list at 32767 as a last guard.
 * All of this runs only with the option on.
 *
 * The daily fitness update (0.6.1). The list fill 0x7af160 asks the status of every player in the list
 * (get_player_status_code 0x4849f0 at 0x7af4ac, and the filters of 0x807450), and the screen 0x808690 asks it
 * for each row it draws (0x62c400 at 0x80e70d). Injury::get_player_injury_status 0x62c400 first runs
 * Injury::update_player_fitness 0x62c680, which brings the player up to today once a day and draws random
 * numbers (random(2) at 0x62cb18). So each extra player had his update of the day at the search, earlier than
 * the game would run it, and every later random number moved: the same clicks gave another game (Q152). Now
 * the builder marks each extra player, and while the search runs (search_time) the wrap of 0x62c680 (picker.c)
 * skips the update for a marked player. The game runs his update later, at the same point as with the option
 * off. The status that the list shows for an extra player is the one of his last update.
 *
 * The memory of the search (0.6.1). With the option on the list holds more rows: the builder's list block grows
 * further (0x945667 at 0x7afdc4), 0x7af160 makes a larger array of the passed rows (0x944e46 at 0x7af343) and
 * adds more rows to the screen list (0x68d0d0 at 0x7af547), and the screen draws other names, so the text
 * blocks of the screen code grow at other points (0x5b1c4d). The heap then held other free blocks after the
 * search, later blocks of the game got other addresses, and the save differed in many parts. So while the search
 * runs (search_time: its build or fill runs, or it is the top screen), the heap gives its blocks from the zone
 * (heap.c), with the option on and off alike. The game's heap after the search is the same in both. With the
 * option off the blocks of the search also sit in the zone now, so after a search the game's blocks have other
 * addresses than in 0.6.0 (memory layout only, as when a screen draws other text). */
#include "options.h"
#include "../rt.h"
#include "../rt_host.h"

#include <stdlib.h>
#include <string.h>

#define LIST_COUNT 0x1f6        /* [search screen + 0x1f6]: the players in the list */
#define LIST_MAX 30000          /* the extra players stop here: room for the later adds */
#define SHOWN_MAX 32767         /* the screen keeps its count in 16 bits (inc word at 0x68d191) */

static uint32_t extras[LIST_MAX];   /* the list rows that hold extra players, in the order they joined */
static uint32_t n_extras;
static int pending;                 /* the player under test joins as an extra player */
static int replaced;                /* the row of the player under test replaces an extra player */
static uint32_t pending_player;     /* the staff record of the player under test */

#define STAFF_BASE 0xae23bcu        /* [ ]: the staff records, 110 bytes each */
#define STAFF_COUNT 0xae2364u       /* [ ]: the count of staff records */
#define STAFF_SIZE 110u
static uint8_t *marks;              /* 1 for each staff record that is an extra player of the list */
static uint32_t marks_cap;
static int searching;               /* 0x7af160 or 0x808690 runs */

static int64_t staff_index(uint32_t staff)
{
    uint32_t base = rd32(g_mem, STAFF_BASE), count = rd32(g_mem, STAFF_COUNT);
    if (staff < base || (staff - base) % STAFF_SIZE) return -1;
    uint32_t i = (staff - base) / STAFF_SIZE;
    return i < count ? (int64_t)i : -1;
}

static void mark(uint32_t staff)
{
    int64_t i = staff_index(staff);
    if (i < 0) return;
    if ((uint32_t)i >= marks_cap) {
        uint32_t cap = rd32(g_mem, STAFF_COUNT);
        uint8_t *m = realloc(marks, cap);
        if (!m) return;
        memset(m + marks_cap, 0, cap - marks_cap);
        marks = m;
        marks_cap = cap;
    }
    marks[i] = 1;
}

static int search_time(void);

/* 1 when the daily update of this staff record waits: the search runs and the player is an extra player. */
int playersearch_holds_update(uint32_t staff)
{
    if (!marks_cap || !search_time()) return 0;
    int64_t i = staff_index(staff);
    return i >= 0 && (uint32_t)i < marks_cap && marks[i];
}

/* 1 when a player that the original rejects joins the list: the option is on and the list has room. */
uint32_t rt_full_player_search(uint32_t screen, uint32_t staff)
{
    if (!options_bool(OPT_FULL_PLAYER_SEARCH)) return 0u;
    if (rd32(g_mem, screen + LIST_COUNT) >= LIST_MAX) return 0u;
    pending = 1;
    pending_player = staff;
    return 1u;
}

/* The builder empties the list (0x7afbb3). */
void rt_full_player_search_reset(void)
{
    n_extras = 0;
    pending = 0;
    replaced = 0;
    if (marks_cap) memset(marks, 0, marks_cap);
}

/* The player under test does not join (0x7afe6b, the end of the loop body). */
void rt_full_player_search_skip(void)
{
    pending = 0;
}

/* The row for the player that joins (0x7afdda reads the count here). */
uint32_t rt_full_player_search_slot(uint32_t screen)
{
    uint32_t count = rd32(g_mem, screen + LIST_COUNT);
    replaced = 0;
    if (pending) {
        pending = 0;
        mark(pending_player);
        if (n_extras < LIST_MAX) extras[n_extras++] = count;
        return count;
    }
    if (n_extras && count >= LIST_MAX && options_bool(OPT_FULL_PLAYER_SEARCH)) {
        replaced = 1;
        return extras[--n_extras];
    }
    return count;
}

/* 1 when the row of the player replaced an extra player: the count stays (0x7afe5e). */
uint32_t rt_full_player_search_replaced(void)
{
    uint32_t r = (uint32_t)replaced;
    replaced = 0;
    return r;
}

/* The players that 0x7af160 adds to the screen list (ebx at 0x7af467). */
uint32_t rt_full_player_search_shown(uint32_t passed)
{
    if (passed > SHOWN_MAX && options_bool(OPT_FULL_PLAYER_SEARCH)) return SHOWN_MAX;
    return passed;
}

void o_007af160(void);
void o_00808690(void);

#define SCREEN_BUILD 0x808690u                  /* the player search screen (its build function) */
#define TOP_NODE (0xb74450u + 0x7d4u + 0x0eu)   /* the top node of the screen stack S; [node] is its build */

/* 1 while the search runs: its build or fill runs, or it is the top screen. */
static int search_time(void)
{
    if (searching) return 1;
    uint32_t node = rd32(g_mem, TOP_NODE);
    return node && rd32(g_mem, node) == SCREEN_BUILD;
}

/* The list fill (thiscall on the search screen): it builds the list and asks each player's status. */
RT_WRAP(007af160, search_fill)
{
    rt_heap_zone = search_time;
    searching++;
    o_007af160();
    searching--;
}

/* The player search screen: its rows ask each shown player's status (0x80e70d). */
RT_WRAP(00808690, search_screen)
{
    rt_heap_zone = search_time;
    searching++;
    o_00808690();
    searching--;
}
