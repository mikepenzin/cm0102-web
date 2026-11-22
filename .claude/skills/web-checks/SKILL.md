---
name: web-checks
description: Which checks to run for a change to the browser page and server (web/), how to run them, how long they take, and the traps they hide. Use before pushing web/ changes, when CI ("Web", .github/workflows/web.yml) fails, for "run the tests", "page-check", "cloud-check", "editor-check", "scout-check", "tour-check", "i18n-check", "screens", "Playwright", or a red run on main.
---

# Checks of the page (web/)

CI (.github/workflows/web.yml) runs, in order: `cd web && npm test`, vault-check.cjs, page-check.cjs, editor-check.mjs,
scout-check.mjs, cloud-check.mjs, tour-check.mjs, i18n-check.mjs. Push to main only when the ones a change touches pass
here; run the whole list before a push that touches shared code or wording. web/README.md, "Tests", says what each one
covers.

Run each browser check as `NODE_PATH=$(npm root -g) node web/test/<name>`, its output into a file, and read the `FAIL`
lines and the last line ("… check: all ok").

| Check | Time | Run it for |
|---|---|---|
| `npm test` (in web/) | ~10 s | everything; includes the translation tables (stale entries and placeholders fail) |
| vault-check.cjs | ~1 min | vault.js, saves in the browser's storage |
| page-check.cjs | ~10 min | loader.js, the start, Saved games, stats.js, report.js, the bar |
| editor-check.mjs | ~2 min | Squad Hub (editor.js, savedit.js, dataset.js), data sets |
| scout-check.mjs | ~2 min | the Scout (scout.js, editor.js), the Membership gate |
| cloud-check.mjs | ~4 min | account.js, cloud.mjs, billing.mjs, payments, wording of the plans |
| tour-check.mjs | ~1 min | tour.js, the bar's buttons |
| i18n-check.mjs | ~1 min | any text a player reads (see the web-languages skill) |
| screens.mjs `<folder> [code]` | ~2 min | the look; with a language it fails on a cut or off-window label |

Unit tests alone: `ls web/test/*.test.js | xargs node --test`.

## Traps

- **Exact English in checks.** cloud-check, tour-check, scout-check and billing.test.js wait for exact sentences;
  reword a text and grep `web/test/` for its old words. (Runs #128 and #129 failed on a reworded membership note.)
- **A second visit starts at once.** The start box shows only while game files are to be fetched; a check that opens a
  second page in the same browser context waits for `#go` *or* `window.__fake` (see scout-check's `editorOn`).
- **Keys.** keyboard.js's guard stops the page's keys at the window before an element's own listener; a part of the
  page that wants a key registers through `onPageKey` (the tour, Squad Hub's tabs).
- **Playwright and the page's account.** `/api/me` can be played by `ctx.route` (scout-check); a change of account
  reaches the page through `new BroadcastChannel('cm-account').postMessage({ ok: true })`.
- **The stand-in site** (test/demo-site.mjs) has accounts, a Buy Me a Coffee offer and a library; `startDemoSite({
  withLibrary: false })` for a server without one.
- **Screenshots** for a person: deviceScaleFactor 2; `screens.mjs <folder> <code>` shoots every screen in a language
  on a laptop, a phone upright and sideways.
- **CI's Node.js 20 warning** (actions/checkout@v4, setup-node@v4) is a notice, not the failure: read the `FAIL` line.
- A red run's log: the GitHub tools (`actions_list` with `list_workflow_runs` for web.yml, then `get_job_logs` with
  `run_id` and `failed_only`).
