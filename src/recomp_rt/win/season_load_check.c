/* A saved game that lacks a key competition name (0.6.1 item 10).
 *
 * set_key_ids 0x601a40 runs at every load of the data, also of a saved game (0x52d8fa in database_load
 * 0x528540). With the English names it calls match_club_comp_key_id 0x60efd0 for each club comp record and then
 * each nation comp record (the loop at 0x60425d, record +4) and stores the record in the key id of the first equal
 * name (_stricmp, ASCII case only). After the chains it checks the key ids (0x6043ac..0x60ba80: cmp [id], -1) and
 * shows the box "v3.9.68 index..cpp 5809" for each one that no record set. Then the load goes on with that id
 * at -1.
 *
 * A season game changes competition names, and the season part of its saves keeps the name in the data
 * (season_change_rename, season_change.h). A save that holds the renamed records without its part (in 0.6.0 a
 * crash after the rename of the save and before the part went in, items 4 and 10) cannot give the names back,
 * so its load stops at that box. The load refuses such a save before any code of the original runs
 * (season.c load_game), with a message that tells the player to load an earlier save.
 *
 * The names in another language: set_key_ids then matches the English names of eng.lng (0x603c52..0x603c89),
 * which no rename changes, so the check is only for the English names (season.c). */
#include "season_load_check.h"

#include <string.h>
#include <strings.h>

int key_comp_entry_count(void);                     /* src/cm3/key_comp_names.c */
uint32_t key_comp_entry(int i, int *slot, int *slot2);
int key_id_reported(int slot);

#define MAX_SLOTS 2048

int season_key_comps_missing(const char *const *names, int count, const char *(*name_at)(uint32_t))
{
    static unsigned char wanted[MAX_SLOTS], found[MAX_SLOTS];
    memset(wanted, 0, sizeof wanted);
    memset(found, 0, sizeof found);
    int entries = key_comp_entry_count();
    for (int i = 0; i < entries; i++) {
        int slot, slot2;
        const char *key = name_at(key_comp_entry(i, &slot, &slot2));
        int ids[2] = {slot, slot2}, hit = 0;
        for (int r = 0; key && !hit && r < count; r++) hit = !strcasecmp(names[r], key);
        for (int k = 0; k < 2; k++) {
            if (ids[k] < 0 || ids[k] >= MAX_SLOTS || !key_id_reported(ids[k])) continue;
            wanted[ids[k]] = 1;
            if (hit) found[ids[k]] = 1;
        }
    }
    int missing = 0;
    for (int s = 0; s < MAX_SLOTS; s++) missing += wanted[s] && !found[s];
    return missing;
}
