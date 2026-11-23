/* DirectDraw (version 1) over SDL: IDirectDraw, IDirectDrawSurface and IDirectDrawClipper as the
 * display code of the original uses them (inventory 2.1). Every surface is RGB565 in flat memory, and
 * the primary and the back buffer have the size of the screen (rt_screen_w x rt_screen_h), so Lock gives the original a pointer it can write. The primary surface is what the window
 * shows: a Blt or BltFast to it, or an Unlock of it, marks the screen changed, and user32.c presents it. */
#include "win.h"

/* A surface: vtable, count, width +8, height +0xc, pitch +0x10, pixels +0x14, primary +0x18, caps +0x1c. */
#define S_W 8u
#define S_H 0xcu
#define S_PITCH 0x10u
#define S_PIXELS 0x14u
#define S_PRIMARY 0x18u
#define S_CAPS 0x1cu

static uint32_t dd_vtable, surface_vtable, clipper_vtable;
static uint32_t primary;
int win_screen_dirty;

uint32_t win_primary_pixels(void) { return primary ? rd32(g_mem, primary + S_PIXELS) : 0; }

/* Rectangles: left, top, right, bottom; NULL is the whole surface. Clipped to the surface. */
typedef struct { int32_t l, t, r, b; } Rect;

static Rect rect_of(uint32_t p, uint32_t surf)
{
    Rect r = {0, 0, (int32_t)rd32(g_mem, surf + S_W), (int32_t)rd32(g_mem, surf + S_H)};
    if (p) {
        r.l = (int32_t)rd32(g_mem, p);
        r.t = (int32_t)rd32(g_mem, p + 4);
        r.r = (int32_t)rd32(g_mem, p + 8);
        r.b = (int32_t)rd32(g_mem, p + 12);
    }
    return r;
}

static void copy_rect(uint32_t dst, int32_t x, int32_t y, uint32_t src, Rect s)
{
    int32_t dw = (int32_t)rd32(g_mem, dst + S_W), dh = (int32_t)rd32(g_mem, dst + S_H);
    int32_t sw = (int32_t)rd32(g_mem, src + S_W), sh = (int32_t)rd32(g_mem, src + S_H);
    if (s.l < 0) { x -= s.l; s.l = 0; }
    if (s.t < 0) { y -= s.t; s.t = 0; }
    if (s.r > sw) s.r = sw;
    if (s.b > sh) s.b = sh;
    if (x < 0) { s.l -= x; x = 0; }
    if (y < 0) { s.t -= y; y = 0; }
    if (x + (s.r - s.l) > dw) s.r = s.l + dw - x;
    if (y + (s.b - s.t) > dh) s.b = s.t + dh - y;
    if (s.r <= s.l || s.b <= s.t) return;
    uint32_t dp = rd32(g_mem, dst + S_PIXELS), sp = rd32(g_mem, src + S_PIXELS);
    uint32_t dpitch = rd32(g_mem, dst + S_PITCH), spitch = rd32(g_mem, src + S_PITCH);
    for (int32_t row = 0; row < s.b - s.t; row++) {
        memmove(g_mem + dp + (uint32_t)(y + row) * dpitch + (uint32_t)x * 2,
                g_mem + sp + (uint32_t)(s.t + row) * spitch + (uint32_t)s.l * 2, (size_t)(s.r - s.l) * 2);
    }
    if (dst == primary) win_screen_dirty = 1;
}

/* IDirectDrawSurface */
static void surf_Blt(void)
{
    uint32_t self = rt_arg(0), dst_rect = rt_arg(1), src = rt_arg(2), src_rect = rt_arg(3);
    uint32_t flags = rt_arg(4), fx = rt_arg(5);
    if (flags & 0x400) {                                  /* DDBLT_COLORFILL */
        Rect d = rect_of(dst_rect, self);
        uint32_t colour = rd32(g_mem, fx + 0x50) & 0xffff;
        uint32_t px = rd32(g_mem, self + S_PIXELS), pitch = rd32(g_mem, self + S_PITCH);
        int32_t w = (int32_t)rd32(g_mem, self + S_W), h = (int32_t)rd32(g_mem, self + S_H);
        if (d.l < 0) d.l = 0;
        if (d.t < 0) d.t = 0;
        if (d.r > w) d.r = w;
        if (d.b > h) d.b = h;
        for (int32_t y = d.t; y < d.b; y++)
            for (int32_t x = d.l; x < d.r; x++) wr16(g_mem, px + (uint32_t)y * pitch + (uint32_t)x * 2, colour);
        if (self == primary) win_screen_dirty = 1;
    } else if (src) {
        /* The windowed flip passes the target in screen coordinates; the window has no frame here, so
         * the picture keeps the place it has in the source. */
        Rect s = rect_of(src_rect, src);
        if (self == primary || !dst_rect) copy_rect(self, s.l, s.t, src, s);
        else copy_rect(self, (int32_t)rd32(g_mem, dst_rect), (int32_t)rd32(g_mem, dst_rect + 4), src, s);
    }
    if (self == primary) win_present();
    rt_ret_stdcall(0, 6);
}

static void surf_BltFast(void)
{
    uint32_t self = rt_arg(0), x = rt_arg(1), y = rt_arg(2), src = rt_arg(3), src_rect = rt_arg(4);
    copy_rect(self, (int32_t)x, (int32_t)y, src, rect_of(src_rect, src));
    if (self == primary) win_present();
    rt_ret_stdcall(0, 6);
}

/* DDSURFACEDESC: size 0, flags 4, height 8, width 0xc, pitch 0x10, surface 0x24, pixel format 0x48
 * (size, flags, fourcc, bit count 0x54, masks 0x58 0x5c 0x60 0x64), caps 0x68. */
static void fill_desc(uint32_t self, uint32_t desc, uint32_t pixels)
{
    memset(g_mem + desc + 4, 0, 0x68);
    wr32(g_mem, desc, 0x6c);
    wr32(g_mem, desc + 4, 0x100fu);                       /* CAPS HEIGHT WIDTH PITCH PIXELFORMAT */
    wr32(g_mem, desc + 8, rd32(g_mem, self + S_H));
    wr32(g_mem, desc + 0xc, rd32(g_mem, self + S_W));
    wr32(g_mem, desc + 0x10, rd32(g_mem, self + S_PITCH));
    wr32(g_mem, desc + 0x24, pixels);
    wr32(g_mem, desc + 0x48, 32);
    wr32(g_mem, desc + 0x4c, 0x40);                       /* DDPF_RGB */
    wr32(g_mem, desc + 0x54, 16);
    wr32(g_mem, desc + 0x58, 0xF800);
    wr32(g_mem, desc + 0x5c, 0x07E0);
    wr32(g_mem, desc + 0x60, 0x001F);
    wr32(g_mem, desc + 0x68, rd32(g_mem, self + S_CAPS));
}

static void surf_GetSurfaceDesc(void)
{
    uint32_t self = rt_arg(0);
    fill_desc(self, rt_arg(1), rd32(g_mem, self + S_PIXELS));
    rt_ret_stdcall(0, 2);
}

/* Lock(rect, desc, flags, event): the pointer of the rect's top-left pixel. */
static void surf_Lock(void)
{
    uint32_t self = rt_arg(0), rect = rt_arg(1), desc = rt_arg(2);
    Rect r = rect_of(rect, self);
    uint32_t px = rd32(g_mem, self + S_PIXELS) + (uint32_t)r.t * rd32(g_mem, self + S_PITCH) + (uint32_t)r.l * 2;
    fill_desc(self, desc, px);
    rt_ret_stdcall(0, 5);
}

static void surf_Unlock(void)
{
    if (rt_arg(0) == primary) win_screen_dirty = 1;
    rt_ret_stdcall(0, 2);
}

static void surf_IsLost(void) { rt_ret_stdcall(0, 1); }
static void surf_Restore(void) { rt_ret_stdcall(0, 1); }
static void surf_SetClipper(void) { rt_ret_stdcall(0, 2); }

static const WinMethod surface_methods[] = {
    {0x04, "AddRef", win_com_addref}, {0x08, "Release", win_com_release},
    {0x14, "Blt", surf_Blt}, {0x1c, "BltFast", surf_BltFast}, {0x58, "GetSurfaceDesc", surf_GetSurfaceDesc},
    {0x60, "IsLost", surf_IsLost}, {0x64, "Lock", surf_Lock}, {0x6c, "Restore", surf_Restore},
    {0x70, "SetClipper", surf_SetClipper}, {0x80, "Unlock", surf_Unlock}, {0, NULL, NULL},
};

/* IDirectDrawClipper */
static void clip_SetHWnd(void) { rt_ret_stdcall(0, 3); }
static const WinMethod clipper_methods[] = {
    {0x04, "AddRef", win_com_addref}, {0x08, "Release", win_com_release}, {0x20, "SetHWnd", clip_SetHWnd},
    {0, NULL, NULL},
};

/* IDirectDraw */
static void dd_CreateClipper(void)
{
    wr32(g_mem, rt_arg(2), win_make_object(clipper_vtable, 16));
    rt_ret_stdcall(0, 4);
}

/* CreateSurface(desc, &surface, outer). The primary (caps 0x200) has the size of the screen; another
 * surface has the size in its desc, else the size of the screen. */
static void dd_CreateSurface(void)
{
    uint32_t desc = rt_arg(1), out = rt_arg(2);
    uint32_t flags = rd32(g_mem, desc + 4), caps = rd32(g_mem, desc + 0x68);
    uint32_t w = rt_screen_w, h = rt_screen_h;
    if (!(caps & 0x200) && (flags & 6) == 6) {
        w = rd32(g_mem, desc + 0xc);
        h = rd32(g_mem, desc + 8);
    }
    uint32_t s = win_make_object(surface_vtable, 0x40);
    wr32(g_mem, s + S_W, w);
    wr32(g_mem, s + S_H, h);
    wr32(g_mem, s + S_PITCH, w * 2);
    wr32(g_mem, s + S_PIXELS, win_static_alloc(w * h * 2));
    wr32(g_mem, s + S_PRIMARY, (caps & 0x200) != 0);
    wr32(g_mem, s + S_CAPS, caps);
    if (caps & 0x200) primary = s;
    WIN_TRACE("CreateSurface(caps %#x, %ux%u) = %#x\n", caps, w, h, s);
    wr32(g_mem, out, s);
    rt_ret_stdcall(0, 4);
}

static void dd_RestoreDisplayMode(void) { rt_ret_stdcall(0, 1); }
/* SetCooperativeLevel(hwnd, level): DDSCL_FULLSCREEN (1, in the 0x13 of the full screen call) or DDSCL_NORMAL (8).
 * The mode is set at the SetDisplayMode that follows in both paths of the display init 0x5e2620 (0x5e2a2c, 0x5e2a7e). */
static int level_full;
static void dd_SetCooperativeLevel(void)
{
    level_full = (rt_arg(2) & 1) != 0;
    rt_ret_stdcall(0, 3);
}
static void dd_SetDisplayMode(void)
{
    win_screen_mode(level_full);
    rt_ret_stdcall(0, 4);
}

static const WinMethod dd_methods[] = {
    {0x04, "AddRef", win_com_addref}, {0x08, "Release", win_com_release},
    {0x10, "CreateClipper", dd_CreateClipper}, {0x18, "CreateSurface", dd_CreateSurface},
    {0x4c, "RestoreDisplayMode", dd_RestoreDisplayMode}, {0x50, "SetCooperativeLevel", dd_SetCooperativeLevel},
    {0x54, "SetDisplayMode", dd_SetDisplayMode}, {0, NULL, NULL},
};

/* DirectDrawCreate(guid, &object, outer) */
RT_IMPORT(DirectDrawCreate)
{
    if (!dd_vtable) {
        dd_vtable = win_make_vtable("IDirectDraw", 23, dd_methods);
        surface_vtable = win_make_vtable("IDirectDrawSurface", 36, surface_methods);
        clipper_vtable = win_make_vtable("IDirectDrawClipper", 9, clipper_methods);
    }
    win_video_init();
    wr32(g_mem, rt_arg(1), win_make_object(dd_vtable, 16));
    rt_ret_stdcall(0, 3);
}
