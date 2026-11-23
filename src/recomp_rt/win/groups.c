/* Attribute groups (#43): the player profile shows its attributes in Technical, Mental and Physical
 * columns with heading rows, and one Goalkeeping group in place of Technical for a goalkeeper (spec
 * 2026-10-15-features-0.4.0, Q122 to Q129; layout in groups_layout.c; evidence in
 * docs/research/2026-10-15-attribute-groups.md).
 *
 * The profile build 0x87a510 fills a table of 36 cells of 0x65 bytes at 0xddf570 (a name of 100 bytes,
 * then a value byte), sorts the 31 attributes by name and adds Preferred Foot, Form, Morale, Condition and
 * On Loan. It then shares the cells out over three columns and draws them in table order. With the option
 * on, three constants of the build (tools/recomp/attribute_groups.py) point it at a table of the runtime:
 * - rt_groups_cells copies the cells of the game's table in group order, with heading and blank cells,
 *   and gives their count: 13 rows in each column;
 * - rt_groups_table gives the first cell of that table, rt_groups_count the count again.
 * The area create of the build (call site profile_area) gets 13 rows. A heading or blank
 * cell has a name that starts with byte 1: the name label (call site profile_name) drops the byte and
 * draws a heading in yellow, and the value label after it gets an empty text.
 *
 * A goalkeeper also gets One On Ones and Consistency, two attributes of the player data that the original
 * does not show (Max, 2026-10-21; evidence in docs/research/2026-10-21-goalkeeper-attributes.md). Their
 * cells take the place of blank cells, so the profile makes the same labels.
 *
 * The runtime only reads the game's data and copies the game's own cells, so no value and no game state
 * changes. With the option off the constants keep their values. */
#include "win.h"
#include "options.h"
#include "groups_layout.h"

#include <string.h>

extern const uint32_t rt_site_profile_area;
extern const uint32_t rt_site_profile_name;
extern const uint32_t rt_site_profile_value;

#define TABLE 0xddf570u          /* the game's cells: 31 sorted attributes, then the five rows */
#define CELL 0x65u               /* a name of 100 bytes, then the value byte */
#define VALUE 0x64u
#define CELLS 36u
#define ATTRIBUTE_NAME 0x5428a0u /* cdecl (attribute 1..31, text, size, 0): the attribute's name */
#define KNOWN 0x543280u       /* cdecl (viewer, staff, attribute): 1 when the viewer knows the attribute */
#define RATING 0x544380u      /* cdecl (player data, byte): a relative attribute as a 1 to 20 rating */
#define MASKED 0xf9u          /* the value of an attribute that the viewer does not know (0x5434c6) */
#define YELLOW 0xaebdecu         /* the colour global of the values and the "Born" line */
#define MARK 1                   /* first byte of the name of a heading or blank cell */

/* Scratch text: the end of the game's text buffer 0xde1f64, as in columns.c (other bytes). */
#define SCRATCH (0xde1f64u + 1400u)

static struct {
    int widened;                 /* the area of the last profile build got 13 rows */
    uint32_t table;              /* the runtime's table, GROUP_COLUMNS * GROUP_ROWS cells */
    uint32_t first;              /* the value byte of the first cell of the table in use */
    uint32_t count;              /* the cells in use */
    int blank_value;             /* the next value label of the profile gets an empty text */
} groups = {0, 0, TABLE + VALUE, CELLS, 0};

static uint32_t call_cdecl(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

/* 1 when the player data is a goalkeeper's, from his position ratings (groups_is_keeper). The test reads the
 * data and not the positions text, which is in the chosen language: "GK" is English only (0.6.1 item 8). */
static int is_keeper(uint32_t data)
{
    if (!data) return 0;
    signed char ratings[GROUP_RATINGS];
    for (int i = 0; i < GROUP_RATINGS; i++) ratings[i] = (signed char)rd8(g_mem, data + 0x0f + (uint32_t)i);
    return groups_is_keeper(ratings);
}

/* The game's cell of attribute k (1 to 31) by its name, or 0. */
static uint32_t attribute_cell(int k)
{
    char name[CELL];
    uint32_t a[4] = {(uint32_t)k, SCRATCH, 100, 0};
    char keep[100];
    memcpy(keep, g_mem + SCRATCH, sizeof keep);
    call_cdecl(ATTRIBUTE_NAME, 4, a);
    snprintf(name, sizeof name, "%s", win_str(SCRATCH));
    memcpy(g_mem + SCRATCH, keep, sizeof keep);
    for (uint32_t i = 0; i < 31; i++)
        if (!strcmp(win_str(TABLE + i * CELL), name)) return TABLE + i * CELL;
    return 0;
}

/* The cell of a hidden attribute of the player data (CELL_ONE_ON_ONES or CELL_CONSISTENCY) at to, with its
 * value as the profile's getter 0x543440 gives the others: 0xf9 when the viewer does not know it.
 * One On Ones (+0x35) is stored relative to current ability, as Handling and Passing: the game's own
 * conversion 0x544380 makes the rating. Consistency (+0x21) is stored as the rating. The knowledge
 * check 0x543280 takes the attribute number only for its pattern of known attributes, so the two cells
 * get the numbers 32 and 33 that follow the game's 31. */
static void hidden_cell(uint32_t to, int k, uint32_t viewer, uint32_t staff)
{
    uint32_t data = rd32(g_mem, staff + 0x61);
    memset(g_mem + to, 0, CELL);
    win_put_str(to, groups_hidden_name(k), VALUE - 1);
    uint32_t known[3] = {viewer, staff, k == CELL_ONE_ON_ONES ? 32u : 33u};
    uint8_t value;
    if (!data) value = 0;
    else if (!(call_cdecl(KNOWN, 3, known) & 0xff)) value = MASKED;
    else if (k == CELL_ONE_ON_ONES) {
        uint32_t a[2] = {data, (uint32_t)(int32_t)(int8_t)rd8(g_mem, data + 0x35)};
        value = (uint8_t)call_cdecl(RATING, 2, a);
    } else value = rd8(g_mem, data + 0x21);
    wr8(g_mem, to + VALUE, value);
}

/* Called by the build 0x87a510 at 0x87a81e, when the game's table is full. esp is the build's stack
 * pointer: [esp+0x20] is the staff record and [esp+0x70] the viewer that the build gives 0x543440
 * (0x87a74a..0x87a754, one push deeper). Returns the count of cells. */
uint32_t rt_groups_cells(uint32_t esp)
{
    groups.first = TABLE + VALUE;
    groups.count = CELLS;
    groups.blank_value = 0;
    if (!options_bool(OPT_ATTRIBUTE_GROUPS) || !groups.widened) return groups.count;
    groups.widened = 0;
    if (!groups.table) groups.table = win_static_alloc(GROUP_COLUMNS * GROUP_ROWS * CELL);
    /* The generated code keeps its registers in locals: the calls of the game's functions below push
     * below the build's own stack. */
    uint32_t keep_esp = cpu.esp;
    cpu.esp = esp - 64;
    uint32_t staff = rd32(g_mem, esp + 0x20), viewer = rd32(g_mem, esp + 0x70);
    GroupLayout l;
    groups_layout(staff ? is_keeper(rd32(g_mem, staff + 0x61)) : 0, &l);
    uint32_t to = groups.table;
    for (int c = 0; c < GROUP_COLUMNS; c++)
        for (int r = 0; r < GROUP_ROWS; r++, to += CELL) {
            int k = l.cell[c][r];
            uint32_t from = k >= 1 && k <= 31 ? attribute_cell(k)
                : k >= CELL_FOOT && k <= CELL_ON_LOAN ? TABLE + (uint32_t)(31 + k - CELL_FOOT) * CELL : 0;
            if (from) {
                memcpy(g_mem + to, g_mem + from, CELL);
                continue;
            }
            if (groups_hidden_name(k) && staff) {
                hidden_cell(to, k, viewer, staff);
                continue;
            }
            const char *h = groups_heading(k);
            memset(g_mem + to, 0, CELL);
            wr8(g_mem, to, MARK);
            if (h) win_put_str(to + 1, h, VALUE - 2);
        }
    cpu.esp = keep_esp;
    groups.first = groups.table + VALUE;
    groups.count = GROUP_COLUMNS * GROUP_ROWS;
    return groups.count;
}

uint32_t rt_groups_table(void) { return groups.first; }
uint32_t rt_groups_count(void) { return groups.count; }

/* From the area create wrap (display.c): the attribute area of the profile gets 13 rows, in classic
 * mode too (Q125: 13 rows of 17 pixels, the font's rows are 18, and no text clips on the screenshots). */
void groups_area_before(void)
{
    if (rd32(g_mem, cpu.esp) != rt_site_profile_area) return;
    groups.widened = options_bool(OPT_ATTRIBUTE_GROUPS);
    if (groups.widened && (rt_arg(6) & 0xff) < GROUP_ROWS) wr32(g_mem, cpu.esp + 4 + 4 * 6, GROUP_ROWS);
}

/* From the label create wrap (columns.c), before the colours: the name and value labels of a heading
 * or blank cell. */
void groups_label_before(void)
{
    uint32_t ret = rd32(g_mem, cpu.esp), text = rt_arg(8);
    if (ret == rt_site_profile_name) {
        uint32_t m = text;   /* the build prints the name as "  %s" (format 0x987ff0) */
        while (m && rd8(g_mem, m) == ' ') m++;
        groups.blank_value = m && rd8(g_mem, m) == MARK;
        if (groups.blank_value) {
            memmove(g_mem + m, g_mem + m + 1, strlen(win_str(m + 1)) + 1);
            if (rd8(g_mem, m)) wr32(g_mem, cpu.esp + 4 + 4 * 7, rd16(g_mem, YELLOW));
        }
    } else if (ret == rt_site_profile_value && groups.blank_value) {
        if (text) wr8(g_mem, text, 0);
        groups.blank_value = 0;
    }
}
