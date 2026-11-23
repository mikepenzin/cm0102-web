/* Qualifying campaigns in the original's shape (0.6.2 slice 4b, open item 58; season_intl.h). Reads only the
 * competition table [0xadadfc] and the objects' year +0x40, so ctest season_part links it with its own memory.
 *
 * A save of 0.6.0 or 0.6.1 after a table end holds qualifying campaigns that its build made in the original's shape:
 * it had no edition after the table, so every gate of the 48-team World Cup qualifying (season_wcq26) and of the Euro
 * qualifying of 24 (season_intl_euroq.c format_of, season_nl_euro.c moved) was off. 0.6.2 sees the continued
 * edition, so the same gates would switch shapes in the middle of the campaign. The load repair (repair_idle) marks
 * such a running campaign: its object and its campaign year (+0x40). While the object holds that year the gates keep
 * the original's shape; the object's next campaign has another year and takes the new shape. The marks go into the
 * season part of the save (version 5, season_change.c), as the record id of the object (its slot in the table
 * [0xadadfc]) and the year, so a save of 0.6.2 reloads with them. A save without the field has no mark. */

#include "win.h"
#include "season_intl.h"

#include <stdio.h>
#include <string.h>

#define COMP_SLOTS 0xadadf8u            /* the slots of the competition object table */
#define COMP_TABLE 0xadadfcu

static int readable(uint32_t p) { return rt_readable(p); }

#define MAX_MARKS 16
static struct { uint32_t id; int year; } marks[MAX_MARKS];
static int mark_count;

static uint32_t object_of(uint32_t id)
{
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int slots = (int)rd32(g_mem, COMP_SLOTS);
    return readable(table) && id < (uint32_t)slots && slots < 100000 ? rd32(g_mem, table + 4u * id) : 0;
}

int season_intl_old_shape(uint32_t obj)
{
    if (!readable(obj)) return 0;
    for (int i = 0; i < mark_count; i++)
        if (object_of(marks[i].id) == obj) return (int16_t)rd16(g_mem, obj + 0x40u) == marks[i].year;
    return 0;
}

void season_intl_mark_old(uint32_t obj)
{
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int slots = (int)rd32(g_mem, COMP_SLOTS);
    for (uint32_t id = 0; readable(table) && mark_count < MAX_MARKS && id < (uint32_t)slots && slots < 100000; id++)
        if (rd32(g_mem, table + 4u * id) == obj) {
            marks[mark_count].id = id;
            marks[mark_count++].year = (int16_t)rd16(g_mem, obj + 0x40u);
            fprintf(stderr, "recomp: season intl: a save of 0.6.1 or older: the campaign %d of record %u keeps the "
                            "original's shape\n", (int16_t)rd16(g_mem, obj + 0x40u), id);
            return;
        }
}

void season_intl_marks_reset(void) { mark_count = 0; }

int season_intl_marks_count(void) { return mark_count; }

uint32_t season_intl_mark_object(int i)
{
    if (i < 0 || i >= mark_count) return 0;
    uint32_t obj = object_of(marks[i].id);
    return readable(obj) && (int16_t)rd16(g_mem, obj + 0x40u) == marks[i].year ? obj : 0;
}

/* The live marks: an object whose year moved on has no mark any more. */
static int live_mark(int i)
{
    uint32_t obj = object_of(marks[i].id);
    return readable(obj) && (int16_t)rd16(g_mem, obj + 0x40u) == marks[i].year;
}

uint32_t season_intl_marks_size(void)
{
    uint32_t n = 0;
    for (int i = 0; i < mark_count; i++) n += (uint32_t)live_mark(i);
    return n ? 4u + 8u * n : 0u;
}

void season_intl_marks_write(uint8_t *out)
{
    uint32_t n = 0, at = 4;
    for (int i = 0; i < mark_count; i++) {
        if (!live_mark(i)) continue;
        uint32_t id = marks[i].id, year = (uint32_t)marks[i].year;
        memcpy(out + at, &id, 4);
        memcpy(out + at + 4, &year, 4);
        at += 8;
        n++;
    }
    memcpy(out, &n, 4);
}

int season_intl_marks_read(const uint8_t *in, uint32_t n)
{
    mark_count = 0;
    if (n < 4) return 0;
    uint32_t count;
    memcpy(&count, in, 4);
    if (count > MAX_MARKS || (n - 4) / 8 < count) return 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t id, year;
        memcpy(&id, in + 4 + 8 * i, 4);
        memcpy(&year, in + 8 + 8 * i, 4);
        if (id > 0xffffu || year > 32767u) return 0;
        marks[mark_count].id = id;
        marks[mark_count++].year = (int)year;
    }
    return 1;
}

