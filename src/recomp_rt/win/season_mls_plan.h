/* The MLS of real life from 2002 (0.7.2 gap row 7): its conferences, its unbalanced schedule and its play-off.
 *
 * The game's MLS edition Y plays in calendar year Y (UsaMls::create_schedule 0x9072d0 writes year offset 0 for its
 * first round, 29 March, 0x907356..0x907367), so edition Y is the real season Y. 2001 is the original's form
 * (three divisions of 4, which is real 2001). Sources: docs/research/2026-11-15-mls-seasons.tsv (Wikipedia season
 * and play-off pages, RSSSF), read 15 November 2026.
 *
 * This file is pure (no game memory): the unit test tests/seasons/test_season_mls_plan.c runs it. */
#ifndef RECOMP_SEASON_MLS_PLAN_H
#define RECOMP_SEASON_MLS_PLAN_H

#define MLS_FIRST_SEASON 2002       /* the first season of two conferences */
#define MLS_LAST_KNOWN 2026         /* later editions keep this season's form (2027 is the spring sprint of the move
                                     * to a summer-spring calendar, which the game's dates do not make) */
#define MLS_MAX_CLUBS 32
#define MLS_MAX_ROUNDS 80
#define MLS_MAX_MATCHES (MLS_MAX_CLUBS * 40 / 2)

enum { MLS_EAST = 0, MLS_WEST = 1 };

/* The regular season of a season: a club meets each club of its own conference `own` times and each club of the
 * other conference `other` times. Then extra matches against its own conference bring each club to `own_to` games,
 * and extra matches against the other conference (each `unit` matches with one club: 2 = one designated rival met
 * home and away) bring it to `games`. */
typedef struct {
    int season, own, other, own_to, games, unit;
} MlsSchedule;

/* The play-off of a season. `east` and `west` clubs (the best of each conference table) and `wild` clubs of either
 * conference by points (the best clubs not in yet), each wild club in its own conference's bracket when that bracket
 * has room (each bracket holds (east + west + wild) / 2), else in the other. Inside a bracket each round brings the
 * clubs left down to 8, 4, 2 and then 1 (the conference champion): with c clubs left a round to t clubs has c - t
 * matches, the best club left against the worst club left, and the best clubs need no match (a bye). legs[k] is the
 * number of legs of the round to 8, 4, 2 and 1 clubs (2: two legs on aggregate, also the nearest form of the
 * best-of-three and points series). The final is one match: at the better club's ground from 2012, at a neutral
 * ground before (final_neutral). */
typedef struct {
    int season, east, west, wild, legs[4], final_neutral;
} MlsPlayoff;

const MlsSchedule *season_mls_schedule(int season);   /* NULL before MLS_FIRST_SEASON */
const MlsPlayoff *season_mls_playoff(int season);     /* NULL before MLS_FIRST_SEASON */

/* The shape of a play-off with ne East and nw West clubs in the league: the size of each bracket (bracket[0] East,
 * bracket[1] West: the qualifiers of the conference and the wild clubs, which fill the brackets to the same size
 * when they can) and the targets of its rounds (8, 4, 2, 1: the rounds that some bracket needs). Returns the number
 * of rounds before the final. */
int season_mls_playoff_rounds(const MlsPlayoff *p, int ne, int nw, int bracket[2], int targets[4]);

/* The matches of a round that brings c clubs (in seed order, 0 the best) down to t: m = c - t matches between the
 * 2m worst clubs, the best of them against the worst. pairs[2k] is the better seed, pairs[2k + 1] the worse.
 * Returns m (0 when c <= t). */
int season_mls_round_pairs(int c, int t, int *pairs);

/* The conference of a club by its name in a season: MLS_EAST, MLS_WEST, or -1 when the name is not known. */
int season_mls_conference(const char *name, int season);

typedef struct {
    int home, away, round;
} MlsMatch;

typedef struct {
    int n;                              /* clubs */
    int own, other;                     /* the base meetings played (the schedule's, or fewer for more clubs) */
    int conf[MLS_MAX_CLUBS];
    int meet[MLS_MAX_CLUBS][MLS_MAX_CLUBS];
    int games[MLS_MAX_CLUBS], home[MLS_MAX_CLUBS];
    int first[MLS_MAX_CLUBS][MLS_MAX_CLUBS];   /* the home club of a pair's first meeting */
    int matches, rounds;
    MlsMatch m[MLS_MAX_MATCHES];
} MlsPlan;

/* Plans the regular season of n clubs (conf[i] MLS_EAST, MLS_WEST, or -1: the conference with fewer clubs) with
 * the schedule s. `turn` turns the club order (the edition year), so the extras differ by season. Each match gets
 * a round: no club plays twice in a round. Returns the number of rounds, or 0 (why holds the reason). */
int season_mls_plan(const MlsSchedule *s, int n, const int *conf, int turn, MlsPlan *out, char *why, int why_size);

#endif
