/* The wait step of RECOMP_SCRIPT (user32.c): "wait <state> [timeout ms]" holds the later steps until the
 * game reaches a named state. The runtime reads the state from the memory of the game, not from the
 * picture. Only test runs with a script read the states. Two parts run in every game: waits_poll (at
 * each input poll it counts the poll, notes the screen node and the date of the first news screen, and
 * prints the screen log when RECOMP_SCREEN_LOG is set) and the wrap of the message box (it counts open
 * boxes). Both only record. They do not change what the game does. The wrap also skips the box "CD Not
 * Required" (cd_box) and the box "Database..cpp 7784" of a .lng file that does not fit the database
 * (lng_count_box).
 *
 * A state of a screen counts only after an input poll 0x55f620 has run on that screen after the wait
 * started. The screen is then built and drawn and takes input (main loop 0x7fc7e0: build, draw, pump,
 * input poll; docs/research/2026-10-13-screen-stack-inventory.md, item 3).
 *
 * The screen stack S is at 0xb74450. The record of the local player is at S + 0x7d4. Its current node
 * (the screen on top) is at record + 0x0e. The build function of a node is at node + 0, its input
 * function at node + 8 (inventory, items 1b and 1c). The titles of the nodes are all "\", so the build
 * function names the screen.
 *
 * States:
 *   setup       the start screen (Setup Game), build 0x81bab0
 *   leagues     Select League(s), build 0x81d010
 *   loaded      the game is made: the database load and the days before the start are done
 *               (0xdd711c = 1, set at the end of game creation 0x828110).
 *   first-news  the first news screen (build 0x7881e0) of this run: a news screen on the date and part
 *               of the day of the first news screen that took input
 *   news        any news screen
 *   popup       a popup is open (input mode, the dword at the screen manager + 0, is 1; keys.c)
 *   box         a message box of 0x5e8290 is open. The box "CD Not Required" of a new game and a
 *               load never shows (cd_box). The box runs its own loop without the input poll, so
 *               this state needs no poll.
 *   typing      a text entry takes keys (keys.c)
 *   match       the match screen of the player's club, build MATCH_BUILD
 *   continue    Continue Game can take a click (keys.c)
 *   day:D.M.YY  the game date is D.M.YY or later (s16 day of the year from 0, s16 year at 0xae2c90)
 *   screen:ADDR the screen with this build function on top, for example screen:0x6b2d80
 *   options     the Bosman Edition Options box on top (options_screen.c)
 *   seasons     the Choose Season screen on top (season_screen.c)
 *   extras      the Extras screen of Setup Game on top (database.c)
 *   label:TEXT  a clickable button of the left bar with a text that starts with TEXT (keys.c), for
 *               example label:Re-start for the button of the match screen at half time. The match
 *               screen changes the text of one button: Kick Off, Pause, Re-start. The log names the
 *               texts of the left bar buttons.
 *
 * RECOMP_SCREEN_LOG=1 prints each change of the state, with the build function of the top screen.
 * Use it to find the address for screen:ADDR. */
#include "win.h"
#include "season_screen.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK 0xb74450u
#define TOP_NODE (STACK + 0x7d4u + 0x0eu)
#define GAME_MADE 0xdd711cu
#define GAME_DATE 0xae2c90u

#define START_BUILD 0x81bab0u
#define LEAGUES_BUILD 0x81d010u
#define NEWS_BUILD 0x7881e0u
#define MATCH_BUILD 0x718e90u   /* the match screen of the player's club, before and during the match */

enum { W_SETUP, W_LEAGUES, W_LOADED, W_FIRST_NEWS, W_NEWS, W_POPUP, W_TYPING, W_MATCH, W_CONTINUE, W_BOX, W_DAY,
       W_SCREEN, W_LABEL };

static const char *const names[] = {"setup", "leagues", "loaded", "first-news", "news", "popup", "typing", "match",
                                    "continue", "box"};

static uint32_t polls;            /* input polls so far */
static uint32_t polled_node;      /* the top node at the last input poll */
static uint32_t first_news_date;  /* date and slice of the first news screen of this run, 0: none yet */
static int in_box;                /* 1 while the message box 0x5e8290 runs */
static char labels[32][32];       /* the texts of the label waits; the value of a label wait is its index */
static int label_count;

static uint32_t top_node(void) { return rd32(g_mem, TOP_NODE); }
static uint32_t top_build(void) { uint32_t n = top_node(); return n ? rd32(g_mem, n) : 0; }

/* The game date and the part of the day (0xae2c88: 0 morning, 1 afternoon, 2 evening), never 0. */
static uint32_t date_slice(void) { return (rd32(g_mem, GAME_DATE) ^ rd32(g_mem, 0xae2c88u) << 24) | 0x80000000u; }

/* The day of the year from 0, as the game keeps it. */
static int day_of_year(int d, int m, int y)
{
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return before[m - 1] + (m > 2 && leap) + d - 1;
}

int waits_parse(const char *name, int *kind, uint32_t *value)
{
    for (int i = 0; i < (int)(sizeof names / sizeof *names); i++)
        if (!strcmp(name, names[i])) { *kind = i; *value = 0; return 1; }
    int d, m, y;
    if (sscanf(name, "day:%d.%d.%d", &d, &m, &y) == 3 && d >= 1 && d <= 31 && m >= 1 && m <= 12) {
        if (y < 100) y += 2000;
        *kind = W_DAY;
        *value = (uint32_t)(y * 366 + day_of_year(d, m, y));
        return 1;
    }
    unsigned long a;
    if (sscanf(name, "screen:%lx", &a) == 1 && a) { *kind = W_SCREEN; *value = (uint32_t)a; return 1; }
    if (!strcmp(name, "options")) { *kind = W_SCREEN; *value = options_screen_build_fn(); return 1; }
    if (!strcmp(name, "seasons")) { *kind = W_SCREEN; *value = season_screen_build_fn(); return 1; }
    if (!strcmp(name, "extras")) { *kind = W_SCREEN; *value = setup_extras_build_fn(); return 1; }
    if (!strncmp(name, "label:", 6) && name[6] && strlen(name + 6) < sizeof labels[0] &&
        label_count < (int)(sizeof labels / sizeof *labels)) {
        snprintf(labels[label_count], sizeof labels[0], "%s", name + 6);
        *kind = W_LABEL;
        *value = (uint32_t)label_count++;
        return 1;
    }
    return 0;
}

uint32_t waits_poll_count(void) { return polls; }

/* After the input poll ran on the screen of the top node: the screen is built and takes input. */
static int screen_up(uint32_t build)
{
    return build && polled_node && polled_node == top_node() && top_build() == build;
}

/* 1 when the state is reached. since: the poll count when the wait started. */
int waits_reached(int kind, uint32_t value, uint32_t since)
{
    if (kind == W_BOX) return in_box;
    if (polls == since) return 0;                 /* no input poll since the wait started */
    uint32_t d = keys_display;
    int x, y;
    switch (kind) {
    case W_SETUP: return screen_up(START_BUILD);
    case W_LEAGUES: return screen_up(LEAGUES_BUILD);
    case W_LOADED: return rd32(g_mem, GAME_MADE) != 0;
    case W_FIRST_NEWS: return screen_up(NEWS_BUILD) && date_slice() == first_news_date;
    case W_NEWS: return screen_up(NEWS_BUILD);
    case W_POPUP: return d && rd32(g_mem, d) == 1;
    case W_TYPING: return keys_typing();
    case W_MATCH: return screen_up(MATCH_BUILD);
    case W_CONTINUE: return keys_continue_game(&x, &y);
    case W_DAY: {
        int day = (int16_t)rd16(g_mem, GAME_DATE), year = (int16_t)rd16(g_mem, GAME_DATE + 2);
        return rd32(g_mem, GAME_MADE) && (uint32_t)(year * 366 + day) >= value;
    }
    case W_SCREEN: return screen_up(value);
    case W_LABEL: return polled_node == top_node() && keys_left_bar_label(labels[value]);
    }
    return 0;
}

/* The state now, in words, for the log and for the message of a wait that timed out. */
void waits_describe(char *out, size_t size)
{
    uint32_t d = keys_display, node = top_node();
    int x, y;
    char texts[96];
    keys_left_bar_labels(texts, sizeof texts);
    snprintf(out, size, "screen %#x, date %d/%d, made %u, popup %d, box %d, typing %d, continue %d, back %d, "
             "left bar %s", node ? rd32(g_mem, node) : 0, (int16_t)rd16(g_mem, GAME_DATE),
             (int16_t)rd16(g_mem, GAME_DATE + 2), rd32(g_mem, GAME_MADE), d ? (int)rd32(g_mem, d) : -1, in_box,
             keys_typing(), keys_continue_game(&x, &y), keys_back(&x, &y), texts);
}

/* At each input poll (user32.c), before the original runs. */
void waits_poll(void)
{
    static int log = -1;
    static char last[256];
    polls++;
    polled_node = top_node();
    if (top_build() == NEWS_BUILD && !first_news_date) first_news_date = date_slice();
    if (log < 0) log = getenv("RECOMP_SCREEN_LOG") != NULL;
    if (!log) return;
    char now[256], texts[96];
    uint32_t d = keys_display;
    int x, y;
    keys_left_bar_labels(texts, sizeof texts);
    snprintf(now, sizeof now, "screen %#x, popup %d, typing %d, continue %d, made %u, left bar %s", top_build(),
             d ? (int)rd32(g_mem, d) : -1, keys_typing(), keys_continue_game(&x, &y), rd32(g_mem, GAME_MADE), texts);
    if (strcmp(now, last)) {
        fprintf(stderr, "recomp: state at %llu ms, date %d/%d: %s\n", (unsigned long long)SDL_GetTicks64(),
                (int16_t)rd16(g_mem, GAME_DATE), (int16_t)rd16(g_mem, GAME_DATE + 2), now);
        snprintf(last, sizeof last, "%s", now);
    }
}

void o_005e8290(void);
extern const uint32_t rt_site_cd_box_new, rt_site_cd_box_load, rt_site_lng_count_box;

/* The box "CD Not Required" at the end of a new game (call 0x82a98b) and of a load (call 0x82e400). The
 * build uses no CD, so the box never shows. Both callers drop the return value of the box. Each call
 * shows only this box: its title is langlib_sprintf 0x66f4e0 of the text 0xa81384 just before the call
 * (0x82a95a, 0x82e3d1), so the return address alone finds it. The title is in the language of the game,
 * "CD Non Richiesto" in Italian, so a check of the English title missed the other languages. */
static int cd_box(void)
{
    uint32_t back = rd32(g_mem, cpu.esp);
    return back == rt_site_cd_box_new || back == rt_site_cd_box_load;
}

/* The box "Database..cpp 7784" (call 0x5358b7 in database_load_language_names 0x5339a0). In a language
 * other than English, the function reads the names of CM3_TEMP\<lang>.lng and first compares the counts
 * in its header with the database (0x533e6a..0x533ec5). The .lng files of the game are those of 3.9.68,
 * so every other database, and a save that holds 3.9.68 .lng copies, gave the box at each new game and
 * load. On a difference the function closes the file, shows the box, sets 0xb67a34 to 0 and returns
 * with no names loaded, so the names stay English. The skip keeps all of that except the box. The same
 * call also shows "Database..cpp 7752" when the .lng file does not open (0x533c74, jump 0x533caa to
 * 0x535897). That box still shows: the skip tests the line number, the last argument of 0x90d130 (0x1e68
 * pushed at 0x535890, 0x1e48 at 0x533ca4), which is still on the stack at the call (argument 8). */
static int lng_count_box(void) { return rd32(g_mem, cpu.esp) == rt_site_lng_count_box && rt_arg(8) == 0x1e68; }

#define FATAL_TITLE 0x9870e0u      /* "Error": the title of the game's fatal box (pushed at 0x41b703 and the like) */

/* The message box of the game: its own loop draws it and waits for Ok. */
RT_WRAP(005e8290, message_box)
{
    if (cd_box()) {
        fprintf(stderr, "recomp: skipped the box CD Not Required\n");
        rt_ret_cdecl(0);
        return;
    }
    if (lng_count_box()) {
        fprintf(stderr, "recomp: skipped the box Database..cpp 7784 (the .lng counts differ from the database)\n");
        rt_ret_cdecl(0);
        return;
    }
    /* Every box goes to the log, with its caller: the browser's error reports carry the log, and the fatal box
     * "Error" says only "v3.9.68 <source>..cpp <line>" before the game exits with -1 (419 sites: sprintf of
     * "v%s %s.%s %d" 0x9870e8, this box with the title "Error" 0x9870e0, game_fatal_cleanup 0x5cc7d0, exit(-1)
     * 0x945934; for example 0x41b6ed..0x41b726). A player's report of a save that would not load had only "The game
     * ended with code -1" (14 November 2026). The texts are the game's own, short and the same for every player. */
    uint32_t title = rt_arg(0), text = rt_arg(1);
    /* Every 32-bit address lies inside g_mem (rt_init maps 4 GB and a page), so the reads stay inside; the
     * precision stops at 200 bytes when a text has no end. */
    fprintf(stderr, "recomp: message box from %#x: %.200s: %.200s\n", rd32(g_mem, cpu.esp),
            title ? (const char *)(g_mem + title) : "", text ? (const char *)(g_mem + text) : "");
    if (title == FATAL_TITLE || getenv("RECOMP_SCREEN_LOG")) {
        /* The values on the stack that look like return addresses of the game's code, so that a test log
         * names the callers of an error box. A value is only a guess: the scan does not follow the frames.
         * The upper bound 0x944000 is the end of the game's code: it leaves out the C runtime part of .text
         * (0x944000 to 0x967000). */
        fprintf(stderr, "recomp: message box stack:");
        for (uint32_t k = 0; k < 400; k++) {
            uint32_t v = rd32(g_mem, cpu.esp + 4 * k);
            if (v >= 0x401000u && v < 0x944000u) fprintf(stderr, " %#x", v);
        }
        fprintf(stderr, "\n");
    }
    in_box++;
    o_005e8290();
    in_box--;
}
