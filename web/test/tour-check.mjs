// The first-visit tour (web/tour.js, web/README.md "The first-visit tour") on the stand-in site of the videos
// (demo-site.mjs): a laptop, a phone held upright and one held sideways. Each card is on the screen and beside its
// button, not over it; the steps are the page's buttons that show (Right click only on a touch device); the tour shows
// once, not again after a reload (which also starts the game from the browser's copy of its files, empty ones too);
// Help's "Show the tour" runs it again, and Escape ends it.
//   NODE_PATH=$(npm root -g) node web/test/tour-check.mjs [folder for screenshots]
import { createRequire } from 'node:module';
import { mkdirSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { startDemoSite } from './demo-site.mjs';

const require = createRequire(import.meta.url);
const { chromium, devices } = require('playwright');
const SHOTS = process.argv[2] ? resolve(process.argv[2]) : null;
if (SHOTS) mkdirSync(SHOTS, { recursive: true });
let ok = true;
const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= Boolean(cond); };
const site = await startDemoSite({});
const browser = await chromium.launch();
const errors = [];
const start = async (p) => {
  await p.waitForFunction(() => document.getElementById('go')?.offsetParent || window.__fake, null, { timeout: 30000 });
  if (await p.locator('#go').isVisible().catch(() => false)) await p.locator('#go').click();
  await p.waitForFunction(() => window.__fake, null, { timeout: 30000 });
};
const where = (p) => p.evaluate(() => {
  const c = document.getElementById('tour').getBoundingClientRect(), t = document.querySelector('.tour-target');
  const r = t.getBoundingClientRect();
  const covers = (x) => !(c.right <= x.left || c.left >= x.right || c.bottom <= x.top || c.top >= x.bottom);
  const buttons = [...document.querySelectorAll('#bar > button, #bar > a')].filter((e) => e.offsetParent !== null).map((e) => e.getBoundingClientRect());
  return { title: document.getElementById('tour-title').textContent, target: t.id,
    over: buttons.some(covers),
    inside: c.left >= 0 && c.top >= 0 && c.right <= innerWidth && c.bottom <= innerHeight };
});

try {
  for (const [name, opts, want] of [
    ['laptop', { viewport: { width: 1280, height: 800 } }, ['Saved games', 'Editor', 'Help']],
    ['phone upright', { ...devices['iPhone 13'] }, ['Saved games', 'Editor', 'Right click', 'Help']],
    ['phone sideways', { ...devices['iPhone 14 Pro Max landscape'] }, ['Saved games', 'Editor', 'Right click', 'Help']]]) {
    const ctx = await browser.newContext(opts);
    const p = await ctx.newPage();
    p.on('pageerror', (e) => errors.push(`${name}: ${e.message}`));
    await p.goto(site.BASE);
    await start(p);
    await p.waitForSelector('#tour', { timeout: 15000 });
    const steps = [];
    for (let i = 0; i < 8; i++) {
      await p.waitForTimeout(250);
      steps.push(await where(p));
      if (SHOTS) await p.screenshot({ path: join(SHOTS, `tour-${name.replace(' ', '-')}-${i + 1}.png`) });
      const next = p.locator('#tour-next');
      const done = (await next.textContent()) === 'Done';
      await next.click();
      if (done) break;
    }
    check(`${name}: the steps are the buttons that show, in order`, JSON.stringify(steps.map((s) => s.title)) === JSON.stringify(want), steps.map((s) => s.title).join(', '));
    check(`${name}: each card is on the screen, beside its button, over none of the page's buttons`, steps.every((s) => s.inside && !s.over),
      steps.filter((s) => !s.inside || s.over).map((s) => `${s.title}${s.over ? ' over a button' : ''}${s.inside ? '' : ' off the screen'}`).join(', '));
    const after = await p.evaluate(() => ({ card: Boolean(document.getElementById('tour')), ring: document.querySelectorAll('.tour-target').length,
      seen: localStorage.getItem('cm0102-tour-seen'), focus: document.activeElement?.id }));
    check(`${name}: Done ends it, takes the ring off, gives the game the focus and marks it seen`, !after.card && after.ring === 0 && after.seen === '1'
      && after.focus === 'canvas', JSON.stringify(after));
    await p.reload();
    await start(p);
    await p.waitForTimeout(2500);
    check(`${name}: a later visit starts the game (from the browser's copy) and shows no tour`, (await p.locator('#tour').count()) === 0);
    await p.locator('#help-open').click();
    await p.locator('#help-tour').click();
    await p.waitForSelector('#tour', { timeout: 3000 }).catch(() => {});
    const fromHelp = await p.evaluate(() => ({ card: Boolean(document.getElementById('tour')), help: document.getElementById('help').open,
      title: document.getElementById('tour-title')?.textContent }));
    check(`${name}: Help's Show the tour closes Help and runs it from the first step`, fromHelp.card && !fromHelp.help && fromHelp.title === 'Saved games', JSON.stringify(fromHelp));
    await p.keyboard.press('Escape');
    check(`${name}: Escape ends it`, (await p.locator('#tour').count()) === 0);
    await ctx.close();
  }
  // A private window, where the browser keeps nothing: the tour shows, and ending it throws nothing.
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
  await ctx.addInitScript(() => { Object.defineProperty(window, 'localStorage', { get() { throw new DOMException('denied', 'SecurityError'); } }); });
  const p = await ctx.newPage();
  p.on('pageerror', (e) => errors.push(`no storage: ${e.message}`));
  await p.goto(site.BASE);
  await start(p);
  const shown = await p.waitForSelector('#tour', { timeout: 15000 }).then(() => true, () => false);
  if (shown) await p.locator('#tour-skip').click();
  check('no storage (a private window): the tour shows and Skip ends it', shown && (await p.locator('#tour').count()) === 0);
  await ctx.close();
  check('no page errors', errors.length === 0, errors.join(' | '));
} finally {
  await browser.close();
  await site.close();
}
console.log(ok ? 'tour check: all ok' : 'tour check: FAILED');
process.exit(ok ? 0 : 1);
