---
name: release
description: Makes a Bosman Edition release: version, changelog, site text, glossary and issues, the dry run, the publish after Max says yes, the issue and milestone close, the site check and the posts. Use for "make the release", "release slice", "publish 0.x.y", "dry run", "make_release.sh", "tag the release". Never publish without Max's yes in this conversation.
---

# Release

## 1. Prepare (on main)

1. Set `project(cm0102-mac VERSION x.y.z ...)` in `CMakeLists.txt`.
2. Add the `## x.y.z (pre-release)` section at the top of `CHANGELOG.md`: one line on what the release is, then one item per player-facing change, with the option name in backticks. A patch release whose work ships with this one is merged into this section and its heading removed.
3. Site (`site/index.html`): the "New in" heading and its list, the FAQ question "Which settings can I change in version x.y.z?", one line per new option with its default, and any FAQ answer the release changes. Coming next comes from the GitHub issues, live.
4. The glossary terms the spec names. The spec's acceptance boxes, ticked only when they pass.
5. Commit. Do not push main before the release when the site needs files from `site-assets.zip`.

## 2. Dry run

The checkout must be clean, untracked files too. If another session has files in the main checkout, run from a detached worktree of the commit:

```
git worktree add --detach /Users/max/Projects/cm0102/wt-release-x.y.z <commit>
```

Then run `tools/release/make_release.sh` with the paths in the handover, `/Users/max/Projects/cm0102/lead-notes/repo-moved/docs/HANDOVER.md` (outside the repo), section "How to make a release". Colima must run for the Linux step (`colima status`). Write the log to a file and wait on the job. Read the log for the Mac, Windows and Linux steps and the site assets.

## 3. Publish

Only after Max says yes in this conversation for this release. Run the same command with `--publish`. Then:

1. `gh release view vx.y.z` shows the files for Mac, Windows and Linux.
2. Close the milestone's issues and the milestone with `gh`, with a comment that names the release.
3. Check that the site workflow ran (`gh run list --workflow pages.yml`). If not: `gh workflow run pages.yml --ref main`. Open the site and check the version and the new text.
4. Push main if the publish did not.

## 4. After

- Posts: use the `release-posts` skill.
- Mark the spec `Status: Shipped`. Update the handover (outside the repo): the release, the next work, Max's open manual checks.
- Ask Max before you remove the build worktrees.
