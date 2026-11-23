/* The Bosman Edition Options box (spec 2026-10-16-features-0.5.0, slice 6; #56).
 *
 * A box the size of the original's Game Settings box (build 0x823950), with every option of the settings
 * file (options.h) on four tabs, Graphics and Game with nine rows each (Graphics was Screen until 0.6.0),
 * Network (0.7.0 slice 4: Online play) and Squads (0.7.3 slice 13: both other tabs were full). Labels and notes come from the mockup docs/research/mockups/2026-10-16-options-screen.html, variant B.
 * The version line shows the project version (CMakeLists.txt, CM0102_VERSION).
 *
 * The box is a screen of the runtime on the game's own screen stack, as Team views (views.c). The push of
 * the original 0x7fe600 gets flag 1, as Game Settings does (0x8238bb): the game draws it as a box over the
 * screen below (docs/research/2026-10-16-options-screen.md, 1a). Its build and input functions are host
 * functions (rt_host_fn). The build copies the frame of 0x823950: a dark area over the whole screen
 * (0x823a92), the green box (145, 50)-(744, 549) (0x823af4), the heading (0x803fd0 at 0x823b41) and the
 * version line (0x823bb9). A row uses the original's choice row style: the label in font 3 (0x8247d9),
 * the current value as a yellow framed label, style 0x221 (0x804100 at 0x82477b), and each other value
 * as a green button (0x8040a0 at 0x8247a4). One Close button closes the box.
 *
 * Two places open it. The first is a new item "Bosman Edition Options" after "Game Settings" in the side bar
 * drop-down "Game Options" (item create 0x760602, options_screen_widget_before/after). The main loop
 * 0x7fc7e0 handles a click on Game Settings before the input of the screen: it calls the hook at S + 0x37ee
 * when the id of the clicked widget is S + 0x37f2 (0x7fcd25), and the side bar build sets 0x823930 and 0x423
 * there (0x75db3b). So the item works on every screen with the side bar: after the poll of the main loop
 * (0x7fcc14) returns the item, the runtime puts its own hook and id there, and its hook puts the original
 * values back. The second place is an item of the start screen menu (database.c).
 *
 * A click on a value calls options_set at once: the running game has the new value, and settings.txt
 * gets it through its temporary file. Rows whose value the game reads only at start say so in a note.
 * When each value takes effect (docs/research/2026-10-16-options-screen.md, 3b):
 * - display, sound and save_folder: at the next start.
 * - changing_rules: the default of a new game. Setup Game shows and sets the value of the next new game
 *   (database.c); a game keeps the value it started with. The database of a new game is chosen on the Choose
 *   Season screen (0.6.0 slice 13, Q236), so the box has no Database row any more.
 * - match_columns, attribute_groups and the colour switches: when their screen is built next. The screen
 *   below the box is built again when the box closes.
 * - full_player_search: on the next game day (the search keeps its list for the day, 0x7af160).
 * - sharp_picture: at once (win_sharp_picture_changed).
 * - the other switches: at the next key, draw or save.
 * The Display mode row says "Next start. Classic suits this screen" in widescreen on a screen less than 800
 * pixels high (win_short_screen, the widescreen hint).
 * save_folder: the Change button of its row opens the computer's own folder picker (host_pick_folder, Q419)
 * and writes the choice to settings.txt; the row then shows the new folder, and the game uses it at the next
 * start. The test variable RECOMP_PICK_FOLDER gives the answer of the picker without a window. When
 * settings.txt cannot be written, the option keeps its old value and its row shows a note. The Open folder button
 * of the row opens the folder in the computer's file manager (host_open_folder, 0.6.1 item 5: players could not
 * find their saves), and a line under the row shows the whole path, with "\" between the parts on Windows. Both
 * use the folder of the next start when the player chose one in this run. Load Game has the same button
 * (file_dialog_build below).
 *
 * Same game (Q152): the box makes its scratch block when it opens the first time, after a click of the
 * player. The side bar item makes its text block at the first side bar build of each game, so each run has
 * it. A switch in the box does not change the game: smoke_run.py --same-save --options compares two
 * switches with two tab clicks.
 *
 * Network tab, 0.7.0 slice 6 (Q38, Q39). Below Online play:
 * - On the start screen and while this game hosts (a single-player game shows Online play only, Q152):
 * - Game password: a text field that hides its text (Display::add_item 0x55f990 with type 0x18 and text flags 0x8c,
 *   as the original's password box at 0x822f56..0x822f9f). It writes into a buffer of the runtime, as the game name
 *   field writes into 0xdd70dc (0x81f243..0x81f287). The box saves it as game_password at the next click in the box
 *   and at Close, and the join check uses it at once (nc_set_host_password).
 * - Human managers online, while this computer hosts a network game (net_session_managers): the name, the club and
 *   a Kick button for each. Kick opens the original's confirm box (confirm_push 0x591840 with 7 arguments, as Exit
 *   Game calls it at 0x764362..0x7643a6), "Kick <name>? Their club goes on holiday.", with No and Yes. Its input
 *   puts its code and the button (1 No, 2 Yes) in 0xdd6be0 and 0xdd6be4 and closes (0x591e7d..0x591e94); the box
 *   below then gets an input with no widget, and Yes kicks (net_session_kick): the original's leave path puts the
 *   club on holiday.
 *
 * Network tab, 0.7.0 slice 7. Between Online play and Game password, outside a single-player game, a Connection
 * test row: the Test button starts
 * the connection test (net_test.c), and the row's text shows "Testing ..." and then its result. The box builds
 * again at its next input with no widget once the test ended. Before a test, the text is this host's place in the
 * lobby with its game code, or a line that says what the test does. The list of human managers online shows each
 * one's ping (nc_ping_ms of the player slot's socket). With many human managers the list rows get closer, so that
 * 15 fit above Close.
 *
 * Node slots: 0 the tab (1 Graphics, 2 Game, 3 Network, 4 Squads), 1 the place that opened the box (1 start screen, 2 game). Only
 * numbers, so the box needs no leave function. */
#include "win.h"
#include "options.h"
#include "dbfolders.h"
#include "net_core.h"
#include "net_errors.h"
#include "season.h"
#include "../host.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CM0102_VERSION
#define CM0102_VERSION "dev"
#endif

extern const uint32_t rt_site_options_item, rt_site_main_poll;

#define SCREENS 0xb74450u           /* the screen stack S */
#define PUSH 0x7fe600u              /* thiscall on S (build, input, flag, leave, exit), ret 0x14 */
#define SET_SLOT 0x7ff270u          /* thiscall on S (slot, value, copy) */
#define NODE_SLOT 0x7ff010u         /* thiscall on S (slot) */
#define BACKGROUND_OFF 0x41c0d0u    /* cdecl (0): as the Game Settings build (0x823a57) */
#define COLOUR 0x5e4800u            /* cdecl (0, green, 0, 0): an RGB565 colour (0x823ac0) */
#define BOX_TEXT 0x803fd0u          /* thiscall on S, 11 args: a text widget with its own rect */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define WIDGET_CREATE 0x55f990u     /* thiscall on a Display, 18 args */
#define CELL_BUTTON 0x8040a0u       /* thiscall on S, 11 args: a button in a cell of an area */
#define CELL_LABEL 0x804100u        /* thiscall on S, 9 args: a label in a cell of an area */
#define HOOK_FN (SCREENS + 0x37eeu) /* the main loop's Game Settings hook and its id (0x7fcd25) */
#define HOOK_ID (SCREENS + 0x37f2u)
#define GAME_SETTINGS_HOOK 0x823930u
#define GAME_SETTINGS_ID 0x423u

#define DISPLAYS 0xb74c4cu          /* the Display of a player slot, 0x300 bytes apart */
#define PLAYER_SLOT 0xb77c7au       /* s16: the player slot that has the turn */
#define WIDGETS 0xba95eu            /* Display + this: widget 0; 0x18c bytes each */
#define WIDGET_SIZE 0x18cu

#define TEXT_YELLOW 0xaebdecu       /* u16 colours of the Game Settings box */
#define TEXT_TITLE 0xaebdeau
#define TEXT_LIGHT 0xae3154u
#define BUTTON_COLOUR 0xae317eu
#define BUTTON_TEXT 0xae3184u

#define VALUE_ID 0x7d3au            /* data: row index * 16 + value index. Ids that the original does not use. */
#define TAB_ID 0x7d3bu              /* data: the tab, 1 to TABS */
#define TABS 4
#define CLOSE_ID 0x7d3cu
#define ITEM_ID 0x4b8u              /* the side bar item; the side bar input 0x7615d0 uses no such id */
#define REBUILD 0xfffffff5u         /* -0xb: build the box again */
#define CLOSE 0xfffffff7u           /* -9: close the box, as Ok and Cancel of Game Settings (0x8259a0) */

#define ROWS 9
#define TEXT_SIZE 0x200u

#define PASSWORD_ID 0x7d60u         /* the game password field */
#define KICK_ID 0x7d61u             /* data: the player slot */
#define TEST_ID 0x7d62u             /* the Test button of the connection test */
#define COPY_ID 0x7d63u             /* the Copy link button of the invite link (0.7.1, #137) */
#define KICK_CODE 0x7d              /* the confirm's code; the original's codes are below 0x40 */
#define CONFIRM_PUSH 0x591840u      /* cdecl (title, heading, text, 1, colour, colour, code) */
#define GAME_TITLE 0x839570u        /* cdecl (): the title text (0x7643a0) */
#define HEADING_COLOUR 0x839590u    /* cdecl (x) (0x76437d) */
#define CONFIRM_CODE 0xdd6be0u
#define CONFIRM_BUTTON 0xdd6be4u
#define FIELD_COLOUR 0xaebe04u      /* the field colour of 0x822f5f */
#define PASSWORD_MAX 40
#define MANAGERS 15
#define LIST_BOTTOM 505             /* the Kick list ends above Close (509) */

typedef enum { ROW_SWITCH, ROW_DISPLAY, ROW_PATH } RowKind;

typedef struct {
    int tab;                        /* 1 Graphics, 2 Game, 3 Network, 4 Squads */
    OptionId id;
    RowKind kind;
    const char *label;
    const char *note;
} Row;

static const Row rows[] = {
    {1, OPT_DISPLAY, ROW_DISPLAY, "Display mode", "Applies at the next start"},
    {1, OPT_SHARP_PICTURE, ROW_SWITCH, "Sharp picture", ""},
    {1, OPT_REMEMBER_WINDOW, ROW_SWITCH, "Remember window", ""},
    {1, OPT_SOUND, ROW_SWITCH, "All sound", "Applies at the next start"},
    {1, OPT_ATTRIBUTE_COLOURS, ROW_SWITCH, "Attribute colours", ""},
    {1, OPT_CONDITION_COLOURS, ROW_SWITCH, "Condition colours", ""},
    {1, OPT_FOREIGN_COLOURS, ROW_SWITCH, "Foreign colours", ""},
    {1, OPT_MATCH_COLUMNS, ROW_SWITCH, "Match columns", "When the tactics or\nmatch screen opens next"},
    {1, OPT_ATTRIBUTE_GROUPS, ROW_SWITCH, "Attribute groups", "When a profile opens next"},
    {2, OPT_SPACE_TO_CONTINUE, ROW_SWITCH, "Space to continue", ""},
    {2, OPT_SHORTCUTS, ROW_SWITCH, "Shortcut keys", ""},
    {2, OPT_CLICKABLE_NAMES, ROW_SWITCH, "Clickable names", ""},
    {2, OPT_SQUAD_VIEWS, ROW_SWITCH, "Team views", ""},
    {2, OPT_FULL_PLAYER_SEARCH, ROW_SWITCH, "Full player search", "From the next game day"},
    {2, OPT_FASTER_DAYS, ROW_SWITCH, "Faster holidays", ""},
    {2, OPT_SAVE_BACKUPS, ROW_SWITCH, "Save backups", ""},
    {2, OPT_CHANGING_RULES, ROW_SWITCH, "Changing rules", "Default for a new game\nSetup Game sets each game"},
    {2, OPT_SAVE_FOLDER, ROW_PATH, "Save folder", "Applies at the next start\nOld saves do not move"},
    {3, OPT_ONLINE_PLAY, ROW_SWITCH, "Online play", "Off: LAN games only"},
    {4, OPT_SQUAD_NUMBERS_99, ROW_SWITCH, "Squad numbers 1 to 99", "New numbers from now on\nNew games keep database numbers"},
};
#define ROW_COUNT ((int)(sizeof rows / sizeof rows[0]))

static const char *const switch_text[] = {"On", "Off"}, *const switch_word[] = {"on", "off"};
static const char *const display_text[] = {"Widescreen", "Classic"};
static const char *const display_word[] = {"widescreen", "classic"};

/* Game memory for the texts and the column weights. The create functions copy each text. */
static uint32_t scratch;
static uint32_t password_field;     /* the buffer of the game password field */
static int kick_slot;               /* the player slot of the open Kick confirm, 0 with none */
static int test_asked;              /* the connection test started while this box is open */
static int test_built;              /* the box was built while the test ran */
static int invite_built;            /* the box was built with the invite link row (the game was in the lobby) */
static int copy_done;               /* the link was copied since the box opened */
static char kick_name[64];
static int failed_row = -1;         /* the row whose last value could not be written to settings.txt */
static int open_failed;             /* the last Open folder of the box could not start the file manager */
static char picked[1100];           /* the save folder chosen in this run, for the next start */

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t call(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t node_slot(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    return call_this(NODE_SLOT, SCREENS, 1, a);
}

static void set_slot(int n, uint32_t value)
{
    uint32_t a[3] = {(uint32_t)n, value, 0};
    call_this(SET_SLOT, SCREENS, 3, a);
}

static uint32_t display(void)
{
    return rd32(g_mem, DISPLAYS + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

static uint32_t text(const char *s)
{
    win_put_str(scratch, s, TEXT_SIZE);
    return scratch;
}

static uint32_t colour16(uint32_t at) { return rd16(g_mem, at); }

/* An area with columns of the given weights (bytes in game memory), one row. */
static int area(int x1, int y1, int x2, int y2, int columns, const uint8_t *weights, uint32_t flags,
                uint32_t colour)
{
    uint32_t w = 0;
    if (weights) {
        w = scratch + TEXT_SIZE;
        memcpy(g_mem + w, weights, (size_t)columns);
    }
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, (uint32_t)columns, w, 1, 0, flags,
                      colour, 0xffffffffu};
    return (int16_t)call_this(AREA_CREATE, display(), 11, a);
}

static void box_text(int x1, int y1, int x2, int y2, uint32_t style, uint32_t colour, uint32_t flags, uint32_t font,
                     uint32_t text_colour, const char *s)
{
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, style, colour, flags, font,
                      text_colour, text(s), 1};
    call_this(BOX_TEXT, SCREENS, 11, a);
}

/* The current value: a yellow label with a frame, as Game Settings (0x82477b). */
static void current(int a, int column, const char *s)
{
    uint32_t y = colour16(TEXT_YELLOW);
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 0x221, y, 0xc, 2, y, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

/* Another value: a green button, as Game Settings (0x8247a4). */
static void choice(int a, int column, const char *s, uint32_t id, uint32_t data, uint32_t green)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 0xc, 2, green, text(s), id, data};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

/* A plain text in a cell: flags 1 left, font 1 small or 3 as the row labels (0x8247d9). */
static void plain(int a, int column, const char *s, uint32_t font, uint32_t colour)
{
    uint32_t l[9] = {(uint32_t)a, (uint32_t)column, 0, 1, 0, 1, font, colour, text(s)};
    call_this(CELL_LABEL, SCREENS, 9, l);
}

/* The characters of font 2 that fit width screen pixels: 7 pixels for each character. Paths measured on
 * the screen take about 6.4 pixels for each character. The box does not call the game's text width 0x5e5ad0 for this: the count of its calls would
 * then depend on the path. Note: the text that the game draws changes later days (Q152), so the path
 * itself does too; it is the same in every game of one player. */
static size_t chars_for(int width) { return (size_t)(width > 0 ? width : 0) / 7u; }

/* A label with a text wider than its cell shows nothing. This cuts the text at the front, with "..." before
 * it, to the characters that fit width screen pixels. */
static void fit_front(const char *s, int width, char *out, size_t room)
{
    size_t n = strlen(s), fit = chars_for(width);
    if (n <= fit || fit < 4) snprintf(out, room, "%s", n <= fit ? s : "");
    else snprintf(out, room, "...%s", s + n - (fit - 3));
}

/* The folder of the saved games: the folder of the next start when the player chose another one in this run, else
 * the folder in use. */
static const char *shown_folder(void) { return picked[0] ? picked : win_save_folder(); }

/* The whole path of that folder in code page 1252, as the system writes it: "\" between the parts on Windows. */
static void folder_text(char *out, size_t room)
{
    char path[1100];
    snprintf(path, sizeof path, "%s", shown_folder());
    if (HOST_FOLDS_CASE) for (char *c = path; *c; c++) if (*c == '/') *c = '\\';
    dbfolders_label(path, out, room);
}

/* Opens the save folder in the file manager. 1 when the file manager started. */
static int open_save_folder(const char *dir)
{
    int ok = host_open_folder(dir);
    fprintf(stderr, "recomp: open folder %s: %s\n", dir, ok ? "started" : "failed");
    return ok;
}

/* The index of the current value of a row: 0 or 1. */
static int current_index(const Row *r)
{
    if (r->kind == ROW_SWITCH) return options_bool(r->id) ? 0 : 1;
    if (r->kind == ROW_DISPLAY) return options_enum(r->id);
    return 0;
}

/* Column weights: the label, three values and the note, as wide as the original's columns allow (Game
 * Settings: 3, 1, 1, 1, 1 at 0x8239ad). A switch row has two values, so its note starts in the third value
 * column. */
static const uint8_t weights_two[] = {38, 17, 17, 48};
static const uint8_t weights_path[] = {38, 17, 22, 43};
#define ROW_X1 160
#define ROW_X2 729

/* The left edge of column c of a row area, in classic numbers. */
static int column_x(const uint8_t *weights, int columns, int c)
{
    int total = 0, before = 0;
    for (int i = 0; i < columns; i++) total += weights[i];
    for (int i = 0; i < c; i++) before += weights[i];
    return ROW_X1 + (ROW_X2 - ROW_X1 + 1) * before / total;
}

static int last_column_x(const uint8_t *weights, int columns) { return column_x(weights, columns, columns - 1); }

/* The note of a row in the last column, in the small font. A "\n" splits it into two lines, for the narrow
 * note columns of classic mode: a label shows one line only. */
static void note(const char *s, const uint8_t *weights, int columns, int y1, int y2, uint32_t colour)
{
    if (!s[0]) return;
    int x = last_column_x(weights, columns) + 4;
    const char *nl = strchr(s, '\n');
    if (!nl) {
        box_text(x, y1, ROW_X2, y2, 1, 0, 1, 1, colour, s);
        return;
    }
    char first[96];
    snprintf(first, sizeof first, "%.*s", (int)(nl - s), s);
    int mid = (y1 + y2) / 2;
    box_text(x, y1 + 1, ROW_X2, mid, 1, 0, 1, 1, colour, first);
    box_text(x, mid + 1, ROW_X2, y2 - 1, 1, 0, 1, 1, colour, nl + 1);
}

static void build_row(const Row *r, int index, int y1, int y2, uint32_t green, uint32_t light)
{
    if (r->kind == ROW_PATH) {
        int a = area(ROW_X1, y1, ROW_X2, y2, 4, weights_path, 1, 0);
        plain(a, 0, r->label, 3, light);
        choice(a, 1, "Change", VALUE_ID, (uint32_t)index * 16u, green);
        choice(a, 2, "Open folder", VALUE_ID, (uint32_t)index * 16u + 1u, green);
        const char *n = index == failed_row ? "Not saved: settings.txt\ncannot be written"
                        : open_failed       ? "The folder could not\nbe opened"
                                            : r->note;
        note(n, weights_path, 4, y1, y2, light);
        /* The whole path on a line under the row, from the label's left edge to the box's right edge. */
        char folder[1100], path[1104];
        folder_text(folder, sizeof folder);
        fit_front(folder, (ROW_X2 - ROW_X1 - 8) * (int)rt_screen_w / 800, path, sizeof path);
        box_text(ROW_X1 + 4, y2 + 1, ROW_X2, y2 + 19, 1, 0, 1, 2, colour16(TEXT_YELLOW), path);
        return;
    }
    int cur = current_index(r);
    int a = area(ROW_X1, y1, ROW_X2, y2, 4, weights_two, 1, 0);
    plain(a, 0, r->label, 3, light);
    for (int i = 0; i < 2; i++) {
        const char *s = r->kind == ROW_SWITCH ? switch_text[i] : display_text[i];
        if (i == cur) current(a, i + 1, s);
        else choice(a, i + 1, s, VALUE_ID, (uint32_t)index * 16u + (uint32_t)i, green);
    }
    const char *n = r->note;
    if (r->id == OPT_DISPLAY && win_short_screen()) n = "Next start. Classic\nsuits this screen";
    note(index == failed_row ? "Not saved: settings.txt\ncannot be written" : n, weights_two, 4, y1, y2, light);
}

/* A small text in two lines at most, split at the space nearest the middle when it is longer than one line. */
static void two_lines(int x1, int y1, int x2, int y2, const char *s, uint32_t colour)
{
    size_t n = strlen(s), per_line = (size_t)(x2 - x1) * rt_screen_w / 800u / 6u;
    if (n <= per_line) {
        box_text(x1, y1, x2, y2, 1, 0, 1, 1, colour, s);
        return;
    }
    size_t cut = n / 2;
    for (size_t d = 0; d < n / 2; d++) {
        if (s[n / 2 + d] == ' ') { cut = n / 2 + d; break; }
        if (s[n / 2 - d] == ' ') { cut = n / 2 - d; break; }
    }
    char first[200];
    snprintf(first, sizeof first, "%.*s", (int)cut, s);
    int mid = (y1 + y2) / 2;
    box_text(x1, y1 + 1, x2, mid, 1, 0, 1, 1, colour, first);
    box_text(x1, mid + 1, x2, y2 - 1, 1, 0, 1, 1, colour, s[cut] == ' ' ? s + cut + 1 : s + cut);
}

/* The Connection test row (0.7.0 slice 7, see the head of this file). */
static void test_row(int y, uint32_t green, uint32_t light)
{
    int a = area(ROW_X1, y, ROW_X2, y + 32, 4, weights_two, 1, 0);
    plain(a, 0, "Connection test", 3, light);
    int ms;
    NetTest t = net_test_state(&ms);
    char line[200];
    if (test_asked && t != NT_NONE) net_test_text(line, sizeof line, t, ms);
    else if (!net_lobby_host_line(line, sizeof line)) snprintf(line, sizeof line, "Checks the lobby and the relay");
    test_built = test_asked && t == NT_RUNNING;
    if (t == NT_RUNNING && test_asked) current(a, 1, "Test");
    else choice(a, 1, "Test", TEST_ID, 0, green);
    two_lines(column_x(weights_two, 4, 2) + 4, y, ROW_X2, y + 32, line, light);
}

/* The invite link row (0.7.1, #137): while this host's game is in the online list, its game code and link, and a
 * Copy link button. Returns the height used. */
static int invite_row(int y, uint32_t green, uint32_t light)
{
    char code[9], link[200], line[260];
    invite_built = net_lobby_invite(code, sizeof code, link, sizeof link);
    if (!invite_built) {
        copy_done = 0;
        return 0;
    }
    int a = area(ROW_X1, y, ROW_X2, y + 32, 4, weights_two, 1, 0);
    plain(a, 0, "Invite link", 3, light);
    if (copy_done) current(a, 1, "Copied");
    else choice(a, 1, "Copy link", COPY_ID, 0, green);
    snprintf(line, sizeof line, "Game code %s  %s", code, link);
    two_lines(column_x(weights_two, 4, 2) + 4, y, ROW_X2, y + 32, line, light);
    return 35;
}

/* The Copy link button: the link goes to the clipboard. */
static void copy_link(void)
{
    char code[9], link[200];
    if (!net_lobby_invite(code, sizeof code, link, sizeof link)) return;
    copy_done = !SDL_SetClipboardText(link);
    fprintf(stderr, "recomp: options box: Copy link %s %s\n", link, copy_done ? "copied" : "failed");
}

/* The Network tab below its switch rows (see the head of this file). */
static void network_rows(int y, uint32_t green, uint32_t light)
{
    /* A single-player game shows only the Online play row, as slice 4 did: the rows below changed the game's memory
     * layout and with it later days (Q152, smoke_run.py --same-save --options, run options-network, 2026-10-28; with
     * the field alone left out the run still failed). The Connection test row is one of them (slice 7): the start
     * screen's box and a host's box show it. */
    invite_built = 0;
    if (season_game_on() && !net_session_hosting()) return;
    test_row(y, green, light);
    y += 35;
    y += invite_row(y, green, light);
    if (!password_field) password_field = win_static_alloc(PASSWORD_MAX + 8);   /* not the game heap (Q152) */
    snprintf((char *)g_mem + password_field, PASSWORD_MAX + 1, "%s", options_string(OPT_GAME_PASSWORD));
    int a = area(ROW_X1, y, ROW_X2, y + 32, 4, weights_two, 1, 0);
    plain(a, 0, "Game password", 3, light);
    uint32_t w[18] = {0x18, (uint32_t)column_x(weights_two, 4, 1) + 2, (uint32_t)y + 2,
                      (uint32_t)column_x(weights_two, 4, 3) - 4, (uint32_t)y + 30, 0, 0, 0x22, colour16(FIELD_COLOUR),
                      0, 0x8c, 2, colour16(BUTTON_TEXT), password_field, PASSWORD_MAX, PASSWORD_ID, 0, 0xffffffffu};
    call_this(WIDGET_CREATE, display(), 18, w);
    note("Empty: no password", weights_two, 4, y, y + 32, light);
    y += 42;
    box_text(ROW_X1, y, ROW_X2, y + 22, 1, 0, 1, 3, light, "Human managers online");
    y += 26;
    NetManager m[MANAGERS];
    int n = net_session_managers(m, MANAGERS);
    if (!net_session_hosting()) {
        box_text(ROW_X1, y, ROW_X2, y + 18, 1, 0, 1, 1, light, "Shown while this computer hosts a network game");
        return;
    }
    if (!n) box_text(ROW_X1, y, ROW_X2, y + 18, 1, 0, 1, 1, light, "None");
    static const uint8_t weights_kick[] = {40, 32, 12, 12};
    int step = n ? (LIST_BOTTOM - y) / n : 15;
    if (step > 15) step = 15;
    for (int i = 0; i < n; i++, y += step) {
        int r = area(ROW_X1, y, ROW_X2, y + step - 1, 4, weights_kick, 1, 0);
        plain(r, 0, m[i].name[0] ? m[i].name : "New manager", 1, light);
        plain(r, 1, m[i].club, 1, light);
        int ms = net_session_ping(m[i].slot);
        char ping[16];
        if (ms >= 0) snprintf(ping, sizeof ping, "%d ms", ms);
        else snprintf(ping, sizeof ping, "...");
        fprintf(stderr, "recomp: options box: %s ping %s\n", m[i].name[0] ? m[i].name : "New manager", ping);
        plain(r, 2, ping, 1, light);
        uint32_t b[11] = {(uint32_t)r, 3, 0, 1, 0, 0xc, 1, green, text("Kick"), KICK_ID, (uint32_t)m[i].slot};
        call_this(CELL_BUTTON, SCREENS, 11, b);
    }
}

/* Saves the game password field when it changed. */
static void save_password(void)
{
    if (!password_field) return;
    char typed[PASSWORD_MAX + 1];
    snprintf(typed, sizeof typed, "%s", (const char *)g_mem + password_field);
    if (!strcmp(typed, options_string(OPT_GAME_PASSWORD))) return;
    int ok = options_set(OPT_GAME_PASSWORD, typed);
    nc_set_host_password(options_string(OPT_GAME_PASSWORD));
    fprintf(stderr, "recomp: option game_password %s (%d)\n", typed[0] ? "set" : "cleared", ok);
}

/* Kick: the original's confirm box over the Options box. */
static void ask_kick(int slot)
{
    NetManager m[MANAGERS];
    int n = net_session_managers(m, MANAGERS);
    kick_slot = 0;
    for (int i = 0; i < n; i++)
        if (m[i].slot == slot) {
            kick_slot = slot;
            snprintf(kick_name, sizeof kick_name, "%s", m[i].name);
        }
    if (!kick_slot) return;
    static uint32_t heading, question;
    if (!heading) {
        heading = win_static_alloc(32);
        question = win_static_alloc(160);
        win_put_str(heading, "Please Confirm", 32);
    }
    char q[160];
    int managers = 0;
    for (int i = 0; i < n; i++)
        if (m[i].slot == slot) managers = m[i].managers;
    if (managers > 1) snprintf(q, sizeof q, "Kick %s? Their clubs go on holiday.", kick_name);
    else if (managers == 1) snprintf(q, sizeof q, "Kick %s? Their club goes on holiday.", kick_name);
    else snprintf(q, sizeof q, "Kick this new human manager? They have no club yet.");
    win_put_str(question, q, 160);
    uint32_t one[1] = {1}, zero[1] = {0};
    uint32_t args[7] = {call(GAME_TITLE, 0, NULL), heading, question, 1, call(HEADING_COLOUR, 1, one),
                        call(HEADING_COLOUR, 1, zero), KICK_CODE};
    call(CONFIRM_PUSH, 7, args);                /* its result is not a status (0x59192a..0x591938) */
    fprintf(stderr, "recomp: options box: %s\n", q);
}

/* The answer of the Kick confirm, at the box's input with no widget. 1 when there was one. */
static int kick_answer(void)
{
    if (rd32(g_mem, CONFIRM_CODE) != KICK_CODE) return 0;
    uint32_t button = rd32(g_mem, CONFIRM_BUTTON);
    wr32(g_mem, CONFIRM_CODE, 0);
    wr32(g_mem, CONFIRM_BUTTON, 0);
    if (button == 2 && kick_slot) {
        NetManager m[MANAGERS];
        int n = net_session_managers(m, MANAGERS), same = 0;
        for (int i = 0; i < n; i++) same |= m[i].slot == kick_slot && !strcmp(m[i].name, kick_name);
        fprintf(stderr, "recomp: options box: Kick %s: %s\n", kick_name, same ? "yes" : "gone already");
        if (same) net_session_kick(kick_slot);
    } else {
        fprintf(stderr, "recomp: options box: Kick %s: no\n", kick_name);
    }
    kick_slot = 0;
    return 1;
}

/* The build of the box: cdecl, no arguments. The frame follows 0x823950. */
static void options_build(void)
{
    if (!scratch) scratch = rt_alloc_zero(TEXT_SIZE + 16);
    int tab = (int)node_slot(0);
    if (tab < 1 || tab > TABS) {
        tab = 1;
        set_slot(0, 1);
    }
    area(0, 0, 0x31f, 0x257, 1, NULL, 2, 0);                    /* the dark cover, 0x823a92 */
    uint32_t off[1] = {0};
    call(BACKGROUND_OFF, 1, off);
    uint32_t c[4] = {0, 0x38, 0, 0};
    uint32_t box_colour = call(COLOUR, 4, c) & 0xffff;
    uint32_t g[4] = {0, 0xc0, 0, 0};
    uint32_t green = call(COLOUR, 4, g) & 0xffff;              /* the value buttons, 0x823979 */
    uint32_t light = colour16(TEXT_LIGHT);
    area(145, 50, 744, 549, 1, NULL, 0x30, box_colour);
    box_text(155, 60, 734, 105, 0x30, colour16(TEXT_TITLE), 0xc, 4, colour16(BUTTON_TEXT), "Bosman Edition Options");
    box_text(165, 115, 420, 135, 1, 0, 1, 1, light, "Bosman Edition " CM0102_VERSION);
    box_text(490, 115, 724, 135, 1, 0, 1, 1, light, "Changes save at once to settings.txt");

    /* The tabs: the open tab as a framed label, the other as a button. */
    /* Squads (tab 4) stands second, so the clicks at x 304, 584 and 680 still reach Graphics, Game and Network. */
    static const uint8_t weights_tabs[TABS] = {8, 7, 7, 6};
    static const uint8_t tab_of_column[TABS] = {1, 4, 2, 3};
    int t = area(165, 142, 724, 168, TABS, weights_tabs, 1, 0);
    static const char *const tab_names[TABS] = {"Graphics", "Squads", "Game", "Network"};
    for (int i = 0; i < TABS; i++) {
        if (tab_of_column[i] == tab) current(t, i, tab_names[i]);
        else choice(t, i, tab_names[i], TAB_ID, tab_of_column[i], green);
    }

    int y = 176;
    for (int i = 0; i < ROW_COUNT; i++) {
        if (rows[i].tab != tab) continue;
        build_row(&rows[i], i, y, y + 32, green, light);
        y += 35;
    }

    if (tab == 3) network_rows(y, green, light);

    int close = area(155, 509, 734, 539, 1, NULL, 1, 0);
    uint32_t b[11] = {(uint32_t)close, 0, 0, 0x30, colour16(BUTTON_COLOUR), 0xc, 3, colour16(BUTTON_TEXT),
                      text("Close"), CLOSE_ID, 0};
    call_this(CELL_BUTTON, SCREENS, 11, b);
    fprintf(stderr, "recomp: options box built, tab %d\n", tab);
    rt_ret_cdecl(0);
}

/* Sets the value of a row from a click, and applies what can apply at once. When settings.txt cannot be
 * written, the old value goes back into memory and the row shows a note. */
/* The Change button of the Save folder row: the folder picker, then save_folder in settings.txt. */
static void pick_save_folder(int index)
{
    char dir[1100];
    const char *test = getenv("RECOMP_PICK_FOLDER");
    int got = test ? (snprintf(dir, sizeof dir, "%s", test), *test ? 1 : 0)
                   : host_pick_folder("Choose the folder for saved games", dir, sizeof dir);
    if (got <= 0) {
        fprintf(stderr, "recomp: save folder picker: %s\n", got < 0 ? "none on this computer" : "cancelled");
        return;
    }
    char old[1100];
    snprintf(old, sizeof old, "%s", options_string(OPT_SAVE_FOLDER));
    int ok = options_set(OPT_SAVE_FOLDER, dir);
    if (!ok) {
        fprintf(stderr, "recomp: option save_folder = %s refused\n", dir);
        return;
    }
    if (ok < 0) {
        options_set(OPT_SAVE_FOLDER, old);
        failed_row = index;
        fprintf(stderr, "recomp: option save_folder = %s not saved, kept %s\n", dir, old);
        return;
    }
    failed_row = -1;
    snprintf(picked, sizeof picked, "%s", dir);
    fprintf(stderr, "recomp: option save_folder = %s (%d), from the next start\n", dir, ok);
}

static void apply(const Row *r, int index, int value)
{
    if (r->kind == ROW_PATH) {
        if (value == 1) {
            open_failed = !open_save_folder(shown_folder());
            return;
        }
        open_failed = 0;
        pick_save_folder(index);
        return;
    }
    const char *word = NULL;
    if (r->kind == ROW_SWITCH && value < 2) word = switch_word[value];
    if (r->kind == ROW_DISPLAY && value < 2) word = display_word[value];
    if (!word) return;
    char old[1100];
    snprintf(old, sizeof old, "%s", options_string(r->id));
    int ok = options_set(r->id, word);
    if (ok < 0) {
        options_set(r->id, old);        /* the file cannot be written: only the memory changes back */
        failed_row = index;
        fprintf(stderr, "recomp: option %s = %s not saved, kept %s\n", options_name(r->id), word, old);
        return;
    }
    failed_row = -1;
    fprintf(stderr, "recomp: option %s = %s (%d)\n", options_name(r->id), word, ok);
    if (r->id == OPT_SHARP_PICTURE) win_sharp_picture_changed();
}

/* The input of the box: cdecl (s16 widget). Only the box's own widgets: the box covers the screen. */
static uint32_t handled_press = 0xffffffffu;   /* the mouse press of the last click that the box handled */

static void options_input(void)
{
    int16_t w = (int16_t)rt_arg(0);
    uint32_t r = 0;
    if (w < 0 && kick_answer()) {
        handled_press = win_press_count();          /* a held Yes or No does not click in the box below */
        rt_ret_cdecl(REBUILD);
        return;
    }
    if (w < 0 && node_slot(0) == 3 && (!season_game_on() || net_session_hosting())) {   /* as network_rows */
        char c[9], l[200];
        if (net_lobby_invite(c, sizeof c, l, sizeof l) != invite_built) {   /* the game went into the lobby, or out */
            save_password();                        /* the build puts the saved text back in the field */
            rt_ret_cdecl(REBUILD);
            return;
        }
    }
    if (w < 0 && test_built && net_test_state(NULL) != NT_RUNNING) {
        save_password();
        rt_ret_cdecl(REBUILD);                      /* the connection test ended: show its result */
        return;
    }
    /* A held button gives its widget again after 400 ms of real time (the repeat of the poll, user32.c). After
     * a build the same widget number can be another button, and the count of builds would depend on the
     * speed of the machine. So the box takes one click for each press of the mouse button. */
    if (w >= 0 && win_press_count() == handled_press) w = -1;
    if (w >= 0) {
        uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
        uint32_t id = (uint16_t)rd32(g_mem, at), data = rd32(g_mem, at + 0x40);
        /* A click that ends the text entry gives the field, then the button under the pointer in the next poll of
         * the same press (research note, section 11): the field does not take the press. */
        if (id != PASSWORD_ID) handled_press = win_press_count();
        save_password();
        if (id == TEST_ID) {
            test_asked = 1;
            net_lobby_test_start();
            r = REBUILD;
        } else if (id == COPY_ID) {
            copy_link();
            r = REBUILD;
        } else if (id == KICK_ID) {
            ask_kick((int)data);
            r = kick_slot ? 0xfffffffcu : REBUILD;    /* -4: build again with the confirm on top (0x7643b1) */
        } else if (id == CLOSE_ID) {
            r = CLOSE;
        } else if (id == TAB_ID && data >= 1 && data <= TABS) {
            set_slot(0, data);
            r = REBUILD;
        } else if (id == VALUE_ID && data / 16 < (uint32_t)ROW_COUNT) {
            apply(&rows[data / 16], (int)(data / 16), (int)(data % 16));
            r = REBUILD;
        }
    }
    rt_ret_cdecl(r);
}

/* The Open folder button of Load Game (0.6.1 item 5). Load Game is the original's file dialog: open_file_dialog
 * 0x58f500 pushes the build 0x58f7c0 and the input 0x58ff80 and keeps its arguments in node slots, the mode in
 * slot 2 (0x58f606) and the routine of Ok in slot 5 (0x58f647). Restore Saved Game (start screen, 0x81ca9a) and
 * Restore Network Game (0x81f837) open it with mode 0 and the load routine 0x838e10; Save Game has mode 1, Delete
 * Saved Game mode 2. The build lays out the File row (190, 195)-(700, 231), the list of six saves (190, 234)-(700,
 * 510) and the row of Cancel, Info and Ok (100, 555)-(790, 590), so the button goes in the free space under the
 * list, at its left edge, in the style of Cancel (0x58fd46). The text goes through the game's own text buffer
 * 0xde1f64, as the build's own buttons do (0x58fd09): the create function copies it, and the button needs no
 * block of its own (Q152: nothing is allocated before a load). The input runs the original unless the widget is
 * the button. */
#define FILE_DIALOG_MODE_SLOT 2
#define FILE_DIALOG_OK_SLOT 5
#define LOAD_ROUTINE 0x838e10u
#define BUTTON_TEXT_BUFFER 0xde1f64u
#define LOAD_OPEN_ID 0x7d3du            /* an id that the original does not use */

static int is_load_dialog(void)
{
    return node_slot(FILE_DIALOG_MODE_SLOT) == 0 && node_slot(FILE_DIALOG_OK_SLOT) == LOAD_ROUTINE;
}

void o_0058f7c0(void);
void o_0058ff80(void);

RT_WRAP(0058f7c0, file_dialog_build)
{
    o_0058f7c0();
    if (rt_unwinding || !is_load_dialog()) return;
    Cpu saved = cpu;
    int a = area(190, 517, 360, 547, 1, NULL, 1, 0);
    win_put_str(BUTTON_TEXT_BUFFER, "Open folder", 0x40);
    uint32_t b[11] = {(uint32_t)a, 0, 0, 0x30, colour16(BUTTON_COLOUR), 0xc, 3, colour16(BUTTON_TEXT),
                      BUTTON_TEXT_BUFFER, LOAD_OPEN_ID, 0};
    call_this(CELL_BUTTON, SCREENS, 11, b);
    cpu = saved;
}

static uint32_t load_handled_press = 0xffffffffu;

RT_WRAP(0058ff80, file_dialog_input)
{
    int16_t w = (int16_t)rt_arg(0);
    if (w >= 0 && w < 1200 && rd16(g_mem, display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE) == LOAD_OPEN_ID) {
        if (win_press_count() != load_handled_press) {     /* a held button repeats: one press, one window */
            load_handled_press = win_press_count();
            open_save_folder(win_save_folder());
        }
        rt_ret_cdecl(0);
        return;
    }
    o_0058ff80();
}

uint32_t options_screen_build_fn(void) { return rt_host_fn("options_build", options_build); }

int options_screen_open(int from)
{
    uint32_t p[5] = {options_screen_build_fn(), rt_host_fn("options_input", options_input), 1, 0, 0};
    if (!call_this(PUSH, SCREENS, 5, p)) {          /* as 0x8238c0: no node, no slots */
        fprintf(stderr, "recomp: options box not opened\n");
        return 0;
    }
    set_slot(0, 1);
    set_slot(1, (uint32_t)from);
    failed_row = -1;
    test_asked = test_built = 0;
    copy_done = 0;
    open_failed = 0;
    handled_press = win_press_count();              /* the press that opened the box does not click in it */
    fprintf(stderr, "recomp: options box opened from %s\n", from == 1 ? "the start screen" : "the side bar");
    return 1;
}

/* The hook that the main loop calls for the item, cdecl (0), as 0x823930. The main loop then builds the
 * screen on top (-4 at 0x7fcd34). */
static void options_hook(void)
{
    wr32(g_mem, HOOK_FN, GAME_SETTINGS_HOOK);
    wr32(g_mem, HOOK_ID, GAME_SETTINGS_ID);
    options_screen_open(2);
    rt_ret_cdecl(0);
}

void options_screen_after_poll(uint32_t ret, uint32_t display_of_poll, uint32_t widget)
{
    if (ret != rt_site_main_poll || widget >= 1200) return;
    if (rd16(g_mem, display_of_poll + WIDGETS + 8 + widget * WIDGET_SIZE) != ITEM_ID) return;
    wr32(g_mem, HOOK_FN, rt_host_fn("options_hook", options_hook));
    wr32(g_mem, HOOK_ID, ITEM_ID);
}

/* The side bar item. */
static uint32_t item_args[18];
static uint32_t item_display;

int options_screen_widget_before(void)
{
    if (rd32(g_mem, cpu.esp) != rt_site_options_item) return 0;
    for (int i = 0; i < 18; i++) item_args[i] = rt_arg(i);
    item_display = cpu.ecx;
    return 1;
}

/* After the Game Settings item: one more item with the same arguments, but with its own text and id. The
 * text block is made at the first side bar build of each game, so each run has it. */
void options_screen_widget_after(void)
{
    static uint32_t label;
    if (!label) label = rt_alloc_str("Bosman Edition Options");
    uint32_t args[18];
    memcpy(args, item_args, sizeof args);
    args[13] = label;
    args[15] = ITEM_ID;
    uint32_t eax = cpu.eax;
    call_this(WIDGET_CREATE, item_display, 18, args);
    cpu.eax = eax;
}
