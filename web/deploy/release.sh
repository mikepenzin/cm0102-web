#!/usr/bin/env bash
# A later release in one go (web/deploy/README.md, "A later release"): pull, build the engines, stage, deploy.
#
#   web/deploy/release.sh [--force] [--no-build] [--game <folder>] [-- <fly deploy options>]
#
# When git pull brings nothing new, it stops there; --force builds, stages and deploys anyway (after a failed deploy).
# --force     goes on when git pull brings nothing new
# --no-build  skips the engine builds (when the engine's code did not change since the last build)
# --game      the folder that holds cm0102.exe and Data (default: the Bosman Edition app's game folder)
# After --, the rest goes to fly deploy, for example: web/deploy/release.sh -- --wg=false
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
game="/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game"
build=1
force=0
fly_args=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --force) force=1 ;;
    --no-build) build=0 ;;
    --game) game="${2:?--game needs a folder}"; shift ;;
    --) shift; fly_args=("$@"); break ;;
    -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "release: unknown option $1 (--help)" >&2; exit 2 ;;
  esac
  shift
done

# One block, read whole before it runs: git pull may change this file.
{
cd "$repo"
echo "== git pull"
before="$(git rev-parse HEAD)"
git pull
if [[ "$(git rev-parse HEAD)" == "$before" && $force -eq 0 ]]; then
  echo "== nothing new from git pull: no build, no deploy (--force to deploy anyway)"
  exit 0
fi

if [[ $build -eq 1 ]]; then
  echo "== build the engine (JSPI)"
  cmake --build build-web --target cm0102_recomp seasons_rules
  if [[ -d build-web-asyncify ]]; then
    echo "== build the engine (Asyncify)"
    cmake --build build-web-asyncify --target cm0102_recomp
  else
    echo "== no build-web-asyncify: only JSPI browsers can play"
  fi
else
  echo "== engine builds skipped (--no-build)"
fi

echo "== stage"
node web/deploy/stage.mjs --game "$game"

echo "== fly deploy"
cd web/deploy/stage
fly deploy ${fly_args[@]+"${fly_args[@]}"}
exit
}
