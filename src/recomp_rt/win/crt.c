/* CRT functions of the original that the runtime replaces by address. The rest of the CRT is
 * recompiled with the game.
 *
 * memcpy (0x94ab50) and memmove (0x94f0f0) are the same MSVC 6 code. It jumps through tables with
 * indexes that the code does not bound (and one table read at negative indexes), which the recompiler
 * cannot follow. Both copy as memmove does and return the target. */
#include "win.h"
#include "qsort6.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RT_REPLACE(0094ab50, memcpy)
{
    uint32_t dst = rt_arg(0), src = rt_arg(1), n = rt_arg(2);
    memmove(g_mem + dst, g_mem + src, n);
    rt_ret_cdecl(dst);
}

RT_REPLACE(0094f0f0, memmove)
{
    uint32_t dst = rt_arg(0), src = rt_arg(1), n = rt_arg(2);
    memmove(g_mem + dst, g_mem + src, n);
    rt_ret_cdecl(dst);
}

/* qsort 0x945333, cdecl (base, count, width, compare). For the compare of the player history lists
 * (0x7c6d40) the runtime sorts in host code with the same steps, the same stack writes and the same eax
 * (qsort6.c); every other compare keeps the recompiled qsort. RECOMP_QSORT=original turns the host sort
 * off; RECOMP_QSORT=check runs both on each call and stops the program at the first row, stack byte or
 * eax that differs. */
#define QSORT 0x945333u
#define HISTORY_CMP 0x7c6d40u

void o_00945333(void);

static int qsort_mode = -1;        /* 0 host, 1 check, 2 original, 3 the self-test below */
static unsigned long long qsort_checked;
static int test_fails;

static unsigned long long sorted_sorts;    /* sorts that took the sorted path (the self-test counts them) */
static uint64_t insert_sorts;              /* sorts done by qsort6_insert */

static void print_qsort_checked(void)
{
    fprintf(stderr, "recomp: qsort check: %llu sorts equal (%llu by qsort6_insert, %llu by the sorted path)\n",
            qsort_checked, (unsigned long long)insert_sorts, sorted_sorts);
}

/* Item 151b: the rows of the last host sort, in their order after it, and whether that order is strict. When
 * the next sort has the same list with one more row (as 0x7c6a30 adds one and sorts again), the rows before
 * the new one equal this copy (memcmp), so only the new row needs a test. Any other call tests the whole list.
 * RECOMP_QSORT_SORTED=0 turns the sorted path off. */
static struct { uint8_t *rows; size_t room; uint32_t base, num, width; int strict, valid; } last_sort;

static int sorted_path_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_QSORT_SORTED");
        on = !(v && !strcmp(v, "0"));
    }
    return on;
}

/* 1 when the first num - 1 rows are in strict order (qsort6_strict). After the last host sort of the same list
 * with one row less, a memcmp with its kept rows answers. */
static int head_in_order(const uint8_t *rows, uint32_t base, uint32_t num, uint32_t width)
{
    if (last_sort.valid && last_sort.strict && base == last_sort.base && width == last_sort.width
        && num == last_sort.num + 1 && !memcmp(rows, last_sort.rows, (size_t)last_sort.num * width))
        return 1;
    return qsort6_strict(rows, num - 1, width);
}

/* 1 when the last row has an id below 0x80000000 and compares strictly with its neighbours at its place among
 * the rows before it (which are in strict order). Then all the rows are in strict order once sorted. */
static int last_row_fits(const uint8_t *rows, uint32_t num, uint32_t width)
{
    const uint8_t *row = rows + (size_t)(num - 1) * width;
    if (row[3] & 0x80) return 0;
    uint32_t l = 0, r = num - 1;
    while (l < r) {
        uint32_t m = l + (r - l) / 2;
        if (qsort6_history_cmp(rows + (size_t)m * width, row) < 0) l = m + 1;
        else r = m;
    }
    if (l < num - 1 && qsort6_history_cmp(row, rows + (size_t)l * width) >= 0) return 0;
    if (l > 0 && qsort6_history_cmp(rows + (size_t)(l - 1) * width, row) >= 0) return 0;
    return 1;
}

static void keep_sorted_rows(const uint8_t *rows, uint32_t base, uint32_t num, uint32_t width, int strict)
{
    size_t bytes = (size_t)num * width;
    if (bytes > last_sort.room) {
        uint8_t *p = realloc(last_sort.rows, bytes);
        if (!p) { last_sort.valid = 0; return; }
        last_sort.rows = p;
        last_sort.room = bytes;
    }
    memcpy(last_sort.rows, rows, bytes);
    last_sort.base = base, last_sort.num = num, last_sort.width = width, last_sort.strict = strict;
    last_sort.valid = 1;
}

static int insert_off;             /* the self-test: 1 tests qsort6's own sorted path */

static uint8_t *host_sort(uint8_t *rows, uint32_t base, uint32_t num, uint32_t width)
{
    qsort6_stack st = {g_mem, cpu.esp - 4, base, cpu.ebp, cpu.ebx, cpu.esi, cpu.edi, HISTORY_CMP, 0};
    if (!sorted_path_on() || num < 2 || width < 5) return qsort6(rows, num, width, &st);
    int head = head_in_order(rows, base, num, width);
    if (head && insert_off) st.sorted = qsort6_strict(rows + (size_t)(num - 2) * width, 2, width);
    else if (head && last_row_fits(rows, num, width)) {
        /* All rows are in strict order once sorted: qsort6_insert, from a copy of the rows as they are. */
        static uint8_t *before;
        static size_t before_room;
        size_t bytes = (size_t)num * width;
        if (bytes > before_room) {
            uint8_t *p = realloc(before, bytes);
            if (p) before = p, before_room = bytes;
        }
        if (before_room >= bytes) {
            memcpy(before, rows, bytes);
            int done;
            uint8_t *eax = qsort6_insert(rows, num, width, &st, before, &done);
            if (done) {
                insert_sorts++;
                keep_sorted_rows(rows, base, num, width, 1);
                return eax;
            }
        }
        st.sorted = qsort6_strict(rows + (size_t)(num - 2) * width, 2, width);  /* the new row is the last */
    }
    sorted_sorts += (unsigned)st.sorted;
    uint8_t *eax = qsort6(rows, num, width, &st);
    keep_sorted_rows(rows, base, num, width, st.sorted || qsort6_strict(rows, num, width));
    return eax;
}

/* Both sorts from the same rows and stack; the original's result stays. 0 when all is equal. */
static int qsort_check(uint32_t base, uint32_t num, uint32_t width)
{
    size_t bytes = (size_t)num * width;
    uint32_t low = cpu.esp - QSORT6_BELOW, span = QSORT6_BELOW + QSORT6_ABOVE;
    uint8_t *copy = malloc(bytes + 1), *before = malloc(span), *host = malloc(span);
    if (!copy || !before || !host) rt_fatal("qsort check: out of memory");
    memcpy(copy, g_mem + base, bytes);
    memcpy(before, g_mem + low, span);
    uint32_t eax_in = cpu.eax;
    uint8_t *eax = host_sort(copy, base, num, width);
    uint32_t want = eax ? base + (uint32_t)(eax - copy) : eax_in;
    memcpy(host, g_mem + low, span);
    memcpy(g_mem + low, before, span);
    uint32_t esp_in = cpu.esp;
    o_00945333();
    int bad = memcmp(copy, g_mem + base, bytes) ? 1 : want != cpu.eax ? 2 : memcmp(host, g_mem + low, span) ? 3 : 0;
    if (bad) {
        uint32_t at = 0;
        if (bad == 3) while (host[at] == g_mem[low + at]) at++;
        fprintf(stderr, "recomp: qsort check: %u rows of %u bytes at 0x%x differ: %s (eax 0x%x, host 0x%x, "
                "stack byte esp%+d)\n", num, width, base, bad == 1 ? "rows" : bad == 2 ? "eax" : "stack", cpu.eax,
                want, bad == 3 ? (int)(low + at - esp_in) : 0);
    }
    free(copy);
    free(before);
    free(host);
    return bad;
}

RT_WRAP(00945333, qsort)
{
    win_sort_step();                          /* the browser's page has a moment in long sorts (user32.c) */
    uint32_t base = rt_arg(0), num = rt_arg(1), width = rt_arg(2), cmp = rt_arg(3);
    if (qsort_mode < 0) {
        const char *v = getenv("RECOMP_QSORT");
        qsort_mode = !v ? 0 : !strcmp(v, "check") ? 1 : !strcmp(v, "original") ? 2 : 0;
    }
    if (cmp != HISTORY_CMP || qsort_mode == 2) { o_00945333(); return; }
    if (qsort_mode == 1) {
        if (qsort_check(base, num, width)) abort();
        if (!qsort_checked++) atexit(print_qsort_checked);
        return;
    }
    if (qsort_mode == 3) {
        if (qsort_check(base, num, width)) test_fails++;
        return;
    }
    uint8_t *eax = host_sort(g_mem + base, base, num, width);
    rt_ret_cdecl(eax ? (uint32_t)(eax - g_mem) : cpu.eax);
}

/* RECOMP_QSORT_TEST: before the game starts, sorts rows made from a fixed seed with both sorts (the
 * check above, from a call of 0x945333 through the runtime) and compares the rows, the stack and eax.
 * The rows have many equal ids and 0xff bytes, so the order of rows that compare equal is tested. Also
 * lists that grow by one row and are sorted again each time, as 0x7c6a30 does. Gives the exit code: 0
 * when all are equal. */
int hot_loops_self_test(void);
int ftol_self_test(void);
int rt_call_cache_self_test(void);

static uint32_t test_seed = 12345;

static uint32_t test_random(void)
{
    test_seed = test_seed * 1103515245u + 12345u;
    return test_seed >> 8;
}

static void test_row(uint8_t *row, uint32_t ids)
{
    uint32_t id = test_random() % ids;
    if (test_random() % 16 == 0) id = 0x80000000u + test_random() % 4;      /* a wrap in the subtraction */
    row[0] = (uint8_t)id; row[1] = (uint8_t)(id >> 8); row[2] = (uint8_t)(id >> 16); row[3] = (uint8_t)(id >> 24);
    uint32_t b = test_random() % 9;
    row[4] = b == 8 ? 0xff : (uint8_t)(b - 4);
    row[5] = (uint8_t)test_random();
}

/* The test's call of 0x945333 lands here through the wrap in check mode. */
static void test_one(uint32_t mem, uint8_t *rows, uint32_t num)
{
    memcpy(g_mem + mem, rows, (size_t)num * 6);
    uint32_t args[4] = {mem, num, 6, HISTORY_CMP};
    cpu.eax = 0x5a5a5a5au;
    rt_callback(QSORT, 4, args);
    memcpy(rows, g_mem + mem, (size_t)num * 6);        /* the original's rows for the next sort */
}

int qsort6_self_test(void)
{
    enum { MAX = 4000 };
    uint8_t *rows = malloc(MAX * 6);
    uint32_t mem = rt_alloc(MAX * 6);
    int sorts = 0;
    qsort_mode = 3;                                  /* check, and count the failures */
    for (uint32_t num = 0; num <= 64; num++)
        for (int k = 0; k < 40; k++) {
            uint32_t ids = 1 + test_random() % (num + 2);
            for (uint32_t i = 0; i < num; i++) test_row(rows + i * 6, ids);
            test_one(mem, rows, num);
            sorts++;
        }
    static const uint32_t big[] = {100, 255, 256, 257, 1000, 2047, 3001, MAX};
    for (size_t b = 0; b < sizeof big / sizeof *big; b++)
        for (int k = 0; k < 6; k++) {
            uint32_t num = big[b], ids = k < 2 ? 3 : k < 4 ? num / 4 + 1 : num * 4;
            for (uint32_t i = 0; i < num; i++) test_row(rows + i * 6, ids);
            if (k == 5) for (uint32_t i = 0; i < num; i++) rows[i * 6 + 4] = 0xff;   /* every row equal by its byte */
            test_one(mem, rows, num);
            test_one(mem, rows, num);                /* sorted rows again */
            sorts += 2;
        }
    for (int list = 0; list < 6; list++) {           /* a list sorted again after each new row (0x7c6a30) */
        uint32_t ids = list < 3 ? 20 : 2000;
        for (uint32_t num = 1; num <= 1500; num++) {
            test_row(rows + (num - 1) * 6, ids);
            test_one(mem, rows, num);
            sorts++;
        }
    }
    /* Item 151b: rows already in strict order take the sorted path. Ids rise with gaps; some ids hold two to four
     * rows with rising bytes, some a lone 0xff byte. Lists of 2 to 4000 rows, and lists that grow by a row above
     * the last (the sorted path after the memcmp) or a row anywhere (the full sort). */
    unsigned long long sorted_before = sorted_sorts;
    insert_off = 1;
    for (int list = 0; list < 8; list++) {
        uint32_t id = test_random() % 50, num = 0;
        int8_t byte = 0;
        for (uint32_t n = 2; n <= (list < 6 ? 1200u : (uint32_t)MAX); n++) {
            uint8_t *row = rows + (n - 1) * 6;
            if (n == 2) { memset(rows, 0, 6); rows[0] = (uint8_t)id; }
            uint32_t r = test_random() % 8;
            int above = list < 4 ? r != 0 : 1;           /* list 0..3: one row in eight lands anywhere */
            if (above && (r < 5 || (uint8_t)byte == 0xff || byte > 100)) { id += 1 + test_random() % 3; byte = (int8_t)(r == 1 ? -1 : -4); }
            else if (above) {
                byte = (int8_t)(byte + 1 + (int)(test_random() % 3));
                if (byte == -1) byte = 0;                /* 0xff in a group of one id compares equal */
            }
            if (!above) { id = test_random() % (id + 1); byte = (int8_t)(test_random() % 9 - 4); }
            row[0] = (uint8_t)id; row[1] = (uint8_t)(id >> 8); row[2] = (uint8_t)(id >> 16); row[3] = (uint8_t)(id >> 24);
            row[4] = (uint8_t)byte;
            row[5] = (uint8_t)test_random();
            if (list >= 6 && n != (list == 6 ? 3001u : (uint32_t)MAX)) continue;   /* lists 6, 7: one sort of all */
            test_one(mem, rows, n);
            sorts++;
            num = n;
        }
        (void)num;
    }
    if (sorted_sorts == sorted_before) test_fails++;     /* the sorted path must have run */
    fprintf(stderr, "qsort test: %llu sorts took the sorted path\n", sorted_sorts - sorted_before);
    insert_off = 0;
    /* qsort6_insert (item 151b): lists that grow by a row at a random place, as with Max's data set: ids differ
     * (one in sixteen a lone 0xff byte), so the rows are in strict order. Lists 0..3 grow to 1500 rows one row
     * at a time; lists 4..7 start at 2000 to 3500 rows. Some rows repeat an id with another byte (0xff among
     * them), and these may land on an equal row (the full sort). In list 2 every walk may visit 3 parts only,
     * so qsort6_insert gives up and the full sort runs from the rows as they were; in list 3 one row in
     * twenty has an id of 0x80000000 or more (the full sort). */
    uint64_t insert_before = insert_sorts;
    static uint8_t used[1u << 16];
    for (int list = 0; list < 8; list++) {
        memset(used, 0, sizeof used);
        uint32_t num = 0, start = list < 4 ? 1 : 2000 + 500 * (uint32_t)(list - 4), end = list < 4 ? 1500 : start + 60;
        while (num < end) {
            uint32_t id;
            do id = test_random() % (1u << 16); while (used[id] && test_random() % 8);
            int same = used[id];
            used[id] = 1;
            uint8_t *row = rows + (size_t)num * 6;
            row[0] = (uint8_t)id; row[1] = (uint8_t)(id >> 8); row[2] = 0;
            row[3] = list == 3 && test_random() % 20 == 0 ? 0x80 : 0;
            row[4] = same ? (uint8_t)(test_random() % 9 - 4) : test_random() % 16 == 0 ? 0xff : (uint8_t)(test_random() % 9 - 4);
            row[5] = (uint8_t)test_random();
            num++;
            if (num < start) continue;
            if (num == start) {                          /* a first full sort of the start rows */
                test_one(mem, rows, num);
                sorts++;
                continue;
            }
            qsort6_insert_max_visit = list == 2 ? 3 : 20000;
            test_one(mem, rows, num);
            sorts++;
        }
    }
    qsort6_insert_max_visit = 20000;
    if (insert_sorts == insert_before) test_fails++;     /* qsort6_insert must have run */
    fprintf(stderr, "qsort test: %llu sorts by qsort6_insert\n", (unsigned long long)(insert_sorts - insert_before));
    free(rows);
    qsort_mode = -1;
    fprintf(stderr, "qsort test: %d sorts, %d differ\n", sorts, test_fails);
    int hot = hot_loops_self_test();                 /* hot_loops.c */
    int ftol = ftol_self_test();                     /* below */
    int calls = rt_call_cache_self_test();           /* rt.c */
    return test_fails || hot || ftol || calls ? 1 : 0;
}

/* _ftol 0x945640, the MSVC 6 float to integer call (0.6.2 item 110). It takes ST(0), pops it and gives the
 * value cut toward zero as a 64-bit integer in edx:eax. The code (0x945640..0x945666): push ebp; mov ebp, esp;
 * add esp, -0xc; wait; fnstcw [ebp-2]; wait; mov ax, [ebp-2]; or ah, 0xc; mov [ebp-4], ax; fldcw [ebp-4];
 * fistp qword [ebp-0xc]; fldcw [ebp-2]; mov eax, [ebp-0xc]; mov edx, [ebp-8]; leave; ret. The game calls it millions of times a
 * day (a profile of a game day: 5.7 % of the main thread, most of it the cost of the call itself). The host
 * version below does the same: the same bytes below esp (ebp, the control word, the changed control word,
 * the integer), the same eax, edx, ebp and esp, and the x87 state with ST(0) popped and the control word
 * as before. The game reads stack bytes that earlier calls left (open item 85), so the stack writes stay.
 * RECOMP_FTOL=original runs the recompiled code; RECOMP_FTOL=check runs both on each call and stops the
 * program at the first difference. The self-test (RECOMP_QSORT_TEST) checks a list of values. */
#define FTOL_BELOW 16u              /* the bytes it writes below esp at entry */

void o_00945640(void);

static int ftol_mode = -1;          /* 0 host, 1 check, 2 original, 3 the self-test */
static unsigned long long ftol_checked;
static int ftol_fails;

static void ftol_host(void)
{
    uint32_t e = cpu.esp;
    uint16_t cw = fpu.cw, cut = (uint16_t)(cw | 0x0c00u);
    wr32(g_mem, e - 4, cpu.ebp);                     /* push ebp */
    wr16(g_mem, e - 6, cw);                          /* fnstcw [ebp-2] */
    wr16(g_mem, e - 8, cut);                         /* mov [ebp-4], ax */
    fpu.cw = cut;
    int64_t v = fist64(ST(0));                       /* fistp qword [ebp-0xc], rounded by the control word */
    fpop();
    fpu.cw = cw;                                     /* fldcw [ebp-2] */
    wr64(g_mem, e - 16, (uint64_t)v);
    cpu.eax = (uint32_t)(uint64_t)v;
    cpu.edx = (uint32_t)((uint64_t)v >> 32);
    cpu.esp = e + 4;                                 /* leave; ret */
}

static void print_ftol_checked(void) { fprintf(stderr, "recomp: ftol check: %llu calls equal\n", ftol_checked); }

/* Both versions from the same state; the original's result stays. 0 when all is equal. */
static int ftol_check(void)
{
    Cpu c0 = cpu;
    Fpu f0 = fpu;
    uint32_t low = cpu.esp - FTOL_BELOW;
    uint8_t before[FTOL_BELOW + 4], host[FTOL_BELOW + 4];
    memcpy(before, g_mem + low, sizeof before);
    ftol_host();
    Cpu c1 = cpu;
    Fpu f1 = fpu;
    memcpy(host, g_mem + low, sizeof host);
    cpu = c0;
    fpu = f0;
    memcpy(g_mem + low, before, sizeof before);
    o_00945640();
    int bad = memcmp(&c1, &cpu, sizeof cpu) ? 1 : memcmp(&f1, &fpu, sizeof fpu) ? 2 :
              memcmp(host, g_mem + low, sizeof host) ? 3 : 0;
    if (bad)
        fprintf(stderr, "recomp: ftol check: %s differs (value %.17g, control word 0x%x, eax 0x%x host 0x%x, "
                "edx 0x%x host 0x%x)\n", bad == 1 ? "a register" : bad == 2 ? "the x87 state" : "the stack",
                f0.st[f0.top & 7], f0.cw, cpu.eax, c1.eax, cpu.edx, c1.edx);
    return bad;
}

RT_WRAP(00945640, ftol)
{
    if (ftol_mode < 0) {
        const char *v = getenv("RECOMP_FTOL");
        ftol_mode = !v ? 0 : !strcmp(v, "check") ? 1 : !strcmp(v, "original") ? 2 : -2;
        if (ftol_mode == -2) rt_fatal("RECOMP_FTOL=%s: use original or check", v);
    }
    if (ftol_mode == 0) { ftol_host(); return; }
    if (ftol_mode == 2) { o_00945640(); return; }
    if (ftol_check()) {
        if (ftol_mode == 1) abort();
        ftol_fails++;
    }
    if (ftol_mode == 1 && !ftol_checked++) atexit(print_ftol_checked);
}

/* The self-test: values around the integer steps, halves, the 64-bit limits, infinities and NaN, each with
 * the four rounding modes in the control word before the call. Gives the count of values that differ. */
int ftol_self_test(void)
{
    static const double base[] = {0.0, -0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.5, 3.49999, 99.99, 1e9, 2147483647.5,
                                  2147483648.0, 4294967295.9, 4294967296.0, 9.2233720368547748e18,
                                  9.2233720368547758e18, 1e19, 1e300, INFINITY, NAN, 4503599627370495.5,
                                  0.1, 1.0 / 3.0, 65535.5, 1e-310};
    uint32_t block = rt_alloc(4096), stack = block + 2048, n = 0;
    Fpu fpu_in = fpu;
    ftol_mode = 3;
    ftol_fails = 0;
    for (size_t i = 0; i < sizeof base / sizeof *base; i++)
        for (int sign = 0; sign < 2; sign++)
            for (uint16_t rc = 0; rc < 4; rc++) {
                Cpu saved = cpu;
                fpu.cw = (uint16_t)(0x027fu | (rc << 10));
                fpush(sign ? -base[i] : base[i]);
                cpu.esp = stack;
                cpu.ebp = 0x12345678u;
                cpu.eax = 0xa5a5a5a5u;
                rt_callback(0x945640u, 0, NULL);
                cpu = saved;
                n++;
            }
    fpu = fpu_in;
    rt_free(block);
    ftol_mode = -1;
    fprintf(stderr, "ftol test: %u values, %d differ\n", n, ftol_fails);
    return ftol_fails;
}
