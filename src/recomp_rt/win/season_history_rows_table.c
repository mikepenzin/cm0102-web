/* Made by tools/seasons/make_history_rows_table.py from docs/research/history-rows/. Do not edit. */
#include "season_history.h"

const SeasonHistListRow season_history_list_rows[] = {
    {"Greek Super Cup", 1980, 2001, {"Olympiacos Piraeus|Olympiakos Piraeus", "AGS Kastorias|AGS Kastoria|Kastoria", "", ""}},
    {"Greek Super Cup", 1987, 2001, {"Olympiacos Piraeus|Olympiakos Piraeus", "OFI Crete|OFI Crete FC|Omilos Filathlon Irakleiou", "", ""}},
    {"Greek Super Cup", 1988, 2001, {"Panathinaikos AO|Panathinaikos Athens|Panathinaikos FC|Panathinaikos", "A.E Larisas|AE Larisa 1964|AE Larissa 1964|AE Larisa|Larisa", "", ""}},
    {"Greek Super Cup", 1989, 2001, {"AEK Athens", "Panathinaikos AO|Panathinaikos Athens|Panathinaikos FC|Panathinaikos", "", ""}},
    {"Greek Super Cup", 1992, 2001, {"Olympiacos Piraeus|Olympiakos Piraeus", "AEK Athens", "", ""}},
    {"Greek Super Cup", 1993, 2001, {"Panathinaikos AO|Panathinaikos Athens|Panathinaikos FC|Panathinaikos", "AEK Athens", "", ""}},
    {"Greek Super Cup", 1994, 2001, {"Panathinaikos AO|Panathinaikos Athens|Panathinaikos FC|Panathinaikos", "AEK Athens", "", ""}},
    {"Greek Super Cup", 1996, 2001, {"AEK Athens", "Panathinaikos AO|Panathinaikos Athens|Panathinaikos FC|Panathinaikos", "", ""}},
    {"Greek Super Cup", 2007, 2008, {"Olympiacos Piraeus|Olympiakos Piraeus", "A.E Larisas|AE Larisa 1964|AE Larissa 1964|AE Larisa|Larisa", "", ""}},
    {"Greek Super Cup", 2025, 2026, {"Olympiacos Piraeus|Olympiakos Piraeus", "OFI Crete|OFI Crete FC|Omilos Filathlon Irakleiou", "", ""}},
    {"Greek Super Cup", 2026, 2027, {"OFI Crete|OFI Crete FC|Omilos Filathlon Irakleiou", "AEK Athens", "", ""}},
    {"UEFA Europa Conference League", 2022, 2022, {"AS Roma|A.S. Roma", "Feyenoord Rotterdam|Feyenoord|SC Feyenoord Rotterdam|SC Feyenoord|Sportclub Feyenoord", "", ""}},
    {"UEFA Europa Conference League", 2023, 2023, {"West Ham United", "ACF Fiorentina|Fiorentina|Fiorentina FC", "", ""}},
    {"UEFA Europa Conference League", 2024, 2024, {"Olympiacos Piraeus|Olympiakos Piraeus", "ACF Fiorentina|Fiorentina|Fiorentina FC", "", ""}},
    {"UEFA Europa Conference League", 2025, 2025, {"Chelsea", "Real Betis Balompi\351", "", ""}},
    {"UEFA Europa Conference League", 2026, 2026, {"Crystal Palace", "Rayo Vallecano de Madrid", "", ""}},
    {"Asian Cup Qualifying", 2019, 2019, {"Portugal", "Holland", "England", "Portugal"}},
    {"Asian Cup Qualifying", 2021, 2021, {"France", "Spain", "Italy", "Italy"}},
    {"Asian Cup Qualifying", 2023, 2023, {"Spain", "Croatia", "Italy", "Holland"}},
    {"Asian Cup Qualifying", 2025, 2025, {"Portugal", "Spain", "France", "Germany"}},
};
const int season_history_list_rows_count = (int)(sizeof season_history_list_rows / sizeof season_history_list_rows[0]);

const SeasonHistRowFix season_history_row_fixes[] = {
    {"European Football Championship", 2000, 3, "France", "Belgium|Holland", "Belgium"},
    {"European Football Championship", 2004, 3, "Greece", "Portugal", "Portugal"},
    {"European Football Championship", 2008, 3, "Spain", "Austria|Switzerland", "Austria"},
    {"European Football Championship", 2012, 3, "Spain", "Poland|Ukraine", "Ukraine"},
    {"European Football Championship", 2016, 3, "Portugal", "France", "France"},
    {"European Football Championship", 2020, 3, "Italy", "England", "England"},
    {"European Football Championship", 2024, 3, "Spain", "Germany", "Germany"},
    {"FIFA World Cup", 2002, 3, "Brazil", "Japan|South Korea", "Japan"},
    {"FIFA World Cup", 2006, 3, "Italy", "Germany", "Germany"},
    {"FIFA World Cup", 2010, 3, "Spain", "South Africa", "South Africa"},
    {"FIFA World Cup", 2014, 3, "Germany", "Brazil", "Brazil"},
    {"FIFA World Cup", 2018, 3, "France", "Russia", "Russia"},
    {"FIFA World Cup", 2022, 3, "Argentina", "Qatar", "Qatar"},
    {"FIFA World Cup", 2026, 3, "Spain", "United States", "United States"},
    {"European Football Championship", 2000, 2, "France", "", ""},
    {"European Football Championship", 2004, 2, "Greece", "", ""},
    {"European Football Championship", 2008, 2, "Spain", "", ""},
    {"European Football Championship", 2024, 2, "Spain", "", ""},
    {"South American Copa Libertadores", 2022, 1, "Clube de Regatas do Flamengo", "Clube Atl\351tico Paranaense", "Clube Atl\351tico Paranaense"},
    {"European Football Championship", 1984, 2, "France", "", ""},
    {"European Football Championship", 1988, 2, "Holland", "", ""},
    {"European Football Championship", 1992, 2, "Denmark", "", ""},
    {"European Football Championship", 1996, 2, "Germany", "", ""},
    {"African Cup of Nations", 2012, 3, "Zambia", "Equatorial Guinea|Gabon", "Equatorial Guinea"},
    {"Oceania Nations Cup", 2012, 3, "Tahiti", "Solomon Islands", "Solomon Islands"},
    {"Oceania Nations Cup", 2012, 2, "Tahiti", "New Zealand", "New Zealand"},
};
const int season_history_row_fixes_count = (int)(sizeof season_history_row_fixes / sizeof season_history_row_fixes[0]);

const SeasonHistClearFrom season_history_clears_from[] = {
    {"European Cup Winners Cup", 2000},
};
const int season_history_clears_from_count = (int)(sizeof season_history_clears_from / sizeof season_history_clears_from[0]);
