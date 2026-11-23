/* The picker of the Assistant drop-down (#12, assistant.c). Research:
 * docs/research/2026-10-15-assistant-pick-inventory.md.
 *
 * The original picks a team only inside a match (0x6db500, 0x6dbc50): it needs the match object and
 * draws random numbers. So the runtime picks the team itself, from the game's own data, and draws no
 * random numbers. It then stores the team with the game's own calls (0x896fe0, 0x896a90, 0x897760), as
 * the original's pick does.
 *
 * The slots are those of the player's active tactic (tactic +0x12d, a position mask per slot), so the
 * formation stays. A player's fit for a slot is the lower of his rating for the position and his
 * rating for the side, from the position ratings of the player data (1 to 20). A player's score in a
 * slot is the average of the key shown attributes of the slot's position times that fit, divided by 20
 * (Max, Q138 to Q140, 2026-10-15). The shown attributes come from the game's getter 0x5431e0 (1 to 20),
 * as the squad screen shows them for the player's own club. The getter does not apply the screen's
 * mask of unknown attributes (0x543280); best XI (picker_compute_view) applies it, so a pick for another
 * club uses only the values the screen shows (Q108, Q111). Current ability is not used: on Max's Yeovil save it put Weale in goal over
 * D. Williams (Agility 20), because fixed attributes such as Agility, Jumping and Technique do not feed
 * current ability. The eleven is the assignment of the highest total
 * value over all slots, found exactly with the Hungarian method (Q130). A player with a fit below 10
 * plays only when no free player has 10 or more for the slot (Q131). The subs then come from the players
 * left over, by the same rule. picker_compute picks without writing (best XI uses it, Q132);
 * picker_pick also stores the pick. 0.3.0 filled one slot at a time, and its swap pass never brought in
 * a bench player: on Max's Yeovil save Warner (a natural DR) sat on the bench.
 *
 * Candidates: the players of the squad (club +0xd7, reserve-team players too, Q134) whom the squad
 * screen shows as able to play the next match. 0x45cbe0 is the screen's own test (0x459b3d): not
 * injured, not loaned out, not banned, not away with a national team and eligible for the competition.
 * A slot of a player who fails it shows red. The virtual players are never candidates: a staff id at or
 * above the staff count [0xae2364] is a temporary row that VIRTUAL_STAFF tops up a short squad with
 * (0x90d4f0 for the club a human views, 0x90d2f0 for a fixture) and takes away again (0x90d6b0, 0x90d860).
 * The squad list shows him grey (0x45b984) and a match counts only the real players of a side (0x6dbaa1).
 * 0.6.0 picked them: on a new game as Barnsley the virtual "Cooper, J" took ML, and Continue Game then
 * said "You have only selected ten outfield players" (0.6.1 item 7).
 * - best: all candidates.
 * - match_fit: the candidates with condition 90% or more. When too few, the fittest of the others join.
 * - under_21: first the candidates under 21. The empty slots then get the best older players.
 * The candidates of the first group come before all others (weight below), so a slot takes a player from
 * outside the group only when no player of the group can take it with a fit of 10 or more (Q142).
 *
 * Goalkeeper slot: as in the original (0x6e2000, research section "scorer"), a player with a goalkeeper
 * rating below 10 (player data +0x0f) never takes a slot that allows a goalkeeper (position bit 0x1).
 * So no outfield player goes in goal. When no young keeper exists, the best older one takes the slot.
 * With no keeper at all, the slot stays empty.
 *
 * No daily update: the getters for condition (0x62c590), injury (0x62c400) and the squad test (0x45cbe0)
 * first run the daily fitness update 0x62c680, which draws random numbers and changes the player. During
 * the pick the runtime wraps 0x62c680 and skips it, so the pick only reads what the records hold. A
 * record that no screen has updated today shows the value of its last update. */
#include "win.h"
#include "pick_score.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HUMANS 0xb63c98u         /* this of 0x5fbd00 */
#define STORE 0xde1d08u          /* the stored selections: this of 0x896fe0, 0x896a90, 0x897760 */
#define FITNESS 0xb63d28u        /* this of 0x62c400 and 0x62c590 */
#define NATIONS 0xb74318u        /* this of 0x7757d0 */
#define FIXTURES 0xae2a58u       /* this of 0x5acbe0 */
#define PLAYER_MANAGER 0xb74c26u /* per player slot, 0x300 bytes apart: the manager */
#define PLAYER_SLOT 0xb77c7au    /* s16: the current player slot */
#define STAFF_COUNT 0xae2364u    /* g_staff_count: a staff id at or above it is a virtual player */
#define TODAY 0xae2c90u          /* the game date: s16 day of year, s16 year; this of 0x54c5b0 */
#define AVAILABLE 0x45cbe0u      /* cdecl (staff, fixture, club, date): 0 when the player cannot play */
#define SQUAD 0xd7u              /* club: 50 staff pointers */
#define MANAGER 0xcfu            /* club: the manager staff record */
#define PLAYER_DATA 0x61u        /* staff: the player data, 0 for no player */
#define AGE 0x18u                /* staff: s16 age */
#define ABILITY 0x05u            /* player data: s16 current ability, 1 to 200 */
#define SLOTS 11
#define MAX_SUBS 9               /* the store holds 20 slots */
_Static_assert(SLOTS + MAX_SUBS <= PICK_SLOTS, "a Pick holds the eleven and the subs");
#define DEFAULT_SUBS 5           /* without a fixture in the next 30 days. Inference: league rules */
#define FIT 90                   /* condition in % for "most match-fit" */
#define NATURAL 15               /* the game's "natural" rating (0x5b7d20) */
#define ABLE 10
#define KNOWN 0x543280u          /* cdecl (viewer manager, staff, index): 0 when the viewer's screens hide it */

/* The positions of the substitutes. The original draws them at random (0x6dd108); the runtime uses a
 * fixed list: GK, D, M, M, F, then M, D, F, M. */
static const uint16_t sub_masks[MAX_SUBS] = {0x1, 0x4, 0x10, 0x10, 0x40, 0x10, 0x4, 0x40, 0x10};

/* Position bits of a mask and the rating byte of the player data for each (0x5b7d20, 0x6e2f20):
 * GK, sweeper, D, DM, M, AM, F. Sides: 0x80 left (+0x18), 0x800 right (+0x17), 0x100, 0x200, 0x400
 * centre (+0x19). */
static const struct { uint16_t bit; uint8_t at; } positions[] = {
    {0x1, 0x0f}, {0x2, 0x10}, {0x4, 0x11}, {0x8, 0x12}, {0x10, 0x13}, {0x20, 0x14}, {0x40, 0x15}};

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

static uint32_t call_cdecl(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, n, args);
    cpu = saved;
    return r;
}

/* The first fixture of club (team 1 = first team) from today to today + 30 days, or 0. */
static uint32_t next_fixture(uint32_t club)
{
    uint32_t to = rt_alloc_zero(8), list = rt_alloc_zero(16);
    uint32_t a[2] = {to, 30};
    call_this(0x54c5b0u, TODAY, 2, a);
    uint32_t f[9] = {rd16(g_mem, TODAY), rd16(g_mem, TODAY + 2), rd16(g_mem, to), rd16(g_mem, to + 2),
                     rd32(g_mem, club), list, 0xffffffffu, 0xfffffffeu, 1};
    uint32_t fixture = 0;
    if ((int16_t)call_this(0x5acbe0u, rd32(g_mem, FIXTURES), 9, f) > 0) {
        wr32(g_mem, list + 0xa, rd32(g_mem, list + 2));
        fixture = call_this(0x68d1b0u, list, 0, NULL);
    }
    call_this(0x68d040u, list, 0, NULL);
    rt_free(list);
    rt_free(to);
    return fixture;
}

/* Set during picker_compute: the wrap of 0x62c680 skips the daily fitness update. The store of
 * picker_pick runs without it: its calls (0x896fe0, 0x896a90, 0x897760, 0x6e1cf0) do not reach the
 * daily update or the random numbers (research, section "store"). */
static int picking;

void o_0062c680(void);
int playersearch_holds_update(uint32_t staff);     /* playersearch.c: an extra player of the full search */

/* The daily fitness update of one player, thiscall (staff), ret 4. It draws random numbers and changes
 * the player (0x62c680 pseudocode). */
RT_WRAP(0062c680, daily_update)
{
    if (picking || playersearch_holds_update(rt_arg(0))) {
        rt_ret_stdcall(cpu.eax, 1);
        return;
    }
    o_0062c680();
}

#define ATTRIBUTES 31             /* the shown attributes, 1 to 31 in the getter 0x5431e0 (names: 0x5428a0) */
enum { ACCELERATION = 1, AGGRESSION, AGILITY, ANTICIPATION, BALANCE, BRAVERY, CREATIVITY, CROSSING,
       DECISIONS, DETERMINATION, DRIBBLING, FINISHING, FLAIR, HANDLING, HEADING, INFLUENCE, JUMPING,
       LONG_SHOTS, MARKING, OFF_THE_BALL, PACE, PASSING, POSITIONING, REFLEXES, SET_PIECES, STAMINA,
       STRENGTH, TACKLING, TEAMWORK, TECHNIQUE, WORK_RATE };

/* The key attributes of each position (spec 2026-10-15-features-0.4.0, Q138 to Q140), ended by 0. A
 * sweeper uses the DC list, a slot without a side (a sub) the centre list, and AML, AMR and DM on a side
 * the AMC and DMC lists. */
enum { ROLE_GK, ROLE_DC, ROLE_DLR, ROLE_DMC, ROLE_MC, ROLE_MLR, ROLE_AM, ROLE_FC };
static const uint8_t keys[][7] = {
    [ROLE_GK] = {HANDLING, REFLEXES, AGILITY, POSITIONING, JUMPING},
    [ROLE_DC] = {TACKLING, MARKING, HEADING, POSITIONING, STRENGTH, JUMPING},
    [ROLE_DLR] = {TACKLING, MARKING, PACE, POSITIONING, CROSSING},
    [ROLE_DMC] = {TACKLING, POSITIONING, PASSING, TEAMWORK, WORK_RATE},
    [ROLE_MC] = {PASSING, DECISIONS, TEAMWORK, WORK_RATE, TECHNIQUE},
    [ROLE_MLR] = {CROSSING, DRIBBLING, PACE, PASSING, TECHNIQUE},
    [ROLE_AM] = {PASSING, CREATIVITY, DECISIONS, TECHNIQUE, FLAIR},
    [ROLE_FC] = {FINISHING, OFF_THE_BALL, ANTICIPATION, TECHNIQUE, PACE},   /* Q140 */
};

/* attr[k] is 0 for an attribute that the screen of the viewer hides (0x543280). */
typedef struct { uint32_t staff, data; int condition, age, ability; uint8_t attr[ATTRIBUTES + 1]; } Candidate;

/* By condition, then by staff address, so that the order (and a pick among equal values) is the same on
 * every system: qsort is not stable. */
static int by_condition(const void *a, const void *b)
{
    const Candidate *x = a, *y = b;
    if (x->condition != y->condition) return y->condition - x->condition;
    return x->staff < y->staff ? -1 : x->staff > y->staff;
}

/* The fit of a player for a slot mask, 1 to 20: the lower of the position and the side rating. For D or
 * DM with a side, the wing-back rating (+0x16) counts as a position rating, as in the original's scorer
 * 0x6e2f20. When bit is not NULL it gets the position bit of the mask whose rating gives the fit (the
 * first on a tie; D or DM for the wing-back rating). */
static int fit_bit(const Candidate *c, uint16_t mask, uint16_t *bit)
{
    int pos = 0, side = 20;
    if (bit) *bit = 0;
    for (unsigned i = 0; i < sizeof positions / sizeof *positions; i++)
        if (mask & positions[i].bit) {
            int r = rd8(g_mem, c->data + positions[i].at);
            if (r > pos) { pos = r; if (bit) *bit = positions[i].bit; }
        }
    if (!(mask & 1) && (mask & 0x880) && (mask & 0xc)) {
        int r = rd8(g_mem, c->data + 0x16);
        if (r > pos) { pos = r; if (bit) *bit = (mask & 0x4) ? 0x4 : 0x8; }
    }
    if (!(mask & 1)) {
        if (mask & 0x80) side = rd8(g_mem, c->data + 0x18);
        else if (mask & 0x800) side = rd8(g_mem, c->data + 0x17);
        else if (mask & 0x700) side = rd8(g_mem, c->data + 0x19);
    }
    return pos < side ? pos : side;
}

static int fit(const Candidate *c, uint16_t mask) { return fit_bit(c, mask, NULL); }

/* The key attributes of a slot's position, for a player: the role and the fit, as value() takes them. */
static int role_of(const Candidate *c, uint16_t mask, int *f)
{
    uint16_t bit;
    *f = fit_bit(c, mask, &bit);
    int wide = (mask & 0x880) != 0;
    return bit == 0x1 ? ROLE_GK : bit == 0x2 ? ROLE_DC : bit == 0x4 ? (wide ? ROLE_DLR : ROLE_DC)
         : bit == 0x8 ? ROLE_DMC : bit == 0x10 ? (wide ? ROLE_MLR : ROLE_MC) : bit == 0x20 ? ROLE_AM
         : ROLE_FC;
}

/* The value of a player in a slot: the sum of the key attributes of the position times fit times 60
 * divided by their count (5 or 6), an exact integer. The score shown is value / 1200: the average of the
 * key attributes times fit / 20. -1 when the player cannot take the slot: a goalkeeper slot needs a
 * goalkeeper rating of 10 or more. PICK_UNSCORED when every key attribute is hidden from the viewer: he
 * has no score, and ranks after every player with one (rank_of). */
static int value(const Candidate *c, uint16_t mask)
{
    if ((mask & 1) && rd8(g_mem, c->data + 0x0f) < ABLE) return -1;
    int f, role = role_of(c, mask, &f);
    return pick_value(c->attr, keys[role], f);
}

static int rank_of(int v) { return pick_rank(v); }

/* The order of a pick, as weights of one player in one slot: an empty slot is worst, then a player with a
 * fit below 10, then a player from outside the pick's first group (kinds 2 and 3), then the score. Each
 * step outweighs any sum of the steps below it, so the best total first fills every slot it can, then
 * takes the fewest players with a fit below 10 (Q131), then the fewest from outside the first group
 * (Q142), then the highest score. So a player with a fit below 10 plays only when no free player, of any
 * group, has 10 or more for that slot: else the swap would lower the count of such players. */
#define W_OUT 1000000LL          /* more than 20 slots x 20 x 20 x 60 */
#define W_LOW (W_OUT * 21)
#define W_EMPTY (W_LOW * 21)
#define COLUMNS (50 + SLOTS)     /* the candidates and one "empty" column per slot */

static long long weight(const Candidate *c, int first, uint16_t mask)
{
    int r = rank_of(value(c, mask));
    if (r < 0) return -W_EMPTY;
    /* The rank less 1: a player with no score weighs -1, after every player with a score, with the same fit
     * and group. */
    return r - 1 - (fit(c, mask) < ABLE ? W_LOW : 0) - (first ? 0 : W_OUT);
}

/* The assignment of rows (slots) to columns of the highest total weight, exactly: the Hungarian method
 * on the cost -weight (rows <= columns, O(rows^2 x columns)). col_of[r] is the column of row r. */
static void hungarian(int rows, int cols, long long w[][COLUMNS], int *col_of)
{
    long long u[SLOTS + 1] = {0}, v[COLUMNS + 1] = {0}, minv[COLUMNS + 1];
    int p[COLUMNS + 1] = {0}, way[COLUMNS + 1] = {0}, used[COLUMNS + 1];
    for (int i = 1; i <= rows; i++) {
        p[0] = i;
        int j0 = 0;
        for (int j = 0; j <= cols; j++) { minv[j] = LLONG_MAX; used[j] = 0; }
        do {
            used[j0] = 1;
            int i0 = p[j0], j1 = 0;
            long long delta = LLONG_MAX;
            for (int j = 1; j <= cols; j++) {
                if (used[j]) continue;
                long long cur = -w[i0 - 1][j - 1] - u[i0] - v[j];
                if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
                if (minv[j] < delta) { delta = minv[j]; j1 = j; }
            }
            for (int j = 0; j <= cols; j++)
                if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
                else minv[j] -= delta;
            j0 = j1;
        } while (p[j0]);
        do { int j1 = way[j0]; p[j0] = p[j1]; j0 = j1; } while (j0);
    }
    for (int j = 1; j <= cols; j++)
        if (p[j]) col_of[p[j] - 1] = j - 1;
}

/* Fills team[lo..hi) with the best assignment of the candidates not taken. */
static void assign(const Candidate *c, int n, const int *first, const uint16_t *masks, int lo, int hi,
                   int *taken, int *team)
{
    long long w[SLOTS][COLUMNS];
    int rows = 0, row_slot[SLOTS], cols = 0, col_cand[COLUMNS], col_of[SLOTS];
    for (int s = lo; s < hi; s++) row_slot[rows++] = s;
    for (int p = 0; p < n; p++)
        if (!taken[p]) col_cand[cols++] = p;
    int players = cols;
    for (int r = 0; r < rows; r++) col_cand[cols++] = -1;        /* empty */
    for (int r = 0; r < rows; r++)
        for (int k = 0; k < cols; k++)
            w[r][k] = col_cand[k] < 0 ? -W_EMPTY : weight(&c[col_cand[k]], first[col_cand[k]], masks[row_slot[r]]);
    hungarian(rows, cols, w, col_of);
    for (int r = 0; r < rows; r++) {
        int k = col_of[r], s = row_slot[r];
        team[s] = -1;
        if (k < players && w[r][k] > -W_EMPTY) {
            team[s] = col_cand[k];
            taken[col_cand[k]] = 1;
        }
    }
}

/* The pick: the eleven first, the best assignment of all candidates (Q130). The subs then come from the
 * players left over, by the same rule. */
static void solve(const Candidate *c, int n, const int *first, const uint16_t *masks, int slots, int *team)
{
    int taken[50] = {0};
    assign(c, n, first, masks, 0, SLOTS, taken, team);
    assign(c, n, first, masks, SLOTS, slots, taken, team);      /* MAX_SUBS <= SLOTS */
}

/* The best of the 11 slots for a player: the slot of his highest value (score), on a tie the higher fit
 * (Q156). Gives the fit there, and in *mask the position bit that gives it with the slot's side bits.
 * Returns the value there (-1 when no slot takes him). */
static int best_slot(const Candidate *c, const uint16_t *masks, int *best_fit, uint16_t *mask)
{
    int best = -1, best_rank = -1;
    *best_fit = 0;
    *mask = 0;
    for (int s = 0; s < SLOTS; s++) {
        uint16_t bit;
        int f = fit_bit(c, masks[s], &bit), v = value(c, masks[s]), r = rank_of(v);
        if (r < 0 || r < best_rank || (r == best_rank && f <= *best_fit)) continue;
        best = v;
        best_rank = r;
        *best_fit = f;
        *mask = (uint16_t)(bit | (masks[s] & 0xf80));
    }
    return best;
}

/* The bench of a pick (Pick.bench): the subs in slot order, then the other candidates by their best value,
 * then by staff address (a fixed order on every system). */
static void bench(const Candidate *c, int n, const uint16_t *masks, int slots, const int *team, Pick *out)
{
    int in_team[50] = {0}, order[50], values[50], k = 0;
    for (int s = 0; s < slots; s++)
        if (team[s] >= 0) in_team[team[s]] = s < SLOTS ? 1 : 2;
    out->bench = 0;
    for (int s = SLOTS; s < slots; s++)
        if (team[s] >= 0) order[k++] = team[s];
    int subs = k;
    for (int i = 0; i < n; i++)
        if (!in_team[i]) order[k++] = i;
    for (int i = 0; i < k; i++) {
        int f;
        uint16_t m;
        values[i] = rank_of(best_slot(&c[order[i]], masks, &f, &m));
    }
    for (int i = subs + 1; i < k; i++)          /* insertion sort of the others */
        for (int j = i; j > subs && (values[j] > values[j - 1] ||
             (values[j] == values[j - 1] && c[order[j]].staff < c[order[j - 1]].staff)); j--) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
            t = values[j]; values[j] = values[j - 1]; values[j - 1] = t;
        }
    for (int i = 0; i < k; i++) {
        out->bench_staff[i] = c[order[i]].staff;
        out->bench_score[i] = best_slot(&c[order[i]], masks, &out->bench_fit[i], &out->bench_mask[i]);
    }
    out->bench = k;
}

/* Set by the picker check (RECOMP_PICK_CHECK): picker_compute writes its candidates and its pick here. */
static FILE *dump_to;

/* A name of a staff record: the common name, else first and second name (staff +0xc, +0x4, +0x8 point
 * to name records with the text at +0). */
static void staff_name(uint32_t staff, char *out, size_t size)
{
    uint32_t common = rd32(g_mem, staff + 0xc), first = rd32(g_mem, staff + 0x4), second = rd32(g_mem, staff + 0x8);
    if (common && g_mem[common]) snprintf(out, size, "%s", (const char *)(g_mem + common));
    else snprintf(out, size, "%s %s", first ? (const char *)(g_mem + first) : "",
                  second ? (const char *)(g_mem + second) : "");
}

/* The one rule for an attribute that another club's player shows to a viewer, for best XI, squad depth and
 * the comparison: the value that the player's screens show (0x5431e0, 1 to 20), or 0 when the game hides
 * it from the viewer's manager. The game's test 0x543280 reads the masking flag [0x9cc270] itself, and
 * answers "shown" at once when the flag is 0, when there is no unknown-player table [0xae2a60], or when the
 * viewer does not hide the player. The viewer 0 hides nothing. 0x543440, the profile's getter, calls the
 * same 0x543280 and then 0x543540, which 0x5431e0 also reaches, so this gives the profile's numbers. */
int picker_attribute_known(uint32_t viewer, uint32_t staff, uint32_t k)
{
    uint32_t q[3] = {viewer, staff, k};
    return !viewer || (call_cdecl(KNOWN, 3, q) & 0xff);
}

int picker_shown_attribute(uint32_t viewer, uint32_t staff, uint32_t k)
{
    if (!picker_attribute_known(viewer, staff, k)) return 0;
    uint32_t a[2] = {staff, k};
    uint32_t v = call_cdecl(0x5431e0u, 2, a) & 0xff;
    return (int)(v > 20 ? 20 : v);
}

/* RECOMP_VIEWS_LOG=1: what the viewer sees of a player: the 31 values (0 hidden) and the game's own answer
 * of 0x543280 for each (1 hidden), for the check that no hidden value is shown (tests/smoke). */
static void log_hidden(uint32_t viewer, uint32_t staff, const uint8_t *attr)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_VIEWS_LOG") != NULL;
    if (!on) return;
    fprintf(stderr, "recomp: picker attributes staff %d values", (int32_t)rd32(g_mem, staff));
    for (uint32_t k = 1; k <= ATTRIBUTES; k++) fprintf(stderr, " %d", attr[k]);
    fprintf(stderr, " hidden");
    for (uint32_t k = 1; k <= ATTRIBUTES; k++) fprintf(stderr, " %d", !picker_attribute_known(viewer, staff, k));
    fprintf(stderr, "\n");
}

/* The candidates of a club's squad (club +0xd7) into c, returns their count. Without out, only the
 * players who can play the next match (fixture, 0 for none). With out, every player, and out[i] is 1 for
 * a player who cannot play. */
static int load(uint32_t club, uint32_t fixture, uint32_t viewer, Candidate *c, int *out)
{
    int n = 0;
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t staff = rd32(g_mem, club + SQUAD + 4 * i);
        if (!staff || !rd32(g_mem, staff + PLAYER_DATA)) continue;
        if (rd32(g_mem, staff) >= rd32(g_mem, STAFF_COUNT)) continue;     /* a virtual player */
        uint32_t s1[1] = {staff}, s2[2] = {staff, 1};
        /* Without a fixture 0x45cbe0 answers yes at once, so test injury and national duty here too. */
        uint32_t intl[5] = {staff, TODAY, club, 0, 0};
        uint32_t av[4] = {staff, fixture, club, TODAY};
        /* The reason, for Best XI's tags: 1 injured, 2 away with a national team, 3 the squad screen's test
         * (a ban, a loan or the competition's rules). */
        int cannot = (call_this(0x62c400u, rd32(g_mem, FITNESS), 1, s1) & 0xff) == 2   /* al */ ? 1
                   : call_this(0x7757d0u, rd32(g_mem, NATIONS), 5, intl)                ? 2
                   : fixture && !(call_cdecl(AVAILABLE, 4, av) & 0xff)                 ? 3 : 0;
        if (cannot && !out) continue;
        if (out) out[n] = cannot;
        c[n].staff = staff;
        c[n].data = rd32(g_mem, staff + PLAYER_DATA);
        c[n].condition = (int16_t)call_this(0x62c590u, rd32(g_mem, FITNESS), 2, s2);
        c[n].age = (int16_t)rd16(g_mem, staff + AGE);
        c[n].ability = (int16_t)rd16(g_mem, c[n].data + ABILITY);    /* for the picker check only */
        for (uint32_t k = 1; k <= ATTRIBUTES; k++) c[n].attr[k] = (uint8_t)picker_shown_attribute(viewer, staff, k);
        if (viewer) log_hidden(viewer, staff, c[n].attr);
        n++;
    }
    return n;
}

static int compute(uint32_t club, uint32_t team_index, int kind, const uint16_t *formation, uint32_t viewer,
                   Pick *out)
{
    memset(out, 0, sizeof *out);
    uint32_t manager = rd32(g_mem, club + MANAGER);
    uint32_t m[1] = {manager};
    uint32_t tactic = formation ? 0 : manager ? call_this(0x5fbd00u, rd32(g_mem, HUMANS), 1, m) : 0;
    if ((!tactic && !formation) || kind < 1 || kind > 3) return 0;
    uint32_t fixture = team_index ? 0 : next_fixture(club);
    int subs = fixture ? (int)(rd8(g_mem, fixture + 0x4d) & 0xf) : DEFAULT_SUBS;
    if (subs > MAX_SUBS) subs = MAX_SUBS;
    int slots = SLOTS + subs;
    uint16_t masks[SLOTS + MAX_SUBS];
    for (int s = 0; s < slots; s++)
        masks[s] = s >= SLOTS ? sub_masks[s - SLOTS]
                 : formation ? formation[s] : (uint16_t)rd16(g_mem, tactic + 0x12d + 2 * (uint32_t)s);

    Candidate c[50];
    int n = load(club, fixture, viewer, c, NULL);
    qsort(c, (size_t)n, sizeof *c, by_condition);

    int first[50], n_first = 0, gk = 0;
    for (int i = 0; i < n; i++) {
        if (kind == 1) first[i] = 1;
        else if (kind == 3) first[i] = c[i].age < 21;
        else {                    /* the fit players, at least enough for the team, and a goalkeeper */
            int keeper = rd8(g_mem, c[i].data + 0x0f) >= NATURAL;
            first[i] = c[i].condition >= FIT || n_first < slots || (keeper && !gk);
            if (first[i]) gk |= keeper;
        }
        n_first += first[i];
    }

    int team[SLOTS + MAX_SUBS];
    solve(c, n, first, masks, slots, team);

    out->slots = slots;
    for (int s = 0; s < slots; s++) {
        out->masks[s] = masks[s];
        out->staff[s] = team[s] >= 0 ? c[team[s]].staff : 0;
        out->fit[s] = team[s] >= 0 ? fit(&c[team[s]], masks[s]) : 0;
        out->score[s] = team[s] >= 0 ? value(&c[team[s]], masks[s]) : 0;
    }
    bench(c, n, masks, slots, team, out);
    if (dump_to) {
        fprintf(dump_to, "pick\t%d\t", kind);
        for (int s = 0; s < slots; s++) fprintf(dump_to, "%s%x", s ? " " : "", masks[s]);
        fprintf(dump_to, "\n");
        for (int i = 0; i < n; i++) {
            char name[128];
            staff_name(c[i].staff, name, sizeof name);
            char ratings[3 * 11 + 1];
            for (uint32_t k = 0; k < 11; k++)
                snprintf(ratings + 3 * k, 4, "%02x ", rd8(g_mem, c[i].data + 0x0f + k));
            ratings[3 * 11 - 1] = 0;
            fprintf(dump_to, "cand\t%d\t%s\t%s\t", i, name, ratings);
            for (int k = 1; k <= ATTRIBUTES; k++) fprintf(dump_to, "%s%d", k > 1 ? " " : "", c[i].attr[k]);
            fprintf(dump_to, "\t%d\t%d\t%d\t%d", c[i].ability, c[i].age, c[i].condition, first[i]);
            for (int s = 0; s < slots; s++) fprintf(dump_to, "\t%d", fit(&c[i], masks[s]));
            for (int s = 0; s < slots; s++) fprintf(dump_to, "\t%d", value(&c[i], masks[s]));
            fprintf(dump_to, "\nstaff\t%d\t%u\t%u\n", i, rd32(g_mem, c[i].staff), rd32(g_mem, STAFF_COUNT));
        }
        fprintf(dump_to, "team\t");
        for (int s = 0; s < slots; s++) fprintf(dump_to, "%s%d", s ? " " : "", team[s]);
        fprintf(dump_to, "\n");
    }
    int picked = 0;
    for (int s = 0; s < slots; s++) picked += out->staff[s] != 0;
    return picked;
}

/* Stores a pick as the club's selection: drop the undo copy and clear (0x896fe0), then each slot and the
 * captain. */
static int store(uint32_t club, uint32_t team_index, const Pick *p)
{
    uint32_t drop[4] = {club, team_index, 0, 1}, clear[4] = {club, team_index, 0, 0};
    uint32_t sel = rd32(g_mem, STORE);
    call_this(0x896fe0u, sel, 4, drop);
    call_this(0x896fe0u, sel, 4, clear);
    int picked = 0, captain = -1, best = 0;
    for (int s = 0; s < p->slots; s++) {
        if (!p->staff[s]) continue;
        uint32_t staff = p->staff[s];
        uint32_t set[4] = {club, (uint32_t)s, staff, team_index};
        call_this(0x896a90u, sel, 4, set);
        picked++;
        if (s < SLOTS) {          /* the captain: the best value of 0x6e1cf0, as 0x6e1940 takes */
            uint32_t k[3] = {staff, 0, club};
            int v = (int16_t)call_cdecl(0x6e1cf0u, 3, k);   /* a s16 (0x6e19f1) */
            if (captain < 0 || v > best) { captain = s; best = v; }
        }
    }
    if (captain >= 0) {
        uint32_t cap[3] = {club, (uint32_t)captain, team_index};
        call_this(0x897760u, sel, 3, cap);
    }
    return picked;
}

int picker_compute(uint32_t club, uint32_t team_index, int kind, Pick *out)
{
    return picker_compute_view(club, team_index, kind, NULL, 0, out);
}

/* formation: 11 slot masks in place of the manager's active tactic, or NULL. viewer: a manager whose
 * screens hide some attributes of other clubs' players (0x543280, as the squad list 0x45c74c asks with
 * the manager of the player slot, 0x456b1b); a hidden attribute does not count in the score. 0: all. */
int picker_compute_view(uint32_t club, uint32_t team_index, int kind, const uint16_t *formation, uint32_t viewer,
                        Pick *out)
{
    picking = 1;
    int n = compute(club, team_index, kind, formation, viewer, out);
    picking = 0;
    return n;
}

/* Squad depth (Q106, Q107, Q161): for each row, every player of the squad who can take one of its slots,
 * unavailable players too, with his best value over the row's slots (and the fit there). xi's players in
 * the row's slots rank as the best available players (their value raised to the highest value of the
 * available players; among them by their own value), so the first available players of a row are best
 * XI's players there. An unavailable player with a higher value stays above them. The others by value,
 * then fit, then staff address. Keeps the first k + 2 of a row of k slots. Returns the number of players. */
int picker_depth(uint32_t club, uint32_t team_index, const uint16_t *formation, uint32_t viewer, const Pick *xi,
                 const int *row_of, int rows, Depth *out)
{
    memset(out, 0, sizeof *out);
    picking = 1;
    Candidate c[50];
    int cannot[50];
    int n = load(club, team_index ? 0 : next_fixture(club), viewer, c, cannot);
    picking = 0;
    for (int r = 0; r < rows && r < SLOTS; r++) {
        int order[50], values[50], fits[50], keys[50], picks[50], k = 0, top = -1, slots = 0;
        for (int s = 0; s < SLOTS; s++) slots += row_of[s] == r;
        for (int i = 0; i < n; i++) {
            int v = -1, f = 0, pick = 0;
            for (int s = 0; s < SLOTS; s++) {
                if (row_of[s] != r) continue;
                int vs = rank_of(value(&c[i], formation[s])), fs = fit(&c[i], formation[s]);
                if (vs > v || (vs == v && fs > f)) { v = vs; f = fs; }
                pick |= !cannot[i] && xi && c[i].staff == xi->staff[s];
            }
            if (v < 0) continue;
            order[k] = i;
            values[k] = v;
            fits[k] = f;
            picks[k] = pick;
            if (!cannot[i] && v > top) top = v;
            k++;
        }
        for (int j = 0; j < k; j++) keys[j] = picks[j] ? top : values[j];
        for (int i = 1; i < k; i++)            /* insertion sort: key, the picks, value, fit, staff address */
            for (int j = i; j > 0; j--) {
                int x = j, y = j - 1;
                int above = keys[x] != keys[y] ? keys[x] > keys[y] : picks[x] != picks[y] ? picks[x]
                          : values[x] != values[y] ? values[x] > values[y]
                          : fits[x] != fits[y] ? fits[x] > fits[y] : c[order[x]].staff < c[order[y]].staff;
                if (!above) break;
                int t;
                t = order[x]; order[x] = order[y]; order[y] = t;
                t = values[x]; values[x] = values[y]; values[y] = t;
                t = fits[x]; fits[x] = fits[y]; fits[y] = t;
                t = keys[x]; keys[x] = keys[y]; keys[y] = t;
                t = picks[x]; picks[x] = picks[y]; picks[y] = t;
            }
        int keep = slots + 2;
        out->n[r] = k < keep ? k : keep;
        for (int j = 0; j < out->n[r]; j++) {
            out->staff[r][j] = c[order[j]].staff;
            out->score[r][j] = values[j] ? values[j] - 1 : PICK_UNSCORED;
            out->fit[r][j] = fits[j];
            out->out[r][j] = cannot[order[j]];
        }
    }
    return n;
}

/* kind 1: best team, 2: most match-fit, 3: under-21. Returns the number of players picked. */
int picker_pick(uint32_t club, uint32_t team_index, int kind)
{
    Pick p;
    if (!picker_compute(club, team_index, kind, &p)) return 0;
    return store(club, team_index, &p);
}

/* The picker check (tests/smoke/picker_check.py): with RECOMP_PICK_CHECK=<file>, at the first human turn
 * the runtime computes the three picks of the player's club into the file and runs "Pick best team".
 * Then it returns 1, and the human turn (testrun.c) ends the program with code 0. Else it returns 0. */
int picker_check_turn(void)
{
    static int done;
    const char *path = getenv("RECOMP_PICK_CHECK");
    if (done || !path || !*path) return 0;
    done = 1;
    /* The manager's club (staff +0x39), else his national team (+0x24), as holiday_next.c club_of. */
    uint32_t manager = rd32(g_mem, PLAYER_MANAGER + (uint32_t)(int16_t)rd16(g_mem, PLAYER_SLOT) * 0x300u);
    uint32_t club = manager ? rd32(g_mem, manager + 0x39) : 0;
    if (manager && !club) club = rd32(g_mem, manager + 0x24);
    dump_to = fopen(path, "w");
    if (!dump_to) rt_fatal("picker check: cannot write %s", path);
    Pick p;
    for (int kind = 1; kind <= 3; kind++) picker_compute(club, 0, kind, &p);
    fclose(dump_to);
    dump_to = NULL;
    int n = picker_pick(club, 0, 1);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "picked\t%d\n", n); fclose(f); }
    fprintf(stderr, "recomp: picker check of club %#x done, %d players stored\n", club, n);
    return 1;
}
