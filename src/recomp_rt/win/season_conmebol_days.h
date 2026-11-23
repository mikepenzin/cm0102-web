/* The match days of the South American section of World Cup qualifying in a season game (open item 163 of 0.6.2).
 * Pure code (season_conmebol_days.c), so ctest season_conmebol_days links it with the FIFA days. */
#ifndef RECOMP_SEASON_CONMEBOL_DAYS_H
#define RECOMP_SEASON_CONMEBOL_DAYS_H

/* 1 when day z (days from 1 January 1970) lies in the days of a South American finals tournament
 * (season_intl_finals_busy for the confederation), so no round takes it. NULL: none does. */
typedef int (*SeasonConmebolBusy)(long z);

/* The span of the window rule (days from 1 January 1970) of World Cup year wc: the real first and last matchday of
 * the cycles 2006 to 2026 (2022: the game's last day, 10 October 2021), and after them from 1 September three
 * years before the finals to 15 October the year before (an inference, season_conmebol_days.c). */
void season_conmebol_span(int wc, long *first, long *last);

/* 1 when day z is a real matchday of the cycles 2006 to 2026 (real[]): it can lie outside the FIFA list (9 June
 * 2012, Euro 2012; 1 June 2004), and a qualifier on it keeps its day (season_friendly_dates.c fifa_target_by). */
int season_conmebol_real_day(long z);

/* The fields of a round record of the template (league_round_set_date 0x68a160) for day z: the day of the year
 * (+0, 0x68a1a5), the year less the campaign year `base` (+2, 0x68a1ac) and the flag (+4, 0x68a1b4): 1 for
 * Saturday or Sunday, else 2, as the class's fixed fixtures set it (0x9224d1..0x9224f0). */
void season_conmebol_round_fields(long z, int base, int *doy, int *year_off, int *flag);

/* The days of the `rounds` rounds (in order) of the section of World Cup year wc, as days from 1 January 1970, in
 * out[]. 18 rounds of the cycles 2006, 2010, 2014, 2018 and 2026 made before their first day: the real days. Else
 * the window rule: each a FIFA day (season_fifa_year_days) in the span, two rounds a FIFA window at least 3 days
 * apart, none earlier than `earliest` and none that busy() holds. Returns rounds, or 0 when the days do not hold
 * them (out[] is then not used). */
int season_conmebol_days(int wc, int rounds, long earliest, SeasonConmebolBusy busy, long out[]);

/* The day the section makes its fixtures (WcSouthAmericanLeague +0xb6 day and +0xb8 year offset, set_up_rules
 * 0x921040 at 0x921118 and 0x921133; update_for_date 0x921160 makes them on that day only, 0x921206..0x92123a).
 * A fixture made for a day already past moves to the next free days (FIXTURE_MANAGER::add_fixture 0x5aa680 at
 * 0x5aa729), so no round may lie on or before it. When the first round is on or before the original's day
 * `original`: MAKE_LEAD days before the first round, but not before tomorrow (today + 1). Else `original`. */
long season_conmebol_make_day(long first_round, long original, long today);

#endif
