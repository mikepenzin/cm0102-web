/* Runs one recompiled function of the original for tests/recomp/test_crashguards.py, and writes the memory
 * that the test compares with the Unicorn run of the original.
 *
 * Usage: guards_driver <cm0102.exe> <case file> <output folder>
 *
 * The case file is a list of 32-bit little-endian words:
 *   entry, ecx, flags (1 = the function returns a value in st(0)), nargs, args[nargs],
 *   nblocks, then per block: address, length, bytes (padded to 4),
 *   nqueues, then per queue: function address, count, values[count].
 * Page 0 is filled with 0xCC before the blocks, so that a NULL read gives a wrong value, not zeros.
 *
 * The functions that a guarded function calls are replaced by stubs (the test writes them as RT_REPLACE lines
 * that call guard_stub). A stub logs the call at GUARD_LOG and returns the next value of its queue, or 0.
 * The driver writes page0.bin, stack.bin, work.bin and image.bin, and prints "eax <x> esp <x> st0 <x>". */
#include "rt.h"
#include "rt_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GUARD_WORK 0x30000000u
#define GUARD_WORK_SIZE 0x200000u
#define GUARD_LOG (GUARD_WORK + 0x1f0000u)

static uint32_t *words;
static size_t nwords, at;

static uint32_t next(void)
{
    if (at >= nwords) rt_fatal("the case file is too short");
    return words[at++];
}

#define MAX_QUEUES 32
static struct { uint32_t addr, count, pos; uint32_t *values; } queues[MAX_QUEUES];
static uint32_t nqueues;

/* The crash guards of issue 132 log their first hit (win/original_faults.c). The test needs no line. */
void rt_crash_site(uint32_t site) { (void)site; }

void guard_stub(uint32_t addr, uint32_t pop)
{
    uint32_t n = rd32(g_mem, GUARD_LOG);
    uint32_t e = GUARD_LOG + 4 + 16 * n;
    wr32(g_mem, e, addr);
    wr32(g_mem, e + 4, cpu.ecx);
    wr32(g_mem, e + 8, rd32(g_mem, cpu.esp + 4));
    wr32(g_mem, e + 12, rd32(g_mem, cpu.esp + 8));
    wr32(g_mem, GUARD_LOG, n + 1);
    uint32_t v = 0;
    for (uint32_t i = 0; i < nqueues; i++) {
        if (queues[i].addr == addr && queues[i].pos < queues[i].count) { v = queues[i].values[queues[i].pos++]; break; }
    }
    cpu.eax = v;
    cpu.esp += 4 + pop;
}

static void dump(const char *dir, const char *name, uint32_t start, uint32_t end)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(g_mem + start, 1, end - start, f) != end - start) rt_fatal("cannot write %s", path);
    fclose(f);
}

static void push(uint32_t v)
{
    cpu.esp -= 4;
    wr32(g_mem, cpu.esp, v);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s <cm0102.exe> <case file> <output folder>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[2], "rb");
    if (!f) rt_fatal("cannot read %s", argv[2]);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    words = malloc((size_t)size + 4);
    nwords = fread(words, 1, (size_t)size, f) / 4;
    fclose(f);

    rt_init(argv[1]);
    memset(g_mem, 0xcc, 0x1000);

    uint32_t entry = next(), ecx = next(), flags = next(), nargs = next();
    uint32_t args[16];
    if (nargs > 16) rt_fatal("too many arguments");
    for (uint32_t i = 0; i < nargs; i++) args[i] = next();
    for (uint32_t n = next(); n; n--) {
        uint32_t addr = next(), len = next();
        memcpy(g_mem + addr, words + at, len);
        at += (len + 3) / 4;
    }
    nqueues = next();
    if (nqueues > MAX_QUEUES) rt_fatal("too many queues");
    for (uint32_t i = 0; i < nqueues; i++) {
        queues[i].addr = next();
        queues[i].count = next();
        queues[i].values = words + at;
        at += queues[i].count;
    }

    memset(&cpu, 0, sizeof cpu);
    cpu.esp = RT_STACK_TOP;
    for (uint32_t i = nargs; i; i--) push(args[i - 1]);
    push(RT_RETURN_ADDR);
    cpu.ecx = ecx;
    rt_call(entry);

    uint32_t eax = cpu.eax, esp = cpu.esp;
    uint64_t st0 = 0;
    if (flags & 1) {
        double d = fpu.st[fpu.top];
        memcpy(&st0, &d, 8);
        fpop();
    }
    uint32_t base = rt_image_base();
    dump(argv[3], "image.bin", base, base + rt_image_size());
    dump(argv[3], "stack.bin", RT_STACK_BASE, RT_STACK_BASE + RT_STACK_SIZE);
    dump(argv[3], "work.bin", GUARD_WORK, GUARD_WORK + GUARD_WORK_SIZE);
    dump(argv[3], "page0.bin", 0, 0x1000);
    printf("eax %x esp %x st0 %llx\n", eax, esp, (unsigned long long)st0);
    return 0;
}
