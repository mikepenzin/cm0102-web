# Scout Request Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Ask a manager's club to scout a player from Squad Hub, as the game's "scout him" does.

**Architecture:** `web/shortlist.js` reads and writes the scout requests after the searches in `scout_manager.dat`;
editor.js shows a button per Scout row and in the player panel.

**Spec:** `docs/superpowers/specs/2026-11-25-scout-request-design.md`

## Global Constraints

- Records of 17 bytes: club id, player id, game date (8), reports 0; sorted by player id, then club id (0x7fc270).
- Members only; `web/features.js` key `scout-request`, Club Legends first. Ten languages.

## Review Focus

- A save with no requests (count 0): the first request writes count 1 and the record.
- Two clubs asking for one player: both kept, club order.
- A player at the manager's club, or a manager without a club: no request, no button.
- The save written and read again: searches and the 16 bytes unchanged.

### Task 1: The model
**Files:** `web/shortlist.js`, `web/test/shortlist.test.js`, `web/test/fakes.mjs` (managers with a `club`, and
`requests: [{ club, player, reports }]`).
- [ ] Tests first (round trip with requests, request in order, own club and no club refused, unrequest, written and
  read again), RED, implement, GREEN. The real Hertha save: 21 requests read, Klaus's club 4232, round trip identical.
- [ ] Commit.

### Task 2: The page
**Files:** `web/editor.js`, `web/features.js`, `web/index.html`, i18n tables, `web/test/scout-check.mjs`.
- [ ] scout-check first (the eye button adds a request for Klaus's club, a second tap takes it off, Mike's club when
  Mike is chosen, saved file holds it; locked while early for a member who is no Club Legend), RED, implement, GREEN.
- [ ] Translations; npm test, i18n-check, editor-check, screens (alignment).
- [ ] Commit.

### Task 3: Proof and docs
- [ ] The edited Hertha save loads in the native engine and runs.
- [ ] web/README.md; the research note's "As built". Commit and push.
