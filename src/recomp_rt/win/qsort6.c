/* The qsort of the original's CRT (MSVC 6, 0x945333) in host code, for the player history lists.
 *
 * Game creation adds the rows of staff_history.dat to lists of 6-byte rows with 0x7c6a30, and that
 * function sorts the whole list again after each row (qsort 0x945333 with the compare 0x7c6d40).
 * The recompiled qsort makes each compare a call through the runtime, and with all nations selected
 * the "Player histories" step took 204 s (2001/02) and 260 s (2025/26) on the Mac
 * (docs/research/2026-10-29-new-game-speed.md). qsort is not stable, so this is a copy of the same
 * steps, read from the code at 0x945333 (qsort), 0x945487 (the short sort below 9 rows) and 0x9454d5
 * (the swap), not a different sort. The self-test (RECOMP_QSORT_TEST, crt.c) runs both on the same rows.
 *
 * The rows are not all that the original changes. It also leaves values in eax and on the stack of the
 * game: its frame, the frames of the short sort, the swap and the compare below esp, and its first two
 * arguments, which it uses as the pointers into its own stack of ranges. Later code reads some of these
 * stack bytes before it writes them: with the rows alone the same-save check from start.sav gave
 * another general.dat. So qsort6 also writes each of these stack words, in the same order, and gives
 * back the eax of the original (0x7c6a30 returns it, and its caller at 0x7c0166 sets only al).
 *
 * Pointer compares are unsigned, as in the original (jb, ja, jbe, jae), and the size test after the
 * partition is signed (jl at 0x945420).
 *
 * Rows in strict order (item 151b). 0x7c6a30 sorts the whole list again after each new row, and with the
 * December 2025 data set of Max's game the list holds about 59,000 rows at each of 28,339 sorts. Before 27,000
 * of those sorts the list is already in strict order: the rows before the new row have no two rows that
 * compare equal, and the new row compares above the last. When st->sorted says so, the steps of the sort are
 * known without a compare, and qsort6 takes them with the same stack writes, swaps and eax:
 *  - the partition: the middle row P goes to lo. The rows from lo + 1 to mid - 1 and the old lo row (now at
 *    mid) compare below P, and the row at mid + 1 above it, so the up scan stops at mid + 1. The rows from hi
 *    down to mid + 1 compare above P and the row at mid below it, so the down scan stops at mid. down < up
 *    ends the loop at once, and the swap of lo and mid puts the rows back in order. Both parts are in order.
 *  - the short sort: the largest row of lo..hi is always the row at hi, after a search that ends at hi + 1.
 * A strict order needs a compare that is an order: the id compare is the 32-bit difference (wrong for ids
 * 0x80000000 or more apart), and 0xff makes two rows of one id equal. qsort6_strict asks for ids below
 * 0x80000000 and each row below the next. Then rows of one id have no 0xff byte (or are alone), and any two
 * rows compare as their places in the list. */
#include "qsort6.h"

#include <stddef.h>

int qsort6_history_cmp(const uint8_t *a, const uint8_t *b)
{
    uint32_t x = (uint32_t)a[0] | (uint32_t)a[1] << 8 | (uint32_t)a[2] << 16 | (uint32_t)a[3] << 24;
    uint32_t y = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    if (x != y) return (int32_t)(x - y);
    if (a[4] == 0xff || b[4] == 0xff) return 0;
    return (int8_t)a[4] - (int8_t)b[4];
}

/* The original's qsort frame: ebp = q, so its return address is at q + 4. */
typedef struct {
    const qsort6_stack *st;
    uint8_t *rows;
    uint32_t width;
} Sort;

static inline uint32_t addr(const Sort *s, const uint8_t *p)
{
    return s->st->rows + (uint32_t)(p - s->rows);
}

static inline void put(const Sort *s, int32_t off, uint32_t v)
{
    uint8_t *m = s->st->mem + (uint32_t)(s->st->q + (uint32_t)off);
    m[0] = (uint8_t)v; m[1] = (uint8_t)(v >> 8); m[2] = (uint8_t)(v >> 16); m[3] = (uint8_t)(v >> 24);
}

/* A compare. The original pushes its two arguments and the return address, and 0x7c6d40 pushes esi, at the
 * four stack words from 0x108 (the partition) or 0x12c (the short sort) below q. Each loop of compares ends
 * with pushes to all four words before any code reads the stack (the partition 0x9453f3..0x9453ff or
 * 0x9453f3..0x94540a, the short sort 0x9454bb..0x9454c0), so the words of the compares are left out and the
 * stack ends the same (0.6.2 slice 14). */
static inline int compare(uint8_t *a, uint8_t *b)
{
    return qsort6_history_cmp(a, b);
}

/* The swap 0x9454d5 after its three arguments are on the stack; at is the offset of its return
 * address. Gives back its eax. */
static uint8_t *swap(const Sort *s, int32_t at, uint8_t *a, uint8_t *b, uint32_t esi, uint32_t ebx)
{
    put(s, at - 4, esi);
    if (a == b) return a;
    put(s, at - 8, ebx);
    for (uint32_t w = s->width; w--; ) {
        uint8_t t = *a;
        *a++ = *b;
        *b++ = t;
    }
    return a;
}

/* The short sort 0x945487, called from 0x945389 with lo, hi, width and the compare. */
static uint8_t *short_sort(const Sort *s, uint8_t *lo, uint8_t *hi, uint32_t q_ebx, uint32_t q_esi, uint32_t q_edi)
{
    uint32_t w = s->width;
    put(s, -0x108, s->st->cmp);
    put(s, -0x10c, w);
    put(s, -0x110, addr(s, hi));
    put(s, -0x114, addr(s, lo));
    put(s, -0x118, 0x94538eu);
    put(s, -0x11c, s->st->q);                       /* push ebp */
    put(s, -0x120, q_edi);
    uint8_t *eax = lo;
    if (hi <= lo) return eax;
    put(s, -0x124, q_ebx);
    put(s, -0x128, q_esi);
    put(s, -0x110, addr(s, lo + w));                /* mov [ebp+0xc], eax */
    while (hi > lo) {
        uint8_t *max = lo, *p;
        if (s->st->sorted) max = hi, p = hi + w;    /* item 151b: the largest row is the last */
        else
            for (p = lo + w; p <= hi; p += w)
                if (compare(p, max) > 0) max = p;
        put(s, -0x12c, w);
        put(s, -0x130, addr(s, hi));
        put(s, -0x134, addr(s, max));
        put(s, -0x138, 0x9454c5u);
        eax = swap(s, -0x138, max, hi, addr(s, p), addr(s, max));
        hi -= w;
    }
    return eax;
}

uint8_t *qsort6(uint8_t *base, uint32_t num, uint32_t width, const qsort6_stack *st)
{
    Sort s = {st, base, width};
    uint8_t *lo_stack[30], *hi_stack[30], *eax = NULL;
    uint32_t q = st->q;
    put(&s, 0, st->ebp);                            /* push ebp */
    put(&s, -0xfc, st->ebx);
    put(&s, -0x100, st->esi);
    put(&s, -0x104, st->edi);
    if (num < 2 || !width) return NULL;
    int top = 0;
    put(&s, -4, 0);
    put(&s, 8, q - 0x80);                           /* the pointer into the his of the ranges */
    put(&s, 0xc, q - 0xf8);                         /* and into the los */
    uint8_t *lo = base, *hi = base + (size_t)width * (num - 1);
    for (;;) {
        uint32_t size = (uint32_t)(hi - lo) / width + 1;
        uint32_t lo_a = addr(&s, lo), hi_a = addr(&s, hi);
        if (size <= 8) {
            eax = short_sort(&s, lo, hi, lo_a, hi_a, width);
        } else {
            uint8_t *mid = lo + (size / 2) * width;
            put(&s, -0x108, width);
            put(&s, -0x10c, lo_a);
            put(&s, -0x110, addr(&s, mid));
            put(&s, -0x114, 0x9453c1u);
            swap(&s, -0x114, mid, lo, hi_a, lo_a);
            uint8_t *up = lo, *down = hi + width;
            put(&s, -8, lo_a);
            for (;;) {
                if (st->sorted) up = mid + width;  /* item 151b: the scans stop next to the old middle */
                else
                    for (;;) {                      /* each step writes up at q - 8: the last one stays */
                        up += width;
                        if (up > hi) break;
                        if (compare(up, lo) > 0) break;
                    }
                put(&s, -8, addr(&s, up));
                if (st->sorted) down = mid;
                else
                    for (;;) {
                        down -= width;
                        if (down <= lo) break;
                        if (compare(down, lo) < 0) break;
                    }
                put(&s, -0x108, width);
                put(&s, -0x10c, addr(&s, down));
                if (down < up) break;
                put(&s, -0x110, addr(&s, up));
                put(&s, -0x114, 0x945404u);
                swap(&s, -0x114, up, down, hi_a, lo_a);
            }
            put(&s, -0x110, lo_a);
            put(&s, -0x114, 0x94540fu);
            swap(&s, -0x114, lo, down, hi_a, lo_a);
            if ((int32_t)(down - 1 - lo) >= (int32_t)(hi - up)) {
                if (lo + width < down) {
                    put(&s, 0xc, q - 0xf8 + 4 * (uint32_t)(top + 1));
                    put(&s, -4, (uint32_t)(top + 1));
                    put(&s, 8, q - 0x80 + 4 * (uint32_t)(top + 1));
                    put(&s, -0xf8 + 4 * top, lo_a);
                    put(&s, -0x80 + 4 * top, addr(&s, down - width));
                    lo_stack[top] = lo; hi_stack[top] = down - width; top++;
                }
                if (up < hi) { lo = up; continue; }
                eax = up;
            } else {
                if (up < hi) {
                    put(&s, -4, (uint32_t)(top + 1));
                    put(&s, 0xc, q - 0xf8 + 4 * (uint32_t)(top + 1));
                    put(&s, -0xf8 + 4 * top, addr(&s, up));
                    put(&s, 8, q - 0x80 + 4 * (uint32_t)(top + 1));
                    put(&s, -0x80 + 4 * top, hi_a);
                    lo_stack[top] = up; hi_stack[top] = hi; top++;
                }
                if (lo + width < down) { hi = down - width; continue; }
                eax = lo + width;
            }
        }
        top--;                                      /* 0x945391 */
        put(&s, -4, (uint32_t)top);
        put(&s, 0xc, q - 0xf8 + 4 * (uint32_t)top);
        put(&s, 8, q - 0x80 + 4 * (uint32_t)top);
        if (top < 0) return eax;
        lo = lo_stack[top];
        hi = hi_stack[top];
    }
}

int qsort6_strict(const uint8_t *base, uint32_t num, uint32_t width)
{
    for (uint32_t i = 0; i < num; i++) {
        const uint8_t *row = base + (size_t)i * width;
        if (row[3] & 0x80) return 0;                /* an id of 0x80000000 or more */
        if (i && qsort6_history_cmp(row - width, row) >= 0) return 0;
    }
    return 1;
}
