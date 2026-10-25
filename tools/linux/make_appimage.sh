#!/bin/bash
# Makes the Linux download: one x86-64 AppImage with the clean files. It runs in a linux/amd64 container
# (Docker, Colima or OrbStack), so the build does not depend on the host. The same AppImage runs on
# the Steam Deck in its desktop mode.
#
# Usage:
#   tools/linux/make_appimage.sh --game <clean files> [--manifest <file>] [--out <folder>] [--smoke <reference folder>]
#
#   --game   the clean files (tools/clean_files/make_clean_files.py). The container checks the
#            AppImage against the manifest.
#   --manifest  the manifest to check against. Default: tools/clean_files/manifest.tsv. Give the
#            merged download manifest when --game holds the Data-3.9.60 and Data-3.9.65 folders.
#   --out    output folder. Default: build/linux in the repo.
#   --smoke  also run tests/smoke/smoke_run.py in the container (classic mode, SDL dummy drivers)
#            against the reference pictures in this folder.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GAME=""; MANIFEST=""; OUT="$REPO/build/linux"; SMOKE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --game) GAME="${2:-}"; shift 2 ;;
        --manifest) MANIFEST="${2:-}"; shift 2 ;;
        --out) OUT="${2:-}"; shift 2 ;;
        --smoke) SMOKE="${2:-}"; shift 2 ;;
        *) echo "make_appimage.sh: unknown option $1" >&2; exit 2 ;;
    esac
done
[ -f "$GAME/cm0102.exe" ] && [ -d "$GAME/Data" ] || { echo "make_appimage.sh: give --game <clean files>." >&2; exit 2; }
command -v docker >/dev/null || { echo "make_appimage.sh: Docker is not installed." >&2; exit 2; }
# The memory of Docker's VM. The build peaked at 4.0 GiB in the container (25 October 2026: recomp.py 1.6 GB, then
# ninja's parallel compile of the 128 shards), and the release dry run of 0.6.0 was killed at the recomp step when
# recomp.py held 8.9 GB in an 8 GiB VM. Stop early with a clear message when the VM has less than 6 GiB. Colima:
# `colima start --arch aarch64 --cpu 6 --memory 8 --vm-type vz --vz-rosetta` (8 GiB shows as 7.7 GiB).
MIN_GIB=6
MEM=$(docker info --format '{{.MemTotal}}' 2>/dev/null || echo 0)
if [ "${MEM:-0}" -lt $((MIN_GIB * 1024 * 1024 * 1024)) ]; then
    echo "make_appimage.sh: Docker's VM has $((MEM / 1024 / 1024)) MiB of memory; the Linux build needs at least $MIN_GIB GiB." >&2
    echo "make_appimage.sh: with Colima: colima stop, then colima start --arch aarch64 --cpu 6 --memory 8 --vm-type vz --vz-rosetta" >&2
    exit 2
fi
GAME="$(cd "$GAME" && pwd)"
[ -n "$MANIFEST" ] || MANIFEST="$REPO/tools/clean_files/manifest.tsv"
[ -f "$MANIFEST" ] || { echo "make_appimage.sh: no manifest file $MANIFEST." >&2; exit 2; }
MANIFEST="$(cd "$(dirname "$MANIFEST")" && pwd)/$(basename "$MANIFEST")"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
VERSION="$(sed -n 's/^project([^)]*VERSION \([0-9][0-9.]*\).*/\1/p' "$REPO/CMakeLists.txt" | head -n 1)"

MOUNTS=(-v "$REPO:/src:ro" -v "$GAME:/game:ro" -v "$OUT:/out" -v "$MANIFEST:/manifest.tsv:ro" -v cm0102-linux-build:/build)
[ -n "$SMOKE" ] && MOUNTS+=(-v "$(cd "$SMOKE" && pwd):/smoke-ref:ro")
docker run --rm --platform linux/amd64 "${MOUNTS[@]}" -e VERSION="$VERSION" -e SMOKE="${SMOKE:+1}" \
    ubuntu:22.04 bash /src/tools/linux/in_container.sh
echo "== $OUT/CM0102-Bosman-Edition-$VERSION-x86_64.AppImage"
