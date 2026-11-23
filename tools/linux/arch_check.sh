#!/bin/bash
# Runs the Linux AppImage on a current Arch Linux image with Mesa, under X11 (Xvfb) and under Wayland
# (weston, headless). Arch, CachyOS and SteamOS 3 (the Steam Deck) are Arch-based. The game must open its
# window with a graphics renderer (OpenGL or OpenGL ES, not the software one), run for a while and quit on
# SIGTERM. The 0.6.0 AppImage bundled the SDL2 of Ubuntu 22.04 (2.0.20), which crashed (SIGSEGV) right after
# "window opened" on every GL renderer (open item 3 of 0.6.1); the Ubuntu container checks did not see it,
# because they use the SDL dummy drivers.
#
# Usage:
#   tools/linux/arch_check.sh <AppImage> [--image <docker image>]
#
#   --image  the Arch image to start from. Default: archlinux:latest (pulled again on each run, so the
#            check uses the current Arch packages).
#
# The AppImage runtime does not start under the amd64 emulation of Colima, so the check extracts the squashfs
# part and starts its AppRun, the launcher the runtime starts.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APPIMAGE=""; IMAGE="archlinux:latest"
while [ $# -gt 0 ]; do
    case "$1" in
        --image) IMAGE="${2:-}"; shift 2 ;;
        -*) echo "arch_check.sh: unknown option $1" >&2; exit 2 ;;
        *) APPIMAGE="$1"; shift ;;
    esac
done
[ -f "$APPIMAGE" ] || { echo "arch_check.sh: give the AppImage." >&2; exit 2; }
command -v docker >/dev/null || { echo "arch_check.sh: Docker is not installed." >&2; exit 2; }
APPIMAGE="$(cd "$(dirname "$APPIMAGE")" && pwd)/$(basename "$APPIMAGE")"

# The squashfs part starts after the runtime's ELF section headers.
OFFSET="$(python3 -I - "$APPIMAGE" <<'PY'
import struct, sys
d = open(sys.argv[1], "rb").read(64)
shoff, = struct.unpack_from("<Q", d, 0x28)
shentsize, shnum = struct.unpack_from("<HH", d, 0x3A)
print(shoff + shentsize * shnum)
PY
)"

[ "$IMAGE" = archlinux:latest ] && docker pull --platform linux/amd64 -q "$IMAGE" >/dev/null
docker run --rm --platform linux/amd64 -v "$APPIMAGE:/app.AppImage:ro" -v "$REPO/tools/linux:/tools:ro" \
    -e OFFSET="$OFFSET" "$IMAGE" bash /tools/arch_check_in_container.sh
