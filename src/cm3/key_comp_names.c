/* The competition names of set_key_ids 0x601a40 for the runtime (src/recomp_rt/win/season_load_check.c), from
 * the generated key table (key_ids_table.c). Kept apart so that the runtime does not include the port's headers.
 * Each entry of the club comp chain (match_club_comp_key_id 0x60efd0, which set_key_ids calls for the club comps
 * and then the nation comps) is the address of a name in cm0102.exe and the one or two key ids that a record of
 * that name sets. */
#include "key_index.h"

#include <stdint.h>

int key_comp_entry_count(void) { return g_key_club_comp_chain_length; }

uint32_t key_comp_entry(int i, int *slot, int *slot2)
{
    *slot = g_key_club_comp_chain[i].slot;
    *slot2 = g_key_club_comp_chain[i].slot2;
    return g_key_club_comp_chain[i].name;
}

/* 1 when set_key_ids shows the box index.cpp 5809 for this key id when no record sets it. */
int key_id_reported(int slot) { return slot >= 0 && slot < KEY_ID_COUNT && g_key_id_globals[slot].report; }
