/* The language table of PlayerStats without a full sort for each new entry (docs/research/2026-11-10-language-sort.md).
 *
 * PlayerStats::add_language 0x7c6a30, thiscall (staff, language, fluency, sort) ret 0x10, appends an entry of 6 bytes
 * to the table at this+0x3c (count this+0x44, room this+0x40): the staff id ([staff], 0x7c6b7f..0x7c6b88), the
 * language (+4) and the fluency at most 10 (+5). With sort nonzero it then sorts the whole table with qsort 0x945333
 * (the call at 0x7c6bda), comparator 0x7c6d40: staff id first (their difference), then the language as a signed
 * byte, and 0 when either language is 0xff.
 *
 * At the start of a game PlayerStats::PlayerStats (0x7bfd50) runs the loop 0x7c00fe..0x7c0271 over every person
 * (stride 0x6e, count [0xae2364]): the languages of his nations go in with sort 0 (0x7c015a), the language of his
 * club's nation with sort 1 (0x7c0207, 0x7c0251) when a binary search (0x946464 at 0x7c0228) does not find it. So
 * each such person sorts the whole table again. With the 175,044 people of a 2026 database that is most of the
 * creation of a game: in Chrome 155 s, of which qsort, the comparator and the calls between them took about 90 s.
 *
 * The wrap: for an entry with sort, when no two entries of the table and the new one compare equal under 0x7c6d40,
 * the sorted order of the table is the only one, so any correct sort gives the bytes that qsort gives. The wrap then
 * calls the original with sort 0 and puts the new entry, and the unsorted entries before it, in their places. When
 * two entries compare equal (a language 0xff, or the same language twice), or the table is not as expected, the
 * original runs as it is. Test switch RECOMP_LANGUAGE_SORT=off: always the original. */
#include "win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE 0x3cu
#define ROOM 0x40u
#define COUNT 0x44u
#define ENTRY 6u
#define QSORT 0x00945333u        /* qsort (base, count, size, compare), cdecl */
#define LANGUAGE_COMPARE 0x007c6d40u
#define MAX_TAIL 64             /* unsorted entries at the end that the wrap places itself; more: the original */

/* The comparator 0x7c6d40 on two entries. */
static int compare(const uint8_t *a, const uint8_t *b)
{
    uint32_t sa = rd32(a, 0), sb = rd32(b, 0);
    if (sa != sb) return (int32_t)(sa - sb);
    if (a[4] == 0xff || b[4] == 0xff) return 0;
    return (int)(int8_t)a[4] - (int)(int8_t)b[4];
}

/* A staff id that the subtraction of the comparator orders right against any other. */
static int plain_id(const uint8_t *e) { return rd32(e, 0) < 0x40000000u; }

static int sort_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_LANGUAGE_SORT");
        on = !(v && !strcmp(v, "off"));
    }
    return on;
}

void o_007c6a30(void);

RT_WRAP(007c6a30, add_language)
{
    uint32_t self = cpu.ecx, staff = rt_arg(0), sort = rt_arg(3);
    uint32_t table = rd32(g_mem, self + TABLE), n = rd32(g_mem, self + COUNT), room = rd32(g_mem, self + ROOM);
    if (!sort || !sort_on() || !table || !staff || n == 0 || n > room || n > 0x7fffffffu / ENTRY) {
        o_007c6a30();
        return;
    }
    /* The new entry as the original writes it. */
    uint8_t add[ENTRY];
    int8_t fluency = (int8_t)rt_arg(2);
    wr32(add, 0, rd32(g_mem, staff));
    add[4] = (uint8_t)rt_arg(1);
    add[5] = (uint8_t)(fluency > 10 ? 10 : fluency);
    if (!plain_id(add)) {
        o_007c6a30();
        return;
    }
    /* The sorted part: the longest start of the table whose neighbours compare below 0. */
    const uint8_t *t = g_mem + table;
    uint32_t p = 1;
    if (!plain_id(t)) {
        o_007c6a30();
        return;
    }
    while (p < n && plain_id(t + p * ENTRY) && compare(t + (p - 1) * ENTRY, t + p * ENTRY) < 0) p++;
    uint32_t k = n - p + 1;     /* the entries after it, and the new one */
    if (k > MAX_TAIL) {
        o_007c6a30();
        return;
    }
    uint8_t tail[MAX_TAIL][ENTRY];
    for (uint32_t i = 0; i + 1 < k; i++) {
        if (!plain_id(t + (p + i) * ENTRY)) {
            o_007c6a30();
            return;
        }
        memcpy(tail[i], t + (p + i) * ENTRY, ENTRY);
    }
    memcpy(tail[k - 1], add, ENTRY);
    /* The tail in order (insertion sort), each entry below the next. */
    for (uint32_t i = 1; i < k; i++)
        for (uint32_t j = i; j > 0 && compare(tail[j - 1], tail[j]) > 0; j--) {
            uint8_t x[ENTRY];
            memcpy(x, tail[j], ENTRY);
            memcpy(tail[j], tail[j - 1], ENTRY);
            memcpy(tail[j - 1], x, ENTRY);
        }
    uint32_t at[MAX_TAIL];
    for (uint32_t i = 0; i < k; i++) {
        if (i && compare(tail[i - 1], tail[i]) >= 0) {
            o_007c6a30();
            return;
        }
        /* Its place in the sorted part, and no entry there that compares equal. */
        uint32_t lo = 0, hi = p;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            if (compare(t + mid * ENTRY, tail[i]) < 0) lo = mid + 1;
            else hi = mid;
        }
        if ((lo < p && compare(t + lo * ENTRY, tail[i]) <= 0) || (lo > 0 && compare(t + (lo - 1) * ENTRY, tail[i]) >= 0)) {
            o_007c6a30();
            return;
        }
        at[i] = lo;
    }
    /* The original appends the entry (and grows the table) without its sort. */
    wr32(g_mem, cpu.esp + 4 + 12, 0);
    o_007c6a30();
    if (rt_unwinding) return;
    table = rd32(g_mem, self + TABLE);
    if (!table || rd32(g_mem, self + COUNT) != n + 1) return;   /* not appended: nothing to put in place */
    if (memcmp(g_mem + table + n * ENTRY, add, ENTRY)) {
        /* Not the entry this file expects: the sort of the original, as at 0x7c6bda. */
        uint32_t args[4] = {table, n + 1, ENTRY, LANGUAGE_COMPARE};
        rt_callback(QSORT, 4, args);
        return;
    }
    /* From the end: the sorted part's entries move up past the tail entries that go below them. */
    uint8_t *w = g_mem + table;
    uint32_t src = p, dst = n + 1;
    for (uint32_t i = k; i-- > 0;) {
        uint32_t moved = src - at[i];
        dst -= moved;
        src -= moved;
        memmove(w + dst * ENTRY, w + src * ENTRY, moved * ENTRY);
        dst--;
        memcpy(w + dst * ENTRY, tail[i], ENTRY);
    }
}
