/* The Swedish third tier in its real groups (issue 106, season_swe_groups.c). */
#ifndef RECOMP_SEASON_SWE_GROUPS_H
#define RECOMP_SEASON_SWE_GROUPS_H

#include <stdint.h>

/* 1 when the Second Division object plays two groups and SweSecond::create_section_stage 0x892670 makes no stage
 * `group` (only group 0, Sodra, exists). */
int season_swe_groups_skip_stage(uint32_t second, int group);

/* In a season of two groups: sends the rows of status 3 to the Lower Division in place of 0x890b40 and returns 1. */
int season_swe_groups_down(uint32_t prm);

/* The name of a group's stage id for the round name function, or NULL for another id. */
const char *season_swe_groups_name(int id);

#endif
