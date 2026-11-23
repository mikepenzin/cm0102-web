// The browser check of levels and early access (web/README.md, "Levels and early access"). The admin page: an
// account's level in People (paid, a gift, both), Give Club Legend and Take back, the level chips, the Members tab's
// level and an unknown level, the Early access tab (a feature and an early database, Open now), the library form's day.
// The game page: an early database locked for a free player and open for a Club Legend, with the badge, unlocked
// without a reload when the player becomes one, and the account dialog's level.
// Needs Playwright: NODE_PATH=$(npm root -g) node web/test/early-check.mjs [screenshot folder]
import { createRequire } from 'node:module';
import { mkdirSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { startDemoSite } from './demo-site.mjs';
import { coffeeEvent, signCoffee } from './fakes.mjs';
import { addDays, todayUTC } from '../early.mjs';

const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
const SHOTS = process.argv[2] ? resolve(process.argv[2]) : null;
const shot = async (page, name) => { if (SHOTS) { mkdirSync(SHOTS, { recursive: true }); await page.screenshot({ path: join(SHOTS, `early-${name}.png`), fullPage: true }); } };

const TOKEN = 'an-admin-token-of-16-chars-x';
const site = await startDemoSite({ adminToken: TOKEN, features: [{ key: 'demo-feature', name: 'Demo feature' }] });
const { BASE, google } = site;
let ok = true;
const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= Boolean(cond); };
const auth = { authorization: `Bearer ${TOKEN}` };
const hook = async (e) => { const raw = JSON.stringify(e); return (await fetch(`${BASE}/api/coffee/webhook`, { method: 'POST', body: raw, headers: { 'x-signature-sha256': signCoffee(raw, 'video') } })).status; };

// An account: signed in through the stand-in of Google, then a visit counted, so People lists it. The session cookie.
async function account(sub, email) {
  google.user = { sub, email, email_verified: true, name: email.split('@')[0] };
  const start = await fetch(`${BASE}/auth/google/start`, { redirect: 'manual' });
  const oauth = start.headers.getSetCookie()[0].split(';')[0];
  const atGoogle = await fetch(start.headers.get('location'), { redirect: 'manual' });
  const back = await fetch(atGoogle.headers.get('location'), { redirect: 'manual', headers: { cookie: oauth } });
  const cookie = back.headers.getSetCookie().filter((c) => c.startsWith('cm_session=')).map((c) => c.split(';')[0]).join('; ');
  const anon = `${sub.padStart(8, '0').slice(-8)}-2222-4333-8444-555555555555`;
  await fetch(`${BASE}/api/stats/beat`, { method: 'POST', body: JSON.stringify({ anon, session: 's', seconds: 60 }), headers: { 'content-type': 'application/json', 'x-cm': '1', cookie } });
  return cookie;
}

const t = Math.floor(Date.now() / 1000);
await hook(coffeeEvent('membership.started', { email: 'squad@example.com', id: 801, created: t }));
await hook(coffeeEvent('membership.started', { email: 'gold@example.com', id: 802, level: 'Gold', levelId: 9, created: t }));
const squadCookie = await account('7100001', 'squad@example.com');
await account('7100002', 'free@example.com');
// A database early for three days (the demo library's newest), as the admin page would set it.
const lib = await (await fetch(`${BASE}/api/admin/library`, { headers: auth })).json();
const early = lib.entries[0], DAY = addDays(todayUTC(), 3);
await fetch(`${BASE}/api/admin/library/${early.id}`, { method: 'PATCH', body: JSON.stringify({ early: DAY }), headers: { ...auth, 'content-type': 'application/json' } });

const browser = await chromium.launch();
try {
  // ---- The admin page ----
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 900 }, extraHTTPHeaders: auth });
  const a = await ctx.newPage();
  a.on('pageerror', (e) => check('no page error on the admin page', false, e.message));
  a.on('dialog', (d) => d.accept());
  await a.goto(`${BASE}/admin#people`);
  await a.waitForSelector('#people tr.person');
  const rowOf = (email) => a.locator('#people tr.person', { hasText: email });
  const planText = async (email) => (await rowOf(email).locator('.plan').textContent()).replace(/\s+/g, ' ').trim();
  check('People: a paid Squad Player', /Squad Player · paid/.test(await planText('squad@example.com')), await planText('squad@example.com'));
  check('People: a free account', /^Free/.test(await planText('free@example.com')), await planText('free@example.com'));
  await rowOf('squad@example.com').locator('[data-give-level="legend"]').click();
  await a.waitForFunction(() => /Club Legend · gift/.test(document.getElementById('people').textContent));
  const both = await planText('squad@example.com');
  check('Give Club Legend: the gift and the paid level both show', /Club Legend · gift/.test(both) && /Squad Player · paid/.test(both), both);
  const chip = (await a.locator('#kinds [data-kind="legend"]').textContent()).replace(/\s+/g, ' ');
  check('the Club Legend chip counts it', /Club Legend\s*1/.test(chip), chip);
  await shot(a, 'people');
  await rowOf('squad@example.com').locator('[data-take-back]').click();
  await a.waitForFunction(() => !/Club Legend · gift/.test(document.getElementById('people').textContent));
  check('Take back: the paid level stays', /^Squad Player · paid/.test(await planText('squad@example.com')), await planText('squad@example.com'));
  // Members: each membership's level, an unknown one flagged.
  await a.goto(`${BASE}/admin#members`);
  await a.waitForSelector('#view-members table');
  const members = (await a.textContent('#view-members')).replace(/\s+/g, ' ');
  check('Members: the level a membership counts as', /squad@example\.com.*Squad Player/.test(members), members.slice(0, 300));
  check('Members: an unknown level is flagged', /not in CM_LEVELS/.test(members));
  // Early access: the feature and the early database; Open now.
  await a.goto(`${BASE}/admin#early`);
  await a.waitForSelector('#view-early [data-early-row]');
  const early1 = `${(await a.textContent('#view-early')).replace(/\s+/g, ' ')} ${(await a.locator('#view-early input').evaluateAll((xs) => xs.map((x) => x.value))).join(' ')}`;
  check('Early access: the feature with its 14 days', early1.includes('Demo feature') && early1.includes(addDays(todayUTC(), 14)), early1.slice(0, 300));
  check('Early access: the early database with its day', early1.includes(early.name) && early1.includes(DAY));
  await shot(a, 'tab');
  await a.locator(`[data-early-row="db:${early.id}"] [data-open-now]`).click();
  await a.waitForFunction((id) => document.querySelector(`[data-early-row="db:${id}"]`)?.closest('[data-open-list]'), early.id);
  check('Open now: the database moves to the open list', true);
  await fetch(`${BASE}/api/admin/library/${early.id}`, { method: 'PATCH', body: JSON.stringify({ early: DAY }), headers: { ...auth, 'content-type': 'application/json' } });
  // The library form: a new database is early for 14 days unless the field is cleared.
  await a.goto(`${BASE}/admin#library`);
  await a.waitForSelector('#lib-early');
  check('the library form: 14 days by default', await a.inputValue('#lib-early') === addDays(todayUTC(), 14), await a.inputValue('#lib-early'));
  // Overview: the members card split by level.
  await a.goto(`${BASE}/admin#overview`);
  await a.waitForSelector('#view-overview .cards');
  const cards = (await a.textContent('#view-overview .cards')).replace(/\s+/g, ' ');
  check('Overview: the members card by level', /1 Squad Player/.test(cards), cards.slice(0, 400));
  await ctx.close();

  // ---- The game page: an early database (the demo library's newest, early for three days). ----
  google.user = { sub: '7100002', email: 'free@example.com', email_verified: true, name: 'Free' };
  const pctx = await browser.newContext({ viewport: { width: 1200, height: 800 } });
  const p = await pctx.newPage();
  p.on('pageerror', (e) => check('no page error on the game page', false, e.message));
  await p.goto(`${BASE}/auth/google/start?mode=page`);
  await p.waitForSelector('#go');
  await p.click('#go');
  await p.waitForFunction(() => window.__fake, null, { timeout: 30000 });
  const openSets = async () => {
    await p.click('#saves-open');
    await p.click('#saves .tabs [data-tab="sets"]');
    await p.waitForSelector('#sets-lib tr + tr', { timeout: 20000 });
  };
  await openSets();
  const lockedRow = p.locator('#sets-lib tr', { hasText: early.name });
  const locked = (await lockedRow.textContent()).replace(/\s+/g, ' ');
  check('a free player: the early database shows, with its day, and no Get', /Club Legends have it now · everyone from/.test(locked)
    && /Become a Club Legend/.test(locked) && await lockedRow.locator('button[data-lib-get]').count() === 0, locked);
  check('the other databases are as before', await p.locator('#sets-lib button[data-lib-get]').count() === 2);
  await shot(p, 'locked');
  // Becomes a Club Legend while the dialog is open (a gift): the row unlocks without a reload.
  await fetch(`${BASE}/api/admin/cloud`, { method: 'POST', body: JSON.stringify({ id: 'g:7100002', level: 'legend' }), headers: { ...auth, 'content-type': 'application/json', 'x-cm': '1' } });
  await p.evaluate(() => new BroadcastChannel('cm-account').postMessage({ ok: true }));
  await p.waitForSelector(`#sets-lib tr:has-text("${early.name}") button[data-lib-get]`, { timeout: 20000 });
  const open = (await lockedRow.textContent()).replace(/\s+/g, ' ');
  check('a Club Legend: Get, and the Early access badge', /Early access/.test(open) && !/everyone from/.test(open), open);
  await shot(p, 'legend');
  await lockedRow.locator('button[data-lib-get]').click();
  await p.waitForFunction((name) => new RegExp(`${name} is here`).test(document.getElementById('sets-note').textContent), early.name, { timeout: 30000 });
  check('a Club Legend gets the early database', true);
  await p.click('#saves-close').catch(() => p.keyboard.press('Escape'));
  await p.click('#account-open');
  const dialog = (await p.innerText('#account-body')).replace(/\s+/g, ' ');
  check('the account dialog names the level', /Club Legend: early access/.test(dialog), dialog.slice(0, 200));
  await pctx.close();
} finally {
  await browser.close();
  await site.close();
}
console.log(ok ? 'early check: all ok' : 'early check: FAILED');
process.exit(ok ? 0 : 1);
