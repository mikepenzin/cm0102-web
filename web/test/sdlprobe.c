/* The SDL probe: a stand-in for the game in the page checks (web/test/sdl-check.cjs), built with Emscripten and the
 * real SDL2 port. It opens its window and renderer as the runtime does (src/recomp_rt/win/user32.c: a resizable
 * high-DPI window, fitted to the usable bounds; a renderer with the logical size of the picture; a streaming texture)
 * and never returns from its loop, sleeping as the runtime does. It prints one line of JSON for each thing SDL tells
 * it, so the check sees what the real SDL browser backend makes of the page's canvas size and of the events that the
 * page passes on or makes (web/touch.js, web/keyboard.js).
 *
 * It also has the runtime's exports for the page (user32.c), with the same names and arguments: rt_web_type,
 * rt_web_key, rt_web_picture and rt_web_typing_row, and prints what they get. No game is behind it.
 *
 * Build (web/test/build-probe.sh): emcc with -sUSE_SDL=2 and -sASYNCIFY, and the link options of web/web.cmake that
 * the page relies on. */
#include <SDL.h>
#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#define PW 1280
#define PH 800

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int typing_row = -1;
static int pic_w = PW, pic_h = PH;                /* the size the probe says its picture has */
static void report_size(const char *why);

EMSCRIPTEN_KEEPALIVE void rt_web_type(const char *utf8) { printf("{\"probe\":\"type\",\"text\":\"%s\"}\n", utf8); }
EMSCRIPTEN_KEEPALIVE void rt_web_key(int vk) { printf("{\"probe\":\"key\",\"vk\":%d}\n", vk); }
EMSCRIPTEN_KEEPALIVE int rt_web_picture(void) { return window ? pic_w << 16 | pic_h : 0; }
EMSCRIPTEN_KEEPALIVE int rt_web_typing_row(void) { return typing_row; }
/* For the check only: a text entry starts or ends taking keys at this row of the picture. */
EMSCRIPTEN_KEEPALIVE void probe_set_typing(int row) { typing_row = row; }
/* For the check only: the picture has another size from now on, as classic mode has (800x600). */
EMSCRIPTEN_KEEPALIVE void probe_set_picture(int w, int h)
{
    pic_w = w;
    pic_h = h;
    SDL_RenderSetLogicalSize(renderer, w, h);
    report_size("picture");
}

static void report_size(const char *why)
{
    int ww = 0, wh = 0, ow = 0, oh = 0;
    SDL_Rect vp = {0, 0, 0, 0};
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GetRendererOutputSize(renderer, &ow, &oh);
    SDL_RenderGetViewport(renderer, &vp);
    float sx = 0, sy = 0;
    SDL_RenderGetScale(renderer, &sx, &sy);
    printf("{\"probe\":\"size\",\"why\":\"%s\",\"window\":[%d,%d],\"output\":[%d,%d],\"viewport\":[%d,%d,%d,%d],\"scale\":%.4f}\n",
           why, ww, wh, ow, oh, vp.x, vp.y, vp.w, vp.h, sx);
}

static void draw(void)
{
    static Uint16 px[PW * PH];
    static int made;
    if (!made) {
        made = 1;
        for (int y = 0; y < PH; y++)
            for (int x = 0; x < PW; x++) {
                int edge = x < 4 || y < 4 || x >= PW - 4 || y >= PH - 4;          /* a red frame at the picture's edge */
                int grid = (x % 160 == 0) || (y % 160 == 0);
                px[y * PW + x] = edge ? 0xF800 : grid ? 0x07E0 : (Uint16)(((x * 31 / PW) << 0) | ((y * 31 / PH) << 11));
            }
        SDL_UpdateTexture(texture, NULL, px, PW * 2);
    }
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    /* As names_draw of the runtime: a line in a light colour. The colour stays the renderer's draw colour, so the next
     * SDL_RenderClear fills the bars beside the picture with it. */
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderDrawLine(renderer, 100, 100, 300, 100);
    SDL_RenderPresent(renderer);
}

/* export_name: some pairings of clang and Emscripten (Debian's 15 and 3.1.6) name a main with arguments
 * __main_argc_argv and then do not find it; the export under the name main is what the page's callMain calls. */
__attribute__((export_name("main"))) int main(int argc, char **argv)
{
    printf("{\"probe\":\"main\",\"args\":%d,\"game\":\"%s\"}\n", argc, argc > 2 ? argv[2] : "");
    if (SDL_Init(SDL_INIT_VIDEO)) { printf("{\"probe\":\"error\",\"what\":\"%s\"}\n", SDL_GetError()); return 1; }
    window = SDL_CreateWindow("probe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, PW, PH,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);
    if (!window) { printf("{\"probe\":\"error\",\"what\":\"%s\"}\n", SDL_GetError()); return 1; }
    /* As fit_new_window: the picture's shape, as large as the usable bounds less 32 rows allow. */
    SDL_Rect r;
    if (!SDL_GetDisplayUsableBounds(0, &r)) {
        int fh = r.h - 32, fw = fh * PW / PH;
        if (fw > r.w) { fw = r.w; fh = fw * PH / PW; }
        printf("{\"probe\":\"fit\",\"usable\":[%d,%d],\"window\":[%d,%d]}\n", r.w, r.h, fw, fh);
        SDL_SetWindowSize(window, fw, fh);
    }
    SDL_ShowWindow(window);
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) { printf("{\"probe\":\"error\",\"what\":\"%s\"}\n", SDL_GetError()); return 1; }
    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    SDL_RenderSetLogicalSize(renderer, PW, PH);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, PW, PH);
    SDL_StartTextInput();
    printf("{\"probe\":\"ready\",\"renderer\":\"%s\"}\n", info.name);
    report_size("start");
    Uint32 last_move = 0;
    for (;;) {
        SDL_Event e;
        int got = 0;
        while (SDL_PollEvent(&e)) {
            got = 1;
            switch (e.type) {
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                printf("{\"probe\":\"button\",\"down\":%d,\"button\":%d,\"x\":%d,\"y\":%d,\"touch\":%d}\n",
                       e.type == SDL_MOUSEBUTTONDOWN, e.button.button, e.button.x, e.button.y, e.button.which == SDL_TOUCH_MOUSEID);
                break;
            case SDL_MOUSEMOTION:
                if (SDL_GetTicks() - last_move >= 30 || e.motion.state) {
                    last_move = SDL_GetTicks();
                    printf("{\"probe\":\"move\",\"x\":%d,\"y\":%d,\"held\":%d}\n", e.motion.x, e.motion.y, (int)e.motion.state);
                }
                break;
            case SDL_MOUSEWHEEL: printf("{\"probe\":\"wheel\",\"y\":%d}\n", e.wheel.y); break;
            case SDL_FINGERDOWN: printf("{\"probe\":\"finger\",\"down\":1}\n"); break;
            case SDL_FINGERUP: printf("{\"probe\":\"finger\",\"down\":0}\n"); break;
            case SDL_KEYDOWN: if (!e.key.repeat) printf("{\"probe\":\"keydown\",\"sym\":%d}\n", (int)e.key.keysym.sym); break;
            case SDL_TEXTINPUT: printf("{\"probe\":\"text\",\"text\":\"%s\"}\n", e.text.text); break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) report_size("size changed");
                break;
            case SDL_QUIT: printf("{\"probe\":\"quit\"}\n"); return 0;
            }
        }
        draw();
        emscripten_sleep(got ? 0 : 4);              /* the runtime: a yield at the input poll, a sleep when idle */
    }
}
