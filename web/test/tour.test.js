// The first-visit tour's own logic (web/tour.js): where a card goes, and what the Cloud Saves step says for each
// state of the account. The tour on the page: tour-check.mjs.
import test from 'node:test';
import assert from 'node:assert/strict';
import { TOUR_STEPS, cloudStepText, placeCard } from '../tour.js';

test('a card goes below a button of a row, beside a button of a column, and stays on the screen', () => {
  const card = { w: 300, h: 160 };
  const row = placeCard({ left: 900, right: 940, top: 6, bottom: 44, width: 40, height: 38 }, card, 1280, 800);
  assert.equal(row.side, 'below');
  assert.equal(row.top, 54);
  assert.equal(row.left, 640, 'its right edge under the button\'s');
  const atLeftEdge = placeCard({ left: 4, right: 44, top: 6, bottom: 44, width: 40, height: 38 }, card, 390, 844);
  assert.equal(atLeftEdge.left, 8, 'kept inside the window');
  const column = placeCard({ left: 900, right: 942, top: 100, bottom: 136, width: 42, height: 36 }, card, 956, 440, { column: true });
  assert.equal(column.side, 'left');
  assert.ok(column.left + card.w <= 900, 'beside the column, not over it');
  const low = placeCard({ left: 900, right: 942, top: 400, bottom: 436, width: 42, height: 36 }, card, 956, 440, { column: true });
  assert.ok(low.top + card.h <= 440 - 8, 'a button near the bottom: the card stays on the screen');
});

test('the Cloud Saves step says what they do and how to turn them on, in the Account dialog\'s words', () => {
  const coffee = { auth: true, offer: { free: false, coffee: true, price: '$5 a month' } };
  const signedOut = cloudStepText(coffee);
  assert.match(signedOut, /all your devices/);
  assert.match(signedOut, /sign in with Google, then press "Become a member" \(\$5 a month\) and join on Buy Me a Coffee with the same email/);
  assert.match(signedOut, /Without them your saves stay in this browser/);
  const signedIn = cloudStepText({ ...coffee, user: { email: 'a@b.c', cloud: false } });
  assert.match(signedIn, /open this button, press "Become a member" \(\$5 a month\)/);
  assert.doesNotMatch(signedIn, /sign in/i);
  const paddle = cloudStepText({ auth: true, user: { email: 'a@b.c' }, offer: { free: false, price: '$5 a month', paddle: {} } });
  assert.match(paddle, /press "Get Cloud Saves" \(\$5 a month\) and pay/);
  assert.match(cloudStepText({ auth: true, user: { email: 'a@b.c', cloud: true }, offer: coffee.offer }), /^Cloud Saves are on for a@b\.c/);
  assert.match(cloudStepText({ auth: true, offer: { free: true } }), /Sign in here with Google, and they are on, free/);
  assert.match(cloudStepText(null), /sign in with Google/, 'before the account is known: the general way');
});

test('the steps: the page\'s buttons, Help last', () => {
  assert.deepEqual(TOUR_STEPS.map((s) => s.target), ['saves-open', 'account-open', 'editor-open', 'kbd-open', 'right-click', 'fullscreen', 'help-open']);
});
