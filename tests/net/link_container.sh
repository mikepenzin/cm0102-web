#!/bin/bash
# The cm0102:// link handler (0.7.0 slice 8) on Linux and on Windows under wine, in a linux/amd64 container:
#   docker run --rm --platform linux/amd64 -v <repo>:/src:ro -v <out folder>:/out ubuntu:22.04 bash /src/tests/net/link_container.sh
# 1. link_test and invite_test, built with gcc.
# 2. The desktop file passes desktop-file-validate, and xdg-open and gio open start the AppImage with the link.
# 3. link_test built with mingw-w64 under wine: the key in HKEY_CURRENT_USER, then reg query sees it.
# 4. link_pass_test (one copy takes the links, Q41) with gcc and under wine, in one process and in two.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends gcc libc6-dev gcc-mingw-w64-x86-64 wine64 desktop-file-utils \
    xdg-utils libglib2.0-bin >/dev/null 2>&1
S=/src/src/recomp_rt; T=/src/tests/net; W=/tmp/w; mkdir -p $W
log() { echo "$*" | tee -a /out/link-container.txt; }
: > /out/link-container.txt

# 1. Unit tests.
gcc -std=c11 -D_DEFAULT_SOURCE -D_GNU_SOURCE -I$S -I$S/win -o $W/invite_test $T/test_invite.c $S/win/invite.c \
    $S/win/lobby_http.c $S/win/lobby.c $S/host/net_posix.c -lpthread
gcc -std=c11 -D_DEFAULT_SOURCE -D_GNU_SOURCE -I$S -o $W/link_test $T/test_link.c $S/host/link.c $S/host/posix.c
log "linux invite_test: $($W/invite_test 2>/dev/null | tail -1)"
log "linux link_test: $($W/link_test | tail -1)"
gcc -std=c11 -D_DEFAULT_SOURCE -D_GNU_SOURCE -I$S -o $W/link_pass_test $T/test_link_pass.c $S/host/link_pass.c -lpthread
log "linux link_pass_test: $($W/link_pass_test | tail -1)"
# Two processes: one listens, a second gives it a link (Q41).
mkdir -p /tmp/u1; $W/link_pass_test serve /tmp/u1 > /tmp/serve.txt & SERVE=$!; sleep 1
PASSED=0; $W/link_pass_test pass /tmp/u1 cm0102://join/K7M2Q9RT || PASSED=$?; wait $SERVE || true
log "linux two processes: pass exit $PASSED, the first process: $(tr '\n' ' ' < /tmp/serve.txt)"
ALONE=0; $W/link_pass_test pass /tmp/u1 cm0102://join/K7M2Q9RT || ALONE=$?; log "linux with no first process: pass exit $ALONE (1: the link stays)"

# 2. A fake AppImage that writes its arguments, then the handler, then a click on the link.
export HOME=/tmp/home XDG_DATA_HOME=/tmp/home/.local/share; mkdir -p $HOME
mkdir -p "/tmp/My Games"; printf '#!/bin/sh\necho "$@" > /tmp/opened.txt\n' > "/tmp/My Games/CM.AppImage"
chmod +x "/tmp/My Games/CM.AppImage"
APPIMAGE="/tmp/My Games/CM.AppImage" $W/link_test --write
F=$XDG_DATA_HOME/applications/cm0102-bosman-edition.desktop
cat "$F" | tee -a /out/link-container.txt
desktop-file-validate "$F" && log "desktop-file-validate: ok"
log "xdg-mime query default: $(xdg-mime query default x-scheme-handler/cm0102)"
rm -f /tmp/opened.txt; XDG_UTILS_DEBUG_LEVEL=3 xdg-open "cm0102://join/K7M2Q9RT" >/out/xdg-open.txt 2>&1 || true; sleep 1
log "xdg-open gave the AppImage: $(cat /tmp/opened.txt 2>/dev/null || echo nothing)"
log "xdg-utils $(dpkg-query -W -f='${Version}' xdg-utils), its URL test: $(grep -m1 -o "egrep -q '[^']*'" /usr/bin/xdg-open)"
rm -f /tmp/opened.txt; XDG_CURRENT_DESKTOP=GNOME xdg-open "cm0102://join/K7M2Q9RT" >/dev/null 2>&1 || true; sleep 1
log "xdg-open on GNOME gave the AppImage: $(cat /tmp/opened.txt 2>/dev/null || echo nothing)"
rm -f /tmp/opened.txt; gio open "cm0102://join/K7M2Q9RT" >/dev/null 2>&1 || true; sleep 1
log "gio open gave the AppImage: $(cat /tmp/opened.txt 2>/dev/null || echo nothing)"

# 3. Windows under wine.
x86_64-w64-mingw32-gcc -std=c11 -I$S -Wall -o $W/link_test.exe $T/test_link.c $S/host/link.c
export PATH=/usr/lib/wine:$PATH WINEDEBUG=-all WINEPREFIX=/tmp/wp
wine64 wineboot -i >/dev/null 2>&1 || true
log "wine link_test: $(wine64 $W/link_test.exe 2>/dev/null | tr -d '\r')"
wine64 $W/link_test.exe --write >/dev/null 2>&1
x86_64-w64-mingw32-gcc -std=c11 -Wall -I$S -o $W/link_pass_test.exe $T/test_link_pass.c $S/host/link_pass.c
log "wine link_pass_test: $(wine64 $W/link_pass_test.exe 2>/dev/null | tr -d '\r' | tail -1)"
wine64 $W/link_pass_test.exe serve C:/u1 > /tmp/wserve.txt 2>/dev/null & WSERVE=$!; sleep 3
WPASSED=0; wine64 $W/link_pass_test.exe pass C:/u1 cm0102://join/K7M2Q9RT 2>/dev/null || WPASSED=$?; wait $WSERVE || true
log "wine two processes: pass exit $WPASSED, the first process: $(tr -d '\r' < /tmp/wserve.txt | tr '\n' ' ')"
WALONE=0; wine64 $W/link_pass_test.exe pass C:/u1 cm0102://join/K7M2Q9RT 2>/dev/null || WALONE=$?; log "wine with no first process: pass exit $WALONE (1: the link stays)"
log "wine reg query after a write:"
wine64 reg query 'HKCU\Software\Classes\cm0102' /s 2>/dev/null | tr -d '\r' | tee -a /out/link-container.txt
