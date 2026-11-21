// Every screen of the page, on a laptop and on a phone, for a review of the look (web/README.md, "Look and feel"):
// the start box, the bar over the game, Saved games and its three tabs, the editor, the account, Help, Send to the
// developer, the legal pages and the admin page. The stand-in site (demo-site.mjs).
// NODE_PATH=$(npm root -g) node web/test/screens.mjs <folder>     → <folder>/<laptop|phone>-NN-<screen>.png
import { createRequire } from 'node:module';
import { mkdirSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { fakeSaveGame } from './fakes.mjs';
import { startDemoSite } from './demo-site.mjs';

const require = createRequire(import.meta.url);
const { chromium, devices } = require('playwright');
const OUT = resolve(process.argv[2] ?? 'screens');
mkdirSync(OUT, { recursive: true });
const TOKEN = 's'.repeat(24);
const site = await startDemoSite({ adminToken: TOKEN });
const { BASE } = site;
const browser = await chromium.launch();
const save = await fakeSaveGame();
const errors = [];

async function walk(kind, options) {
  const ctx = await browser.newContext(options);
  const p = await ctx.newPage();
  p.on('pageerror', (e) => errors.push(`${kind}: ${e.message}`));
  p.on('dialog', (d) => d.accept());
  let n = 0;
  const shot = async (name) => { await p.waitForTimeout(350); await p.screenshot({ path: join(OUT, `${kind}-${String(++n).padStart(2, '0')}-${name}.png`) }); };
  const tap = (sel) => p.locator(sel).first().click();
  await p.goto(BASE);
  await p.waitForSelector('#go');
  await shot('start');
  await tap('#go');
  await p.waitForFunction(() => window.__fake, null, { timeout: 30000 });
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
    await shot(page);
  }
  await p.goto(`${BASE}/admin`);
  await shot('admin');
  await ctx.close();
}

try {
  await walk('laptop', { viewport: { width: 1280, height: 800 } });
  await walk('phone', { ...devices['iPhone 13'] });
  console.log(errors.length ? `page errors:\n${errors.join('\n')}` : 'no page errors');
} finally {
  await browser.close();
  await site.close();
}
