---
name: slice-build
description: Builds the slices of a Bosman Edition release spec with parallel worktree agents, a shared brief, two fresh review agents per slice and a merge to main when the checks pass. Use for "build the release", "build slices 1 to 5", "build the spec", "parallel agents", "run the slices", or a brief from another session that names spec slices. Stops before the release slice: Max approves every publish.
---

# Slice build

The lead session plans, briefs, reviews and merges. Agents build. Read the handover, `/Users/max/Projects/cm0102/lead-notes/repo-moved/docs/HANDOVER.md` (outside the repo), then the spec and its research.

Never commit briefs, hand-over notes, session state or messages to Max. They stay in `/Users/max/Projects/cm0102/lead-notes/`.

## 1. Before any agent starts

1. Check the spec says `Status: Ready` with no open decisions. If not, stop and use the grilling skill.
2. Add any shared test tool that every slice needs to main first, and commit it. Example: `smoke_run.py --same-save --save start.sav` was added before the 0.5.0 slices.
3. Write one brief, `/Users/max/Projects/cm0102/lead-notes/briefs/build-brief-x.y.z.md`, from the template of the last one (old briefs: `/Users/max/Projects/cm0102/lead-notes/repo-moved/docs/research/`). It holds the shared facts (paths, build commands, the main build to compare with), the same-save checks (see the same-save skill), one paragraph per slice with its research file, its test seam and its option, the files that two slices share, and the rules.
4. Write the review brief, `/Users/max/Projects/cm0102/lead-notes/briefs/review-brief-x.y.z.md`, with two angles: spec (does it do what the spec says, can it change a decision, does off give the original) and standards (memory, registers, wrong call sites, file writes, Windows and Mac, comments and evidence).

## 2. Start the agents

- One agent per slice, `isolation: worktree`, run in the background. Opus for anything a player sees, judgement or the recompiler. Sonnet for a routine slice with a clear brief.
- The prompt names the slice and says: "First run `git merge main` (the worktree can be cut from an older commit), then read `/Users/max/Projects/cm0102/lead-notes/briefs/build-brief-x.y.z.md` and follow it." Without the merge, a worktree may miss the shared test tools, and a flag such as `--save` is ignored without an error.
- A slice that depends on others starts when they are on main.
- Tell the build agent to write or update the slice's session-state file (`/Users/max/Projects/cm0102/lead-notes/session-states/slice-NN.md`) before it stops. A fresh fix agent reads that file.
- Tell every agent the output rules in section 6.

## 3. Review each slice

1. When an agent reports, start two fresh Opus reviewers with the review brief, the worktree, the branch and `git diff main...HEAD`. Give each reviewer the risk you see in the report.
2. Send every must-fix and should-fix item to a fresh agent, not to the build agent. Give it the review list, the slice's session-state file (`/Users/max/Projects/cm0102/lead-notes/session-states/slice-NN.md`), the branch and the build agent's worktree path. The fresh agent works in that worktree. Ask it to merge main first, fix, test, commit, update the state file and reply in at most 250 words. Start a new fresh agent for each further fix round.
3. A large design change after review gets one more review of the new design.
4. Look at the screenshots yourself before the merge.
5. Read any change outside the slice's own files (for example an input change in `user32.c`) before you accept it.

## 4. Merge

- `git merge --no-ff <branch>` into main from the main checkout. Resolve conflicts in the shared lists (options table, `ctest` cases, `SAME_SAVE_RUNS`, `MATCH_RUNS`) by keeping both sides.
- After each merge that touches game code, rebuild `build/` and run `--same-save --save start.sav` and `--save prematch.sav`.
- When all slices are in, run the full test set once (the same-save skill lists it) and build Windows with MinGW to check it links.

## 5. Report

- Record each of Max's answers in the spec under Decisions.
- Update the handover (outside the repo): what is merged, what is next, open questions.
- Put every question for Max in full in the last message, after the wrap-up box.

## 6. Output rules for agents

- Run independent commands in one tool call.
- Speed rules (Max, 2026-10-19 ~20:45). Keep-awake: `caffeinate -i -s` runs (PID in `/Users/max/Projects/cm0102/run-logs/caffeinate.pid`); stop it only after the 0.6.0 release with `kill $(cat /Users/max/Projects/cm0102/run-logs/caffeinate.pid)`. Merge chains together: when a later slice already holds an earlier one, merge both as one step with one same-save check (24 with 23; 28 with 26 and 27's follow-ups; 21b+21c after 26); on a failure, bisect between the two merge commits. Active slice limit 5 for now, the lead included; resume the next paused session in merge order when one merges. Above about 85 % of the 5-hour usage, start no new rounds until the reset. Small fix rounds get one Opus review; a second one only when the first finds a real problem (wrong result, crash, box, same-save difference, fact error). A first build of a slice gets its full review.
- Builds go through the machine-wide queue (Max, 2026-10-19): `/Users/max/Projects/cm0102/build-slot.sh cmake --build build -j4` (any build command after the script). At most 3 builds run at once; the script waits for a free slot and keeps the exit code. Never more than `-j4`.
- Ghidra (the rea MCP) takes 2 to 4 cores per open binary. Open a binary only when a decompile is needed, and call `close_binary` when done.
- Chain build and test in one command, for example the build then `ctest` with `&&`.
- A hook sends build and test output through a log wrapper. It prints the exit code, the error lines, the last 40 lines and the log path in `/Users/max/Projects/cm0102/run-logs/`. If the short view is not enough, read the log file.
- Put `RAW=1` at the start of a command only when you need the full output in the reply.
- grep output stops at 100 lines. Use `-l` or `-c` first, then read the lines you need.
- A screenshot that you view is cut to 1024 px. End a file name with `-full.png` when you need the full size.

## Traps

- A wait loop with `pgrep -f "<name>"` matches its own command line, so it waits for ever. Wait on a file the job writes, or on the job's own background notification.
- Do not rebuild the main `build/` while agents compare against it, unless you tell them.
- Test games under a load above about 20 fail on timing. Rerun a failed run alone before you call it a bug.
- Agents must not run `rm` with a wildcard or a variable path.
