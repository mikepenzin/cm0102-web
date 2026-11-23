"""International caps and goals above 255 (0.6.2 slice 12, fault 30, Q25, and items 56 and 66;
src/recomp_rt/win/caps_wide.h).

`NationalTeams::record_international_result` 0x76b600 gives each player of a national side his cap: 0x76b890 reads
the caps byte at staff +0x22 and, when it is 0, takes the first-cap path (0x76b897..0x76b8c3: the nation of the
match becomes his first nation, and call 0x86cc50 writes the "first cap" history row). Then 0x76b8c8 mov al,
[esi + 0x22] / 0x76b8ce inc al / 0x76b8d0 mov [esi + 0x22], al raise the byte with no check, so the 256th cap gave
0 and the next one a second first cap. The flags of the inc are not read: 0x76b8d6 add dl, cl sets them again.

Here the byte stops at 255 and the runtime counts on (rt_caps_add). Below 255 the result is the original's.

The screens that show caps take the full count (rt_caps_full, the byte below 255). Each site below reads the byte into
a register that it cleared first (xor) or masks with 0xff, then gives it as the number of a "%d" text (sprintf 0x944c9f,
langlib_sprintf 0x66f4e0) or a news slot. The inventory of all 171 caps sites (run-logs/0.6.2-s12/caps-sites.tsv,
outside the repo): the other readers compare, sort or copy the byte, and see 255, the most it holds.
- 0x45bcaf (selectable column) and 0x45c870 (caps column) of `club_screen_draw_squad_tab` 0x456900, staff in edi
- 0x462550 and 0x462638 of `club_screen_draw_info_tab` 0x460590: the highest and the lowest capped player, staff in eax
- 0x479af2 and 0x47a60a of `squad_numbers_screen_build` 0x475520, staff in edi
- 0x480401 and 0x480f07 of `part_exchange_screen_build` 0x47c180, staff in ecx
- 0x7a9422 of `player_regen_post_retirement_news` 0x7a92e0: news slot 9 (NewsItem::set_slot 0x785790), staff in esi
- 0x7d4c6c of the squad printout 0x7d4460 (caps not 0, read at 0x7d4c65), staff in ebx
- 0x80e5f1 of `search_screens_build_player_search_screen` 0x808690, staff in esi
- 0x874e7f, 0x874ea2 and 0x874ec3 of `staff_screens_build_profile_screen` 0x870700: the nationality line with more than
  one cap (read at 0x874e6a), for more than one goal, one goal and none, staff in ebx

The two sorts by caps compare the full counts: `compare_players_by_caps_then_name` 0x486970 (0x4869a7, 0x4869aa: a in
esi, b in ecx, then sub eax, edx at 0x4869ad) and `search_screens_compare_staff_by_caps` 0x81a630 (0x81a66a, 0x81a66d,
sub at 0x81a670). Both clear eax and edx first.

The national team record "most caps" is a signed word at record +0x2f9, -1 for none. `Club_Records::Club_Records`
0x444520 sets it from every player at a new game (0x44483d compare, jle at 0x444845; 0x444847 store) and
`Club_Records::update_records_for_match` 0x444930 after each international (0x445ef3 compare, jle at 0x445efb;
0x445f3d store). They take the full count up to 32767 (rt_caps_word), so a record above 255 is kept and beaten.

The club info tab's "Total Number Of Caps" is a word (ClubSquadStats +0x32, shown by a movsx at 0x4624c4) that
`club_screen_collect_squad_stats` 0x487840 adds each player's caps to (0x487ad1 movzx ax, then 0x487ad6 add). The add
takes the full count. ax keeps the byte, as before: 0x487ada loads al again and the code after it masks with 0xff, and
the and at 0x487ae4 sets the flags again.

The highest and the lowest capped player of that tab (item 56, an original fault from 128 caps): 0x487840 keeps the
best caps so far in the signed byte locals [esp + 0x12] and [esp + 0x13] (stored at 0x487afb, 0x487b1a) and reads them
with movsx (0x487add, 0x487aff), so a player of 128 to 255 caps counted as fewer than none. The compares take the full
caps of the player found so far (ClubSquadStats +0x34 and +0x38, set with the locals at 0x487af5 and 0x487b14) and of
this player (0x487ae4, 0x487b04: the and that kept the byte). With no player found yet the original takes this one
whatever the compare gives (0x487aee, 0x487b0d), so 0 there changes nothing. Below 128 every result is the original's.

International goals (item 66) in the same way as the caps. 0x76b8cb mov dl, [esi + 0x23] / 0x76b8d3 mov cl,
[edi + 0x21] (his goals of the match) / 0x76b8d6 add dl, cl / 0x76b8d8 mov [esi + 0x23], dl added them with no check.
The add stops the byte at 255 and the runtime counts on (rt_goals_add). The flags of the add are not read: the code
after it to the call at 0x76b8fd only moves values. The screens that show goals take the full count (rt_goals_full):
- 0x45bcbd (selectable column) and 0x45c8ac (goals column) of `club_screen_draw_squad_tab` 0x456900, staff in edi
- 0x479b00 and 0x47a647 of `squad_numbers_screen_build` 0x475520, staff in edi
- 0x480429 and 0x480f4c of `part_exchange_screen_build` 0x47c180, staff in eax
- 0x7d4d17 of the squad printout 0x7d4460 (goals not 0, read at 0x7d4d10), staff in ebx
- 0x80e630 of `search_screens_build_player_search_screen` 0x808690, staff in esi
- 0x874e79 and 0x874eed of `staff_screens_build_profile_screen` 0x870700: the goals of the nationality line with more
  than one cap (read at 0x874e71) and with one cap (read at 0x874ee6), staff in ebx
Each clears the register first (xor) or masks it with 0xff. The two sorts by goals compare the full counts, as those by
caps: 0x486a67 and 0x486a6a of `compare_players_by_international_goals_then_name` 0x486a30 (sub at 0x486a6d) and
0x81a6fa and 0x81a6fd of `search_screens_compare_staff_by_international_goals` 0x81a6c0 (sub at 0x81a700), after
xor eax, eax and xor edx, edx.
"""

INSNS = {
    0x76B8CE: ("inc al", "eax = (eax & 0xffffff00u) | rt_caps_add(esi, eax & 0xffu);"),
    0x45BCAF: ("mov dl, byte ptr [edi + 0x22]", "edx = rt_caps_full(edi);"),
    0x45C870: ("mov dl, byte ptr [edi + 0x22]", "edx = rt_caps_full(edi);"),
    0x462550: ("mov cl, byte ptr [eax + 0x22]", "ecx = rt_caps_full(eax);"),
    0x462638: ("mov cl, byte ptr [eax + 0x22]", "ecx = rt_caps_full(eax);"),
    0x479AF2: ("mov dl, byte ptr [edi + 0x22]", "edx = rt_caps_full(edi);"),
    0x47A60A: ("mov al, byte ptr [edi + 0x22]", "eax = rt_caps_full(edi);"),
    0x480401: ("mov al, byte ptr [ecx + 0x22]", "eax = rt_caps_full(ecx);"),
    0x480F07: ("mov al, byte ptr [ecx + 0x22]", "eax = rt_caps_full(ecx);"),
    0x7A9422: ("mov cl, byte ptr [esi + 0x22]", "ecx = rt_caps_full(esi);"),
    0x7D4C6C: ("and eax, 0xff", "eax = rt_caps_full(ebx);"),
    0x80E5F1: ("mov cl, byte ptr [esi + 0x22]", "ecx = rt_caps_full(esi);"),
    0x874E7F: ("and eax, 0xff", "eax = rt_caps_full(ebx);"),
    0x874EA2: ("and eax, 0xff", "eax = rt_caps_full(ebx);"),
    0x874EC3: ("and eax, 0xff", "eax = rt_caps_full(ebx);"),
    0x4869A7: ("mov al, byte ptr [esi + 0x22]", "eax = rt_caps_full(esi);"),
    0x4869AA: ("mov dl, byte ptr [ecx + 0x22]", "edx = rt_caps_full(ecx);"),
    0x81A66A: ("mov al, byte ptr [esi + 0x22]", "eax = rt_caps_full(esi);"),
    0x81A66D: ("mov dl, byte ptr [ecx + 0x22]", "edx = rt_caps_full(ecx);"),
    0x44483D: ("movzx dx, byte ptr [esi + 0x22]", "edx = (edx & 0xffff0000u) | rt_caps_word(esi);"),
    0x444847: ("movzx cx, byte ptr [esi + 0x22]", "ecx = (ecx & 0xffff0000u) | rt_caps_word(esi);"),
    0x445EF3: ("movzx cx, byte ptr [esi + 0x22]", "ecx = (ecx & 0xffff0000u) | rt_caps_word(esi);"),
    0x445F3D: ("movzx dx, byte ptr [esi + 0x22]", "edx = (edx & 0xffff0000u) | rt_caps_word(esi);"),
    0x487AD6: ("add word ptr [esi + 0x32], ax", "W16(esi + 0x32u, (uint16_t)(R16(esi + 0x32u) + rt_caps_word(edi)));"),
    0x487ADD: ("movsx edx, byte ptr [esp + 0x12]", "edx = R32(esi + 0x34u) ? rt_caps_full(R32(esi + 0x34u)) : 0u;"),
    0x487AE4: ("and ecx, 0xff", "ecx = rt_caps_full(edi);"),
    0x487AFF: ("movsx ecx, byte ptr [esp + 0x13]", "ecx = R32(esi + 0x38u) ? rt_caps_full(R32(esi + 0x38u)) : 0u;"),
    0x487B04: ("and eax, 0xff", "eax = rt_caps_full(edi);"),
    # International goals (item 66)
    0x76B8D6: ("add dl, cl", "edx = (edx & 0xffffff00u) | rt_goals_add(esi, edx & 0xffu, ecx & 0xffu);"),
    0x45BCBD: ("mov dl, byte ptr [edi + 0x23]", "edx = rt_goals_full(edi);"),
    0x45C8AC: ("mov dl, byte ptr [edi + 0x23]", "edx = rt_goals_full(edi);"),
    0x479B00: ("mov dl, byte ptr [edi + 0x23]", "edx = rt_goals_full(edi);"),
    0x47A647: ("mov al, byte ptr [edi + 0x23]", "eax = rt_goals_full(edi);"),
    0x480429: ("mov dl, byte ptr [eax + 0x23]", "edx = rt_goals_full(eax);"),
    0x480F4C: ("mov dl, byte ptr [eax + 0x23]", "edx = rt_goals_full(eax);"),
    0x7D4D17: ("and eax, 0xff", "eax = rt_goals_full(ebx);"),
    0x80E630: ("mov cl, byte ptr [esi + 0x23]", "ecx = rt_goals_full(esi);"),
    0x874E79: ("and ecx, 0xff", "ecx = rt_goals_full(ebx);"),
    0x874EED: ("and eax, 0xff", "eax = rt_goals_full(ebx);"),
    0x486A67: ("mov al, byte ptr [esi + 0x23]", "eax = rt_goals_full(esi);"),
    0x486A6A: ("mov dl, byte ptr [ecx + 0x23]", "edx = rt_goals_full(ecx);"),
    0x81A6FA: ("mov al, byte ptr [esi + 0x23]", "eax = rt_goals_full(esi);"),
    0x81A6FD: ("mov dl, byte ptr [ecx + 0x23]", "edx = rt_goals_full(ecx);"),
}
