/* World Cup qualifying from the 2026 cycle (0.6.0 slice 29, season_wcq26.c). */
#ifndef RECOMP_SEASON_WCQ26_H
#define RECOMP_SEASON_WCQ26_H

#include <stdint.h>

/* 1 when a qualifying section plays for a World Cup of 48 teams in a season game. */
int season_wcq26(uint32_t self);

/* Called first by slice 28's wraps of wc_europe_league (season_nl_wc.c) at their entry state: the template
 * 0x9185c0, the play-off 0x919910 and get_asia_play_off_team 0x91def0. Each gives 1 when it completed the call
 * (a campaign for a World Cup of 48 teams), else 0 and changes nothing. */
int season_wcq26_europe_template(void);
int season_wcq26_europe_paths(void);
int season_wcq26_europe_no_asia(void);

/* The test log (RECOMP_SEASON_INTL_LOG=1) of the original's European pool at +0xee (count +0xf2) of a World Cup
 * qualifying section (0x919e30) or a European Championship qualifying (0x57f600): its size, Kosovo and every
 * nation in it with no continent or no players' club. what: the name of the line. */
void season_wcq26_log_pool(uint32_t self, const char *what);

/* The places of each confederation with hosts (0.6.2 slice 5, open item 59; season_wcq26_places.c). Index of a
 * confederation in the arrays. */
enum { WCQ_UEFA, WCQ_CONMEBOL, WCQ_CONCACAF, WCQ_CAF, WCQ_AFC, WCQ_OFC, WCQ_CONFS };

/* The direct places of each confederation (hosts not counted) for its hosts in a World Cup of 48 teams: each keeps
 * its 2026 total less its hosts (UEFA 16, CONMEBOL 6, CONCACAF 6, CAF 9, AFC 8, OFC 1). The shapes set the limits:
 * UEFA keeps its 12 group winners and at least 1 path, CAF at least 5 groups. OFC hosts beyond its one place each
 * take an AFC place. The play-off tournament gives 2 more (its six: one of CONMEBOL, CAF, AFC, OFC, two of
 * CONCACAF). */
void season_wcq26_places(const int hosts[WCQ_CONFS], int direct[WCQ_CONFS]);

/* The direct places of AFC final group g (0 or 1) of the `places` of the AFC: group A takes the odd one. */
int season_wcq26_asia_group(int places, int g);

#endif
