#!/bin/bash
# The game list run (game_list_run.py) with the Windows build under wine, inside a linux/amd64 container. The run
# needs root and network namespaces for the fake host's 50 addresses, so the container runs with --privileged.
#
#   docker run --rm --privileged --platform linux/amd64 -v <repo>:/src:ro -v <clean files>:/game:ro \
#       -v <build-win>:/win:ro -v <out folder>:/out ubuntu:22.04 bash /src/tests/smoke/game_list_wine.sh
#
# Wrap the docker run in build-slot.sh: it counts as a build. The out folder must be under /Users.
set -uo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq && apt-get install -y -qq --no-install-recommends wine64 python3 iproute2 2>&1 | tail -1
export PATH=/usr/lib/wine:$PATH WINEDEBUG=-all WINEPREFIX=/tmp/wp SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
wine64 wineboot -i >/dev/null 2>&1; echo "wine $(wine64 --version) wineboot $?" | tee /out/wine.txt
printf '#!/bin/sh\nexec wine64 /win/cm0102_recomp.exe "$@"\n' > /tmp/cm0102; chmod +x /tmp/cm0102
python3 -I /src/tests/smoke/game_list_run.py /tmp/cm0102 /game --shots /out/shots --slow 4 "$@" > /out/game-list.log 2>&1
echo "game list run exit $?" | tee -a /out/game-list.log
