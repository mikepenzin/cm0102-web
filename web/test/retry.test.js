// Tests of retry.js: which errors of a game file's download are a dropped connection, so the file is asked for again.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { retryable } from '../retry.js';

test('a dropped connection is asked for again: fetch\'s TypeError, a short body, a server error', () => {
  assert.equal(retryable(new TypeError('Failed to fetch')), true);
  assert.equal(retryable(Object.assign(new Error('Data/staff.dat: 10 of 20 bytes'), { again: true })), true);
});

test('the browser\'s cache refusing a body that broke on the way (Android, 26 November 2026) is asked for again', () => {
  const e = new DOMException("Failed to execute 'put' on 'Cache': Cache.put() encountered a network error", 'NetworkError');
  assert.equal(retryable(e), true);
});

test('a stop the player asked for, or an error that will not go away, is not', () => {
  assert.equal(retryable(new DOMException('The user aborted a request.', 'AbortError')), false);
  assert.equal(retryable(new DOMException('Quota exceeded', 'QuotaExceededError')), false);
  assert.equal(retryable(Object.assign(new Error('HTTP 404'), { again: false })), false);
  assert.equal(retryable(null), false);
});
