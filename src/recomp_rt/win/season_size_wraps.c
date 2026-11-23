/* The set_up_rules functions of the league classes of tools/recomp/league_sizes.py, wrapped for the
 * promotion and relegation places of each season (season_sizes.h). Made by tools/seasons/make_size_wraps.py:
 * do not edit by hand. */
#include "win.h"
#include "season_sizes.h"
#include "season_template.h"

/* Each wrap: the original (thiscall, no arguments), then season_sizes_rules(this). The recompiler reads
 * the RT_WRAP lines as they are (tools/recomp/recomp.py), so they are written out in full. */

/* ArgPrm::set_up_rules */
void o_00405670(void);
RT_WRAP(00405670, rules_405670)
{
    uint32_t self = cpu.ecx;
    o_00405670();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ArgSecond::set_up_rules */
void o_00407e50(void);
RT_WRAP(00407e50, rules_407e50)
{
    uint32_t self = cpu.ecx;
    o_00407e50();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* AusNsl::set_up_rules */
void o_00411ce0(void);
RT_WRAP(00411ce0, rules_411ce0)
{
    uint32_t self = cpu.ecx;
    o_00411ce0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* BelFirst::set_up_rules */
void o_0041e9e0(void);
RT_WRAP(0041e9e0, rules_41e9e0)
{
    uint32_t self = cpu.ecx;
    o_0041e9e0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* BelSecond::set_up_rules */
void o_0041fea0(void);
RT_WRAP(0041fea0, rules_41fea0)
{
    uint32_t self = cpu.ecx;
    o_0041fea0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* BraNatFirst::set_up_rules */
void o_0042a5a0(void);
RT_WRAP(0042a5a0, rules_42a5a0)
{
    uint32_t self = cpu.ecx;
    o_0042a5a0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* BraNatSecond::set_up_rules */
void o_0042cb70(void);
RT_WRAP(0042cb70, rules_42cb70)
{
    uint32_t self = cpu.ecx;
    o_0042cb70();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* CroA1::set_up_rules */
void o_005148e0(void);
RT_WRAP(005148e0, rules_5148e0)
{
    uint32_t self = cpu.ecx;
    o_005148e0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* CroA2a::set_up_rules */
void o_00515de0(void);
RT_WRAP(00515de0, rules_515de0)
{
    uint32_t self = cpu.ecx;
    o_00515de0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* CroA2b::set_up_rules */
void o_00516f00(void);
RT_WRAP(00516f00, rules_516f00)
{
    uint32_t self = cpu.ecx;
    o_00516f00();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* DenFirst::set_up_rules */
void o_005523b0(void);
RT_WRAP(005523b0, rules_5523b0)
{
    uint32_t self = cpu.ecx;
    o_005523b0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* DenPrm::set_up_rules */
void o_00553a00(void);
RT_WRAP(00553a00, rules_553a00)
{
    uint32_t self = cpu.ecx;
    o_00553a00();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* EngConf::set_up_rules */
void o_0056ede0(void);
RT_WRAP(0056ede0, rules_56ede0)
{
    uint32_t self = cpu.ecx;
    o_0056ede0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* EngFirst::set_up_rules */
void o_00572d90(void);
RT_WRAP(00572d90, rules_572d90)
{
    uint32_t self = cpu.ecx;
    o_00572d90();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* EngPrm::set_up_rules */
void o_00574b70(void);
RT_WRAP(00574b70, rules_574b70)
{
    uint32_t self = cpu.ecx;
    o_00574b70();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* EngSecond::set_up_rules */
void o_00576780(void);
RT_WRAP(00576780, rules_576780)
{
    uint32_t self = cpu.ecx;
    o_00576780();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* EngThird::set_up_rules */
void o_005780c0(void);
RT_WRAP(005780c0, rules_5780c0)
{
    uint32_t self = cpu.ecx;
    o_005780c0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* FinPrm::set_up_rules */
void o_00595c90(void);
RT_WRAP(00595c90, rules_595c90)
{
    uint32_t self = cpu.ecx;
    o_00595c90();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* FraFirst::set_up_rules */
void o_005bc410(void);
RT_WRAP(005bc410, rules_5bc410)
{
    uint32_t self = cpu.ecx;
    o_005bc410();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* FraSecond::set_up_rules */
void o_005bf0a0(void);
RT_WRAP(005bf0a0, rules_5bf0a0)
{
    uint32_t self = cpu.ecx;
    o_005bf0a0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* FraThird::set_up_rules */
void o_005c0700(void);
RT_WRAP(005c0700, rules_5c0700)
{
    uint32_t self = cpu.ecx;
    o_005c0700();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* GerFirst::set_up_rules */
void o_005db400(void);
RT_WRAP(005db400, rules_5db400)
{
    uint32_t self = cpu.ecx;
    o_005db400();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* GerSecond::set_up_rules */
void o_005df3e0(void);
RT_WRAP(005df3e0, rules_5df3e0)
{
    uint32_t self = cpu.ecx;
    o_005df3e0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* GrePrm::set_up_rules */
void o_005eae90(void);
RT_WRAP(005eae90, rules_5eae90)
{
    uint32_t self = cpu.ecx;
    o_005eae90();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* GreSecond::set_up_rules */
void o_005ebde0(void);
RT_WRAP(005ebde0, rules_5ebde0)
{
    uint32_t self = cpu.ecx;
    o_005ebde0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* HolFirst::set_up_rules */
void o_005f6340(void);
RT_WRAP(005f6340, rules_5f6340)
{
    uint32_t self = cpu.ecx;
    o_005f6340();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* HolPrm::set_up_rules */
void o_005f8560(void);
RT_WRAP(005f8560, rules_5f8560)
{
    uint32_t self = cpu.ecx;
    o_005f8560();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* IreFirst::set_up_rules */
void o_00635aa0(void);
RT_WRAP(00635aa0, rules_635aa0)
{
    uint32_t self = cpu.ecx;
    o_00635aa0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* IrePrm::set_up_rules */
void o_0063a900(void);
RT_WRAP(0063a900, rules_63a900)
{
    uint32_t self = cpu.ecx;
    o_0063a900();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ItaSerA::set_up_rules */
void o_00641640(void);
RT_WRAP(00641640, rules_641640)
{
    uint32_t self = cpu.ecx;
    o_00641640();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ItaSerB::set_up_rules */
void o_006468a0(void);
RT_WRAP(006468a0, rules_6468a0)
{
    uint32_t self = cpu.ecx;
    o_006468a0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ItaSerC1a::set_up_rules */
void o_0064b770(void);
RT_WRAP(0064b770, rules_64b770)
{
    uint32_t self = cpu.ecx;
    o_0064b770();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ItaSerC1b::set_up_rules */
void o_0064f920(void);
RT_WRAP(0064f920, rules_64f920)
{
    uint32_t self = cpu.ecx;
    o_0064f920();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* JapJ1::set_up_rules */
void o_00663620(void);
RT_WRAP(00663620, rules_663620)
{
    uint32_t self = cpu.ecx;
    o_00663620();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* JapJ2::set_up_rules */
void o_00665140(void);
RT_WRAP(00665140, rules_665140)
{
    uint32_t self = cpu.ecx;
    o_00665140();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* KorLeague::set_up_rules */
void o_0066bcc0(void);
RT_WRAP(0066bcc0, rules_66bcc0)
{
    uint32_t self = cpu.ecx;
    o_0066bcc0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* NirFirst::set_up_rules */
void o_0078bd70(void);
RT_WRAP(0078bd70, rules_78bd70)
{
    uint32_t self = cpu.ecx;
    o_0078bd70();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* NirPrm::set_up_rules */
void o_0078ed80(void);
RT_WRAP(0078ed80, rules_78ed80)
{
    uint32_t self = cpu.ecx;
    o_0078ed80();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* NorFirst::set_up_rules */
void o_00790b00(void);
RT_WRAP(00790b00, rules_790b00)
{
    uint32_t self = cpu.ecx;
    o_00790b00();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* NorPrm::set_up_rules */
void o_00792700(void);
RT_WRAP(00792700, rules_792700)
{
    uint32_t self = cpu.ecx;
    o_00792700();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* PolFirst::set_up_rules */
void o_007c9690(void);
RT_WRAP(007c9690, rules_7c9690)
{
    uint32_t self = cpu.ecx;
    o_007c9690();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* PolSecond::set_up_rules */
void o_007cbf40(void);
RT_WRAP(007cbf40, rules_7cbf40)
{
    uint32_t self = cpu.ecx;
    o_007cbf40();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* PorPrm::set_up_rules */
void o_007cf930(void);
RT_WRAP(007cf930, rules_7cf930)
{
    uint32_t self = cpu.ecx;
    o_007cf930();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* PorSecond::set_up_rules */
void o_007d16a0(void);
RT_WRAP(007d16a0, rules_7d16a0)
{
    uint32_t self = cpu.ecx;
    o_007d16a0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* RusFirst::set_up_rules */
void o_007ea810(void);
RT_WRAP(007ea810, rules_7ea810)
{
    uint32_t self = cpu.ecx;
    o_007ea810();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* RusPrm::set_up_rules */
void o_007eb740(void);
RT_WRAP(007eb740, rules_7eb740)
{
    uint32_t self = cpu.ecx;
    o_007eb740();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ScoFirst::set_up_rules */
void o_007eee50(void);
RT_WRAP(007eee50, rules_7eee50)
{
    uint32_t self = cpu.ecx;
    o_007eee50();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ScoPrm::set_up_rules */
void o_007f2970(void);
RT_WRAP(007f2970, rules_7f2970)
{
    uint32_t self = cpu.ecx;
    o_007f2970();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ScoSecond::set_up_rules */
void o_007f3f80(void);
RT_WRAP(007f3f80, rules_7f3f80)
{
    uint32_t self = cpu.ecx;
    o_007f3f80();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* ScoThird::set_up_rules */
void o_007f4f20(void);
RT_WRAP(007f4f20, rules_7f4f20)
{
    uint32_t self = cpu.ecx;
    o_007f4f20();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* SpaFirst::set_up_rules */
void o_0084fdf0(void);
RT_WRAP(0084fdf0, rules_84fdf0)
{
    uint32_t self = cpu.ecx;
    o_0084fdf0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* SpaSecond::set_up_rules */
void o_00855cc0(void);
RT_WRAP(00855cc0, rules_855cc0)
{
    uint32_t self = cpu.ecx;
    o_00855cc0();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* SweFirst::set_up_rules */
void o_0088f090(void);
RT_WRAP(0088f090, rules_88f090)
{
    uint32_t self = cpu.ecx;
    o_0088f090();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* SwePrm::set_up_rules */
void o_00890550(void);
RT_WRAP(00890550, rules_890550)
{
    uint32_t self = cpu.ecx;
    o_00890550();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* TurFirst::set_up_rules */
void o_008fdf90(void);
RT_WRAP(008fdf90, rules_8fdf90)
{
    uint32_t self = cpu.ecx;
    o_008fdf90();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* TurSecond::set_up_rules */
void o_008ff280(void);
RT_WRAP(008ff280, rules_8ff280)
{
    uint32_t self = cpu.ecx;
    o_008ff280();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* WelFirst::set_up_rules */
void o_00925c50(void);
RT_WRAP(00925c50, rules_925c50)
{
    uint32_t self = cpu.ecx;
    o_00925c50();
    if (rt_unwinding) return;
    season_sizes_rules(self);
}

/* The source files of these classes (season_sizes.h). */
const char *const season_size_files[] = {
    "arg_prm",
    "arg_second",
    "aus_nsl",
    "bel_first",
    "bel_second",
    "bra_nat_first",
    "bra_nat_second",
    "cro_a1",
    "cro_a2a",
    "cro_a2b",
    "den_first",
    "den_prm",
    "eng_conf",
    "eng_first",
    "eng_prm",
    "eng_second",
    "eng_third",
    "fin_prm",
    "fra_first",
    "fra_second",
    "fra_third",
    "ger_first",
    "ger_second",
    "gre_prm",
    "gre_second",
    "hol_first",
    "hol_prm",
    "ire_first",
    "ire_prm",
    "ita_ser_a",
    "ita_ser_b",
    "ita_ser_c1a",
    "ita_ser_c1b",
    "jap_j1",
    "jap_j2",
    "kor_league",
    "nir_first",
    "nir_prm",
    "nor_first",
    "nor_prm",
    "pol_first",
    "pol_second",
    "por_prm",
    "por_second",
    "rus_first",
    "rus_prm",
    "sco_first",
    "sco_prm",
    "sco_second",
    "sco_third",
    "spa_first",
    "spa_second",
    "swe_first",
    "swe_prm",
    "tur_first",
    "tur_second",
    "wel_first",
};
const int season_size_file_count = 57;
/* The largest count of each file (0: the class counts the clubs of the data). */
const int season_size_caps[] = {
    30,       /* arg_prm */
    38,       /* arg_second */
    13,       /* aus_nsl */
    18,       /* bel_first */
    19,       /* bel_second */
    28,       /* bra_nat_first */
    28,       /* bra_nat_second */
    16,       /* cro_a1 */
    18,       /* cro_a2a */
    16,       /* cro_a2b */
    16,       /* den_first */
    14,       /* den_prm */
    24,       /* eng_conf */
    24,       /* eng_first */
    20,       /* eng_prm */
    24,       /* eng_second */
    24,       /* eng_third */
    14,       /* fin_prm */
    20,       /* fra_first */
    20,       /* fra_second */
    21,       /* fra_third */
    18,       /* ger_first */
    18,       /* ger_second */
    18,       /* gre_prm */
    21,       /* gre_second */
    20,       /* hol_first */
    18,       /* hol_prm */
    0,       /* ire_first */
    0,       /* ire_prm */
    20,       /* ita_ser_a */
    24,       /* ita_ser_b */
    20,       /* ita_ser_c1a */
    20,       /* ita_ser_c1b */
    20,       /* jap_j1 */
    22,       /* jap_j2 */
    16,       /* kor_league */
    17,       /* nir_first */
    16,       /* nir_prm */
    16,       /* nor_first */
    16,       /* nor_prm */
    0,       /* pol_first */
    20,       /* pol_second */
    18,       /* por_prm */
    24,       /* por_second */
    22,       /* rus_first */
    16,       /* rus_prm */
    10,       /* sco_first */
    12,       /* sco_prm */
    10,       /* sco_second */
    10,       /* sco_third */
    20,       /* spa_first */
    22,       /* spa_second */
    16,       /* swe_first */
    16,       /* swe_prm */
    21,       /* tur_first */
    20,       /* tur_second */
    18,       /* wel_first */
};
/* The smallest count of each file: the largest where a cup of the nation needs it (league_sizes.py FLOORS). */
const int season_size_floors[] = {
    0,       /* arg_prm */
    0,       /* arg_second */
    0,       /* aus_nsl */
    0,       /* bel_first */
    0,       /* bel_second */
    0,       /* bra_nat_first */
    0,       /* bra_nat_second */
    0,       /* cro_a1 */
    0,       /* cro_a2a */
    0,       /* cro_a2b */
    0,       /* den_first */
    0,       /* den_prm */
    0,       /* eng_conf */
    0,       /* eng_first */
    0,       /* eng_prm */
    0,       /* eng_second */
    0,       /* eng_third */
    0,       /* fin_prm */
    0,       /* fra_first */
    0,       /* fra_second */
    0,       /* fra_third */
    0,       /* ger_first */
    0,       /* ger_second */
    0,       /* gre_prm */
    0,       /* gre_second */
    0,       /* hol_first */
    0,       /* hol_prm */
    0,       /* ire_first */
    0,       /* ire_prm */
    0,       /* ita_ser_a */
    0,       /* ita_ser_b */
    0,       /* ita_ser_c1a */
    0,       /* ita_ser_c1b */
    0,       /* jap_j1 */
    0,       /* jap_j2 */
    0,       /* kor_league */
    0,       /* nir_first */
    0,       /* nir_prm */
    0,       /* nor_first */
    0,       /* nor_prm */
    0,       /* pol_first */
    0,       /* pol_second */
    0,       /* por_prm */
    0,       /* por_second */
    0,       /* rus_first */
    0,       /* rus_prm */
    0,       /* sco_first */
    0,       /* sco_prm */
    0,       /* sco_second */
    0,       /* sco_third */
    0,       /* spa_first */
    0,       /* spa_second */
    0,       /* swe_first */
    0,       /* swe_prm */
    0,       /* tur_first */
    0,       /* tur_second */
    0,       /* wel_first */
};
/* The count whose places and exchange stay the class's (league_sizes.py FIXED_EXCHANGE). */
const int season_size_fixed[] = {
    0,       /* arg_prm */
    0,       /* arg_second */
    0,       /* aus_nsl */
    0,       /* bel_first */
    0,       /* bel_second */
    0,       /* bra_nat_first */
    0,       /* bra_nat_second */
    0,       /* cro_a1 */
    0,       /* cro_a2a */
    0,       /* cro_a2b */
    0,       /* den_first */
    0,       /* den_prm */
    0,       /* eng_conf */
    0,       /* eng_first */
    0,       /* eng_prm */
    0,       /* eng_second */
    0,       /* eng_third */
    0,       /* fin_prm */
    0,       /* fra_first */
    0,       /* fra_second */
    0,       /* fra_third */
    0,       /* ger_first */
    0,       /* ger_second */
    0,       /* gre_prm */
    0,       /* gre_second */
    0,       /* hol_first */
    0,       /* hol_prm */
    0,       /* ire_first */
    0,       /* ire_prm */
    0,       /* ita_ser_a */
    0,       /* ita_ser_b */
    0,       /* ita_ser_c1a */
    0,       /* ita_ser_c1b */
    0,       /* jap_j1 */
    0,       /* jap_j2 */
    0,       /* kor_league */
    0,       /* nir_first */
    0,       /* nir_prm */
    0,       /* nor_first */
    0,       /* nor_prm */
    0,       /* pol_first */
    0,       /* pol_second */
    0,       /* por_prm */
    0,       /* por_second */
    0,       /* rus_first */
    0,       /* rus_prm */
    0,       /* sco_first */
    0,       /* sco_prm */
    0,       /* sco_second */
    0,       /* sco_third */
    0,       /* spa_first */
    0,       /* spa_second */
    0,       /* swe_first */
    0,       /* swe_prm */
    0,       /* tur_first */
    0,       /* tur_second */
    0,       /* wel_first */
};

/* The grown classes (league_sizes.py GROWN): file, the rows that create_table allocates, the cap, the
 * mode of the date template. */
const SeasonGrown season_grown[] = {
    {"arg_prm", 20, 30, SEASON_TEMPLATE_HALVES},
    {"arg_second", 25, 38, SEASON_TEMPLATE_SHUFFLED},
    {"aus_nsl", 13, 13, SEASON_TEMPLATE_SHUFFLED},
    {"bel_first", 18, 18, SEASON_TEMPLATE_SEQUENTIAL},
    {"bel_second", 18, 19, SEASON_TEMPLATE_SEQUENTIAL},
    {"bra_nat_first", 28, 28, SEASON_TEMPLATE_SHUFFLED},
    {"bra_nat_second", 28, 28, SEASON_TEMPLATE_SHUFFLED},
    {"cro_a1", 16, 16, SEASON_TEMPLATE_SEQUENTIAL},
    {"cro_a2a", 16, 18, SEASON_TEMPLATE_SEQUENTIAL},
    {"cro_a2b", 16, 16, SEASON_TEMPLATE_SEQUENTIAL},
    {"den_prm", 12, 14, SEASON_TEMPLATE_SHUFFLED},
    {"eng_conf", 22, 24, SEASON_TEMPLATE_SHUFFLED},
    {"fin_prm", 12, 14, SEASON_TEMPLATE_SHUFFLED},
    {"fra_third", 20, 21, SEASON_TEMPLATE_MIRRORED},
    {"gre_prm", 14, 18, SEASON_TEMPLATE_SEQUENTIAL},
    {"gre_second", 14, 21, SEASON_TEMPLATE_SEQUENTIAL},
    {"hol_first", 18, 20, SEASON_TEMPLATE_SHUFFLED},
    {"ire_first", 0, 12, SEASON_TEMPLATE_SHUFFLED},
    {"ire_prm", 0, 12, SEASON_TEMPLATE_SHUFFLED},
    {"ita_ser_a", 18, 20, SEASON_TEMPLATE_SEQUENTIAL},
    {"ita_ser_b", 20, 24, SEASON_TEMPLATE_SEQUENTIAL},
    {"ita_ser_c1a", 18, 20, SEASON_TEMPLATE_SEQUENTIAL},
    {"ita_ser_c1b", 18, 20, SEASON_TEMPLATE_SEQUENTIAL},
    {"jap_j1", 16, 20, SEASON_TEMPLATE_SHUFFLED},
    {"jap_j2", 12, 22, SEASON_TEMPLATE_SHUFFLED},
    {"kor_league", 10, 16, SEASON_TEMPLATE_SHUFFLED},
    {"nir_first", 10, 17, SEASON_TEMPLATE_SHUFFLED},
    {"nir_prm", 10, 16, SEASON_TEMPLATE_SHUFFLED},
    {"nor_prm", 14, 16, SEASON_TEMPLATE_SHUFFLED},
    {"por_second", 18, 24, SEASON_TEMPLATE_SHUFFLED},
    {"rus_first", 18, 22, SEASON_TEMPLATE_SHUFFLED},
    {"swe_prm", 14, 16, SEASON_TEMPLATE_SHUFFLED},
    {"tur_first", 21, 21, SEASON_TEMPLATE_SHUFFLED},
    {"tur_second", 20, 20, SEASON_TEMPLATE_SHUFFLED},
    {"wel_first", 18, 18, SEASON_TEMPLATE_SHUFFLED},
};
const int season_grown_count = 35;

/* Each wrap: the original, then season_sizes_schedule(this, stage, &count). */

/* ArgPrm::create_schedule */
void o_00404570(void);
RT_WRAP(00404570, schedule_404570)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00404570();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* ArgSecond::create_schedule */
void o_00406f00(void);
RT_WRAP(00406f00, schedule_406f00)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00406f00();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* AusNsl::create_schedule */
void o_004111a0(void);
RT_WRAP(004111a0, schedule_4111a0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_004111a0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* BelFirst::create_schedule */
void o_0041e010(void);
RT_WRAP(0041e010, schedule_41e010)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_0041e010();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* BelSecond::create_schedule 0x41f340: wrapped in season_bel_playoffs.c bel_second_schedule */

/* BraNatFirst::create_schedule */
void o_00429b50(void);
RT_WRAP(00429b50, schedule_429b50)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00429b50();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* BraNatSecond::create_schedule */
void o_0042c140(void);
RT_WRAP(0042c140, schedule_42c140)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_0042c140();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* CroA1::create_schedule */
void o_005140e0(void);
RT_WRAP(005140e0, schedule_5140e0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_005140e0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* CroA2a::create_schedule */
void o_005155e0(void);
RT_WRAP(005155e0, schedule_5155e0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_005155e0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* CroA2b::create_schedule */
void o_00516700(void);
RT_WRAP(00516700, schedule_516700)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00516700();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* DenPrm::create_schedule */
void o_00552900(void);
RT_WRAP(00552900, schedule_552900)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00552900();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* EngConf::create_schedule */
void o_0056de60(void);
RT_WRAP(0056de60, schedule_56de60)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_0056de60();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* FinPrm::create_schedule */
void o_005954b0(void);
RT_WRAP(005954b0, schedule_5954b0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_005954b0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* FraThird::create_schedule */
void o_005bfe30(void);
RT_WRAP(005bfe30, schedule_5bfe30)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_005bfe30();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* GrePrm::create_schedule */
void o_005ea6d0(void);
RT_WRAP(005ea6d0, schedule_5ea6d0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_005ea6d0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* GreSecond::create_schedule */
void o_005eb570(void);
RT_WRAP(005eb570, schedule_5eb570)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_005eb570();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* HolFirst::create_schedule */
void o_005f5790(void);
RT_WRAP(005f5790, schedule_5f5790)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_005f5790();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* IreFirst::create_schedule */
void o_00634f40(void);
RT_WRAP(00634f40, schedule_634f40)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00634f40();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* IrePrm::create_schedule */
void o_00639e90(void);
RT_WRAP(00639e90, schedule_639e90)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00639e90();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* ItaSerA::create_schedule */
void o_006409b0(void);
RT_WRAP(006409b0, schedule_6409b0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_006409b0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* ItaSerB::create_schedule */
void o_00645af0(void);
RT_WRAP(00645af0, schedule_645af0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00645af0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* ItaSerC1a::create_schedule 0x64ad80: wrapped in season_divisions_ita.c c1a_schedule */

/* ItaSerC1b::create_schedule 0x64ef30: wrapped in season_divisions_ita.c c1b_schedule */

/* JapJ1::create_schedule */
void o_00662e30(void);
RT_WRAP(00662e30, schedule_662e30)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00662e30();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* JapJ2::create_schedule */
void o_00664780(void);
RT_WRAP(00664780, schedule_664780)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_00664780();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* KorLeague::create_schedule */
void o_0066b130(void);
RT_WRAP(0066b130, schedule_66b130)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_0066b130();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* NirFirst::create_schedule */
void o_0078b310(void);
RT_WRAP(0078b310, schedule_78b310)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_0078b310();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* NirPrm::create_schedule */
void o_0078e330(void);
RT_WRAP(0078e330, schedule_78e330)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_0078e330();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* NorPrm::create_schedule */
void o_007917c0(void);
RT_WRAP(007917c0, schedule_7917c0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_007917c0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* PorSecond::create_schedule */
void o_007d0c40(void);
RT_WRAP(007d0c40, schedule_7d0c40)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_007d0c40();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* RusFirst::create_schedule */
void o_007ea220(void);
RT_WRAP(007ea220, schedule_7ea220)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_007ea220();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* SwePrm::create_schedule */
void o_0088feb0(void);
RT_WRAP(0088feb0, schedule_88feb0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_0088feb0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* TurFirst::create_schedule */
void o_008fd220(void);
RT_WRAP(008fd220, schedule_8fd220)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_008fd220();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* TurSecond::create_schedule */
void o_008fe9b0(void);
RT_WRAP(008fe9b0, schedule_8fe9b0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_008fe9b0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}

/* WelFirst::create_schedule */
void o_009250f0(void);
RT_WRAP(009250f0, schedule_9250f0)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_009250f0();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
}
