/* The closed league table and plan (0.7.3 slice 6, issue 157). */
#include <stdio.h>
#include <string.h>
#include "season_closed.h"
#include "season_mls_plan.h"

static int fails;

#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); \
                                             printf("\n"); } } while (0)

/* The clubs of the rows of docs/research/rules-0.6.0/arg-usa.tsv (arg-usa-217 and on), real seasons 2002 to 2027. */
static const int ROW[][2] = {
    {2002, 10}, {2003, 10}, {2004, 10}, {2005, 12}, {2006, 12}, {2007, 13}, {2008, 14}, {2009, 15}, {2010, 16},
    {2011, 18}, {2012, 19}, {2013, 19}, {2014, 19}, {2015, 20}, {2016, 20}, {2017, 22}, {2018, 23}, {2019, 24},
    {2020, 26}, {2021, 27}, {2022, 28}, {2023, 29}, {2024, 29}, {2025, 30}, {2026, 30}, {2027, 30},
};

static void test_table(void)
{
    for (size_t k = 0; k < sizeof ROW / sizeof ROW[0]; k++)
        CHECK(season_closed_count("usa_mls", ROW[k][0]) == ROW[k][1], "%d: %d clubs, the row %d", ROW[k][0],
              season_closed_count("usa_mls", ROW[k][0]), ROW[k][1]);
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", NULL, 2001) == 1, "Miami Fusion 2001");
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", NULL, 2002) == 0, "Miami Fusion 2002");
    CHECK(season_closed_member("usa_mls", "Tampa Bay Mutiny", NULL, 2002) == 0, "Tampa Bay 2002");
    CHECK(season_closed_member("usa_mls", "Inter Miami CF", NULL, 2019) == 0, "Inter Miami 2019");
    CHECK(season_closed_member("usa_mls", "Inter Miami CF", NULL, 2020) == 1, "Inter Miami 2020");
    CHECK(season_closed_member("usa_mls", "San Jose Earthquakes", NULL, 2005) == 1, "San Jose 2005");
    CHECK(season_closed_member("usa_mls", "San Jose Earthquakes", NULL, 2006) == 0, "San Jose 2006");
    CHECK(season_closed_member("usa_mls", "San Jose Earthquakes", NULL, 2008) == 1, "San Jose 2008");
    CHECK(season_closed_member("usa_mls", "Chivas USA", NULL, 2014) == 1, "Chivas 2014");
    CHECK(season_closed_member("usa_mls", "Chivas USA", NULL, 2015) == 0, "Chivas 2015");
    /* The names of the databases. */
    CHECK(season_closed_member("usa_mls", "NY/NJ Metrostars", NULL, 2010) == 1, "Metrostars");
    CHECK(season_closed_member("usa_mls", "NYNJ Metrostars", NULL, 2010) == 1, "NYNJ Metrostars");
    CHECK(season_closed_member("usa_mls", "Washington DC United", NULL, 2010) == 1, "DC United");
    CHECK(season_closed_member("usa_mls", "Dallas Burn", NULL, 2010) == 1, "Dallas Burn");
    CHECK(season_closed_member("usa_mls", "CF Montr\xe9" "al", NULL, 2012) == 1, "CF Montreal");
    CHECK(season_closed_member("usa_mls", "Montreal Impact", NULL, 2011) == 0, "Montreal 2011");
    /* Clubs of the same towns that the table does not name. */
    CHECK(season_closed_member("usa_mls", "San Diego Loyal SC", NULL, 2025) == -1, "San Diego Loyal");
    CHECK(season_closed_member("usa_mls", "Minnesota Thunder", NULL, 2017) == -1, "Minnesota Thunder");
    CHECK(season_closed_member("usa_mls", "Nashville Metros", NULL, 2020) == -1, "Nashville Metros");
    CHECK(season_closed_member("usa_mls", "Cincinnati Riverhawks", NULL, 2019) == -1, "Cincinnati Riverhawks");
    CHECK(season_closed_member("usa_mls", "Toronto Lynx", NULL, 2007) == -1, "Toronto Lynx");
    CHECK(season_closed_member("no_league", "Chicago Fire", NULL, 2010) == -1, "another league");
    /* The update databases give a retired record to a newer club: the short name decides. */
    CHECK(season_closed_member("usa_mls", "Tampa Bay Mutiny", "LAFC", 2024) == 1, "LAFC record 2024");
    CHECK(season_closed_member("usa_mls", "Tampa Bay Mutiny", "LAFC", 2017) == 0, "LAFC record 2017");
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", "Inter Miami", 2024) == 1, "Inter Miami record");
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", "Miami", 2002) == 0, "the real Miami Fusion");
    CHECK(season_closed_member("usa_mls", "Tampa Bay Mutiny", "Tampa Bay", 2002) == 0, "the real Tampa Bay");
    CHECK(season_closed_member("usa_mls", "Tampa Bay Mutiny", "Seattle", 2010) == 1, "Seattle record");
    CHECK(season_closed_member("usa_mls", "Tampa Bay Mutiny", "Houston Dynamo", 2010) == 1, "Houston record");
    CHECK(season_closed_member("usa_mls", "Tampa Bay Mutiny", "Portland", 2022) == 1, "Portland record");
    CHECK(season_closed_member("usa_mls", "Tampa Bay Mutiny", "Real Salt Lake", 2012) == 1, "Salt Lake record");
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", "Philadephia", 2011) == 1, "Philadelphia spelling");
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", "Atlanta Utd", 2019) == 1, "Atlanta record");
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", "NYCFC", 2022) == 1, "NYCFC record");
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", "Toronto", 2018) == 1, "Toronto record");
    CHECK(season_closed_member("usa_mls", "Miami Fusion FC", "Houston", 2013) == 1, "Houston short");
    CHECK(season_closed_member("usa_mls", "San Jose Earthquakes", "Houston Dynamo", 2007) == 1, "San Jose record");
    CHECK(season_closed_member("usa_mls", "Toronto Lynx", "Toronto", 2007) == -1, "Toronto Lynx keeps out");
    CHECK(season_mls_conference(season_closed_real_name("usa_mls", "Tampa Bay Mutiny", "LAFC"), 2024) == MLS_WEST,
          "the LAFC record plays in the West");
    CHECK(!strcmp(season_closed_real_name("usa_mls", "Toronto Lynx", "Toronto"), "Toronto Lynx"), "a name of no row");
    /* Every club of the table has a conference (season_mls_plan.c). */
    for (int i = 0; season_closed_real("usa_mls", i); i++) {
        const char *real = season_closed_real("usa_mls", i);
        CHECK(season_mls_conference(real, season_closed_first("usa_mls", i)) >= 0, "%s has no conference", real);
    }
}

static void test_plan(void)
{
    int a[16];
    ClosedResult r;
    /* 2002: the two clubs that fold leave, and no other club moves. */
    ClosedClub y2002[] = {{"Miami Fusion FC", 1, 0, 0, 10},    {"Tampa Bay Mutiny", 1, 0, 0, 10},
                          {"Chicago Fire", 1, 0, 0, 90},       {"Colorado Rapids", 1, 0, 0, 50},
                          {"Columbus Crew", 1, 0, 0, 50},      {"Washington DC United", 1, 0, 0, 50},
                          {"Dallas Burn", 1, 0, 0, 50},        {"Los Angeles Galaxy", 1, 0, 0, 50},
                          {"New England Revolution", 1, 0, 0, 50}, {"NYNJ Metrostars", 1, 0, 0, 50},
                          {"San Jose Earthquakes", 1, 0, 0, 50},   {"Kansas City Wizards", 1, 0, 0, 50},
                          {"Seattle Sounders", 0, 0, 0, 30}};
    season_closed_plan("usa_mls", 2002, 10, y2002, 13, a, &r);
    CHECK(a[0] == CLOSED_FOLD && a[1] == CLOSED_FOLD, "2002 folds %d %d", a[0], a[1]);
    CHECK(a[2] == CLOSED_STAY && a[12] == CLOSED_STAY, "2002 stays %d %d", a[2], a[12]);
    CHECK(r.folded == 2 && r.joined == 0 && r.count == 10 && r.missing == 0, "2002 result %d %d %d %d", r.folded,
          r.joined, r.count, r.missing);
    /* A human manager keeps the club in the league. */
    y2002[0].human = 1;
    season_closed_plan("usa_mls", 2002, 10, y2002, 13, a, &r);
    CHECK(a[0] == CLOSED_STAY && a[1] == CLOSED_FOLD && r.human_stays == 1 && r.count == 11, "human %d %d %d %d", a[0],
          a[1], r.human_stays, r.count);
    /* A club joins in its season, and never its reserve side or a club of the same town. */
    ClosedClub y2009[] = {{"Seattle Sounders FC 2", 0, 0, 1, 90}, {"Seattle Sounders", 0, 0, 0, 30},
                          {"Portland Timbers", 0, 0, 0, 30},      {"Chicago Fire", 1, 0, 0, 50}};
    season_closed_plan("usa_mls", 2009, 0, y2009, 4, a, &r);
    CHECK(a[0] == CLOSED_STAY && a[1] == CLOSED_JOIN && a[2] == CLOSED_STAY && a[3] == CLOSED_STAY, "2009 %d %d %d %d",
          a[0], a[1], a[2], a[3]);
    CHECK(r.joined == 1 && r.missing == 13, "2009 joined %d missing %d", r.joined, r.missing);
    /* San Diego FC joins in 2025, and San Diego Loyal never does. */
    ClosedClub y2025[] = {{"San Diego Loyal SC", 0, 0, 0, 90}, {"San Diego FC", 0, 0, 0, 10}};
    season_closed_plan("usa_mls", 2024, 0, y2025, 2, a, &r);
    CHECK(a[0] == CLOSED_STAY && a[1] == CLOSED_STAY, "2024 %d %d", a[0], a[1]);
    season_closed_plan("usa_mls", 2025, 0, y2025, 2, a, &r);
    CHECK(a[0] == CLOSED_STAY && a[1] == CLOSED_JOIN, "2025 %d %d", a[0], a[1]);
    /* October 2023: the two retired records play as LAFC and Inter Miami, so they stay and San Diego FC makes 30. */
    ClosedClub slots[] = {{"Tampa Bay Mutiny", 1, 0, 0, 80, "LAFC"}, {"Miami Fusion FC", 1, 0, 0, 80, "Inter Miami"},
                          {"San Diego FC", 0, 0, 0, 10, "San Diego"}};
    season_closed_plan("usa_mls", 2025, 0, slots, 3, a, &r);
    CHECK(a[0] == CLOSED_STAY && a[1] == CLOSED_STAY && a[2] == CLOSED_JOIN, "slots %d %d %d", a[0], a[1], a[2]);
    CHECK(r.folded == 0 && r.joined == 1, "slots folded %d joined %d", r.folded, r.joined);
    /* A club that the table does not name leaves only above the count, the lowest reputation first. */
    ClosedClub extra[] = {{"Stand In A", 1, 0, 0, 20}, {"Stand In B", 1, 0, 0, 10}, {"Stand In C", 1, 1, 0, 5},
                          {"Chicago Fire", 1, 0, 0, 50}};
    season_closed_plan("usa_mls", 2010, 3, extra, 4, a, &r);
    CHECK(a[0] == CLOSED_STAY && a[1] == CLOSED_CUT && a[2] == CLOSED_STAY && a[3] == CLOSED_STAY && r.cut == 1 &&
          r.count == 3, "cut %d %d %d %d, %d", a[0], a[1], a[2], a[3], r.count);
    season_closed_plan("usa_mls", 2010, 4, extra, 4, a, &r);
    CHECK(r.cut == 0 && r.count == 4, "no cut at the count: %d", r.cut);
}

/* The A-League: rows aus-jap-kor-28 to -49 of docs/research/rules-0.6.0/aus-jap-kor.tsv (0.7.3 slice 7, issue 157). */
static const int AUS_ROW[][2] = {
    {2005, 8},  {2006, 8},  {2007, 8},  {2008, 8},  {2009, 10}, {2010, 11}, {2011, 10}, {2012, 10},
    {2013, 10}, {2014, 10}, {2015, 10}, {2016, 10}, {2017, 10}, {2018, 10}, {2019, 11}, {2020, 12},
    {2021, 12}, {2022, 12}, {2023, 12}, {2024, 13}, {2025, 12}, {2026, 12},
};

static void test_a_league(void)
{
    for (size_t k = 0; k < sizeof AUS_ROW / sizeof AUS_ROW[0]; k++)
        CHECK(season_closed_count("aus_nsl", AUS_ROW[k][0]) == AUS_ROW[k][1], "%d: %d clubs, the row %d", AUS_ROW[k][0],
              season_closed_count("aus_nsl", AUS_ROW[k][0]), AUS_ROW[k][1]);
    CHECK(!strcmp(season_closed_league("Australian National Soccer League"), "aus_nsl"), "the division");
    CHECK(!season_closed_active("aus_nsl", 2004) && season_closed_active("aus_nsl", 2005), "the first season");
    CHECK(season_closed_active("usa_mls", 2002), "the MLS has no first season");
    /* The clubs that join and fold in the notes of the rows. */
    CHECK(season_closed_member("aus_nsl", "New Zealand Knights", NULL, 2006) == 1, "Knights 2006");
    CHECK(season_closed_member("aus_nsl", "New Zealand Knights", NULL, 2007) == 0, "Knights 2007");
    CHECK(season_closed_member("aus_nsl", "Football Kingz", "Kingz", 2005) == 1, "the Kingz became the Knights");
    CHECK(season_closed_member("aus_nsl", "Wellington Phoenix", NULL, 2006) == 0, "Phoenix 2006");
    CHECK(season_closed_member("aus_nsl", "Wellington Phoenix", NULL, 2007) == 1, "Phoenix 2007");
    CHECK(season_closed_member("aus_nsl", "North Queensland Fury", NULL, 2010) == 1, "Fury 2010");
    CHECK(season_closed_member("aus_nsl", "North Queensland Fury", NULL, 2011) == 0, "Fury 2011");
    CHECK(season_closed_member("aus_nsl", "Gold Coast United", NULL, 2011) == 1, "Gold Coast 2011");
    CHECK(season_closed_member("aus_nsl", "Gold Coast United", NULL, 2012) == 0, "Gold Coast 2012");
    CHECK(season_closed_member("aus_nsl", "Western United FC", NULL, 2024) == 1, "Western United 2024");
    CHECK(season_closed_member("aus_nsl", "Western United FC", NULL, 2025) == 0, "Western United 2025");
    CHECK(season_closed_member("aus_nsl", "Auckland FC", NULL, 2023) == 0, "Auckland 2023");
    CHECK(season_closed_member("aus_nsl", "Auckland FC", NULL, 2024) == 1, "Auckland 2024");
    /* The names of the databases: one row for each name of a club. */
    CHECK(season_closed_row("aus_nsl", "Queensland Roar") == season_closed_row("aus_nsl", "Brisbane Roar FC"), "Roar");
    CHECK(season_closed_row("aus_nsl", "Melbourne Heart") == season_closed_row("aus_nsl", "Melbourne City FC"), "City");
    CHECK(season_closed_row("aus_nsl", "Newcastle United FC") == season_closed_row("aus_nsl", "Newcastle United Jets"),
          "Newcastle");
    CHECK(season_closed_row("aus_nsl", "Sydney Football Club") == season_closed_row("aus_nsl", "Sydney FC"), "Sydney");
    /* Clubs of the same towns that never played in the A-League. */
    CHECK(season_closed_member("aus_nsl", "Sydney United", "Sydney Utd", 2010) == -1, "Sydney United");
    CHECK(season_closed_member("aus_nsl", "Sydney Olympic Sharks", "Olympic Sharks", 2010) == -1, "Sydney Olympic");
    CHECK(season_closed_member("aus_nsl", "Melbourne Knights", NULL, 2010) == -1, "Melbourne Knights");
    CHECK(season_closed_member("aus_nsl", "Brisbane Strikers", NULL, 2010) == -1, "Brisbane Strikers");
    CHECK(season_closed_member("aus_nsl", "Adelaide City Force", "Adelaide City", 2010) == -1, "Adelaide City");
    /* The clubs of New Zealand can stand under their own nation in a database. */
    CHECK(season_closed_abroad("aus_nsl", "Wellington Phoenix", NULL), "Phoenix abroad");
    CHECK(season_closed_abroad("aus_nsl", "Auckland FC", NULL), "Auckland abroad");
    CHECK(season_closed_abroad("aus_nsl", "New Zealand Knights", NULL), "Knights abroad");
    CHECK(!season_closed_abroad("aus_nsl", "Newcastle United FC", "Newcastle"), "the English Newcastle stays out");
    CHECK(!season_closed_abroad("usa_mls", "Toronto FC", NULL), "no abroad row in the MLS");
    /* A record waits for its season in any league of the table. */
    CHECK(season_closed_waits("Sydney FC", "Sydney FC", 2004), "Sydney FC waits in 2004");
    CHECK(!season_closed_waits("Sydney FC", "Sydney FC", 2005), "Sydney FC plays in 2005");
    CHECK(season_closed_waits("San Diego FC", "San Diego FC", 2024), "San Diego FC waits in 2024");
    CHECK(!season_closed_waits("Marconi Stallions", "Marconi", 2004), "a club of no row");
}

static void test_a_league_plan(void)
{
    int a[20];
    ClosedResult r;
    /* The 13 clubs of the last NSL edition become the 8 of 2005: five new records join, ten stand-ins leave. */
    ClosedClub y2005[] = {{"Adelaide City Force", 1, 0, 0, 25, "Adelaide City"}, {"Brisbane Strikers", 1, 0, 0, 35},
                          {"Football Kingz", 1, 0, 0, 25, "Kingz"},              {"Marconi Stallions", 1, 0, 0, 30},
                          {"Melbourne Knights", 1, 0, 0, 35},                    {"Newcastle United FC", 1, 0, 0, 35},
                          {"Northern Spirit", 1, 0, 0, 30},                      {"Sydney Olympic Sharks", 1, 0, 0, 35},
                          {"Parramatta Power", 1, 0, 0, 30},                     {"Perth Glory", 1, 0, 0, 40},
                          {"South Melbourne", 1, 0, 0, 35},                      {"Sydney United", 1, 0, 0, 25},
                          {"Wollongong Wolves", 1, 0, 0, 30},                    {"Adelaide United", 0, 0, 0, 10},
                          {"Brisbane Roar", 0, 0, 0, 10},                        {"Central Coast Mariners", 0, 0, 0, 10},
                          {"Melbourne Victory", 0, 0, 0, 10},                    {"Sydney FC", 0, 0, 0, 10}};
    season_closed_plan("aus_nsl", 2005, 8, y2005, 18, a, &r);
    CHECK(r.count == 8 && r.joined == 5 && r.cut == 10 && r.folded == 0 && r.missing == 0,
          "2005: %d clubs, %d join, %d cut, %d fold, %d missing", r.count, r.joined, r.cut, r.folded, r.missing);
    CHECK(a[2] == CLOSED_STAY && a[5] == CLOSED_STAY && a[9] == CLOSED_STAY, "2005 stays %d %d %d", a[2], a[5], a[9]);
    CHECK(a[0] == CLOSED_CUT && a[12] == CLOSED_CUT && a[13] == CLOSED_JOIN, "2005 %d %d %d", a[0], a[12], a[13]);
    /* Before the first season the table moves no club. */
    season_closed_plan("aus_nsl", 2004, 13, y2005, 18, a, &r);
    CHECK(r.count == 13 && !r.joined && !r.cut && !r.folded && !r.missing, "2004: %d clubs, %d join, %d cut", r.count,
          r.joined, r.cut);
    for (int i = 0; i < 18; i++) CHECK(a[i] == CLOSED_STAY, "2004 club %d: %d", i, a[i]);
    /* 2024: a human manager keeps a club of no row beside the 12 real clubs. The class holds 13 rows, so Auckland waits. */
    ClosedClub y2024[] = {{"Adelaide United", 1, 0, 0, 50},   {"Brisbane Roar", 1, 0, 0, 50},
                          {"Central Coast Mariners", 1, 0, 0, 50}, {"Melbourne Victory", 1, 0, 0, 50},
                          {"Newcastle Jets", 1, 0, 0, 50},    {"Perth Glory", 1, 0, 0, 50},
                          {"Sydney FC", 1, 0, 0, 50},         {"Wellington Phoenix", 1, 0, 0, 50},
                          {"Melbourne City", 1, 0, 0, 50},    {"Western Sydney Wanderers", 1, 0, 0, 50},
                          {"Western United", 1, 0, 0, 20},    {"Macarthur FC", 1, 0, 0, 50},
                          {"Marconi Stallions", 1, 1, 0, 30}, {"Sydney United", 0, 0, 0, 30},
                          {"Auckland FC", 0, 0, 0, 10}};
    season_closed_plan("aus_nsl", 2024, 13, y2024, 15, a, &r);
    CHECK(r.count == 13 && r.joined == 0 && a[14] == CLOSED_STAY && a[12] == CLOSED_STAY, "2024 human: %d clubs, %d join",
          r.count, r.joined);
    /* Two human managers: the real club with the lowest reputation leaves as well. */
    y2024[13].in_league = y2024[13].human = 1;
    season_closed_plan("aus_nsl", 2024, 13, y2024, 15, a, &r);
    CHECK(r.count == 13 && a[10] == CLOSED_CUT && a[12] == CLOSED_STAY && a[13] == CLOSED_STAY, "2024 two humans: %d, %d",
          r.count, a[10]);
}

/* s073-16: the clubs that leave the league take the places of the clubs that join, in the divisions they left. Row
 * sp2026 of the release set: all 18 clubs of American A-League joined the MLS of 2026, the 18 stand-ins left for no
 * division, and the US Open Cup found 0 of its 10 A-League clubs (comp_util..cpp 241). */
static void test_places(void)
{
    ClosedClub c[] = {{"Seattle Sounders FC", 0, 0, 0, 80}, {"Austin FC", 0, 0, 0, 70},
                      {"Charlotte Independence", 1, 0, 0, 20}, {"Richmond Kickers", 1, 0, 0, 40},
                      {"Union Omaha", 1, 0, 0, 30}, {"Chicago Fire", 1, 0, 0, 90}, {"Inter Miami CF", 0, 0, 0, 60}};
    int a[] = {CLOSED_JOIN, CLOSED_JOIN, CLOSED_CUT, CLOSED_CUT, CLOSED_CUT, CLOSED_STAY, CLOSED_JOIN};
    unsigned from[] = {32, 33, 31, 31, 31, 31, 0}, to[7];
    int filled = season_closed_places(c, a, from, NULL, 7, to);
    CHECK(filled == 2, "places filled %d", filled);
    CHECK(to[3] == 32 && to[4] == 33 && to[2] == 0, "places: %u %u %u", to[3], to[4], to[2]);
    CHECK(to[0] == 0 && to[1] == 0 && to[5] == 0 && to[6] == 0, "only a club that leaves moves");
    /* More places than clubs that leave: the rest stay empty. */
    a[2] = a[3] = CLOSED_STAY;
    filled = season_closed_places(c, a, from, NULL, 7, to);
    CHECK(filled == 1 && to[4] == 32 && to[2] == 0 && to[3] == 0, "one club, two places: %d %u", filled, to[4]);
    /* A running game: no club leaves, so the club of the pool with the highest reputation takes each place (a 2005
     * start on the October 2026 database emptied the A-League one club a season). */
    int none[] = {CLOSED_JOIN, CLOSED_JOIN, CLOSED_STAY, CLOSED_STAY, CLOSED_STAY, CLOSED_STAY, CLOSED_STAY};
    int pool[] = {0, 0, 1, 1, 0, 0, 0};
    filled = season_closed_places(c, none, from, pool, 7, to);
    CHECK(filled == 2 && to[3] == 32 && to[2] == 33 && to[4] == 0 && to[6] == 0, "pool: %d %u %u", filled, to[3], to[2]);
    /* A club that leaves comes before the pool. */
    none[4] = CLOSED_CUT;
    filled = season_closed_places(c, none, from, pool, 7, to);
    CHECK(filled == 2 && to[4] == 32 && to[3] == 33 && to[2] == 0, "cut first: %u %u", to[4], to[3]);
}

int main(void)
{
    test_places();
    test_table();
    test_plan();
    test_a_league();
    test_a_league_plan();
    printf(fails ? "season_closed: %d failures\n" : "season_closed: ok\n", fails);
    return fails != 0;
}
