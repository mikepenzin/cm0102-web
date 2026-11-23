#ifndef KEY_INDEX_H
#define KEY_INDEX_H

#include "database.h"

/*
 * Key ids of index.cpp. The original keeps 1,284 globals, at
 * 0x9cf178..0x9d0524 and 0xb63c9c..0xb63d00, one for each record that its
 * code names, such as England or Europe. FUN_00601a40 sets 1,282 of them to
 * -1, then matches each row of six tables by name (_stricmp, ASCII case
 * only) against fixed chains of names and stores the row in the global of
 * the first equal name of each chain. A later row with the same name wins.
 * The port keeps them in g_key_ids, in the order of their addresses
 * (src/cm3/key_ids_table.c, made by tools/research/gen_key_ids.py). The
 * The names are not in the repo: each chain entry holds the address of its
 * name in the image of the player's cm0102.exe (exe_image.h), which must be
 * loaded before key_index_match or key_index_set. The
 * enums below name the ones that the port reads so far, and the arrays
 * g_key_nations and g_key_continents copy them.
 */

#define KEY_ID_COUNT 1284

typedef struct {
    u32 address;    /* the global in the original */
    s32 initial;    /* its value in the file */
    u8 reset;       /* FUN_00601a40 sets it to -1 first */
    u8 report;      /* FUN_00601a40 shows the box index.cpp 5809 when it stays -1 (0x6043ac..0x60ba80) */
} key_id_global_t;

typedef struct {
    u32 name;       /* the address of the name in cm0102.exe */
    s16 slot;       /* index into g_key_ids */
    s16 slot2;      /* a second key id that the name sets, or -1 */
    u8 chain;       /* in each chain the first equal name wins */
} key_id_entry_t;

extern const key_id_global_t g_key_id_globals[KEY_ID_COUNT];
extern const key_id_entry_t g_key_colour_chain[], g_key_nation_chain[], g_key_club_comp_chain[],
    g_key_continent_chain[], g_key_staff_comp_chain[], g_key_club_chain[];
extern const s32 g_key_colour_chain_length, g_key_nation_chain_length,
    g_key_club_comp_chain_length, g_key_continent_chain_length, g_key_staff_comp_chain_length,
    g_key_club_chain_length;

/* The key ids, in the order of g_key_id_globals. */
extern s32 g_key_ids[KEY_ID_COUNT];

/* The key id of a global of the original, or NULL. */
s32 *key_id(u32 address);

/* Every key id as the file has it: the state of the original at its start. */
void key_ids_initial(void);

enum key_nation {
    KEY_ARGENTINA,        /* DAT_009cf218 */
    KEY_AUSTRALIA,        /* DAT_009cf224 */
    KEY_BELGIUM,          /* DAT_009cf244 */
    KEY_BRAZIL,           /* DAT_009cf260 */
    KEY_CROATIA,          /* DAT_009cf2ac */
    KEY_DENMARK,          /* DAT_009cf2c0 */
    KEY_ENGLAND,          /* DAT_009cf2e4 */
    KEY_FINLAND,          /* DAT_009cf2fc */
    KEY_FRANCE,           /* DAT_009cf300 */
    KEY_GERMANY,          /* DAT_009cf314 */
    KEY_GREECE,           /* DAT_009cf31c */
    KEY_HOLLAND,          /* DAT_009cf338 */
    KEY_IRELAND,          /* DAT_009cf2dc "Republic Of Ireland" */
    KEY_ITALY,            /* DAT_009cf360 */
    KEY_JAPAN,            /* DAT_009cf36c */
    KEY_NORTHERN_IRELAND, /* DAT_009cf3e4 */
    KEY_NORWAY,           /* DAT_009cf408 */
    KEY_POLAND,           /* DAT_009cf430 */
    KEY_PORTUGAL,         /* DAT_009cf434 */
    KEY_RUSSIA,           /* DAT_009cf448 */
    KEY_SCOTLAND,         /* DAT_009cf45c */
    KEY_SOUTH_KOREA,      /* DAT_009cf484 */
    KEY_SPAIN,            /* DAT_009cf488 */
    KEY_SWEDEN,           /* DAT_009cf4a8 */
    KEY_TURKEY,           /* DAT_009cf4dc */
    KEY_UNITED_STATES,    /* DAT_009cf4f8 */
    KEY_WALES,            /* DAT_009cf510 */
    KEY_NATION_COUNT
};

enum key_continent {
    KEY_AFRICA,           /* DAT_009cfa08 */
    KEY_ASIA,             /* DAT_009cfa0c */
    KEY_EUROPE,           /* DAT_009cfa10 */
    KEY_NORTH_AMERICA,    /* DAT_009cfa14 */
    KEY_OCEANIA,          /* DAT_009cfa18 */
    KEY_SOUTH_AMERICA,    /* DAT_009cfa1c */
    KEY_CONTINENT_COUNT
};

/* The row of each key record in its table, or -1 when no row has the name. */
extern s32 g_key_nations[KEY_NATION_COUNT];
extern s32 g_key_continents[KEY_CONTINENT_COUNT];

/*
 * The nation loop (0x60420d, which calls FUN_0060d810 for each row) and the
 * continent loop (0x604278) of FUN_00601a40, over g_nations and
 * g_continents. Sets the named key ids to -1 first.
 */
void key_index_match(void);

/*
 * FUN_00601a40, the key ids at the end of the database load: every reset
 * key id to -1, then the colours, nations, club comps (without the 127
 * reserve rows), continents, staff comps and clubs. Left out: the names of
 * the eng.lng file when DAT_009b5984 is set (0x603c52, not for the shipped
 * data), and the log line and error box for each key id that stays -1
 * (0x6043ac..0x60ba6e; none with the shipped data).
 */
void key_index_set(void);

/* The nation of a key id, or NULL when the id is -1. */
nation_t *key_nation(enum key_nation key);
continent_t *key_continent(enum key_continent key);

#endif
