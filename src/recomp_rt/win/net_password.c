/* The joiner's password box (0.7.0 slice 6, Q38, docs/specs/2026-10-27-online-play-0.7.0-spec.md).
 *
 * The host's password is game_password on the Network tab of the Options box (options_screen.c). The join check
 * of slice 4 carries it (net_core.h, BOSMAN HELLO): a joiner with no password or a wrong one gets
 * "BOSMAN PASSWORD", and its connect fails. The original then shows "Unable To Join" (setup_create_or_join_
 * network_game 0x8388e0: confirm_push 0x591840 at 0x838a67, whose result it does not read: it returns -4 at
 * 0x838a6f). For that refusal the wrap of 0x591840 (net_lobby.c) opens this box in its place, a screen of the
 * runtime pushed with flag 1 as Game Settings is (0x8238bb), so the game draws it over the game list.
 *
 * The box has one text field that hides its text (Display::add_item 0x55f990 with type 0x18 and text flags 0x8c,
 * as the original's password box at 0x822f56..0x822f9f), and Cancel and Join. The field writes into a buffer of
 * the runtime, as the game name field writes into 0xdd70dc (0x81f243..0x81f287). Join sets the password of the join
 * check and closes the box (-9). The screen loop then calls the game list's input with no widget (-1). For that
 * call the wrap of setup_handle_select_network_game_input 0x81fbd0 sets the command word 0xdd6bde to -3, the id of
 * Next: the input takes the word when no widget is clicked (0x81fd8f..0x81fd96), and Next joins the game named in
 * the Join field 0xdd70dc (0x81fd99..0x81fdc4). So the join runs again through the original's code, with the
 * password. A wrong password opens the box again, with a line that says so. */
#include "win.h"
#include "invite.h"
#include "net_core.h"

#include <stdio.h>
#include <string.h>

#define SCREENS 0xb74450u
#define PUSH 0x7fe600u              /* thiscall on S (build, input, flag, leave, exit) */
#define BACKGROUND_OFF 0x41c0d0u
#define COLOUR 0x5e4800u
#define BOX_TEXT 0x803fd0u
#define AREA_CREATE 0x55fba0u
#define WIDGET_CREATE 0x55f990u
#define CELL_BUTTON 0x8040a0u
#define DISPLAYS 0xb74c4cu
#define PLAYER_SLOT 0xb77c7au
#define WIDGETS 0xba95eu
#define WIDGET_SIZE 0x18cu
#define TEXT_TITLE 0xaebdeau
#define TEXT_LIGHT 0xae3154u
#define FIELD_COLOUR 0xaebe04u      /* the field colour of 0x822f5f */
#define BUTTON_COLOUR 0xae317eu
#define BUTTON_TEXT 0xae3184u
#define COMMAND 0xdd6bdeu           /* s16: the command that an input takes when no widget is clicked */
#define NEXT_ID 0xfffdu             /* Next (-3) of the game list */
#define JOIN_NAME 0xdd70dcu         /* the Join field's text (0x81fda3) */

#define FIELD_ID 0x7d62u            /* ids that the original does not use */
#define CANCEL_ID 0x7d63u
#define JOIN_ID 0x7d64u
#define CLOSE 0xfffffff7u           /* -9 */
#define PASSWORD_MAX 40

static uint32_t scratch, field;     /* game memory: a text, and the field's buffer */
static int wrong;                   /* the box opens again after a wrong password */
static int typed;                   /* Join was clicked since the last Cancel or the last join that passed */
static int retry;                   /* Join was clicked: the game list joins again */
static int command_set;             /* the command word holds Next until the join's connect */
static uint16_t command_before;

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t call(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t display(void)
{
    return rd32(g_mem, DISPLAYS + (uint32_t)(int32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
}

static uint32_t text(const char *s)
{
    win_put_str(scratch, s, 0x100);
    return scratch;
}

static void box_text(int x1, int y1, int x2, int y2, uint32_t style, uint32_t colour, uint32_t flags, uint32_t font,
                     uint32_t text_colour, const char *s)
{
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, style, colour, flags, font,
                      text_colour, text(s), 1};
    call_this(BOX_TEXT, SCREENS, 11, a);
}

static int area(int x1, int y1, int x2, int y2, int columns, uint32_t flags, uint32_t colour)
{
    uint32_t a[11] = {(uint32_t)x1, (uint32_t)y1, (uint32_t)x2, (uint32_t)y2, (uint32_t)columns, 0, 1, 0, flags,
                      colour, 0xffffffffu};
    return (int16_t)call_this(AREA_CREATE, display(), 11, a);
}

static void button(int a, int column, const char *s, uint32_t id)
{
    uint32_t b[11] = {(uint32_t)a, (uint32_t)column, 0, 0x30, rd16(g_mem, BUTTON_COLOUR), 0xc, 3,
                      rd16(g_mem, BUTTON_TEXT), text(s), id, 0};
    call_this(CELL_BUTTON, SCREENS, 11, b);
}

/* The build, cdecl, no arguments: the frame of the Options box (options_screen.c), smaller. */
static void password_build(void)
{
    area(0, 0, 0x31f, 0x257, 1, 2, 0);                         /* the dark cover, as 0x823a92 */
    uint32_t off[1] = {0};
    call(BACKGROUND_OFF, 1, off);
    uint32_t c[4] = {0, 0x38, 0, 0};
    uint32_t box_colour = call(COLOUR, 4, c) & 0xffff;
    uint32_t light = rd16(g_mem, TEXT_LIGHT);
    area(195, 170, 694, 429, 1, 0x30, box_colour);
    box_text(205, 180, 684, 220, 0x30, rd16(g_mem, TEXT_TITLE), 0xc, 4, rd16(g_mem, BUTTON_TEXT), "Game Password");
    box_text(215, 230, 674, 252, 1, 0, 0xc, 2, light,
             wrong ? "The password is wrong. Type it again." : "This network game has a password.");
    uint32_t w[18] = {0x18, 215, 266, 674, 316, 0, 0, 0x22, rd16(g_mem, FIELD_COLOUR), 0, 0x8c, 4,
                      rd16(g_mem, BUTTON_TEXT), field, PASSWORD_MAX, FIELD_ID, 0, 0xffffffffu};
    call_this(WIDGET_CREATE, display(), 18, w);
    int b = area(205, 382, 684, 417, 2, 1, 0);
    button(b, 0, "Cancel", CANCEL_ID);
    button(b, 1, "Join", JOIN_ID);
    fprintf(stderr, "recomp: password box built%s\n", wrong ? " (wrong password)" : "");
    rt_ret_cdecl(0);
}

/* The input, cdecl (s16 widget). A click that ends the text entry gives the field, then Join in the next poll of the
 * same press (research note, section 11): both count. */
static void password_input(void)
{
    int16_t w = (int16_t)rt_arg(0);
    uint32_t r = 0;
    if (w >= 0) {
        uint32_t id = (uint16_t)rd32(g_mem, display() + WIDGETS + 8 + (uint32_t)w * WIDGET_SIZE);
        if (id == CANCEL_ID) {
            fprintf(stderr, "recomp: password box: Cancel\n");
            retry = 0;
            nc_set_join_password("");
            typed = 0;
            r = CLOSE;
        } else if (id == JOIN_ID) {
            char typed_text[PASSWORD_MAX + 1];
            snprintf(typed_text, sizeof typed_text, "%s", (const char *)g_mem + field);
            nc_set_join_password(typed_text);
            typed = 1;
            retry = 1;
            fprintf(stderr, "recomp: password box: Join\n");
            r = CLOSE;
        }
    }
    rt_ret_cdecl(r);
}

int net_password_typed(void) { return typed; }
/* After a join that passed, or a refusal for another reason: the next host must not get this password. */
void net_password_reset(void)
{
    typed = 0;
    nc_set_join_password("");
}

int net_password_ask(int was_wrong)
{
    if (!scratch) {
        scratch = win_static_alloc(0x100);
        field = win_static_alloc(PASSWORD_MAX + 8);
    }
    wrong = was_wrong;
    memset(g_mem + field, 0, PASSWORD_MAX + 8);
    uint32_t p[5] = {rt_host_fn("password_build", password_build), rt_host_fn("password_input", password_input), 1,
                     0, 0};
    if (!call_this(PUSH, SCREENS, 5, p)) return 0;
    fprintf(stderr, "recomp: password box opened\n");
    return 1;
}

void invite_call(void (*original)(void), uint16_t command)
{
    uint16_t before = rd16(g_mem, COMMAND);
    wr16(g_mem, COMMAND, command);
    original();
    wr16(g_mem, COMMAND, before);
}

void o_0081fbd0(void);

/* setup_handle_select_network_game_input, cdecl (s16 widget): see the head of this file. */
RT_WRAP(0081fbd0, select_network_game_input)
{
    /* An invite link (invite.c): its game's row text in the Join field, then Next as for a join again. */
    if (!retry && (int16_t)rt_arg(0) == -1 && invite_list_join((char *)g_mem + JOIN_NAME, 50)) retry = 1;
    if ((int16_t)rt_arg(0) >= 0) invite_clicked();
    if (!retry || (int16_t)rt_arg(0) != -1) {
        o_0081fbd0();
        return;
    }
    /* The input can return -11 before it reads the command word while the list's count changes (0x81fc58..
     * 0x81fc7a), so the retry stays until the join's connect, or until the Join field is empty (0x81fdb2). */
    command_before = rd16(g_mem, COMMAND);
    command_set = 1;
    wr16(g_mem, COMMAND, NEXT_ID);
    o_0081fbd0();
    if (command_set) {             /* no connect in this call */
        command_set = 0;
        wr16(g_mem, COMMAND, command_before);
        if (!g_mem[JOIN_NAME]) retry = 0;
    }
}

/* The join's connect (net_lobby.c, before_connect): the joiner's whole session runs inside the input call, so the
 * command word gets its old value back here. */
void net_password_connecting(void)
{
    if (!command_set) return;
    command_set = 0;
    retry = 0;
    wr16(g_mem, COMMAND, command_before);
    fprintf(stderr, "recomp: password box: the join runs again\n");
}
