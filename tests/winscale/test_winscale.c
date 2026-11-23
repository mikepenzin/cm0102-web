/* The scaling of the picture to the window (winscale.h, 0.6.0 picture fill). */
#include "winscale.h"

#include <stdio.h>

static int failed;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failed = 1; } } while (0)

int main(void)
{
    /* The fit keeps the shape: the smaller of the two scales. */
    CHECK(winscale_fit(1280, 800, 1280, 800) == 1);
    CHECK(winscale_fit(1366, 705, 800, 600) == 705.0 / 600);       /* classic maximised on 1366x768 */
    CHECK(winscale_fit(1110, 694, 1280, 800) < 0.87);               /* widescreen fitted on 1366x768 */

    /* The work texture: the smallest whole number at least the fit. */
    CHECK(winscale_work(1110, 694, 1280, 800, 0, 0) == 1);          /* smaller than the picture: none */
    CHECK(winscale_work(1280, 800, 1280, 800, 0, 0) == 1);          /* 1 time: none */
    CHECK(winscale_work(1281, 801, 1280, 800, 0, 0) == 1);          /* a 1-pixel round-up at 150 %: none */
    CHECK(winscale_work(1366, 705, 800, 600, 0, 0) == 2);           /* 1.175 times: 2, then down */
    CHECK(winscale_work(1366, 768, 800, 600, 0, 0) == 2);           /* classic full screen, 1.28 times */
    CHECK(winscale_work(1920, 1009, 1280, 800, 0, 0) == 2);         /* 1080p at 125 %, maximised */
    CHECK(winscale_work(2560, 1600, 1280, 800, 0, 0) == 2);         /* 2 times exactly */
    CHECK(winscale_work(3024, 1890, 1280, 800, 0, 0) == 3);         /* 2.36 times */
    CHECK(winscale_work(3840, 2160, 800, 600, 0, 0) == 4);          /* classic on 4K: 3.6 times */
    CHECK(winscale_work(3840, 2160, 800, 600, 2048, 2048) == 2);    /* a small texture limit */
    CHECK(winscale_work(0, 0, 1280, 800, 0, 0) == 1);

    /* The 800x600 full screen: classic only, and only when the screen lists the mode. */
    CHECK(winscale_mode_change(800, 600, 1));
    CHECK(!winscale_mode_change(800, 600, 0));                     /* a Mac: no 800x600 mode */
    CHECK(!winscale_mode_change(1280, 800, 1));                    /* widescreen: the desktop size */

    /* The widescreen hint: a screen less than 800 real pixels high. */
    CHECK(winscale_short_screen(1280, 768));
    CHECK(winscale_short_screen(1280, 720));
    CHECK(!winscale_short_screen(1280, 800));
    CHECK(!winscale_short_screen(1280, 1080));
    CHECK(!winscale_short_screen(800, 768));                       /* classic: no hint */
    CHECK(!winscale_short_screen(1280, 0));                        /* height unknown */
    /* The Screen Mode at start: the row decides when game.cfg holds it; a remembered window decides when stored. */
    CHECK(winscale_start_full(1, 1, 0, 0, 0));                     /* Full in game.cfg */
    CHECK(!winscale_start_full(0, 1, 0, 0, 0));                    /* Windowed in game.cfg */
    CHECK(!winscale_start_full(1, 0, 0, 0, 0));                    /* first start: the default Full is not used */
    CHECK(winscale_start_full(1, 1, 1, 0, 0));                     /* option on, no stored window: the row */
    CHECK(winscale_start_full(1, 1, 0, 1, 0));                     /* option off: the row, not the file */
    CHECK(!winscale_start_full(1, 1, 1, 1, 0));                    /* F11 left full screen at the last quit */
    CHECK(winscale_start_full(0, 1, 1, 1, 1));                     /* F11 entered full screen at the last quit */
    if (!failed) printf("winscale: all checks passed\n");
    return failed;
}
