# The Page Breathes Inside the Game's Sort: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The page gets a moment every 100 ms while the game sorts (qsort's swap), so a new game no longer freezes it for seconds.

**Architecture:** `RT_WRAP` of the C library's swap 0x9454d5 in `src/recomp_rt/win/user32.c`, beside the random function's wrapper; in the browser build every 1024th call calls `rt_web_breathe()` (host/web.c, unchanged).

**Tech Stack:** C runtime, the recompiler's `RT_WRAP`, Emscripten (JSPI and Asyncify builds).

**Spec:** `docs/superpowers/specs/2026-11-21-sort-breathe-design.md`

## Global Constraints

- Only time passes: nothing of the game changes (rule Q152); the native build calls the original and nothing else.
- The same `rt_web_breathe` as the random function: at most once per 100 ms (`BREATHE_MS`), only in `__EMSCRIPTEN__`.
- Cite the function addresses in the code comment (CLAUDE.md): 0x9454d5 swap, 0x945333 qsort, 0x7bfd50 PlayerStats.

## Review Focus

- The Asyncify build (Safari before JSPI): a sleep inside qsort must be allowed there too; it builds and starts.
- A sort of a list already in order: MSVC's qsort still swaps (the middle element to the front), so the hook runs.
- The native build: the same game, byte for byte (the `-seed 7` dump in the browser, and a native smoke run).

---

### Task 1: Wrap qsort's swap

**Files:**
- Modify: `src/recomp_rt/win/user32.c` (beside `RT_WRAP(0090cfc0, game_random)`)
- Modify: `web/README.md` ("How it works", Time)
- Test: the freeze measurement (scratch harness `freeze.mjs`) and the `-seed 7` dump

**Interfaces:**
- Consumes: `void rt_web_breathe(void)` (host/web.c), `RT_WRAP(addr, name)` (rt.h): the recompiler writes `o_009454d5`.
- Produces: `RT_WRAP(009454d5, sort_swap)`.

- [ ] **Step 1: Measure before** — `freeze.mjs` on the live build: long tasks 1.8 s and 0.8 s in PlayerStats (done, design doc).
- [ ] **Step 2: Add the wrapper** — a static counter; `#ifdef __EMSCRIPTEN__` every 1024th call (`!(n & 0x3ffu)`) calls `rt_web_breathe()`; then `o_009454d5()`.
- [ ] **Step 3: Build** — `emcmake cmake -S . -B <build> -G Ninja -DCM0102_EXE=<game>/cm0102.exe && cmake --build <build> --target cm0102_recomp seasons_rules`. Expected: builds; the recompiler leaves 0x9454d5 out as f_ and writes o_009454d5.
- [ ] **Step 4: Measure after** — `freeze.mjs` on the new build. Expected: no long task over about 0.3 s after "Player Initialisation"; the dump hash 4257912697.
- [ ] **Step 5: Native build and smoke run** — the native build compiles and the smoke run passes.
- [ ] **Step 6: README and commit** — one line in "How it works" (Time); commit and push to main.
