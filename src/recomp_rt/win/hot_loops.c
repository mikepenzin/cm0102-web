/* Host copies of hot loops of the original that game creation and the game days spend their time in
 * (docs/research/2026-10-29-new-game-speed.md, 0.6.2 slice 14). Each copy gives the same memory, stack bytes and
 * registers as the recompiled code, so the game stays the same (rule Q152). RECOMP_HOT=original runs the
 * recompiled code; RECOMP_HOT=check runs both on each call and stops at the first difference. The
 * self-test (RECOMP_QSORT_TEST, crt.c) calls hot_loops_self_test.
 *
 * 0x7ae350, thiscall (int *id), ret 4: is the id in the list of this? The list holds [this+8] rows of
 * 0x1a bytes from [this+4], with the id at +0 of a row. With all nations, the second "Initialising game
 * data" of a 2025/26 start spent most of its 50 s in this loop. It pushes esi and edi, so it leaves them
 * below esp. al is 1 when found, else 0; the upper bytes of eax are those of the found row, of the row
 * after the last, or of the argument; edx is the index of the row, or the count, when the loop runs.
 *
 * 0x69fc60, thiscall (int *id), ret 4: is the id in the linked list at [this+8]? (0.6.2 slice 14.) The
 * shortlist search fill_shortlist_slot 0x83f3a0 calls it for each candidate (0x83fa8e), and with its calls
 * of the list step 0x68d1b0 it took 13 % of the samples of July to September 2025 in the profile of
 * 30 October 2026. The other callers are 0x4ceca2 and add_job_vacancy 0x69ba50 (0x69bd8f, 0x69bfb3,
 * 0x69c20c, 0x69c5c2); each tests al next, so none reads the flags. The list holds a
 * 16-bit count at +0, the first node at +2 and a cursor at +0xa; a node holds its data at +0 and the next
 * node at +6, and the id is at +6 of the data. The code (0x69fc60..0x69fd4a) saves the cursor in ebx, sets
 * it to the first node, steps it with 0x68d1b0 up to count times and puts the saved cursor back, so the
 * list ends as it was. It pushes ebx, ebp, esi and edi below 0x200 bytes of locals, and each step pushes
 * its return address 0x69fc8e. al is 1 when found, else 0; the upper bytes of eax are those of the list
 * when found or when the count is 0 or less, else those of the count (movsx); edx is the id of the last
 * data read. A node or data of 0 before the count ends gives the original's error box: the host copy finds
 * that before it writes, and then runs the recompiled code.
 *
 * The index of 0x7ae350 (item 151). The only caller of 0x7ae350 is is_staff_suitable_for_job 0x69f810
 * (0x69fa4b, ecx = the PLAYER_REGEN of [0xb7434c]), and add_job_vacancy 0x69ba50 calls that for each
 * member of staff. With all nations a 2025/26 start makes 790,068 searches of a list of many thousand
 * rows. While a call of 0x69ba50 runs, the copy finds the row in a hash index of the ids (the first row
 * of each id) in place of the scan, and gives the same eax, edx and stack. The index lives from the first
 * search in a call of 0x69ba50 to the end of that call, and is made again when the list, its rows or its
 * count change. The list (rows at +4, count at +8, room at +0xc) is written only by the members of
 * PLAYER_REGEN: every reference to the object [0xb7434c] passes it to a member, and of the members that
 * give a pointer into the rows (0x7a7ed0) the callers 0x696ba5, 0x696bd7 and 0x83d3cb only test it for 0.
 * Each member that stores to +4, +8 or +0xc of a register or calls add_retirement 0x7a8510 or
 * remove_retirement 0x7a8690 drops the index when it starts and when it ends, whatever calls it, directly
 * or through an address (REGEN_WRITER below; 0x7abac0 matches that scan only by its writes to the staff
 * record). Of them 0x69ba50 reaches by direct calls only the destructor 0x7a56f0. The index also lives only
 * while esp is below the esp of the outermost 0x69ba50, so code that an unwind runs in an older frame scans.
 * A found row whose id differs from the index prints "recomp: hot index stale" and the copy scans;
 * RECOMP_HOT=check compares each search with the recompiled code. Outside 0x69ba50 the copy scans.
 *
 * 0x600a50 human_manager_is_human_club, cdecl (club, ignore_holiday, quick, manager, any), item 151. The
 * shortlist search fill_shortlist_slot 0x83f3a0 calls it for each candidate (0x83fab3), and it took 13 % of
 * "Creating Shortlists" with all nations. The copy runs only the common path of a club of the computer:
 * a club, and either no manager at [club+0xcf], or a manager other than the argument, or a manager below
 * the first human id ([0xae2364] - 0x10, jl at 0x600aa5); then club_is_national_team 0x5409b0 (called at
 * 0x600be6) gives 0: the club id [club] is below [0xae235c] - [0xae2350] (setge at 0x540a40). Every other
 * case runs the recompiled code, before the copy writes anything. The path leaves ebx, ebp, esi and edi
 * (pushes at 0x600a56..0x600a59) at esp - 0x204..esp - 0x210, the argument and return address 0x600beb
 * of the call at esp - 0x214 and esp - 0x218, and the esi that 0x5409b0 pushes (0x540a36) at esp - 0x41c.
 * eax and edx are 0, ecx holds [0xae235c] - [0xae2350]. No caller reads the flags (the port keeps no flags
 * across a call).
 *
 * 0x53a610 database_player_to_attribute_vector, cdecl (player, staff, vector, sides_end, positions_end,
 * weighted_end), item 151. database_player_initialisation 0x535a80 calls it for each player, and with all
 * nations it took 36 % of "Player Initialisation". It has no branch on the data: it clears the third byte of
 * the 63 rows of 3 bytes of the vector, writes 55 words (each a signed byte of the player record, the last of
 * the staff record at +0x58), 18 bytes of 1 or 2, and 9, 12 and 32 to the three end pointers that are not 0.
 * The tables below are read from 0x53a610..0x53a8a4 in that order. It pushes esi (esp - 4). It returns eax =
 * the vector, ecx = the upper half of the player pointer and the sign-extended byte at staff + 0x58 in cx, and
 * edx = the staff. */
#include "win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_HAS 0x7ae350u
#define LLIST_HAS 0x69fc60u
#define HUMAN_CLUB 0x600a50u
#define ATTR_VECTOR 0x53a610u

void o_007ae350(void);
void o_0069fc60(void);
void o_0069ba50(void);
void o_0053a610(void);
void o_00600a50(void);

static void attr_vector_host(void);
static int attr_vector_check(void);

static int hot_mode = -1;          /* 0 host, 1 check, 2 original, 3 the self-test */

/* The index of 0x7ae350: open addressing on the id, the first row of each id. */
static struct {
    uint32_t list, rows, n;        /* the list it was made for: this, [this+4], [this+8] */
    uint32_t mask;
    uint32_t *ids;
    int32_t *at;                   /* the row, or -1 for an empty place */
    int valid;
} ridx;
static int vacancy_depth;          /* calls of add_job_vacancy 0x69ba50 that run */
static uint32_t vacancy_esp;       /* the esp at the start of the outermost of them */
static uint64_t ridx_builds;

static uint32_t ridx_hash(uint32_t id) { return (id * 0x9e3779b1u) >> 7; }

static void ridx_build(uint32_t list, uint32_t rows, uint32_t n)
{
    uint32_t want = 16;
    while (want < n * 2) want <<= 1;
    if (want - 1 != ridx.mask || !ridx.ids) {
        free(ridx.ids);
        free(ridx.at);
        ridx.ids = malloc(want * sizeof *ridx.ids);
        ridx.at = malloc(want * sizeof *ridx.at);
        if (!ridx.ids || !ridx.at) { fprintf(stderr, "recomp: out of memory for the hot index\n"); abort(); }
        ridx.mask = want - 1;
    }
    memset(ridx.at, 0xff, want * sizeof *ridx.at);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t id = rd32(g_mem, rows + i * 0x1a), h = ridx_hash(id) & ridx.mask;
        while (ridx.at[h] >= 0 && ridx.ids[h] != id) h = (h + 1) & ridx.mask;
        if (ridx.at[h] < 0) { ridx.ids[h] = id; ridx.at[h] = (int32_t)i; }   /* the first row of the id stays */
    }
    ridx.list = list, ridx.rows = rows, ridx.n = n, ridx.valid = 1;
    ridx_builds++;
}

/* The row of the id, or n when no row has it; -1 when the index cannot answer. */
static int64_t ridx_find(uint32_t list, uint32_t rows, uint32_t n, uint32_t id)
{
    if (!vacancy_depth || cpu.esp >= vacancy_esp || (int32_t)n <= 0 || n > 0x1000000u) return -1;
    if (!ridx.valid || ridx.list != list || ridx.rows != rows || ridx.n != n) ridx_build(list, rows, n);
    uint32_t h = ridx_hash(id) & ridx.mask;
    while (ridx.at[h] >= 0) {
        if (ridx.ids[h] == id) {
            uint32_t i = (uint32_t)ridx.at[h];
            if (rd32(g_mem, rows + i * 0x1a) == id) return i;
            fprintf(stderr, "recomp: hot index stale: row %u of 0x%x\n", i, rows);
            ridx.valid = 0;
            return -1;
        }
        h = (h + 1) & ridx.mask;
    }
    return n;
}

static void list_has_host(void)
{
    uint32_t esp = cpu.esp, arg = rd32(g_mem, esp + 4), eax = arg, edx = cpu.edx;
    wr32(g_mem, esp - 4, cpu.esi);
    wr32(g_mem, esp - 8, cpu.edi);
    int found = 0;
    uint32_t n;
    if (arg && (n = rd32(g_mem, cpu.ecx + 8)) != 0) {
        edx = 0;
        if ((int32_t)n > 0) {
            uint32_t id = rd32(g_mem, arg), row = rd32(g_mem, cpu.ecx + 4);
            int64_t at = ridx_find(cpu.ecx, row, n, id);
            if (at >= 0) {
                found = (uint32_t)at < n;
                edx = (uint32_t)at;
                row += (uint32_t)at * 0x1a;
            } else for (;;) {
                if (rd32(g_mem, row) == id) { found = 1; break; }
                edx++;
                row += 0x1a;
                if ((int32_t)edx >= (int32_t)n) break;
            }
            eax = row;
        }
    }
    cpu.eax = (eax & 0xffffff00u) | (uint32_t)found;
    cpu.edx = edx;
    cpu.esp = esp + 8;
}

/* Both from the same state; the original's result stays. 0 when all is equal. */
static int list_has_check(void)
{
    uint32_t low = cpu.esp - 8;
    uint8_t before[12], host[12];
    memcpy(before, g_mem + low, 12);
    Cpu in = cpu;
    list_has_host();
    Cpu want = cpu;
    memcpy(host, g_mem + low, 12);
    memcpy(g_mem + low, before, 12);
    cpu = in;
    o_007ae350();
    int bad = want.eax != cpu.eax || want.edx != cpu.edx || want.ecx != cpu.ecx || want.esi != cpu.esi
              || want.edi != cpu.edi || want.esp != cpu.esp || memcmp(host, g_mem + low, 12);
    if (bad)
        fprintf(stderr, "recomp: hot check: 0x7ae350 differs (eax 0x%x host 0x%x, edx 0x%x host 0x%x)\n", cpu.eax,
                want.eax, cpu.edx, want.edx);
    return bad;
}

/* 0 when the walk would reach the error box, else 1 with the result in the registers and the stack. */
static int llist_has_host(void)
{
    uint32_t esp = cpu.esp, list = rd32(g_mem, cpu.ecx + 8), arg = rd32(g_mem, esp + 4);
    int32_t n = (int16_t)rd16(g_mem, list);
    uint32_t eax = list, edx = cpu.edx, node = rd32(g_mem, list + 2);
    int found = 0;
    if (n > 0) {
        for (int32_t i = 0; i < n; i++) {
            if (!node) return 0;
            uint32_t data = rd32(g_mem, node);
            if (!data) return 0;
            node = rd32(g_mem, node + 6);
            edx = rd32(g_mem, data + 6);
            if (edx == rd32(g_mem, arg)) { found = 1; break; }
        }
        if (!found) eax = (uint32_t)n;
        wr32(g_mem, esp - 0x214, 0x69fc8eu);
    }
    wr32(g_mem, esp - 0x204, cpu.ebx);
    wr32(g_mem, esp - 0x208, cpu.ebp);
    wr32(g_mem, esp - 0x20c, cpu.esi);
    wr32(g_mem, esp - 0x210, cpu.edi);
    cpu.eax = (eax & 0xffffff00u) | (uint32_t)found;
    cpu.ecx = list;
    cpu.edx = edx;
    cpu.esp = esp + 8;
    return 1;
}

static int llist_has_check(void)
{
    uint32_t low = cpu.esp - 0x214;
    uint8_t before[0x14], host[0x14];
    memcpy(before, g_mem + low, 0x14);
    Cpu in = cpu;
    if (!llist_has_host()) { o_0069fc60(); return 0; }
    Cpu want = cpu;
    memcpy(host, g_mem + low, 0x14);
    memcpy(g_mem + low, before, 0x14);
    cpu = in;
    o_0069fc60();
    int bad = want.eax != cpu.eax || want.ecx != cpu.ecx || want.edx != cpu.edx || want.ebx != cpu.ebx
              || want.ebp != cpu.ebp || want.esi != cpu.esi || want.edi != cpu.edi || want.esp != cpu.esp
              || memcmp(host, g_mem + low, 0x14);
    if (bad)
        fprintf(stderr, "recomp: hot check: 0x69fc60 differs (eax 0x%x host 0x%x, edx 0x%x host 0x%x)\n", cpu.eax,
                want.eax, cpu.edx, want.edx);
    return bad;
}

static unsigned long long hot_checked;
static int hot_test_fails;

static void print_hot_checked(void) { fprintf(stderr, "recomp: hot check: %llu calls equal\n", hot_checked); }

static void hot_mode_read(void)
{
    if (hot_mode < 0) {
        const char *v = getenv("RECOMP_HOT");
        hot_mode = !v ? 0 : !strcmp(v, "check") ? 1 : !strcmp(v, "original") ? 2 : 0;
    }
}

RT_WRAP(007ae350, list_has_id)
{
    hot_mode_read();
    if (hot_mode == 2) { o_007ae350(); return; }
    if (hot_mode == 1) {
        if (list_has_check()) abort();
        if (!hot_checked++) atexit(print_hot_checked);
        return;
    }
    if (hot_mode == 3) { hot_test_fails += list_has_check(); return; }
    list_has_host();
}

/* 1 with the result in the registers and the stack; 0 when the case is not the common one (nothing written). */
static uint64_t human_club_taken;  /* the self-test checks that the copy takes its cases */

static int human_club_host(void)
{
    uint32_t esp = cpu.esp, club = rd32(g_mem, esp + 4), manager = rd32(g_mem, esp + 0x10);
    if (!club) return 0;
    uint32_t mgr = rd32(g_mem, club + 0xcf), esi = manager ? manager : mgr;
    if (mgr && esi == mgr && (int32_t)rd32(g_mem, esi) >= (int32_t)(rd32(g_mem, 0xae2364u) - 0x10u)) return 0;
    uint32_t limit = rd32(g_mem, 0xae235cu) - rd32(g_mem, 0xae2350u);
    if ((int32_t)rd32(g_mem, club) >= (int32_t)limit) return 0;              /* a national side */
    wr32(g_mem, esp - 0x204, cpu.ebx);
    wr32(g_mem, esp - 0x208, cpu.ebp);
    wr32(g_mem, esp - 0x20c, cpu.esi);
    wr32(g_mem, esp - 0x210, cpu.edi);
    wr32(g_mem, esp - 0x214, club);
    wr32(g_mem, esp - 0x218, 0x600bebu);
    wr32(g_mem, esp - 0x41c, esi);
    cpu.eax = 0;
    cpu.ecx = limit;
    cpu.edx = 0;
    cpu.esp = esp + 4;
    human_club_taken++;
    return 1;
}

static int human_club_check(void)
{
    uint32_t low = cpu.esp - 0x420;
    static uint8_t before[0x424], host[0x424];
    memcpy(before, g_mem + low, sizeof before);
    Cpu in = cpu;
    int took = human_club_host();
    Cpu want = cpu;
    memcpy(host, g_mem + low, sizeof host);
    memcpy(g_mem + low, before, sizeof before);
    cpu = in;
    o_00600a50();
    if (!took) return 0;
    int bad = memcmp(&want, &cpu, sizeof cpu) || memcmp(host, g_mem + low, sizeof host);
    if (bad)
        fprintf(stderr, "recomp: hot check: 0x600a50 differs (eax 0x%x host 0x%x, ecx 0x%x host 0x%x)\n", cpu.eax,
                want.eax, cpu.ecx, want.ecx);
    return bad;
}

RT_WRAP(0053a610, attr_vector)
{
    hot_mode_read();
    if (hot_mode == 2) { o_0053a610(); return; }
    if (hot_mode == 1) {
        if (attr_vector_check()) abort();
        if (!hot_checked++) atexit(print_hot_checked);
        return;
    }
    if (hot_mode == 3) { hot_test_fails += attr_vector_check(); return; }
    attr_vector_host();
}

RT_WRAP(00600a50, human_club)
{
    win_sort_step();                          /* the browser's page has a moment in a day's long work (user32.c) */
    hot_mode_read();
    if (hot_mode == 2) { o_00600a50(); return; }
    if (hot_mode == 1) {
        if (human_club_check()) abort();
        if (!hot_checked++) atexit(print_hot_checked);
        return;
    }
    if (hot_mode == 3) { hot_test_fails += human_club_check(); return; }
    if (!human_club_host()) o_00600a50();
}

/* 0x53a610: its writes in the order of the code (0x53a625..0x53a8a4). W: a word of the vector at +a from the signed
 * byte of the player at +b; T: the same from the staff; B: the byte b at +a; P, S, G: the byte b to positions_end,
 * sides_end, weighted_end when that pointer is not 0; U: push esi. */
static const struct { char op; uint8_t a, b; } attr_ops[77] = {
    {'W', 0x0, 0xf},
    {'W', 0x3, 0x10},
    {'W', 0x6, 0x11},
    {'W', 0x9, 0x12},
    {'W', 0xc, 0x13},
    {'W', 0xf, 0x14},
    {'W', 0x12, 0x15},
    {'W', 0x15, 0x16},
    {'W', 0x18, 0x1a},
    {'P', 0, 9},
    {'W', 0x1b, 0x17},
    {'W', 0x1e, 0x18},
    {'W', 0x21, 0x19},
    {'S', 0, 12},
    {'W', 0x24, 0x30},
    {'U', 0, 0},
    {'W', 0x27, 0x3b},
    {'W', 0x2a, 0x1e},
    {'B', 0x2f, 2},
    {'W', 0x2d, 0x23},
    {'W', 0x30, 0x24},
    {'B', 0x35, 2},
    {'W', 0x33, 0x26},
    {'B', 0x38, 2},
    {'W', 0x36, 0x27},
    {'B', 0x3b, 1},
    {'W', 0x39, 0x2a},
    {'B', 0x3e, 2},
    {'W', 0x3c, 0x2b},
    {'B', 0x41, 2},
    {'W', 0x3f, 0x31},
    {'B', 0x44, 2},
    {'W', 0x42, 0x32},
    {'B', 0x47, 2},
    {'W', 0x45, 0x33},
    {'W', 0x48, 0x35},
    {'B', 0x4d, 2},
    {'W', 0x4b, 0x37},
    {'B', 0x50, 2},
    {'W', 0x4e, 0x38},
    {'W', 0x51, 0x39},
    {'W', 0x54, 0x3a},
    {'B', 0x59, 2},
    {'W', 0x57, 0x3e},
    {'W', 0x5a, 0x41},
    {'B', 0x5f, 2},
    {'W', 0x5d, 0x43},
    {'G', 0, 32},
    {'B', 0x62, 2},
    {'W', 0x60, 0x28},
    {'B', 0x65, 2},
    {'W', 0x63, 0x22},
    {'W', 0x66, 0x29},
    {'B', 0x6b, 2},
    {'W', 0x69, 0x40},
    {'W', 0x6c, 0x3d},
    {'W', 0x6f, 0x3c},
    {'W', 0x72, 0x36},
    {'W', 0x75, 0x2e},
    {'W', 0x78, 0x2f},
    {'W', 0x7b, 0x25},
    {'W', 0x7e, 0x1f},
    {'W', 0x81, 0x20},
    {'W', 0x84, 0x21},
    {'B', 0x89, 2},
    {'W', 0x87, 0x1b},
    {'W', 0x8a, 0x1c},
    {'W', 0x8d, 0x1d},
    {'W', 0x90, 0x2c},
    {'W', 0x93, 0x2d},
    {'W', 0x96, 0x42},
    {'B', 0x9b, 2},
    {'W', 0x99, 0x3f},
    {'B', 0x9e, 2},
    {'W', 0x9c, 0x44},
    {'W', 0x9f, 0x34},
    {'T', 0xa2, 0x58}};

static void attr_vector_host(void)
{
    uint32_t esp = cpu.esp, player = rd32(g_mem, esp + 4), staff = rd32(g_mem, esp + 8), vec = rd32(g_mem, esp + 0xc);
    uint32_t sides = rd32(g_mem, esp + 0x10), positions = rd32(g_mem, esp + 0x14), weighted = rd32(g_mem, esp + 0x18);
    uint32_t last = 0;
    for (uint32_t k = 0; k < 0x3f; k++) wr8(g_mem, vec + 2 + 3 * k, 0);
    for (int k = 0; k < 77; k++) {
        uint32_t a = attr_ops[k].a, b = attr_ops[k].b;
        switch (attr_ops[k].op) {
        case 'W': wr16(g_mem, vec + a, (uint32_t)(int32_t)(int8_t)rd8(g_mem, player + b)); break;
        case 'T': last = (uint32_t)(int32_t)(int8_t)rd8(g_mem, staff + b); wr16(g_mem, vec + a, last); break;
        case 'B': wr8(g_mem, vec + a, b); break;
        case 'P': if (positions) wr8(g_mem, positions, b); break;
        case 'S': if (sides) wr8(g_mem, sides, b); break;
        case 'G': if (weighted) wr8(g_mem, weighted, b); break;
        case 'U': wr32(g_mem, esp - 4, cpu.esi); break;
        }
    }
    cpu.eax = vec;
    cpu.ecx = (player & 0xffff0000u) | (last & 0xffffu);
    cpu.edx = staff;
    cpu.esp = esp + 4;
}

static int attr_vector_check(void)
{
    uint32_t esp = cpu.esp, vec = rd32(g_mem, esp + 0xc);
    uint32_t outs[3] = {rd32(g_mem, esp + 0x10), rd32(g_mem, esp + 0x14), rd32(g_mem, esp + 0x18)};
    uint8_t before[0xc0], host[0xc0], ob[3] = {0}, oh[3] = {0};
    uint32_t sb = rd32(g_mem, esp - 4), sh;
    memcpy(before, g_mem + vec, sizeof before);
    for (int i = 0; i < 3; i++) if (outs[i]) ob[i] = g_mem[outs[i]];
    Cpu in = cpu;
    attr_vector_host();
    Cpu want = cpu;
    memcpy(host, g_mem + vec, sizeof host);
    for (int i = 0; i < 3; i++) if (outs[i]) { oh[i] = g_mem[outs[i]]; g_mem[outs[i]] = ob[i]; }
    sh = rd32(g_mem, esp - 4);
    memcpy(g_mem + vec, before, sizeof before);
    wr32(g_mem, esp - 4, sb);
    cpu = in;
    o_0053a610();
    int bad = memcmp(&want, &cpu, sizeof cpu) || memcmp(host, g_mem + vec, sizeof host) || sh != rd32(g_mem, esp - 4);
    for (int i = 0; i < 3; i++) if (outs[i] && oh[i] != g_mem[outs[i]]) bad = 1;
    if (bad) fprintf(stderr, "recomp: hot check: 0x53a610 differs (ecx 0x%x host 0x%x)\n", cpu.ecx, want.ecx);
    return bad;
}

/* MANAGER_MANAGER::add_job_vacancy 0x69ba50: the index of 0x7ae350 lives while it runs. A nested call
 * or an unwind drops it too. */
RT_WRAP(0069ba50, job_vacancy_index)
{
    if (!vacancy_depth++) vacancy_esp = cpu.esp;
    ridx.valid = 0;
    o_0069ba50();
    ridx.valid = 0;
    if (--vacancy_depth < 0 || rt_unwinding) vacancy_depth = 0;
}

/* The members of PLAYER_REGEN that may write the list of 0x7ae350 drop its index (see the head of the file). */
void o_007a5380(void); /* PLAYER_REGEN::PLAYER_REGEN */
RT_WRAP(007a5380, regen_writer_007a5380) { ridx.valid = 0; o_007a5380(); ridx.valid = 0; }
void o_007a56f0(void); /* ~PLAYER_REGEN, frees the list */
RT_WRAP(007a56f0, regen_writer_007a56f0) { ridx.valid = 0; o_007a56f0(); ridx.valid = 0; }
void o_007a5790(void); /* find_new_retirements */
RT_WRAP(007a5790, regen_writer_007a5790) { ridx.valid = 0; o_007a5790(); ridx.valid = 0; }
void o_007a58f0(void); /* build_initial_retirement_list */
RT_WRAP(007a58f0, regen_writer_007a58f0) { ridx.valid = 0; o_007a58f0(); ridx.valid = 0; }
void o_007a59e0(void); /* process_retirements */
RT_WRAP(007a59e0, regen_writer_007a59e0) { ridx.valid = 0; o_007a59e0(); ridx.valid = 0; }
void o_007a7310(void);
RT_WRAP(007a7310, regen_writer_007a7310) { ridx.valid = 0; o_007a7310(); ridx.valid = 0; }
void o_007a79c0(void);
RT_WRAP(007a79c0, regen_writer_007a79c0) { ridx.valid = 0; o_007a79c0(); ridx.valid = 0; }
void o_007a7f20(void); /* schedule_injury_retirement */
RT_WRAP(007a7f20, regen_writer_007a7f20) { ridx.valid = 0; o_007a7f20(); ridx.valid = 0; }
void o_007a8050(void);
RT_WRAP(007a8050, regen_writer_007a8050) { ridx.valid = 0; o_007a8050(); ridx.valid = 0; }
void o_007a82b0(void); /* remove_spare_non_player_record */
RT_WRAP(007a82b0, regen_writer_007a82b0) { ridx.valid = 0; o_007a82b0(); ridx.valid = 0; }
void o_007a8470(void); /* remove_player_data_from_pool */
RT_WRAP(007a8470, regen_writer_007a8470) { ridx.valid = 0; o_007a8470(); ridx.valid = 0; }
void o_007a8510(void); /* add_retirement */
RT_WRAP(007a8510, regen_writer_007a8510) { ridx.valid = 0; o_007a8510(); ridx.valid = 0; }
void o_007a8690(void); /* remove_retirement */
RT_WRAP(007a8690, regen_writer_007a8690) { ridx.valid = 0; o_007a8690(); ridx.valid = 0; }
void o_007a8780(void);
RT_WRAP(007a8780, regen_writer_007a8780) { ridx.valid = 0; o_007a8780(); ridx.valid = 0; }
void o_007ab340(void);
RT_WRAP(007ab340, regen_writer_007ab340) { ridx.valid = 0; o_007ab340(); ridx.valid = 0; }
void o_007adcd0(void); /* load_retire_dat */
RT_WRAP(007adcd0, regen_writer_007adcd0) { ridx.valid = 0; o_007adcd0(); ridx.valid = 0; }

RT_WRAP(0069fc60, llist_has_id)
{
    hot_mode_read();
    if (hot_mode == 2) { o_0069fc60(); return; }
    if (hot_mode == 1) {
        if (llist_has_check()) abort();
        if (!hot_checked++) atexit(print_hot_checked);
        return;
    }
    if (hot_mode == 3) { hot_test_fails += llist_has_check(); return; }
    if (!llist_has_host()) o_0069fc60();
}

/* Lists of 0 to 40 rows with ids from a small range, found and not found, a null argument and a count
 * below 0. Gives the count of calls that differ. */
int hot_loops_self_test(void)
{
    uint32_t list = rt_alloc(16), rows = rt_alloc(41 * 0x1a), idp = rt_alloc(4), seed = 777;
    int calls = 0;
    hot_mode = 3;
    for (int n = -2; n <= 40; n++)
        for (int k = 0; k < 30; k++) {
            for (int i = 0; i < 41 * 0x1a; i++) {
                seed = seed * 1103515245u + 12345u;
                g_mem[rows + (uint32_t)i] = (uint8_t)(seed >> 16);
            }
            for (int i = 0; i < 41; i++) wr32(g_mem, rows + (uint32_t)i * 0x1a, (seed >> 8) % 7 + (uint32_t)i % 5);
            wr32(g_mem, list + 4, rows);
            wr32(g_mem, list + 8, (uint32_t)n);
            wr32(g_mem, idp, (uint32_t)k % 12);
            cpu.ecx = list;
            cpu.edx = 0x1234u + (uint32_t)k;
            cpu.eax = 0xabcdef01u;
            uint32_t args[1] = {k == 29 ? 0 : idp};
            rt_callback(LIST_HAS, 1, args);
            calls++;
        }
    /* The index (item 151): as inside add_job_vacancy, one list and many searches. The rows change only
     * with the index dropped, as the destructor and a new call of 0x69ba50 drop it. Lists of up to 3000
     * rows hold repeated ids, so the first row of an id must win. */
    uint32_t big = rt_alloc(3001 * 0x1a);
    vacancy_depth = 1;
    vacancy_esp = 0xffffffffu;
    for (int n = -2; n <= 3000; n += n < 40 ? 1 : 397) {
        ridx.valid = 0;
        for (int i = 0; i < 3001 * 0x1a; i++) {
            seed = seed * 1103515245u + 12345u;
            g_mem[big + (uint32_t)i] = (uint8_t)(seed >> 16);
        }
        for (int i = 0; i < 3001; i++) {
            seed = seed * 1103515245u + 12345u;
            wr32(g_mem, big + (uint32_t)i * 0x1a, (seed >> 8) % (uint32_t)(n > 4 ? n : 4) + (i % 3 ? 0 : 0x7fffffffu));
        }
        wr32(g_mem, list + 4, big);
        wr32(g_mem, list + 8, (uint32_t)n);
        for (int k = 0; k < 60; k++) {
            wr32(g_mem, idp, (uint32_t)k * 37u + (k % 4 ? 0 : 0x7fffffffu));
            cpu.ecx = list;
            cpu.edx = 0x5678u + (uint32_t)k;
            cpu.eax = 0x12345678u;
            uint32_t args[1] = {k == 59 ? 0 : idp};
            rt_callback(LIST_HAS, 1, args);
            calls++;
        }
    }
    if (!ridx_builds) hot_test_fails++;            /* the index must have run */
    /* A row whose id changes while the index is valid: the copy must see that the found row is stale and scan.
     * Then a search with esp at or above the esp of 0x69ba50 (an older frame): the copy must scan, with no
     * index made. */
    wr32(g_mem, list + 8, 3000);
    wr32(g_mem, big + 1500 * 0x1a, 0x55555555u);    /* an id no other row holds */
    ridx.valid = 0;
    wr32(g_mem, idp, 0x55555555u);
    for (int k = 0; k < 2; k++) {
        cpu.ecx = list;
        uint32_t args[1] = {idp};
        rt_callback(LIST_HAS, 1, args);                 /* k 0: makes the index; k 1: the found row is stale */
        wr32(g_mem, big + 1500 * 0x1a, 0x66666666u);
        calls++;
    }
    if (ridx.valid) hot_test_fails++;               /* the stale row must drop the index */
    uint64_t builds = ridx_builds;
    vacancy_esp = cpu.esp - 0x100;                  /* below the esp of the call: an older frame */
    for (int k = 0; k < 4; k++) {
        wr32(g_mem, idp, (uint32_t)k);
        cpu.ecx = list;
        uint32_t args[1] = {idp};
        rt_callback(LIST_HAS, 1, args);
        calls++;
    }
    if (ridx_builds != builds) hot_test_fails++;
    vacancy_depth = 0;
    ridx.valid = 0;
    /* 0x600a50 (item 151): clubs with ids around the national-side limit, with no manager, the argument's
     * manager or another, and manager ids around the first human id. The three globals are put back. */
    uint32_t g64 = rd32(g_mem, 0xae2364u), g5c = rd32(g_mem, 0xae235cu), g50 = rd32(g_mem, 0xae2350u);
    uint32_t club = rt_alloc(0x100), man = rt_alloc(8), other = rt_alloc(8);
    wr32(g_mem, 0xae2364u, 5000);
    wr32(g_mem, 0xae235cu, 9000);
    wr32(g_mem, 0xae2350u, 1000);
    for (int cid = 7990; cid <= 8010; cid += 5)
        for (int m = 0; m < 3; m++)
            for (int arg = 0; arg < 3; arg++)
                for (int mid = 4979; mid <= 4995; mid += mid < 4985 ? 1 : 5) {   /* the jl at 0x600aa5: 4984 */
                    wr32(g_mem, club, (uint32_t)cid);
                    wr32(g_mem, club + 0xcf, m == 0 ? 0 : m == 1 ? man : other);
                    wr32(g_mem, man, (uint32_t)mid);
                    wr32(g_mem, other, (uint32_t)mid + 3);
                    cpu.eax = 0x11111111u, cpu.ecx = 0x22222222u, cpu.edx = 0x33333333u;
                    cpu.ebx = 0x1111u, cpu.ebp = 0x2222u, cpu.esi = 0x3333u, cpu.edi = 0x4444u;
                    uint32_t args[5] = {club, 1, 1, arg == 0 ? 0 : arg == 1 ? man : other, 0};
                    uint32_t esi = args[3] ? args[3] : rd32(g_mem, club + 0xcf), mgr = rd32(g_mem, club + 0xcf);
                    int common = cid < 8000 && !(mgr && esi == mgr && (int32_t)rd32(g_mem, esi) >= 4984);
                    if (!common) {
                        /* The original's other paths need a game (0x5fe040, 0x53b3d0): the copy must only decline. */
                        uint32_t sp = cpu.esp - 0x40;
                        for (int i = 0; i < 5; i++) wr32(g_mem, sp + 4 + 4u * (uint32_t)i, args[i]);
                        Cpu in = cpu;
                        in.esp = sp;
                        cpu = in;
                        if (human_club_host() || memcmp(&in, &cpu, sizeof cpu)) hot_test_fails++;
                        cpu.esp = in.esp + 0x40;
                        calls++;
                        continue;
                    }
                    uint64_t taken = human_club_taken;
                    rt_callback(HUMAN_CLUB, 5, args);
                    if (human_club_taken == taken) hot_test_fails++;    /* the copy must take a common case */
                    calls++;
                }
    wr32(g_mem, 0xae2364u, g64);
    wr32(g_mem, 0xae235cu, g5c);
    wr32(g_mem, 0xae2350u, g50);
    /* 0x53a610 (item 151): random player and staff records, each end pointer set or 0. */
    uint32_t prec = rt_alloc(0x80), srec = rt_alloc(0x80), vec = rt_alloc(0xc0), ends = rt_alloc(4);
    for (int k = 0; k < 64; k++) {
        for (int i = 0; i < 0x80; i++) {
            seed = seed * 1103515245u + 12345u;
            g_mem[prec + (uint32_t)i] = (uint8_t)(seed >> 16);
            g_mem[srec + (uint32_t)i] = (uint8_t)(seed >> 8);
        }
        for (int i = 0; i < 0xc0; i++) g_mem[vec + (uint32_t)i] = (uint8_t)(i * 7 + k);
        uint32_t args[6] = {prec, srec, vec, k & 1 ? ends : 0, k & 2 ? ends + 1 : 0, k & 4 ? ends + 2 : 0};
        cpu.eax = 0x1111u, cpu.ecx = 0x2222u + (uint32_t)k, cpu.edx = 0x3333u, cpu.esi = 0x4444u + (uint32_t)k;
        rt_callback(ATTR_VECTOR, 6, args);
        calls++;
    }
    /* Linked lists of 0 to 30 nodes, a count from -2 to the length (a count below the length stops early),
     * ids from a small range, found and not found. */
    uint32_t head = rt_alloc(16), nodes = rt_alloc(31 * 10), datas = rt_alloc(31 * 10), this_ = rt_alloc(12);
    wr32(g_mem, this_ + 8, head);
    for (int len = 0; len <= 30; len++)
        for (int n = -2; n <= len; n++)
            for (int k = 0; k < 8; k++) {
                for (int i = 0; i < len; i++) {
                    seed = seed * 1103515245u + 12345u;
                    uint32_t node = nodes + (uint32_t)i * 10, data = datas + (uint32_t)i * 10;
                    wr32(g_mem, node, data);
                    wr32(g_mem, node + 6, i + 1 < len ? node + 10 : 0);
                    wr32(g_mem, data + 6, (seed >> 16) % 9);
                }
                wr16(g_mem, head, (uint32_t)n);
                wr32(g_mem, head + 2, len ? nodes : 0);
                wr32(g_mem, head + 0xa, 0x5150u + (uint32_t)k);
                wr32(g_mem, idp, (uint32_t)k);
                cpu.ecx = this_;
                cpu.edx = 0x4321u + (uint32_t)k;
                cpu.eax = 0xfedcba98u;
                cpu.ebx = 0x1111u, cpu.ebp = 0x2222u, cpu.esi = 0x3333u, cpu.edi = 0x4444u;
                uint32_t args[1] = {idp};
                rt_callback(LLIST_HAS, 1, args);
                if (rd32(g_mem, head + 0xa) != 0x5150u + (uint32_t)k) hot_test_fails++;
                calls++;
            }
    hot_mode = -1;
    fprintf(stderr, "hot loops test: %d calls, %d differ\n", calls, hot_test_fails);
    return hot_test_fails;
}
