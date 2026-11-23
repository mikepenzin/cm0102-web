/* News items of a match that write no text (news_text.c). */
#ifndef NEWS_TEXT_H
#define NEWS_TEXT_H

#include <stdint.h>

/* After NewsItem::write_text 0x77bc80 (the wrap in season_change.c): a match item of a season game that wrote no text
 * takes the shared match text. */
void news_text_written(uint32_t item, uint32_t text, uint32_t full);

#endif
