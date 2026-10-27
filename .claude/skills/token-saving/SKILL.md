---
name: token-saving
description: How this project keeps agent output and sessions short, and where the full logs are. Use when a build or test prints a short view, when "RAW=1", "run-logs", "grep cap" or "-full.png" comes up, when a hook seems to hide output, when you plan a fix round, or when you measure token use or estimate the cost of the rest of a release. The general method is the user skill lean-agent-sessions.
---

# Token saving in this project

Spec: `docs/specs/2026-10-19-token-saving-spec.md` (decisions Q1 to Q12, the Q6 trial). The general method, the traps and the cost estimate are in the user skill `lean-agent-sessions`. This file holds what is specific to this project.

## What runs here

- `.claude/settings.json` runs two PreToolUse hooks: `.claude/hooks/bash_rewrite.py` and `.claude/hooks/read_shrink.py`. Both let the call through when they fail. `/Users/max/Projects/cm0102/.claude/settings.json` holds the same settings for sessions started in the parent folder.
- `tools/dev/run_logged.py` is the wrapper. Full logs are in `/Users/max/Projects/cm0102/run-logs/`, outside every worktree. Shrunk screenshots are in `run-logs/shots-1024/`.
- The hook wraps cmake, ctest, make, ninja, `smoke_run.py`, every `*_run.py` and `*_check.py`, `recomp.py` and `make_release.sh`. A command inside a script is not wrapped: a script that starts many runs writes its own log files.
- The cache lasts 1 hour (`promptCacheTtl`, `subagentPromptCacheTtl`).

## Failure lines specific to this project

The wrapper always prints these lines:

- the same-save check: a line with "differ" and "save"
- `FAIL` lines from `smoke_run.py` and `rules_report_check.py`
- `recomp: message box`, which the runtime prints for every error box of the game
- an error box of the original, for example `Database.cpp 13471`

If a new test prints its failure in another form, add the pattern to `ERROR_PATTERNS` in `tools/dev/run_logged.py` and a sample to `tests/dev/test_hooks.py`.

## Rules for agents

- Read the log file when the short view is not enough. Put `RAW=1` at the start of a command only when you need the full output in the reply.
- grep stops at 100 lines. Use `-l` or `-c` first.
- Name a screenshot `-full.png` when you need the full size, for example to read small widescreen text.
- Each slice agent reads the shared brief and its own slice file (in `/Users/max/Projects/cm0102/lead-notes/`, outside the repo).
- The slice-build skill has the fix-round rules and the session-state files.

## Measuring

`python3 tools/dev/token_report.py --from <date> --to <date>` gives tokens per day and model, cache read per turn and tool output by tool. The figures before the change (16 to 18 October 2026): 215,000 tokens of context per turn on average, 9.9 billion cache-read tokens, about 46,000 turns. Write the figures after the change and the Q6 trial result in the spec, then copy the result into the Results section of `lean-agent-sessions`.

## Tests

`ctest -R hooks` in `build/` runs `tests/dev/test_hooks.py`. Run it after any change to a hook or the wrapper.
