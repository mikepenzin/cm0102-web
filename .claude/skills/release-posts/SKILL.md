---
name: release-posts
description: Writes the two posts for each Bosman Edition release on the champman0102 forum, the thread post (a reply with two or three pictures of what is new) and the updated original post (the first post of the thread), plus an optional Reddit update, and makes the pictures. Use for "update post", "release post", "forum post", "thread post", "Reddit post", "original post", "announce the release", or at the end of a release slice. Max posts them himself; this skill only drafts.
---

# Release posts

Each release gets two posts in the Bosman Edition thread on the champman0102 forum (champman0102.co.uk). Max posts both himself, after the release is published. Never post, and never open a site to post.

1. The thread post is a reply in the thread. It has two or three pictures of the biggest new features, each with a short paragraph under it. Each picture is an `[img]` tag with its link on the project site (see Pictures), so Max can copy and paste the post as it is.
2. The updated original post is the new text of the first post of the thread. It explains the work (the recompile, the official 2008 files, the same-game rule and how it is tested), puts the current release first, lists what the port adds, lists every release with its date and a one-line summary, and ends with what comes next, the credits and the bug report line. Start from the original post in the previous `release-x.y.z-posts.md` and bring every part up to date.
3. Optional: a short "Update: x.y.z is out" list for the end of the Reddit post (https://www.reddit.com/r/footballmanagergames/comments/1x6ih6l/ive_rebuilt_cm_0102_to_run_natively_on_mac/), where Max posts as u/IndividualAd6919.

## Inputs

- The release section of `CHANGELOG.md` (the facts).
- The spec of the release in `docs/specs/` (what each feature does, and its option name and default).
- `/Users/max/Projects/cm0102/lead-notes/repo-moved/docs/requests.md` (outside the repo): credit each player whose request ships, by name and where they asked (for example "gundenaf on Reddit").
- `gh release list`: the dates of earlier releases for the list in the original post.
- The previous `/Users/max/Projects/cm0102/release-*-posts.md` (the voice and the layout to follow).

## Output

- `/Users/max/Projects/cm0102/release-x.y.z-posts.md`, outside the repo. It starts with one line that says Max posts these himself.
- Pictures in `/Users/max/Projects/cm0102/release-x.y.z-images/`, outside the repo. Name them `1-<feature>.png`, `2-<feature>.png` and so on, in the order of the post. Never commit them: they show the original game's screens.

## Pictures

1. Make them from the release build, after the version bump: some screens show the version (the options box does).
2. Use widescreen (1280 x 800) at full size. Do not scale down.
3. Load `start.sav` (Arsenal, 14.7.01) or `prematch.sav` from `/Users/max/Projects/cm0102/test-fixtures`. Use a temporary `RECOMP_USER_DIR`. Never use Max's own saves.
4. Reuse the dumps of the feature's own smoke test where one exists (for example `tests/smoke/foreign_run.py` and `tests/smoke/options_box_run.py` write each screen to their out folder). Otherwise write a `tests/fixtures/fixtures.py play` script with `dump` steps.
5. Look at each picture before you use it. It must show the feature at once, with no debug text and no cut labels. The manager name in test saves is "Test Smoke", which is fine.

## Hosting the pictures

Max pastes the posts as they are, so each picture needs a public link that serves `image/png`. GitHub release files do not (they come as `application/octet-stream`). Host them on the project site, after the publish:

1. Copy each picture to `/Users/max/Projects/cm0102/site-release-assets/post-x.y.z-<feature>.png`. The folder is outside git; the site's pictures come from it.
2. Zip the folder again as `make_release.sh` does: `(cd site-release-assets && zip -q -r -X - . -x '.*') > site-assets.zip`. Put it in `build/release-x.y.z/` of the release worktree, write `SHA256SUMS` again, and upload both with `gh release upload vx.y.z ... --clobber`.
3. Run `gh workflow run pages.yml --ref main` and wait for it.
4. Check each link with `curl -s -o /dev/null -w '%{http_code} %{content_type}'`: it must give `200 image/png`. The links are `https://maxevans60.github.io/cm0102-bosman-edition/assets/post-x.y.z-<feature>.png`.

Better: copy the pictures into `site-release-assets` before the publish, so `make_release.sh` zips them and no upload is needed after.

## BBCode

The champman0102 forum uses BBCode. Write both forum posts in it, ready to paste: `[b]...[/b]` for headings, `[size=150]...[/size]` for the title, `[url=link]text[/url]` for links, `[img]link[/img]` for pictures, and a plain bullet character (•) at the start of each list line. Do not use `[list]` and `[*]`: the champman0102 forum removes asterisks in pairs, so the items run together (seen on 0.5.0). No Markdown in the forum posts. The optional Reddit update stays in Markdown.

## Rules for the text

- Say what each feature does, in plain words, for a player. Name the screen and the menu item where it opens.
- Put the biggest player-facing features first, with the pictures. Fixes and small changes go in an "Also new" list.
- Claim only what the tests show. Do not write "no feature changes the game's decisions" unless the release's same-save checks show it for every option. Say which options take effect only at the next start.
- Do not write "no fan patches" in the original post, and do not list exceptions to the same-game rule there (Max removed both from the 0.5.0 post). The changelog keeps the details.
- No public text names patch authors, even when a feature follows their patch.
- End the thread post with what comes next (from the roadmap in the handover, `/Users/max/Projects/cm0102/lead-notes/repo-moved/docs/HANDOVER.md` (outside the repo)) and the line asking for bug reports with the computer and the clicks. From 0.6.0 on, that line and the bug report line of the original post also say: for a picture or window problem, press Ctrl+Shift+D (Cmd+Shift+D on a Mac) and paste the display report.
- British English. No exclamation marks. Apply the humanizer skill to both posts before you show them to Max: fix every §1 to §5 tell, and remove dashes and bold labels on list items. The bold "Update: x.y.z is out" line of the Reddit update is the one bold line.
- Write the release date (the day of the publish) in the original post. Never leave a placeholder.
- After Max posts, add the shipped version to the rows of that requests file.
