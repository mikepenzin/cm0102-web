/* The injury records at a new game (0.6.0 slice 15). This fixes a bug of the original that a season game can
 * reach.
 *
 * Injury::init_records 0x62cf70 runs once at a new game (Injury::Injury 0x62a680 calls it at 0x62a873). It
 * allocates one 31-byte record per staff (malloc at 0x62cf91, count [0xae2364]). It fills the records in the
 * order of the staff table: at 0x62d036..0x62d056 record i gets its values (+0xe, +0x14 and +0x18 are -1).
 * In the same loop about one player in a hundred gets an injury or illness (Injury::add_injury 0x62d250,
 * called at 0x62d0df). An illness can spread: Injury::add_illness_outbreak 0x62e820 (called at 0x62dbfd)
 * calls add_injury at 0x62eb6e for other players. A player of a later staff row then has a record that the
 * loop has not filled yet. Its memory is fresh, so every field is 0.
 *
 * add_injury reads the current injury at +0xe (0x62d3f1). The value 0 is not -1. For an illness type (0x4d,
 * 0x4e, 0x4f, 0x51, tests at 0x62d3fa..0x62d40c) it writes that injury to the history file with
 * FILE_LLIST::add_node 0x58e340 and the offsets +0x14 and +0x18, both 0. With offsets that are not -1,
 * add_node opens the file "r+b" (0x58e5c1). The file does not exist yet, so the game stops at the box "Disk
 * Space Error" (file_llist.cpp 161, 0x58e601). For other types add_injury ends at 0x62dc82 and returns -1.
 * A start in 2009/10 with an older club moves table showed it: the illness of staff row 19736 reached row
 * 19744.
 *
 * The fix: while init_records runs, an add_injury call for a staff row after the row of the loop returns -1 at
 * once, as add_injury does at 0x62dc82. That player gets no injury, and the loop fills his record later as
 * before. So no injury of the injury list (this +4, written at 0x62d4bc) names a record that the loop then
 * clears. Only a season game has the fix: the base game keeps the code of the original. */
#include "win.h"
#include "season_names.h"
#include "tier_a.h"

#include <stdio.h>

#define STAFF_TABLE 0xae23bcu       /* the staff table, 0x6e bytes a staff */
#define STAFF_SIZE 0x6eu

static int in_init;                 /* init_records runs */
static int loop_row = -1;           /* the staff row of the loop: the row of the outer add_injury call */
static int depth;                   /* add_injury calls in progress */

void o_0062cf70(void);
void o_0062d250(void);

/* Injury::init_records 0x62cf70, thiscall, no arguments. */
RT_WRAP(0062cf70, injury_init_records)
{
    in_init = season_game_changes();
    loop_row = -1;
    depth = 0;
    o_0062cf70();
    in_init = 0;
    depth = 0;
}

/* Injury::add_injury 0x62d250, thiscall, 8 arguments (ret 0x20 at 0x62dc8f). The staff is the third argument
 * (0x62d25a). The Tier A switch (tier_a.c) gives no new injury after the game is made: it returns -1 as
 * 0x62dc82 does. No caller reads eax (0x62bc8e, 0x62c28f, 0x62e69d, 0x62eb73, 0x7a44c9, 0x7a44ff, 0x7e5f32). */
RT_WRAP(0062d250, injury_add_injury)
{
    if (tier_a_in_play()) {
        rt_ret_stdcall(0xffffffffu, 8);
        return;
    }
    if (!in_init) {
        o_0062d250();
        return;
    }
    uint32_t staff = rt_arg(2), table = rd32(g_mem, STAFF_TABLE);
    if (staff && table && staff >= table) {
        int row = (int)((staff - table) / STAFF_SIZE);
        if (depth == 0) loop_row = row;
        else if (row > loop_row) {
            fprintf(stderr, "recomp: injury records: staff row %d reached from row %d before its turn: no injury\n",
                    row, loop_row);
            rt_ret_stdcall(0xffffffffu, 8);
            return;
        }
    }
    depth++;
    o_0062d250();
    depth--;
}
