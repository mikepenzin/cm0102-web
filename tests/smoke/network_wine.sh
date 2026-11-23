#!/bin/bash
# The two-game run (network_run.py) with the Windows build under wine, inside a linux/amd64 container, as the
# release checks run the Windows smoke test. Both games run in the container and meet on its own address.
#
#   docker run --rm --platform linux/amd64 -v <repo>:/src:ro -v <clean files>:/game:ro \
#       -v <build-win>:/win:ro -v <out folder>:/out ubuntu:22.04 bash /src/tests/smoke/network_wine.sh [--joiners n]
#
# Wrap the docker run in build-slot.sh: it counts as a build.
set -uo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq && apt-get install -y -qq --no-install-recommends wine64 python3 2>&1 | tail -1
export PATH=/usr/lib/wine:$PATH WINEDEBUG=-all WINEPREFIX=/tmp/wp SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
wine64 wineboot -i >/dev/null 2>&1; echo "wine $(wine64 --version) wineboot $?" | tee /out/wine.txt
printf '#!/bin/sh\nexec wine64 /win/cm0102_recomp.exe "$@"\n' > /tmp/cm0102; chmod +x /tmp/cm0102
python3 -I /src/tests/smoke/network_run.py /tmp/cm0102 /game --shots /out/shots --slow 4 "$@" > /out/network.log 2>&1
echo "network run exit $?" | tee -a /out/network.log
