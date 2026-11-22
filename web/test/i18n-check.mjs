// The page's language (web/i18n.js, web/README.md "Languages") on the stand-in site (demo-site.mjs): ?lang= and the
// browser's language choose it, Help's Language choice changes it at once with no reload (an open dialog too), and the
// choice is kept. A stand-in German table is served in place of i18n-de.js, so the check does not hang on the wording.
//   NODE_PATH=$(npm root -g) node web/test/i18n-check.mjs
import { createRequire } from 'node:module';
import { startDemoSite } from './demo-site.mjs';
import { fakeSaveGame } from './fakes.mjs';

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
  {
    // The sweep: a German table that is the English between "DE:" and ":ED", so any text a player reads outside the
    // two is English the page did not put through t, tn or data-t. A sentence with a link or bold in it is one text:
    // its pieces after the link are inside its marks. Save names, sizes, dates, the legal pages' names (English, as
    // the pages are) and the brand are the allowed exceptions; the library's entries are the server's (data-raw).
    const { pageStrings, pagePlurals } = await import('./i18n-strings.mjs');
    const sweep = {}, plurals = pagePlurals();
    for (const s of pageStrings().keys()) sweep[s] = plurals.has(s) ? { one: `DE:${s}:ED`, other: `DE:${plurals.get(s)}:ED` } : `DE:${s}:ED`;
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
    await ctx.route('**/i18n-de.js', (r) => r.fulfill({ contentType: 'text/javascript', body: `export default ${JSON.stringify(sweep)};` }));
    // Every toast as it shows (they go after a few seconds): the saved one, the cloud hint after it.
    await ctx.addInitScript(() => {
      window.__toasts = [];
      new MutationObserver((list) => { for (const m of list) for (const n of m.addedNodes) if (n.classList?.contains('toast')) window.__toasts.push(n.textContent); })
        .observe(document, { childList: true, subtree: true });
    });
    const p = await ctx.newPage();
    p.on('pageerror', (e) => errors.push(e.message));
    const english = () => p.evaluate(() => {
      const ALLOW = /^([\d\s.,:/·%()+×–—≥-]+(MB|KB|GB|s|min|h)?$|CM 01\/02$|Pricing$|Terms$|Refunds$|Privacy$|.*\.(sav|tct|zip)$|Made with$|by .+$|×$|›$|English$|Español$|Português$|Italiano$|Français$|Deutsch$|Nederlands$|Dansk$|Norsk$|Svenska$)/;
      const out = [];
      const visible = (el) => el && el.getClientRects().length > 0 && getComputedStyle(el).visibility !== 'hidden';
      // The pieces of a text that are outside every text's marks. depth: how many texts are open where it starts.
      const outside = (s, depth = 0) => {
        const out = [];
        let piece = '';
        for (const part of s.split(/(DE:|:ED)/)) {
          if (part === 'DE:') { if (!depth) { out.push(piece); piece = ''; } depth++; } else if (part === ':ED') depth = Math.max(0, depth - 1);
          else if (!depth) piece += part;
        }
        out.push(piece);
        return out.map((x) => x.trim().replace(/\s+/g, ' ')).filter(Boolean);
      };
      // How many texts are open where a text node starts, within its block (a paragraph, a cell, a button...).
      const depthAt = (n) => {
        const block = n.parentElement.closest('p,li,td,th,h1,h2,h3,h4,label,button,summary,legend,dt,dd,div') ?? document.body;
        let d = 0;
        const w = document.createTreeWalker(block, NodeFilter.SHOW_TEXT);
        for (let m = w.nextNode(); m && m !== n; m = w.nextNode()) for (const k of m.textContent.match(/DE:|:ED/g) ?? []) d = k === 'DE:' ? d + 1 : Math.max(0, d - 1);
        return d;
      };
      const walk = (root) => {
        const w = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
        for (let n = w.nextNode(); n; n = w.nextNode()) {
          if (!n.textContent.trim() || !visible(n.parentElement) || n.parentElement.closest('script,style,#log,.raw,[data-raw]')) continue;
          for (const s of outside(n.textContent, depthAt(n))) if (!ALLOW.test(s)) out.push(s.slice(0, 80));
        }
        for (const el of root.querySelectorAll('[title],[aria-label],[placeholder]')) {
          if (!visible(el)) continue;
          for (const a of ['title', 'aria-label', 'placeholder']) { const v = el.getAttribute(a); for (const x of outside(v ?? '')) if (!ALLOW.test(x)) out.push(`@${a}: ${x.slice(0, 60)}`); }
        }
      };
      walk(document.body);
      for (const s of window.__toasts ?? []) for (const x of outside(s)) if (!ALLOW.test(x)) out.push(`toast: ${x.slice(0, 80)}`);
      return [...new Set(out)];
    });
    await p.goto(`${site.BASE}/?lang=de`);
    await p.waitForSelector('#go');
    const startBox = await english();
    await start(p);
    await p.waitForSelector('#tour', { timeout: 15000 }).catch(() => {});
    const tour = await english();
    await p.locator('#tour-skip').click().catch(() => {});
    await p.locator('#saves-open').click();
    await p.setInputFiles('#saves-file', { name: 'junk.sav', mimeType: 'application/octet-stream', buffer: Buffer.from('hello') });
    // The refusal is in Saved games' note (the sweep of Saved games reads it), not a toast.
    await p.waitForFunction(() => /junk\.sav/.test(document.getElementById('saves-note').textContent) && window.__toasts.length >= 2, null, { timeout: 15000 }).catch(() => {});
    const toastsSeen = await p.evaluate(() => window.__toasts.length), refusal = await p.textContent('#saves-note');
    const saves = await english();
    for (const tab of ['tactics', 'sets']) { await p.locator(`#saves .tabs [data-tab="${tab}"]`).click(); await p.waitForTimeout(300); }
    const tabs = await english();
    await p.locator('#saves-close').click();
    await p.locator('#account-open').click();
    const account = await english();
    await p.locator('#account-close').click();
    await p.locator('#help-open').click();
    const help = await english();
    // The Editor's Scout as a free player: its filters, 3 matches and the invitation.
    await p.locator('#help-close').click();
    const scoutSave = await fakeSaveGame(['Aurelio', 'Benedikt', 'Cassiano', 'Dorotheus', 'Eustachio', 'Florentin'].map((first, i) =>
      ({ first, second: 'Scoutson', ca: 120 + i, pa: 190 - i, gk: 1, born: [1981, 1, 1], club: 0, fill: 6 + i })));
    await p.evaluate((b) => window.Module.FS.writeFile('/user/Scouting 2001-07-29.sav', new Uint8Array(b)), Array.from(scoutSave));
    await p.locator('#saves-open').click();
    await p.locator('#saves .tabs [data-tab="saves"]').click();
    await p.locator('#saves-list tr:has-text("Scouting 2001-07-29.sav") button[data-edit]').click();
    await p.waitForSelector('#editor[open] .ed-row');
    await p.locator('#editor-tabs [data-pane="scout"]').click();
    await p.waitForSelector('#scout-invite', { timeout: 15000 }).catch(() => {});
    const scout = await english();
    const invited = await p.isVisible('#scout-invite');
    const log = await p.evaluate(() => document.getElementById('log').textContent);
    const left = { startBox, tour, saves, tabs, account, help, scout };
    const n = Object.values(left).reduce((a, x) => a + x.length, 0);
    check('the page in a language: no English left in the start box, the bar, the tour, Saved games, Account, Help, the toasts, a refused upload and the Scout', n === 0,
      Object.entries(left).filter(([, x]) => x.length).map(([k, x]) => `${k}: ${x.slice(0, 12).join(' | ')}`).join(' || '));
    check('the toasts and the refusal were seen: saved, the cloud hint, and junk.sav refused in Saved games', toastsSeen >= 2 && /junk\.sav/.test(refusal),
      `${await p.evaluate(() => window.__toasts.join(' | '))} || ${refusal}`);
    check('the Scout was swept as a free player (its invitation showed)', invited);
    check('the engine log stays English', !/DE:/.test(log));
    await ctx.close();
  }
  {
    // A server with no database library: the Databases tab's steps in the page's language too (its first step is
    // written by the library's list when there is one).
    const bare = await startDemoSite({ withLibrary: false });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
    await ctx.addInitScript(() => { try { localStorage.setItem('cm0102-tour-seen', '1'); } catch { /* none */ } });
    const p = await ctx.newPage();
    p.on('pageerror', (e) => errors.push(e.message));
    await p.goto(`${bare.BASE}/?lang=de`);
    await start(p);
    await p.locator('#saves-open').click();
    await p.locator('#saves .tabs [data-tab="sets"]').click();
    const step = await p.textContent('#sets-step1');
    check('no library on the server: the Databases tab\'s first step in German', !/Upload a database|make one from the game/.test(step), step);
    await ctx.close();
    await bare.close();
  }
  check('no page errors', errors.length === 0, errors.join(' | '));
} finally {
  await browser.close();
  await site.close();
}
console.log(ok ? 'i18n check: all ok' : 'i18n check: FAILED');
process.exit(ok ? 0 : 1);
