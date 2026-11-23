#!/usr/bin/env python3
"""Reads of the original that depend on where memory lies are patched (0.6.2 open item 138b).

Usage: layout_reads_check.py --exe <cm0102.exe>

tools/recomp/layout_reads.py gives each such read a defined value. This check finds them in the original and fails
when one is not in its INSNS:
- An address used as a number, in one form: `mov eax, dword ptr [reg + off]`, then within six instructions `cdq`
  and `idiv` or `div` with eax not loaded again, where the same function also loads [reg + off] into a register and
  reads memory through it (so the field holds a pointer). In the 3.9.68 exe this form occurs once: choose_captain
  0x6e19b9 divides the manager's non-player record by 20. Other forms (a hash, a compare, `[reg]` with no offset) are
  not searched.
- The local [esp + 0x70] of game_daily_staff_update 0x5ce2d0: its loop reads it at 0x5ce830 and 0x5ce84f, and no
  instruction from the entry to the first read writes it. The patch at 0x5ce311 must write it.
- MATCH_PLAYER::MATCH_PLAYER 0x6eada0 sets no byte from +0x1d to +0x22, which match_eng_update_post_match_reputations
  0x6d1cc0 reads for an empty bench slot (+0x1f, +0x20, +0x21). The patch at 0x6eada0 must clear the record. The
  ctest layout_reads runs the constructor itself.
- SimulatedStats::play_daily_matches 0x84be20 builds a fixture at [esp + 0x40] (it passes [esp + 0x48] after two
  pushes at 0x84c3df) and no instruction of the function writes its match flags word +0x38, at any push depth from 0
  to 12 bytes. quick_simulate_match reads the word (0x6bfeb1). The patch at 0x84c266 must clear the fixture.
- TRANSFER_MANAGER::add_config_loan 0x8bf400 builds a contract at [esp + 0x28] (0x8bf420, also passed at 0x8bf5af)
  and gives it to sign_loan_contract. Neither the function nor STAFF_CONTRACT::STAFF_CONTRACT 0x85e410 nor
  init_from_current_contract 0x85e590 writes its morale byte +0x3d. The patch at 0x8bf420 must clear the contract.
- NationalTeams::select_squad 0x76c120 sets 26 place masks at [esp + 0x70] for squad size 26 (cmp 0x1a at 0x76c769),
  24 for size 24 (cmp 0x18 at 0x76c859) and 22 for any other size: the branch 0x76c939..0x76c9e6 (with 0x76c860 and
  0x76c865) writes no word from [esp + 0x9c]. The patch at 0x76c939 must set the masks from the 23rd to the 50th
  to 0x240, the mask that each size's last places take (0x76c7d9, 0x76c8c1, 0x76c973).
- SimulatedStats: the push depth at 0x84c266 is 0 (0x84c271 writes the fixture's +0xc at [esp + 0x4c]), and
  VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0, which gets the fixture, writes no +0x38 or +0x39 through a register.
- officials_manager_add_random_officials 0x79c5c0 fills each new record in the loop 0x79c6aa..0x79c772 (esi the record
  + 8, 0x79c6a7) and writes no byte of the id +0..+3 or the birth date's leap year flag +0x10..+0x13 there, and the
  sort it calls (0x79c78c) breaks ties by the id (0x79d4c2). The patch at 0x79c6aa must clear the record and set the
  id, and the patch at 0x79c76a must set the flag from the year as Date::Date 0x549ef0 does.
- AWOL::miss_training 0x41a360 writes the stack only at 0x41a38a, 0x41a3e6 and 0x41a57b (the record's staff id), adds a
  13-byte record from [esp + 0x18] (0x41a587, 0x41a58b), and Date::Date 0x54a110 and 0x417c10 write 8 bytes from the
  date +4. So nothing writes the flag +0xc. The patch at 0x41a56b must clear it.
- CONTRACT_MANAGER::initialise_all_staff_contracts 0x4c7560 makes the first contract table (one 0x50-byte slot per
  staff member, malloc at 0x4c75a4, stored at 0x4c75ae), and get_free_staff_contract_space 0x4c79a0 grows it by 200
  slots (0x4c79dd) with realloc 0x945667 or malloc 0x944e46, which both come to 0x4c7a0a, and stores it at 0x4c7a0c.
  Neither writes the new slots, and STAFF_CONTRACT::generate_initial_contract 0x85e960 writes no morale +0x3d. The
  patches at 0x4c75ae and 0x4c7a0c must clear the new slots.
- STAFF_CONTRACT::STAFF_CONTRACT 0x85e410 (this in ecx, 0x85e428) writes the word +0x3b at 0x85e454 and no byte +0x3d.
  The patch at 0x85e428 must clear the 0x50-byte contract.
- TRAINING_SCHEDULE::TRAINING_SCHEDULE 0x8b4e10 writes no category level +4..+8, set_type 0x8b4e50 skips a type above
  7 (0x8b4e64), and set_default_schedules gives the type -1 (0x8b3e63). The patch at 0x8b4e13 must clear the schedule.
- AWOL::report_poor_fitness 0x41add0 writes the stack only at 0x41ae00, 0x41ae2a, 0x41ae38 and 0x41ae9d before its
  return 0x41b01f, adds an 0x11-byte record from [esp + 0xc] (0x41ae44, 0x41ae48), and its date fills +8..+0xf. So
  nothing writes the flag +0x10. The patch at 0x41ae41 must clear it.
- Each patch calls rt_layout_fix with the index of its group in names[] of layout_reads.c.
Each INSNS row must name the instruction that the original has at its address.
Exit code 0 when every case passes.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "recomp"))
import layout_reads  # noqa: E402
from pe import Image  # noqa: E402

DAILY, DAILY_READS, DAILY_LOCAL = 0x5CE2D0, (0x5CE830, 0x5CE84F), "dword ptr [esp + 0x70]"
CTOR, CTOR_END, UNSET = 0x6EADA0, 0x6EAE98, range(0x1D, 0x23)
STATS, STATS_END, STATS_PASS, STATS_FIXTURE, FLAGS = 0x84BE20, 0x84C500, 0x84C3DF, 0x40, {0x38, 0x39}
LOAN, LOAN_END, LOAN_AT, MORALE = 0x8BF400, 0x8BF5B9, 0x8BF420, 0x3D
CONTRACT_FUNCS = ((0x85E410, 0x85E473), (0x85E590, 0x85E746))   # STAFF_CONTRACT::STAFF_CONTRACT, init_from_current_contract
WRITES = ("mov", "and", "or", "xor", "add", "sub", "inc", "dec", "adc", "sbb", "neg", "not", "shl", "shr", "sar",
          "sete", "setne", "setl", "setle", "setg", "setge", "setb", "setbe", "seta", "setae", "sets", "setns")
SIZES = {"byte": 1, "word": 2, "dword": 4}
FILL, FILL_END = 0x90D2F0, 0x90D4EC   # VIRTUAL_STAFF::fill_fixture_squads, which gets the fixture (0x84c2a9, 0x84c2b7)
GROUP_OF = {0x6E19B9: "captain", 0x6EADA0: "players", 0x5CE311: "daily", 0x84C266: "stats", 0x8BF420: "loans",
            0x76C939: "squads", 0x79C6AA: "officials", 0x79C76A: "officials", 0x41A56B: "training",
            0x4C7A0C: "contracts", 0x4C75AE: "contracts",
            0x85E428: "contracts", 0x8B4E13: "schedules",
            0x41AE41: "fitness"}


def disassemble(img):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    start, end = img.text
    return [(i.address, i.mnemonic, i.op_str) for i in md.disasm(img.read(start, end - start), start)]


def address_dividends(code):
    field = re.compile(r"eax, dword ptr \[(e[a-z]{2}) \+ (0x[0-9a-f]+)\]$")
    out = []
    for k, (a, m, o) in enumerate(code):
        f = field.match(o) if m == "mov" else None
        if not f or f.group(1) == "esp":
            continue
        after = code[k + 1:k + 7]
        div = next((n for n, x in enumerate(after) if x[1] in ("idiv", "div")), None)
        if div is None or not any(x[1] == "cdq" for x in after[:div]) or \
                any(x[2].startswith("eax,") and x[1] != "cdq" for x in after[:div]):
            continue
        load = f"dword ptr [{f.group(1)} + {f.group(2)}]"
        for j in range(max(0, k - 120), min(len(code), k + 120)):
            if j == k or code[j][1] != "mov" or not code[j][2].endswith(load):
                continue
            reg = code[j][2].split(",")[0]
            if any(re.search(rf"\[{reg}( \+ [^\]]+)?\]", y[2]) for y in code[j + 1:j + 6]):
                out.append(a)
                break
    return out


def main():
    exe = sys.argv[sys.argv.index("--exe") + 1]
    img = Image(exe)
    code = disassemble(img)
    at = {a: (m, o) for a, m, o in code}
    fails = []
    for a, (text, _) in sorted(layout_reads.INSNS.items()):
        got = " ".join(x for x in at.get(a, ("?", "")) if x)
        if got != text:
            fails.append(f"{a:#x}: the original has {got!r}, INSNS names {text!r}")
    for a in address_dividends(code):
        if a not in layout_reads.INSNS:
            fails.append(f"{a:#x}: an address used as a number ({' '.join(at[a])}) with no patch")
    daily = [x for x in code if DAILY <= x[0] < DAILY_READS[0]]
    if any(x[2].startswith(DAILY_LOCAL + ",") for x in daily):
        fails.append(f"{DAILY:#x}: the original writes {DAILY_LOCAL} before {DAILY_READS[0]:#x}; read the case again")
    for r in DAILY_READS:
        if not at.get(r, ("", ""))[1].endswith(DAILY_LOCAL):
            fails.append(f"{r:#x}: not a read of {DAILY_LOCAL}")
    if "W32(esp + 0x70u" not in layout_reads.INSNS.get(0x5CE311, ("", ""))[1]:
        fails.append(f"{DAILY:#x}: {DAILY_LOCAL} is read before any write, and no patch sets it")
    ctor = [x for x in code if CTOR <= x[0] < CTOR_END]
    set_bytes = set()
    for _, m, o in ctor:
        s = re.match(r"(byte|word|dword) ptr \[eax \+ (0x[0-9a-f]+)\], ", o) if m == "mov" else None
        if s:
            n = {"byte": 1, "word": 2, "dword": 4}[s.group(1)]
            set_bytes |= set(range(int(s.group(2), 16), int(s.group(2), 16) + n))
    if set_bytes & set(UNSET):
        fails.append(f"{CTOR:#x}: the original sets a byte of +0x1d..+0x22; read the case again")
    if "memset(M + ecx, 0, 0x1beu)" not in layout_reads.INSNS.get(CTOR, ("", ""))[1]:
        fails.append(f"{CTOR:#x}: the match player's bytes +0x1d..+0x22 stay unset, and no patch clears them")
    if at.get(STATS_PASS) != ("lea", "ecx, [esp + 0x48]"):
        fails.append(f"{STATS_PASS:#x}: not the fixture at [esp + 0x48]; read the case again")
    for a, m, o in code:
        w = re.match(r"(byte|word|dword) ptr \[esp \+ (0x[0-9a-f]+)\]", o) \
            if STATS <= a < STATS_END and m in WRITES else None
        if w and any(set(range(int(w.group(2), 16) - d - STATS_FIXTURE, int(w.group(2), 16) - d - STATS_FIXTURE
                               + SIZES[w.group(1)])) & FLAGS for d in (0, 4, 8, 12)):
            fails.append(f"{a:#x}: the original may write the simulated fixture's flags ({m} {o}); read the case again")
    if at.get(0x84C271) != ("mov", "dword ptr [esp + 0x4c], eax"):
        fails.append("0x84c266: the push depth is not 0 there (the fixture's +0xc at [esp + 0x4c]); "
                     "read the case again")
    for a, m, o in code:
        w = re.match(r"(byte|word|dword) ptr \[(e[a-z]{2}) \+ (0x[0-9a-f]+)\]", o) \
            if FILL <= a < FILL_END and m in WRITES else None
        at_off = int(w.group(3), 16) if w else 0
        if w and w.group(2) != "esp" and set(range(at_off, at_off + SIZES[w.group(1)])) & FLAGS:
            fails.append(f"{a:#x}: fill_fixture_squads may write the fixture's flags ({m} {o}); read the case again")
    if "memset(M + esp + 0x40u, 0, 0x4fu)" not in layout_reads.INSNS.get(0x84C266, ("", ""))[1]:
        fails.append(f"{STATS:#x}: the simulated fixture's match flags +0x38 stay unset, and no patch clears them")
    if at.get(LOAN_AT) != ("lea", "ecx, [esp + 0x28]") or at.get(0x8BF5AF) != ("lea", "eax, [esp + 0x28]"):
        fails.append(f"{LOAN_AT:#x}: not the loan contract at [esp + 0x28]; read the case again")
    for a, m, o in code:
        own = LOAN <= a < LOAN_END
        if not (own or any(lo <= a < hi for lo, hi in CONTRACT_FUNCS)) or m not in WRITES:
            continue
        w = re.match(r"(byte|word|dword) ptr \[(e[a-z]{2}) \+ (0x[0-9a-f]+)\]", o)
        if not w or (own and w.group(2) != "esp") or (not own and w.group(2) == "esp"):
            continue
        base = 0x28 if own else 0
        if MORALE in {int(w.group(3), 16) - d - base + k for d in ((0, 4, 8, 12, 16, 20, 24) if own else (0,))
                      for k in range(SIZES[w.group(1)])}:
            fails.append(f"{a:#x}: the original may write the loan contract's morale ({m} {o}); read the case again")
    if "memset(M + esp + 0x28u, 0, 0x50u)" not in layout_reads.INSNS.get(LOAN_AT, ("", ""))[1]:
        fails.append(f"{LOAN:#x}: the loan contract's morale +0x3d stays unset, and no patch clears it")
    if at.get(0x76C769) != ("cmp", "al, 0x1a") or at.get(0x76C859) != ("cmp", "al, 0x18"):
        fails.append("0x76c120: the squad sizes are not tested at 0x76c769 and 0x76c859; read the case again")
    other = {int(m.group(1), 16) for a, mn, o in code if 0x76C939 <= a < 0x76C9E6 or a in (0x76C860, 0x76C865)
             for m in [re.match(r"word ptr \[esp \+ (0x[0-9a-f]+)\], ", o)] if m and mn == "mov"}
    if other != set(range(0x70, 0x9C, 2)):
        fails.append(f"0x76c939: the 22-place branch writes the masks {sorted(map(hex, other))}; read the case again")
    if any(at.get(a) != ("mov", "eax, 0x240") for a in (0x76C7D9, 0x76C8C1, 0x76C973)):
        fails.append("0x76c120: the last places of sizes 26, 24 and 22 do not take the mask 0x240; read the case again")
    if "for (uint32_t i = 0x9cu; i < 0xd4u; i += 2u) W16(esp + i, 0x240u);" not in \
            layout_reads.INSNS.get(0x76C939, ("", ""))[1]:
        fails.append("0x76c120: the place masks after the 22nd stay unset for other squad sizes, "
                     "or do not take the original's mask 0x240")
    if at.get(0x79C6A7) != ("lea", "esi, [ecx + 8]") or at.get(0x79C78C) != ("push", "0x79d480") or \
            at.get(0x79D4C2) != ("mov", "eax, dword ptr [edx]") or at.get(0x79D4C4) != ("mov", "ecx, dword ptr [esi]"):
        fails.append("0x79c5c0: not the new official loop and its sort by id; read the case again")
    for a, m, o in code:
        w = re.match(r"(byte|word|dword) ptr \[esi(?: ([+-]) (0x[0-9a-f]+|\d+))?\]", o) \
            if 0x79C6AA <= a < 0x79C772 and m in WRITES else None
        if w:
            off = 8 + (int(w.group(3), 0) * (-1 if w.group(2) == "-" else 1) if w.group(3) else 0)
            if set(range(off, off + SIZES[w.group(1)])) & (set(range(0, 4)) | set(range(0x10, 0x14))):
                fails.append(f"{a:#x}: the original may set the new official's id or leap year flag ({m} {o}); "
                             "read the case again")
    if at.get(0x79C76A) != ("mov", "word ptr [esi + 4], ax") or at.get(0x79C75D) != ("mov", "word ptr [esi + 6], ax"):
        fails.append("0x79c5c0: the loop's last fill is not the birth day after the year; read the case again")
    if "W32(esi + 8u, y_ % 4u == 0u && (y_ % 100u != 0u || y_ % 400u == 0u) ? 1u : 0u)" not in \
            layout_reads.INSNS.get(0x79C76A, ("", ""))[1]:
        fails.append("0x79c5c0: a new official's leap year flag does not follow the year of his birth date")
    if "memset(M + r_, 0, 0x2bu); W32(r_, (r_ - R32(0xae23e8u)) / 0x2bu);" not in \
            layout_reads.INSNS.get(0x79C6AA, ("", ""))[1]:
        fails.append("0x79c5c0: a new official's id stays unset, and no patch sets it")
    esp_writes = {a for a, m, o in code if 0x41A360 <= a < 0x41A62A and m in WRITES
                  and o.startswith(("byte ptr [esp", "word ptr [esp", "dword ptr [esp"))}
    if esp_writes != {0x41A38A, 0x41A3E6, 0x41A57B} or at.get(0x41A587) != ("lea", "edx, [esp + 0x18]") or \
            at.get(0x41A58B) != ("push", "0xd") or at.get(0x41A57B) != ("mov", "dword ptr [esp + 0x20], eax"):
        fails.append(f"0x41a360: the missed training record's writes are {sorted(map(hex, esp_writes))}; "
                     "read the case again")
    for lo, hi in ((0x54A110, 0x54A124), (0x417C10, 0x417C40)):
        for a, m, o in code:
            w = re.match(r"(byte|word|dword) ptr \[(eax|ecx)( \+ (0x[0-9a-f]+|[0-9]))?\]", o) \
                if lo <= a < hi and m in WRITES else None
            if w and (int(w.group(4), 0) if w.group(4) else 0) + SIZES[w.group(1)] > 8:
                fails.append(f"{a:#x}: the date writes past its 8 bytes ({m} {o}); read the case again")
    if "W8(esp + 0x24u, 0u)" not in layout_reads.INSNS.get(0x41A56B, ("", ""))[1]:
        fails.append("0x41a360: the missed training record's flag +0xc stays unset, and no patch clears it")
    grow = [(0x4C79DD, "add", "eax, 0xc8"), (0x4C79E4, "mov", "dword ptr [esi + 4], eax"),
            (0x4C79F1, "call", "0x945667"), (0x4C79F9, "jmp", "0x4c7a0a"), (0x4C7A02, "call", "0x944e46"),
            (0x4C7A0A, "test", "eax, eax"), (0x4C79E9, "lea", "eax, [eax + eax*4]"), (0x4C79EC, "shl", "eax, 4"),
            (0x4C759A, "mov", "dword ptr [ebx + 4], eax"), (0x4C759D, "lea", "ecx, [eax + eax*4]"),
            (0x4C75A0, "shl", "ecx, 4"), (0x4C75A4, "call", "0x944e46")]
    if any(at.get(a) != (m, o) for a, m, o in grow):
        fails.append("0x4c79a0: not the growth of the contract table by 200 slots; read the case again")
    for a, m, o in code:
        w = re.match(r"(byte|word|dword) ptr \[(e[a-z]{2}) \+ (0x[0-9a-f]+)\]", o) \
            if 0x85E960 <= a < 0x85ED79 and m in WRITES else None
        if w and w.group(2) != "esp" and MORALE in range(int(w.group(3), 16), int(w.group(3), 16) + SIZES[w.group(1)]):
            fails.append(f"{a:#x}: generate_initial_contract may write the morale ({m} {o}); read the case again")
    if "memset(M + eax + (R32(esi + 4u) - 200u) * 0x50u, 0, 200u * 0x50u)" not in \
            layout_reads.INSNS.get(0x4C7A0C, ("", ""))[1] or "memset(M + eax, 0, R32(ebx + 4u) * 0x50u)" not in \
            layout_reads.INSNS.get(0x4C75AE, ("", ""))[1]:
        fails.append("0x4c79a0: the new slots of the contract table stay unset, and no patch clears them")
    if at.get(0x85E428) != ("mov", "esi, ecx") or at.get(0x85E454) != ("mov", "word ptr [esi + 0x3b], bx") or any(
            m in WRITES and o.startswith(("byte ptr [esi + 0x3d]", "word ptr [esi + 0x3c]", "dword ptr [esi + 0x3a]",
                                          "dword ptr [esi + 0x3b]", "dword ptr [esi + 0x3c]", "dword ptr [esi + 0x3d]",
                                          "word ptr [esi + 0x3d]"))
            for a, m, o in code if 0x85E410 <= a < 0x85E473):
        fails.append("0x85e410: the contract constructor's writes near the morale +0x3d differ; read the case again")
    if "memset(M + ecx, 0, 0x50u)" not in layout_reads.INSNS.get(0x85E428, ("", ""))[1]:
        fails.append("0x85e410: the contract constructor leaves the morale +0x3d unset, and no patch sets it")
    sched = [a for a, m, o in code if 0x8B4E10 <= a < 0x8B4E43 and m in WRITES
             for w in [re.match(r"(byte|word|dword) ptr \[esi \+ (0x[0-9a-f]+|\d+)\]", o)] if w
             and set(range(int(w.group(2), 0), int(w.group(2), 0) + SIZES[w.group(1)])) & set(range(4, 9))]
    sizes = [at.get(a) for a in (0x8B0EEE, 0x8B0A30, 0x8B987D)]   # the schedule size at each place that builds them
    if sched or sizes != [("push", "0x9b")] * 3 or at.get(0x8B4E64) != ("ja", "0x8b4f05") or at.get(0x8B4E61) != ("cmp", "eax, 7") \
            or at.get(0x8B3E63) != ("push", "-1"):
        fails.append("0x8b4e10: the schedule constructor or set_type differ at the levels; read the case again")
    if "memset(M + esi, 0, 0x9bu)" not in layout_reads.INSNS.get(0x8B4E13, ("", ""))[1]:
        fails.append("0x8b4e10: the training schedule levels stay unset, and no patch clears them")
    fit = {a for a, m, o in code if 0x41ADD0 <= a < 0x41B01F and m in WRITES
           and o.startswith(("byte ptr [esp", "word ptr [esp", "dword ptr [esp"))}
    if fit != {0x41AE00, 0x41AE2A, 0x41AE38, 0x41AE9D} or at.get(0x41AE44) != ("lea", "eax, [esp + 0xc]") or \
            at.get(0x41AE48) != ("push", "0x11") or at.get(0x41AE34) != ("lea", "ecx, [esp + 0x1c]"):
        fails.append(f"0x41add0: the fitness incident record's writes are {sorted(map(hex, fit))}; read the case again")
    if "W8(esp + 0x1cu, 0u)" not in layout_reads.INSNS.get(0x41AE41, ("", ""))[1]:
        fails.append("0x41add0: the fitness incident record's flag +0x10 stays unset, and no patch clears it")
    # Each patch asks the switch for its own group: the index of its name in names[] of layout_reads.c.
    src = open(os.path.join(HERE, "..", "..", "src", "recomp_rt", "win", "layout_reads.c")).read()
    names = re.findall(r'"([a-z]+)"', re.search(r"names\[GROUPS\] = \{([^}]*)\}", src).group(1))
    for a, group in GROUP_OF.items():
        want = f"rt_layout_fix({names.index(group)}u)" if group in names else None
        if not want or want not in layout_reads.INSNS.get(a, ("", ""))[1]:
            fails.append(f"{a:#x}: the patch does not ask the switch for its group {group} ({want})")
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
