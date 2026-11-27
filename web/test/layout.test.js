// Tests of the layout's arithmetic (web/layout.js): the canvas size for a picture, and the move for the keyboard.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { BAR_STEP, barPlace, chipPlace, fitPicture, shiftForRow } from '../layout.js';

test('fitPicture: whole multiples of the picture\'s shape, as large as fits', () => {
  // 1280x800 is 8 by 5: a stage of 1470x704 takes 8*140 by 5*140.
  assert.deepEqual(fitPicture(1280, 800, 1470, 704, 2), { cssW: 1120, cssH: 700, bw: 2240, bh: 1400 });
  // The width limits: 700 wide gives 8*87 by 5*87.
  assert.deepEqual(fitPicture(1280, 800, 700, 856, 1), { cssW: 696, cssH: 435, bw: 696, bh: 435 });
  // Classic mode is 4 by 3.
  assert.deepEqual(fitPicture(800, 600, 1470, 704, 2), { cssW: 936, cssH: 702, bw: 1872, bh: 1404 });
  // A ratio that is not whole: the canvas's own pixels are whole, the shape still exact where the ratio allows.
  const f = fitPicture(1280, 800, 1000, 700, 1.5);
  assert.deepEqual(f, { cssW: 1000, cssH: 625, bw: 1500, bh: 937 });
  for (const [w, h, dpr] of [[1470, 704, 2], [1180, 770, 2], [1024, 700, 1], [1920, 1036, 1.25], [844, 340, 3]]) {
    const r = fitPicture(1280, 800, w, h, dpr);
    assert.equal(r.cssW * 5, r.cssH * 8, 'the shape is exact');
    assert.ok(r.cssW <= w && r.cssH <= h, 'it fits');
    assert.ok(w - r.cssW < 8 || h - r.cssH < 5, 'no larger step would fit');
  }
  assert.equal(fitPicture(1280, 800, 5, 3, 1), null, 'no room for even one step');
  assert.equal(fitPicture(0, 0, 100, 100, 1), null);
});

test('shiftForRow: the row comes into view above the keyboard, and the picture leaves no gap', () => {
  // A picture of 700 high from 60 down; 420 pixels stay visible above the keyboard.
  assert.equal(shiftForRow(60, 700, 100, 420), 0, 'a row near the top is in view already');
  assert.equal(shiftForRow(60, 700, 300, 420), 0, 'so is one above the keyboard\'s edge');
  const s = shiftForRow(60, 700, 560, 420);
  assert.ok(s < 0);
  assert.ok(60 + 560 + s > 60 && 60 + 560 + s < 420 - 30, `the row lands in view (${60 + 560 + s})`);
  // The last row: the picture moves until its bottom meets the visible bottom, not further.
  assert.equal(shiftForRow(60, 700, 690, 420), 420 - 760);
  // A picture that already ends above the keyboard does not move.
  assert.equal(shiftForRow(0, 300, 290, 420), 0);
});

test('barPlace: the page\'s buttons go beside or above the picture when there is room, else over its corner', () => {
  // A phone held sideways (844x390): a 1280x800 picture 624x390, 110 pixels of room on each side.
  const phone = fitPicture(1280, 800, 844, 390, 3);
  const left = (844 - phone.cssW) / 2;
  assert.equal(barPlace({ left, top: 0 }, 844, 390, 7 * BAR_STEP + 10), 'side');
  // Room beside it, but too short for the column of buttons: over the corner.
  assert.equal(barPlace({ left: 110, top: 0 }, 700, 250, 7 * BAR_STEP + 10), 'over');
  // Held upright (390x844): room above and below the picture, a row there.
  assert.equal(barPlace({ left: 0, top: 300 }, 390, 844, 7 * BAR_STEP + 10), 'top');
  // An iPad (1180x820, 16:10 like the picture): a few pixels of room only.
  const pad = fitPicture(1280, 800, 1180, 820, 2);
  assert.equal(barPlace({ left: (1180 - pad.cssW) / 2, top: (820 - pad.cssH) / 2 }, 1180, 820, 7 * BAR_STEP + 10), 'over');
  assert.equal(barPlace(null, 800, 600, 300), 'over');
});

// "Tap to type" sits on the game's red title bar, at its right end (a player: on the typing row it covered the column
// heads and the buttons under them). The bar is at the top of every screen of the game: 54 of 800 rows down.
test('chipPlace: on the title bar\'s right end, whatever the typing row', () => {
  const box = { left: 100, top: 20, cssW: 640, cssH: 400 };
  assert.deepEqual(chipPlace(box, 375), { x: 734, y: 47 }, 'x: 6px in from the right edge; y: 20 + 400 x 54/800');
  assert.deepEqual(chipPlace({ ...box, top: 0, cssH: 200 }, 375), { x: 734, y: 28 }, 'never closer than 28px to the stage\'s top');
});

// Full screen with no room beside the picture: the page's buttons lie over the picture's top right corner, where the
// chip goes. The chip then goes beside them on the same row, else below them; it never lies under them (a tap at its
// middle would reach a button).
test('chipPlace: clear of the buttons over the corner, beside them, else below them', () => {
  const box = { left: 0, top: 0, cssW: 1180, cssH: 737 }, chip = { w: 120, h: 32 };
  const at = chipPlace(box, 820, chip);
  assert.deepEqual(at, { x: 1174, y: 50 }, 'no buttons given: where it was');
  const bar = { left: 820, top: 8, right: 1172, bottom: 56 };
  assert.deepEqual(chipPlace(box, 820, chip, bar), { x: 814, y: 50 }, 'beside them: its right edge 6px left of theirs');
  const wide = { left: 60, top: 8, right: 1172, bottom: 56 };
  assert.deepEqual(chipPlace(box, 820, chip, wide), { x: 1174, y: 78 }, 'no room beside: below them, 6px under');
  assert.deepEqual(chipPlace(box, 820, chip, { left: 1100, top: 300, right: 1180, bottom: 380 }), at, 'buttons elsewhere (a folded tab): where it was');
});
