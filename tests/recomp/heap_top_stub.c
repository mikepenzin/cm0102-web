/* For unit tests that link runtime files without heap.c: the heap top that rt_readable (rt.h) asks for, as the
 * ceiling the runtime used before it read the heap's own top (0xfff00000). */
#include <stdint.h>
uint32_t rt_heap_top(void) { return 0xfff00000u; }
