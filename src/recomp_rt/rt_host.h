/* Host side of the runtime: setup, the heap, host functions and the tables that the recompiler writes. */
#ifndef RECOMP_RT_HOST_H
#define RECOMP_RT_HOST_H

#include <stdint.h>

#include "rt.h"

/* The same layout as the Unicorn run of tests/recomp/reference.py. */
#define RT_STACK_BASE 0x20000000u
#define RT_STACK_SIZE 0x100000u
#define RT_STACK_TOP (RT_STACK_BASE + RT_STACK_SIZE - 0x1000u)
#define RT_RETURN_ADDR 0x40000000u
#define RT_HEAP_BASE 0x70000000u
#define RT_HEAP_SIZE 0x40000000u
/* The arena of the free-list heap: up to 0xEF000000, below the standard handles at 0xF0000000 (heap.c). The first
 * RT_HEAP_SIZE of it is the arena of 0.5.0. */
#define RT_HEAP_ARENA 0x7F000000u
/* Host functions (COM methods, GetProcAddress results) get addresses from here up; see rt_host_fn. */
#define RT_HOST_FN_BASE 0x63000000u

typedef struct { uint32_t iat, stub; } RtIat;
extern const RtIat rt_iat[];
extern const uint32_t rt_iat_size;

void rt_init(const char *exe_path);
uint32_t rt_image_base(void);
uint32_t rt_image_size(void);
extern const uint8_t *rt_exe_data;      /* the whole cm0102.exe file */
extern uint32_t rt_exe_size;

/* The heap (heap.c). With rt_heap_bump set it never reuses memory and rounds blocks to 16 bytes, as the
 * Unicorn run of the load test does. Otherwise freed blocks are reused. */
extern int rt_heap_bump;
uint32_t rt_alloc(uint32_t size);
uint32_t rt_alloc_zero(uint32_t size);
uint32_t rt_alloc_str(const char *s);
void rt_free(uint32_t addr);
uint32_t rt_realloc(uint32_t addr, uint32_t size, int in_place_only);
uint32_t rt_block_size(uint32_t addr);
void rt_set_block_size(uint32_t addr, uint32_t size);
int rt_is_block(uint32_t addr);
uint32_t rt_heap_top(void);
uint32_t rt_heap_free_bytes(void);     /* the free bytes below the top (heap.c) */
void rt_heap_free_parts(uint32_t *small_bytes, uint32_t *small_blocks, uint32_t *spans);
/* The arena in bytes (a test: the ctest of heap.c; the game reads RECOMP_HEAP_ARENA_MB). 0: RT_HEAP_ARENA. */
void rt_heap_set_arena(uint32_t bytes);
uint32_t rt_heap_arena_bytes(void);       /* the arena: RT_HEAP_ARENA, or RECOMP_HEAP_ARENA_MB */
/* The zone (heap.c): while rt_heap_zone returns 1, new blocks come from RT_ZONE_BASE up (the player search,
 * playersearch.c). NULL: no zone. rt_heap_zone_top: the top of the zone, for the test. */
#define RT_ZONE_BASE 0x50000000u
#define RT_ZONE_SIZE 0x10000000u
extern int (*rt_heap_zone)(void);
uint32_t rt_heap_zone_top(void);

/* A host function at a fake address that the original can call (stdcall: it pops its own arguments).
 * The same function under another name gets another address; rt_call_target tells them apart. */
extern uint32_t rt_call_target;
uint32_t rt_host_fn(const char *name, RtFn fn);
const char *rt_name_of(uint32_t target);
/* Calls the original's function at target with nargs 32-bit arguments; returns eax. */
uint32_t rt_callback(uint32_t target, int nargs, const uint32_t *args);
void rt_missing_method(const char *iface, uint32_t offset) __attribute__((noreturn));
/* Runs before a fatal stop exits, for example to save the screen. */
extern void (*rt_on_fatal)(void);

/* How often each replacement ran (tests). */
void rt_count(const char *name);
void rt_print_counts(void);

#endif
