/* Test of the attribute groups layout (src/recomp_rt/win/groups_layout.c).
 * Usage: groups_test */
#include "groups_layout.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

/* The English names of the game's attributes 1 to 31 (0x5428a0), for the alphabetical check. */
static const char *names[32] = {"", "Acceleration", "Aggression", "Agility", "Anticipation", "Balance",
    "Bravery", "Creativity", "Crossing", "Decisions", "Determination", "Dribbling", "Finishing", "Flair",
    "Handling", "Heading", "Influence", "Jumping", "Long Shots", "Marking", "Off The Ball", "Pace", "Passing",
    "Positioning", "Reflexes", "Set Pieces", "Stamina", "Strength", "Tackling", "Teamwork", "Technique",
    "Work Rate"};

/* 1 when column c holds exactly the cells of want (ended by -1), then blanks. */
static int column_is(const GroupLayout *l, int c, const int *want)
{
    int r = 0;
    for (; want[r] >= 0; r++)
        if (r >= GROUP_ROWS || l->cell[c][r] != want[r]) return 0;
    for (; r < GROUP_ROWS; r++)
        if (l->cell[c][r] != CELL_BLANK) return 0;
    return 1;
}

/* How often cell occurs in the layout. */
static int occurs(const GroupLayout *l, int cell)
{
    int n = 0;
    for (int c = 0; c < GROUP_COLUMNS; c++)
        for (int r = 0; r < GROUP_ROWS; r++) n += l->cell[c][r] == cell;
    return n;
}

/* 1 when the attributes between headings of each column are in alphabetical order. */
static int alphabetical(const GroupLayout *l)
{
    for (int c = 0; c < GROUP_COLUMNS; c++) {
        const char *last = "";
        for (int r = 0; r < GROUP_ROWS; r++) {
            int k = l->cell[c][r];
            if (groups_heading(k)) last = "";
            else if (k >= 1 && k <= 31) {
                if (strcmp(last, names[k]) >= 0) return 0;
                last = names[k];
            }
        }
    }
    return 1;
}

int main(void)
{
    GroupLayout l;

    /* 1. Outfield player: Technical, Mental, Physical with heading rows (Q122, Q124). */
    groups_layout(0, &l);
    static const int tech[] = {CELL_HEAD_TECHNICAL, ATTR_CROSSING, ATTR_DRIBBLING, ATTR_FINISHING, ATTR_HEADING,
        ATTR_LONG_SHOTS, ATTR_MARKING, ATTR_PASSING, ATTR_SET_PIECES, ATTR_TACKLING, ATTR_TECHNIQUE, -1};
    static const int mental[] = {CELL_HEAD_MENTAL, ATTR_AGGRESSION, ATTR_ANTICIPATION, ATTR_BRAVERY,
        ATTR_CREATIVITY, ATTR_DECISIONS, ATTR_DETERMINATION, ATTR_FLAIR, ATTR_INFLUENCE, ATTR_OFF_THE_BALL,
        ATTR_POSITIONING, ATTR_TEAMWORK, ATTR_WORK_RATE, -1};
    static const int physical[] = {CELL_HEAD_PHYSICAL, ATTR_ACCELERATION, ATTR_AGILITY, ATTR_BALANCE,
        ATTR_JUMPING, ATTR_PACE, ATTR_STAMINA, ATTR_STRENGTH, CELL_FOOT, CELL_FORM, CELL_MORALE,
        CELL_CONDITION, CELL_ON_LOAN, -1};
    CHECK(column_is(&l, 0, tech));
    CHECK(column_is(&l, 1, mental));
    CHECK(column_is(&l, 2, physical));
    CHECK(occurs(&l, ATTR_HANDLING) == 0 && occurs(&l, ATTR_REFLEXES) == 0);
    for (int k = 1; k <= 31; k++)
        if (k != ATTR_HANDLING && k != ATTR_REFLEXES) CHECK(occurs(&l, k) == 1);
    CHECK(alphabetical(&l));

    CHECK(occurs(&l, CELL_ONE_ON_ONES) == 0 && occurs(&l, CELL_CONSISTENCY) == 0);

    /* 2. Goalkeeper: one Goalkeeping group with One On Ones, Consistency, Set Pieces and Technique, and no
     * Technical heading (Q129, Max 2026-10-21). */
    groups_layout(1, &l);
    static const int keeper[] = {CELL_HEAD_GOALKEEPING, ATTR_HANDLING, ATTR_REFLEXES, CELL_ONE_ON_ONES,
        CELL_CONSISTENCY, ATTR_PASSING, ATTR_SET_PIECES, ATTR_TECHNIQUE, -1};
    CHECK(column_is(&l, 0, keeper));
    CHECK(column_is(&l, 1, mental));
    CHECK(column_is(&l, 2, physical));
    static const int hidden[] = {ATTR_CROSSING, ATTR_DRIBBLING, ATTR_FINISHING, ATTR_HEADING, ATTR_LONG_SHOTS,
        ATTR_MARKING, ATTR_TACKLING};
    for (int i = 0; i < 7; i++) CHECK(occurs(&l, hidden[i]) == 0);
    CHECK(occurs(&l, CELL_HEAD_TECHNICAL) == 0);

    /* 3. The heading texts. */
    CHECK(!strcmp(groups_heading(CELL_HEAD_GOALKEEPING), "Goalkeeping"));
    CHECK(!strcmp(groups_heading(CELL_HEAD_TECHNICAL), "Technical"));
    CHECK(groups_heading(ATTR_PACE) == NULL && groups_heading(CELL_BLANK) == NULL);
    CHECK(groups_heading(CELL_ONE_ON_ONES) == NULL);

    /* 4. The names of the hidden attribute cells. */
    CHECK(!strcmp(groups_hidden_name(CELL_ONE_ON_ONES), "One On Ones"));
    CHECK(!strcmp(groups_hidden_name(CELL_CONSISTENCY), "Consistency"));
    CHECK(groups_hidden_name(ATTR_HANDLING) == NULL && groups_hidden_name(CELL_HEAD_GOALKEEPING) == NULL);

    /* 5. A goalkeeper from the position ratings (+0x0f to +0x15), as the positions text 0x53ee50 puts the
     * goalkeeper first. The old test read "GK" from that text, which is English only (0.6.1 item 8). */
    static const struct { signed char r[GROUP_RATINGS]; int keeper; } cases[] = {
        {{20, 1, 1, 1, 1, 1, 1}, 1},     /* a goalkeeper (Seaman) */
        {{1, 1, 18, 20, 20, 1, 1}, 0},   /* an outfield player (Vieira) */
        {{15, 1, 18, 1, 1, 1, 1}, 1},    /* 15 or more: the goalkeeper comes first at threshold 15 */
        {{14, 1, 15, 1, 1, 1, 1}, 0},    /* a defender of 15: the text is D at threshold 15 */
        {{12, 1, 11, 1, 1, 1, 1}, 1},    /* no position of 15: the threshold goes down to 12 */
        {{12, 1, 1, 1, 1, 1, 13}, 0},    /* an attacker of 13 comes at threshold 13 */
        {{12, 12, 1, 1, 1, 1, 1}, 1},    /* the same rating: the goalkeeper is first in the text */
        {{12, 1, 1, 1, 1, 14, 1}, 1},    /* an attacking midfielder alone adds no text */
        {{9, 1, 1, 1, 1, 1, 1}, 0},      /* below 10: the threshold stops at 10, no goalkeeper */
        {{10, 1, 1, 1, 1, 1, 1}, 1},     /* 10 is the last threshold */
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        if (groups_is_keeper(cases[i].r) != cases[i].keeper) {
            printf("FAIL keeper case %d\n", (int)i);
            failures++;
        }
    }

    if (failures) printf("%d failures\n", failures);
    else printf("groups: all checks passed\n");
    return failures != 0;
}
