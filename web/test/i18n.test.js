// The page's translations (web/i18n.js; docs/superpowers/specs/2026-11-22-page-languages-design.md): the choice of
// language, the lookup with English as the floor, placeholders and plurals.
import test from 'node:test';
import assert from 'node:assert/strict';
import { LANGUAGES, pickLanguage, useTable, t, tn } from '../i18n.js';

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
