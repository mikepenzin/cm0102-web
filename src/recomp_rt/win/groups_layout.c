/* Attribute groups (#43): the layout of the profile's attribute table (groups_layout.h). */
#include "groups_layout.h"

#include <stddef.h>
#include <string.h>

/* The groups of each column, as cells ended by -1. Inside a group the attributes are in alphabetical
 * order, the five rows of the original stay in its order after Physical (Q122, Q124). */
static const int outfield[GROUP_COLUMNS][GROUP_ROWS + 1] = {
    {CELL_HEAD_TECHNICAL, ATTR_CROSSING, ATTR_DRIBBLING, ATTR_FINISHING, ATTR_HEADING, ATTR_LONG_SHOTS,
     ATTR_MARKING, ATTR_PASSING, ATTR_SET_PIECES, ATTR_TACKLING, ATTR_TECHNIQUE, -1},
    {CELL_HEAD_MENTAL, ATTR_AGGRESSION, ATTR_ANTICIPATION, ATTR_BRAVERY, ATTR_CREATIVITY, ATTR_DECISIONS,
     ATTR_DETERMINATION, ATTR_FLAIR, ATTR_INFLUENCE, ATTR_OFF_THE_BALL, ATTR_POSITIONING, ATTR_TEAMWORK,
     ATTR_WORK_RATE, -1},
    {CELL_HEAD_PHYSICAL, ATTR_ACCELERATION, ATTR_AGILITY, ATTR_BALANCE, ATTR_JUMPING, ATTR_PACE, ATTR_STAMINA,
     ATTR_STRENGTH, CELL_FOOT, CELL_FORM, CELL_MORALE, CELL_CONDITION, CELL_ON_LOAN, -1},
};

/* A goalkeeper: one Goalkeeping group that also holds Set Pieces and Technique (Max, 2026-10-21: no
 * Technical group). Crossing, Dribbling, Finishing, Heading, Long Shots, Marking and Tackling are not shown
 * (Q129). One On Ones and Consistency come from the player data, which the original does not show (Max,
 * 2026-10-21). */
static const int keeper_first[GROUP_ROWS + 1] = {CELL_HEAD_GOALKEEPING, ATTR_HANDLING, ATTR_REFLEXES,
    CELL_ONE_ON_ONES, CELL_CONSISTENCY, ATTR_PASSING, ATTR_SET_PIECES, ATTR_TECHNIQUE, -1};

const char *groups_heading(int cell)
{
    switch (cell) {
    case CELL_HEAD_GOALKEEPING: return "Goalkeeping";
    case CELL_HEAD_TECHNICAL: return "Technical";
    case CELL_HEAD_MENTAL: return "Mental";
    case CELL_HEAD_PHYSICAL: return "Physical";
    default: return NULL;
    }
}

const char *groups_hidden_name(int cell)
{
    switch (cell) {
    case CELL_ONE_ON_ONES: return "One On Ones";
    case CELL_CONSISTENCY: return "Consistency";
    default: return NULL;
    }
}

/* The positions text 0x53ee50 adds a position when its rating is at least a threshold. The threshold starts at
 * 15 (0x53ef62). The goalkeeper (+0x0f) comes first (0x53ef9e). The others that add text are the sweeper
 * (+0x10, 0x53f0b0), the defender (+0x11, 0x53f1c2), the defensive midfielder (+0x12, 0x53f2d4), the midfielder
 * (+0x13, alone or with the attacking midfielder, 0x53f3af and 0x53f496) and the attacker (+0x15, 0x53f5a8).
 * The attacking midfielder alone adds no text. When the goalkeeper rating is below the threshold (0x53f7b1),
 * the text is empty and the threshold is 10 or more, the threshold goes down by 1 and the text is made again
 * (0x53f7b9..0x53f7da). So the text starts with the goalkeeper when his rating is 15 or more, or when it is
 * 10 or more and no other position of the text has a higher rating. */
int groups_is_keeper(const signed char ratings[GROUP_RATINGS])
{
    int keeper = ratings[0];
    if (keeper >= 15) return 1;
    if (keeper < 10) return 0;
    for (int i = 1; i < GROUP_RATINGS; i++)
        if (i != 5 && ratings[i] > keeper) return 0;
    return 1;
}

void groups_layout(int keeper, GroupLayout *out)
{
    memset(out, 0, sizeof *out);
    for (int c = 0; c < GROUP_COLUMNS; c++) {
        const int *src = keeper && c == 0 ? keeper_first : outfield[c];
        for (int r = 0; src[r] >= 0; r++) out->cell[c][r] = src[r];
    }
}
