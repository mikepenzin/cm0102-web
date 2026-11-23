/* The history sort when one row joins rows in strict order (item 151b).
 *
 * PlayerStats::add_language 0x7c6a30 adds a row to a list and sorts the whole list again with qsort 0x945333
 * (qsort6.c has the steps). With Max's December 2025 data set the list holds about 59,000 rows at each of
 * 28,339 sorts in "Player histories", and the sorts took about 32 s of the step. The rows before the new row
 * are the output of the sort before, so they are in order, and the new row lands anywhere.
 *
 * When the rows before the new one are in strict order and the new row compares strictly with its neighbours
 * at its place (qsort6_strict, the caller's test), the compare is a strict order of all the rows. Then:
 *  - The rows after the sort are the old rows with the new row at its place, whatever the steps were.
 *  - The game sees the rest of the sort only through the stack words it leaves (each the last value written
 *    to it) and eax. This code finds them by a walk of the steps backwards in time: the part the sort does
 *    last first. A part is skipped when it can write no word that is still open: the fixed words are written
 *    by every partition or short sort, so they close on the first steps of the walk; a word of the stack of
 *    ranges at depth d can be written only by a part of size s at depth t with t <= d < t + depth_bound(s),
 *    since the sort goes on with the smaller part (at most (s - 1) / 2 rows) one depth deeper and keeps the
 *    larger part at its own depth. The word at q - 0x140 is written only by a short sort that swaps two other
 *    rows, which a part in order never does.
 *  - The steps of the parts that the walk does visit are the steps of qsort6 (the same writes, in the same
 *    order), done on the rows. A part with no break (each row below the next) needs no compare: its middle
 *    row goes to lo and back, the scans stop next to the middle, and its short sort takes the last row each
 *    time. A part with breaks scans by runs: inside a run of rising rows the compare with the pivot only
 *    rises, so a binary search finds where each scan stops. The breaks are kept in a bit map, set at the
 *    start (only the place before the new row can break) and after each swap.
 * A random test of 20,000 sorts of 2 to 70,000 rows against the full steps (run-logs/0.6.2-s151/sortexp,
 * rev9.c) gave the same words and eax, with about 24 parts visited in place of about 15,000. qsort6_self_test
 * and RECOMP_QSORT=check compare it with the recompiled qsort. */
#include "qsort6.h"

#include <stdlib.h>
#include <string.h>

#define NS 84                      /* stack words from q - 0x140 to q + 0xc */
#define SLOT(off) (((off) + 0x140) / 4)
#define DEPTHS 30                  /* the original's stack of ranges */
#define MAX_VISIT 20000            /* a walk that visits more parts gives up (the caller sorts in full) */
#define MAX_NEST 200               /* nor may the walk nest deeper (the C stack) */
int qsort6_insert_max_visit = MAX_VISIT;   /* the self-test lowers it to test the fall-back */

typedef struct { int16_t slot; uint32_t v; } Wr;

typedef struct {
    const qsort6_stack *st;
    uint8_t *rows;
    uint32_t width, num;
    uint64_t *brk;                 /* bit i: row i does not compare below row i + 1 */
    Wr *rec;                       /* the writes of the part being done */
    size_t nrec, room;
    int failed;
    /* the walk */
    uint32_t value[NS];
    char open[NS], got[NS];         /* got: the word has its last value */
    int open_fixed;                /* open words outside the stack of ranges, without q - 0x140 */
    uint32_t open_depths;          /* depths with an open word of the stack of ranges */
    int eax_open;
    uint8_t *eax;
    int visits, nest;
} Ins;

static int slot_depth(int slot)
{
    int off = slot * 4 - 0x140;
    if (off >= -0xf8 && off < -0xf8 + 4 * DEPTHS) return (off + 0xf8) / 4;
    if (off >= -0x80 && off < -0x80 + 4 * DEPTHS) return (off + 0x80) / 4;
    return -1;
}

static inline uint32_t ad(const Ins *s, const uint8_t *p) { return s->st->rows + (uint32_t)(p - s->rows); }
static inline uint32_t idx(const Ins *s, const uint8_t *p) { return (uint32_t)(p - s->rows) / s->width; }

static void put(Ins *s, int32_t off, uint32_t v)
{
    if (s->nrec == s->room) {
        size_t room = s->room ? 2 * s->room : 256;
        Wr *p = realloc(s->rec, room * sizeof *p);
        if (!p) { s->failed = 1; return; }
        s->rec = p;
        s->room = room;
    }
    s->rec[s->nrec].slot = (int16_t)SLOT(off);
    s->rec[s->nrec++].v = v;
}

static void set_break(Ins *s, uint32_t i)
{
    if (i + 1 >= s->num) return;
    const uint8_t *r = s->rows + (size_t)i * s->width;
    if (qsort6_history_cmp(r, r + s->width) >= 0) s->brk[i >> 6] |= 1ull << (i & 63);
    else s->brk[i >> 6] &= ~(1ull << (i & 63));
}

/* The first break in [i, lim], or lim + 1. */
static uint32_t next_break(const Ins *s, uint32_t i, uint32_t lim)
{
    while (i <= lim) {
        uint64_t w = s->brk[i >> 6] >> (i & 63);
        if (w) { uint32_t j = i + (uint32_t)__builtin_ctzll(w); return j <= lim ? j : lim + 1; }
        i = (i | 63) + 1;
    }
    return lim + 1;
}

/* The last break in [lim, i], or lim - 1 (lim >= 1). */
static uint32_t prev_break(const Ins *s, uint32_t i, uint32_t lim)
{
    while ((int64_t)i >= (int64_t)lim) {
        uint64_t w = s->brk[i >> 6] << (63 - (i & 63));
        if (w) { uint32_t j = i - (uint32_t)__builtin_clzll(w); return j >= lim ? j : lim - 1; }
        if (i < 64) return lim - 1;
        i = (i & ~63u) - 1;
    }
    return lim - 1;
}

static int in_order(const Ins *s, const uint8_t *lo, const uint8_t *hi)
{
    if (hi <= lo) return 1;
    uint32_t l = idx(s, lo), h = idx(s, hi);
    return next_break(s, l, h - 1) > h - 1;
}

/* The swap 0x9454d5, as in qsort6.c, and the breaks next to the two rows. */
static uint8_t *swap(Ins *s, int32_t at, uint8_t *a, uint8_t *b, uint32_t esi, uint32_t ebx)
{
    put(s, at - 4, esi);
    if (a == b) return a;
    put(s, at - 8, ebx);
    uint8_t *ra = a, *rb = b;
    for (uint32_t w = s->width; w--; ) {
        uint8_t t = *a;
        *a++ = *b;
        *b++ = t;
    }
    uint32_t i = idx(s, ra), j = idx(s, rb);
    if (i) set_break(s, i - 1);
    set_break(s, i);
    if (j) set_break(s, j - 1);
    set_break(s, j);
    return a;
}

/* The short sort 0x945487 of qsort6.c. */
static uint8_t *short_sort(Ins *s, uint8_t *lo, uint8_t *hi)
{
    uint32_t w = s->width, lo_a = ad(s, lo), hi_a = ad(s, hi);
    put(s, -0x108, s->st->cmp);
    put(s, -0x10c, w);
    put(s, -0x110, hi_a);
    put(s, -0x114, lo_a);
    put(s, -0x118, 0x94538eu);
    put(s, -0x11c, s->st->q);
    put(s, -0x120, w);
    uint8_t *eax = lo;
    if (hi <= lo) return eax;
    put(s, -0x124, lo_a);
    put(s, -0x128, hi_a);
    put(s, -0x110, ad(s, lo + w));
    while (hi > lo) {
        uint8_t *max = lo, *p;
        if (in_order(s, lo, hi)) max = hi, p = hi + w;
        else
            for (p = lo + w; p <= hi; p += w)
                if (qsort6_history_cmp(p, max) > 0) max = p;
        put(s, -0x12c, w);
        put(s, -0x130, ad(s, hi));
        put(s, -0x134, ad(s, max));
        put(s, -0x138, 0x9454c5u);
        eax = swap(s, -0x138, max, hi, ad(s, p), ad(s, max));
        hi -= w;
    }
    return eax;
}

/* The first row from up + 1 to hi that compares above the pivot at lo, or hi + 1 (the up scan). */
static uint8_t *scan_up(Ins *s, uint8_t *lo, uint8_t *up, uint8_t *hi)
{
    uint32_t w = s->width, i = idx(s, up) + 1, h = idx(s, hi);
    while (i <= h) {
        uint32_t j = next_break(s, i, h);
        if (j > h) j = h;
        if (qsort6_history_cmp(s->rows + (size_t)j * w, lo) > 0) {
            uint32_t l = i, r = j;
            while (l < r) {
                uint32_t m = l + (r - l) / 2;
                if (qsort6_history_cmp(s->rows + (size_t)m * w, lo) > 0) r = m;
                else l = m + 1;
            }
            return s->rows + (size_t)l * w;
        }
        i = j + 1;
    }
    return s->rows + (size_t)(h + 1) * w;
}

/* The last row from down - 1 to lo + 1 that compares below the pivot at lo, or lo (the down scan). */
static uint8_t *scan_down(Ins *s, uint8_t *lo, uint8_t *down)
{
    uint32_t w = s->width, L = idx(s, lo), i = idx(s, down) - 1;
    while (i > L) {
        uint32_t k = prev_break(s, i - 1, L + 1), st = k + 1;
        if (st <= L) st = L + 1;
        if (qsort6_history_cmp(s->rows + (size_t)st * w, lo) < 0) {
            uint32_t l = st, r = i;
            while (l < r) {
                uint32_t m = l + (r - l + 1) / 2;
                if (qsort6_history_cmp(s->rows + (size_t)m * w, lo) < 0) l = m;
                else r = m - 1;
            }
            return s->rows + (size_t)l * w;
        }
        i = st - 1;
    }
    return lo;
}

/* The partition of qsort6.c (0x945389..0x945417). */
static void partition(Ins *s, uint8_t *lo, uint8_t *hi, uint8_t **pup, uint8_t **pdown)
{
    uint32_t width = s->width, size = (uint32_t)(hi - lo) / width + 1, lo_a = ad(s, lo), hi_a = ad(s, hi);
    int clean = in_order(s, lo, hi);
    uint8_t *mid = lo + (size / 2) * width;
    put(s, -0x108, width);
    put(s, -0x10c, lo_a);
    put(s, -0x110, ad(s, mid));
    put(s, -0x114, 0x9453c1u);
    swap(s, -0x114, mid, lo, hi_a, lo_a);
    uint8_t *up = lo, *down = hi + width;
    put(s, -8, lo_a);
    for (;;) {
        up = clean ? mid + width : scan_up(s, lo, up, hi);
        put(s, -8, ad(s, up));
        down = clean ? mid : scan_down(s, lo, down);
        put(s, -0x108, width);
        put(s, -0x10c, ad(s, down));
        if (down < up) break;
        put(s, -0x110, ad(s, up));
        put(s, -0x114, 0x945404u);
        swap(s, -0x114, up, down, hi_a, lo_a);
    }
    put(s, -0x110, lo_a);
    put(s, -0x114, 0x94540fu);
    swap(s, -0x114, lo, down, hi_a, lo_a);
    *pup = up;
    *pdown = down;
}

/* The pushes and the pop of qsort6.c, at depth t. */
static void push_writes(Ins *s, int branch, int t, uint32_t a, uint32_t b)
{
    uint32_t q = s->st->q;
    if (branch == 1) {
        put(s, 0xc, q - 0xf8 + 4 * (uint32_t)(t + 1));
        put(s, -4, (uint32_t)(t + 1));
        put(s, 8, q - 0x80 + 4 * (uint32_t)(t + 1));
        put(s, -0xf8 + 4 * t, a);
        put(s, -0x80 + 4 * t, b);
    } else {
        put(s, -4, (uint32_t)(t + 1));
        put(s, 0xc, q - 0xf8 + 4 * (uint32_t)(t + 1));
        put(s, -0xf8 + 4 * t, a);
        put(s, 8, q - 0x80 + 4 * (uint32_t)(t + 1));
        put(s, -0x80 + 4 * t, b);
    }
}

static void pop_writes(Ins *s, int t)
{
    put(s, -4, (uint32_t)t);
    put(s, 0xc, s->st->q - 0xf8 + 4 * (uint32_t)t);
    put(s, 8, s->st->q - 0x80 + 4 * (uint32_t)t);
}

/* The writes from rec[from] on, latest first: each open word takes its last value. */
static void close_words(Ins *s, size_t from)
{
    for (size_t i = s->nrec; i-- > from; ) {
        int sl = s->rec[i].slot;
        if (!s->open[sl]) continue;
        s->open[sl] = 0;
        s->got[sl] = 1;
        s->value[sl] = s->rec[i].v;
        int d = slot_depth(sl);
        if (sl == 0) continue;
        if (d < 0) s->open_fixed--;
        else if (!s->open[SLOT(-0xf8 + 4 * d)] && !s->open[SLOT(-0x80 + 4 * d)]) s->open_depths &= ~(1u << d);
    }
    s->nrec = from;
}

static int depth_bound(uint32_t size)
{
    int d = 0;
    while (size > 8) { d++; size = (size - 1) / 2; }
    return d;
}

/* Can the steps of lo..hi at depth t write a word that is still open? */
static int worth(const Ins *s, uint8_t *lo, uint8_t *hi, int t)
{
    if (s->eax_open || s->open_fixed) return 1;
    if (s->open[0] && !in_order(s, lo, hi)) return 1;
    int top = t + depth_bound((uint32_t)(hi - lo) / s->width + 1);
    if (top > 31) top = 31;
    if (top <= t) return 0;
    uint32_t window = ((1u << top) - 1) & ~((1u << t) - 1);
    return (s->open_depths & window) != 0;
}

static void walk(Ins *s, uint8_t *lo, uint8_t *hi, int t);

/* A part in order, when only words of the stack of ranges are open: only its pushes matter, and its splits
 * are known (the middle row ends at the middle). */
static void walk_in_order(Ins *s, uint8_t *lo, uint8_t *hi, int t)
{
    uint32_t w = s->width, size = (uint32_t)(hi - lo) / w + 1;
    if (size <= 8 || !worth(s, lo, hi, t) || s->failed) return;
    if (++s->visits > qsort6_insert_max_visit || t >= DEPTHS) { s->failed = 1; return; }
    uint8_t *mid = lo + (size / 2) * w, *up = mid + w, *down = mid, *xl = NULL, *xh = NULL, *yl = NULL, *yh = NULL;
    if ((int32_t)(down - 1 - lo) >= (int32_t)(hi - up)) {
        if (lo + w < down) xl = lo, xh = down - w;
        if (up < hi) yl = up, yh = hi;
    } else {
        if (up < hi) xl = up, xh = hi;
        if (lo + w < down) yl = lo, yh = down - w;
    }
    if (s->nest >= MAX_NEST) { s->failed = 1; return; }
    s->nest++;
    if (xl) walk_in_order(s, xl, xh, t);
    if (yl) walk_in_order(s, yl, yh, xl ? t + 1 : t);
    s->nest--;
    if (xl) {
        size_t from = s->nrec;
        put(s, -0xf8 + 4 * t, ad(s, xl));
        put(s, -0x80 + 4 * t, ad(s, xh));
        close_words(s, from);
    }
}

/* The steps of lo..hi at depth t (t entries on the stack of ranges), latest first. */
static void walk(Ins *s, uint8_t *lo, uint8_t *hi, int t)
{
    if (s->failed || !worth(s, lo, hi, t)) return;
    if (!s->eax_open && !s->open_fixed && in_order(s, lo, hi)) { walk_in_order(s, lo, hi, t); return; }
    if (++s->visits > qsort6_insert_max_visit || t >= DEPTHS || s->nest >= MAX_NEST) { s->failed = 1; return; }
    uint32_t w = s->width, size = (uint32_t)(hi - lo) / w + 1;
    size_t mark = s->nrec;
    if (size <= 8) {
        uint8_t *e = short_sort(s, lo, hi);
        if (s->eax_open) s->eax_open = 0, s->eax = e;
        close_words(s, mark);
        return;
    }
    uint8_t *up, *down;
    partition(s, lo, hi, &up, &down);
    size_t node_end = s->nrec;
    uint8_t *xl = NULL, *xh = NULL, *yl = NULL, *yh = NULL, *eax = NULL;
    int branch;
    if ((int32_t)(down - 1 - lo) >= (int32_t)(hi - up)) {
        branch = 1;
        if (lo + w < down) xl = lo, xh = down - w;
        if (up < hi) yl = up, yh = hi;
        else eax = up;
    } else {
        branch = 2;
        if (up < hi) xl = up, xh = hi;
        if (lo + w < down) yl = lo, yh = down - w;
        else eax = lo + w;
    }
    /* Latest first: the kept part (after the pop to depth t), the pop, the part done at once, the push, an eax
     * that an empty part sets, the partition. The partition's writes stay in rec[mark .. node_end). */
    s->nest++;
    if (xl) {
        walk(s, xl, xh, t);
        size_t from = s->nrec;
        pop_writes(s, t);
        close_words(s, from);
    }
    if (yl) walk(s, yl, yh, xl ? t + 1 : t);
    s->nest--;
    if (xl) {
        size_t from = s->nrec;
        push_writes(s, branch, t, ad(s, xl), ad(s, xh));
        close_words(s, from);
    }
    if (eax && s->eax_open) s->eax_open = 0, s->eax = eax;
    s->nrec = node_end;
    close_words(s, mark);
}

uint8_t *qsort6_insert(uint8_t *base, uint32_t num, uint32_t width, const qsort6_stack *st, const uint8_t *before,
                       int *done)
{
    *done = 0;
    if (num < 2 || width < 5) return NULL;
    static uint64_t *bits;
    static size_t bits_room;
    size_t words = num / 64 + 2;
    if (words > bits_room) {
        uint64_t *p = realloc(bits, words * sizeof *p);
        if (!p) return NULL;
        bits = p;
        bits_room = words;
    }
    memset(bits, 0, words * sizeof *bits);
    static Ins s;
    Wr *rec = s.rec;
    size_t room = s.room;
    memset(&s, 0, sizeof s);
    s.rec = rec, s.room = room;
    s.st = st, s.rows = base, s.width = width, s.num = num, s.brk = bits;
    set_break(&s, num - 2);                         /* the rows before the new one are in strict order */
    static const int16_t fixed[] = {-4, 8, 0xc, -8};
    for (int i = 0; i < 4; i++) s.open[SLOT(fixed[i])] = 1, s.open_fixed++;
    for (int off = -0x140; off <= -0x108; off += 4) {
        s.open[SLOT(off)] = 1;
        if (off != -0x140) s.open_fixed++;
    }
    int dmax = depth_bound(num);
    for (int d = 0; d < dmax && d < DEPTHS; d++) {
        s.open[SLOT(-0xf8 + 4 * d)] = s.open[SLOT(-0x80 + 4 * d)] = 1;
        s.open_depths |= 1u << d;
    }
    s.eax_open = 1;
    pop_writes(&s, -1);                             /* the last steps: the pop to -1 */
    close_words(&s, 0);
    walk(&s, base, base + (size_t)(num - 1) * width, 0);
    /* The rows: the old rows with the new row at its place. 'before' holds the old rows and the new row last. */
    const uint8_t *row = before + (size_t)(num - 1) * width;
    uint32_t l = 0, r = num - 1;
    while (l < r) {
        uint32_t m = l + (r - l) / 2;
        if (qsort6_history_cmp(before + (size_t)m * width, row) < 0) l = m + 1;
        else r = m;
    }
    /* The first steps of the sort: the saved registers and the empty stack of ranges. */
    size_t from = s.nrec;
    s.open[SLOT(0)] = s.open[SLOT(-0xfc)] = s.open[SLOT(-0x100)] = s.open[SLOT(-0x104)] = 1;
    s.open_fixed += 4;
    put(&s, 0, st->ebp);
    put(&s, -0xfc, st->ebx);
    put(&s, -0x100, st->esi);
    put(&s, -0x104, st->edi);
    put(&s, -4, 0);
    put(&s, 8, st->q - 0x80);
    put(&s, 0xc, st->q - 0xf8);
    close_words(&s, from);
    if (s.failed || s.eax_open || !s.eax) {         /* the caller sorts in full, from the rows as they were */
        memcpy(base, before, (size_t)num * width);
        return NULL;
    }
    memcpy(base, before, (size_t)l * width);
    memcpy(base + (size_t)l * width, row, width);
    memcpy(base + (size_t)(l + 1) * width, before + (size_t)l * width, (size_t)(num - 1 - l) * width);
    for (int sl = 0; sl < NS; sl++) {
        if (!s.got[sl]) continue;                   /* a word the sort never writes */
        uint32_t at = st->q + (uint32_t)(sl * 4 - 0x140);
        uint8_t *m = st->mem + at;
        uint32_t v = s.value[sl];
        m[0] = (uint8_t)v, m[1] = (uint8_t)(v >> 8), m[2] = (uint8_t)(v >> 16), m[3] = (uint8_t)(v >> 24);
    }
    *done = 1;
    return s.eax;
}
