/* International caps and goals above 255 (0.6.2 slice 12 and item 66, caps_wide.h). */
#include "caps_wide.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t id;
    uint32_t count;          /* 256 or more */
} Wide;

typedef struct {
    Wide *rows;
    int n, room;
} Store;

static Store caps, goals;

/* Few players pass 255, so a list is enough. */
static int find(const Store *s, uint32_t id)
{
    for (int i = 0; i < s->n; i++)
        if (s->rows[i].id == id) return i;
    return -1;
}

static void drop(Store *s, int i)
{
    s->rows[i] = s->rows[--s->n];
}

static Wide *add(Store *s, uint32_t id)
{
    if (s->n == s->room) {
        int room = s->room ? 2 * s->room : 64;
        Wide *p = realloc(s->rows, (size_t)room * sizeof *p);
        if (!p) return NULL;
        s->rows = p;
        s->room = room;
    }
    Wide *w = &s->rows[s->n++];
    w->id = id;
    w->count = CAPS_BYTE_MAX;
    return w;
}

/* The new byte after n more for the player id with this byte. A byte that stays below 255 or reaches exactly 255
 * from below keeps no stored count (one left there belongs to an earlier player with this id). */
static uint32_t store_add(Store *s, uint32_t id, uint32_t byte, uint32_t n)
{
    byte &= 0xffu;
    int i = find(s, id);
    if (byte < CAPS_BYTE_MAX) {
        if (i >= 0) drop(s, i);
        if (byte + n <= CAPS_BYTE_MAX) return byte + n;
        i = -1;
    }
    if (!n && i < 0) return byte;
    Wide *w = i >= 0 ? &s->rows[i] : add(s, id);
    if (w) {
        uint32_t from = i >= 0 ? w->count : byte;
        w->count = from > 0xffffffffu - n ? 0xffffffffu : from + n;
    }
    return CAPS_BYTE_MAX;
}

static uint32_t store_full(const Store *s, uint32_t id, uint32_t byte)
{
    byte &= 0xffu;
    if (byte < CAPS_BYTE_MAX) return byte;
    int i = find(s, id);
    return i >= 0 ? s->rows[i].count : byte;
}

uint32_t caps_wide_add(uint32_t id, uint32_t byte) { return store_add(&caps, id, byte, 1); }
uint32_t caps_wide_full(uint32_t id, uint32_t byte) { return store_full(&caps, id, byte); }
int caps_wide_count(void) { return caps.n; }
uint32_t goals_wide_add(uint32_t id, uint32_t byte, uint32_t n) { return store_add(&goals, id, byte, n & 0xffu); }
uint32_t goals_wide_full(uint32_t id, uint32_t byte) { return store_full(&goals, id, byte); }
int goals_wide_count(void) { return goals.n; }

void caps_wide_reset(void) { caps.n = goals.n = 0; }

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static uint32_t field_size(const Store *s) { return 4u + 8u * (uint32_t)s->n; }

static void field_write(const Store *s, uint8_t *out)
{
    put32(out, (uint32_t)s->n);
    for (int i = 0; i < s->n; i++) {
        put32(out + 4 + 8 * (uint32_t)i, s->rows[i].id);
        put32(out + 8 + 8 * (uint32_t)i, s->rows[i].count);
    }
}

/* Reads exactly n bytes of a field into s: 0 for damaged bytes. */
static int field_read(Store *s, const uint8_t *in, uint32_t n)
{
    s->n = 0;
    if (n < 4) return 0;
    uint32_t count = get32(in);
    if (count > (n - 4) / 8 || n != 4 + 8 * count) return 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t id = get32(in + 4 + 8 * i), c = get32(in + 8 + 8 * i);
        Wide *w = c > CAPS_BYTE_MAX && find(s, id) < 0 ? add(s, id) : NULL;
        if (!w) {
            s->n = 0;
            return 0;
        }
        w->count = c;
    }
    return 1;
}

int caps_wide_form(void) { return goals.n ? 4 : caps.n ? 3 : 0; }

uint32_t caps_wide_part_size(void)
{
    int form = caps_wide_form();
    return form == 4 ? field_size(&caps) + field_size(&goals) : form == 3 ? field_size(&caps) : 0u;
}

void caps_wide_part_write(uint8_t *out)
{
    int form = caps_wide_form();
    if (form >= 3) field_write(&caps, out);
    if (form == 4) field_write(&goals, out + field_size(&caps));
}

/* Version 5 and later write both fields whatever their counts (season_change.c). */
uint32_t caps_wide_part_size_all(void) { return field_size(&caps) + field_size(&goals); }

void caps_wide_part_write_all(uint8_t *out)
{
    field_write(&caps, out);
    field_write(&goals, out + field_size(&caps));
}

/* The fields of version form (3: the caps; 4 and later: the caps, then the goals, then a later version's own fields,
 * which the read leaves). */
int caps_wide_part_read(const uint8_t *in, uint32_t n, uint32_t form)
{
    caps_wide_reset();
    if (form < 3) return 1;
    uint32_t c = n;
    if (form > 3 && n >= 4 && get32(in) <= (n - 4) / 8) c = 4 + 8 * get32(in);
    if (!field_read(&caps, in, c)) return 0;
    if (form == 3) return 1;
    uint32_t g = n - c;
    if (form > 4 && g >= 4 && get32(in + c) <= (g - 4) / 8) g = 4 + 8 * get32(in + c);
    if (!field_read(&goals, in + c, g)) {
        caps_wide_reset();
        return 0;
    }
    return 1;
}
