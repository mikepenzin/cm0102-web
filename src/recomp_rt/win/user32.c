/* USER32 over SDL: the window, the message queue fed from SDL events, the window procedure call
 * (DispatchMessageA calls the original's 0x68d630), cursors and the message box. Also the screen:
 * the SDL window shows the primary DirectDraw surface (ddraw.c) as an RGB565 texture of the screen size
 * (rt_screen_w x rt_screen_h). The picture keeps its shape in a window of any size.
 *
 * For runs without a person: RECOMP_SCRIPT holds steps "<ms> <action> [args]" separated by ';', with
 * the time since the window opened. Actions: move x y, down x y, up x y, click x y (down, then up 100 ms
 * later), rclick x y, key <virtual key>, char <code>, type <text> (one character each 100 ms),
 * press <key> (an SDL key press, as from the keyboard, for the keys of the options: for example
 * space, escape, gui+[, ctrl+[, gui+f, f11, or mouseback for the back button of a mouse),
 * resize w h (the window size), dump <file.bmp>, shot <file.bmp> (the window as shown, after the
 * scaling), quit. RECOMP_DUMP names a file to save the screen to when the program ends.
 *
 * The keys of the options (options.h): Space presses Continue Game (space_to_continue). Cmd+[ on a
 * Mac, Ctrl+[ on Windows and the back button of a mouse press the Back arrow, Escape closes an open
 * popup, and Cmd+F on a Mac or F11 on Windows switch full screen on and off (shortcuts). A key
 * that presses something on the screen of the game becomes a click of the left button there
 * (click_for_key), so the game's own input code runs, as for a click with the mouse. */
#include "win.h"
#include "host.h"
#include "options.h"
#include "winfit.h"
#include "winscale.h"

#include <SDL.h>
#include <stdlib.h>
#include <strings.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
void rt_web_yield(void);                       /* host/web.c */
/* The browser: an idle sleep hands the time to the page, so it draws and takes input (host/web.c). */
#define SDL_Delay(ms) emscripten_sleep(ms)
#endif

#define WM_KEYDOWN 0x100u
#define WM_KEYUP 0x101u
#define WM_CHAR 0x102u
#define WM_MOUSEMOVE 0x200u
#define WM_LBUTTONDOWN 0x201u
#define WM_LBUTTONUP 0x202u
#define WM_RBUTTONDOWN 0x204u
#define WM_RBUTTONUP 0x205u
#define WM_MOUSEWHEEL 0x20Au
#define WM_QUIT 0x12u

uint32_t win_wndproc;
uint32_t win_hwnd = 0x00010010u;
#define DESKTOP_HWND 0x00010001u

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
SDL_Window *win_sdl_window(void) { return window; }
SDL_Renderer *win_sdl_renderer(void) { return renderer; }
SDL_Texture *win_sdl_texture(void) { return texture; }
static uint64_t opened_at, last_present, last_poll;
extern int win_screen_dirty;
uint32_t win_primary_pixels(void);

/* The message queue. */
typedef struct { uint32_t msg, wparam, lparam, time; } Msg;
#define QUEUE 1024
static Msg queue[QUEUE];
static uint32_t q_head, q_tail;
static int mouse_x, mouse_y, buttons;
static uint64_t last_input_at;                     /* the time of the last message, for idle_rest */

static void post(uint32_t msg, uint32_t wparam, uint32_t lparam)
{
    if (q_tail - q_head >= QUEUE) return;          /* full: drop, as a busy Windows queue would not */
    Msg *m = &queue[q_tail++ % QUEUE];
    m->msg = msg;
    m->wparam = wparam;
    m->lparam = lparam;
    m->time = (uint32_t)SDL_GetTicks();
    last_input_at = SDL_GetTicks64();
}

static uint32_t mouse_lparam(void) { return ((uint32_t)mouse_y & 0xffff) << 16 | ((uint32_t)mouse_x & 0xffff); }

/* The press that the left button is in: a new number at each WM_LBUTTONDOWN, and its time. */
static uint32_t press_count;
uint32_t win_press_count(void) { return press_count; }
static uint64_t press_at;

static void post_mouse(uint32_t msg, int x, int y)
{
    mouse_x = x;
    mouse_y = y;
    if (msg == WM_LBUTTONDOWN) { press_count++; press_at = SDL_GetTicks64(); }
    post(msg, (uint32_t)buttons, mouse_lparam());
}

/* The scaling of the picture to the window (winscale.h). SDL_RenderSetLogicalSize keeps the shape, with
 * bars at the sides or at the top and bottom only where the shape of the window differs, and maps the
 * mouse back to the picture. The picture fills the window at any size, up or down. With the option
 * sharp_picture, a fit above 1 goes through the work texture: the picture is drawn into it with nearest
 * pixels at a whole number of times its size (work_k), and the work texture goes to the window with
 * linear filtering. Otherwise the picture goes to the window with linear filtering. */
static SDL_Texture *work;
static int work_k = 1, work_failed_k;             /* work_failed_k: a size that SDL could not make */

static void update_scaling(void)
{
    if (!renderer || !texture) return;
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
    int ow = 0, oh = 0, k = 1;
    SDL_RendererInfo ri;
    if (SDL_GetRendererInfo(renderer, &ri)) ri.max_texture_width = ri.max_texture_height = 0;
    if (options_bool(OPT_SHARP_PICTURE) && !SDL_GetRendererOutputSize(renderer, &ow, &oh))
        k = winscale_work(ow, oh, (int)rt_screen_w, (int)rt_screen_h, ri.max_texture_width, ri.max_texture_height);
    if (k != work_k || (k > 1 && !work)) {
        if (work) SDL_DestroyTexture(work);
        work = NULL;
        if (k > 1 && k != work_failed_k) {
            work = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                     (int)rt_screen_w * k, (int)rt_screen_h * k);
            if (!work) {
                fprintf(stderr, "recomp: work texture %dx: %s\n", k, SDL_GetError());
                work_failed_k = k;
            }
            else SDL_SetTextureScaleMode(work, SDL_ScaleModeLinear);
        }
        work_k = work ? k : 1;
        if (k == 1) work_failed_k = 0;
        win_screen_dirty = 1;
    }
    SDL_SetTextureScaleMode(texture, work ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
}

/* The way the picture is scaled now, for the display report. */
void win_picture_way(char *out, size_t n)
{
    if (work) snprintf(out, n, "sharp: %d times with nearest pixels, then linear", work_k);
    else snprintf(out, n, "linear");
}

/* The option sharp_picture changed in the options box (options_screen.c): at once. */
void win_sharp_picture_changed(void)
{
    update_scaling();
    win_screen_dirty = 1;
}

/* The usable part of the window's screen, less the window's borders. 0 when SDL does not know it. When the
 * title bar size is unknown (SDL gives it on Windows and Linux), 32 rows are kept for it. */
static int usable_area(SDL_Rect *r, int *left, int *top, int *right, int *bottom)
{
    int d = SDL_GetWindowDisplayIndex(window);
    if (d < 0 || SDL_GetDisplayUsableBounds(d, r)) return 0;
    *top = *left = *bottom = *right = 0;
    if (SDL_GetWindowBordersSize(window, top, left, bottom, right) || *top + *bottom == 0) *top = 32;
    return 1;
}

/* The size of a new window, which is also the smallest size (winfit.h): the picture's size, in real pixels
 * on Windows (with display scaling at 125 % or 150 % the window then shows it at scale 1), in points on a
 * Mac and on Linux. Never larger than the usable part of the screen, with the same shape. */
static void picture_window_size(int *out_w, int *out_h)
{
    int w = (int)rt_screen_w, h = (int)rt_screen_h;
#ifdef _WIN32
    int pw = 0, ph = 0, lw = 0, lh = 0;
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    SDL_GetWindowSize(window, &lw, &lh);
    if (pw > lw && lw > 0 && lh > 0) {
        w = winfit_points(w, pw, lw);
        h = winfit_points(h, ph, lh);
    }
#endif
    *out_w = w;
    *out_h = h;
    SDL_Rect r;
    int left, top, right, bottom;
    if (usable_area(&r, &left, &top, &right, &bottom))
        winfit_size(w, h, r.w - left - right, r.h - top - bottom, out_w, out_h);
}

/* The smallest window (Max, 2026-10-21): the size of a new window. A smaller window, for example a
 * remembered one, grows to it. Set again when the window moves to another screen. */
static void set_smallest_window(void)
{
    int w, h;
    picture_window_size(&w, &h);
    int cw = 0, ch = 0;
    SDL_GetWindowMinimumSize(window, &cw, &ch);
    if (cw == w && ch == h) return;
    SDL_SetWindowMinimumSize(window, w, h);
    fprintf(stderr, "recomp: smallest window %dx%d\n", w, h);
}

/* 1 when the window's screen lists an 800x600 mode. */
static int screen_has_800x600(SDL_DisplayMode *mode)
{
    int d = SDL_GetWindowDisplayIndex(window);
    for (int i = 0, n = d < 0 ? 0 : SDL_GetNumDisplayModes(d); i < n; i++) {
        SDL_DisplayMode m;
        if (SDL_GetDisplayMode(d, i, &m) || m.w != 800 || m.h != 600) continue;
        if (getenv("RECOMP_NO_MODE_CHANGE")) return 0;    /* for tests: do not change the screen of the person */
        *mode = m;
        return 1;
    }
    return 0;
}

/* Full screen on: in classic mode a change of the screen to 800x600 when the screen lists that mode, as
 * the original does; the monitor or the graphics chip then scales it. Otherwise the desktop size, with the
 * picture filled as in a window. Returns the SDL result. */
static int full_screen_on(void)
{
    SDL_DisplayMode m;
    if (winscale_mode_change((int)rt_screen_w, (int)rt_screen_h, screen_has_800x600(&m))) {
        if (!SDL_SetWindowDisplayMode(window, &m) && !SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN)) {
            fprintf(stderr, "recomp: full screen 800x600 mode\n");
            return 0;
        }
        fprintf(stderr, "recomp: full screen 800x600 mode: %s\n", SDL_GetError());
    }
    return SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

static void toggle_full_screen(void)
{
    if (!window) return;
    int full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    if (full ? SDL_SetWindowFullscreen(window, 0) : full_screen_on())
        fprintf(stderr, "recomp: full screen: %s\n", SDL_GetError());
    update_scaling();
    win_screen_dirty = 1;
    display_report_event(full ? "full screen off" : "full screen on");
}

/* Remember the window (option remember_window): window.txt in the user folder, beside settings.txt,
 * holds "x y w h full". The size and place are those of the window, not of full screen: the last
 * windowed values are kept while the window is full screen. */
#define FALLBACK_W 1280
#define FALLBACK_H 800
static int win_x, win_y, win_w, win_h;          /* the last windowed geometry */

static void note_windowed(void)
{
    if (!window || (SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))) return;
    SDL_GetWindowPosition(window, &win_x, &win_y);
    SDL_GetWindowSize(window, &win_w, &win_h);
}

static void window_file(char *path, size_t n)
{
    snprintf(path, n, "%s/window.txt", win_user_dir);
}

void win_save_window(void)
{
    char report[1100];
    if (window) display_report_write("at quit", report, sizeof report, NULL);
    if (!window || !options_bool(OPT_REMEMBER_WINDOW)) return;
    int full = (SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
    note_windowed();
    char path[1100];
    window_file(path, sizeof path);
    FILE *f = host_fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d %d\n", win_x, win_y, win_w, win_h, full);
    fclose(f);
    fprintf(stderr, "recomp: window saved %d %d %d %d %d\n", win_x, win_y, win_w, win_h, full);
}

/* 1 when the stored window shows on a connected screen: its middle lies inside one. */
static int window_on_screen(int x, int y, int w, int h)
{
    if (w < 200 || h < 150) return 0;
    int cx = x + w / 2, cy = y + h / 2;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        SDL_Rect r;
        if (SDL_GetDisplayBounds(i, &r)) continue;
        if (cx >= r.x && cx < r.x + r.w && cy >= r.y && cy < r.y + r.h) return 1;
    }
    return 0;
}

/* A new window (not a remembered one) gets the size of picture_window_size: for example a 1280x800 window
 * on a small laptop screen gets smaller with the same shape and goes to the middle. The window opens
 * hidden and shows after this. */
static void fit_new_window(int made_w, int made_h)
{
    int fw, fh;
    picture_window_size(&fw, &fh);
    if (fw != made_w || fh != made_h) {
        SDL_SetWindowSize(window, fw, fh);
        SDL_Rect r;
        int left, top, right, bottom;
        if (usable_area(&r, &left, &top, &right, &bottom))
            SDL_SetWindowPosition(window, r.x + left + (r.w - left - right - fw) / 2,
                                  r.y + top + (r.h - top - bottom - fh) / 2);
        fprintf(stderr, "recomp: window fitted to %dx%d\n", fw, fh);
    }
    SDL_ShowWindow(window);
}

/* The widescreen hint (Max, 2026-10-21): on a screen less than 800 real pixels high, widescreen shows
 * smaller than its own size and classic suits the screen better. The height is that of the desktop mode
 * of the window's screen (real pixels on Windows, points on a Mac). RECOMP_TEST_SCREEN_H gives a height
 * for tests. */
int win_short_screen(void)
{
    const char *test = getenv("RECOMP_TEST_SCREEN_H");
    const char *driver = SDL_GetCurrentVideoDriver();
    if (!test && driver && !strcmp(driver, "dummy")) return 0;   /* the test runs: no screen, no hint */
    int h = test ? atoi(test) : 0;
    SDL_DisplayMode m;
    int d = window ? SDL_GetWindowDisplayIndex(window) : -1;
    if (!test && d >= 0 && !SDL_GetDesktopDisplayMode(d, &m)) h = m.h;
    return winscale_short_screen((int)rt_screen_w, h);
}

/* The hint box shows once: hints.txt in the user folder, beside window.txt, keeps that it showed. It is
 * not an option, so it is not in settings.txt. */
static void short_screen_hint(void)
{
    if (!win_short_screen()) return;
    char path[1100];
    snprintf(path, sizeof path, "%s/hints.txt", win_user_dir);
    FILE *f = host_fopen(path, "r");
    char line[64] = "";
    if (f) {
        if (!fgets(line, sizeof line, f)) line[0] = 0;
        fclose(f);
    }
    if (!strncmp(line, "short_screen_hint = shown", 25)) return;
    fprintf(stderr, "recomp: short screen hint\n");
    f = host_fopen(path, "w");
    if (f) {
        fprintf(f, "short_screen_hint = shown\n");
        fclose(f);
    }
    if (!getenv("RECOMP_NO_BOX"))
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Screen size",
                                 "Your screen is less than 800 pixels high. Classic mode fits it better. "
                                 "Choose it in Game Options, Display mode, then start the game again.", window);
}

/* The window. CM0102_VERSION comes from the CMake project version. */
#ifndef CM0102_VERSION
#define CM0102_VERSION "dev"
#endif
void win_video_init(void)
{
    if (window) return;
    int x = SDL_WINDOWPOS_CENTERED, y = SDL_WINDOWPOS_CENTERED, w = (int)rt_screen_w, h = (int)rt_screen_h, full = 0;
    int remembered = 0;
    if (options_bool(OPT_REMEMBER_WINDOW)) {
        char path[1100];
        window_file(path, sizeof path);
        FILE *f = host_fopen(path, "r");
        int sx, sy, sw, sh, sf;
        int ok = f && fscanf(f, "%d %d %d %d %d", &sx, &sy, &sw, &sh, &sf) == 5;
        if (f) fclose(f);
        if (ok) {
            if (window_on_screen(sx, sy, sw, sh)) { x = sx; y = sy; w = sw; h = sh; full = sf != 0; remembered = 1; }
            else { w = FALLBACK_W; h = FALLBACK_H; }          /* off every screen: 1280x800 in the middle */
        }
    }
    window = SDL_CreateWindow("CM 01/02: Bosman Edition " CM0102_VERSION, x, y, w, h,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (remembered ? 0 : SDL_WINDOW_HIDDEN));
    if (!window) rt_fatal("SDL_CreateWindow: %s", SDL_GetError());
    if (!remembered) fit_new_window(w, h);
    set_smallest_window();
    note_windowed();
    if (full && full_screen_on()) fprintf(stderr, "recomp: full screen: %s\n", SDL_GetError());
    fprintf(stderr, "recomp: window opened %d %d %d %d %d\n", win_x, win_y, win_w, win_h, full);
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) rt_fatal("SDL_CreateRenderer: %s", SDL_GetError());
    SDL_RenderSetLogicalSize(renderer, (int)rt_screen_w, (int)rt_screen_h);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                (int)rt_screen_w, (int)rt_screen_h);
    if (!texture) rt_fatal("SDL_CreateTexture: %s", SDL_GetError());
    update_scaling();
    SDL_StartTextInput();
    opened_at = SDL_GetTicks64();
    display_report_event("window opened");
    char report[1100];
    display_report_write("at start", report, sizeof report, NULL);
    short_screen_hint();
}

/* The frame into the window's back buffer, without the present. */
static int draw_frame(void)
{
    uint32_t px = win_primary_pixels();
    if (!renderer || !px) return 0;
    SDL_UpdateTexture(texture, NULL, g_mem + px, (int)rt_screen_w * 2);
    if (work && !SDL_SetRenderTarget(renderer, work)) {
        /* The target has the size of the work texture and scale 1; SDL gives back the window's view after. */
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderSetScale(renderer, (float)work_k, (float)work_k);
        names_draw(renderer);
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, work, NULL, NULL);
    } else {
        if (work) SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);   /* the target failed: no uneven letters */
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        names_draw(renderer);
    }
    return 1;
}

static void present_now(void)
{
    if (!draw_frame()) return;
    SDL_RenderPresent(renderer);
    win_screen_dirty = 0;
    last_present = SDL_GetTicks64();
}

/* At most about 60 times a second; the rest waits for the next poll. */
void win_present(void)
{
    win_screen_dirty = 1;
    if (SDL_GetTicks64() - last_present >= 16) present_now();
}

void win_dump_screen(const char *path)
{
    uint32_t px = win_primary_pixels();
    if (!px) { fprintf(stderr, "recomp: no screen to save to %s\n", path); return; }
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(g_mem + px, (int)rt_screen_w, (int)rt_screen_h, 16,
                                                        (int)rt_screen_w * 2, SDL_PIXELFORMAT_RGB565);
    SDL_Surface *rgb = s ? SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGB24, 0) : NULL;
    names_draw_surface(rgb);
    if (!rgb || SDL_SaveBMP(rgb, path)) fprintf(stderr, "recomp: cannot save %s: %s\n", path, SDL_GetError());
    else fprintf(stderr, "recomp: screen saved to %s\n", path);
    SDL_FreeSurface(rgb);
    SDL_FreeSurface(s);
}

/* The window as shown, after the scaling, to a BMP file. The read comes before the present: after it, the
 * back buffer can be empty (Metal on a Mac). */
static void shot_window(const char *path)
{
    int w = 0, h = 0;
    if (!renderer || SDL_GetRendererOutputSize(renderer, &w, &h) || !draw_frame()) return;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
    if (!s) return;
    /* The read covers the whole output, with the bars: the logical size is off for it. */
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    int bad = SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGB24, s->pixels, s->pitch);
    SDL_RenderSetLogicalSize(renderer, (int)rt_screen_w, (int)rt_screen_h);
    if (bad || SDL_SaveBMP(s, path)) fprintf(stderr, "recomp: cannot save %s: %s\n", path, SDL_GetError());
    else fprintf(stderr, "recomp: window (%dx%d) saved to %s\n", w, h, path);
    SDL_FreeSurface(s);
    present_now();
}

/* A click of the left button for a key (click_for_key). The key posts a move to the place and a
 * button down. The button goes up after the input poll 0x55f620 has run once with the down in the
 * game (or after CLICK_LONGEST ms), then the pointer goes back to the place of the mouse. So the
 * game sees one click, as from the mouse, and the repeat pacing of input_poll below gives it one
 * result. While the click runs, moves of the mouse wait. */
#define CLICK_LONGEST 500u
static struct {
    int on, x, y, real_x, real_y;
    uint32_t down_at;                     /* queue count after the down: it is in the game once q_head passes it */
    int polls;                            /* input polls since the down went into the game */
    uint64_t at;
} key_click;

/* The same rule for a click of RECOMP_SCRIPT (test runs only): its up step waits until the input poll has run
 * once with the down in the game, or SCRIPT_CLICK_LONGEST ms. Under a slow emulation (the Windows build under
 * wine in an emulated container) one frame took longer than the 100 ms from the down to the up, the game took
 * both in one poll and the button did not react. On a normal machine the poll runs within the 100 ms, so the
 * up keeps its time. */
#define SCRIPT_CLICK_LONGEST 10000u
static struct {
    int on;
    uint32_t down_at;
    int polls;
    uint64_t at;
} script_click;

static int click_for_key(int x, int y)
{
    if (key_click.on || (buttons & 1)) return 0;
    key_click.on = 1;
    key_click.x = x;
    key_click.y = y;
    key_click.real_x = mouse_x;
    key_click.real_y = mouse_y;
    key_click.polls = 0;
    key_click.at = SDL_GetTicks64();
    post_mouse(WM_MOUSEMOVE, x, y);
    buttons |= 1;
    post_mouse(WM_LBUTTONDOWN, x, y);
    key_click.down_at = q_tail;
    return 1;
}

static void end_click_for_key(void)
{
    if (!key_click.on) return;
    if (key_click.polls < 1 && SDL_GetTicks64() - key_click.at < CLICK_LONGEST) return;
    key_click.on = 0;
    buttons &= ~1;
    post_mouse(WM_LBUTTONUP, key_click.x, key_click.y);
    post_mouse(WM_MOUSEMOVE, key_click.real_x, key_click.real_y);
}

/* The keys of the options. Returns 1 when the key did its work; the game then does not get it, nor its repeats
 * and release. Returns 2 for the display report key: the game does not get the press, and nothing is swallowed. */
static int shortcut_key(SDL_Keycode k, Uint16 mod)
{
    int x, y;
#ifdef __APPLE__
    int windows = getenv("RECOMP_KEYS_WINDOWS") != NULL; /* for the script test of the Windows keys */
#else
    int windows = 1;
#endif
    int back, full;
    if (windows) {
        back = k == SDLK_AC_BACK || (k == SDLK_LEFTBRACKET && (mod & KMOD_CTRL));
        full = k == SDLK_F11 && !(mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI | KMOD_SHIFT));
    } else {
        back = k == SDLK_AC_BACK || (k == SDLK_LEFTBRACKET && (mod & KMOD_GUI));
        full = k == SDLK_f && (mod & KMOD_GUI);
    }
    int plain = !(mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI | KMOD_SHIFT));
    /* The display report key works with the option shortcuts off too: a player with a broken picture needs it. */
    if (k == SDLK_d && (mod & KMOD_SHIFT) && (mod & (windows ? KMOD_CTRL : KMOD_GUI)) && !(mod & KMOD_ALT)) {
        fprintf(stderr, "recomp: key display report\n");
        display_report_key();
        return 2;    /* not swallowed: the release of D may go to the message box, and a kept swallow would lose the next D */
    }
    if (k == SDLK_SPACE && plain && options_bool(OPT_SPACE_TO_CONTINUE))
        return keys_continue_game(&x, &y) && click_for_key(x, y);
    if (!options_bool(OPT_SHORTCUTS)) return 0;
    if (full) { fprintf(stderr, "recomp: key full screen\n"); toggle_full_screen(); return 1; }
    if (back) { fprintf(stderr, "recomp: key back\n"); if (keys_back(&x, &y)) click_for_key(x, y); return 1; }
    if (k == SDLK_ESCAPE && plain) return keys_outside_popup(&x, &y) && click_for_key(x, y);
    return 0;
}

/* Windows virtual keys of SDL keys. */
static uint32_t virtual_key(SDL_Keycode k)
{
    if (k >= SDLK_a && k <= SDLK_z) return 'A' + (uint32_t)(k - SDLK_a);
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (uint32_t)(k - SDLK_0);
    if (k >= SDLK_F1 && k <= SDLK_F12) return 0x70 + (uint32_t)(k - SDLK_F1);
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return 0x61 + (uint32_t)(k - SDLK_KP_1);
    switch (k) {
    case SDLK_BACKSPACE: return 0x08;
    case SDLK_TAB: return 0x09;
    case SDLK_RETURN: case SDLK_KP_ENTER: return 0x0D;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
    case SDLK_LCTRL: case SDLK_RCTRL: return 0x11;
    case SDLK_LALT: case SDLK_RALT: return 0x12;
    case SDLK_PAUSE: return 0x13;
    case SDLK_CAPSLOCK: return 0x14;
    case SDLK_ESCAPE: return 0x1B;
    case SDLK_SPACE: return 0x20;
    case SDLK_PAGEUP: return 0x21;
    case SDLK_PAGEDOWN: return 0x22;
    case SDLK_END: return 0x23;
    case SDLK_HOME: return 0x24;
    case SDLK_LEFT: return 0x25;
    case SDLK_UP: return 0x26;
    case SDLK_RIGHT: return 0x27;
    case SDLK_DOWN: return 0x28;
    case SDLK_INSERT: return 0x2D;
    case SDLK_DELETE: return 0x2E;
    case SDLK_KP_0: return 0x60;
    case SDLK_KP_MULTIPLY: return 0x6A;
    case SDLK_KP_PLUS: return 0x6B;
    case SDLK_KP_MINUS: return 0x6D;
    case SDLK_KP_PERIOD: return 0x6E;
    case SDLK_KP_DIVIDE: return 0x6F;
    case SDLK_SEMICOLON: return 0xBA;
    case SDLK_EQUALS: return 0xBB;
    case SDLK_COMMA: return 0xBC;
    case SDLK_MINUS: return 0xBD;
    case SDLK_PERIOD: return 0xBE;
    case SDLK_SLASH: return 0xBF;
    case SDLK_BACKQUOTE: return 0xC0;
    case SDLK_LEFTBRACKET: return 0xDB;
    case SDLK_BACKSLASH: return 0xDC;
    case SDLK_RIGHTBRACKET: return 0xDD;
    case SDLK_QUOTE: return 0xDE;
    default: return 0;
    }
}

/* The script of a run without a person. A wait step ends a part of the script. The steps of each part
 * run in time order, with their times from the start of the part: the window opening for the first
 * part, the moment its wait was reached for each later part. A step may leave out its time (0). */
#define WAIT_DEFAULT_MS 120000
typedef struct {
    uint64_t at;
    char action[16];
    int a, b;
    char text[512];
    int part;                           /* the number of waits before this step */
    int kind;                           /* wait: the state (waits.c) */
    uint32_t value;
} Step;
static Step *steps;
static int step_count, step_next, script_loaded;
static uint64_t part_start;             /* ms since the window opened at the start of the current part */
static int waiting;                     /* 1 while the wait step steps[step_next] runs */
static uint64_t wait_began;
static uint32_t wait_polls;

static void load_script(void)
{
    script_loaded = 1;
    const char *s = getenv("RECOMP_SCRIPT");
    if (!s) return;
    char *copy = strdup(s), *save = NULL;
    int part = 0;
    for (char *item = strtok_r(copy, ";", &save); item; item = strtok_r(NULL, ";", &save)) {
        Step st;
        memset(&st, 0, sizeof st);
        unsigned long long at = 0;
        char *rest = item;
        while (*rest == ' ' || *rest == '\t' || *rest == '\n') rest++;
        if (*rest >= '0' && *rest <= '9') at = strtoull(rest, &rest, 10);
        if (sscanf(rest, " %15s", st.action) < 1) continue;
        st.at = at;
        st.part = part;
        if (!strcmp(st.action, "dump") || !strcmp(st.action, "type") || !strcmp(st.action, "press") ||
            !strcmp(st.action, "shot") || !strcmp(st.action, "wait"))
            sscanf(rest, " %*s %511s %d", st.text, &st.a);
        else sscanf(rest, " %*s %d %d", &st.a, &st.b);
        if (!strcmp(st.action, "wait")) {
            if (!waits_parse(st.text, &st.kind, &st.value)) {
                fprintf(stderr, "recomp: script: unknown state in \"wait %s\"\n", st.text);
                exit(2);
            }
            if (st.a <= 0) st.a = WAIT_DEFAULT_MS;
            part++;
        }
        if (!strcmp(st.action, "type")) {
            /* One character every 100 ms: the window procedure keeps only the last WM_CHAR. */
            size_t n = strlen(st.text);
            steps = realloc(steps, sizeof *steps * (size_t)(step_count + (int)n + 2));
            for (size_t i = 0; i < n; i++) {
                Step c = st;
                c.at = st.at + 100 * i;
                snprintf(c.action, sizeof c.action, "char");
                c.a = (unsigned char)st.text[i];
                c.text[0] = 0;
                steps[step_count++] = c;
            }
            continue;
        }
        steps = realloc(steps, sizeof *steps * (size_t)(step_count + 2));
        steps[step_count++] = st;
        if (!strcmp(st.action, "click") || !strcmp(st.action, "rclick")) {
            Step up = st;
            up.at += 100;
            snprintf(up.action, sizeof up.action, "%s", st.action[0] == 'r' ? "rup" : "up");
            steps[step_count++] = up;
        }
    }
    free(copy);
    /* In parts, each in time order with its wait last, keeping the order of equal times. */
    for (int i = 1; i < step_count; i++) {
        Step s = steps[i];
        int j = i;
        while (j > 0) {
            Step *p = &steps[j - 1];
            int pw = !strcmp(p->action, "wait"), sw = !strcmp(s.action, "wait");
            int after = p->part != s.part ? p->part > s.part : pw != sw ? pw : p->at > s.at;
            if (!after) break;
            steps[j] = *p;
            j--;
        }
        steps[j] = s;
    }
}

/* Ends the program at once, as the quit step does, with this exit code. */
static void script_exit(int code)
{
    if (rt_on_fatal) rt_on_fatal();
    SDL_Quit();
    exit(code);
}

/* The wait step steps[step_next]: 1 when reached. After the timeout the program ends with code 3. */
static int wait_done(Step *s, uint64_t now)
{
    if (!waiting) {
        waiting = 1;
        wait_began = now;
        wait_polls = waits_poll_count();
        fprintf(stderr, "recomp: script wait %s (at most %d ms)\n", s->text, s->a);
    }
    if (waits_reached(s->kind, s->value, wait_polls)) {
        fprintf(stderr, "recomp: script reached %s after %llu ms\n", s->text, (unsigned long long)(now - wait_began));
        waiting = 0;
        return 1;
    }
    if (now - wait_began >= (uint64_t)s->a) {
        char state[256];
        waits_describe(state, sizeof state);
        fprintf(stderr, "recomp: script: wait %s timed out after %d ms. Now: %s\n", s->text, s->a, state);
        script_exit(3);
    }
    return 0;
}

static SDL_Keycode swallow_key = SDLK_UNKNOWN;    /* a key that did the work of an option */

/* One SDL event to the message queue. */
static void handle_event(SDL_Event *e)
{
    switch (e->type) {
    case SDL_RENDER_TARGETS_RESET:            /* Direct3D after a lost device, for example Alt+Tab from the 800x600 mode */
        win_screen_dirty = 1;
        break;
    case SDL_RENDER_DEVICE_RESET:             /* every texture is gone: make them again */
        fprintf(stderr, "recomp: render device reset\n");
        if (work) SDL_DestroyTexture(work);
        work = NULL;
        work_k = 1;
        work_failed_k = 0;
        SDL_DestroyTexture(texture);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                    (int)rt_screen_w, (int)rt_screen_h);
        if (!texture) rt_fatal("SDL_CreateTexture: %s", SDL_GetError());
        update_scaling();
        win_screen_dirty = 1;
        break;
    case SDL_QUIT:
        win_save_window();
        if (rt_on_fatal) rt_on_fatal();
        SDL_Quit();
        exit(0);
    case SDL_MOUSEMOTION:
        if (key_click.on) { key_click.real_x = e->motion.x; key_click.real_y = e->motion.y; break; }
        post_mouse(WM_MOUSEMOVE, e->motion.x, e->motion.y);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        int down = e->type == SDL_MOUSEBUTTONDOWN;
        if (e->button.button == SDL_BUTTON_X1) {
            if (down) shortcut_key(SDLK_AC_BACK, 0);
            break;
        }
        uint32_t bit = e->button.button == SDL_BUTTON_LEFT ? 1u : e->button.button == SDL_BUTTON_RIGHT ? 2u : 0u;
        if (!bit) break;
        if (down) buttons |= (int)bit; else buttons &= ~(int)bit;
        if (key_click.on) break;               /* the click of a key runs: drop the event, keep the button state */
        uint32_t msg = bit == 1 ? (down ? WM_LBUTTONDOWN : WM_LBUTTONUP) : (down ? WM_RBUTTONDOWN : WM_RBUTTONUP);
        post_mouse(msg, e->button.x, e->button.y);
        break;
    }
    case SDL_MOUSEWHEEL:
        if (e->wheel.y) post(WM_MOUSEWHEEL, ((uint32_t)(e->wheel.y * 120) & 0xffff) << 16 | (uint32_t)buttons, mouse_lparam());
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        int down = e->type == SDL_KEYDOWN;
        int done = down && !e->key.repeat ? shortcut_key(e->key.keysym.sym, e->key.keysym.mod) : 0;
        if (done == 2) break;
        if (done) {
            swallow_key = e->key.keysym.sym;
            break;
        }
        if (e->key.keysym.sym == swallow_key) {     /* its repeats, its character and its release */
            if (!down) swallow_key = SDLK_UNKNOWN;
            break;
        }
        uint32_t vk = virtual_key(e->key.keysym.sym);
        if (!vk) break;
        post(down ? WM_KEYDOWN : WM_KEYUP, vk, down ? 1u : 0xC0000001u);
        /* TranslateMessage gives these keys a character; SDL gives none. */
        if (down && (vk == 0x08 || vk == 0x09 || vk == 0x0D || vk == 0x1B)) post(WM_CHAR, vk, 1);
        break;
    }
    case SDL_TEXTINPUT: {
        const unsigned char *t = (const unsigned char *)e->text.text;
        if (swallow_key == SDLK_SPACE && !strcmp(e->text.text, " ")) break;
        /* UTF-8 to code page 1252 for the Latin-1 range. */
        for (; *t; t++) {
            uint32_t c = *t;
            if (c >= 0xC0 && c < 0xE0 && (t[1] & 0xC0) == 0x80) { c = ((c & 0x1F) << 6) | (t[1] & 0x3F); t++; }
            else if (c >= 0x80) continue;
            if (c < 0x100) post(WM_CHAR, c, 1);
        }
        break;
    }
    case SDL_WINDOWEVENT:
        if (e->window.event == SDL_WINDOWEVENT_EXPOSED) win_screen_dirty = 1;
        if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            update_scaling();
            note_windowed();
            display_report_event("size changed");
        }
        if (e->window.event == SDL_WINDOWEVENT_MAXIMIZED) display_report_event("maximised");
        if (e->window.event == SDL_WINDOWEVENT_RESTORED) display_report_event("restored");
        if (e->window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
            set_smallest_window();
            update_scaling();
            display_report_event("moved to another screen");
        }
        if (e->window.event == SDL_WINDOWEVENT_MOVED) note_windowed();
        if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) win_sound_focus(0);
        if (e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) win_sound_focus(1);
        break;
    }
}

/* A key press of the script, through the event code of the keyboard (handle_event): for example
 * "space", "gui+[", "ctrl+[" or "mouseback". A space also gives its character, as the keyboard does. */
static void press_key(const char *name)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    if (!strcmp(name, "mouseback")) {
        e.type = SDL_MOUSEBUTTONDOWN;
        e.button.button = SDL_BUTTON_X1;
        e.button.state = SDL_PRESSED;
        e.button.x = mouse_x;
        e.button.y = mouse_y;
        handle_event(&e);
        e.type = SDL_MOUSEBUTTONUP;
        e.button.state = SDL_RELEASED;
        handle_event(&e);
        return;
    }
    Uint16 mod = 0;
    const char *plus;
    while ((plus = strchr(name, '+')) && plus[1]) {
        if (!strncmp(name, "gui", 3)) mod |= KMOD_LGUI;
        else if (!strncmp(name, "alt", 3)) mod |= KMOD_LALT;
        else if (!strncmp(name, "ctrl", 4)) mod |= KMOD_LCTRL;
        else if (!strncmp(name, "shift", 5)) mod |= KMOD_LSHIFT;
        name = plus + 1;
    }
    SDL_Keycode k = SDL_GetKeyFromName(name);
    if (k == SDLK_UNKNOWN) { fprintf(stderr, "recomp: script: unknown key %s\n", name); return; }
    e.type = SDL_KEYDOWN;
    e.key.state = SDL_PRESSED;
    e.key.keysym.sym = k;
    e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
    e.key.keysym.mod = mod;
    handle_event(&e);
    if (k == SDLK_SPACE && !mod) {
        SDL_Event t;
        memset(&t, 0, sizeof t);
        t.type = SDL_TEXTINPUT;
        t.text.text[0] = ' ';
        handle_event(&t);
    }
    e.type = SDL_KEYUP;
    e.key.state = SDL_RELEASED;
    handle_event(&e);
}

static void run_script(void)
{
    if (!script_loaded) load_script();
    uint64_t now = SDL_GetTicks64() - opened_at;
    while (step_next < step_count && part_start + steps[step_next].at <= now) {
        Step *s = &steps[step_next];
        if (!strcmp(s->action, "wait")) {
            if (!wait_done(s, now)) break;
            step_next++;
            part_start = now;
            continue;
        }
        if (!strcmp(s->action, "up") && script_click.on && script_click.polls < 1 &&
            SDL_GetTicks64() - script_click.at < SCRIPT_CLICK_LONGEST)
            break;
        step_next++;
        fprintf(stderr, "recomp: script %llu %s %d %d %s\n", (unsigned long long)(part_start + s->at), s->action, s->a,
                s->b, s->text);
        if (!strcmp(s->action, "move")) post_mouse(WM_MOUSEMOVE, s->a, s->b);
        else if (!strcmp(s->action, "click") || !strcmp(s->action, "down")) {
            post_mouse(WM_MOUSEMOVE, s->a, s->b);
            buttons |= 1;
            post_mouse(WM_LBUTTONDOWN, s->a, s->b);
            script_click.on = 1;
            script_click.down_at = q_tail;
            script_click.polls = 0;
            script_click.at = SDL_GetTicks64();
        } else if (!strcmp(s->action, "up")) {
            script_click.on = 0;
            buttons &= ~1;
            post_mouse(WM_LBUTTONUP, s->a, s->b);
        } else if (!strcmp(s->action, "rclick")) {
            post_mouse(WM_MOUSEMOVE, s->a, s->b);
            buttons |= 2;
            post_mouse(WM_RBUTTONDOWN, s->a, s->b);
        } else if (!strcmp(s->action, "rup")) {
            buttons &= ~2;
            post_mouse(WM_RBUTTONUP, s->a, s->b);
        } else if (!strcmp(s->action, "key")) {
            post(WM_KEYDOWN, (uint32_t)s->a, 1);
            post(WM_KEYUP, (uint32_t)s->a, 0xC0000001u);
        } else if (!strcmp(s->action, "char")) post(WM_CHAR, (uint32_t)s->a, 1);
        else if (!strcmp(s->action, "press")) press_key(s->text);
        else if (!strcmp(s->action, "resize")) {
            SDL_SetWindowSize(window, s->a, s->b);
            update_scaling();
            present_now();
        } else if (!strcmp(s->action, "dump")) { present_now(); win_dump_screen(s->text); }
        else if (!strcmp(s->action, "shot")) shot_window(s->text);
        else if (!strcmp(s->action, "report")) display_report_key();
        else if (!strcmp(s->action, "quit")) {
            win_save_window();
            script_exit(0);
        }
    }
}

void win_poll_events(void)
{
#ifdef __EMSCRIPTEN__
    rt_web_yield();                               /* the page draws and takes input in this time (host/web.c) */
#endif
    uint64_t now = SDL_GetTicks64();
    if (win_screen_dirty && now - last_present >= 16) present_now();
    if (now - last_poll < 2) return;              /* the game pumps very often; SDL needs less */
    last_poll = now;
    end_click_for_key();
    SDL_Event e;
    while (SDL_PollEvent(&e))
        handle_event(&e);
    if (window) run_script();
}

/* PeekMessageA(msg, hwnd, first, last, remove) */
RT_IMPORT(PeekMessageA)
{
    uint32_t m = rt_arg(0), remove = rt_arg(4);
    win_poll_events();
    if (q_head == q_tail) { rt_ret_stdcall(0, 5); return; }
    Msg *q = &queue[q_head % QUEUE];
    wr32(g_mem, m, win_hwnd);
    wr32(g_mem, m + 4, q->msg);
    wr32(g_mem, m + 8, q->wparam);
    wr32(g_mem, m + 12, q->lparam);
    wr32(g_mem, m + 16, q->time);
    wr32(g_mem, m + 20, (uint32_t)mouse_x);
    wr32(g_mem, m + 24, (uint32_t)mouse_y);
    if (remove & 1) q_head++;
    rt_ret_stdcall(1, 5);
}

/* GetMessageA(msg, hwnd, first, last): waits for a message. */
RT_IMPORT(GetMessageA)
{
    uint32_t m = rt_arg(0);
    while (q_head == q_tail) {
        win_poll_events();
        if (q_head == q_tail) SDL_Delay(1);
    }
    Msg *q = &queue[q_head++ % QUEUE];
    wr32(g_mem, m, win_hwnd);
    wr32(g_mem, m + 4, q->msg);
    wr32(g_mem, m + 8, q->wparam);
    wr32(g_mem, m + 12, q->lparam);
    wr32(g_mem, m + 16, q->time);
    wr32(g_mem, m + 20, (uint32_t)mouse_x);
    wr32(g_mem, m + 24, (uint32_t)mouse_y);
    rt_ret_stdcall(q->msg != WM_QUIT, 4);
}

/* Repeat of a held button. The screen loop 0x7fc7e0 runs a pass: the pump 0x68d520, the input poll
 * 0x55f620, the input function of the screen. While the left button flag 0xb67a34 is set, the hit
 * test 0x560490 gives the widget under the pointer in every pass, so a held button acts once per pass.
 * The +/- buttons (for example the Offer Contract input 0x4e2280, cases 4/5, 9/10, 0x17/0x18) change
 * the value, redraw the screen and return 0, which keeps the flag. The original has no timer for this
 * (its only clock read for input is the pump tick 0xb67a08, which no button code reads). Its pass took
 * the time of two full screen draws on a PC of 2001. Here a pass takes about 1 ms, so one click gave
 * a hundred steps.
 *
 * So the runtime paces a repeat: the same widget again from the poll in the same press. It is given to
 * the game REPEAT_DELAY ms after the press, then every REPEAT_INTERVAL ms. If the button goes up before
 * that, the poll gives -1 (nothing). The first result of each press, and a result for another widget
 * (a click that ends a text entry and then hits the button under the pointer), go through at once. */
#define REPEAT_DELAY 400u
#define REPEAT_INTERVAL 100u
static uint32_t last_press, last_widget = 0xffffu, last_top;
static uint64_t next_repeat_at;

/* Idle CPU (#58). The original has no Sleep: a screen that waits for input runs the screen loop
 * 0x7fc7e0 without a stop, so the game used one full core (99.6% on the transfer status screen in
 * 25 s, ps, 2026-10-16). So when a pass of the screen loop gives no input (the poll gives -1, no
 * button is down, the queue is empty), no message came for IDLE_AFTER ms and no game work ran for
 * WORK_HOLD ms, the runtime sleeps 1 ms after the poll. The sleep touches no game memory. It changes
 * only the real time of a pass and so the count of passes in a stretch of real time.
 *
 * The game work rule keeps match results. While a commentary line waits, each pass runs one step of
 * a background match (0x6b5ce0 from the commentary step 0x6b55c0), and that step draws from the
 * game's random function 0x90cfc0 (docs/research/2026-10-15-commentary-speed-inventory.md, section
 * 3). The human match takes its numbers from the same table. So a call of the commentary step or of
 * the random function counts as game work, and no pass sleeps until WORK_HOLD ms after the last poll
 * that saw one. Evidence: with RECOMP_IDLE_SLEEP=force (no IDLE_AFTER and no WORK_HOLD rule) the
 * match check fails (tests/smoke/smoke_run.py --match --sleep-danger). An idle screen draws no number
 * (docs/research/2026-10-14-assistant-and-days-inventory.md: no draw under the screen loop in a 10 s
 * sample).
 *
 * Paths that measure real time and can see the sleep, each at most 1 ms later than without it:
 *   - The holiday hold: the side bar input 0x7615d0 reads the clock 0x90c510 at 0x762252 in each pass
 *     until 2000 ms passed. The day's work draws numbers, so the sleep runs only in the last part of
 *     each hold, after WORK_HOLD.
 *   - The screen timeout at 0x7fcc54..0x7fccbf in the screen loop: it reads time() in each pass.
 *   - The commentary line delay (0x6d8c70): it reads 0x90c510, but WORK_HOLD keeps the sleep off
 *     while the commentary step runs.
 *
 * Test runs only: RECOMP_IDLE_SLEEP=0 turns the sleep off, and the same-save checks compare the two.
 * RECOMP_IDLE_SLEEP=force drops the IDLE_AFTER and WORK_HOLD rules, to show that the match check
 * catches a sleep in the wrong place. RECOMP_IDLE_COUNT=1 prints the count of sleeps at the exit, so
 * the checks can see that the sleep ran. */
#define IDLE_AFTER 100u
#define WORK_HOLD 1000u
enum { SLEEP_OFF, SLEEP_ON, SLEEP_FORCE };
static int idle_sleep = -1;
static uint32_t work_count, work_seen;            /* calls that count as game work; the count at the last poll */
static uint64_t last_work_at, sleep_count;

void win_game_work(void) { work_count++; }

void o_0090cfc0(void);

uint32_t (*win_random_hook)(uint32_t esp, uint32_t edi, uint32_t result);

/* The game's random function random(n), 0x90cfc0: noted as game work for idle_rest only. The hook (win.h) can
 * replace the result of a call from one place of the game's code; it is NULL in the base game. */
RT_WRAP(0090cfc0, game_random)
{
    win_game_work();
    uint32_t esp = cpu.esp, edi = cpu.edi;
    o_0090cfc0();
    if (win_random_hook && !rt_unwinding) cpu.eax = win_random_hook(esp, edi, cpu.eax);
}

static void print_sleep_count(void) { fprintf(stderr, "recomp: idle sleeps %llu\n", (unsigned long long)sleep_count); }

static void idle_rest(uint32_t widget)
{
    if (idle_sleep < 0) {
        const char *v = getenv("RECOMP_IDLE_SLEEP");
        idle_sleep = !v ? SLEEP_ON : !strcmp(v, "0") ? SLEEP_OFF : !strcmp(v, "force") ? SLEEP_FORCE : SLEEP_ON;
        const char *c = getenv("RECOMP_IDLE_COUNT");
        if (c && !strcmp(c, "1")) atexit(print_sleep_count);
    }
    uint64_t now = SDL_GetTicks64();
    if (work_count != work_seen) {
        work_seen = work_count;
        last_work_at = now;
    }
    if (idle_sleep == SLEEP_OFF || widget != 0xffffu || buttons || q_head != q_tail) return;
    if (idle_sleep == SLEEP_ON && (now - last_input_at < IDLE_AFTER || now - last_work_at < WORK_HOLD)) return;
    sleep_count++;
    SDL_Delay(1);
}

void o_0055f620(void);

/* The input poll, thiscall, no arguments: the widget index in AX, -1 for nothing. */
RT_WRAP(0055f620, input_poll)
{
    int key_down_in = key_click.on && (int32_t)(q_head - key_click.down_at) >= 0;
    keys_display = cpu.ecx;
    uint32_t poll_ret = rd32(g_mem, cpu.esp), poll_display = cpu.ecx;
    waits_poll();
    o_0055f620();
    options_screen_after_poll(poll_ret, poll_display, cpu.eax & 0xffffu);   /* the options box item */
    names_after_poll();
    if (key_down_in) key_click.polls++;
    if (script_click.on && (int32_t)(q_head - script_click.down_at) >= 0) script_click.polls++;
    uint32_t widget = cpu.eax & 0xffffu;
    int held = (buttons & 1) && rd32(g_mem, 0xb67a34u) && !rd32(g_mem, 0xb67a3cu);
    idle_rest(widget);
    if (widget == 0xffffu || !held) return;
    uint32_t top = rd32(g_mem, 0xb74450u + 0x7d4u + 0x0eu);   /* the screen node on top (waits.c) */
    if (press_count != last_press || widget != last_widget) {
        last_press = press_count;
        last_widget = widget;
        last_top = top;
        next_repeat_at = press_at + REPEAT_DELAY;
        return;
    }
    /* A press that opened or closed a screen does not repeat on the next screen: the same widget number
     * there is another widget (the options box, options_screen.c). */
    if (top != last_top) { cpu.eax |= 0xffffu; return; }
    while ((buttons & 1) && SDL_GetTicks64() < next_repeat_at) {
        win_poll_events();
        SDL_Delay(1);
    }
    if (!(buttons & 1)) { cpu.eax |= 0xffffu; return; }    /* released while it waited */
    next_repeat_at = SDL_GetTicks64() + REPEAT_INTERVAL;
}

/* The same pacing for the scrollbars. The hit test calls the scrollbar test 0x561f10 for each area
 * with a scrollbar in every pass. While 0xb67a34 is set and the pointer is on an arrow or on the track,
 * it moves the list one row or one page and returns 1, so one click moved a list to its end. Here the
 * first move of a press goes through at once. Later moves of the same area in the same press wait as
 * above. If the button goes up during the wait, the test runs with the button flag clear for that call:
 * it then draws the arrow up and moves nothing. The wheel and the keys hold the button for a count of
 * passes (0xb67a3c), one row each; they do not wait. A click on the thumb starts a drag (display mode
 * 2), which does not call this test. */
static uint32_t scroll_press, scroll_this, scroll_area = 0xffffffffu;
static uint64_t next_scroll_at;

void o_00561f10(void);

/* The scrollbar test, thiscall: (s16 area index); returns 1 when the list moved. */
RT_WRAP(00561f10, scrollbar_test)
{
    uint32_t area = rt_arg(0) & 0xffffu, self = cpu.ecx;
    int held = (buttons & 1) && rd32(g_mem, 0xb67a34u) && !rd32(g_mem, 0xb67a3cu);
    int again = held && scroll_press == press_count && scroll_this == self && scroll_area == area;
    if (again) {
        while ((buttons & 1) && SDL_GetTicks64() < next_scroll_at) {
            win_poll_events();
            SDL_Delay(1);
        }
        if (!(buttons & 1)) {                                  /* released while it waited */
            wr32(g_mem, 0xb67a34u, 0);
            o_00561f10();
            wr32(g_mem, 0xb67a34u, 1);
            return;
        }
    }
    o_00561f10();
    if (!held || !(cpu.eax & 0xffu)) return;
    if (!again) {
        scroll_press = press_count;
        scroll_this = self;
        scroll_area = area;
        next_scroll_at = press_at + REPEAT_DELAY;
    } else {
        next_scroll_at = SDL_GetTicks64() + REPEAT_INTERVAL;
    }
}

RT_IMPORT(TranslateMessage) { rt_ret_stdcall(1, 1); }

RT_IMPORT(DispatchMessageA)
{
    uint32_t m = rt_arg(0);
    uint32_t args[4] = {rd32(g_mem, m), rd32(g_mem, m + 4), rd32(g_mem, m + 8), rd32(g_mem, m + 12)};
    if (!win_wndproc) rt_fatal("DispatchMessageA before RegisterClassA");
    uint32_t r = rt_callback(win_wndproc, 4, args);
    if (rt_unwinding) return;
    rt_ret_stdcall(r, 1);
}

RT_IMPORT(DefWindowProcA) { rt_ret_stdcall(0, 4); }

/* WNDCLASSA: the window procedure is at +4. */
RT_IMPORT(RegisterClassA)
{
    win_wndproc = rd32(g_mem, rt_arg(0) + 4);
    WIN_TRACE("RegisterClassA: window procedure %#x\n", win_wndproc);
    rt_ret_stdcall(0xC001u, 1);
}

/* CreateWindowExA(ex style, class, title, style, x, y, w, h, parent, menu, instance, param) */
RT_IMPORT(CreateWindowExA)
{
    win_video_init();
    if (rt_arg(2)) SDL_SetWindowTitle(window, win_str(rt_arg(2)));
    rt_ret_stdcall(win_hwnd, 12);
}

RT_IMPORT(ShowWindow)
{
    if (window) { if (rt_arg(1)) SDL_ShowWindow(window); else SDL_HideWindow(window); }
    rt_ret_stdcall(1, 2);
}
RT_IMPORT(UpdateWindow) { rt_ret_stdcall(1, 1); }
RT_IMPORT(MoveWindow) { rt_ret_stdcall(1, 6); }
RT_IMPORT(SetWindowLongA) { rt_ret_stdcall(0, 3); }
RT_IMPORT(DestroyWindow) { rt_ret_stdcall(1, 1); }
RT_IMPORT(CloseWindow) { rt_ret_stdcall(1, 1); }
RT_IMPORT(ChangeDisplaySettingsA) { rt_ret_stdcall(0, 2); }
RT_IMPORT(GetDesktopWindow) { rt_ret_stdcall(DESKTOP_HWND, 0); }

/* The screen and the window both have the screen size, with no frame. */
RT_IMPORT(GetSystemMetrics)
{
    uint32_t i = rt_arg(0);
    rt_ret_stdcall(i == 0 ? rt_screen_w : i == 1 ? rt_screen_h : 0u, 1);
}

RT_IMPORT(GetWindowRect)
{
    uint32_t r = rt_arg(1);
    wr32(g_mem, r, 0);
    wr32(g_mem, r + 4, 0);
    wr32(g_mem, r + 8, rt_screen_w);
    wr32(g_mem, r + 12, rt_screen_h);
    rt_ret_stdcall(1, 2);
}

/* BeginPaint(hwnd, PAINTSTRUCT): a fake device context. */
RT_IMPORT(BeginPaint)
{
    uint32_t ps = rt_arg(1);
    memset(g_mem + ps, 0, 64);
    wr32(g_mem, ps, 0x00020001u);
    wr32(g_mem, ps + 16, rt_screen_w);
    wr32(g_mem, ps + 20, rt_screen_h);
    rt_ret_stdcall(0x00020001u, 2);
}
RT_IMPORT(EndPaint) { rt_ret_stdcall(1, 2); }

/* Cursors: a handle per resource; system cursors stand in for the images (win_cursor_shape). */
#define CURSOR_BASE 0x00030000u
static uint32_t current_cursor;
static int cursor_count;                          /* the ShowCursor counter; shown while >= 0 */

RT_IMPORT(LoadCursorA)
{
    uint32_t id = rt_arg(1);
    rt_ret_stdcall(CURSOR_BASE + (id & 0xffff), 2);
}
RT_IMPORT(LoadIconA) { rt_ret_stdcall(0x00040000u + (rt_arg(1) & 0xffff), 2); }
RT_IMPORT(DestroyCursor) { rt_ret_stdcall(1, 1); }
RT_IMPORT(GetCursor) { rt_ret_stdcall(current_cursor, 0); }

RT_IMPORT(SetCursor)
{
    static SDL_Cursor *cursors[SDL_NUM_SYSTEM_CURSORS];
    uint32_t h = rt_arg(0), old = current_cursor;
    current_cursor = h;
    if (window && h) {
        int shape = win_cursor_shape(h);          /* names.c */
        if (!cursors[shape]) cursors[shape] = SDL_CreateSystemCursor((SDL_SystemCursor)shape);
        if (cursors[shape]) SDL_SetCursor(cursors[shape]);
    }
    rt_ret_stdcall(old, 1);
}

RT_IMPORT(ShowCursor)
{
    cursor_count += rt_arg(0) ? 1 : -1;
    SDL_ShowCursor(cursor_count >= 0 ? SDL_ENABLE : SDL_DISABLE);
    rt_ret_stdcall((uint32_t)cursor_count, 1);
}

/* MessageBoxA(hwnd, text, caption, type): print it, show it, answer OK. */
RT_IMPORT(MessageBoxA)
{
    const char *text = rt_arg(1) ? win_str(rt_arg(1)) : "";
    const char *caption = rt_arg(2) ? win_str(rt_arg(2)) : "";
    fprintf(stderr, "recomp: message box \"%s\": %s\n", caption, text);
    if (!getenv("RECOMP_NO_BOX")) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, caption, text, window);
    rt_ret_stdcall(1, 4);
}

/* English (UK). */
RT_IMPORT(GetKeyboardLayout) { rt_ret_stdcall(0x08090809u, 1); }
