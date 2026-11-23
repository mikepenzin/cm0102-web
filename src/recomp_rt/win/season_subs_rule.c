/* The substitute rule lookup (season_subs_rule.h). Host code only. */
#include "season_subs_rule.h"
#include "season_rules.h"

#include <stddef.h>

static int clamp(int n) { return n > SEASON_SUBS_MAX ? SEASON_SUBS_MAX : n; }

/* The bit when the season's rows of the file give that count only as "unknown" (or "as the Champions League", with
 * at least one unknown) and found does not hold it: no n/a, not held, description or "see each" row. */
static int carry_wanted(int season, const char *file, int found, int bit)
{
    if (found & bit) return 0;
    int unknown = 0;
    for (int n = 0; ; n++) {
        const SeasonRule *r = season_rules_find(season, file, n);
        if (!r) break;
        int c = bit == SEASON_SUBS_USED ? r->subs_used : r->bench;
        if (c == SEASON_RULE_UNKNOWN) unknown = 1;
        else if (c != SEASON_RULE_AS_CL) return 0;
    }
    return unknown ? bit : 0;
}

int season_subs_rule(int season, const char *file, int *used, int *bench)
{
    return season_subs_rule_from(season, file, used, bench, NULL, NULL);
}

int season_subs_rule_from(int season, const char *file, int *used, int *bench, int *used_from, int *bench_from)
{
    if (!file) return 0;
    int found = 0;
    for (int n = 0; found != (SEASON_SUBS_USED | SEASON_SUBS_BENCH); n++) {
        const SeasonRule *r = season_rules_find(season, file, n);
        if (!r) break;
        if (!(found & SEASON_SUBS_USED) && r->subs_used >= 0) {
            if (used) *used = clamp(r->subs_used);
            if (used_from) *used_from = season;
            found |= SEASON_SUBS_USED;
        }
        if (!(found & SEASON_SUBS_BENCH) && r->bench >= 0) {
            if (bench) *bench = clamp(r->bench);
            if (bench_from) *bench_from = season;
            found |= SEASON_SUBS_BENCH;
        }
    }
    /* Item 274: a count that the season's rows give as "unknown" keeps the latest earlier count of the file, so a
     * new season row with no source does not fall back to the competition's own 3 and 5. A row "as the Champions
     * League" follows the main row, so it neither gives nor stops a count. A row n/a, not held, a description or
     * "see each competition" stops it: the season does not carry, and the search back stops at such a season. No
     * count of a season before SEASON_SUBS_ERA reaches a season from it on. */
    int want = carry_wanted(season, file, found, SEASON_SUBS_USED) |
               carry_wanted(season, file, found, SEASON_SUBS_BENCH);
    for (int y = season - 1; want && y >= SEASON_RULE_FIRST; y--) {
        if (season >= SEASON_SUBS_ERA && y < SEASON_SUBS_ERA) break;
        int stop = 0;
        for (int bit = SEASON_SUBS_USED; bit <= SEASON_SUBS_BENCH; bit <<= 1) {
            if (!(want & bit)) continue;
            int v = SEASON_RULE_UNKNOWN;
            for (int n = 0; ; n++) {
                const SeasonRule *r = season_rules_find(y, file, n);
                if (!r) break;
                int c = bit == SEASON_SUBS_USED ? r->subs_used : r->bench;
                if (c >= 0) { v = c; break; }
                if (c != SEASON_RULE_UNKNOWN && c != SEASON_RULE_AS_CL) v = c;   /* n/a, text: stops */
            }
            if (v >= 0) {
                if (bit == SEASON_SUBS_USED) {
                    if (used) *used = clamp(v);
                    if (used_from) *used_from = y;
                } else {
                    if (bench) *bench = clamp(v);
                    if (bench_from) *bench_from = y;
                }
                found |= bit;
                want &= ~bit;
            } else if (v != SEASON_RULE_UNKNOWN) {
                stop |= bit;
            }
        }
        want &= ~stop;
    }
    return found;
}
