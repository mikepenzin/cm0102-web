/* The club records history and the fixtures keep the last 50 seasons (open item 192, 0.7.0, every game).
 *
 * Part 1, the club records history.
 *
 * At the season change Club_Records::end_season_for_nation 0x447880 adds one node for each club of the nation to
 * "club records history.tmp": FILE_LLIST::add_node 0x58e340 at 0x447fd2, with the list's head and tail at +8 and +0xc
 * of the club's 0x6a-byte entry (the table at Club_Records +0x1c). A node on disk is the node data (FILE_LLIST +4:
 * 0x38d bytes, the season year as a u16 and the record of 0x38b bytes) and then the file offset of the next node (-1
 * at the end). After each add 0x447880 adds 1 to the club's count of seasons (+0x68, a s16), so the count is never
 * below the length of the list. The original never removes a node, so the file grows by about 4.5 MB a season
 * (stored) in a full-world game and helps push the save past 4 GB before 2300.
 *
 * Only the Club History screen reads the list: FILE_LLIST::get_nodes 0x58f120 from Club_Records::build_history_list
 * 0x450d50, which the positions, results, attendances, sequences, players and transfers tabs call
 * (club_history_screen_build 0x46a1e0). get_nodes reads from the head to the tail and stops there (0x58f120: the
 * loop ends when the offset is the tail). No game decision reads the list. The count of seasons (which the records
 * checks of 0x447880 test) and the all-time records are not in the list and stay as they are.
 *
 * Here, before the add of a club whose list already holds 50 seasons or more, the oldest nodes leave the list:
 * the head moves on to the node that makes 49 with the tail, and the nodes before it go after the tail, in front
 * of what was after the tail. add_node then finds a node after the tail (0x58e5bf..: the tail's next is not -1)
 * and writes the new season into it in place, as it does in the lists that PlayerStats::reset_player 0x7c6650
 * resets. So the list holds 50 seasons in season order, the screen shows them without a gap, and the file stops
 * growing. A save with more than 50 seasons in a list keeps 50 from its next season change. The nodes it drops
 * stay in the file after the tail, where nothing reads them.
 *
 * A club whose count of seasons is below 50 is passed by with no read, so the first 50 seasons of a game run as
 * before. For the others the list is read and written with host file calls: add_node closes the file after each
 * add (0x58e98b), so the game holds no buffer of it, and the host calls leave the game's memory (heap, stack, the
 * C library's state) as it was. A list that does not reach its tail (a broken chain) is left as it is, with a line
 * on stderr. RECOMP_HISTORY_SEASONS=n keeps n seasons (at least 2); 0 keeps every season, as the original.
 *
 * Part 2, the fixtures. FIXTURE_MANAGER keeps one year of fixtures in memory (its year at +0x1130) and each other year
 * in CM3_TEMP\fixtures_<year>.tmp: set_year 0x5aa2d0 saves the year it leaves (save_fixtures 0x5ac060) and loads the
 * new one (load_fixtures 0x5ac3b0). No file is ever deleted, so a full-world game gains about 2.95 MB a season
 * (stored). Here, when the day loop moves the manager to a new year (the call at 0x5cd25a in game_main_day_loop
 * 0x5cccd0, and at 0x5cc923 in game_run_to_start_date 0x5cc850), the files of the years 51 or more before the new one
 * are deleted. A file holds a calendar year, so the 51 kept years hold the last 50 seasons whole. A save with older
 * files loses them at its next year change. fixtures_2999.tmp is never touched. If the second manager (+0x112c) holds
 * an old year at that moment, its next set_year writes the file again (save_fixtures 0x5ac060), and the next year
 * change deletes it.
 *
 * What reads an old year (trace of 9 November 2026, open item 192): load_fixtures returns an empty list, quietly,
 * when the file of a year is missing (0x5ac3b0: the fopen fails and it returns). Every game decision asks for the
 * fixtures of today or a date at most 5 years back (scan_fixtures 0x5acee0), or for the match just played. Screens
 * that show an old year (match details, past meetings, club and competition screens) show no fixtures for a deleted
 * year. One stored key can be older: the human manager's watched fixture (human record +0x9ad4, set from the menubar
 * 0x764fb1). clear_invalid_watched_fixtures 0x5fe913 (from MATCH_DAY::MATCH_DAY 0x6b410e) clears a key whose date
 * has a match of the human's club; with the year's file deleted it finds no match and keeps the key, as the original
 * does on a day with no match. is_watching_fixture 0x5fe290 needs an exact key, so an old key changes no match.
 *
 * Test: with RECOMP_TEST_CLUB_HISTORY=1, after the test save of testrun.c (RECOMP_SAVE_AFTER_DAYS), the runtime builds
 * the list of the Club History tabs for every club (Club_Records::build_history_list 0x450d50, as the tabs call it,
 * thiscall on the object at 0xad9dac with the club and &count) and prints one line: the clubs, the longest list,
 * and the lists whose seasons do not run one way (the tabs' list may run from the newest season). The build can
 * update a season rating record (0x452a90), so it runs only after the save, just before the program ends. */
#include "win.h"
#include "../rt.h"
#include "../host.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SITE_CLUB_RECORDS 0x00447fd7u   /* the return of the call at 0x447fd2 in 0x447880 */
#define CLUB_SEASONS 0x68u              /* the club entry's count of seasons (s16), from its head at +8 */
#define MAX_NODES 1000000               /* a longer walk is a broken chain */
#define CLUB_RECORDS 0x00ad9dacu        /* the Club_Records object */
#define CLUB_TABLE 0x00ae23b4u          /* the clubs, 0x245 bytes each */
#define CLUB_COUNT 0x00ae235cu
#define CLUB_SIZE 0x245u
#define BUILD_HISTORY_LIST 0x00450d50u  /* thiscall (club, &count): malloc'ed entries of 0x38d bytes, year first */
#define HISTORY_ENTRY 0x38du
#define FREE 0x009452cau                /* cdecl (block) */
#define FIXTURE_YEAR 0x1130u            /* FIXTURE_MANAGER: the year in memory (u16) */
#define SITE_DAY_LOOP 0x005cd25fu       /* the returns of the calls of set_year at 0x5cd25a and 0x5cc923 */
#define SITE_START_DATE 0x005cc928u
#define FIRST_YEAR 1980                 /* load_fixtures reads no year before 1980 (0x5ac3e9: year > 0x7bb) */

void o_0058e340(void);
void o_005aa2d0(void);

int history_trim_seasons(void)
{
    static int keep = -1;
    if (keep < 0) {
        const char *v = getenv("RECOMP_HISTORY_SEASONS");
        char *end = NULL;
        keep = v && *v ? (int)strtol(v, &end, 10) : 50;
        if (v && *v && (*end || keep < 0)) {
            fprintf(stderr, "recomp: RECOMP_HISTORY_SEASONS=%s is not a count of seasons; every season is kept\n", v);
            keep = 0;
        }
        if (keep == 1) keep = 2;
    }
    return keep;
}

static int read_next(int fd, uint32_t node, uint32_t size, int32_t *next)
{
    uint8_t b[4];
    if (host_seek(fd, (int64_t)node + size, SEEK_SET) < 0 || host_read(fd, b, 4) != 4) return 0;
    *next = (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
    return 1;
}

static int write_next(int fd, uint32_t node, uint32_t size, int32_t next)
{
    uint32_t v = (uint32_t)next;
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    return host_seek(fd, (int64_t)node + size, SEEK_SET) >= 0 && host_write(fd, b, 4) == 4;
}

/* The list of FILE_LLIST llist with its head and tail at headp and tailp: before an add, at most keep - 1 nodes
 * stay between the head and the tail, and the others go after the tail. */
static void trim(uint32_t llist, uint32_t headp, uint32_t tailp, int keep)
{
    int32_t head = (int32_t)rd32(g_mem, headp), tail = (int32_t)rd32(g_mem, tailp);
    if (head < 0 || tail < 0) return;
    if ((int16_t)rd16(g_mem, headp + CLUB_SEASONS - 8) < keep) return;     /* fewer seasons than keep: no read */
    const char *name = win_str(rd32(g_mem, llist));
    uint32_t size = rd32(g_mem, llist + 4);
    char host[2048];
    int fd = win_existing_file(name, host, sizeof host) ? host_open(host, O_RDWR, 0) : -1;
    if (fd < 0) {
        fprintf(stderr, "recomp: club records history: cannot open %s; the list is left as it is\n", name);
        return;
    }
    int32_t *nodes = NULL;
    int n = 0, room = 0, ok = 1;
    for (int32_t at = head;;) {
        if (n == room) {
            room = room ? room * 2 : 64;
            int32_t *more = realloc(nodes, sizeof *nodes * (size_t)room);
            if (!more) { ok = 0; break; }
            nodes = more;
        }
        nodes[n++] = at;
        if (at == tail) break;
        if (n >= MAX_NODES || !read_next(fd, (uint32_t)at, size, &at) || at < 0) { ok = 0; break; }
    }
    if (!ok) {
        fprintf(stderr, "recomp: club records history: the list at %d does not reach its tail %d; left as it is\n",
                head, tail);
    } else if (n >= keep) {
        int drop = n - (keep - 1);          /* nodes[0..drop-1] go after the tail; 1 <= drop <= n - 1 */
        int32_t after;
        if (read_next(fd, (uint32_t)tail, size, &after)
            && write_next(fd, (uint32_t)nodes[drop - 1], size, after)
            && write_next(fd, (uint32_t)tail, size, nodes[0]))
            wr32(g_mem, headp, (uint32_t)nodes[drop]);
        else
            fprintf(stderr, "recomp: club records history: a write at the list %d failed; the list may be broken\n",
                    head);
    }
    free(nodes);
    host_close(fd);
}

/* FILE_LLIST::add_node, thiscall (int *head, int *tail, void *data), ret 0xc. */
RT_WRAP(0058e340, file_llist_add_node)
{
    int keep = history_trim_seasons();
    if (keep && rd32(g_mem, cpu.esp) == SITE_CLUB_RECORDS) trim(cpu.ecx, rt_arg(0), rt_arg(1), keep);
    o_0058e340();
}

/* Deletes fixtures_<y>.tmp of each year y that is more than keep years before year. load_fixtures 0x5ac3b0 opens the
 * file by the path of TEMP_PATH 0x90bde0 with the folder key CM3_TEMP, which 0x90bc50 maps to the empty string
 * 0xad9c64 when no CM3_TEMP variable is set: the bare name. The same bare name goes to the runtime here, so it finds
 * the file that the game's CreateFileA finds. No code of the original runs, so the game's memory stays as it was. */
static void drop_old_fixtures(int year, int keep)
{
    int dropped = 0;
    for (int y = year - keep - 1; y >= FIRST_YEAR; y--) {
        char name[32];
        snprintf(name, sizeof name, "fixtures_%d.tmp", y);
        if (win_delete_file(name)) dropped++;
    }
    if (dropped && getenv("RECOMP_HISTORY_LOG"))
        fprintf(stderr, "recomp: fixtures of %d: %d files of %d or before deleted\n", year, dropped, year - keep - 1);
}

/* FIXTURE_MANAGER::set_year, thiscall (year), ret 4. */
RT_WRAP(005aa2d0, fixture_set_year)
{
    uint32_t site = rd32(g_mem, cpu.esp), manager = cpu.ecx;
    int keep = history_trim_seasons();
    o_005aa2d0();
    if (rt_unwinding || !keep || (site != SITE_DAY_LOOP && site != SITE_START_DATE)) return;
    int year = (int)rd16(g_mem, manager + FIXTURE_YEAR);
    if (year - keep - 1 >= FIRST_YEAR) drop_old_fixtures(year, keep);
}

/* The test of the Club History lists (RECOMP_TEST_CLUB_HISTORY=1), called by testrun.c after its save. Entry 0 of a
 * list is the all-time record (year 0), entry 1 the current season, then the seasons of the history list. */
void history_trim_screen_check(void)
{
    const char *v = getenv("RECOMP_TEST_CLUB_HISTORY");
    if (!v || *v != '1') return;
    uint32_t records = rd32(g_mem, CLUB_RECORDS), table = rd32(g_mem, CLUB_TABLE);
    int clubs = (int)rd32(g_mem, CLUB_COUNT), longest = 0, unordered = 0, failed = 0, with = 0;
    uint32_t esp0 = cpu.esp;
    cpu.esp = (cpu.esp - 16) & ~3u;
    uint32_t count = cpu.esp;
    for (int i = 0; i < clubs; i++) {
        uint32_t args[2] = {table + (uint32_t)i * CLUB_SIZE, count};
        Cpu saved = cpu;
        cpu.ecx = records;
        uint32_t list = rt_callback(BUILD_HISTORY_LIST, 2, args);
        cpu = saved;
        if (rt_unwinding) return;
        if (!list) { failed++; continue; }
        int n = (int)rd32(g_mem, count) - 2;
        if (n > 0) with++;
        if (n > longest) longest = n;
        int way = 0;                    /* the history entries run one way: +1 rising years, -1 falling */
        for (int k = 3; k < n + 2; k++) {
            int a = (int)rd16(g_mem, list + (uint32_t)(k - 1) * HISTORY_ENTRY);
            int b = (int)rd16(g_mem, list + (uint32_t)k * HISTORY_ENTRY);
            int step = b > a ? 1 : b < a ? -1 : 0;
            if (!way) way = step;
            if (!step || step != way) { unordered++; break; }
        }
        uint32_t f[1] = {list};
        rt_callback(FREE, 1, f);
        if (rt_unwinding) return;
    }
    cpu.esp = esp0;
    fprintf(stderr, "recomp: club history lists: %d clubs, %d with seasons, longest %d, not in order %d, failed %d\n",
            clubs, with, longest, unordered, failed);
}
