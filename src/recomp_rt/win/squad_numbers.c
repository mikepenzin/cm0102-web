/* Squad numbers 1 to 99 (GitHub issue 140, 0.7.3 slice 13). The patches are in tools/recomp/squad_numbers.py.
 *
 * A squad number is the signed byte +0x3a of a contract: positive at a human club, negative at another (0x85a639).
 * The option squad_numbers_99 (off by default) decides which NEW numbers the game gives:
 * - a new game keeps database numbers up to 99 in place of cutting them to 50, in the contract build (0x4c78bc..0x4c78c5)
 *   and in the loader (0x859f6b..0x859f6f);
 * - allocate_squad_number 0x85a470 gives 51 to 99 when 1 to 50 are all taken;
 * - a transfer (0x8ca7c7) carries a number above 50 to the new club only with the option on.
 *
 * The two tables of the original hold 50 numbers and have no bounds check, so they widen in both modes. A save made
 * with the option on must load with it off: the game keeps those numbers and gives no new ones above 50.
 * - allocate_squad_number 0x85a470: 50 flag bytes on the stack, written at 0x85a500. Number 53 reaches the pointer list
 *   at [esp + 0x50] that the same loop goes on to read.
 * - assign_club_squad_numbers 0x859fe0: int[50] at [esp + 0x110], read at 0x85a293 and written at 0x85a328. Number 51
 *   reaches the squad list at [esp + 0x1d8].
 * Numbers 1 to 50 stay in the original's stack tables, so a game with no higher number runs as the original. Numbers
 * 51 to 99 go in the tables below. Neither function calls itself. assign calls allocate at 0x85a3de, and each has its
 * own table.
 *
 * Network games: a joiner runs only ScreenManager::run_network_client 0x7fdc00, so these rules run on the host and
 * use the host's setting.
 *
 * Test switch RECOMP_TEST_SQUAD_NUMBERS=1: the checks at the end of this file run at the first human turn.
 * =screen: the squad numbers screen logs its buttons and each number above 50 with its holder. */
#include "win.h"
#include "options.h"
#include "../rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LOW_TOP = 50, HIGH_TOP = 99, HIGH_COUNT = HIGH_TOP - LOW_TOP };

#define LINKED_CLUB 0x00540a50u         /* cdecl (club, 0, 1): the club's linked club, or 0 */
#define STAFF_COUNT 0xae2364u
#define STAFF_ROWS 0xae2d0cu            /* 79-byte rows: +0 the main contract, +4 the loan contract, -1 for none */
#define CONTRACTS 0xae1050u             /* [this] is the table of 80-byte contracts: +4 the club id, +0x3a the number */

static uint32_t assign_high[HIGH_COUNT];   /* the holder of each number 51 to 99 in assign_club_squad_numbers */
static uint8_t allocate_high[HIGH_COUNT];  /* 1 when allocate_squad_number found the number 51 to 99 in the squad */

static int is_high(int32_t n) { return n > LOW_TOP && n <= HIGH_TOP; }

/* The highest number that the game gives or keeps from the database. */
uint32_t rt_squad_number_top(void)
{
    return options_bool(OPT_SQUAD_NUMBERS_99) ? HIGH_TOP : LOW_TOP;
}

/* assign_club_squad_numbers 0x859fe0: its start, where the original clears its two tables (0x859fee). */
void rt_squad_assign_begin(void)
{
    memset(assign_high, 0, sizeof assign_high);
}

/* 0x85a293: the holder of number n. A number outside 1 to 99 counts as taken, so 0x85a2a5 clears it. */
uint32_t rt_squad_assign_taken(uint32_t esp, uint32_t number)
{
    int32_t n = (int32_t)number;
    if (n >= 1 && n <= LOW_TOP) return rd32(g_mem, esp + (uint32_t)n * 4u + 0x10cu);
    return is_high(n) ? assign_high[n - LOW_TOP - 1] : 1u;
}

/* 0x85a29a: the address of the slot of number n in the original's table, 0 for a number above 50. */
uint32_t rt_squad_assign_slot(uint32_t esp, uint32_t number)
{
    int32_t n = (int32_t)number;
    return n >= 1 && n <= LOW_TOP ? esp + (uint32_t)n * 4u + 0x10cu : 0u;
}

/* 0x85a328: the player holds the number kept in bl. */
void rt_squad_assign_keep(uint32_t slot, uint32_t ebx, uint32_t player)
{
    int32_t n = (int8_t)ebx;
    if (slot) wr32(g_mem, slot, player);
    else if (is_high(n)) assign_high[n - LOW_TOP - 1] = player;
}

/* 0x85a40b and 0x85a440: the holder of number n in the last loops, which the patches run on to 99. */
uint32_t rt_squad_assign_holder(uint32_t slot, int32_t n)
{
    return n <= LOW_TOP ? rd32(g_mem, slot) : assign_high[n - LOW_TOP - 1];
}

/* allocate_squad_number 0x85a470: its start, where the original clears its flags (0x85a48f). */
void rt_squad_allocate_begin(void)
{
    memset(allocate_high, 0, sizeof allocate_high);
}

/* 0x85a500: 1 when the number is not one of the original's 50 flags, so the stack stays as it is. */
uint32_t rt_squad_allocate_mark(uint32_t number)
{
    int32_t n = (int32_t)number;
    if (n >= 1 && n <= LOW_TOP) return 0;
    if (is_high(n)) allocate_high[n - LOW_TOP - 1] = 1;
    return 1;
}

/* Marks the numbers above 50 of every contract at the club or its linked club. The list of 0x541f90 holds at most 50
 * players (0x54201c), and they hold 1 to 50 here, so a number above 50 belongs to a player outside the list. */
static void mark_contracts(uint32_t club)
{
    uint32_t args[3] = {club, 0, 1};
    Cpu saved = cpu;
    uint32_t linked = rt_callback(LINKED_CLUB, 3, args);     /* as 0x85a7c4 */
    cpu = saved;
    int32_t id = (int32_t)rd32(g_mem, club), other = linked ? (int32_t)rd32(g_mem, linked) : -1;
    uint32_t rows = rd32(g_mem, STAFF_ROWS), table = rd32(g_mem, rd32(g_mem, CONTRACTS));
    uint32_t count = rd32(g_mem, STAFF_COUNT);
    for (uint32_t s = 0; s < count; s++) {
        for (uint32_t k = 0; k < 2; k++) {                    /* +0 the main contract, +4 the loan contract */
            int32_t c = (int32_t)rd32(g_mem, rows + s * 79u + k * 4u);
            if (c < 0) continue;
            uint32_t contract = table + (uint32_t)c * 80u;
            int32_t at = (int32_t)rd32(g_mem, contract + 4u);
            if (at != id && at != other) continue;
            int32_t n = abs((int8_t)g_mem[contract + 0x3au]);
            if (is_high(n)) allocate_high[n - LOW_TOP - 1] = 1;
        }
    }
}

/* 0x85a6f3, when 1 to 50 are all taken: the first free number above 50, 0 for none or with the option off. */
uint32_t rt_squad_allocate_high(uint32_t club)
{
    if (rt_squad_number_top() <= LOW_TOP) return 0;
    mark_contracts(club);
    for (int i = 0; i < HIGH_COUNT; i++)
        if (!allocate_high[i]) return (uint32_t)(LOW_TOP + 1 + i);
    return 0;
}

/* 0x8ca7c7: the number that a moving player takes to his new club (saved at 0x8c9dfe and 0x8c9f1c). With the option
 * off a number above 50 stays behind, because the game gives no new number above 50. He has no number at the new club,
 * as a player who had none before the move (0x8c9782). */
uint32_t rt_squad_carried(uint32_t number)
{
    return abs((int8_t)number) > (int32_t)rt_squad_number_top() ? number & 0xffffff00u : number;
}

/* The squad numbers screen, squad_numbers_screen_build 0x475520. The original draws two strips of 25 number buttons in
 * classic positions: y 210 to 230 above the player list and y 515 to 535 below it (0x477640..0x4776a7). With the option
 * on each strip has two rows: 1 to 50 above the list and 51 to 99 below it. */
enum {
    STRIP_LEFT = 0x6e, STRIP_RIGHT = 0x30c, STRIP_COLUMNS = 25, STRIP_ROWS = 2,
    TOP_STRIP = 210, BOTTOM_STRIP_END = 535, STRIP_HEIGHT = 42, STRIP_GAP = 5,
    LIST_TOP = TOP_STRIP + STRIP_HEIGHT + STRIP_GAP,
    LIST_BOTTOM = BOTTOM_STRIP_END - STRIP_HEIGHT - STRIP_GAP
};

#define SCREEN_CHANNEL 0xb77c7au        /* s16: the channel of the screen manager */
#define CHANNEL_DISPLAY 0xb74c4cu       /* + channel * 0x300: the Display */
#define AREA_CREATE 0x0055fba0u         /* thiscall on a Display, 11 args */
#define WIDGET_CREATE 0x0055f990u       /* thiscall on a Display, 18 args */
#define TEXT_BUFFER 0xde1f64u           /* the buffer of the number's text (0x477727) */
#define COLOUR_BACK 0xae3150u
#define COLOUR_FREE 0xae3184u
#define COLOUR_HELD 0xaebdecu
#define NUMBER_ACTION 5u                /* the button's action, read at 0x47b627 */

uint32_t rt_squad_screen_list_top(void) { return rt_squad_number_top() > LOW_TOP ? LIST_TOP : 0xebu; }
uint32_t rt_squad_screen_list_bottom(void) { return rt_squad_number_top() > LOW_TOP ? LIST_BOTTOM : 0x1feu; }

static uint32_t screen_display(void)
{
    return rd32(g_mem, CHANNEL_DISPLAY + (uint32_t)((int32_t)(int16_t)rd16(g_mem, SCREEN_CHANNEL) * 0x300));
}

static uint32_t display_call(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = screen_display();
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t strip_area(uint32_t top)
{
    uint32_t a[11] = {STRIP_LEFT, top, STRIP_RIGHT, top + STRIP_HEIGHT, STRIP_COLUMNS, 0, STRIP_ROWS, 0, 1, 0,
                      0xffffffffu};
    return (uint32_t)(int32_t)(int16_t)display_call(AREA_CREATE, 11, a);
}

/* The holder of the number in the screen's list of 50 players, as 0x4776cb..0x47771f: 0 for none. */
static uint32_t screen_holder(uint32_t club, uint32_t list, int number)
{
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t person = rd32(g_mem, list + i * 4u);
        if (!person || (int32_t)rd32(g_mem, person) >= (int32_t)rd32(g_mem, STAFF_COUNT)) continue;
        uint32_t a[2] = {person, club};
        Cpu saved = cpu;
        cpu.ecx = rd32(g_mem, 0xdd7ef0u);
        int n = (int8_t)rt_callback(0x0085a760u, 2, a);
        cpu = saved;
        if (abs(n) == number) return person;
    }
    return 0;
}

/* One number button, with the three looks of 0x477738, 0x477791 and 0x4777c3. */
static void strip_button(uint32_t area, int column, int row, int number, uint32_t club, uint32_t holder)
{
    uint32_t back = rd16(g_mem, COLOUR_BACK);
    snprintf((char *)g_mem + TEXT_BUFFER, 8, "%d", number);
    uint32_t w[18] = {0x46, 0, 0, 0, 0, (uint32_t)column, (uint32_t)row, 0x30, back, back, 0xc, 1,
                      rd16(g_mem, COLOUR_FREE), TEXT_BUFFER, 0, NUMBER_ACTION, (uint32_t)number, area};
    if (holder) {
        uint32_t a[2] = {holder, club};
        Cpu saved = cpu;
        cpu.ecx = rd32(g_mem, 0xdd7ef0u);
        int held = (int8_t)rt_callback(0x0085a760u, 2, a);
        cpu = saved;
        w[7] = 0x70;
        w[12] = rd16(g_mem, COLOUR_HELD);
        if (held <= 0) {                /* a number that the player cannot change: no action */
            w[0] = 1;
            w[10] = 0x2c;
            w[15] = 0;
            w[16] = 0;
        }
    }
    display_call(WIDGET_CREATE, 18, w);
}

/* 0x477640: draws both strips with the option on and returns the lower area. 0x80000000: the original draws them. */
uint32_t rt_squad_screen_strips(uint32_t club, uint32_t list)
{
    if (rt_squad_number_top() <= LOW_TOP) return 0x80000000u;
    const char *test = getenv("RECOMP_TEST_SQUAD_NUMBERS");
    int log_rows = test && !strcmp(test, "screen");
    uint32_t upper = strip_area(TOP_STRIP), lower = strip_area(BOTTOM_STRIP_END - STRIP_HEIGHT);
    for (int number = 1; number <= HIGH_TOP; number++) {
        int index = (number - 1) % LOW_TOP;
        uint32_t holder = screen_holder(club, list, number);
        strip_button(number <= LOW_TOP ? upper : lower, index % STRIP_COLUMNS, index / STRIP_COLUMNS, number, club, holder);
        if (log_rows && holder && number > LOW_TOP)
            fprintf(stderr, "recomp: squad numbers screen: number %d has staff %u\n", number, rd32(g_mem, holder));
    }
    if (log_rows) fprintf(stderr, "recomp: squad numbers screen: %d buttons\n", HIGH_TOP);
    return lower;
}

/* The checks of RECOMP_TEST_SQUAD_NUMBERS. They change the squads of the game, and the game stops after them. */
#define SQUAD_MANAGER 0xdd7ef0u
#define GET_NUMBER 0x0085a760u          /* thiscall (person, club) */
#define SET_NUMBER 0x0085a850u          /* thiscall (person, club, number) */
#define ALLOCATE 0x0085a470u            /* thiscall (club, player) */
#define ASSIGN 0x00859fe0u              /* thiscall (club, keep numbers) */
#define IS_HUMAN_CLUB 0x00600a50u       /* cdecl (club, 0, 1, 0, 0), as 0x85a3f6 */
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_SQUAD 0xd7u                /* 50 staff pointers (0x85a028) */

static int test_failures;
static int screen_pending;      /* RECOMP_TEST_SQUAD_NUMBERS=screen: the screen waits for the first news screen */
static uint32_t watch_person, watch_club;   /* the player whose number the test reads on the open screen */

static uint32_t manager_call(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = rd32(g_mem, SQUAD_MANAGER);
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static int number_of(uint32_t person, uint32_t club)
{
    uint32_t a[2] = {person, club};
    return (int8_t)manager_call(GET_NUMBER, 2, a);
}

static void set_number(uint32_t person, uint32_t club, int n)
{
    uint32_t a[3] = {person, club, (uint32_t)n};
    manager_call(SET_NUMBER, 3, a);
}

static void test_line(int ok, const char *name, int value, int want)
{
    test_failures += !ok;
    fprintf(stderr, "recomp: squad numbers test: %s %s (%d, want %d)\n", ok ? "ok" : "FAIL", name, value, want);
}

static uint32_t squad_slot(uint32_t club, int i)
{
    return rd32(g_mem, club + CLUB_SQUAD + (uint32_t)i * 4u);
}

static int is_player(uint32_t person)
{
    return person && rd32(g_mem, person) < rd32(g_mem, STAFF_COUNT) && rd32(g_mem, person + 0x61u);
}

static int squad_size(uint32_t club)
{
    int n = 0;
    for (int i = 0; i < 50; i++) n += is_player(squad_slot(club, i));
    return n;
}

static int human_club(uint32_t club)
{
    uint32_t a[5] = {club, 0, 1, 0, 0};
    Cpu saved = cpu;
    uint32_t r = rt_callback(IS_HUMAN_CLUB, 5, a);
    cpu = saved;
    return r != 0;
}

/* The first club of the wanted kind with at least 15 players, 0 for none. */
static uint32_t find_club(int human)
{
    uint32_t clubs = rd32(g_mem, CLUBS), count = rd32(g_mem, CLUB_COUNT);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t club = clubs + i * CLUB_SIZE;
        if (squad_size(club) >= 15 && human_club(club) == human) return club;
    }
    return 0;
}

/* Moves a player of another club into the club: his contract and an empty squad slot. */
static int take_player(uint32_t club, uint32_t person)
{
    int32_t index = (int32_t)rd32(g_mem, rd32(g_mem, STAFF_ROWS) + rd32(g_mem, person) * 79u);
    if (index < 0) return 0;
    wr32(g_mem, rd32(g_mem, rd32(g_mem, CONTRACTS)) + (uint32_t)index * 80u + 4u, rd32(g_mem, club));
    wr32(g_mem, person + 0x39u, club);
    return 1;
}

/* Fills the club's 50 slots from other clubs, and gives it two more players who are outside the list. */
static int fill_squad(uint32_t club, uint32_t extra[2])
{
    uint32_t clubs = rd32(g_mem, CLUBS), count = rd32(g_mem, CLUB_COUNT);
    int slot = 0, extras = 0;
    for (uint32_t c = 0; c < count && extras < 2; c++) {
        uint32_t other = clubs + c * CLUB_SIZE;
        if (other == club) continue;
        for (int i = 0; i < 50 && extras < 2; i++) {
            uint32_t person = squad_slot(other, i);
            if (!is_player(person) || !take_player(club, person)) continue;
            while (slot < 50 && is_player(squad_slot(club, slot))) slot++;
            if (slot < 50) wr32(g_mem, club + CLUB_SQUAD + (uint32_t)slot * 4u, person);
            else extra[extras++] = person;
            wr32(g_mem, other + CLUB_SQUAD + (uint32_t)i * 4u, 0);
        }
    }
    return extras;
}

/* High numbers go through both functions and stay, at a club of each kind. */
static void check_kept(uint32_t club, const char *kind)
{
    static const int numbers[] = {53, 60, 75, 99};
    uint32_t players[4] = {0};
    int have = 0;
    char name[80];
    for (int i = 0; i < 50 && have < 4; i++)
        if (is_player(squad_slot(club, i))) players[have++] = squad_slot(club, i);
    for (int i = 0; i < have; i++) set_number(players[i], club, numbers[i]);
    for (int i = 0; i < have; i++) {
        snprintf(name, sizeof name, "%s club: the setter takes %d", kind, numbers[i]);
        test_line(abs(number_of(players[i], club)) == numbers[i], name, number_of(players[i], club), numbers[i]);
    }
    uint32_t a[2] = {club, 1};
    manager_call(ASSIGN, 2, a);
    for (int i = 0; i < have; i++) {
        snprintf(name, sizeof name, "%s club: assign keeps %d", kind, numbers[i]);
        test_line(abs(number_of(players[i], club)) == numbers[i], name, abs(number_of(players[i], club)), numbers[i]);
    }
    int sign = human_club(club) ? 1 : -1;
    snprintf(name, sizeof name, "%s club: the kept number has the sign of the club", kind);
    test_line(number_of(players[3], club) == sign * numbers[3], name, number_of(players[3], club), sign * numbers[3]);
    uint32_t last = players[have - 1];
    set_number(last, club, 0);
    uint32_t b[2] = {club, last};
    manager_call(ALLOCATE, 2, b);
    int got = abs(number_of(last, club));
    snprintf(name, sizeof name, "%s club: allocate gives a free number below 51", kind);
    test_line(got >= 1 && got <= LOW_TOP, name, got, 1);
    for (int i = 0; i < have - 1; i++) {
        snprintf(name, sizeof name, "%s club: allocate leaves %d", kind, numbers[i]);
        test_line(abs(number_of(players[i], club)) == numbers[i], name, abs(number_of(players[i], club)), numbers[i]);
    }
}

/* A signing at a club whose 1 to 50 are all taken. */
static void check_full(uint32_t club)
{
    uint32_t extra[2] = {0, 0};
    int extras = fill_squad(club, extra);
    test_line(extras == 2, "a full squad and two more players", extras, 2);
    if (extras != 2) return;
    int sign = human_club(club) ? 1 : -1, n = 0;
    for (int i = 0; i < 50; i++)
        if (is_player(squad_slot(club, i))) set_number(squad_slot(club, i), club, sign * ++n);
    test_line(n == 50, "the squad holds 1 to 50", n, 50);
    int on = rt_squad_number_top() > LOW_TOP;
    for (int i = 0; i < 2; i++) {
        set_number(extra[i], club, 0);
        uint32_t a[2] = {club, extra[i]};
        manager_call(ALLOCATE, 2, a);
        int want = on ? sign * (LOW_TOP + 1 + i) : 0;
        test_line(number_of(extra[i], club) == want, i ? "the second signing's number" : "the first signing's number",
                  number_of(extra[i], club), want);
    }
}

/* The number that a moving player takes to his new club. */
static void check_carried(void)
{
    int on = rt_squad_number_top() > LOW_TOP;
    test_line((int8_t)rt_squad_carried(50) == 50, "a move keeps 50", (int8_t)rt_squad_carried(50), 50);
    test_line((int8_t)rt_squad_carried((uint32_t)-50) == -50, "a move keeps -50",
              (int8_t)rt_squad_carried((uint32_t)-50), -50);
    test_line((int8_t)rt_squad_carried(75) == (on ? 75 : 0), "a move and 75", (int8_t)rt_squad_carried(75), on ? 75 : 0);
    test_line((int8_t)rt_squad_carried((uint32_t)-75) == (on ? -75 : 0), "a move and -75",
              (int8_t)rt_squad_carried((uint32_t)-75), on ? -75 : 0);
}

/* =screen: opens the squad numbers screen of the human club with the slots of open_squad_numbers_screen 0x475070
 * (0x475136..0x475296). Slot 0 is a person of the club, because the screen reads only his club (+0x39, 0x47aa26). */
static void open_numbers_screen(void)
{
    uint32_t club = find_club(1), person = 0;
    for (int i = 0; club && i < 50 && !person; i++)
        if (is_player(squad_slot(club, i))) person = squad_slot(club, i);
    if (!person) {
        fprintf(stderr, "recomp: squad numbers screen: FAIL no player of the human club\n");
        return;
    }
    /* The handler leaves a confirmed (negative) number alone (0x47ba06), so the player who holds 1 gets none. With the
     * option off he gets 75, as in a save made with the option on. */
    int on = rt_squad_number_top() > LOW_TOP;
    for (int i = 0; i < 50; i++) {
        uint32_t p = squad_slot(club, i);
        if (!is_player(p) || abs(number_of(p, club)) != 1) continue;
        set_number(p, club, on ? 0 : 75);
        fprintf(stderr, on ? "recomp: squad numbers screen: staff %u has no number\n"
                           : "recomp: squad numbers screen: staff %u has number 75\n", rd32(g_mem, p));
        watch_person = p;
        watch_club = club;
        break;
    }
    uint32_t push[5] = {0x475520u, 0x47a9e0u, 1, 0x47c070u, 0};
    Cpu saved = cpu;
    cpu.ecx = 0xb74450u;
    uint32_t opened = rt_callback(0x007fe600u, 5, push);
    for (uint32_t slot = 0; opened && slot <= 0x12u; slot++) {
        uint32_t value = slot == 0 ? person : slot == 2 ? 1u : slot == 4 ? 0x41u : slot == 5 ? 8u : slot == 6 ? 6u :
                         slot == 9 || slot == 10 ? 0xffffffffu : 0u;
        uint32_t a[3] = {slot, value, 0};
        cpu.ecx = 0xb74450u;
        rt_callback(0x007ff270u, 3, a);
    }
    cpu = saved;
    fprintf(stderr, "recomp: squad numbers screen: opened %u\n", opened != 0);
}

/* The input poll 0x55f620: the screen opens on top of the first news screen. */
void squad_numbers_test_poll(void)
{
    static int polls;
    uint32_t node = rd32(g_mem, 0xb74c32u);
    if (watch_person && node && rd32(g_mem, node) == 0x475520u && ++polls == 50) {     /* the screen is built */
        fprintf(stderr, "recomp: squad numbers screen: staff %u shows number %d\n", rd32(g_mem, watch_person),
                abs(number_of(watch_person, watch_club)));
        watch_person = 0;
    }
    if (!screen_pending) return;
    if (!node || rd32(g_mem, node) != 0x7881e0u) return;
    screen_pending = 0;
    open_numbers_screen();
}

/* RECOMP_TEST_SQUAD_NUMBERS=1 runs the checks at the first human turn and stops the game. =count only prints the
 * count of numbers above 50 at each human turn of a new game day. */
void squad_numbers_test_turn(void)
{
    static int state = -1, last_day = -1;
    if (state < 0) {
        const char *v = getenv("RECOMP_TEST_SQUAD_NUMBERS");
        state = !v ? 0 : !strcmp(v, "screen") ? 3 : strcmp(v, "count") ? 1 : 2;
    }
    if (!state) return;
    if (state == 3) {
        state = 0;
        screen_pending = 1;
        return;
    }
    int day = (int16_t)rd16(g_mem, 0xae2c90u) + (int16_t)rd16(g_mem, 0xae2c92u) * 366;
    if (state == 2 && day == last_day) return;
    last_day = day;

    uint32_t clubs = rd32(g_mem, CLUBS), count = rd32(g_mem, CLUB_COUNT);
    int above = 0, highest = 0;
    for (uint32_t c = 0; c < count; c++) {
        uint32_t club = clubs + c * CLUB_SIZE;
        for (int i = 0; i < 50; i++) {
            if (!is_player(squad_slot(club, i))) continue;
            int n = abs(number_of(squad_slot(club, i), club));
            above += n > LOW_TOP;
            if (n > highest) highest = n;
        }
    }
    fprintf(stderr, "recomp: squad numbers test: option %s, %d players above 50, highest %d\n",
            rt_squad_number_top() > LOW_TOP ? "on" : "off", above, highest);
    if (state == 2) return;
    state = 0;

    uint32_t ai = find_club(0), human = find_club(1);
    test_line(ai != 0, "a computer club", ai != 0, 1);
    test_line(human != 0, "the human club", human != 0, 1);
    check_carried();
    if (ai) check_kept(ai, "computer");
    if (human) check_kept(human, "human");
    if (ai) check_full(ai);
    if (human) check_full(human);
    fprintf(stderr, "recomp: squad numbers test: done, %d failures\n", test_failures);
    fflush(stderr);
    if (rt_on_fatal) rt_on_fatal();
    exit(test_failures ? 1 : 0);
}
