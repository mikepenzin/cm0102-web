/* ctest net_scale (0.7.0 item 212): every coordinate on the network is a classic one. ds_classic must undo
 * display.c's scale exactly, for left and top edges and for right and bottom edges, so that a widescreen host sends
 * the values of the original and a widescreen joiner gets its own screen back after it widens them. In classic mode
 * both are the identity. Zero and negative values keep their meaning. */
#include "display_scale.h"

#include <stdio.h>

int main(void)
{
    int bad = 0, n = 0;
    const uint32_t sizes[][2] = {{1280, 800}, {800, 600}};
    for (int axis = 0; axis < 2; axis++)
        for (int far = 0; far < 2; far++) {
            uint32_t screen = sizes[axis][0], classic = sizes[axis][1];
            for (int v = -3; v <= (int)classic; v++, n++) {
                uint32_t w = ds_screen((uint32_t)v, screen, classic, far);
                if (ds_classic(w, screen, classic, far) != (uint32_t)v) {
                    if (bad++ < 5) printf("FAIL  axis %d far %d: %d -> %d -> %d\n", axis, far, v, (int)w,
                                          (int)ds_classic(w, screen, classic, far));
                }
                if (ds_screen((uint32_t)v, classic, classic, far) != (uint32_t)v ||
                    ds_classic((uint32_t)v, classic, classic, far) != (uint32_t)v) {
                    if (bad++ < 5) printf("FAIL  classic mode changes %d\n", v);
                }
            }
            /* Every screen value maps to a classic value whose own screen value is at most 2 pixels away. */
            for (int w = 1; w < (int)screen; w++) {
                uint32_t c = ds_classic((uint32_t)w, screen, classic, far), back = ds_screen(c, screen, classic, far);
                int d = (int)back - w;
                if (d < -2 || d > 2) {
                    if (bad++ < 5) printf("FAIL  axis %d far %d: screen %d -> %d -> %d\n", axis, far, w, (int)c, (int)back);
                }
            }
        }
    /* The clamp of Area::init and Guio::init (799, 599 widened: 1279, 799) comes back as 799 and 599. */
    if (ds_classic(1279, 1280, 800, 1) != 799 || ds_classic(799, 800, 600, 1) != 599) {
        printf("FAIL  the far screen edge does not give 799 and 599\n");
        bad++;
    }
    printf("%s  net_scale: %d values, the inverse of the widescreen scale is exact\n", bad ? "FAIL" : "ok  ", n);
    return bad != 0;
}
