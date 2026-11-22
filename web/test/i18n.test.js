// The page's translations (web/i18n.js; docs/superpowers/specs/2026-11-22-page-languages-design.md): the choice of
// language, the lookup with English as the floor, placeholders and plurals.
import test from 'node:test';
import assert from 'node:assert/strict';
import { LANGUAGES, pickLanguage, priceText, useTable, t, tn } from '../i18n.js';

test('the languages: English and the nine of the game, each named in its own language', () => {
  assert.deepEqual(LANGUAGES.map((l) => l.code), ['en', 'es', 'pt', 'it', 'fr', 'de', 'nl', 'da', 'no', 'sv']);
  assert.equal(LANGUAGES.find((l) => l.code === 'de').name, 'Deutsch');
});

test('the language: the stored choice, else the browser\'s first match, else English', () => {
  assert.equal(pickLanguage({ stored: 'de', browser: ['fr'] }), 'de');
  assert.equal(pickLanguage({ stored: 'xx', browser: ['pt-BR', 'en'] }), 'pt');
  assert.equal(pickLanguage({ browser: ['nb-NO'] }), 'no');
  assert.equal(pickLanguage({ browser: ['nn'] }), 'no');
  assert.equal(pickLanguage({ browser: ['ja', 'sv-SE'] }), 'sv');
  assert.equal(pickLanguage({ browser: ['ja'] }), 'en');
  assert.equal(pickLanguage({}), 'en');
});

test('t: the translation, or the English as it is; placeholders filled', () => {
  useTable('de', { 'Saved {name}': '{name} gespeichert' });
  assert.equal(t('Saved {name}', { name: 'Arsenal.sav' }), 'Arsenal.sav gespeichert');
  assert.equal(t('Not in the table <b>x</b>'), 'Not in the table <b>x</b>', 'an unknown text (a save name) comes back as it is');
  assert.equal(t('Saved {name}'), '{name} gespeichert', 'no value: the placeholder stays');
  assert.equal(t('{a} and {a}', { a: 1 }), '1 and 1');
  useTable('en', {});
  assert.equal(t('Saved {name}', { name: 'x' }), 'Saved x');
});

test('tn: plural forms by the language\'s own rules, English as the floor', () => {
  useTable('de', { '{n} save': { one: '{n} Spielstand', other: '{n} Spielstände' } });
  const de = [0, 1, 2, 21].map((n) => tn(n, { one: '{n} save', other: '{n} saves' }));
  assert.deepEqual(de, ['0 Spielstände', '1 Spielstand', '2 Spielstände', '21 Spielstände']);
  useTable('de', {});
  assert.deepEqual([1, 3].map((n) => tn(n, { one: '{n} save', other: '{n} saves' }, {})), ['1 save', '3 saves']);
  useTable('pt', { '{n} save in {where}': { one: '{n} jogo salvo em {where}', other: '{n} jogos salvos em {where}' } });
  assert.equal(tn(2, { one: '{n} save in {where}', other: '{n} saves in {where}' }, { where: 'X' }), '2 jogos salvos em X');
  useTable('en', {});
});

// The tables against the page (web/test/i18n-strings.mjs): placeholders equal, no entry left behind by an edited
// sentence; the missing ones listed, and a failure only with I18N_STRICT=1 (before a language is announced).
test('the translation tables match the page\'s English', async () => {
  const { pageStrings, tables, checkTables } = await import('./i18n-strings.mjs');
  const strings = pageStrings();
  const report = checkTables(strings, await tables());
  for (const [code, r] of report) if (r.missing.length) console.log(`# ${code}: ${r.missing.length} missing`);
  assert.deepEqual([...report].flatMap(([code, r]) => r.placeholders.map((e) => `${code}: ${e}`)), [], 'placeholders of a translation equal its English\'s');
  assert.deepEqual([...report].flatMap(([code, r]) => r.stale.map((e) => `${code}: ${e}`)), [], 'no entry whose English is no longer on the page');
  if (process.env.I18N_STRICT === '1') assert.deepEqual([...report].flatMap(([code, r]) => r.missing.map((e) => `${code}: ${e}`)), []);
});

test('the check finds a wrong placeholder, a stale entry and a missing one', async () => {
  const { checkTables } = await import('./i18n-strings.mjs');
  const strings = new Map([['Saved {name}', new Set(['x.js'])], ['Help', new Set(['index.html'])], ['{n} save', new Set(['x.js'])]]);
  const r = checkTables(strings, new Map([['de', { 'Saved {name}': 'Gespeichert {nom}', 'Gone': 'Weg', '{n} save': { one: '{n} Spielstand', other: 'Spielstände' } }]])).get('de');
  assert.deepEqual(r.placeholders, ['Saved {name} → Gespeichert {nom}', '{n} save (other) → Spielstände']);
  assert.deepEqual(r.stale, ['Gone']);
  assert.deepEqual(r.missing, ['Help']);
});

test('the page\'s strings: t, tn and data-t found in the code and the page', async () => {
  const { stringsOf, htmlStrings } = await import('./i18n-strings.mjs');
  const js = "toast(t('Saved {name}', { name })); x = tn(n, { one: '{n} save', other: '{n} saves' }); t(\"Two\"); t(`Three`); t(variable);";
  assert.deepEqual([...stringsOf(js)].sort(), ['Saved {name}', 'Three', 'Two', '{n} save']);
  const { pluralsOf } = await import('./i18n-strings.mjs');
  assert.deepEqual([...pluralsOf(js)], [['{n} save', '{n} saves']]);
  const html = `<h2 data-t>How to play
    here</h2><p data-t-html>Read <a href="/x">this</a>.</p><button data-t-attr="title,aria-label" title="Close it" aria-label="Close">x</button>`;
  assert.deepEqual([...htmlStrings(html)].sort(), ['Close', 'Close it', 'How to play here', 'Read <a href="/x">this</a>.']);
});

test('the price from the server (CM_PRICE, "$5 a month"): its period in the page\'s language, any other as it is', () => {
  useTable('de', { '{amount} a month': '{amount} im Monat' });
  assert.equal(priceText('$5 a month'), '$5 im Monat');
  assert.equal(priceText('€4.50 a month'), '€4.50 im Monat');
  assert.equal(priceText('$50 once'), '$50 once', 'a price the page does not know: as it comes');
  assert.equal(priceText(undefined), '');
  useTable('en', {});
  assert.equal(priceText('$5 a month'), '$5 a month');
});
