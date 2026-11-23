#!/bin/bash
# Runs inside the Arch container of arch_check.sh. Mounts: /app.AppImage (read-only), /tools (this folder).
# OFFSET is the start of the squashfs part in the AppImage.
set -euo pipefail
# pacman's sandbox user does not start under the amd64 emulation of Colima.
grep -qx 'DisableSandbox' /etc/pacman.conf || sed -i '/^\[options\]/a DisableSandbox' /etc/pacman.conf
# The packages of an Arch desktop that the game and its SDL2 use. No sdl2 package: the check is of the AppImage's own copy.
pacman -Syu --noconfirm --needed --noprogressbar mesa libglvnd xorg-server-xvfb weston alsa-lib libpulse \
    libdecor libxkbcommon wayland libx11 libxext libxcursor libxinerama libxi libxfixes libxrandr libxss \
    libxxf86vm libdrm squashfs-tools >/dev/null
echo "check: $(pacman -Q mesa weston | tr '\n' ' ')"

unsquashfs -q -d /app -o "$OFFSET" /app.AppImage >/dev/null
export SDL_AUDIODRIVER=dummy XDG_RUNTIME_DIR=/tmp/xdg
mkdir -p "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"
FAIL=0

# One run: start the game, wait for its first display report, let it run, then SIGTERM and wait for the
# report it writes at the quit. $1 is the name of the run; the environment names the display.
run() {
    local name="$1" home="/tmp/home-$1" log="/tmp/$1.log" pid i
    rm -rf "$home"; mkdir -p "$home"
    HOME="$home" /app/AppRun > "$log" 2>&1 & pid=$!
    for i in $(seq 1 240); do
        grep -q "display report written (at start)" "$log" && break
        kill -0 "$pid" 2>/dev/null || break
        sleep 1
    done
    sleep 20
    if ! kill -0 "$pid" 2>/dev/null; then
        wait "$pid" && rc=0 || rc=$?
        echo "check: $name FAIL: the game stopped (exit $rc) before the quit"; tail -n 5 "$log"; FAIL=1; return
    fi
    kill -TERM "$pid"; wait "$pid" && rc=0 || rc=$?
    local report="$home/.local/share/cm0102/display-report.txt"
    local sdl renderer
    sdl="$(grep -m1 '^SDL ' "$report" 2>/dev/null || true)"
    renderer="$(grep -m1 '^Renderer ' "$report" 2>/dev/null || true)"
    echo "check: $name: $sdl; $renderer"
    if [ "$rc" != 0 ] || ! grep -q "display report written (at quit)" "$log"; then
        echo "check: $name FAIL: no clean quit (exit $rc)"; tail -n 5 "$log"; FAIL=1
    elif [ -z "$renderer" ] || [[ "$renderer" == *software* ]]; then
        echo "check: $name FAIL: no graphics renderer in the display report"; FAIL=1
    else
        echo "check: $name PASS"
    fi
}

# X11: Xvfb with Mesa (llvmpipe). Game Mode on the Steam Deck (gamescope) gives the game an X11 display too.
Xvfb :1 -screen 0 1920x1080x24 >/dev/null 2>&1 &
XPID=$!
sleep 3
DISPLAY=:1 run x11
kill "$XPID" 2>/dev/null || true

# Wayland: weston headless with its GL renderer. The game's SDL2 is told to use Wayland, as on a Wayland desktop
# where the player sets SDL_VIDEODRIVER=wayland (SDL2 takes X11 first when Xwayland is there).
weston --backend=headless --renderer=gl --socket=wayland-1 --width=1920 --height=1080 >/tmp/weston.log 2>&1 &
WPID=$!
for i in $(seq 1 60); do [ -S "$XDG_RUNTIME_DIR/wayland-1" ] && break; sleep 1; done
if [ -S "$XDG_RUNTIME_DIR/wayland-1" ]; then
    WAYLAND_DISPLAY=wayland-1 SDL_VIDEODRIVER=wayland run wayland
else
    echo "check: wayland FAIL: weston did not start"; tail -n 5 /tmp/weston.log; FAIL=1
fi
kill "$WPID" 2>/dev/null || true

[ "$FAIL" = 0 ] && echo "check: Arch PASS" || { echo "check: Arch FAIL"; exit 1; }
