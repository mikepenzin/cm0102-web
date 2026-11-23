/* Attribute groups (#43): the layout of the attribute table of the player profile, without game memory,
 * so that tests/groups/test_groups.c can test it (spec 2026-10-15-features-0.4.0, Q122 to Q129). */
#ifndef RECOMP_GROUPS_LAYOUT_H
#define RECOMP_GROUPS_LAYOUT_H

#define GROUP_COLUMNS 3
#define GROUP_ROWS 13            /* the rows of a column with heading rows (the original has 12) */

/* The kinds of cell. 1 to 31 are the attribute numbers of the game's getter (names: 0x5428a0), in
 * alphabetical order of their English names. */
enum {
    CELL_BLANK = 0,
    ATTR_ACCELERATION = 1, ATTR_AGGRESSION, ATTR_AGILITY, ATTR_ANTICIPATION, ATTR_BALANCE, ATTR_BRAVERY,
    ATTR_CREATIVITY, ATTR_CROSSING, ATTR_DECISIONS, ATTR_DETERMINATION, ATTR_DRIBBLING, ATTR_FINISHING,
    ATTR_FLAIR, ATTR_HANDLING, ATTR_HEADING, ATTR_INFLUENCE, ATTR_JUMPING, ATTR_LONG_SHOTS, ATTR_MARKING,
    ATTR_OFF_THE_BALL, ATTR_PACE, ATTR_PASSING, ATTR_POSITIONING, ATTR_REFLEXES, ATTR_SET_PIECES,
    ATTR_STAMINA, ATTR_STRENGTH, ATTR_TACKLING, ATTR_TEAMWORK, ATTR_TECHNIQUE, ATTR_WORK_RATE,
    /* The five rows that the original adds after the sorted attributes, in its order. */
    CELL_FOOT = 32, CELL_FORM, CELL_MORALE, CELL_CONDITION, CELL_ON_LOAN,
    /* Heading rows. */
    CELL_HEAD_GOALKEEPING, CELL_HEAD_TECHNICAL, CELL_HEAD_MENTAL, CELL_HEAD_PHYSICAL,
    /* Goalkeeper attributes of the player data that the original does not show (Max, 2026-10-21). */
    CELL_ONE_ON_ONES, CELL_CONSISTENCY
};

typedef struct {
    int cell[GROUP_COLUMNS][GROUP_ROWS];       /* blank cells fill each column to GROUP_ROWS */
} GroupLayout;

/* Fills out for a goalkeeper (keeper 1) or an outfield player. Every column has its heading rows, in
 * classic mode too (Q125). */
void groups_layout(int keeper, GroupLayout *out);

/* The position ratings of the player data that the positions text reads, +0x0f to +0x15 (1 to 20):
 * goalkeeper, sweeper, defender, defensive midfielder, midfielder, attacking midfielder, attacker. */
#define GROUP_RATINGS 7

/* 1 when the positions text of the player data starts with the goalkeeper, as the profile and the squad list
 * show it, in every language (database_format_player_positions 0x53ee50, style 1). ratings holds the bytes
 * +0x0f to +0x15 of the player data. */
int groups_is_keeper(const signed char ratings[GROUP_RATINGS]);

/* The text of a heading cell, or NULL for any other cell. */
const char *groups_heading(int cell);

/* The name of a hidden attribute cell (CELL_ONE_ON_ONES, CELL_CONSISTENCY), or NULL for any other cell. */
const char *groups_hidden_name(int cell);

#endif
