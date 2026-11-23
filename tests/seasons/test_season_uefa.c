/* The UEFA formats table (0.6.0 slice 22): the rows that the game reads for each season. ctest season_uefa. */
#include "season.h"
#include "season_uefa.h"

#include <stdio.h>
#include <string.h>

static int failures;

/* Stand-ins of the season of the game (season.c). */
static int code_on = 1, start_year = 2001, changing = 0;
int season_code_on(void) { return code_on; }
int season_start_year(void) { return start_year; }
int season_changing_rules(void) { return changing; }

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
            failures++;                                                             \
        }                                                                           \
    } while (0)

int main(void)
{
    CHECK(season_uefa_row_count >= 8);
    for (int i = 1; i < season_uefa_row_count; i++) CHECK(season_uefa_rows[i].season == season_uefa_rows[i - 1].season + 1);

    /* Two group stages and a quarter-final to 2002/03 (the original's format), one group stage and a round
     * of 16 from 2003/04 (change point U1). */
    const SeasonUefa *r2001 = season_uefa_table_row(2001), *r2002 = season_uefa_table_row(2002);
    const SeasonUefa *r2003 = season_uefa_table_row(2003), *r2008 = season_uefa_table_row(2008);
    CHECK(r2001 && r2001->season == 2001 && !season_uefa_cl_round_of_16(r2001));
    CHECK(r2002 && !season_uefa_cl_round_of_16(r2002));
    CHECK(season_uefa_cl_round(r2002, "Groups 2") == 16 && season_uefa_cl_round(r2002, "KO") == 8);
    CHECK(r2003 && r2003->season == 2003 && season_uefa_cl_round_of_16(r2003));
    CHECK(season_uefa_cl_round(r2003, "Q3") == 32 && season_uefa_cl_round(r2003, "Groups") == 32);

    /* UEFA Cup groups from 2004/05 (U2): 8 groups of 5, single round robin, a round of 32 after them. */
    CHECK(r2003->uc_groups == 0);
    const SeasonUefa *r2004 = season_uefa_table_row(2004);
    CHECK(r2004->uc_groups == 8 && r2004->uc_group_clubs == 5 && r2004->uc_meetings == 1);
    CHECK(season_uefa_uc_round(r2004, "R1") == 80 && season_uefa_uc_round(r2004, "KO") == 32);

    /* The Intertoto Cup: 5 rounds and 3 winners into the UEFA Cup first round to 2005, then 3 rounds and 11
     * winners into the second qualifying round to 2008 (U3). */
    CHECK(season_uefa_table_row(2005)->uic_rounds == 5 && season_uefa_table_row(2005)->uic_winners == 3);
    CHECK(!strcmp(season_uefa_table_row(2005)->uic_entry, "R1"));
    CHECK(season_uefa_table_row(2006)->uic_rounds == 3 && season_uefa_table_row(2006)->uic_winners == 11);
    CHECK(!strcmp(season_uefa_table_row(2006)->uic_entry, "Q2"));

    /* Slice 23. The Europa League from 2009/10 (U4): 12 groups of 4, double round robin, top two and the 8
     * Champions League thirds to a round of 32. The UEFA Cup groups of 2004/05 to 2008/09 send three. */
    const SeasonUefa *r2009 = season_uefa_table_row(2009), *r2018 = season_uefa_table_row(2018);
    CHECK(r2008->uc_groups == 8 && r2008->uc_through == 3);
    CHECK(r2009 && r2009->season == 2009 && r2009->uc_groups == 12 && r2009->uc_group_clubs == 4);
    CHECK(r2009->uc_meetings == 2 && r2009->uc_through == 2 && season_uefa_uc_round(r2009, "KO") == 32);
    CHECK(season_uefa_uc_round(r2009, "PO") == 76 && season_uefa_uc_round(r2009, "Groups") == 48);
    /* The Intertoto Cup is not held from 2009 (U3). */
    CHECK(r2008->uic_clubs == 50 && r2009->uic_clubs == -1 && r2009->uic_rounds == 0);
    /* The Champions League paths from 2009/10 (U5, Q296): Q3 and PO of two paths. */
    CHECK(season_uefa_cl_round(r2009, "Q3 CP") == 20 && season_uefa_cl_round(r2009, "Q3 LP") == 10);
    CHECK(season_uefa_cl_level(r2009, "Q3") == 30 && season_uefa_cl_level(r2009, "PO") == 20);
    CHECK(season_uefa_cl_level(r2009, "Q2") == 34 && season_uefa_cl_round_of_16(r2009));
    CHECK(season_uefa_cl_level(r2008, "Q3") == 32);
    /* 2018/19: a preliminary round, paths from Q2; Europa League paths CP and MP. */
    CHECK(season_uefa_cl_round(r2018, "PR") == 4 && season_uefa_cl_level(r2018, "Q2") == 24);
    CHECK(season_uefa_uc_level(r2018, "Q2") == 92 && season_uefa_uc_round(r2018, "Q2 MP") == 74);
    /* The Super Cup: Monaco to 2012, then a venue each year (U6). */
    CHECK(!strcmp(r2008->sc_city, "Monaco") && r2008->sc_day == 29 && r2008->sc_month == 7);
    const SeasonUefa *r2013 = season_uefa_table_row(2013);
    CHECK(!strcmp(r2013->sc_city, "Prague") && r2013->sc_day == 30 && r2013->sc_month == 7);
    CHECK(!strcmp(season_uefa_table_row(2020)->sc_city, "Budapest") && season_uefa_table_row(2020)->sc_month == 8);

    /* The access list by rank (Q369): none to 2008/09, the real list from 2009/10. */
    CHECK(season_uefa_access_places(2008, 'c', 1) == NULL);
    CHECK(!strcmp(season_uefa_access_places(2009, 'c', 1), "G G G PO-LP"));
    CHECK(!strcmp(season_uefa_access_places(2009, 'c', 39), "Q2"));       /* Liechtenstein's rank: the next bracket */
    CHECK(!strcmp(season_uefa_access_places(2009, 'c', 70), "Q1"));       /* after the last bracket: the last */
    CHECK(!strcmp(season_uefa_access_places(2009, 'e', 1), "PO PO Q3"));
    CHECK(!strcmp(season_uefa_access_places(2018, 'c', 4), "G G G G"));  /* four places for the top four (U7) */
    CHECK(!strcmp(season_uefa_access_places(2017, 'c', 4), "G G PO-LP"));
    CHECK(!strcmp(season_uefa_access_places(2030, 'c', 4), "G G G G"));  /* after the table: the last season */
    CHECK(!strcmp(season_uefa_access_rule(2009, 'h'), "G"));
    CHECK(!strcmp(season_uefa_access_rule(2015, 'X'), "PO-LP") && !strcmp(season_uefa_access_rule(2018, 'X'), "G"));
    CHECK(season_uefa_access_league_cup(2009, "France") && season_uefa_access_league_cup(2015, "England"));
    CHECK(!season_uefa_access_league_cup(2009, "Spain") && !season_uefa_access_league_cup(2008, "England"));
    CHECK(season_uefa_access_rule(2015, 'f') && season_uefa_access_rule(2016, 'f') == NULL);
    /* Open item 70: the association limit of Art. 3 (4 to 2014/15, 5 from 2015/16, 7 from 2024/25). */
    CHECK(season_uefa_cl_cap(2009) == 4 && season_uefa_cl_cap(2012) == 4 && season_uefa_cl_cap(2014) == 4);
    CHECK(season_uefa_cl_cap(2015) == 5 && season_uefa_cl_cap(2017) == 5 && season_uefa_cl_cap(2023) == 5);
    CHECK(season_uefa_cl_cap(2024) == 7 && season_uefa_cl_cap(2300) == 7);

    /* The new clubs of each entry code. 2009/10 Champions League: Q1 4, Q2 34 (2 Q1 winners and 32 new),
     * Q3 CP 20 (17 and 3), Q3 LP 10, PO CP 10 (10 winners), PO LP 10 (5 and 5), groups 32 (10 and 22). */
    int t[SEASON_UEFA_CODE_SLOTS];
    season_uefa_new_clubs(r2009, 'c', t);
    CHECK(t[SEASON_UEFA_CL_Q1] == 4 && t[SEASON_UEFA_CL_Q2_CP] == 32 && t[SEASON_UEFA_CL_Q3_CP] == 3);
    CHECK(t[SEASON_UEFA_CL_Q3_LP] == 10 && t[SEASON_UEFA_CL_PO_CP] == 0 && t[SEASON_UEFA_CL_PO_LP] == 5);
    CHECK(t[SEASON_UEFA_CL_G] == 22 && t[SEASON_UEFA_CL_Q2_LP] == 0 && t[SEASON_UEFA_CL_PR] == 0);
    /* Europa League 2009/10: Q1 46, Q2 80 (23 and 57), Q3 70 (40 and 30), PO 76 (35 winners, 15 Champions
     * League Q3 losers, 26 new), groups 48 (38 winners, 10 Champions League PO losers). */
    season_uefa_new_clubs(r2009, 'e', t);
    CHECK(season_uefa_dropped(r2009, SEASON_UEFA_EL_PO) == 15 && season_uefa_dropped(r2009, SEASON_UEFA_EL_G) == 10);
    CHECK(t[SEASON_UEFA_EL_Q1] == 46 && t[SEASON_UEFA_EL_Q2] == 57 && t[SEASON_UEFA_EL_Q3] == 30);
    CHECK(t[SEASON_UEFA_EL_PO] == 26 && t[SEASON_UEFA_EL_G] == 0 && t[SEASON_UEFA_EL_PR] == 0);
    /* 2018/19: the preliminary round of 4 (semi-finals and a final, one winner): CL Q1 31 new, groups 26 new;
     * EL Q2 takes the 3 preliminary round losers and the 16 Q1 losers; EL PR 14 (7 winners), Q1 87 new. */
    season_uefa_new_clubs(r2018, 'c', t);
    CHECK(t[SEASON_UEFA_CL_PR] == 4 && t[SEASON_UEFA_CL_Q1] == 31 && t[SEASON_UEFA_CL_G] == 26);
    CHECK(t[SEASON_UEFA_CL_Q2_LP] == 4 && t[SEASON_UEFA_CL_Q3_LP] == 6 && t[SEASON_UEFA_CL_Q3_CP] == 2);
    int cl_sum = 0, el_sum = 0;
    for (int i = 0; i < SEASON_UEFA_CODE_SLOTS; i++) cl_sum += t[i];
    CHECK(cl_sum == 79);                                                /* the row's 79 clubs */
    CHECK(season_uefa_dropped(r2018, SEASON_UEFA_EL_Q2) == 19 && season_uefa_dropped(r2018, SEASON_UEFA_EL_G) == 10);
    season_uefa_new_clubs(r2018, 'e', t);
    CHECK(t[SEASON_UEFA_EL_PR] == 14 && t[SEASON_UEFA_EL_Q1] == 87);
    for (int i = 0; i < SEASON_UEFA_CODE_SLOTS; i++) el_sum += t[i];
    CHECK(el_sum + 19 + 12 + 6 + 10 + 8 == 213);                         /* 158 entered and 55 from the CL */
    CHECK(season_uefa_dropped(r2018, SEASON_UEFA_EL_Q3) == 12 && season_uefa_dropped(r2018, SEASON_UEFA_EL_PO) == 6);
    CHECK(season_uefa_code('c', "Q3-CP", 5) == SEASON_UEFA_CL_Q3_CP && season_uefa_code('e', "Q3-CP", 5) == SEASON_UEFA_EL_Q3);
    CHECK(season_uefa_dropped(r2008, SEASON_UEFA_EL_G) == 0);

    /* Slice 24. The Europa League of 2021/22 (U8): Q3, play-offs, 8 groups of 4, the winners to the round of
     * 16, the runners-up and the 8 Champions League thirds to knockout round play-offs of 16. */
    const SeasonUefa *r2020 = season_uefa_table_row(2020), *r2021 = season_uefa_table_row(2021);
    const SeasonUefa *r2022 = season_uefa_table_row(2022), *r2023 = season_uefa_table_row(2023);
    CHECK(r2021 && r2021->season == 2021 && r2023 && r2023->season == 2023);
    CHECK(r2021->uc_groups == 8 && r2021->uc_group_clubs == 4 && r2021->uc_meetings == 2 && r2021->uc_through == 2);
    CHECK(r2021->uc_ko_po == 16 && season_uefa_uc_round(r2021, "KO") == 16 && season_uefa_uc_round(r2021, "PO") == 20);
    CHECK(season_uefa_uc_level(r2021, "Q3") == 16 && season_uefa_uc_round(r2021, "Q2 MP") == 0);
    CHECK(r2020->uc_ko_po == 0 && r2020->uc_groups == 12);
    /* No away goals rule from 2021/22 (U9). */
    CHECK(r2020->away_goals == 1 && r2021->away_goals == 0 && r2023->away_goals == 0 && r2009->away_goals == 1);
    /* Russia has no UEFA places from 2022/23 (Q293, Q365); in 2021/22 the places of the start stay (Q278). */
    CHECK(!strcmp(r2021->suspended, "") && !strcmp(r2022->suspended, "Russia") && !strcmp(r2023->suspended, "Russia"));
    CHECK(season_uefa_suspended(r2022, "Russia") && !season_uefa_suspended(r2021, "Russia"));
    CHECK(!season_uefa_suspended(r2022, "Russ") && !season_uefa_suspended(r2022, "England"));
    /* The Conference League from 2021/22 (U8): its rounds in the table. */
    CHECK(r2020->ecl_round_count == 0 && season_uefa_ecl_round(r2021, "Q1") == 66);
    CHECK(season_uefa_ecl_level(r2021, "Q2") == 108 && season_uefa_ecl_level(r2021, "PO") == 44);
    CHECK(r2021->ecl_groups == 8 && r2021->ecl_group_clubs == 4 && r2021->ecl_ko_po == 16);
    CHECK(season_uefa_ecl_round(r2022, "Q1") == 60 && season_uefa_ecl_round(r2023, "Q1") == 62);
    /* The Champions League losers into the Europa League, 2021/22: Q2 losers to Q3 (10 and 3), Champions Path
     * Q3 losers to the play-offs (6), League Path Q3 losers and play-off losers to the groups (4 and 6). */
    CHECK(season_uefa_dropped(r2021, SEASON_UEFA_EL_Q3) == 13 && season_uefa_dropped(r2021, SEASON_UEFA_EL_PO) == 6);
    CHECK(season_uefa_dropped(r2021, SEASON_UEFA_EL_G) == 10 && season_uefa_dropped(r2021, SEASON_UEFA_EL_Q2) == 0);
    season_uefa_new_clubs(r2021, 'e', t);
    CHECK(t[SEASON_UEFA_EL_Q3] == 3 && t[SEASON_UEFA_EL_PO] == 6 && t[SEASON_UEFA_EL_G] == 12);
    CHECK(t[SEASON_UEFA_EL_Q2] == 0 && t[SEASON_UEFA_EL_Q1] == 0);
    season_uefa_new_clubs(r2022, 'e', t);
    CHECK(t[SEASON_UEFA_EL_Q3] == 2 && t[SEASON_UEFA_EL_PO] == 7 && t[SEASON_UEFA_EL_G] == 12);
    /* The access list of 2021/22: Europa League places only for the associations ranked 1 to 15, the rest go to
     * the Conference League. */
    CHECK(!strcmp(season_uefa_access_places(2021, 'c', 1), "G G G G"));
    CHECK(!strcmp(season_uefa_access_places(2021, 'e', 1), "G G") && !strcmp(season_uefa_access_places(2021, 'e', 13), "Q3"));
    CHECK(!strcmp(season_uefa_access_places(2021, 'e', 20), "") && !strcmp(season_uefa_access_places(2021, 'e', 60), ""));
    CHECK(!strcmp(season_uefa_access_places(2021, 'k', 1), "PO") && !strcmp(season_uefa_access_rule(2021, 'X'), "G"));
    CHECK(!season_uefa_access_league_cup(2021, "England") && !strcmp(season_uefa_access_rule(2022, 'Y'), "G"));
    /* The Super Cup of 2023 at Piraeus, penalties after 90 minutes (no extra time from 2023). */
    CHECK(!strcmp(r2021->sc_city, "Belfast") && !strcmp(r2023->sc_city, "Piraeus") && r2023->sc_day == 16);
    CHECK(r2022->sc_extra_time == 1 && r2023->sc_extra_time == 0 && r2008->sc_extra_time == 1);

    /* Part 24b, the Conference League of 2021/22 with one path a round: Q1 66 new; Q2 108 = 33 Q1 winners, the 3
     * preliminary round and 16 first qualifying round losers of the Champions League, 56 new; Q3 64 = 54 winners
     * and 10 new; PO 44 = 32 winners, the 8 third qualifying round losers of the Europa League and 4 new; groups 32
     * = 22 winners and the 10 Europa League play-off losers. 136 entered, as the row says. */
    CHECK(season_uefa_code('k', "Q2", 2) == SEASON_UEFA_ECL_Q2 && season_uefa_code('k', "PO", 2) == SEASON_UEFA_ECL_PO);
    CHECK(season_uefa_code_clubs(r2021, 'k', SEASON_UEFA_ECL_Q2) == 108 && season_uefa_code_clubs(r2020, 'k', SEASON_UEFA_ECL_Q1) == 0);
    CHECK(season_uefa_dropped(r2021, SEASON_UEFA_ECL_Q2) == 19 && season_uefa_dropped(r2021, SEASON_UEFA_ECL_PO) == 8);
    CHECK(season_uefa_dropped(r2021, SEASON_UEFA_ECL_G) == 10 && season_uefa_dropped(r2021, SEASON_UEFA_ECL_Q1) == 0);
    CHECK(season_uefa_dropped(r2020, SEASON_UEFA_ECL_Q2) == 0);
    season_uefa_new_clubs(r2021, 'k', t);
    CHECK(t[SEASON_UEFA_ECL_Q1] == 66 && t[SEASON_UEFA_ECL_Q2] == 56 && t[SEASON_UEFA_ECL_Q3] == 10);
    CHECK(t[SEASON_UEFA_ECL_PO] == 4 && t[SEASON_UEFA_ECL_G] == 0 && t[SEASON_UEFA_EL_Q3] == 0);
    int ecl_sum = 0;
    for (int i = 0; i < SEASON_UEFA_CODE_SLOTS; i++) ecl_sum += t[i];
    CHECK(ecl_sum == 136);
    CHECK(r2021->ecl_meetings == 2 && r2021->ecl_through == 2 && !strcmp(r2020->ecl_drops, ""));
    /* The league cup winner's Conference League place (rows M), the Conference League holder in the Europa
     * League groups from 2022/23 (Y). */
    CHECK(season_uefa_access_ecl_league_cup(2021, "England") && !season_uefa_access_ecl_league_cup(2021, "France"));
    CHECK(season_uefa_access_rule(2021, 'Y') == NULL && !strcmp(season_uefa_access_rule(2023, 'Y'), "G"));

    /* Outside the table: the first row before it, the last row after it (Q248). */
    CHECK(season_uefa_table_row(1999) == &season_uefa_rows[0]);
    CHECK(season_uefa_table_row(2099) == &season_uefa_rows[season_uefa_row_count - 1]);
    CHECK(season_uefa_table_row(2099)->season >= r2008->season);

    /* The row of the game that runs: none for the base game and with the season code off (the original runs),
     * the row of each season with changing rules on, the row of the start year with changing rules off. */
    start_year = 2001, changing = 0;
    CHECK(season_uefa_row(2001) == NULL && season_uefa_row(2005) == NULL);
    changing = 1;
    CHECK(season_uefa_row(2003) == r2003 && season_uefa_row(2001) == r2001);
    code_on = 0;
    CHECK(season_uefa_row(2003) == NULL);
    code_on = 1, start_year = 2005, changing = 0;
    CHECK(season_uefa_row(2007) == season_uefa_table_row(2005) && season_uefa_row(2003) == season_uefa_table_row(2005));
    changing = 1;
    CHECK(season_uefa_row(2007) == season_uefa_table_row(2007));
    start_year = 2010, changing = 0;
    CHECK(season_uefa_row(2010) == season_uefa_table_row(2010) && season_uefa_row(2012) == season_uefa_table_row(2010));
    code_on = 0;
    CHECK(season_uefa_row(2010) == NULL);

    /* Issue 119: with stages a winner of the cup's own round table (stage -1: Q1, Q2, R1 of 2004/05 to 2008/09,
     * the qualifying rounds from 2009/10) goes on to the group stage. It is not the cup winner (outcome 1 gave
     * the cup winners record 0x86a800 and "Fans celebrate winning the UEFA Cup"). Only the final of the knockout
     * stage gives outcome 1. A loser of any round stays out (2 or -1), and a winner of an earlier round stays 0. */
    CHECK(season_uefa_cup_outcome(-1, 1) == 0);
    CHECK(season_uefa_cup_outcome(-1, 2) == 2 && season_uefa_cup_outcome(-1, -1) == -1);
    CHECK(season_uefa_cup_outcome(-1, 0) == 0);
    CHECK(season_uefa_cup_outcome(8, 1) == 1 && season_uefa_cup_outcome(1, 1) == 1);
    CHECK(season_uefa_cup_outcome(8, 2) == 2 && season_uefa_cup_outcome(8, 0) == 0);

    if (failures) return 1;
    printf("season_uefa: all checks pass\n");
    return 0;
}
