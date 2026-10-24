/* A club releases a staff member who has a loan contract (O358, box contract_manager..cpp 5941).
 *
 * Each staff has a row of 79 bytes at [0xae2d0c] + id * 79: +0 the index of his main contract, +4 the index of his
 * loan contract, -1 for none. A contract is 80 bytes in the table at [CONTRACT_MANAGER] ([0xae1050]): +0x4 the club,
 * +0x2d and +0x2f the end day and year, +0x35 the type.
 *
 * The cause. Shortlist_Manager::assess_club_squad 0x83cb40 lists the club's staff slots (club +0xd3, then the slots
 * from +0x19f, 0x83ec1f..0x83ec64) into [esp+0xe4]. For a staff slot of the club it takes the first of them who is
 * a coach or a player/coach (+0x3d 6 or 13, 0x83edac); when his rating for the slot (0x69d1b0, 0x83ee23) is below the
 * slot's need and he has no contract offer from the club (find_contract_offer 0x4cc9e0, 0x83ee4b), it releases him:
 * MANAGER_MANAGER::remove_staff_from_club 0x690890 (staff, club) at 0x83ee72. The other staff jobs take the same
 * steps with their own release call: +0x3d 7 or 14 at 0x83ef82, 8 or 15 at 0x83f092, then 0x83f19b and 0x83f2b1.
 * Nothing there looks at his loan
 * contract: for a staff on loan at the club, remove_staff_from_club removes his main contract, the one with his
 * parent club (0x4d0b50, called at 0x69089e), and the loan contract stays. On the loan's last day
 * CONTRACT_MANAGER::daily_update 0x4c7ac0 calls end_loan 0x4d07e0 (0x4c8114), which needs the main contract
 * (0x4d0857..0x4d088b) and shows the box at 0x4d08e3. Seen in soak-2021 leg 1 (Select All, 2021): Brown de Adrogue
 * released Alfredo Grelak, a player/coach on loan from Platense to day 262 of 2022, so his Platense contract went
 * and the loan stayed. A second case, soak-tip leg 0 (main 144ed82c, a new 2021 game): Francisco Beltrao released
 * Juliano Cesar de Moraes Tobias, on loan from Joinville to day 123 of 2022, at 0x83f092.
 *
 * The fix, in a season game (season_game_changes, Q152): at the five calls, the release of a staff who has a loan
 * contract does not happen; he keeps his contract and his loan, and a club can release him after the loan ends. The
 * slot fields that the original clears before each call (as 0x83ee62, 0x83ee68) stay cleared. The base game keeps
 * the original's calls. Test switch RECOMP_TEST_LOAN_RELEASE=original: a season game takes the original's calls too,
 * so that the net below acts.
 *
 * The net (all games; the test switch RECOMP_BOX_FIXES=off turns it off): end_loan of a staff with no main contract
 * does not call the original: remove_staff_from_club (staff, loan club) takes him off the loan club's staff slots,
 * then CONTRACT_MANAGER::remove_loan_contract 0x4d0bd0 frees the loan contract. Logged. */
#include "win.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAFF_COUNT 0xae2364u
#define STAFF_ROWS 0xae2d0cu            /* 79-byte rows: +0 main contract, +4 loan contract */
#define MANAGER_MANAGER 0xb67a44u       /* ecx of remove_staff_from_club (0x83ee6c) */
#define CLUB_OF 0x39u
/* The return addresses of the five release calls of assess_club_squad (0x83ee72, 0x83ef82, 0x83f092, 0x83f19b,
 * 0x83f2b1). */
static const uint32_t release_in_assess[] = {0x0083ee77u, 0x0083ef87u, 0x0083f097u, 0x0083f1a0u, 0x0083f2b6u};
#define REMOVE_STAFF 0x00690890u
#define REMOVE_LOAN 0x004d0bd0u

void o_00690890(void);
void o_004d07e0(void);

static int contract_index(uint32_t staff, uint32_t k)
{
    uint32_t id = rd32(g_mem, staff);
    if (id >= rd32(g_mem, STAFF_COUNT) || !rd32(g_mem, STAFF_ROWS)) return -1;
    return (int32_t)rd32(g_mem, rd32(g_mem, STAFF_ROWS) + id * 79u + 4u * k);
}

static const char *name_of(uint32_t staff, char *out, size_t size)
{
    uint32_t first = rd32(g_mem, staff + 0x4), second = rd32(g_mem, staff + 0x8);
    snprintf(out, size, "%s %s", first ? win_str(first) : "", second ? win_str(second) : "");
    return out;
}

static const char *club_name(uint32_t club)
{
    return club ? win_str(club + 4) : "no club";
}

static int test_original(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("RECOMP_TEST_LOAN_RELEASE"); v = e && !strcmp(e, "original"); }
    return v;
}

static int from_assess(uint32_t ret)
{
    for (unsigned k = 0; k < sizeof release_in_assess / sizeof *release_in_assess; k++)
        if (ret == release_in_assess[k]) return 1;
    return 0;
}

/* remove_staff_from_club 0x690890, thiscall (staff, club) ret 8. */
RT_WRAP(00690890, remove_staff_from_club)
{
    uint32_t staff = rt_arg(0), club = rt_arg(1);
    if (from_assess(rd32(g_mem, cpu.esp)) && staff && season_game_changes() && !test_original() &&
        contract_index(staff, 1) >= 0) {
        if (getenv("RECOMP_SCREEN_LOG")) {
            char name[120];
            fprintf(stderr, "recomp: loans: %s has a loan contract, so %s does not release him before the loan ends\n",
                    name_of(staff, name, sizeof name), club_name(club));
        }
        rt_ret_stdcall(0, 2);
        return;
    }
    o_00690890();
}

/* CONTRACT_MANAGER::end_loan 0x4d07e0, thiscall (staff) ret 4. */
RT_WRAP(004d07e0, end_loan)
{
    uint32_t staff = rt_arg(0), self = cpu.ecx, loan_club = staff ? rd32(g_mem, staff + CLUB_OF) : 0;
    const char *off = getenv("RECOMP_BOX_FIXES");
    if (!staff || !loan_club || contract_index(staff, 0) >= 0 || contract_index(staff, 1) < 0 ||
        (off && !strcmp(off, "off"))) {
        o_004d07e0();
        return;
    }
    char name[120];
    fprintf(stderr, "recomp: loans: net: %s has no main contract at the end of his loan at %s: the loan is freed\n",
            name_of(staff, name, sizeof name), club_name(loan_club));
    Cpu saved = cpu;
    uint32_t a[2] = {staff, loan_club};
    cpu.ecx = rd32(g_mem, MANAGER_MANAGER);
    rt_callback(REMOVE_STAFF, 2, a);
    if (rt_unwinding) return;
    cpu.ecx = self;
    rt_callback(REMOVE_LOAN, 1, a);
    if (rt_unwinding) return;
    cpu = saved;
    rt_ret_stdcall(0, 1);
}
