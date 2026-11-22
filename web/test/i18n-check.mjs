// The page's language (web/i18n.js, web/README.md "Languages") on the stand-in site (demo-site.mjs): ?lang= and the
// browser's language choose it, Help's Language choice changes it at once with no reload (an open dialog too), and the
// choice is kept. A stand-in German table is served in place of i18n-de.js, so the check does not hang on the wording.
//   NODE_PATH=$(npm root -g) node web/test/i18n-check.mjs
import { createRequire } from 'node:module';
import { startDemoSite } from './demo-site.mjs';

const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
let ok = true;
const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= Boolean(cond); };
const site = await startDemoSite({});
const browser = await chromium.launch();
const errors = [];
const GERMAN = { 'How to play here': 'So spielst du hier', 'Saved games': 'Gespeicherte Spiele' };
const page = async (opts = {}) => {
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 }, ...opts });
  await ctx.route('**/i18n-de.js', (r) => r.fulfill({ contentType: 'text/javascript', body: `export default ${JSON.stringify(GERMAN)};` }));
  await ctx.addInitScript(() => { try { localStorage.setItem('cm0102-tour-seen', '1'); } catch { /* none */ } });
  const p = await ctx.newPage();
  p.on('pageerror', (e) => errors.push(e.message));
  return { ctx, p };
};
const start = async (p) => {
  await p.waitForFunction(() => document.getElementById('go')?.offsetParent || window.__fake, null, { timeout: 30000 });
  if (await p.locator('#go').isVisible().catch(() => false)) await p.locator('#go').click();
  await p.waitForFunction(() => window.__fake, null, { timeout: 30000 });
};
try {
  {
    const { ctx, p } = await page();
    await p.goto(`${site.BASE}/?lang=de`);
    await start(p);
    await p.locator('#help-open').click();
    const help = await p.evaluate(() => ({ title: document.getElementById('help-title').textContent, lang: document.documentElement.lang,
      options: [...document.querySelectorAll('#help-lang option')].map((o) => o.value), chosen: document.getElementById('help-lang')?.value }));
    check('?lang=de: Help\'s title in German, the page\'s lang de', help.title === 'So spielst du hier' && help.lang === 'de', JSON.stringify(help));
    check('Help\'s Language choice: English and the nine of the game, the page\'s one chosen',
      help.options.join(',') === 'en,es,pt,it,fr,de,nl,da,no,sv' && help.chosen === 'de', JSON.stringify(help));
    await p.locator('#help-close').click();
    await p.evaluate(() => { window.__same = true; });
    await p.locator('#saves-open').click();
    const before = await p.textContent('#saves-title');
    await p.evaluate(() => { const s = document.getElementById('help-lang'); s.value = 'en'; s.dispatchEvent(new Event('change', { bubbles: true })); });
    await p.waitForTimeout(300);
    const after = await p.evaluate(() => ({ title: document.getElementById('saves-title').textContent, same: window.__same === true, lang: document.documentElement.lang,
      kept: localStorage.getItem('cm0102-lang') }));
    check('English chosen while Saved games is open: its title turns English at once, no reload, the choice kept',
      before === 'Gespeicherte Spiele' && after.title === 'Saved games' && after.same && after.lang === 'en' && after.kept === 'en', JSON.stringify({ before, ...after }));
    await ctx.close();
  }
  {
    const { ctx, p } = await page({ locale: 'de-AT' });
    await p.goto(site.BASE);
    await p.waitForSelector('#go');
    check('a browser in German (de-AT): the page in German before the game starts', await p.evaluate(() => document.documentElement.lang) === 'de');
    await ctx.close();
  }
  {
    const { ctx, p } = await page({ locale: 'ja-JP' });
    await p.goto(site.BASE);
    await p.waitForSelector('#go');
    check('a browser in a language the game does not speak (Japanese): English', await p.evaluate(() => document.documentElement.lang) === 'en');
    await ctx.close();
  }
  check('no page errors', errors.length === 0, errors.join(' | '));
} finally {
  await browser.close();
  await site.close();
}
console.log(ok ? 'i18n check: all ok' : 'i18n check: FAILED');
process.exit(ok ? 0 : 1);
