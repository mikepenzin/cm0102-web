/* The UEFA Cup 2007/08 group stage as played (season_uefa_days.c): the Thursday of each matchday. Sources: English
 * Wikipedia (2007-08 UEFA Cup group stage) and rsssf.org (https://www.rsssf.org/ec/ec200708.html), read 26 October
 * 2026 (docs/research/2026-10-26-uefa-group-dates.tsv). */
static const SeasonLeagueDay UC_2007[5] = {{25, 9, 0}, {8, 10, 0}, {29, 10, 0}, {6, 11, 0}, {20, 11, 0}};
/* The Europa League group stages as played, 2009/10 to 2023/24 (season_uefa_days.c): the Thursday of each matchday;
 * the Conference League groups of 2021/22 to 2023/24 played on the same Thursdays. Two sources for each season: English
 * Wikipedia ("20XX-YY UEFA Europa League group stage", "... Europa Conference League group stage") and rsssf.org
 * (https://www.rsssf.org/ec/ecYYYYYY.html), read 26 October 2026; the rows with their sources:
 * docs/research/2026-10-26-uefa-group-dates-el.tsv. */
typedef struct {
    short season;
    SeasonLeagueDay thu[6];
} ElRealDays;
static const ElRealDays EL_REAL[] = {
    {2009, {{17, 8, 0}, {1, 9, 0}, {22, 9, 0}, {5, 10, 0}, {3, 11, 0}, {17, 11, 0}}},
    {2010, {{16, 8, 0}, {30, 8, 0}, {21, 9, 0}, {4, 10, 0}, {2, 11, 0}, {16, 11, 0}}},
    {2011, {{15, 8, 0}, {29, 8, 0}, {20, 9, 0}, {3, 10, 0}, {1, 11, 0}, {15, 11, 0}}},
    {2012, {{20, 8, 0}, {4, 9, 0}, {25, 9, 0}, {8, 10, 0}, {22, 10, 0}, {6, 11, 0}}},
    {2013, {{19, 8, 0}, {3, 9, 0}, {24, 9, 0}, {7, 10, 0}, {28, 10, 0}, {12, 11, 0}}},
    {2014, {{18, 8, 0}, {2, 9, 0}, {23, 9, 0}, {6, 10, 0}, {27, 10, 0}, {11, 11, 0}}},
    {2015, {{17, 8, 0}, {1, 9, 0}, {22, 9, 0}, {5, 10, 0}, {26, 10, 0}, {10, 11, 0}}},
    {2016, {{15, 8, 0}, {29, 8, 0}, {20, 9, 0}, {3, 10, 0}, {24, 10, 0}, {8, 11, 0}}},
    {2017, {{14, 8, 0}, {28, 8, 0}, {19, 9, 0}, {2, 10, 0}, {23, 10, 0}, {7, 11, 0}}},
    {2018, {{20, 8, 0}, {4, 9, 0}, {25, 9, 0}, {8, 10, 0}, {29, 10, 0}, {13, 11, 0}}},
    {2019, {{19, 8, 0}, {3, 9, 0}, {24, 9, 0}, {7, 10, 0}, {28, 10, 0}, {12, 11, 0}}},
    {2020, {{22, 9, 0}, {29, 9, 0}, {5, 10, 0}, {26, 10, 0}, {3, 11, 0}, {10, 11, 0}}},
    {2021, {{16, 8, 0}, {30, 8, 0}, {21, 9, 0}, {4, 10, 0}, {25, 10, 0}, {9, 11, 0}}},
    {2022, {{8, 8, 0}, {15, 8, 0}, {6, 9, 0}, {13, 9, 0}, {27, 9, 0}, {3, 10, 0}}},
    {2023, {{21, 8, 0}, {5, 9, 0}, {26, 9, 0}, {9, 10, 0}, {30, 10, 0}, {14, 11, 0}}},
};
