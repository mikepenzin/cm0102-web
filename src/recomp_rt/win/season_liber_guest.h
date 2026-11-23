/* A Copa Libertadores winner from outside CONMEBOL (item 219): season_liber_guest.c. */
#ifndef SEASON_LIBER_GUEST_H
#define SEASON_LIBER_GUEST_H

#include <stdint.h>

/* 1 when the rule acts: season code on and changing rules on. */
int season_liber_rule_on(void);

/* 1 when `club` is a club of a nation outside CONMEBOL (a guest, as the Mexican clubs of 2000 to 2016), with the
 * rule on. Such a winner does not take CONMEBOL's place in the Intercontinental Cup, the Club World Cup or the Recopa. */
int season_liber_guest(uint32_t club);

/* 1 when `club` is a club of a CONMEBOL nation. */
int season_liber_conmebol(uint32_t club);

/* The club at `place` (0 winner, 1 runner-up) of the latest history row of record `rec`, as
 * comp_util_get_latest_placed_club 0x4afd80 finds it, but with no sort of the history table (0x945333), so a read
 * changes nothing. 0 when none. */
uint32_t season_liber_latest(uint32_t rec, int place);

/* The log line of each time the rule acts: `cup` of `year` gets `club` in place of the guest winner `guest`. */
void season_liber_guest_log(const char *cup, int year, uint32_t guest, uint32_t club);

#endif
