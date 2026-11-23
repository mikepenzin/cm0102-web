/* The third-place tie whose loser comes late (0.6.2 open item 73).
 *
 * Cup::update_for_date 0x51f890 sets up a last round of code 0x8c (the third place) on the day of its draw date
 * (0x51f979..0x51f9ef, 0x51fab6..0x51fb1f). When a semi-final is played after that day, Cup::set_up_round 0x51c800
 * has already drawn the third-place tie with one club missing (a slot of -1 passes Cup::draw_round 0x51c410 at
 * 0x51c47f) and set it up: Cup::create_tie_fixtures 0x51cb90 gives it state 3 and makes no fixture
 * (0x51cc79..0x51cc9b). When the late semi-final is decided, Cup::process_fixture 0x51a150 puts its winner into the
 * final and its loser into the third-place tie. For the winner's tie it makes the fixture there and then (a tie of
 * the next round with state 3 and both clubs: the check at 0x51b67b, the home side 0x51e870 at 0x51b6a8, the fixture
 * filled in place, vtable +0x7c at 0x51b72b, and added by FIXTURE_MANAGER::add_fixture 0x5aa680), but for the
 * loser's tie it makes none, so the third-place match never comes and the cup never ends: no history row and no next edition (the AFC Champions
 * League 2020/21 of a 2013 game, semi-final moved from 27 May to 2 June 2021 by AFC World Cup qualifiers on 28 May).
 *
 * The wrap of process_fixture (season_playoffs.c) marks the third-place ties that were set up with a club missing
 * before the original runs, and after it makes the fixture of each one that now has both clubs: the home side
 * through Cup::set_home_teams_for_round 0x51e870, as the winner's tie, then create_tie_fixtures, as the round's
 * set-up does for a tie with both clubs. A tie set up with both clubs already
 * has its fixture and is never touched, and a tie that is not yet set up (state 0 to 2) gets its fixture from the
 * set-up as before. A fault of the original that long games reach: the fix acts in every game, and the test switch
 * RECOMP_ORIGINAL_FAULTS=keep turns it off, as the fixes of original_faults.c. */
#ifndef SEASON_CUP_THIRD_PLACE_H
#define SEASON_CUP_THIRD_PLACE_H

#include <stdint.h>

#define THIRD_PLACE_TIES 8

typedef struct {
    uint32_t cup;
    int round, n;                       /* the third-place round, -1 for none; its ties */
    int waiting[THIRD_PLACE_TIES];      /* set up with a club missing, so no fixture */
} ThirdPlaceTies;

void season_cup_third_place_before(uint32_t cup, ThirdPlaceTies *s);
void season_cup_third_place_after(const ThirdPlaceTies *s);

#endif
