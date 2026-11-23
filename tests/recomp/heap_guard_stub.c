/* For unit tests that link heap.c without rt.c: the test guard (RECOMP_GUARD, rt.c) that follows the heap's top
 * as it grows; a unit test has none. */
#include <stdint.h>
void rt_guard_heap_top(uint32_t top) { (void)top; }
