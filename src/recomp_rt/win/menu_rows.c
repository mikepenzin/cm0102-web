/* A competitions menu of 27 rows at most for a nation (O259, guard B6, the day screen check of 27 October 2026).
 *
 * Area::layout 0x403350 stops with "Area..cpp 507" for an area of more than 30 rows (+0xbac, 0x4033ec..0x403451).
 * The Competitions menu of the side bar (Nations A-J and K-Z, one sub menu for each nation) is built by the function
 * that calls ScreenManager::add_menu_area 0x415600 and add_menu_item 0x415640 at 0x75f836..0x75f977: one item for
 * each competition of the nation (link type 2000, 0x75f883), then a separator (0x761260) and the Awards item (link
 * 2004, 0x75f8de). The menu of Brazil has 29 competitions since slice 20e (Serie A, B and C, the 24 state
 * championships and the two cups), so with the separator and Awards it wants 31 rows. The original's Brazil has 17
 * competitions, and no other nation of the original has more. (Found with a test log of add_menu_item on the areas
 * 16 and 26, the two menus of the guard line, which came from the first draw after each start or load: the area
 * create call 0x415638, rect 0, then the 31 items.)
 *
 * A menu also has to fit the screen: a row is 21 pixels (the 31 rows of Brazil gave a rect of 193 x 651 pixels, more
 * than the 600 of the classic screen, and the first hover over Brazil with the clamp of 30 rows stopped with the box
 * "Area..cpp 838"). So in a season game the 25th competition item of a menu opens a sub menu "More" (an item with a
 * child area, as the nation items have), and the competitions after it go into that area. The menu then has 24
 * competitions, More, the separator and Awards: 27 rows (567 pixels) at most for a nation menu. The sub menu has the
 * rest. A continent entry (World Cup, International and the like) can add a second separator, Awards and one more
 * item, so its menu can reach 28 to 30 rows with More (today the International menu has 9 items). Menus with fewer
 * competitions, and the base game, are not touched. */
#include "win.h"
#include "season.h"
#include "season_comps.h"

#include <stdio.h>
#include <string.h>

#define ADD_MENU_AREA 0x415600u         /* thiscall (parent item), ret 4: the child area of an item */
#define ADD_MENU_ITEM 0x415640u         /* thiscall (area, text, link type, link data), ret 0x10: the item */
#define SCREEN_MANAGER 0xb74450u
#define LINK_COMPETITION 2000
#define MAIN_ITEMS 24                   /* competition items in a menu before More: 24 + More + separator + Awards = 27 rows */
#define AREAS 256
#define F_MALLOC 0x00944e46u            /* cdecl (size) */

void o_00415600(void);
void o_00415640(void);

static int16_t redirect[AREAS];         /* the child area that takes the competitions of this area, or -1 */
static int16_t count[AREAS];            /* the competition items added to this area */
static uint32_t more_text;              /* "More" in the game's memory */

/* Calls the original of ScreenManager::add_menu_item: the original sees the arguments as its own call. */
static uint32_t add_item(uint32_t area, uint32_t text, uint32_t link, uint32_t data)
{
    Cpu saved = cpu;
    uint32_t args[4] = {area, text, link, data};
    cpu.ecx = SCREEN_MANAGER;
    uint32_t r = rt_callback(ADD_MENU_ITEM, 4, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t add_area(uint32_t item)
{
    Cpu saved = cpu;
    uint32_t args[1] = {item};
    cpu.ecx = SCREEN_MANAGER;
    uint32_t r = rt_callback(ADD_MENU_AREA, 1, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* ScreenManager::add_menu_area 0x415600: a new area starts with no items and no sub menu. */
RT_WRAP(00415600, menu_rows_area)
{
    o_00415600();
    int area = (int16_t)cpu.eax;
    if (area >= 0 && area < AREAS) {
        count[area] = 0;
        redirect[area] = -1;
    }
}

/* The area that takes the next competition item of `area`: the area itself while it has room, else its sub menu
 * More (made at the first overflow), else the sub menu of that. */
static uint32_t area_for_next(uint32_t area)
{
    for (int guard = 0; guard < 8 && !rt_unwinding; guard++) {
        if (area >= AREAS) return area;
        if (redirect[area] >= 0) {
            area = (uint32_t)redirect[area];
            continue;
        }
        if (count[area] < MAIN_ITEMS) {
            count[area]++;
            return area;
        }
        if (!more_text) {
            uint32_t sz[1] = {8};
            more_text = rt_callback(F_MALLOC, 1, sz);
            if (rt_unwinding || more_text < 0x10000u) return area;
            memcpy(g_mem + more_text, "More", 5);
        }
        uint32_t item = add_item(area, more_text, 0, 0);
        uint32_t child = rt_unwinding ? 0xffffu : add_area(item);
        if (rt_unwinding || child >= AREAS) return area;
        redirect[area] = (int16_t)child;
        count[child] = 0;
        redirect[child] = -1;
        fprintf(stderr, "recomp: menu rows: a menu of more than %d competitions has a sub menu More (area %u, item %u)\n",
                MAIN_ITEMS, area, item);
        area = child;
    }
    return area;
}

/* ScreenManager::add_menu_item 0x415640 (thiscall: area, text, link type, link data). */
RT_WRAP(00415640, menu_rows_item)
{
    uint32_t area = rt_arg(0) & 0xffffu;
    if ((int16_t)rt_arg(0) >= 0 && (int)rt_arg(2) == LINK_COMPETITION && season_comps_on()) {
        uint32_t to = area_for_next(area);
        if (rt_unwinding) return;
        if (to != area) wr32(g_mem, cpu.esp + 4, to);
    }
    o_00415640();
}

/* The arrow menu of a competition screen and its history screen (issue 163). comp_screens_add_related_comps_menu
 * 0x49d070 lists the objects of one league table entry, and asks key_nation_index_of_comp 0x669470 which (0x49d10d).
 * That function knows the world club competitions by the key ids of the Intercontinental Cup and the Club World
 * Championship (0x669570, 0x66957a: entry 2). Any other record with no nation and no continent runs through its loop
 * and gets the fallback entry 3, Europe (0x6695ac). The FIFA Intercontinental Cup is such a record, made for entry 2
 * (season_club_cups.c), so its menu listed the UEFA competitions. For this one call the menu takes the entry that
 * holds the competition. The other callers of 0x669470 keep its answer. */
#define MENU_ENTRY_RET 0x49d112u
#define ENTRIES 0xb63d60u               /* league table entries, 0x48 bytes */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35u

static int entry_holds_record(uint32_t entry, uint32_t rec)
{
    uint32_t e = ENTRIES + entry * ENTRY_SIZE, list = rd32(g_mem, e + 0x10);
    int32_t n = (int32_t)rd32(g_mem, e + 0xc);
    for (int32_t k = 0; list >= 0x10000u && k < n; k++) {
        uint32_t obj = rd32(g_mem, list + 4u * (uint32_t)k);
        if (obj >= 0x10000u && rd32(g_mem, obj + 4) == rec) return 1;
    }
    return 0;
}

void o_00669470(void);

RT_WRAP(00669470, comp_menu_entry)
{
    uint32_t ret = rd32(g_mem, cpu.esp), rec = rt_arg(0);
    o_00669470();
    if (ret != MENU_ENTRY_RET || rt_unwinding || !season_comps_on()) return;
    uint32_t asked = cpu.eax & 0xffu;
    if (asked < ENTRY_COUNT && entry_holds_record(asked, rec)) return;
    for (uint32_t e = 0; e < ENTRY_COUNT; e++) {
        if (!entry_holds_record(e, rec)) continue;
        cpu.eax = (cpu.eax & 0xffffff00u) | e;
        return;
    }
}
