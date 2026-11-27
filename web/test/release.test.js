// web/deploy/release.sh, run for real in a throwaway checkout: a bare "origin", a clone with the script, and stand-ins
// for cmake, the stage step and fly on the PATH. What it must do: deploy when HEAD is not the last commit deployed,
// skip when it is, whatever git pull brought.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { chmodSync, copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const SCRIPT = join(dirname(fileURLToPath(import.meta.url)), '..', 'deploy', 'release.sh');

function setUp() {
  const tmp = mkdtempSync(join(tmpdir(), 'cm0102-release-'));
  const git = (cwd, ...args) => execFileSync('git', args, { cwd, encoding: 'utf8', env: { ...process.env, GIT_AUTHOR_NAME: 't', GIT_AUTHOR_EMAIL: 't@t', GIT_COMMITTER_NAME: 't', GIT_COMMITTER_EMAIL: 't@t' } }).trim();
  const origin = join(tmp, 'origin.git'), repo = join(tmp, 'repo'), bin = join(tmp, 'bin'), log = join(tmp, 'fly.log');
  execFileSync('git', ['init', '-q', '--bare', '-b', 'main', origin]);
  execFileSync('git', ['clone', '-q', origin, repo]);
  mkdirSync(join(repo, 'web', 'deploy'), { recursive: true });
  copyFileSync(SCRIPT, join(repo, 'web', 'deploy', 'release.sh'));
  // The stage step: makes the folder fly deploy runs in.
  writeFileSync(join(repo, 'web', 'deploy', 'stage.mjs'), "import { mkdirSync } from 'node:fs'; mkdirSync(new URL('./stage', import.meta.url), { recursive: true });\n");
  writeFileSync(join(repo, 'README'), 'one\n');
  git(repo, 'add', '-A'); git(repo, 'commit', '-qm', 'one'); git(repo, 'push', '-q', 'origin', 'main');
  // cmake: nothing to build. fly: notes the commit it deployed, or fails when FLY_FAIL is set.
  mkdirSync(bin);
  writeFileSync(join(bin, 'cmake'), '#!/bin/sh\nexit 0\n');
  writeFileSync(join(bin, 'fly'), `#!/bin/sh\n[ -n "$FLY_FAIL" ] && exit 1\ngit -C "${repo}" rev-parse HEAD >> "${log}"\n`);
  for (const f of ['cmake', 'fly']) chmodSync(join(bin, f), 0o755);
  const run = (args = [], env = {}) => spawnSync('bash', [join(repo, 'web', 'deploy', 'release.sh'), '--no-build', '--game', tmp, ...args],
    { cwd: repo, encoding: 'utf8', env: { ...process.env, PATH: `${bin}:${process.env.PATH}`, ...env } });
  const deployed = () => (existsSync(log) ? readFileSync(log, 'utf8').trim().split('\n') : []);
  return { tmp, repo, git, run, deployed };
}

test('release.sh: deploys a commit made in the release checkout itself, skips one already deployed', () => {
  const r = setUp();
  try {
    const first = r.run();
    assert.equal(first.status, 0, first.stderr);
    assert.deepEqual(r.deployed(), [r.git(r.repo, 'rev-parse', 'HEAD')], 'nothing deployed yet: the first run deploys');
    const again = r.run();
    assert.equal(again.status, 0, again.stderr);
    assert.equal(r.deployed().length, 1, 'HEAD already deployed: no second deploy');
    assert.match(again.stdout, /already deployed/);
    // A commit made and pushed from the release checkout: git pull brings nothing, yet HEAD is new.
    writeFileSync(join(r.repo, 'README'), 'two\n');
    r.git(r.repo, 'commit', '-qam', 'two'); r.git(r.repo, 'push', '-q', 'origin', 'main');
    const local = r.run();
    assert.equal(local.status, 0, local.stderr);
    assert.deepEqual(r.deployed().at(-1), r.git(r.repo, 'rev-parse', 'HEAD'), 'the new HEAD is deployed though git pull brought nothing');
  } finally { rmSync(r.tmp, { recursive: true, force: true }); }
});

test('release.sh: a failed deploy is not recorded, so the next run deploys again; --force deploys what is deployed', () => {
  const r = setUp();
  try {
    const failed = r.run([], { FLY_FAIL: '1' });
    assert.notEqual(failed.status, 0, 'fly failed: the script fails');
    assert.equal(r.deployed().length, 0);
    const next = r.run();
    assert.equal(next.status, 0, next.stderr);
    assert.equal(r.deployed().length, 1, 'not deployed before: deployed now');
    const forced = r.run(['--force']);
    assert.equal(forced.status, 0, forced.stderr);
    assert.equal(r.deployed().length, 2, '--force: deployed again');
  } finally { rmSync(r.tmp, { recursive: true, force: true }); }
});
