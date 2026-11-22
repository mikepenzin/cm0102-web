// The page in the game's languages (web/README.md, "Languages"; docs/superpowers/specs/2026-11-22-page-languages-design.md).
// The English text is the key: t('Saved in this browser') gives its translation in the page's language, or the English
// when there is none, so a sentence not yet translated never breaks a language. Each language's table is a page file
// of its own (i18n-<code>.js, export default { English: translation }). The page's static text is marked in index.html
// (data-t, data-t-html, data-t-attr) and apply() translates it. Logs and reports stay English: only what the player
// reads goes through here. Nothing here touches the page at import.

// English and the nine other languages of the game (its Data/*.lng: dan, eng, esp, fra, ger, hol, ita, nor, por, swe).
export const LANGUAGES = [
  { code: 'en', name: 'English' }, { code: 'es', name: 'Español' }, { code: 'pt', name: 'Português' },
  { code: 'it', name: 'Italiano' }, { code: 'fr', name: 'Français' }, { code: 'de', name: 'Deutsch' },
  { code: 'nl', name: 'Nederlands' }, { code: 'da', name: 'Dansk' }, { code: 'no', name: 'Norsk' }, { code: 'sv', name: 'Svenska' },
];
const CODES = new Set(LANGUAGES.map((l) => l.code));
export const LANG_KEY = 'cm0102-lang';

// The stored choice when it is one of LANGUAGES, else the first of the browser's languages that is (Norwegian Bokmål
// and Nynorsk count as Norwegian), else English.
export function pickLanguage({ stored = null, browser = [] } = {}) {
  if (stored && CODES.has(stored)) return stored;
  for (const tag of browser ?? []) {
    let code = String(tag).toLowerCase().split(/[-_]/)[0];
    if (code === 'nb' || code === 'nn') code = 'no';
    if (CODES.has(code)) return code;
  }
  return 'en';
}

let lang = 'en', table = {};
const listeners = new Set();

export const language = () => lang;
export function useTable(code, t) { lang = code; table = t ?? {}; }
export function onLanguage(fn) { listeners.add(fn); return () => listeners.delete(fn); }

const fill = (text, vars) => (vars ? text.replace(/\{(\w+)\}/g, (m, k) => (k in vars ? String(vars[k]) : m)) : text);

// The text of english in the page's language, its {placeholders} filled from vars.
export function t(english, vars) {
  const s = table[english];
  return fill(typeof s === 'string' && s ? s : english, vars);
}

// The price as the server gives it (CM_PRICE, "$5 a month"): the period in the page's language. A price of another
// shape shows as it comes (the server's text, English).
export function priceText(price) {
  const m = /^(.+?) a month$/.exec(price ?? '');
  return m ? t('{amount} a month', { amount: m[1] }) : (price ?? '');
}

// A count with its noun: forms { one, other } in English; the table keeps the language's plural categories under the
// English one-form ({ one, few, many, other }, as Intl.PluralRules names them). {n} is the count.
export function tn(count, forms, vars = {}) {
  const all = { ...vars, n: count };
  const entry = table[forms.one];
  if (entry && typeof entry === 'object') {
    let cat = 'other';
    try { cat = new Intl.PluralRules(lang).select(count); } catch { /* the language's rules unknown: other */ }
    const s = entry[cat] ?? entry.other;
    if (s) return fill(s, all);
  }
  let cat = 'other';
  try { cat = new Intl.PluralRules('en').select(count); } catch { /* other */ }
  return fill(cat === 'one' ? forms.one : forms.other, all);
}

// The page's marked text in the page's language. data-t: the element's text (its English kept in data-t on first
// use, so it can go back to English); data-t-html: the same as HTML (only for the page's own text, with a link or bold);
// data-t-attr="title,aria-label": those attributes, each from its English kept in data-t-<name>.
export function apply(root) {
  for (const el of root.querySelectorAll('[data-t]')) {
    if (!el.dataset.t) el.dataset.t = el.textContent.trim().replace(/\s+/g, ' ');
    el.textContent = t(el.dataset.t);
  }
  for (const el of root.querySelectorAll('[data-t-html]')) {
    if (!el.dataset.tHtml) el.dataset.tHtml = el.innerHTML.trim().replace(/\s+/g, ' ');
    el.innerHTML = t(el.dataset.tHtml);
  }
  for (const el of root.querySelectorAll('[data-t-attr]')) {
    for (const name of el.dataset.tAttr.split(',').map((s) => s.trim()).filter(Boolean)) {
      const keep = `t-${name}`;
      if (el.getAttribute(`data-${keep}`) === null) el.setAttribute(`data-${keep}`, el.getAttribute(name) ?? '');
      const english = el.getAttribute(`data-${keep}`);
      if (english) el.setAttribute(name, t(english));
    }
  }
}

// The page in code: its table loaded (load(code) gives it; the default imports i18n-<code>.js), html's lang set, the
// marked text translated, the choice kept when keep, and every listener told (they render what they show anew).
// A table that does not load leaves the page in English.
export async function setLanguage(code, { load = (c) => import(`./i18n-${c}.js`).then((m) => m.default), keep = true, storage = null } = {}) {
  if (!CODES.has(code)) code = 'en';
  let next = {};
  if (code !== 'en') {
    try { next = (await load(code)) ?? {}; } catch { code = 'en'; next = {}; }
  }
  useTable(code, next);
  if (typeof document !== 'undefined') {
    document.documentElement.lang = code;
    apply(document);
  }
  if (keep) { try { storage?.setItem(LANG_KEY, code); } catch { /* not kept: the browser's language next time */ } }
  for (const fn of listeners) { try { fn(code); } catch { /* one listener does not stop the others */ } }
  return code;
}
