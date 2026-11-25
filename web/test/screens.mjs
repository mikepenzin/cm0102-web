// Every screen of the page, on a laptop and on a phone, for a review of the look (web/README.md, "Look and feel"):
// the start box, the bar over the game, Saved games and its three tabs, the editor, the account, Help, Send to the
// developer, the tour, the legal pages and the admin page. The stand-in site (demo-site.mjs). With a language
// (web/README.md, "Languages"), the page in it; and on every screen of the page, a label, tab, dialog button or the
// tour card whose text is cut or that leaves the window is listed, and the run fails.
// NODE_PATH=$(npm root -g) node web/test/screens.mjs <folder> [de]  → <folder>/<laptop|phone|sideways>-NN-<screen>.png
import { createRequire } from 'node:module';
import { mkdirSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { fakeSaveGame } from './fakes.mjs';
import { startDemoSite } from './demo-site.mjs';

const require = createRequire(import.meta.url);
const { chromium, devices } = require('playwright');
const OUT = resolve(process.argv[2] ?? 'screens');
const LANG = process.argv[3] ?? '';
mkdirSync(OUT, { recursive: true });
const TOKEN = 's'.repeat(24);
const site = await startDemoSite({ adminToken: TOKEN });
const { BASE } = site;
const browser = await chromium.launch();
// Six players: the Scout of a free player shows 3 and the invitation.
const save = await fakeSaveGame(['Alessandro', 'Gianluigi', 'Pavel', 'Filippo', 'Edgar', 'Lilian'].map((first, i) =>
  ({ first, second: ['Del Piero', 'Buffon', 'Nedved', 'Inzaghi', 'Davids', 'Thuram'][i], ca: 150 + i, pa: 190 - i, gk: i === 1 ? 20 : 1, born: [1975 + i, 1, 1], club: 0, fill: 10 + i })),
  { managers: [{ first: 'Klaus', second: 'Kastelmann' }, { first: 'Mike', second: 'Penzin' }] });   // the Scout's stars and "Shortlist for"
const errors = [], cut = [];
// The labels whose text does not fit, and the ones out of the window (left or right).
const SEL = '#bar > button, #bar > a, #bar > button span, #bar > a span, #status, .tabs button, dialog[open] .row button, dialog[open] .row a.button, dialog[open] h2, #tour, #tour button, #type-chip';
const notFitting = (sel) => [...document.querySelectorAll(sel)].filter((el) => el.getClientRects().length && getComputedStyle(el).visibility !== 'hidden').flatMap((el) => {
  const r = el.getBoundingClientRect(), name = `${el.id ? `#${el.id}` : el.tagName.toLowerCase()} "${el.textContent.trim().replace(/\s+/g, ' ').slice(0, 40)}"`;
  const out = [];
  // A bar button's box is its icon's when the label is hidden (a phone): its label, the span, is checked for text.
  if (!el.matches('#bar > button, #bar > a') && el.scrollWidth > el.clientWidth + 1 && !/auto|scroll/.test(getComputedStyle(el).overflowX)) out.push(`${name}: text cut (${el.scrollWidth} > ${el.clientWidth})`);
  if (r.left < -1 || r.right > innerWidth + 1) out.push(`${name}: out of the window (${Math.round(r.left)}..${Math.round(r.right)} of ${innerWidth})`);
  return out;
});

// The look's alignment: a button's, chip's or badge's text in the middle of its box (across when the box is wider than
// its text, always up and down), and the items of one line of a row level with each other (their middles). Rows of a list
// (.ed-row and the like, whose text starts at the left on purpose) are left out.
const misaligned = () => {
  const shown = (el) => el.getClientRects().length && getComputedStyle(el).visibility !== 'hidden' && el.getBoundingClientRect().width > 0;
  const name = (el) => `${el.tagName.toLowerCase()}${el.id ? `#${el.id}` : ''}${el.classList.length ? `.${[...el.classList].join('.')}` : ''} "${el.textContent.trim().replace(/\s+/g, ' ').slice(0, 30)}"`;
  const out = [];
  const textBox = (el) => { const r = document.createRange(); r.selectNodeContents(el); const rs = [...r.getClientRects()].filter((x) => x.width > 0); if (!rs.length) return null;
    return { left: Math.min(...rs.map((x) => x.left)), right: Math.max(...rs.map((x) => x.right)), top: Math.min(...rs.map((x) => x.top)), bottom: Math.max(...rs.map((x) => x.bottom)) }; };
  for (const el of document.querySelectorAll('button, a.button, .badge, .chip, [role="tab"]')) {
    if (!shown(el) || el.closest('.ed-row, [data-row], #bar') && !el.matches('#bar > *') ) continue;
    const cs = getComputedStyle(el);
    if (cs.textAlign === 'left' && !/flex/.test(cs.display) || /grid/.test(cs.display)) continue;
    const b = el.getBoundingClientRect(), t = textBox(el);
    if (!t) continue;
    const pl = parseFloat(cs.paddingLeft) + parseFloat(cs.borderLeftWidth), pr = parseFloat(cs.paddingRight) + parseFloat(cs.borderRightWidth);
    const pt = parseFloat(cs.paddingTop) + parseFloat(cs.borderTopWidth), pb = parseFloat(cs.paddingBottom) + parseFloat(cs.borderBottomWidth);
    const dx = (t.left + t.right) / 2 - (b.left + pl + b.right - pr) / 2, dy = (t.top + t.bottom) / 2 - (b.top + pt + b.bottom - pb) / 2;
    if (Math.abs(dx) > 2 && (b.width - pl - pr) - (t.right - t.left) > 4) out.push(`${name(el)}: text ${Math.round(dx)} px off the middle across`);
    if (Math.abs(dy) > 2.5) out.push(`${name(el)}: text ${Math.round(dy)} px off the middle up and down`);
  }
  for (const row of document.querySelectorAll('*')) {
    const cs = getComputedStyle(row);
    const grid = /grid/.test(cs.display) && cs.gridTemplateColumns.split(' ').length > 1;
    if (!shown(row) || !(grid || /flex/.test(cs.display) && cs.flexDirection === 'row') || /baseline|start|end/.test(cs.alignItems)) continue;
    // The items of a line: small ones (not the page's panes), with text or a control, one grid row each; a line is the
    // items that overlap up and down.
    const kids = [...row.children].filter((k) => shown(k) && getComputedStyle(k).position !== 'absolute' && k.tagName !== 'DIALOG'
      && !/start|end|baseline/.test(getComputedStyle(k).alignSelf) && k.getBoundingClientRect().height < 120
      && !/span [2-9]/.test(getComputedStyle(k).gridRowEnd) && (k.textContent.trim() || k.querySelector('input, select, button, textarea') || k.matches('input, select, button, textarea')));
    const lines = [];
    for (const k of kids) {
      const r = k.getBoundingClientRect(), line = lines.find((l) => { const q = l[0].getBoundingClientRect(); return r.top < q.bottom - 2 && r.bottom > q.top + 2; });
      if (line) line.push(k); else lines.push([k]);
    }
    for (const line of lines) {
      if (line.length < 2) continue;
      // An item that holds controls is measured by its first one (a label is level with the box beside it).
      const anchor = (k) => (k.matches('input, select, button, textarea') ? k : [...k.querySelectorAll('input, select, button, textarea')].find(shown) ?? k);
      const mids = line.map((k) => { const r = anchor(k).getBoundingClientRect(); return (r.top + r.bottom) / 2; });
      if (Math.max(...mids) - Math.min(...mids) > 3) out.push(`${name(row)}: its items not level (${Math.round(Math.max(...mids) - Math.min(...mids))} px): ${line.map(name).join(' | ')}`);
    }
  }
  return out;
};
const crooked = [];

async function walk(kind, options) {
  const ctx = await browser.newContext(options);
  const p = await ctx.newPage();
  p.on('pageerror', (e) => errors.push(`${kind}: ${e.message}`));
  p.on('dialog', (d) => d.accept());
  let n = 0;
  const shot = async (name, fit = true) => {
    await p.waitForTimeout(350);
    await p.screenshot({ path: join(OUT, `${kind}-${String(++n).padStart(2, '0')}-${name}.png`) });
    if (fit) for (const c of await p.evaluate(notFitting, SEL)) cut.push(`${kind} ${name}: ${c}`);
    for (const c of await p.evaluate(misaligned)) crooked.push(`${kind} ${name}: ${c}`);
  };
  const tap = (sel) => p.locator(sel).first().click();
  await p.goto(LANG ? `${BASE}/?lang=${LANG}` : BASE);
  await p.waitForSelector('#go');
  await shot('start');
  await tap('#go');
  await p.waitForFunction(() => window.__fake, null, { timeout: 30000 });
  if (await p.waitForSelector('#tour', { timeout: 15000 }).catch(() => null)) {
    await shot('tour');
    for (let k = 0; k < 8 && await p.isVisible('#tour'); k++) {         // each step's card, the last one shot
      if (!(await p.locator('#tour-next').isVisible())) break;
      for (const c of await p.evaluate(notFitting, SEL)) cut.push(`${kind} tour ${k + 1}: ${c}`);
      await p.locator('#tour-next').click();
      await p.waitForTimeout(150);
    }
  }
  await p.evaluate((bytes) => {
    const FS = window.Module.FS, b = new Uint8Array(bytes);
    FS.writeFile('/user/Juventus 2001-07-29.sav', b);
    FS.writeFile('/user/A very long name of a career save that a player gave it 2003-04-19.sav', b);
    try { FS.mkdir('/user/tactics'); } catch { /* there */ }
    FS.writeFile('/user/tactics/Attacking 4-3-3.tct', new Uint8Array(3000).fill(1));
  }, Array.from(save));
  await shot('game');
  await tap('#saves-open');
  await p.waitForSelector('#saves-list button[data-play]');
  await shot('saves');
  await tap('#saves .tabs [data-tab="tactics"]');
  await p.waitForSelector('#tactics-list button[data-tactic]');
  await shot('tactics');
  await tap('#saves .tabs [data-tab="sets"]');
  await p.waitForSelector('#sets-lib button[data-lib-get]', { timeout: 20000 });
  await shot('databases');
  await tap('#sets-lib button[data-lib-get]');
  await p.waitForSelector('#sets-season', { timeout: 30000 });
  await shot('databases-use');
  await tap('#saves .tabs [data-tab="saves"]');
  await tap('#saves-list button[data-edit]');
  await p.waitForSelector('#editor[open] .ed-row');
  await shot('editor-save');
  await tap('#editor-tabs [data-pane="scout"]');
  await p.waitForSelector('#scout-results .ed-row');
  await shot('editor-scout');
  await tap('#editor-tabs [data-pane="players"]');
  await tap('.ed-row');
  await p.waitForSelector('#editor-form [data-f="Pace"]');
  await shot('editor-player');
  await tap('#editor-close');
  await tap('#editor-open');
  await p.waitForSelector('#editor[open] #editor-new');
  await shot('editor-home');
  await tap('#editor-close');
  await tap('#account-open');
  await p.waitForSelector('#account[open]');
  await shot('account');
  await tap('#account-close');
  await tap('#help-open');
  await p.waitForSelector('#help[open]');
  await shot('help');
  if (await p.isVisible('#help-report-open')) {
    await tap('#help-report-open');
    await p.waitForSelector('#report[open]');
    await shot('report');
    await tap('#report-close');
  } else await tap('#help-close');
  for (const page of ['pricing', 'privacy', 'terms', 'refunds']) {
    await p.goto(`${BASE}/${page}`);
    await shot(page, false);
  }
  await p.goto(`${BASE}/admin`);
  await shot('admin', false);
  await ctx.close();
}

try {
  await walk('laptop', { viewport: { width: 1280, height: 800 } });
  await walk('phone', { ...devices['iPhone 13'] });
  await walk('sideways', { ...devices['iPhone 13 landscape'] });
  console.log(errors.length ? `page errors:\n${errors.join('\n')}` : 'no page errors');
  console.log(cut.length ? `not fitting (${[...new Set(cut)].length}):\n${[...new Set(cut)].join('\n')}` : 'everything fits');
  console.log(crooked.length ? `not aligned (${[...new Set(crooked)].length}):\n${[...new Set(crooked)].join('\n')}` : 'everything aligned');
  process.exitCode = errors.length || cut.length || crooked.length ? 1 : 0;
} finally {
  await browser.close();
  await site.close();
}
