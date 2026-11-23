/* A saved game that lacks a key competition name (0.6.1 item 10, season_load_check.c). */
#ifndef RECOMP_SEASON_LOAD_CHECK_H
#define RECOMP_SEASON_LOAD_CHECK_H

#include <stdint.h>

/* The number of key ids of the competitions that set_key_ids 0x601a40 would report with the box "index.cpp 5809"
 * when names[0..count) are the long names of the club comp and nation comp records. name_at gives the text of a
 * name address of cm0102.exe (NULL when it has none). */
int season_key_comps_missing(const char *const *names, int count, const char *(*name_at)(uint32_t));

#endif
