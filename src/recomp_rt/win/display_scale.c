/* The coordinate scale of widescreen mode and its inverse (display_scale.h). */
#include "display_scale.h"

uint32_t ds_screen(uint32_t v, uint32_t screen, uint32_t classic, int far_edge)
{
    int32_t x = (int32_t)v;
    if (screen == classic || x <= 0) return v;
    if (far_edge) return (uint32_t)((int64_t)(x + 1) * screen / classic - 1);
    return (uint32_t)((int64_t)x * screen / classic);
}

uint32_t ds_classic(uint32_t v, uint32_t screen, uint32_t classic, int far_edge)
{
    int64_t x = (int32_t)v;
    if (screen == classic || x <= 0) return v;
    if (far_edge) return (uint32_t)(((x + 1) * classic + screen - 1) / screen - 1);
    return (uint32_t)((x * classic + screen - 1) / screen);
}
