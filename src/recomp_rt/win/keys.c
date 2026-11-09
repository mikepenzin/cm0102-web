/* The keys of the options space_to_continue and shortcuts: where on the screen of the game a key
 * presses. The key itself becomes a click of the left button at that place (user32.c), so the
 * game's own input code runs, as for a click with the mouse.
 *
 * The screen manager D is the one of the last input poll 0x55f620 (its ECX, kept by user32.c). Its widgets
 * are at D + 0xba95e + i * 0x18c, i < the s16 count at D + 0x12e9a0. Its areas are at D + i * 0xbf1,
 * i < the s16 count at D + 0x12e99e. The input mode is the dword D + 0 (1: a popup is open).
 * Source: docs/research/2026-10-13-input-inventory.md, sections 3 and 4.
 *
 * Widget fields: +0x08 action id (dword), +0x0c flags (dword, 2: takes clicks, 8: text entry),
 * +0x10..+0x1c rect x1, y1, x2, y2 (dwords, inclusive), +0x44 text cursor (dword, -1: not typing),
 * +0x7a owner area (s16, -1: none), +0x80 text (read in a test run).
 * Area fields: +0x04..+0x10 rect x1, y1, x2, y2 (dwords), +0x212 owner widget (s16, -1: top level),
 * +0xbed open (dword).
 *
 * The left bar (0x75db00) makes:
 * - Continue Game: a widget with flags 2 and action id 0x3e8 when the game can go on (0x75e3da..0x75e43d
 *   and 0x75e549..0x75e58d). When it cannot, the cell is a label: flags 1, id 0 (0x75e5a5..0x75e605).
 * - The Back arrow: a button with action id -2 in the arrow row (0x75e1f4..0x75e254; the start screen
 *   0x75dd31..0x75ddaa). The left bar input 0x7615d0 decides what -2 does (0x762c97). */
#include "win.h"

#include <stdio.h>
#include <string.h>

#define CONTINUE_ID 0x3e8
#define BACK_ID (-2)

uint32_t keys_display;

static uint32_t screen_manager(void) { return keys_display; }

static uint32_t widget_at(uint32_t d, int i) { return d + 0xba95eu + (uint32_t)i * 0x18cu; }
static uint32_t area_at(uint32_t d, int i) { return d + (uint32_t)i * 0xbf1u; }
static int widget_count(uint32_t d) { return (int16_t)rd16(g_mem, d + 0x12e9a0u); }
static int area_count(uint32_t d) { return (int16_t)rd16(g_mem, d + 0x12e99eu); }

/* 1 when the widget can take a click now: flag 2, and free or in an open top-level area. */
static int clickable(uint32_t d, uint32_t w)
{
    if (!(rd32(g_mem, w + 0x0c) & 2)) return 0;
    int owner = (int16_t)rd16(g_mem, w + 0x7a);
    if (owner < 0) return 1;
    if (owner >= area_count(d)) return 0;
    uint32_t a = area_at(d, owner);
    return rd32(g_mem, a + 0xbed) && (int16_t)rd16(g_mem, a + 0x212) == -1;
}

/* The left bar is the part of the screen left of x 90 of the classic screen. */
static int in_left_bar(uint32_t w)
{
    return (int32_t)rd32(g_mem, w + 0x18) < (int32_t)(90u * rt_screen_w / 800u);
}

/* The centre of a clickable widget of the left bar with this action id, in *x and *y. */
static int find_left_bar_button(int id, int *x, int *y)
{
    uint32_t d = screen_manager();
    if (!d || rd32(g_mem, d) != 0) return 0;          /* no screen, or a popup is open */
    int n = widget_count(d);
    if (n < 0 || n > 0x4b0) return 0;
    for (int i = 0; i < n; i++) {
        uint32_t w = widget_at(d, i);
        if ((int32_t)rd32(g_mem, w + 0x08) != id || !in_left_bar(w) || !clickable(d, w)) continue;
        *x = ((int32_t)rd32(g_mem, w + 0x10) + (int32_t)rd32(g_mem, w + 0x18)) / 2;
        *y = ((int32_t)rd32(g_mem, w + 0x14) + (int32_t)rd32(g_mem, w + 0x1c)) / 2;
        return 1;
    }
    return 0;
}

/* 1 while a text entry takes keys: a widget with flag 8 and a cursor. */
int keys_typing(void)
{
    uint32_t d = screen_manager();
    if (!d) return 0;
    int n = widget_count(d);
    if (n < 0 || n > 0x4b0) return 0;
    for (int i = 0; i < n; i++) {
        uint32_t w = widget_at(d, i);
        if ((rd32(g_mem, w + 0x0c) & 8) && (int32_t)rd32(g_mem, w + 0x44) >= 0) return 1;
    }
    return 0;
}

/* The middle row of the text entry that takes keys, in rows of the picture; -1 when none does. For the page of the
 * browser build (web/keyboard.js), which keeps that row above the on-screen keyboard of a touch device. */
int keys_typing_row(void)
{
    uint32_t d = screen_manager();
    if (!d) return -1;
    int n = widget_count(d);
    if (n < 0 || n > 0x4b0) return -1;
    for (int i = 0; i < n; i++) {
        uint32_t w = widget_at(d, i);
        if ((rd32(g_mem, w + 0x0c) & 8) && (int32_t)rd32(g_mem, w + 0x44) >= 0)
            return ((int32_t)rd32(g_mem, w + 0x14) + (int32_t)rd32(g_mem, w + 0x1c)) / 2;
    }
    return -1;
}

int keys_continue_game(int *x, int *y) { return !keys_typing() && find_left_bar_button(CONTINUE_ID, x, y); }

int keys_back(int *x, int *y) { return !keys_typing() && find_left_bar_button(BACK_ID, x, y); }

/* The text of a widget, at +0x80, with a 0 at the end. A new line splits the text of a two-line button,
 * for example "Kick Off\nMatch" (read from the left bar of the match screen in a test run). */
static const char *label(uint32_t w) { return (const char *)(g_mem + w + 0x80); }

/* 1 when a clickable button of the left bar has a text that starts with this prefix. */
int keys_left_bar_label(const char *prefix)
{
    uint32_t d = screen_manager();
    if (!d || rd32(g_mem, d) != 0) return 0;
    int n = widget_count(d);
    if (n < 0 || n > 0x4b0) return 0;
    for (int i = 0; i < n; i++) {
        uint32_t w = widget_at(d, i);
        if (in_left_bar(w) && clickable(d, w) && !strncmp(label(w), prefix, strlen(prefix))) return 1;
    }
    return 0;
}

/* The texts of the clickable buttons of the left bar, for RECOMP_SCREEN_LOG. */
void keys_left_bar_labels(char *out, size_t size)
{
    uint32_t d = screen_manager();
    size_t used = 0;
    out[0] = 0;
    if (!d || rd32(g_mem, d) != 0) return;
    int n = widget_count(d);
    if (n < 0 || n > 0x4b0) return;
    for (int i = 0; i < n && used + 20 < size; i++) {
        uint32_t w = widget_at(d, i);
        if (!in_left_bar(w) || !clickable(d, w) || !label(w)[0]) continue;
        size_t at = used + (used ? 1 : 0);
        used += (size_t)snprintf(out + used, size - used, "%s%.16s", used ? "|" : "", label(w));
        for (size_t k = at; k < used; k++)
            if (out[k] == '\n') out[k] = ' ';
    }
}

/* A place outside every open popup, while a popup is open. In this mode a click that hits no row
 * of a popup closes it (0x560c30). */
int keys_outside_popup(int *x, int *y)
{
    uint32_t d = screen_manager();
    if (!d || rd32(g_mem, d) != 1) return 0;
    int n = area_count(d);
    if (n < 0 || n > 0xfa) return 0;
    const int cx[4] = {(int)rt_screen_w - 1, (int)rt_screen_w - 1, (int)rt_screen_w / 2, 0};
    const int cy[4] = {(int)rt_screen_h - 1, 0, (int)rt_screen_h - 1, (int)rt_screen_h - 1};
    for (int k = 0; k < 4; k++) {
        int inside = 0;
        for (int i = 0; i < n && !inside; i++) {
            uint32_t a = area_at(d, i);
            if (!rd32(g_mem, a + 0xbed) || (int16_t)rd16(g_mem, a + 0x212) == -1) continue;
            inside = cx[k] >= (int32_t)rd32(g_mem, a + 4) && cx[k] <= (int32_t)rd32(g_mem, a + 0xc) &&
                     cy[k] >= (int32_t)rd32(g_mem, a + 8) && cy[k] <= (int32_t)rd32(g_mem, a + 0x10);
        }
        if (!inside) { *x = cx[k]; *y = cy[k]; return 1; }
    }
    return 0;
}
