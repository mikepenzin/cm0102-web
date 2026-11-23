#!/bin/bash
# Builds the three downloads (Mac, Windows, Linux) of one release, checks them and, with --publish, uploads a GitHub release.
#
# Usage:
#   tools/release/make_release.sh --game <clean files> --data-3960 <folder> --data-3965 <folder>
#       --sdl2-mac <folder> --sdl2-mingw <folder> --site-assets <folder> --season-dbs <folder> --own-dbs <folder>
#       --run-set <folder> [--program <cm0102_recomp>] [--publish]
#   tools/release/make_release.sh --check-set --run-set <folder>
#
#   --game         the clean files (tools/clean_files/make_clean_files.py). The script checks them
#                  against tools/clean_files/manifest.tsv.
#   --data-3960    the 3.9.60 data set (make_clean_files.py --data-3960), checked against manifest-3960.tsv
#   --data-3965    the 3.9.65 data set (make_clean_files.py --data-3965), checked against manifest-3965.tsv
#                  Both go into the game folder of each download as Data-3.9.60 and Data-3.9.65, where
#                  the option database finds them (src/recomp_rt/win/files.c).
#   --season-dbs   the folder with the zips of the Update Team databases (test-fixtures/databases/zips). The
#                  script makes the 25 season database zips of the season database table from it
#                  (tools/release/make_season_zips.py): each holds only the 20 data files and the source's euro.cfg and player_setup.cfg, in a folder with
#                  the neutral name of docs/research/rules-0.6.0/databases-signatures.tsv. It checks each
#                  zip against that table and stops on a mismatch. The zips go in Seasons/ of the game
#                  folder of each download (src/recomp_rt/win/seasons_db.h unpacks them on the first pick).
#   --own-dbs      the folder with the zips of our own season databases (Summer 2002 for 2002/03, Summer 2004 for
#                  2004/05 and Summer 2006 for 2006/07, Q351), each a zip of the converter's or build's output
#                  folder. make_season_zips.py reads it as a second source.
#   --sdl2-mac     the folder with mac-arm64 and mac-x86_64 SDL2 builds (tools/mac/build_sdl2.sh)
#   --sdl2-mingw   the SDL2 mingw development release (SDL2-2.x.y)
#   --site-assets  the site's pictures (logo, screenshots). They go into site-assets.zip on the release,
#                  where the Pages workflow fetches them, so git never holds them. logo.png also gives
#                  the intro logo, Data/bosman_logo.rgn of each download (tools/release/make_intro_logo.py).
#   --run-set      the --out folder of a run of the release set (tests/seasons/future/run_set.py, the 34 rows of the
#                  release tests from 0.7.1). Before any build the script lists the set with the seed of this release
#                  (version 0.M.P gives the seed M*10+P: 0.7.2 gives 72), runs its release check (run_set.py --verify
#                  --seed) and stops when a game of the set did not pass, when the run used another seed, when the
#                  checker of this checkout finds a fault on a save the run kept, or when the game code changed after
#                  the run's commit (Q12). Then it runs online release test 1 (tests/online/release_mac_run.py: two
#                  and four games through the real lobby and relay) and stops unless it passes. Not needed with
#                  --stage-only.
#   --program      the cm0102_recomp for online release test 1. Default build/cm0102_recomp of this checkout.
#   --no-run-set   skip the run set check (run_set.py --verify) and the --run-set option. Online release test 1 still
#                  runs. Only on a decision of Max to release without the full run set (0.7.2, 17 November 2026).
#   --check-set    a dry run: print the seed and the rows of the release set, run the release check on --run-set and
#                  stop (no build, no online test). It needs only --run-set. Exit 1 when the run does not pass.
#   --stage-only   stop after the game folder and its manifest are staged in build/release-<version> (before the
#                  Mac, Windows and Linux builds). --sdl2-mac and --sdl2-mingw are then not needed. For tests.
#   --publish      tag v<version>, push the tag and upload the release. Without it the script only builds
#                  and checks, and prints what it would upload.
#
# The version is the one in CMakeLists.txt. CHANGELOG.md must hold a "## <version>" section, which
# becomes the release notes. Versions below 1.0.0 go up as pre-releases.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GH_REPO="maxevans60/cm0102-bosman-edition"
GAME=""; DATA_3960=""; DATA_3965=""; SDL_MAC=""; SDL_MINGW=""; SITE=""; SEASON_DBS=""; OWN_DBS=""; RUN_SET=""; STAGE_ONLY=0
PUBLISH=0; CHECK_SET=0; PROGRAM=""; NO_RUN_SET=0
while [ $# -gt 0 ]; do
    case "$1" in
        --game) GAME="$2"; shift 2 ;;
        --data-3960) DATA_3960="$2"; shift 2 ;;
        --data-3965) DATA_3965="$2"; shift 2 ;;
        --sdl2-mac) SDL_MAC="$2"; shift 2 ;;
        --sdl2-mingw) SDL_MINGW="$2"; shift 2 ;;
        --site-assets) SITE="$2"; shift 2 ;;
        --season-dbs) SEASON_DBS="$2"; shift 2 ;;
        --own-dbs) OWN_DBS="$2"; shift 2 ;;
        --run-set) RUN_SET="$2"; shift 2 ;;
        --program) PROGRAM="$2"; shift 2 ;;
        --check-set) CHECK_SET=1; shift ;;
        --no-run-set) NO_RUN_SET=1; shift ;;
        --stage-only) STAGE_ONLY=1; shift ;;
        --publish) PUBLISH=1; shift ;;
        *) echo "make_release.sh: unknown option $1" >&2; exit 2 ;;
    esac
done
NEEDED="GAME DATA_3960 DATA_3965 SDL_MAC SDL_MINGW SITE SEASON_DBS OWN_DBS RUN_SET"
[ "$NO_RUN_SET" = 1 ] && NEEDED="GAME DATA_3960 DATA_3965 SDL_MAC SDL_MINGW SITE SEASON_DBS OWN_DBS"
[ "$STAGE_ONLY" = 1 ] && NEEDED="GAME DATA_3960 DATA_3965 SITE SEASON_DBS OWN_DBS"
[ "$CHECK_SET" = 1 ] && NEEDED="RUN_SET"
for v in $NEEDED; do
    [ -n "${!v}" ] || { echo "make_release.sh: give --$(echo $v | tr 'A-Z_' 'a-z-' | sed 's/sdl-/sdl2-/')." >&2; exit 2; }
done
[ "$CHECK_SET" = 1 ] || [ -f "$GAME/cm0102.exe" ] || { echo "make_release.sh: $GAME has no cm0102.exe." >&2; exit 2; }

VERSION="$(sed -n 's/^project([^)]*VERSION \([0-9][0-9.]*\).*/\1/p' "$REPO/CMakeLists.txt" | head -n 1)"
TAG="v$VERSION"
# The seed of the release tests (spec R8): each row plays this seed, so a release is repeatable and the next release
# plays other games. Version 0.M.P gives M*10+P: 0.7.1 gives 71, 0.7.2 gives 72.
SEED="$(echo "$VERSION" | awk -F. '{ print $2 * 10 + $3 }')"
if [ "$CHECK_SET" = 1 ]; then
    echo "make_release.sh: dry run of the release set for $VERSION, seed $SEED"
    python3 -I "$REPO/tests/seasons/future/run_set.py" --list --set release --seed "$SEED"
    python3 -I "$REPO/tests/seasons/future/run_set.py" --verify "$RUN_SET" --seed "$SEED"
    exit $?
fi
NOTES="$(awk -v v="## $VERSION" 'index($0, v) == 1 { on = 1; next } /^## / && on { exit } on' "$REPO/CHANGELOG.md")"
[ -n "$NOTES" ] || { echo "make_release.sh: CHANGELOG.md has no section for $VERSION." >&2; exit 2; }
if [ -n "$(git -C "$REPO" status --porcelain)" ]; then
    echo "make_release.sh: commit or remove every change and every new file first, so that the tag names the code that was built." >&2; exit 2
fi

# The release set (release tests from 0.7.1, Q12): every row passed with this release's seed and the checker finds no
# fault on its saves. Then online release test 1 (two and four games through the real lobby and relay).
if [ "$STAGE_ONLY" = 0 ]; then
    echo "make_release.sh: release set of $VERSION, seed $SEED"
    python3 -I "$REPO/tests/seasons/future/run_set.py" --list --set release --seed "$SEED"
    if [ "$NO_RUN_SET" = 1 ]; then
        echo "make_release.sh: WARNING: the run set check is skipped (--no-run-set)." >&2
    else
        python3 -I "$REPO/tests/seasons/future/run_set.py" --verify "$RUN_SET" --seed "$SEED" ||
            { echo "make_release.sh: the run set in $RUN_SET does not pass (above)." >&2; exit 1; }
    fi
    PROGRAM="${PROGRAM:-$REPO/build/cm0102_recomp}"
    [ -x "$PROGRAM" ] || { echo "make_release.sh: no program $PROGRAM for online release test 1. Give --program." >&2; exit 2; }
    # The online test must run the program that played the set (its sha256 is in the head line of results.tsv).
    if [ "$NO_RUN_SET" = 0 ]; then
        grep -q "$(shasum -a 256 "$PROGRAM" | cut -d' ' -f1)" "$RUN_SET/results.tsv" ||
            { echo "make_release.sh: $PROGRAM is not the program that played the run set. Give --program." >&2; exit 2; }
    fi
    python3 -I "$REPO/tests/online/release_mac_run.py" "$PROGRAM" "$GAME" ||
        { echo "make_release.sh: online release test 1 did not pass (above)." >&2; exit 1; }
fi
OUT="$REPO/build/release-$VERSION"
mkdir -p "$OUT"
CLEAN="$REPO/tools/clean_files"
python3 -I "$CLEAN/make_clean_files.py" --check "$GAME" --manifest "$CLEAN/manifest.tsv"
python3 -I "$CLEAN/make_clean_files.py" --check "$DATA_3960" --manifest "$CLEAN/manifest-3960.tsv"
python3 -I "$CLEAN/make_clean_files.py" --check "$DATA_3965" --manifest "$CLEAN/manifest-3965.tsv"

# The game folder of the downloads: the clean files and the two extra data sets. Its manifest is the
# three manifests, with the data set paths in their folders.
# --delete keeps a file of an earlier run out of it.
STAGE="$OUT/game"
mkdir -p "$STAGE"
rsync -a --delete --exclude .DS_Store --exclude /Data-3.9.60 --exclude /Data-3.9.65 "$GAME/" "$STAGE/"
rsync -a --delete --exclude .DS_Store "$DATA_3960/" "$STAGE/Data-3.9.60/"
rsync -a --delete --exclude .DS_Store "$DATA_3965/" "$STAGE/Data-3.9.65/"
# The intro logo (Q208): made from the site logo, in the downloads only. The game shows it in place of
# Data/logo.rgn (src/recomp_rt/win/files.c).
python3 -I "$REPO/tools/release/make_intro_logo.py" "$SITE/logo.png" "$STAGE/Data/bosman_logo.rgn"
# The season databases (0.6.0 slice 14): one zip each, checked against the signatures table. The first rsync
# above removed any Seasons folder of an earlier run, and the tool stops when its output folder is not empty.
python3 -I "$REPO/tools/release/make_season_zips.py" --season-dbs "$SEASON_DBS" --season-dbs "$OWN_DBS" --out "$STAGE/Seasons"
# The rules table of the game (0.6.0 slice 12): one file from the parts of docs/research/rules-0.6.0. The game
# reads Seasons/rules.tsv at the start (src/recomp_rt/win/season_rules.c). The tool stops on a row it cannot read.
python3 -I "$REPO/tools/seasons/make_rules.py" --out "$STAGE/Seasons/rules.tsv"
# The names table and the club moves table (0.6.0 slice 15): the game reads them at the start
# (src/recomp_rt/win/season_names.c, season_clubs.c).
python3 -I "$REPO/tools/seasons/make_names.py" --out "$STAGE/Seasons/names.tsv"
python3 -I "$REPO/tools/seasons/make_clubs.py" --out "$STAGE/Seasons/clubs.tsv"
python3 -I "$REPO/tools/seasons/make_club_records.py" --out "$STAGE/Seasons/club_records.tsv"
GAME="$STAGE"
MANIFEST="$OUT/manifest-download.tsv"
{
    cat "$CLEAN/manifest.tsv"
    grep -v '^#' "$CLEAN/manifest-3960.tsv" | awk -F '\t' -v OFS='\t' '{ $3 = "Data-3.9.60/" $3; print }'
    grep -v '^#' "$CLEAN/manifest-3965.tsv" | awk -F '\t' -v OFS='\t' '{ $3 = "Data-3.9.65/" $3; print }'
    printf '%s\t%s\tData/bosman_logo.rgn\n' "$(shasum -a 256 "$STAGE/Data/bosman_logo.rgn" | cut -d ' ' -f 1)" \
        "$(wc -c < "$STAGE/Data/bosman_logo.rgn" | tr -d ' ')"
    for z in "$STAGE"/Seasons/*.zip; do
        printf '%s\t%s\tSeasons/%s\n' "$(shasum -a 256 "$z" | cut -d ' ' -f 1)" "$(wc -c < "$z" | tr -d ' ')" "$(basename "$z")"
    done
    for t in rules names clubs club_records; do
        printf '%s\t%s\tSeasons/%s.tsv\n' "$(shasum -a 256 "$STAGE/Seasons/$t.tsv" | cut -d ' ' -f 1)" \
            "$(wc -c < "$STAGE/Seasons/$t.tsv" | tr -d ' ')" "$t"
    done
} > "$MANIFEST"
CHECK_ARGS=(--manifest "$MANIFEST")
if [ "$STAGE_ONLY" = 1 ]; then
    echo "== Staged in $STAGE, manifest $MANIFEST"
    du -sh "$STAGE/Seasons"
    exit 0
fi

echo "== Mac"
"$REPO/tools/mac/make_app.sh" --game "$GAME" --sdl2-arm64 "$SDL_MAC/mac-arm64" --sdl2-x86_64 "$SDL_MAC/mac-x86_64" \
    --out "$REPO/build/mac" --dmg-logo "$SITE/logo.png"
DMG="$REPO/build/mac/CM-01-02-Bosman-Edition-$VERSION-mac.dmg"
python3 -I "$REPO/tools/mac/check_package.py" "$DMG" ${CHECK_ARGS[@]+"${CHECK_ARGS[@]}"}
cp "$DMG" "$OUT/"

echo "== Windows"
# The source is this checkout (-S): with -B only, cmake takes the current folder as the source of a new build folder,
# so a dry run started in another checkout built that checkout's code (gate (d) on 61bbb76d: the exe had version
# 0.5.0, the main checkout's, and the setup name was the 0.5.0 one). A build-win made from another source stops here.
CACHE="$REPO/build-win/CMakeCache.txt"
if [ -f "$CACHE" ]; then
    HOME_DIR="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$CACHE")"
    if [ "$HOME_DIR" != "$REPO" ]; then
        echo "make_release.sh: $REPO/build-win was made from $HOME_DIR, not from this checkout. Remove that folder and run again." >&2
        exit 1
    fi
fi
cmake -S "$REPO" -B "$REPO/build-win" -DCMAKE_TOOLCHAIN_FILE="$REPO/packaging/windows/mingw-w64-x86_64.cmake" \
    -DSDL2_DIR="$SDL_MINGW/x86_64-w64-mingw32/lib/cmake/SDL2" -DCM0102_EXE="$GAME/cm0102.exe" \
    -DCM0102_GAME_DIR="$GAME" >/dev/null
cmake --build "$REPO/build-win" -j --target windows_installer
SETUP="$REPO/build-win/CM0102-Bosman-Edition-$VERSION-Setup.exe"
python3 -I "$REPO/tools/windows/check_installer.py" "$REPO/build-win/cm0102_recomp.exe" "$SETUP" --version "$VERSION" \
    --manifest "$MANIFEST" --game-dir "$GAME" --nsis-log "$REPO/build-win/installer-log.txt"
cp "$SETUP" "$OUT/"

echo "== Linux"
# One x86-64 AppImage, built and checked in a linux/amd64 container (Docker, Colima or OrbStack).
"$REPO/tools/linux/make_appimage.sh" --game "$GAME" --manifest "$MANIFEST" --out "$REPO/build/linux"
APPIMAGE="$REPO/build/linux/CM0102-Bosman-Edition-$VERSION-x86_64.AppImage"
[ -f "$APPIMAGE" ] || { echo "make_release.sh: the Linux build made no AppImage." >&2; exit 1; }
# Arch, CachyOS and the Steam Deck: the AppImage must run with a graphics renderer on a current Arch image.
"$REPO/tools/linux/arch_check.sh" "$APPIMAGE"
cp "$APPIMAGE" "$OUT/"

echo "== Site pictures"
# Write to a new file and move it over the old one, so that a removed picture does not stay.
SITE_TMP="$(mktemp "$OUT/site-assets.XXXXXX")"
( cd "$SITE" && zip -q -r -X - . -x '.*' ) > "$SITE_TMP"
mv -f "$SITE_TMP" "$OUT/site-assets.zip"

( cd "$OUT" && shasum -a 256 "$(basename "$DMG")" "$(basename "$SETUP")" "$(basename "$APPIMAGE")" site-assets.zip > SHA256SUMS )
printf '%s\n' "$NOTES" > "$OUT/notes.md"
echo "== Built in $OUT"
ls -la "$OUT"

if [ "$PUBLISH" != 1 ]; then
    echo "Dry run. Run again with --publish to tag $TAG and upload these files to $GH_REPO."
    exit 0
fi

PRE=()
case "$VERSION" in 0.*) PRE=(--prerelease) ;; esac
git -C "$REPO" tag -a "$TAG" -m "CM 01/02 Bosman Edition $VERSION"
git -C "$REPO" push origin "$TAG"
gh release create "$TAG" --repo "$GH_REPO" ${PRE[@]+"${PRE[@]}"} --title "CM 01/02 Bosman Edition $VERSION" \
    --notes-file "$OUT/notes.md" "$OUT/$(basename "$DMG")" "$OUT/$(basename "$SETUP")" "$OUT/$(basename "$APPIMAGE")" "$OUT/site-assets.zip" "$OUT/SHA256SUMS"
echo "Published $TAG."
