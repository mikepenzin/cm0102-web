/* The Choose Season screen (0.6.0 slice 13; spec docs/specs/2026-10-16-seasons-0.6.0-spec.md, "Seasons and
 * databases", "Eras", "Changing rules", "Where it shows").
 *
 * The layout is "A3 + both" of docs/research/mockups/2026-10-15-season-picker.html with five eras (Q240,
 * Q261): one coloured heading per era, the tiles of its seasons under it in the era colour with a coloured
 * top edge, a status tag on each tile and a star on each season that has a saved game. A Custom database
 * tile follows the seasons (Q254). Below the tiles the strip shows the chosen season, its database, its
 * credit and the start date. Each season has one database, the one of its tile (Q417, Max's play test of the
 * 0.6.0 preview): the version choice of Q252 and Q257 is gone. A line still links to the Update Team's
 * Databases forum at champman0102.co.uk (Q418). At the foot of the strip, the changing rules switch of the
 * next new game (Q232, Q259; on Setup Game until Max's play test).
 *
 * The screen is a screen of the runtime on the game's own screen stack, as Team views (views.c): the
 * Choose Season item of Setup Game (database.c) pushes it with the push of the original 0x7fe600, flag 0,
 * as the start screen's own items push their screens (0x81bd70, inventory 6c). Its build and input are host
 * functions (rt_host_fn). The frame copies the start screen 0x81bab0: the picture of the start screen (its
 * node title, through background_set_picture 0x41c0d0), the heading 0x803f30, the side bar 0x75db00 and the
 * Back bar 0x5edcc0. Tiles and labels are widgets of the original (Display::add_item 0x55f990,
 * ScreenManager::add_text_item 0x803fd0). The top edges, the frame of the chosen tile, the stars and the
 * era headings are drawn by the runtime after the game draws their widgets (draw.c, from the widget draw
 * wrap in names.c through views_widget_drawn), as the pitch of Team views.
 *
 * A click on a tile chooses it for the strip. "Choose" starts it: the season database comes from
 * seasons_db_prepare (slice 14), which unpacks a zipped database once into Databases. Before the unpack the
 * screen is built once more with a line that says so, and the unpack runs at the next input call. Then the
 * options database and season get the choice, season_set_new_game gets the start year and the screen goes
 * back to Setup Game, whose top row shows the season, its database and the changing rules state.
 *
 * Same game (Q152): the screen keeps its texts in runtime memory (win_static_alloc), not in the game's heap.
 * It reads files only: settings.txt changes through options_set. The push of the node is the original's.
 *
 * Node slots: 0 the chosen tile (1 to 25 for the seasons, 26 for the Custom database), 2 the Custom folder,
 * 3 the season of the Custom folder (start year), 4 the state (0, 1 unpack next, 2 error). Slots 1 and 5
 * held the version and its page until Q417 and stay 0. Only numbers, so the screen needs no leave
 * function. */
#include "win.h"
#include "draw.h"
#include "options.h"
#include "season.h"
#include "seasons_db.h"
#include "season_screen.h"
#include "../host.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREENS 0xb74450u           /* the screen stack S */
#define TOP_NODE (SCREENS + 0x7d4u + 0x0eu)
#define PUSH 0x7fe600u              /* thiscall on S (build, input, flag, leave, exit), ret 0x14 */
#define SET_SLOT 0x7ff270u          /* thiscall on S (slot, value, copy) */
#define NODE_SLOT 0x7ff010u         /* thiscall on S (slot) */
#define CANNOT_GO_BACK 0x7fec60u    /* thiscall on S (any): 0 when there is a screen to go back to */
#define SET_PICTURE 0x41c0d0u       /* cdecl (path): background_set_picture */
#define HEADING 0x803f30u           /* thiscall on S (bar colour, text colour, text) */
#define SIDE_BAR 0x75db00u          /* cdecl (mode, arg) */
#define BACK_BAR 0x5edcc0u          /* cdecl (back, next) */
#define AREA_CREATE 0x55fba0u       /* thiscall on a Display, 11 args */
#define WIDGET_CREATE 0x55f990u     /* thiscall on a Display, 18 args */
#define BOX_TEXT 0x803fd0u          /* thiscall on S, 11 args: a text widget with its own rect */
#define SIDE_BAR_INPUT 0x7615d0u    /* cdecl (widget): 0 when it does not handle the click */
#define GAME_INPUT 0x7643d0u        /* cdecl (widget): the game's own actions, Back among them */

#define DISPLAYS 0xb74c4cu          /* the Display of a player slot, 0x300 bytes apart */
#define PLAYER_SLOT 0xb77c7au       /* s16: the player slot that has the turn */
#define WIDGETS 0xba95eu            /* Display + this: widget 0; 0x18c bytes each */
#define WIDGET_SIZE 0x18cu
#define MAX_WIDGETS 1200
#define NO_DRAW 0xaebe0cu           /* not 0: the draw functions of the original draw nothing */
#define DRAW_SURFACE 0xaebde4u      /* the surface that the draw functions of the original draw on */
#define NODE_PREV 0x1f4u            /* screen node: the node before it */
#define NODE_TITLE 0x1fcu           /* screen node: the title, the picture of the screen (260 bytes) */

#define TEXT_YELLOW 0xaebdecu       /* u16 colours of the original (include/gui_utils.h) */
#define TEXT_LIGHT 0xae3184u
#define HEADING_RED 0xae31a8u
#define BUTTON_GREY 0xae317eu

#define TILE_ID 0x7d40u             /* data: tile index 0 to 25. Ids that the original does not use. */
#define RULES_ID 0x7d41u            /* data: 1 changing rules on, 0 off (the version choice until Q417) */
#define CHOOSE_ID 0x7d42u
#define FOLDER_ID 0x7d43u           /* the next Custom folder */
#define CUSTOM_YEAR_ID 0x7d44u      /* data: 0 an earlier season, 1 a later one */
#define LINK_ID 0x7d45u
#define BACK_ID 0x7d47u             /* Back: the screen goes back to Setup Game */
#define REBUILD 0xfffffff5u         /* -0xb */
#define GO_BACK 0xfffffffeu         /* -2: the result of Back (0x762c97) */

#define CUSTOM_TILE SEASONS_COUNT   /* tile index of the Custom database */
#define TILES (SEASONS_COUNT + 1)
#define TEXT_ROOM 0x200u
#define TEXTS 8                     /* text buffers in a row: a create copies its text, but a few are kept */

#define LINK_URL "https://champman0102.co.uk/viewforum.php?f=73"

/* The credit of each database (Q147, Q269, Q340): the official data sets credit Sports Interactive, and every
 * Update Team database, October 2003 and September 2005 included, credits the Update Team. Our own season
 * databases Summer 2004 and Summer 2006 (Q351) take their facts from Football Manager and their ratings from the Update Team.
 * Summer 2002 takes its facts from SI's Xbox data of Championship Manager: Season 02/03 and its ratings from 3.9.68
 * (Summer 2002 spec, Q13; the text is Max's, Q44).
 * Never a person's name in the code. One table, so a later change is one line. */
typedef struct { const char *name; const char *credit; } Credit;
static const Credit credits[] = {
    {"3.9.68", "Data by Sports Interactive"},
    {"3.9.60", "Data by Sports Interactive"},
    {"3.9.65", "Data by Sports Interactive"},
    {"Summer 2004", "Bosman Edition database, ratings by the CM 01/02 Update Team"},
    {"Summer 2006", "Bosman Edition database, ratings by the CM 01/02 Update Team"},
    {"Summer 2002", "Bosman Edition database, data from CM 02/03"},
};
#define CREDIT_UPDATE_TEAM "Database by the CM 01/02 Update Team"
#define CREDIT_CUSTOM "Your own database"

const char *season_screen_credit(const char *name)
{
    for (size_t i = 0; i < sizeof credits / sizeof credits[0]; i++)
        if (!strcmp(credits[i].name, name)) return credits[i].credit;
    return CREDIT_UPDATE_TEAM;
}

/* ---- Layout C (Max, Q346): three decade rows, ten columns by the last digit of the start year ----
 *
 * Every number of the look is in this block, as screen pixels of each display mode (the mockups
 * docs/research/mockups/2026-10-17-season-screen-c/GridClassic.dc.html and GridWide.dc.html). The widget create
 * of the game scales classic numbers (800 x 600) to the screen (display.c), so px() and py() turn a pixel of the
 * mode into a classic number first. Texts use the game's own fonts: each text takes the largest font of its list
 * that fits its room, measured with the game's text width 0x5e5ad0, and long texts wrap at spaces. */

typedef struct {
    int w, h;                           /* the screen of the mode */
    int left, right;                    /* the content between the side bar and the right edge */
    int grid_x, grid_y;                 /* the first heading bar */
    int tile_w, tile_h, tile_gap;       /* ten columns of tiles */
    int bar_h, bar_gap, row_gap;        /* the heading bar of a decade, the room under it and between rows */
    int star_pad;                       /* the room above the year, where the star sits */
    int year_h, tag_h, tag_gap;         /* the year line, the tag box and the room between them */
    int star_r;
    uint8_t year_fonts[3], title_fonts[3], name_fonts[3], text_fonts[2];
    int side;                           /* 1: the details are a panel on the right of the grid (widescreen) */
    int det_x1, det_y1, det_x2, det_y2, pad;   /* the details: the strip (classic) or the panel (widescreen) */
    int left_w;                         /* classic: the column of the year in the strip */
    int button_h;                       /* the season buttons of the details */
    int link_y, link_h;                 /* the forum line; its height is the height of the changing rules row */
    int bottom_y, bottom_h, start_w;    /* Back and Start */
} Layout;

static const Layout layout_classic = {
    800, 600, 100, 790,
    100, 66, 64, 60, 5, 20, 3, 10,
    15, 18, 14, 4, 5,
    {3, 2, 1}, {7, 6, 4}, {3, 2, 1}, {1, 1},
    0, 100, 352, 790, 518, 12, 130, 22,
    524, 26,
    555, 36, 230,
};

static const Layout layout_wide = {
    1280, 800, 160, 1264,
    160, 98, 103, 92, 8, 26, 4, 10,
    26, 30, 20, 8, 7,
    {6, 4, 3}, {7, 6, 4}, {6, 5, 4}, {3, 2},
    0, 160, 498, 1264, 698, 16, 210, 28,
    704, 30,
    740, 47, 330,
};

static const Layout *lay(void) { return rt_screen_w >= 1280 ? &layout_wide : &layout_classic; }

/* A pixel of the mode as a classic number, for the widget create. */
static int px(int x) { return x * 800 / (int)lay()->w; }
static int py(int y) { return y * 600 / (int)lay()->h; }

/* The height in pixels that a line of each font takes (fonts 1 to 7 of the game). */
static int line_h(uint32_t font)
{
    static const int h[8] = {14, 14, 17, 20, 25, 30, 34, 42};
    return font < 8 ? h[font] : 20;
}

typedef struct { uint8_t r, g, b; } Rgb;

/* The decades (Q346): the colour of the bar, a lighter colour for text on the dark details and the run of
 * seasons. The names come from seasons_db_decade_name (seasons_db.c): one table for every screen. */
typedef struct { Rgb bar, text; const char *range; } DecadeLook;
static const DecadeLook decade_look[3] = {
    {{224, 138, 0}, {255, 184, 74}, "2001/02 to 2009/10"},
    {{44, 154, 68}, {111, 216, 138}, "2010/11 to 2019/20"},
    {{47, 98, 224}, {142, 170, 255}, "2020/21 to 2025/26"},
};
static const Rgb custom_bar = {138, 138, 160};

/* The European cups of a season, for the details (the mockup's europe()). */
static const char *europe_text(int y)
{
    if (y <= 2002) return "Champions League with two group stages, UEFA Cup, Intertoto Cup";
    if (y == 2003) return "Champions League, UEFA Cup, Intertoto Cup";
    if (y <= 2008) return "Champions League, UEFA Cup with groups, Intertoto Cup";
    if (y <= 2020) return "Champions League, Europa League";
    if (y <= 2023) return "Champions League, Europa League, Conference League";
    return "Champions League, Europa League, Conference League, each with a 36-club league phase";
}

/* The tags of the tiles: text and box colours. */
typedef struct { const char *text; Rgb box, ink; int boxed; } Tag;
static const Tag tag_official = {"Official", {217, 168, 0}, {26, 18, 0}, 1};
static const Tag tag_installed = {"Installed", {42, 154, 68}, {255, 255, 255}, 1};
static const Tag tag_included = {"Included", {58, 62, 104}, {230, 233, 255}, 1};
static const Tag tag_missing = {"Missing", {128, 32, 32}, {255, 255, 255}, 1};
static const Tag tag_none = {"No database", {40, 40, 52}, {154, 154, 171}, 0};

static const Rgb yellow_rgb = {245, 224, 0}, navy_rgb = {14, 18, 66}, white_rgb = {255, 255, 255};
static const Rgb pale_rgb = {214, 220, 255}, grey_text = {138, 138, 156}, greyed_box = {40, 40, 52};
static const Rgb start_ink = {26, 20, 0}, error_rgb = {255, 120, 100}, rule_rgb = {80, 84, 130};

static uint16_t rgb565(Rgb c) { return (uint16_t)(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3)); }

/* c over the navy of the tiles with weight w of 256: the tint of a tile. */
static Rgb mix(Rgb c, int w)
{
    Rgb n = navy_rgb;
    return (Rgb){(uint8_t)((c.r * w + n.r * (256 - w)) / 256), (uint8_t)((c.g * w + n.g * (256 - w)) / 256),
                 (uint8_t)((c.b * w + n.b * (256 - w)) / 256)};
}

/* ---- What the screen shows: filled when the screen opens and after an unpack ---- */
static SeasonsList list;
static int starred[SEASONS_COUNT];  /* 1 when a saved game of the save folder starts in that season */
static char error_text[240];

static void scan_saves(void)
{
    memset(starred, 0, sizeof starred);
    const char *dir = win_save_folder();
    HostDir *d = host_opendir(dir);
    if (!d) return;
    for (const char *n; (n = host_readdir(d));) {
        size_t len = strlen(n);
        if (len < 5 || strcasecmp(n + len - 4, ".sav")) continue;
        char path[2048];
        snprintf(path, sizeof path, "%s/%s", dir, n);
        int year;
        if (season_of_save(path, &year) >= 0 && year >= SEASONS_FIRST && year < SEASONS_FIRST + SEASONS_COUNT)
            starred[year - SEASONS_FIRST] = 1;
    }
    host_closedir(d);
}

/* ---- The season of the next new game, from the options database and season ---- */

static int option_season(void) { return SEASONS_FIRST + options_enum(OPT_SEASON); }

/* The known database of a folder of Databases, or NULL (a Custom database or no such folder). *exists gets 1
 * when the folder is there and usable. */
static const SeasonKnownDb *folder_db(const char *name, int *exists)
{
    char path[2400];
    *exists = dbfolders_path(win_databases_root(), name, path, sizeof path);
    return *exists ? seasons_db_identify(path) : NULL;
}

int season_screen_year(void)
{
    if (!season_code_on()) return SEASONS_FIRST;
    const char *db = options_string(OPT_DATABASE);
    int opt = option_season();
    /* 3.9.68 is the data of the 2001/02 tile. Before Summer 2002 it was the data of the 2002/03 tile too, so the
     * option values of such a choice (database = 3.9.68, season = 2002/03) still start 3.9.68 in 2002/03. */
    int official = opt == 2002 ? 2002 : 2001;
    if (strncmp(db, "folder:", 7)) return strcmp(db, "3.9.68") ? 2001 : official;
    int exists;
    const SeasonKnownDb *k = folder_db(db + 7, &exists);
    if (!exists) return 2001;                            /* files.c then loads 3.9.68 */
    if (!k) return opt;                                  /* a Custom database: the season the player named */
    if (k->role == SEASONS_ROLE_OFFICIAL_BASE) return official;
    if (k->role == SEASONS_ROLE_SEASON || k->role == SEASONS_ROLE_EXTRA) return k->season;
    return 2001;
}

void season_screen_database_text(char *out, size_t room)
{
    const char *db = options_string(OPT_DATABASE);
    if (strncmp(db, "folder:", 7)) {
        snprintf(out, room, "%s data", db);
        return;
    }
    if (!season_code_on()) {                            /* RECOMP_SEASON_CODE=off: no folder is read */
        dbfolders_label(db + 7, out, room);
        return;
    }
    int exists;
    const SeasonKnownDb *k = folder_db(db + 7, &exists);
    if (!exists) snprintf(out, room, "3.9.68 data");
    else if (k && k->role == SEASONS_ROLE_OFFICIAL_BASE) snprintf(out, room, "3.9.68 data");
    else if (k) snprintf(out, room, "%s", k->name);
    else dbfolders_label(db + 7, out, room);
}

/* ---- Calls into the game ---- */

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

static uint32_t scratch;            /* runtime memory: TEXTS buffers of TEXT_ROOM bytes */
static int next_text;

static uint32_t text(const char *s)
{
    uint32_t at = scratch + (uint32_t)(next_text++ % TEXTS) * TEXT_ROOM;
    win_put_str(at, s, TEXT_ROOM);
    return at;
}

static uint32_t colour16(uint32_t at) { return rd16(g_mem, at); }

/* A widget with its own rect (classic numbers), no area. flags 2 is a button, 1 a label. */
static int widget(uint32_t flags, int x1, int y1, int x2, int y2, uint32_t style, uint32_t colour, uint32_t text_flags,
                  uint32_t font, uint32_t text_colour, const char *s, uint32_t id, uint32_t data)
{
    uint32_t w[18] = {flags, (uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, 0, 0, style, colour,
                      flags == 2 ? colour : 0, text_flags, font, text_colour, text(s), 0, id, data, 0xffffffffu};
    return (int16_t)call_this(WIDGET_CREATE, display(), 18, w);
}

/* A text: flags 1 left, 0xc centred; font 1 small, 2 medium, 3 as the menu, 4 large. */
static void label(int x1, int y1, int x2, int y2, uint32_t text_flags, uint32_t font, uint32_t colour, const char *s)
{
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, 1, 0, text_flags, font, colour, text(s), 1};
    call_this(BOX_TEXT, SCREENS, 11, a);
}

/* A grey button as Back of the original (style 0x30, 0x5edd10); greyed text and no id when off. */
static void button(int x1, int y1, int x2, int y2, uint32_t font, const char *s, uint32_t id, uint32_t data, int on)
{
    widget(on ? 2 : 1, x1, y1, x2, y2, 0x30, colour16(BUTTON_GREY), on ? 0xc : 0x2c, font, colour16(TEXT_LIGHT), s,
           on ? id : 0, on ? data : 0);
}

static void panel(int x1, int y1, int x2, int y2)
{
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, 1, 0, 1, 0, 2, 0, 0xffffffffu};
    call_this(AREA_CREATE, display(), 11, a);
}

/* ---- Texts: the game's own fonts, measured ---- */

#define TEXT_WIDTH 0x5e5ad0u        /* cdecl (font, text): the width in screen pixels (display.c) */

static int text_w(uint32_t font, const char *s)
{
    return (int32_t)call(TEXT_WIDTH, 2, (uint32_t[2]){font, text(s)});
}

/* The first font of the list (largest first) whose text fits width pixels, or the last one. */
static uint32_t fit_font(const uint8_t *fonts, int n, const char *s, int width)
{
    for (int i = 0; i < n; i++)
        if (fonts[i] && text_w(fonts[i], s) <= width) return fonts[i];
    return fonts[n - 1];
}

/* A label in pixels of the mode. flags 1: left, 0xc: centred. */
static void plabel(int x1, int y1, int x2, int y2, uint32_t flags, uint32_t font, Rgb colour, const char *s)
{
    label(px(x1), py(y1), px(x2), py(y2), flags, font, rgb565(colour), s);
}

/* A text that wraps at spaces into lines of width pixels, from y down. Returns the y below it. At most
 * max_lines lines; a last line that still does not fit takes the next smaller font (font 1 at the least). */
static int paragraph(int x, int y, int width, uint32_t font, Rgb colour, const char *s, int max_lines)
{
    char line[256], rest[512];
    snprintf(rest, sizeof rest, "%s", s);
    int lh = line_h(font);
    for (int n = 0; rest[0] && n < max_lines; n++) {
        size_t cut = strlen(rest);
        if (n < max_lines - 1) {
            /* the longest run of words that fits */
            while (cut > 0) {
                snprintf(line, sizeof line, "%.*s", (int)cut, rest);
                if (text_w(font, line) <= width) break;
                char *sp = NULL;
                for (size_t i = 0; i < cut; i++)
                    if (rest[i] == ' ') sp = rest + i;
                if (!sp) break;
                cut = (size_t)(sp - rest);
            }
        }
        snprintf(line, sizeof line, "%.*s", (int)cut, rest);
        uint32_t f = font;
        while (f > 1 && text_w(f, line) > width) f--;
        plabel(x, y, x + width, y + lh, 1, f, colour, line);
        y += lh;
        const char *next = rest + cut;
        while (*next == ' ') next++;
        memmove(rest, next, strlen(next) + 1);
    }
    return y;
}

/* A grey button in pixels of the mode. */
static void pbutton(int x1, int y1, int x2, int y2, uint32_t font, const char *s, uint32_t id, uint32_t data, int on)
{
    button(px(x1), py(y1), px(x2), py(y2), font, s, id, data, on);
}

/* A filled box with no text, in pixels of the mode. */
static int pbox(int x1, int y1, int x2, int y2, Rgb c)
{
    return widget(1, px(x1), py(y1), px(x2), py(y2), 0x30, rgb565(c), 0xc, 1, 0, "", 0, 0);
}

/* ---- What the runtime draws over the widgets ---- */

typedef enum { MARK_TILE, MARK_BAND } MarkKind;
/* The rect widget gives the place. A draw of any of the widgets of the mark draws it again, so it stays
 * over the texts of a tile when the game draws one of them alone. */
typedef struct {
    int widgets[3];
    int rect;
    MarkKind kind;
    Rgb colour;
    int star, chosen;
} Mark;
#define MARKS 64
static Mark marks[MARKS];
static int mark_count;

static void mark(int rect, int w1, int w2, MarkKind kind, Rgb colour, int star, int chosen)
{
    if (rect < 0 || mark_count >= MARKS) return;
    marks[mark_count++] = (Mark){{rect, w1, w2}, rect, kind, colour, star, chosen};
}

static uint32_t build_fn(void);

static int on_top(void)
{
    uint32_t node = rd32(g_mem, TOP_NODE);
    return node && rd32(g_mem, node) == build_fn();
}

static void fill(const DrawTarget *t, int x1, int y1, int x2, int y2, uint16_t c, int alpha)
{
    int xy[8] = {x1, y1, x2 + 1, y1, x2 + 1, y2 + 1, x1, y2 + 1};
    draw_polygon(t, xy, 4, c, alpha);
}

/* A five-pointed star of radius r around (cx, cy), in yellow with a dark edge. */
static void star(const DrawTarget *t, int cx, int cy, int r)
{
    static const int unit[20] = {0, -100, 22, -31, 95, -31, 36, 12, 59, 81, 0, 38, -59, 81, -36, 12, -95, -31, -22, -31};
    int xy[20], edge[20];
    for (int i = 0; i < 20; i += 2) {
        xy[i] = cx + unit[i] * r / 100;
        xy[i + 1] = cy + unit[i + 1] * r / 100;
        edge[i] = cx + unit[i] * (r + 1) / 100;
        edge[i + 1] = cy + unit[i + 1] * (r + 1) / 100 + 1;
    }
    draw_polygon(t, edge, 10, 0x0000, 256);
    draw_polygon(t, xy, 10, rgb565(yellow_rgb), 256);
}

void season_screen_widget_drawn(uint32_t w)
{
    if (!mark_count || !on_top() || rd32(g_mem, NO_DRAW)) return;
    if (w < display() + WIDGETS || (w - display() - WIDGETS) % WIDGET_SIZE) return;
    int index = (int)((w - display() - WIDGETS) / WIDGET_SIZE);
    const Mark *m = NULL;
    for (int i = 0; i < mark_count; i++)
        for (int k = 0; k < 3; k++)
            if (marks[i].widgets[k] == index) m = &marks[i];
    if (!m) return;
    w = display() + WIDGETS + (uint32_t)m->rect * WIDGET_SIZE;
    uint32_t surface = rd32(g_mem, DRAW_SURFACE);
    if (!surface) return;
    DrawTarget t = {g_mem + rd32(g_mem, surface + 0x14), rd32(g_mem, surface + 0x10),
                    (int)rd32(g_mem, surface + 8), (int)rd32(g_mem, surface + 0xc)};
    int x1 = (int32_t)rd32(g_mem, w + 0x10), y1 = (int32_t)rd32(g_mem, w + 0x14);
    int x2 = (int32_t)rd32(g_mem, w + 0x18), y2 = (int32_t)rd32(g_mem, w + 0x1c);
    uint16_t c = rgb565(m->colour);
    if (m->kind == MARK_BAND) {
        /* The heading bar: the decade colour, fading out towards 85 % of the width. */
        int w85 = (x2 - x1) * 85 / 100;
        for (int x = x1; x <= x2; x++) {
            int a = x - x1 >= w85 ? 0 : 240 - 240 * (x - x1) / (w85 ? w85 : 1);
            if (a > 0) fill(&t, x, y1, x, y2, c, a);
        }
        return;
    }
    uint16_t yellow = rgb565(yellow_rgb);
    fill(&t, x1, y1, x2, y1 + 2, m->chosen ? yellow : c, 256);           /* the top edge */
    if (m->chosen) {                                                     /* the yellow frame */
        fill(&t, x1, y2 - 1, x2, y2, yellow, 256);
        fill(&t, x1, y1, x1 + 1, y2, yellow, 256);
        fill(&t, x2 - 1, y1, x2, y2, yellow, 256);
    }
    if (m->star) {               /* top right, in the room above the year (Layout star_pad) */
        int r = lay()->star_r;
        star(&t, x2 - r - 4, y1 + 3 + r + 2, r);
    }
}

/* ---- The tiles ---- */

static Tag status_tag(const SeasonsEntry *s)
{
    switch (s->status) {
    case SEASONS_OFFICIAL: return tag_official;
    case SEASONS_UNPACKED: return tag_installed;
    case SEASONS_IN_DOWNLOAD: return tag_included;
    case SEASONS_NO_DATABASE: return tag_none;
    default: return tag_missing;
    }
}

/* One tile, in pixels of the mode: a button in the tint of its decade with the year, then the status tag. The
 * year and the tag are buttons with the same action, so a click anywhere on the tile chooses it. A tile with no
 * database is greyed and takes no click. The star (season_screen_widget_drawn) sits above the year. */
static uint32_t year_font;          /* one font for the years of all tiles: the largest that fits every year */

static void tile(int x1, int y1, int x2, int y2, const char *title, Tag tag, Rgb colour, int index, int enabled,
                 int is_star, int chosen)
{
    const Layout *L = lay();
    uint32_t flags = enabled ? 2 : 1, id = enabled ? TILE_ID : 0, data = enabled ? (uint32_t)index : 0;
    uint16_t fill_c = rgb565(enabled ? mix(colour, 80) : greyed_box);
    Rgb year_rgb = !enabled ? grey_text : chosen ? yellow_rgb : white_rgb;
    int w = widget(flags, px(x1), py(y1), px(x2), py(y2), 0x30, fill_c, 0xc, 1, 0, "", id, data);
    int room = x2 - x1 - 6;
    uint32_t yf = index == CUSTOM_TILE ? fit_font(L->year_fonts, 3, title, room) : year_font;
    int yy = y1 + L->star_pad;
    int t = widget(flags, px(x1 + 3), py(yy), px(x2 - 3), py(yy + L->year_h), 0x30, fill_c, 0xc, yf, rgb565(year_rgb),
                   title, id, data);
    int ty = yy + L->year_h + L->tag_gap, g;
    uint32_t tf = fit_font(L->text_fonts, 2, tag.text, room - 8);
    if (tag.boxed) {
        int tw = text_w(tf, tag.text) + 10;
        if (tw > room) tw = room;
        int tx = x1 + (x2 - x1 - tw) / 2;
        g = widget(flags, px(tx), py(ty), px(tx + tw), py(ty + L->tag_h), 0x30, rgb565(tag.box), 0xc, tf, rgb565(tag.ink),
                   tag.text, id, data);
    } else {
        /* "No database": one line when it fits, else two lines (the greyed tile has the room). */
        char two[64];
        const char *sp = strrchr(tag.text, ' ');
        if (text_w(tf, tag.text) <= room || !sp) snprintf(two, sizeof two, "%s", tag.text);
        else snprintf(two, sizeof two, "%.*s\n%s", (int)(sp - tag.text), tag.text, sp + 1);
        g = widget(1, px(x1 + 2), py(ty), px(x2 - 2), py(y2 - 2), 0x30, fill_c, 0xc, tf, rgb565(tag.ink), two, 0, 0);
    }
    mark(w, t, g, MARK_TILE, enabled ? colour : (Rgb){85, 85, 106}, is_star, chosen);
}

/* A heading bar: the decade name and the run of seasons, on the decade colour that fades out. */
static void bar(int x1, int y1, int x2, int y2, Rgb colour, const char *name, const char *range)
{
    const Layout *L = lay();
    int w = widget(1, px(x1), py(y1), px(x2), py(y2), 1, 0, 1, 1, 0, "", 0, 0);
    mark(w, -1, -1, MARK_BAND, colour, 0, 0);
    uint32_t nf = L->name_fonts[1], rf = L->text_fonts[0];
    int name_w = text_w(nf, name) + 14;
    plabel(x1 + 8, y1, x1 + 8 + name_w, y2, 1, nf, white_rgb, name);
    plabel(x1 + 8 + name_w, y1, x2, y2, 1, rf, pale_rgb, range);
}

/* ---- The details of the chosen tile ---- */

static int folder_fault(const char *name);

/* A database name for the player: "Official data 3.9.68" for the data sets. */
static void db_title(const char *name, char *out, size_t room)
{
    if (!strncmp(name, "3.9.", 4)) snprintf(out, room, "Official data %s", name);
    else snprintf(out, room, "%s", name);
}

/* What the details show, worked out once for both layouts. */
typedef struct {
    int custom, year, ok;
    char big[32], decade[48], start[40], db[160], credit[160], europe[160];
    Rgb decade_rgb, top_rgb;
    const char *row_note;           /* a line under the details: unpack, error or a hint */
    Rgb row_rgb;
} Details;

static const char *folder_fault_text(int fault)
{
    return fault == 1 ? "Rename this folder: its name starts or ends with a space."
                      : "Rename this folder: its name holds a tab or another control character.";
}

static void details_of(int sel, Details *d)
{
    memset(d, 0, sizeof *d);
    int state = (int)node_slot(4);
    d->ok = 1;
    if (sel == CUSTOM_TILE) {
        d->custom = 1;
        d->year = (int)node_slot(3);
        if (d->year < SEASONS_FIRST || d->year >= SEASONS_FIRST + SEASONS_COUNT) d->year = 2025;
        snprintf(d->big, sizeof d->big, "Custom");
        snprintf(d->decade, sizeof d->decade, "Season: %d/%02d", d->year, (d->year + 1) % 100);
        d->decade_rgb = pale_rgb;
        d->top_rgb = custom_bar;
        int folder = (int)(node_slot(2) % (uint32_t)(list.custom_count ? list.custom_count : 1));
        if (list.custom_count) {
            dbfolders_label(list.custom[folder], d->db, sizeof d->db);
            int fault = folder_fault(list.custom[folder]);
            snprintf(d->credit, sizeof d->credit, "%s",
                     fault ? folder_fault_text(fault) : "A folder in Databases. You name its season.");
            if (fault) d->ok = 0;
        } else {
            d->ok = 0;
            snprintf(d->db, sizeof d->db, "No folder found");
            snprintf(d->credit, sizeof d->credit,
                     "Copy a database folder into Databases in the save folder, then open this screen again.");
        }
    } else {
        const SeasonsEntry *s = &list.season[sel];
        d->year = s->start_year;
        snprintf(d->big, sizeof d->big, "%s", s->label);
        snprintf(d->decade, sizeof d->decade, "%s", seasons_db_decade_name(s->decade));
        d->decade_rgb = decade_look[s->decade].text;
        d->top_rgb = decade_look[s->decade].bar;
        db_title(s->database ? s->database : "No database", d->db, sizeof d->db);
        if (s->database) snprintf(d->credit, sizeof d->credit, "%s", season_screen_credit(s->database));
        if (!s->database || s->status == SEASONS_NO_DATABASE || s->status == SEASONS_MISSING) d->ok = 0;
        d->row_note = s->status == SEASONS_IN_DOWNLOAD ? "The first start unpacks this database into Databases."
                      : s->status == SEASONS_MISSING   ? "This database is not in this game folder."
                                                       : NULL;
        d->row_rgb = pale_rgb;
    }
    snprintf(d->start, sizeof d->start, "Starts July %d", d->year);
    snprintf(d->europe, sizeof d->europe, "%s", europe_text(d->year));
    if (state == 1) {
        static char unpack[120];
        snprintf(unpack, sizeof unpack, "Unpacking %d/%02d into Databases. This takes a few seconds.", d->year,
                 (d->year + 1) % 100);
        d->row_note = unpack;
        d->row_rgb = yellow_rgb;
        d->ok = 0;
    } else if (state == 2) {
        d->row_note = error_text;
        d->row_rgb = error_rgb;
    }
}

/* The Custom choices: Season < 2010/11 > and Next folder, from x on a row at y. */
static void custom_choices(const Details *d, int x, int y, int x2)
{
    const Layout *L = lay();
    int h = L->button_h, f = L->text_fonts[0];
    char a[16];
    snprintf(a, sizeof a, "%d/%02d", d->year, (d->year + 1) % 100);
    int lw = text_w(f, "Season:") + 10, yw = text_w(f, a) + 16;
    plabel(x, y, x + lw, y + h, 1, f, pale_rgb, "Season:");
    x += lw;
    pbutton(x, y, x + h, y + h, f, "<", CUSTOM_YEAR_ID, 0, d->year > SEASONS_FIRST);
    plabel(x + h, y, x + h + yw, y + h, 0xc, f, yellow_rgb, a);
    pbutton(x + h + yw, y, x + 2 * h + yw, y + h, f, ">", CUSTOM_YEAR_ID, 1, d->year < SEASONS_FIRST + SEASONS_COUNT - 1);
    x += 2 * h + yw + 12;
    int nw = text_w(f, "Next folder") + 20;
    if (list.custom_count > 1 && x + nw <= x2) pbutton(x, y, x + nw, y + h, f, "Next folder", FOLDER_ID, 0, 1);
}

/* Classic: the strip below the grid. Two columns: the year, the decade and the start; the database, the
 * credit and Europe. */
static void details_strip(const Details *d)
{
    const Layout *L = lay();
    int x1 = L->det_x1, y1 = L->det_y1, x2 = L->det_x2, y2 = L->det_y2, p = L->pad;
    panel(px(x1), py(y1), px(x2), py(y2));
    pbox(x1, y1, x2, y1 + 3, d->top_rgb);
    int lx = x1 + p + 4, rx = lx + L->left_w + 20, ex = x2 - p, y = y1 + p;
    uint32_t bf = fit_font(L->title_fonts, 3, d->big, L->left_w);
    plabel(lx, y, lx + L->left_w, y + line_h(bf), 1, bf, yellow_rgb, d->big);
    uint32_t tf = L->text_fonts[0], sf = L->name_fonts[1];    /* small texts; the decade line */
    int ly = y + line_h(bf) + 2;
    uint32_t decf = fit_font((uint8_t[]){(uint8_t)sf, 2, 1}, 3, d->decade, L->left_w);
    plabel(lx, ly, lx + L->left_w, ly + line_h(decf), 1, decf, d->decade_rgb, d->decade);
    ly += line_h(decf) + 8;
    plabel(lx, ly, lx + L->left_w, ly + line_h(tf), 1, fit_font(L->text_fonts, 2, d->start, L->left_w), pale_rgb,
           d->start);

    int width = ex - rx;
    uint32_t df = fit_font(L->name_fonts, 3, d->db, width);
    plabel(rx, y, ex, y + line_h(df), 1, df, white_rgb, d->db);
    y += line_h(df);
    y = paragraph(rx, y, width, tf, pale_rgb, d->credit, 2);
    if (!d->custom) {
        int ew = text_w(tf, "Europe:") + 8;
        plabel(rx, y + 2, rx + ew, y + 2 + line_h(tf), 1, tf, pale_rgb, "Europe:");
        y = paragraph(rx + ew, y + 2, width - ew, tf, white_rgb, d->europe, 2);
    }
    y += 6;
    if (d->row_note) {
        paragraph(rx, y, width, tf, d->row_rgb, d->row_note, 2);
        return;
    }
    if (d->custom) custom_choices(d, rx, y, ex);
}

/* Widescreen: the panel on the right of the grid. */
static void details_panel(const Details *d)
{
    const Layout *L = lay();
    int x1 = L->det_x1, y1 = L->det_y1, x2 = L->det_x2, y2 = L->det_y2, p = L->pad;
    panel(px(x1), py(y1), px(x2), py(y2));
    pbox(x1, y1, x2, y1 + 4, d->top_rgb);
    int x = x1 + p, w = x2 - x1 - 2 * p, y = y1 + p, ex = x + w;
    uint32_t bf = fit_font(L->title_fonts, 3, d->big, w);
    plabel(x, y, ex, y + line_h(bf), 1, bf, yellow_rgb, d->big);
    y += line_h(bf) + 4;
    plabel(x, y, ex, y + line_h(3), 1, fit_font((uint8_t[]){3, 2}, 2, d->decade, w), d->decade_rgb, d->decade);
    y += line_h(3);
    plabel(x, y, ex, y + line_h(2), 1, 2, pale_rgb, d->start);
    y += line_h(2) + 8;
    pbox(x, y, ex, y + 1, rule_rgb);
    y += 8;
    plabel(x, y, ex, y + line_h(1), 1, 1, pale_rgb, "Database");
    y += line_h(1);
    uint32_t df = fit_font(L->name_fonts, 3, d->db, w);
    y = paragraph(x, y, w, df, white_rgb, d->db, 2);
    y = paragraph(x, y, w, 2, pale_rgb, d->credit, 3) + 6;
    int h = L->button_h;
    if (d->row_note) {
        y = paragraph(x, y, w, 2, d->row_rgb, d->row_note, 3) + 6;
    } else if (d->custom) {
        custom_choices(d, x, y, ex);
        y += h + 6;
        if (list.custom_count > 1 && x + 300 > ex) {
            /* the Next folder button goes on its own row when the row above has no room */
            int nw = text_w(2, "Next folder") + 20;
            pbutton(x, y, x + nw, y + h, 2, "Next folder", FOLDER_ID, 0, 1);
            y += h + 6;
        }
    }
    if (d->custom) return;
    y += 4;
    pbox(x, y, ex, y + 1, rule_rgb);
    y += 8;
    plabel(x, y, ex, y + line_h(1), 1, 1, pale_rgb, "Europe");
    y += line_h(1);
    paragraph(x, y, w, 2, white_rgb, d->europe, 4);
}

/* ---- The changing rules switch (Q232, Q259) ---- */
static int rules_choice = -1;       /* -1: the option changing_rules */

int season_screen_rules_choice(void) { return rules_choice; }
int season_screen_rules_on(void) { return rules_choice < 0 ? options_bool(OPT_CHANGING_RULES) : rules_choice; }
void season_screen_set_rules(int on) { rules_choice = on ? 1 : 0; }

/* "Changing rules", On and Off (the chosen one a yellow framed label, as the options box), and what it does. */
static void rules_row(const Layout *L, int x, int y)
{
    int h = L->link_h;
    uint32_t f = L->text_fonts[0];
    int on = season_screen_rules_on();
    const char *name = "Changing rules", *note = on ? "The rules change each July, as they did in real life."
                                                    : "The rules of the start season stay for the whole game.";
    int nw = text_w(f, name) + 12, bw = text_w(f, "Off") + 28;
    plabel(x, y, x + nw, y + h, 1, f, white_rgb, name);
    x += nw;
    for (int v = 1; v >= 0; v--) {
        const char *t = v ? "On" : "Off";
        if (v == on)
            widget(1, px(x), py(y), px(x + bw), py(y + h), 0x221, rgb565(yellow_rgb), 0xc, f, rgb565(yellow_rgb), t, 0, 0);
        else
            pbutton(x, y, x + bw, y + h, f, t, RULES_ID, (uint32_t)v, 1);
        x += bw + 6;
    }
    x += 8;
    plabel(x, y, L->right, y + h, 1, fit_font(L->text_fonts, 2, note, L->right - x), pale_rgb, note);
}

/* ---- Build and input ---- */

static void season_build(void)
{
    if (!scratch) scratch = win_static_alloc(TEXTS * TEXT_ROOM);
    const Layout *L = lay();
    mark_count = 0;
    /* The picture of the screen below: the start screen's own (its node title). */
    uint32_t node = rd32(g_mem, TOP_NODE), prev = node ? rd32(g_mem, node + NODE_PREV) : 0;
    char title[260] = "";
    if (prev) snprintf(title, sizeof title, "%.259s", win_str(prev + NODE_TITLE));
    uint32_t p[1] = {text(title)};
    call(SET_PICTURE, 1, p);
    uint32_t h[3] = {colour16(HEADING_RED), colour16(TEXT_LIGHT), text("Choose Season")};
    call_this(HEADING, SCREENS, 3, h);
    uint32_t sb[2] = {4, 0};
    call(SIDE_BAR, 2, sb);

    int sel = (int)node_slot(0) - 1;
    if (sel < -1 || sel >= TILES) sel = -1;
    int grid_w = 10 * L->tile_w + 9 * L->tile_gap;
    year_font = L->year_fonts[2];
    for (int f = 0; f < 3; f++) {
        int fits = 1;
        for (int i = 0; i < SEASONS_COUNT && fits; i++) fits = text_w(L->year_fonts[f], list.season[i].label) <= L->tile_w - 16;
        if (fits) {
            year_font = L->year_fonts[f];
            break;
        }
    }
    for (int dec = 0; dec < 3; dec++) {
        int y = L->grid_y + dec * (L->bar_h + L->bar_gap + L->tile_h + L->row_gap);
        bar(L->grid_x, y, L->grid_x + grid_w - 1, y + L->bar_h, decade_look[dec].bar,
            seasons_db_decade_name((SeasonsDecade)dec), decade_look[dec].range);
        int ty = y + L->bar_h + L->bar_gap;
        for (int col = 0; col < 10; col++) {
            int year = 2000 + 10 * dec + col, i = year - SEASONS_FIRST;
            if (i < 0 || i >= SEASONS_COUNT) continue;          /* 2000/01 and the cells after 2025/26 */
            const SeasonsEntry *s = &list.season[i];
            int tx = L->grid_x + col * (L->tile_w + L->tile_gap);
            tile(tx, ty, tx + L->tile_w - 1, ty + L->tile_h, s->label, status_tag(s), decade_look[dec].bar, i,
                 s->status != SEASONS_NO_DATABASE, starred[i], sel == i);
        }
        if (dec == 2) {                     /* the Custom database fills the free cells after 2025/26 */
            int col = SEASONS_FIRST + SEASONS_COUNT - 2020;
            int tx = L->grid_x + col * (L->tile_w + L->tile_gap);
            char tag[40];
            snprintf(tag, sizeof tag, list.custom_count == 1 ? "1 folder" : "%d folders", list.custom_count);
            Tag t = tag_included;
            t.text = tag;
            tile(tx, ty, L->grid_x + grid_w - 1, ty + L->tile_h, "Custom database", t, custom_bar, CUSTOM_TILE, 1, 0,
                 sel == CUSTOM_TILE);
        }
    }

    Details d;
    memset(&d, 0, sizeof d);
    int ok = 0;
    if (sel >= 0) {
        details_of(sel, &d);
        ok = d.ok;
        if (L->side) details_panel(&d);
        else details_strip(&d);
    } else {
        panel(px(L->det_x1), py(L->det_y1), px(L->det_x2), py(L->det_y2));
        paragraph(L->det_x1 + L->pad, L->det_y1 + L->pad, L->det_x2 - L->det_x1 - 2 * L->pad, 2, pale_rgb,
                  "Click a season to see its database.", 2);
    }

    /* The changing rules switch at the foot of the strip, under the database column. */
    if (sel >= 0) rules_row(L, L->det_x1 + L->pad + 4 + L->left_w + 20, L->det_y2 - L->pad - L->link_h);

    const char *more = "More versions of each season are on the Update Team's forum.", *open = "Open champman0102.co.uk";
    uint32_t lf = L->text_fonts[0];
    int link_w = text_w(lf, open) + 24;
    plabel(L->left + 4, L->link_y, L->right - link_w - 8, L->link_y + L->link_h, 1,
           fit_font(L->text_fonts, 2, more, L->right - link_w - 12 - L->left), pale_rgb, more);
    pbutton(L->right - link_w, L->link_y, L->right, L->link_y + L->link_h, lf, open, LINK_ID, 0, 1);

    /* Back, and the yellow Start button of the chosen season. */
    int by = L->bottom_y, bh = L->bottom_h, sx = L->right - L->start_w;
    pbutton(L->left, by, sx - 4, by + bh, 3, "Back", BACK_ID, 0, 1);
    char start[40];
    int state = (int)node_slot(4);
    int year = sel == CUSTOM_TILE ? (sel >= 0 ? d.year : 0) : sel >= 0 ? SEASONS_FIRST + sel : 0;
    if (state == 1) snprintf(start, sizeof start, "Please wait");
    else if (year) snprintf(start, sizeof start, "Start %d/%02d", year, (year + 1) % 100);
    else snprintf(start, sizeof start, "Start");
    if (ok && state != 1)
        widget(2, px(sx), py(by), px(L->right), py(by + bh), 0x30, rgb565(yellow_rgb), 0xc, 3, rgb565(start_ink), start,
               CHOOSE_ID, 0);
    else
        widget(1, px(sx), py(by), px(L->right), py(by + bh), 0x30, colour16(BUTTON_GREY), 0x2c, 3,
               colour16(TEXT_LIGHT), start, 0, 0);
    fprintf(stderr, "recomp: choose season built, tile %d, year font %u\n", sel + 1, year_font);
    rt_ret_cdecl(0);
}

static void choose_select(int tile_index)
{
    set_slot(0, (uint32_t)tile_index + 1);
    set_slot(4, 0);
}

/* Why the option database cannot hold the folder name as it is: 0 it can, 1 a space at the start or the end
 * (options.c trims a value), 2 a tab or another control character (a line of settings.txt cannot hold it). */
static int folder_fault(const char *name)
{
    size_t n = strlen(name);
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)name[i] < 0x20 || name[i] == 0x7f) return 2;
    if (!n || name[0] == ' ' || name[n - 1] == ' ') return 1;
    return 0;
}

/* Starts the choice: the options database and season, and the season of the next new game. Returns 1 when
 * the screen goes back to Setup Game, 0 when it shows an error. */
static int choose_apply(void)
{
    int sel = (int)node_slot(0) - 1;
    char value[DBFOLDER_NAME + 8];
    int year;
    if (sel == CUSTOM_TILE) {
        if (!list.custom_count) return 0;
        year = (int)node_slot(3);
        if (year < SEASONS_FIRST || year >= SEASONS_FIRST + SEASONS_COUNT) year = 2025;
        const char *name = list.custom[node_slot(2) % (uint32_t)list.custom_count];
        if (folder_fault(name)) {
            snprintf(error_text, sizeof error_text, "%s", folder_fault_text(folder_fault(name)));
            return 0;
        }
        snprintf(value, sizeof value, "folder:%s", name);
    } else if (sel >= 0 && sel < SEASONS_COUNT) {
        const SeasonsEntry *s = &list.season[sel];
        year = s->start_year;
        if (!s->database) return 0;
        if (year == SEASONS_FIRST) {
            snprintf(value, sizeof value, "3.9.68");
        } else {
            SeasonsPrepare r;
            memset(&r, 0, sizeof r);
            SeasonsPrepareStatus st = seasons_db_prepare(year, &r);
            fprintf(stderr, "recomp: choose season %d: prepare status %d, folder %s, unpacked %d\n", year, (int)st,
                    r.name, r.unpacked);
            if (st != SEASONS_PREPARE_OK) {
                snprintf(error_text, sizeof error_text, "%s",
                         r.error[0] ? r.error : "The database of this season cannot be started.");
                seasons_db_scan(&list);
                return 0;
            }
            snprintf(value, sizeof value, "folder:%s", r.name);
        }
    } else {
        return 0;
    }
    char season_word[16];
    snprintf(season_word, sizeof season_word, "%d/%02d", year, (year + 1) % 100);
    if (!strncmp(value, "folder:", 7) && folder_fault(value + 7)) {
        snprintf(error_text, sizeof error_text, "%s", folder_fault_text(folder_fault(value + 7)));
        return 0;
    }
    int a = options_set(OPT_DATABASE, value), b = options_set(OPT_SEASON, season_word);
    if (!a || !b || strcmp(options_string(OPT_DATABASE), value)) {
        /* options_set refused or changed the value: the next new game would start another data set. */
        snprintf(error_text, sizeof error_text, "This database cannot be chosen: its folder name cannot be stored.");
        fprintf(stderr, "cm0102_recomp: database = %s was not stored as it is\n", value);
        return 0;
    }
    if (a < 0 || b < 0)
        fprintf(stderr, "cm0102_recomp: the season choice applies to this run only: settings.txt cannot be written\n");
    win_data_set_changed();
    season_set_new_game(year, season_screen_rules_choice());
    fprintf(stderr, "recomp: choose season: %s with database = %s\n", season_word, value);
    return 1;
}

static uint32_t handled_press = 0xffffffffu;

static void season_input(void)
{
    int16_t w = (int16_t)rt_arg(0);
    if ((int)node_slot(4) == 1) {
        /* The screen with the unpack line is on show: the unpack runs now. */
        if (choose_apply()) {
            set_slot(4, 0);
            rt_ret_cdecl(GO_BACK);
        } else {
            set_slot(4, 2);
            rt_ret_cdecl(REBUILD);
        }
        return;
    }
    /* One click for each press of the mouse button, as the options box (a held button repeats). */
    if (w >= 0 && win_press_count() == handled_press) w = -1;
    if (w >= 0 && w < MAX_WIDGETS) {
        uint32_t at = display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE;
        uint32_t id = (uint16_t)rd32(g_mem, at), data = rd32(g_mem, at + 0x40);
        uint32_t r = 0;
        if (id == TILE_ID && data < TILES) {
            choose_select((int)data);
            r = REBUILD;
        } else if (id == RULES_ID) {
            rules_choice = data ? 1 : 0;
            season_set_new_game(season_start_year(), rules_choice);
            fprintf(stderr, "recomp: changing rules of the next new game: %s\n", rules_choice ? "on" : "off");
            r = REBUILD;
        } else if (id == FOLDER_ID) {
            set_slot(2, list.custom_count ? (node_slot(2) + 1) % (uint32_t)list.custom_count : 0);
            r = REBUILD;
        } else if (id == CUSTOM_YEAR_ID) {
            int year = (int)node_slot(3) + (data ? 1 : -1);
            if (year >= SEASONS_FIRST && year < SEASONS_FIRST + SEASONS_COUNT) set_slot(3, (uint32_t)year);
            r = REBUILD;
        } else if (id == LINK_ID) {
            fprintf(stderr, "recomp: choose season: open %s\n", LINK_URL);
            handled_press = win_press_count();  /* one press opens one page: a held button repeats */
            if (!getenv("RECOMP_SCRIPT") && SDL_OpenURL(LINK_URL))
                fprintf(stderr, "cm0102_recomp: the page %s could not be opened: %s\n", LINK_URL, SDL_GetError());
            r = 0;
        } else if (id == BACK_ID) {
            r = GO_BACK;
        } else if (id == CHOOSE_ID) {
            int sel = (int)node_slot(0) - 1;
            int unpack = sel >= 0 && sel < SEASONS_COUNT && list.season[sel].database &&
                         list.season[sel].status == SEASONS_IN_DOWNLOAD;
            if (unpack) {
                set_slot(4, 1);             /* show the unpack line first; the unpack runs at the next call */
                r = REBUILD;
            } else if (choose_apply()) {
                r = GO_BACK;
            } else {
                set_slot(4, 2);
                r = REBUILD;
            }
        }
        if (r) {
            handled_press = win_press_count();
            rt_ret_cdecl(r);
            return;
        }
    }
    uint32_t a[1] = {rt_arg(0)};
    uint32_t r = call(SIDE_BAR_INPUT, 1, a);
    if (!r) r = call(GAME_INPUT, 1, a);
    rt_ret_cdecl(r);
}

static uint32_t build_fn(void) { return rt_host_fn("season_build", season_build); }
uint32_t season_screen_build_fn(void) { return build_fn(); }

void season_screen_open(void)
{
    seasons_db_scan(&list);
    scan_saves();
    error_text[0] = 0;
    /* The tile of the current choice. */
    int year = season_screen_year(), tile_index = year - SEASONS_FIRST, folder = 0;
    const char *db = options_string(OPT_DATABASE);
    if (!strncmp(db, "folder:", 7)) {
        for (int i = 0; i < list.custom_count; i++)
            if (!strcmp(list.custom[i], db + 7)) {
                tile_index = CUSTOM_TILE;
                folder = i;
            }
    }
    uint32_t p[5] = {build_fn(), rt_host_fn("season_input", season_input), 0, 0, 0};
    call_this(PUSH, SCREENS, 5, p);
    set_slot(0, (uint32_t)tile_index + 1);
    set_slot(1, 0);
    set_slot(2, (uint32_t)folder);
    set_slot(3, (uint32_t)(tile_index == CUSTOM_TILE ? year : 2025));
    set_slot(4, 0);
    set_slot(5, 0);
    handled_press = win_press_count();          /* the press that opened the screen does not click in it */
    fprintf(stderr, "recomp: choose season opened, %d custom folders\n", list.custom_count);
}
