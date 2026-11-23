#ifndef RECOMP_DISPLAY_SCALE_H
#define RECOMP_DISPLAY_SCALE_H

#include <stdint.h>

/* The coordinate scale of widescreen mode (display.c), with no game memory, so ctest net_scale tests it alone.
 * ds_screen: a coordinate for 800x600 on a screen `screen` pixels across (or down). A left or top edge maps to the
 * first pixel of its part of the screen and a right or bottom edge (far_edge) to the last, so that areas that touch
 * still touch. Zero and negative values keep their meaning ("none" or "from the owner").
 * ds_classic: the exact inverse (item 212): ds_classic(ds_screen(v)) == v for every v, because the screen is
 * larger than 800x600 (each step is more than one pixel). A value that ds_screen did not make (a width that comes
 * from text in screen pixels) rounds up to a classic value whose own screen value is at most 2 pixels away. */
uint32_t ds_screen(uint32_t v, uint32_t screen, uint32_t classic, int far_edge);
uint32_t ds_classic(uint32_t v, uint32_t screen, uint32_t classic, int far_edge);

#endif
