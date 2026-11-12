/* Widescreen mode and classic mode (docs/research/2026-10-14-widescreen-inventory.md).
 *
 * The original draws at 800x600. In widescreen mode the screen is 1280x800. The mode uses this rule:
 * - tools/recomp/widescreen.py turns the size constants of the display code into rt_screen_w and
 *   rt_screen_h;
 * - the area and widget create functions get coordinates for 800x600 from about 600 places, so these
 *   wrappers scale them to the screen before the original stores them;
 * - a picture of exactly 800x600 (a background) is scaled to fill the screen when it is loaded.
 *
 * The settings file settings.txt in the user folder chooses the mode with the line
 * "display = classic" or "display = widescreen". options.c reads it and writes it when it is missing. The HD mode of
 * 0.4.0 to 0.5.0 (1920x1080) is gone (Max, 2026-10-16): an old "display = hd" line gives widescreen. */
#include "win.h"
#include "host.h"
#include "options.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLASSIC_W 800u
#define CLASSIC_H 600u
#define WIDE_W 1280u
#define WIDE_H 800u

/* Reads settings.txt in the user folder through the options module (options.h), which writes the
 * file when it is missing. Sets the screen size. */
void win_read_settings(void)
{
    char path[1100];
    snprintf(path, sizeof path, "%s/settings.txt", win_user_dir);
    options_load(path);
    if (options_enum(OPT_DISPLAY) == DISPLAY_CLASSIC) {
        rt_screen_w = CLASSIC_W;
        rt_screen_h = CLASSIC_H;
    } else {
        rt_screen_w = WIDE_W;
        rt_screen_h = WIDE_H;
    }
}

/* A coordinate for 800x600 on the screen. A left or top edge maps to the first pixel of its part of
 * the screen and a right or bottom edge to the last, so that areas that touch still touch. Zero and
 * negative values keep their meaning ("none" or "from the owner"). */
static uint32_t scale(uint32_t v, uint32_t screen, uint32_t classic, int far_edge)
{
    int32_t x = (int32_t)v;
    if (screen == classic || x <= 0) return v;
    if (far_edge) return (uint32_t)((int64_t)(x + 1) * screen / classic - 1);
    return (uint32_t)((int64_t)x * screen / classic);
}

/* Scales the rectangle x1, y1, x2, y2 in the four stack arguments from argument `first`. */
static void scale_rect_args(int first)
{
    uint32_t a = cpu.esp + 4 + 4 * (uint32_t)first;
    wr32(g_mem, a, scale(rd32(g_mem, a), rt_screen_w, CLASSIC_W, 0));
    wr32(g_mem, a + 4, scale(rd32(g_mem, a + 4), rt_screen_h, CLASSIC_H, 0));
    wr32(g_mem, a + 8, scale(rd32(g_mem, a + 8), rt_screen_w, CLASSIC_W, 1));
    wr32(g_mem, a + 12, scale(rd32(g_mem, a + 12), rt_screen_h, CLASSIC_H, 1));
}

void o_0055fba0(void);

/* Match columns (columns.c): new column counts and column numbers for two lists of the match screens. */
void columns_area_before(void);
void columns_area_after(int area, int unwound);
void groups_area_before(void);
void columns_widget_before(void);
void foreign_widget_before(void);
void o_0055f990(void);
void o_005e40d0(void);
void o_00800890(void);
void o_005e5ad0(void);

/* Area create, thiscall: (x1, y1, x2, y2, columns, column weights, rows, row weights, flags, colour,
 * owner). */
RT_WRAP(0055fba0, area_create)
{
    columns_area_before();
    scale_rect_args(0);
    groups_area_before();
    o_0055fba0();
    columns_area_after((int16_t)cpu.eax, rt_unwinding);
}

/* The text with links 0x4aeaf0 (comp_text.cpp), thiscall: (y, x1, x2, first row, font, colour, flags, ...).
 * It makes a label for each line of its text and a button over each linked name (the clickable names of
 * the news). It wraps the text at x2 - x1 + 1 and, with flag 1, centres each line in x1..x2 (0x4af17d),
 * by the widths of the text in screen pixels (widescreen.py TRUE_WIDTH). So in widescreen mode the
 * wrapper gives it x1 and x2 in screen pixels, and the widget create scales only the y of the widgets
 * that it makes. Before, the lines wrapped and centred in the classic box, and then moved right by the
 * scale: the confirm "The file ... already exists" (0x591b70 by 0x5ed070) was off centre. */
void o_004aeaf0(void);

RT_WRAP(004aeaf0, comp_text)
{
    if (rt_screen_w != CLASSIC_W) {
        uint32_t a = cpu.esp + 4;
        wr32(g_mem, a + 4, scale(rd32(g_mem, a + 4), rt_screen_w, CLASSIC_W, 0));
        wr32(g_mem, a + 8, scale(rd32(g_mem, a + 8), rt_screen_w, CLASSIC_W, 1));
    }
    o_004aeaf0();
}

/* 1 when the widget create comes from 0x4aeaf0: its x values are screen pixels already. */
static int from_comp_text(void)
{
    uint32_t ret = rd32(g_mem, cpu.esp);
    return ret >= 0x4aeaf0u && ret < 0x4afa30u;
}

/* Widget create, thiscall: (flags, x1, y1, x2, y2, column, row, ...). */
RT_WRAP(0055f990, widget_create)
{
    columns_widget_before();
    foreign_widget_before();    /* the tactics screen's names (foreign.c) */
    int holiday = holiday_widget_before();
    int options_item = options_screen_widget_before();
    commentary_widget_before();
    if (rt_screen_w != CLASSIC_W && from_comp_text()) {
        uint32_t a = cpu.esp + 8;
        wr32(g_mem, a + 4, scale(rd32(g_mem, a + 4), rt_screen_h, CLASSIC_H, 0));
        wr32(g_mem, a + 12, scale(rd32(g_mem, a + 12), rt_screen_h, CLASSIC_H, 1));
    } else scale_rect_args(1);
    o_0055f990();
    if (holiday) holiday_widget_after();
    if (options_item) options_screen_widget_after();
}

/* Widget rect setter 0x800890, thiscall on the screen stack: (widget, x1, y1, x2, y2), each a 16-bit
 * value. The match screens move widgets with it, for example the possession bar of Match Overview
 * (0x71d9e0: x from 277 to 713 by the share of the ball). The values are classic coordinates, as for
 * the widget create, so they get the same scale. */
RT_WRAP(00800890, widget_set_rect)
{
    for (int i = 1; i <= 4; i++) {
        uint32_t a = cpu.esp + 4 + 4 * (uint32_t)i;
        wr32(g_mem, a, (uint32_t)(int32_t)(int16_t)rd32(g_mem, a));
    }
    scale_rect_args(1);
    o_00800890();
}

/* Text width 0x5e5ad0, cdecl: (font, text); returns the width in pixels. Screen code lays out widgets
 * in classic coordinates and often places a widget at "x + width of a text", or sizes it to a text.
 * The runtime scales those coordinates, so in widescreen mode such a call gets the width in classic
 * pixels (rounded up): the widget then lands on the text, and its click area covers what is drawn.
 * The calls of tools/recomp/widescreen.py TRUE_WIDTH compare with rects on the screen or draw, and
 * get screen pixels, as do calls from the runtime (rt_callback). */
extern const uint32_t rt_true_width_returns[];
extern const uint32_t rt_true_width_count;

RT_WRAP(005e5ad0, text_width)
{
    uint32_t ret = rd32(g_mem, cpu.esp);
    o_005e5ad0();
    if (rt_unwinding || rt_screen_w == CLASSIC_W) return;
    if (ret == RT_RETURN_ADDR) return;           /* the runtime itself (names.c) measures the drawn text */
    for (uint32_t i = 0; i < rt_true_width_count; i++)
        if (rt_true_width_returns[i] == ret) return;
    int32_t w = (int32_t)cpu.eax;
    if (w > 0) cpu.eax = (uint32_t)(((int64_t)w * CLASSIC_W + rt_screen_w - 1) / rt_screen_w);
}

/* The pixel at (x, y) of a 565 picture, mixed from its four neighbours by the weights fx, fy (0..256). */
static uint16_t mix565(const uint8_t *px, uint32_t w, uint32_t h, uint32_t x, uint32_t y, uint32_t fx, uint32_t fy)
{
    uint32_t x1 = x + 1 < w ? x + 1 : x, y1 = y + 1 < h ? y + 1 : y;
    uint32_t p[4] = {rd16(px, (y * w + x) * 2), rd16(px, (y * w + x1) * 2), rd16(px, (y1 * w + x) * 2),
                     rd16(px, (y1 * w + x1) * 2)};
    uint32_t k[4] = {(256 - fx) * (256 - fy), fx * (256 - fy), (256 - fx) * fy, fx * fy};
    uint32_t r = 0, g = 0, b = 0;
    for (int i = 0; i < 4; i++) {
        r += (p[i] >> 11) * k[i];
        g += ((p[i] >> 5) & 0x3f) * k[i];
        b += (p[i] & 0x1f) * k[i];
    }
    return (uint16_t)(((r + 32768) >> 16) << 11 | ((g + 32768) >> 16) << 5 | ((b + 32768) >> 16));
}

/* Gives the picture at pic new pixels of w x h, made from its old ones. step_x and step_y are 256ths
 * of an old pixel per new pixel, off_x and off_y the 256ths of the old picture left out at the left and
 * the top. Returns 0 when there is no memory; then the picture keeps its old pixels. */
static int resize_picture(uint32_t pic, uint32_t w, uint32_t h, uint32_t step_x, uint32_t step_y,
                          uint32_t off_x, uint32_t off_y)
{
    uint32_t old_w = rd32(g_mem, pic), old_h = rd32(g_mem, pic + 4), size = w * h * 2;
    uint32_t calloc_args[2] = {1, size};
    uint32_t pixels = rt_callback(0x945501u, 2, calloc_args);      /* the original's calloc */
    if (!pixels) return 0;
    uint32_t old = rd32(g_mem, pic + 0xc);
    const uint8_t *src = g_mem + old;
    uint8_t *dst = g_mem + pixels;
    for (uint32_t y = 0; y < h; y++) {
        uint32_t sy = off_y + y * step_y;
        for (uint32_t x = 0; x < w; x++) {
            uint32_t sx = off_x + x * step_x;
            wr16(dst, (y * w + x) * 2, mix565(src, old_w, old_h, sx >> 8, sy >> 8, sx & 255, sy & 255));
        }
    }
    uint32_t free_args[1] = {old};
    rt_callback(0x9452cau, 1, free_args);                           /* the original's free */
    wr32(g_mem, pic, w);
    wr32(g_mem, pic + 4, h);
    wr32(g_mem, pic + 8, size);
    wr32(g_mem, pic + 0xc, pixels);
    return 1;
}

/* Picture load 0x5e40d0, cdecl: (path, picture to reuse). The picture struct holds the width +0, the
 * height +4, the size of the pixels +8 and their address +0xc. In widescreen mode:
 * - a picture of 800x600 (a background) is scaled by the same factor in both directions until it
 *   covers the screen, and centred, so that the parts outside the screen are cut off;
 * - a picture 600 high and narrower (the bar at the left, game.mbr and match.mbr) is scaled to the
 *   screen height and by the factor of the screen width, as the area that holds it. */
RT_WRAP(005e40d0, picture_load)
{
    o_005e40d0();
    uint32_t pic = cpu.eax;
    if (rt_unwinding || !pic || rt_screen_w == CLASSIC_W) return;
    uint32_t w = rd32(g_mem, pic), h = rd32(g_mem, pic + 4);
    if (h != CLASSIC_H || w > CLASSIC_W || !w) return;
    if (w == CLASSIC_W) {
        uint32_t step = CLASSIC_W * 256 / rt_screen_w;
        if (CLASSIC_H * 256 / rt_screen_h < step) step = CLASSIC_H * 256 / rt_screen_h;
        resize_picture(pic, rt_screen_w, rt_screen_h, step, step, (CLASSIC_W * 256 - rt_screen_w * step) / 2,
                       (CLASSIC_H * 256 - rt_screen_h * step) / 2);
    } else {
        uint32_t new_w = w * rt_screen_w / CLASSIC_W;
        resize_picture(pic, new_w, rt_screen_h, w * 256 / new_w, CLASSIC_H * 256 / rt_screen_h, 0, 0);
    }
    cpu.eax = pic;
}

/* Coordinates of the classic screen on this screen: x and y of a left or top edge, xe and ye of a right
 * or bottom edge. */
static uint32_t sx(uint32_t v) { return scale(v, rt_screen_w, CLASSIC_W, 0); }
static uint32_t sy(uint32_t v) { return scale(v, rt_screen_h, CLASSIC_H, 0); }
static uint32_t sxe(uint32_t v) { return scale(v, rt_screen_w, CLASSIC_W, 1); }
static uint32_t sye(uint32_t v) { return scale(v, rt_screen_h, CLASSIC_H, 1); }

/* Calls a cdecl function of the original with up to 9 arguments. */
#define CALL(addr, ...) do { uint32_t a_[] = {__VA_ARGS__}; rt_callback(addr, (int)(sizeof a_ / 4), a_); } while (0)

/* The progress box 0x5e76f0, cdecl: (text, step, total); returns 1, or 0 when it draws nothing. It draws
 * straight onto the screen over the bottom bar, so the runtime draws it with the original's functions
 * at the scaled places: the panel (100, 555)-(790, 590), the text (120, 565)-(250, 580) and the bar
 * (270, 565)-(770, 580) filled from x 272 over 496 pixels, or the text alone over (120, 565)-(770, 580).
 * Step 0 first darkens the screen once (0x55f2c0 on the screen manager of the record). */
/* The engine log gets the time of each stage of a long job, for example a new game's "Player histories" or
 * "Initialising Leagues": a phone or a tablet takes far longer than a computer, and a player's engine log or error
 * report then says which stage took the time, or after which one the game stopped. A stage runs from its first
 * progress call to the next stage, or to its last call when the game then waits for the player (win_progress_idle),
 * so the player's time on a screen is not counted. Only the log: a stage under 2 s writes nothing. */
static char stage[64];
static double stage_first, stage_latest;

static double mono_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static void stage_close(double end)
{
    if (stage[0] && end - stage_first >= 2.0) fprintf(stderr, "recomp: progress: %s took %.1f s\n", stage, end - stage_first);
    stage[0] = 0;
}

static void progress_note(uint32_t text)
{
    double now = mono_seconds();
    char name[64] = "";
    if (text) snprintf(name, sizeof name, "%s", win_str(text));
    if (strcmp(name, stage)) {
        stage_close(now - stage_latest < 5.0 ? now : stage_latest);
        snprintf(stage, sizeof stage, "%s", name);
        stage_first = now;
    }
    stage_latest = now;
}

/* user32.c, idle_rest: the game waits for the player, so the stage in progress ended at its last call. */
void win_progress_idle(void)
{
    if (stage[0]) stage_close(stage_latest);
}

RT_REPLACE(005e76f0, progress_box)
{
    uint32_t text = rt_arg(0), step = rt_arg(1), total = rt_arg(2);
    progress_note(text);
    if (rd32(g_mem, 0xaebe0cu)) { rt_ret_cdecl(0); return; }        /* the display is off */
    rt_callback(0x68d520u, 0, NULL);                                /* the message pump */
    if (!text) { rt_ret_cdecl(0); return; }
    if (!step) {
        uint32_t record = (uint32_t)((int32_t)(int16_t)rd16(g_mem, 0xb77c7au) * 0x300);
        Cpu saved = cpu;
        cpu.ecx = rd32(g_mem, 0xb74c4cu + record);
        rt_callback(0x55f2c0u, 0, NULL);
        cpu = saved;
    }
    uint32_t grey = rd16(g_mem, 0xae317eu), light = rd16(g_mem, 0xae3184u), navy = rd16(g_mem, 0xaebe04u);
    CALL(0x5e5df0u, sx(100), sy(555), sxe(790), sye(590), 0x30, grey, 0);
    if ((int32_t)total > 0 && (int32_t)step <= (int32_t)total) {
        CALL(0x5e6d80u, sx(120), sy(565), sxe(250), sye(580), 0xc, 1, light, text, 0xffffffffu);
        CALL(0x5e5df0u, sx(270), sy(565), sxe(770), sye(580), 0x60, grey, 0);
        uint32_t left = sx(272), width = sxe(768) - left;
        int32_t fill = (int32_t)((int64_t)(int32_t)step * width / (int32_t)total);
        CALL(0x5e3b50u, left, sy(567), left + (uint32_t)fill, sye(578), 4, navy);
    } else {
        CALL(0x5e6d80u, sx(120), sy(565), sxe(770), sye(580), 0xc, 1, light, text, 0xffffffffu);
    }
    CALL(0x5e2eb0u, sx(100), sy(555), sxe(790), sye(590));                       /* flip */
    rt_ret_cdecl(1);
}
