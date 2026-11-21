// The how-to films again from their HyperFrames projects (make-videos.mjs), with the music of musicOf: the capture and
// the steps as recorded, no new recording. For a new track, or the music without a new capture of the page.
// node web/test/videos-music.mjs <folder> [editor|app]   (CM_VIDEO_MUSIC and the like as for make-videos.mjs)
import { copyFileSync, mkdtempSync, readFileSync, renameSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { composeVideo, musicOf, render } from './videos-compose.mjs';

const OUT = resolve(process.argv[2] ?? '.');
const which = process.argv[3];
for (const name of ['editor', 'app'].filter((n) => !which || n === which)) {
  const project = join(OUT, `${name}-project`), tmp = mkdtempSync(join(tmpdir(), 'cm0102-music-'));
  const capture = join(tmp, 'capture.mp4'), log = readFileSync(join(project, 'steps.json'));
  copyFileSync(join(project, 'assets', 'capture.mp4'), capture);
  rmSync(project, { recursive: true, force: true });
  const seconds = composeVideo({ dir: project, capture, log: { captions: JSON.parse(log).steps }, music: musicOf(name) });
  writeFileSync(join(project, 'steps.json'), log);
  rmSync(tmp, { recursive: true, force: true });
  const mp4 = join(OUT, `${name}.mp4`);
  render(project, `${mp4}.part.mp4`);
  renameSync(`${mp4}.part.mp4`, mp4);
  console.log(`made ${mp4} (${Math.round(seconds)} s, music: ${musicOf(name) ?? 'none'})`);
}
