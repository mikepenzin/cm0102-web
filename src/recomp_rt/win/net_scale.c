/* Network screens in Widescreen mode (0.7.0 item 212). Every coordinate on the network is a classic one, as in the
 * original: a game in Widescreen mode sends classic values and widens what it gets.
 *
 * The fault. display.c widens each area and widget from 800x600 to the screen when the game creates it (the wraps of
 * area create 0x55fba0 and widget create 0x55f990). The host builds a joiner's screen with the same functions, so
 * a host in Widescreen mode stored and sent widened values, and the joiner widened them again when its packet applier
 * Display::process_command_packet 0x564080 created them (areas at 0x564c31, widgets at 0x565c11). A widescreen joiner
 * drew the screen 1.6 times too far right, a classic joiner drew 1280-wide values on 800x600. With a classic host
 * both worked, except the joiner's red title bar (widget changes on the joiner, below).
 *
 * Where coordinates cross the network, and the change at each (classic mode: every function here gives its value
 * back, ds_screen and ds_classic are the identity at 800x600):
 * - Areas. Area create 0x55fba0 writes each area into the command block at 0xae2908 (x1, y1, x2, y2 at 0x55fc29,
 *   0x55fcb7, 0x55fd45, 0x55fdd3, from its arguments, which display.c has widened) before it builds it (Area::init
 *   0x402ac0 at 0x560109). Display::send_screen_section 0x562370 sends that block as section 2 (0x5627d4..0x5627e9).
 *   tools/recomp/net_scale.py writes the classic values in the block (rt_net_classic_x/y).
 * - Widgets. send_screen_section sends the fields of each widget that differ from the copy it sent last
 *   (Guio::get_fields 0x5ee4a0 at 0x5628e8 on the widget, at 0x5629e0 on the copy; the copy is a raw copy of the
 *   host's widget table, 0x563c7e). Both calls give classic rectangles here (stack arguments 2 to 5, counted from 1:
 *   the out pointers of x1, y1, x2, y2, the widget's +0x10..+0x1c).
 * - Widget moves. ScreenManager::set_widget_rect 0x800890 writes opcode 8 sub-op 1 with x1, y1, x2, y2 (0x800a9c,
 *   0x800b1d, 0x800b9e, 0x800c1f) and applies it through 0x564080 on this computer (0x800c5f, 0x800c8e) or sends it
 *   (0x800cb1). net_scale.py writes classic values, and the applier's sub-op 1 (0x567127: the four values at
 *   esp+0x28, +0x30, +0x38, +0x2c, then 0x567ac0) widens them before it uses them (0x567157), on the host and on the
 *   joiner alike.
 * - Clicks. A joiner's click (Display::send_widget_click 0x568560) sends all fields of the widget, the rectangle too
 *   (get_fields at 0x568713), and of a second widget when there is one (get_fields at 0x569226): a shirt dropped on
 *   the pitch of the tactics screen sends the pitch. The joiner sends classic values, and the host's opcode 6 case
 *   widens them when it builds the widgets again (Guio::init 0x5ee150 at 0x5669be and 0x566fab; x1, y1, x2, y2 are
 *   its stack arguments 5 to 8, counted from 1).
 * - Drags. Between the two widgets the click sends the Display's drag: the dragged widget (+0xba9dc) and where it was
 *   let go, x1, y1, x2, y2 at +0xba9be (0x569114..0x56918b). net_scale.py loads them as classic values
 *   (0x569146..0x56914e), and the host's opcode 6 case widens them where it stores them (0x566d33..0x566d42). The
 *   tactics input measures the drop from it and from the pitch (0x89f710: the drag at 0x8a2099, the target at
 *   0x8a20bb). Without it a widescreen joiner's drag on a classic host left the player where he was, and any joiner's
 *   drag on a widescreen host made him FL (run-logs/i212/drag-*).
 * A host value in Widescreen mode that ds_screen did not make (a widget placed from the width of its text, comp_text
 * 0x4aeaf0) goes over the network rounded to a classic value, so a joiner may draw it up to 2 pixels off.
 * - Widget changes on the joiner. Opcode 1 sends only the fields that changed. The joiner takes the others from its
 *   own widget ([0xae2918] + index * 0x18c, get_fields at 0x564ef8), whose rectangle it has widened, and builds the
 *   widget again with widget create (0x565c11), which widens it once more. That get_fields gives classic values too.
 *   Without it the red title bar, which stays from one screen to the next, was drawn 1.6 times too far right from the
 *   second screen on (run-logs/i212/combo-ww), with a classic host too. */
#include "win.h"
#include "../rt.h"
#include "display_scale.h"

#define CLASSIC_W 800u
#define CLASSIC_H 600u

uint32_t rt_net_classic_x(uint32_t v, int far_edge) { return ds_classic(v, rt_screen_w, CLASSIC_W, far_edge); }
uint32_t rt_net_classic_y(uint32_t v, int far_edge) { return ds_classic(v, rt_screen_h, CLASSIC_H, far_edge); }
uint32_t rt_net_screen_x(uint32_t v, int far_edge) { return ds_screen(v, rt_screen_w, CLASSIC_W, far_edge); }
uint32_t rt_net_screen_y(uint32_t v, int far_edge) { return ds_screen(v, rt_screen_h, CLASSIC_H, far_edge); }

static void widen16(uint32_t at, int y, int far_edge)
{
    uint32_t v = (uint32_t)(int32_t)(int16_t)rd16(g_mem, at);
    v = y ? ds_screen(v, rt_screen_h, CLASSIC_H, far_edge) : ds_screen(v, rt_screen_w, CLASSIC_W, far_edge);
    wr16(g_mem, at, (uint16_t)v);
}

/* 0x567157, opcode 8 sub-op 1: the four words of the move, classic on the network. */
void rt_net_rect_in16(uint32_t x1_at, uint32_t y1_at, uint32_t x2_at, uint32_t y2_at)
{
    widen16(x1_at, 0, 0);
    widen16(y1_at, 1, 0);
    widen16(x2_at, 0, 1);
    widen16(y2_at, 1, 1);
}

void o_005ee4a0(void);
void o_005ee150(void);

/* Guio::get_fields, thiscall (19 out pointers: flags, x1, y1, x2, y2, ...), ret 0x4c. */
RT_WRAP(005ee4a0, net_get_fields)
{
    uint32_t from = rd32(g_mem, cpu.esp), p[4];
    for (int i = 0; i < 4; i++) p[i] = rd32(g_mem, cpu.esp + 8 + 4 * (uint32_t)i);
    o_005ee4a0();
    if (rt_unwinding || rt_screen_w == CLASSIC_W) return;
    if (from != 0x5628edu && from != 0x5629e5u && from != 0x568718u && from != 0x56922bu && from != 0x564efdu) return;
    wr32(g_mem, p[0], rt_net_classic_x(rd32(g_mem, p[0]), 0));
    wr32(g_mem, p[1], rt_net_classic_y(rd32(g_mem, p[1]), 0));
    wr32(g_mem, p[2], rt_net_classic_x(rd32(g_mem, p[2]), 1));
    wr32(g_mem, p[3], rt_net_classic_y(rd32(g_mem, p[3]), 1));
}

/* Guio::init, thiscall (display, widgets, index, flags, x1, y1, x2, y2, ...), ret 0x54. */
RT_WRAP(005ee150, net_widget_init)
{
    uint32_t from = rd32(g_mem, cpu.esp);
    if (rt_screen_w != CLASSIC_W && (from == 0x5669c3u || from == 0x566fb0u)) {
        uint32_t a = cpu.esp + 4 + 4 * 4;
        wr32(g_mem, a, ds_screen(rd32(g_mem, a), rt_screen_w, CLASSIC_W, 0));
        wr32(g_mem, a + 4, ds_screen(rd32(g_mem, a + 4), rt_screen_h, CLASSIC_H, 0));
        wr32(g_mem, a + 8, ds_screen(rd32(g_mem, a + 8), rt_screen_w, CLASSIC_W, 1));
        wr32(g_mem, a + 12, ds_screen(rd32(g_mem, a + 12), rt_screen_h, CLASSIC_H, 1));
    }
    o_005ee150();
}
