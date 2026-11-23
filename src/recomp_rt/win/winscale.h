/* The scaling of the picture to the window (user32.c, 0.6.0 picture fill, spec
 * docs/specs/2026-10-21-picture-fill-spec.md).
 *
 * The picture fills the window at any size, up or down, and keeps its shape. Bars show only where the
 * shape of the window differs from the shape of the picture.
 *
 * With the option sharp_picture ("sharp bilinear"), the game first draws its picture with nearest pixels
 * into a work texture of a whole number of times its size: the smallest whole number that is at least the
 * fit to the window. The work texture then goes to the window with linear filtering. So each letter keeps
 * an even shape, and only the edges of the pixels are soft. At a fit of 1 or less, no work texture is
 * needed: the picture goes to the window with linear filtering. Without the option, the picture always
 * goes to the window with linear filtering (a smooth, softer picture). */
#ifndef WINSCALE_H
#define WINSCALE_H

/* The fit of a pic_w x pic_h picture to an out_w x out_h window (real pixels): the scale that keeps the
 * shape and fills the window in one direction. */
static inline double winscale_fit(int out_w, int out_h, int pic_w, int pic_h)
{
    if (out_w <= 0 || out_h <= 0 || pic_w <= 0 || pic_h <= 0) return 1;
    double fit = (double)out_w / pic_w;
    if ((double)out_h / pic_h < fit) fit = (double)out_h / pic_h;
    return fit;
}

/* The whole number for the work texture: 1 means no work texture. max_w x max_h: the largest texture of
 * the renderer (0: no limit). A fit just above a whole number, for example 2.0001, counts as that number. */
static inline int winscale_work(int out_w, int out_h, int pic_w, int pic_h, int max_w, int max_h)
{
    double fit = winscale_fit(out_w, out_h, pic_w, pic_h);
    int k = (int)fit;
    if (fit - k > 0.001) k++;
    if (k < 1) k = 1;
    while (k > 1 && ((max_w > 0 && pic_w * k > max_w) || (max_h > 0 && pic_h * k > max_h))) k--;
    return k;
}

/* Full screen with a change of the screen to 800x600, as the original does: only in classic mode, and only
 * when the screen lists an 800x600 mode. Otherwise full screen keeps the desktop size. */
static inline int winscale_mode_change(int pic_w, int pic_h, int has_800x600)
{
    return pic_w == 800 && pic_h == 600 && has_800x600;
}

/* Full screen at the start of the game. With the option remember_window and a stored window, the state of the
 * last quit (window.txt) decides: it follows the Screen Mode row after every change of the row, and it also
 * holds a change with the F11 key, which the file game.cfg does not. Else the row (pref_full) decides, but only
 * when the preferences come from game.cfg (prefs_saved). The defaults of the original (0x5d7bc0) are Full, and
 * a first start, which has no game.cfg, opens in a window as the port always did. */
static inline int winscale_start_full(int pref_full, int prefs_saved, int remember_on, int remembered_known,
                                      int remembered_full)
{
    if (remember_on && remembered_known) return remembered_full != 0;
    return prefs_saved && pref_full != 0;
}

/* The hint for widescreen on a short screen: the picture is 1280x800 and the screen is less than 800 real
 * pixels high. */
#define WINSCALE_SHORT_SCREEN 800
static inline int winscale_short_screen(int pic_w, int screen_h)
{
    return pic_w > 800 && screen_h > 0 && screen_h < WINSCALE_SHORT_SCREEN;
}

#endif
