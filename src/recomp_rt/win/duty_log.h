/* Test log of the original's call-up move (duty_log.c, RECOMP_DUTY_LOG=1). */
#ifndef DUTY_LOG_H
#define DUTY_LOG_H

#include <stdint.h>

int duty_log_on(void);
void duty_log_remove(uint32_t fix, int removed);    /* inside 0x5abb10: remove_fixture 0x5abc30 and its result */
void duty_log_moved(uint32_t fix, uint32_t old_date); /* inside 0x5abb10: queue_fixture_moved_news 0x5aac70 */

#endif
