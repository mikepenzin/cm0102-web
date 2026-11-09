# Original transfer and contract decision evidence

This chapter supplies four bounded algorithm contracts extracted from actual instruction bodies. It supplements the transfer/contract workflow requirements; it does **not** establish the complete transfer market, club bid valuation, player willingness, wage demands, or contract negotiation simulation.

## 1. Evidence boundary and reproduction

The inspected executable is `/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game/cm0102.exe`, SHA-256 `0c78e7da6d0bcb4603fa179449bbe6d1b6b5e1c627a22c445d14d489bd9bff86`. Its identity is recorded in [the data audit](evidence/data-audit.json). The generated C is derived from this matching original executable. Function names in [the generated index](evidence/generated-function-index.tsv) are research navigation hints, not independently sufficient evidence of behavior.

**Observed static evidence:** the generated bodies listed below were read, including actual loads, writes, comparisons, branch directions and calling conventions. Their instruction sequences were independently disassembled from the matching PE using Capstone. Numeric formulas were derived from those instructions, then tested within the bounded coverage below. No live game was launched for this chapter.

**Observed isolated execution:** the retained [probe](evidence/negotiation-probe.py) executed original x86 instructions under Unicorn using synthetic memory. All **65,867 cases passed**: 65,536 offer closure state/substate pairs, 256 expired-status bytes, 64 release-clause combinations, four talks-status cases, and seven no-list aggregation cases. [The result report](evidence/negotiation-probe-results.json) records counts, executable/script hashes, mapping size and limitations. These are passing original-code oracle fixtures; the future web implementation still needs to run its equivalent tests.

The initial sandbox attempt terminated with `SIGILL`. An initial run outside the sandbox then exposed a probe-loader error: a fixed `0x800000` mapping did not contain the executable's resource section. The loader was corrected to use PE `SizeOfImage`, `0x9e7000`, with base `0x400000`; the expected algorithms and cases were unchanged. The corrected isolated run passed. These harness issues are not gameplay findings. No live-game UI, complete negotiation lifecycle, calendar processing or save/load was tested.

**Translator caveat:** generated C can contain inserted safety behavior. For example, [fn_033.c](../../build-web-evidence/original/gen/fn_033.c), line 3685, inserts a null-pointer branch before the translated instruction at `0x8e8827` in `0x8e8800`. That branch must not be called original game behavior merely because it appears in generated C. This chapter's four contracts follow the independently checked original instruction paths, not that separate injected guard. Generated line numbers refer to the current evidence export and can change after regeneration.

| Contract | Original entry | Generated body |
|---|---|---|
| Offer closure predicate | `0x8d0d90` | [fn_032.c](../../build-web-evidence/original/gen/fn_032.c), lines 60369–60462 |
| Per-person talks status lookup and normalization | `0x8e7f10` | [fn_033.c](../../build-web-evidence/original/gen/fn_033.c), lines 1180–1301 |
| Contract status and release-clause predicates | `0x85f9f0`, `0x867b50` | [fn_029.c](../../build-web-evidence/original/gen/fn_029.c), lines 111653–111679; [fn_030.c](../../build-web-evidence/original/gen/fn_030.c), lines 1131–1181 |
| Offer value aggregation | `0x8e7a70` | [fn_033.c](../../build-web-evidence/original/gen/fn_033.c), lines 4–283 |

These are original live-memory offsets, not database import offsets. They must not be applied to a packed `.dat` record or arbitrary save member. `u8` means unsigned byte; `i8` means signed byte; `i32` means signed 32-bit interpretation; `add32` and `mul32` retain the low 32 bits. The full destination entity mappings remain a separate recovery task.

## 2. Offer closure depends on two bytes

At `0x8d0d90`, the function takes an offer pointer as its stack argument. It reads the byte at `offer+0x2c` and, only for one state, the unsigned byte at `offer+0x2e`. It returns a full 32-bit `0` or `1`.

```text
closed(offer):
    state = u8(offer + 0x2c)
    if state in {2, 3, 4, 16, 20}: return true
    if state == 15: return u8(offer + 0x2e) >= 3
    return false
```

The set is literal and non-contiguous. In particular, state `15` is not always closed, and states numerically above `20` are not automatically closed. The comparison at `0x8d0dbc` is unsigned `jae`; a substate byte of `255` therefore satisfies the threshold.

**Observed:** no state writes, allocation, date comparison, random call, or other helper call occurs in this body. **Unknown:** the complete names and transition meanings of these numeric states and the `+0x2e` byte. Calling state `2` “rejected” or state `20` “completed” solely from this predicate would exceed the evidence.

**Port contract:** preserve the raw state/substate pair during native import. A decoded enum may present named states only after their writer paths and news/UI consumers have been checked. This predicate can be implemented independently of those labels. It must not stand in for the full command-permission matrix: an open offer is not thereby eligible for every action.

Acceptance vectors, derived from the body and passed against original instructions:

| State byte | Substate byte | Closed |
|---:|---:|---|
| 2, 3, 4, 16 or 20 | Any | True |
| 15 | 0, 1 or 2 | False |
| 15 | 3 through 255 | True |
| 0, 1, 5, 14, 17, 19, 21 or 255 | Any | False |

The isolated-oracle fixture enumerated all 65,536 state/substate pairs, confirmed the return value, and asserted that the offer bytes were unchanged. Repeat the same coverage against the port.

## 3. Talks status is a lookup with a persistent normalization side effect

At `0x8e7f10`, `ECX` supplies the offer and the stack argument supplies a person identifier. The comparison at `0x8e7f18` distinguishes the identifier stored at `offer+0x0c` from identifiers in the list referenced by `offer+0x04`.

```text
talksStatus(offer, personId):
    if personId == u32(offer + 0x0c):
        if u8(offer + 0x2c) == 3:
            write_u8(offer + 0x10, 4)
        return u8(offer + 0x10)

    list = pointer(offer + 0x04)
    if list != null:
        reset list cursor to its head
        for each item in list order:
            if u32(item + 0x00) == personId:
                return u8(item + 0x04)
    return 4
```

The return contract is the low byte `AL`, not the entire `EAX`. The original uses `mov al` on paths where upper register bits need not be cleared. A bridge which reads full `EAX` as the status can produce spurious large values.

**Observed persistent write:** asking for the primary person's status when the offer state byte is `3` writes `4` into `offer+0x10` before returning. A function whose research name looks like a getter is therefore not entirely read-only. The same normalization does not occur on the auxiliary-list path. A missing identifier also returns `4`, so this return value alone cannot distinguish “not in this offer” from a stored status byte of `4`.

**Observed list behavior:** the routine copies `list+0x02` to `list+0x0a` before iteration. The actual iterator body at `0x68d1b0` ([fn_018.c](../../build-web-evidence/original/gen/fn_018.c), lines 34205–34245) reads the node at `list+0x0a`, returns its pointer at `node+0`, and advances the cursor using the pointer at `node+6`. It returns zero at exhaustion. The first matching identifier wins. On an early return, the shared cursor remains advanced past the matched node; after an unsuccessful complete scan, it is zero. These internal cursor changes are distinct from the meaningful status write.

**Port contract:** model the primary person and ordered exchange-person entries separately. Preserve per-person talks status; one status on the overall offer is insufficient. Decide explicitly where primary-status normalization occurs in the authoritative engine so Angular rendering does not silently mutate the world. A typed pure projection may return the normalized value, but persistence/command sequencing must still reproduce any downstream behavior that depends on the original stored write. Internal native iterator cursors do not need to become public API fields.

Acceptance vectors, derived from the body and passed against original instructions:

1. Primary identifier, offer state not `3`, stored talks byte `7`: return byte `7`, leave it unchanged.
2. Primary identifier, offer state `3`, stored talks byte `7`: return byte `4`, persist byte `4`.
3. Different identifier, null or exhausted list: return byte `4`; do not rewrite the primary status.
4. Two auxiliary entries with the same identifier and statuses `6`, then `7`: return `6`, with native cursor advanced to the second node.

**Unknown:** names of talks-status values, how refusal/acceptance transitions are scheduled, how duplicated entries are prevented in normal play, and which caller relies on the normalization write. The lookup does not prove those algorithms.

## 4. Two contract predicates use exact values and masked status

### 4.1 Expired-status predicate

At `0x85f9f0`, `ECX` supplies a contract pointer. The body consists of a byte read, mask, comparison and `sete al`:

```text
expiredStatus(contract):
    return (u8(contract + 0x35) & 0x3f) == 3
```

The original body does not compare a date. It examines an already encoded state and returns its Boolean result in `AL`. Therefore a web implementation must not replace this predicate with `contractEndDate < today` and assume equivalence. Another process must establish or change the status; that writer and its timing require independent recovery.

Flags `0x03`, `0x43`, `0x83` and `0xc3` all produce true. All other byte values produce false. The upper two bits do not affect this predicate and must not be discarded from a retained raw contract record.

### 4.2 Active release-clause predicate

At `0x867b50`, `ECX` supplies a contract pointer. It returns full `EAX` zero or one:

```text
hasActiveReleaseClause(contract):
    return u8(contract + 0x1c) == 2
        or u8(contract + 0x1d) == 1
        or u8(contract + 0x1f) == 2
```

These are equality checks, not generic truthiness checks. Values `(1,0,0)` at those three offsets return false; `(0,1,0)` returns true; `(0,0,2)` returns true; `(0,2,0)` returns false. The routine does not inspect `+0x1e`, release-fee amounts, purchaser nationality, the transfer date, or a proposed bid. Its true result therefore establishes only this encoded clause-activity predicate, not that any particular bid must be accepted.

**Port contract:** distinguish clause existence, clause activity, bid matching and mandatory acceptance. Preserve raw clause state values until each clause-type mapping is verified. A public `releaseClauseActive: boolean` may be a projection of these state bytes; it must not be used as sufficient authorization to complete a transfer.

Passed isolated coverage: all 256 status bytes for the first predicate; for the second, all 64 combinations of values `0`, `1`, `2`, `255` at the three read offsets, including combinations where more than one condition is true. The harness reads the low byte for the first predicate and full register for the second. Retain that distinction in bridge and port tests.

## 5. Offer value aggregation has selectable terms, signed arithmetic and list effects

At `0x8e7a70`, `ECX` supplies the offer and four stack arguments control which contributions are included. Since the verified caller-level names of these arguments are not established here, they are kept as `arg0` through `arg3`. Renaming them from research alone could reverse a caller's intended inclusion flag.

Observed input layout for this routine:

| Location | Use established by body |
|---|---|
| `offer+0x1b`, 32 bits | Initial aggregate value |
| `offer+0x35`, signed 32 bits | Added only if positive and `arg0 == 0` |
| `offer+0x3a`, signed 32 bits | Added only if positive and `arg0 == 0` |
| `offer+0x3f`, signed 8 bits | Percentage factor used only when positive, `arg0 == 0`, and `arg3 != 0` |
| `offer+0x0c`, 32 bits | Main person index |
| `offer+0x04`, pointer | Optional ordered list, whose item begins with another person index |
| Global `0xae23bc` | Base of the live staff array, addressed with stride 110 bytes |
| `staff+0x52`, signed 32 bits | Value read for percentage contribution and the direct-value auxiliary path |

The 110-byte live stride is observable from the `index * 5`, `*11`, `*55`, `*110` address arithmetic. It must not be confused with the packed disk staff width described in [chapter 04](04-data-ui-and-assets.md).

```text
total = i32(offer + 0x1b)
mainStaff = liveStaffBase + 110 * u32(offer + 0x0c)

if arg0 == 0:
    for offset in [0x35, 0x3a]:
        amount = i32(offer + offset)
        if amount > 0: total = add32(total, amount)

    percent = i8(offer + 0x3f)
    if percent > 0 and arg3 != 0:
        product = mul32(i32(mainStaff + 0x52), percent)
        total = add32(total, truncTowardZero(product / 100))

if pointer(offer + 0x04) != null and arg1 == 0:
    reset that list's shared cursor to its head
    for each item in list order:
        if arg2 == 0:
            otherStaff = liveStaffBase + 110 * u32(item + 0)
            total = add32(total, i32(otherStaff + 0x52))
        else:
            total = add32(total, result of helper 0x4d0c60)

return total as 32 bits
```

At `0x8e7abb`, the product is truncated to 32 bits **before** the signed division sequence. The magic multiply by `0x51eb851f`, arithmetic shift and sign correction at `0x8e7abe`–`0x8e7acd` implement signed division by 100 rounded toward zero for that already wrapped product. JavaScript floating-point multiplication followed by a final integer cast does not reproduce overflow at this intermediate step. Use the compatibility numeric helpers specified in the architecture.

The auxiliary branch with `arg2 != 0` calls `0x4d0c60` with the manager context loaded from global `0xae1050` and stack arguments `(mainStaff, 0, 0, -1, 0)`, once per visited item. The observed pointer is the previously calculated **main** staff pointer; this path does not load the current auxiliary item's staff index before that call. Do not silently substitute the exchange person's pointer because it appears more intuitive. The helper's result, internal behavior, side effects and intended caller usage are **unresolved** in this chapter.

**Observed:** the body always begins with the `+0x1b` value; optional amount fields contribute only when signed-positive; a percent byte `200` is signed-negative and does not contribute. The percentage is based on the main staff field, not on the running cash total. Every addition wraps to 32 bits. List iteration mutates the shared cursor as in section 3. No random call is directly made on the no-list or `arg2 == 0` paths.

**Semantic boundary:** research calls this routine `TRANSFER_OFFER::get_total_value`. The arithmetic confirms an aggregation operation, but does not by itself establish which real-world clause each amount represents, the currency unit, whether the result is displayed or used for an AI decision, or whether a different weighted-value function governs acceptance. This chapter deliberately does not treat nominal aggregation as the seller's bid-acceptance formula.

Acceptance vectors for the no-list path, derived from the body and all passed against original instructions:

| Initial | `+0x35` | `+0x3a` | Percent byte | Main staff `+0x52` | `arg0` | `arg3` | Result as signed 32 bits |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 20 | 30 | 10 | 12,345 | 0 | 0 | 150 |
| 100 | 20 | 30 | 10 | 12,345 | 0 | 1 | 1,384 |
| 100 | -20 | 30 | 10 | 12,345 | 0 | 0 | 130 |
| 100 | 20 | 30 | 10 | 12,345 | 1 | 1 | 100 |
| 100 | 20 | 30 | 200 | 12,345 | 0 | 1 | 150 |
| 0 | 0 | 0 | 100 | 2,147,483,647 | 0 | 1 | -1 |
| 2,147,483,647 | 1 | 0 | 0 | 0 | 0 | 0 | -2,147,483,648 |

In the penultimate vector, the multiply wraps to `-100`, so signed division yields `-1`. This intentionally tests arithmetic fidelity, not a claim that such a staff value occurs in an ordinary career. Add ordered auxiliary-list fixtures with `arg1` on/off, `arg2 == 0`, zero/negative auxiliary values, and overflow. The alternate helper branch needs its own independently recovered oracle before implementation can be considered complete.

## 6. Remaining negotiation recovery gates

The four contracts above can be assigned to an implementation agent as bounded tasks with explicit evidence and acceptance expectations. They do not remove the larger subsystem's recovery gates:

1. Establish the full offer-state and talks-status enum by reading writer paths and consumers, then map accept/reject/withdraw/hold/collapse/complete transitions to dated events and news actions. Start with entries `0x8bf680`, `0x8bfcb0`, `0x8c0080`, `0x8c2180`, `0x8c28c0`, `0x8c34e0`, and `0x8c44e0` in the index; these are navigation candidates, not reviewed behavior claims here.
2. Recover club mandatory-acceptance rules, work permits, roster restrictions, deadlines, free-transfer eligibility and contract-expiry status writers. The `expiredStatus` predicate above is not a free-transfer eligibility rule.
3. Recover player willingness, wage/bonus demands, offer comparison and response delays. A partial inspection of `0x4d4660` revealed many flag/date/helper gates; its research name does not justify reducing it to a simple reputation or wage threshold. A complete semantic contract has not been established here.
4. Recover the meaning and every caller mode of `0x8e7a70`, the separate weighted-value path, clause encoding, and helper `0x4d0c60`. Preserve the amount/percent arithmetic while these meanings are unresolved.
5. Run the retained passing oracle fixtures against the port; expand them to the untested exchange-list and alternate-helper paths, then execute representative complete negotiation lifecycles in the live game. Current bounded evidence cannot confirm user-visible news timing, all state interactions, or save/load behavior during negotiation.

The web UI should retain the full manual/source-derived actions from chapter 04 while these engine details are recovered. It must not replace unresolved original decisions with invented probabilities and claim gameplay parity.
