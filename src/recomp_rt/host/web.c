/* The browser host (Emscripten): the parts of the runtime that differ from host/posix.c when the recompiled game runs
 * as WebAssembly in a page (web/README.md). The rest of posix.c serves the browser as it is: files are Emscripten's
 * file system, which the page fills with the game files before the game starts.
 *
 * Memory. Natively rt.c reserves 4 GB at some host address and adds it to every guest address (g_mem). WebAssembly
 * (wasm32) has one memory of at most 4 GB, so here a guest address IS a memory address: g_mem is 0. The guest uses
 * the low page (fs:[0]), the PE image from 0x400000, the game stack at 0x10000000 (main.c), the static block at
 * 0x60000000 (kernel32.c) and the heap from 0x70000000 to 0xEF000000 (rt_host.h). The addresses at 0x63000000 (host
 * functions) and 0xF0000000 (standard handles) are names, never memory. The browser build's own data, its C stack and
 * its malloc heap start at GLOBAL_BASE 0x21000000 (the link options in CMakeLists.txt), in the free gap below the
 * static block, and must stay below it: web_check_host_heap stops the game with a message if they ever reach it. Each
 * guest address keeps its native value, so blocks sit where they sit on the desktop (rule Q152 depends on that).
 *
 * Time. The page draws and takes input only while the game gives it time. The game never returns from its screen
 * loop, so the browser build is linked with JSPI (or Asyncify) and the runtime sleeps through emscripten_sleep: at
 * each idle sleep (user32.c) and, through rt_web_yield, at least every YIELD_MS while the game polls for input. */
#ifdef __EMSCRIPTEN__

#include "../rt.h"
#include "../rt_host.h"

#include <emscripten.h>
#include <emscripten/heap.h>
#include <emscripten/stack.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define WEB_GLOBAL_BASE 0x21000000u     /* the link option -sGLOBAL_BASE */
#define WEB_HOST_LIMIT 0x5F000000u      /* the host heap stops 16 MB before the static block at 0x60000000 */
#define WEB_SLACK (64u << 20)           /* grow the memory this far past what the guest needs, to grow less often */
#define WEB_MEMORY_MAX 0xFFFF0000u      /* 4 GB less one page: the largest wasm32 memory */
#define YIELD_MS 12.0

static size_t memory_size;               /* the memory as last grown: emscripten_get_heap_size is a call into JS */

static void ensure(uint64_t top)
{
    if (top <= memory_size) return;
    uint64_t want = top + WEB_SLACK;
    if (want > WEB_MEMORY_MAX) want = WEB_MEMORY_MAX;
    if (top > want || !emscripten_resize_heap((size_t)want))
        rt_fatal("the browser gave no more memory: the game needs %llu MB", (unsigned long long)(top >> 20));
    memory_size = emscripten_get_heap_size();
}

static void web_check_host_heap(void)
{
    uintptr_t brk = (uintptr_t)sbrk(0);
    if (brk >= WEB_HOST_LIMIT)
        rt_fatal("the browser build's own heap reached the game's memory (%#lx)", (unsigned long)brk);
}

uint8_t *rt_web_memory(void)
{
    /* The layout this file relies on: the host's data and stack above GLOBAL_BASE, below the static block. */
    static int probe;
    uintptr_t data = (uintptr_t)&probe, stack = emscripten_stack_get_base(), end = emscripten_stack_get_end();
    if (data < WEB_GLOBAL_BASE || stack < WEB_GLOBAL_BASE || end < WEB_GLOBAL_BASE || data >= WEB_HOST_LIMIT)
        rt_fatal("the browser build has its data at %#lx and its stack at %#lx to %#lx, not above %#x: link it with "
                 "-sGLOBAL_BASE=%u (CMakeLists.txt)", (unsigned long)data, (unsigned long)end, (unsigned long)stack,
                 WEB_GLOBAL_BASE, WEB_GLOBAL_BASE);
    memory_size = emscripten_get_heap_size();
    ensure((uint64_t)RT_HEAP_BASE + WEB_SLACK);          /* every fixed region and the start of the heap */
    web_check_host_heap();
    fprintf(stderr, "recomp: web memory %zu MB, host data at %#lx, host heap at %#lx\n", memory_size >> 20,
            (unsigned long)data, (unsigned long)(uintptr_t)sbrk(0));
    return (uint8_t *)0;
}

/* heap.c: the guest heap's top moved up. */
void rt_web_heap_top(uint32_t top)
{
    ensure(top);
}

/* user32.c: at each input poll. Gives the page time to draw and to take input if YIELD_MS passed since the last time. */
void rt_web_yield(void)
{
    static double last;
    double now = emscripten_get_now();
    if (now - last < YIELD_MS) return;
    web_check_host_heap();
    emscripten_sleep(0);
    last = emscripten_get_now();
}

#endif
