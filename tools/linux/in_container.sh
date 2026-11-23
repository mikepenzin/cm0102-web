#!/bin/bash
# Runs inside the linux/amd64 container of make_appimage.sh. Mounts: /src (repo, read-only), /game (clean
# files, read-only), /out (output), /manifest.tsv (the manifest to check against), /smoke-ref (reference pictures, only with --smoke).
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
# No libsdl2-dev: the game is built against the SDL2 of step 1, not Ubuntu's 2.0.20. The -dev packages are the
# headers SDL2 needs for its video and sound drivers; SDL2 loads their libraries at run time (dlopen).
apt-get install -y -qq --no-install-recommends build-essential cmake ninja-build pkg-config python3 python3-venv \
    python3-pip wget ca-certificates file squashfs-tools zstd \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxss-dev libxxf86vm-dev \
    libxfixes-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev libgl-dev \
    libgles-dev libdrm-dev libgbm-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev >/dev/null
python3 -m venv /venv
/venv/bin/pip install -q capstone

# 1. SDL2 from its source release (pinned by hash), kept in the build volume. Ubuntu 22.04's SDL2 is 2.0.20, and
# its SDL_SetTextureScaleMode calls the renderer on a texture with no renderer data when the format (the game's
# RGB565) is not native to the renderer. Every OpenGL and OpenGL ES renderer is such a case, so the 0.6.0
# AppImage crashed after "window opened" on Arch, CachyOS and the Steam Deck (open item 3 of 0.6.1). SDL 2.0.22
# fixed it. 2.32.10 is the newest SDL2 and the one the Windows build uses.
SDL_VER=2.32.10
SDLP="/build/sdl2-$SDL_VER"
if [ ! -f "$SDLP/lib/libSDL2-2.0.so.0" ]; then
    mkdir -p /work && cd /work
    wget -q -O sdl2.tar.gz "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-$SDL_VER.tar.gz"
    echo "5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165  sdl2.tar.gz" | sha256sum -c -
    tar xzf sdl2.tar.gz
    cmake -S "SDL2-$SDL_VER" -B sdl2-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$SDLP" \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF >/dev/null
    cmake --build sdl2-build
    cmake --install sdl2-build >/dev/null
    cd /
fi

# 2. Build. The source is read-only, so the build folder is in the container. No RPATH: AppRun names the
# folder of the bundled SDL2.
cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCM0102_EXE=/game/cm0102.exe \
    -DRECOMP_PYTHON=/venv/bin/python -DSDL2_DIR="$SDLP/lib/cmake/SDL2" -DCMAKE_SKIP_RPATH=ON >/dev/null
cmake --build /build --target cm0102_recomp

# 3. The AppDir: program, SDL2, clean files, launcher, desktop file, icon and .DirIcon (the AppImage
# catalogue requires it: https://docs.appimage.org/reference/appdir.html).
A=/work/CM-01-02.AppDir
mkdir -p "$A/usr/bin" "$A/usr/lib" "$A/usr/share/game"
cp /build/cm0102_recomp "$A/usr/bin/"
strip "$A/usr/bin/cm0102_recomp"
cp -L "$SDLP/lib/libSDL2-2.0.so.0" "$A/usr/lib/"
cp -r /game/. "$A/usr/share/game/"
mkdir -p "$A/usr/share/licences"                     # the licences of the libraries in the program (mbedTLS)
cp /src/packaging/licences/*.txt "$A/usr/share/licences/"
cat > "$A/AppRun" <<'RUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/cm0102_recomp" --game "$HERE/usr/share/game" "$@"
RUN
chmod +x "$A/AppRun"
cat > "$A/cm0102.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=CM 01/02 Bosman Edition
Exec=AppRun
Icon=cm0102
Categories=Game;
DESK
# A plain placeholder icon (the original's icon is not in git).
python3 - "$A/cm0102.png" <<'PY'
import struct, sys, zlib
w = 64
raw = b"".join(b"\0" + bytes([30, 90, 50]) * w for _ in range(w))
def ch(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
open(sys.argv[1], "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, w, 8, 2, 0, 0, 0))
                              + ch(b"IDAT", zlib.compress(raw)) + ch(b"IEND", b""))
PY
ln -s cm0102.png "$A/.DirIcon"

# 4. The AppImage: the type 2 runtime (pinned by hash), then the squashfs of the AppDir. appimagetool is
# not used, because it does not run under the Rosetta of the container.
cd /work
wget -q -O runtime https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64
echo "2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d  runtime" | sha256sum -c -
# The AppImage catalogue reads the name of the program up to "-<version>" and wants no "linux" in it (every
# AppImage is for Linux). So the name has no dash between CM and 0102, as the Windows installer.
NAME="CM0102-Bosman-Edition-$VERSION-x86_64.AppImage"
mksquashfs "$A" /work/app.sqfs -root-owned -noappend -comp zstd -quiet
cat runtime app.sqfs > "/out/$NAME"
chmod +x "/out/$NAME"

# 5. Check: x86-64 ELF, the files the catalogue needs at the top, and every game file matches the manifest, none missing or extra.
file "/out/$NAME" | grep -q "x86-64" || { echo "check: not an x86-64 AppImage" >&2; exit 1; }
rm -rf /work/x && mkdir -p /work/x && unsquashfs -q -d /work/x/squashfs-root -o "$(stat -c %s /work/runtime)" "/out/$NAME" >/dev/null
for f in AppRun cm0102.desktop cm0102.png .DirIcon; do
    [ -e "/work/x/squashfs-root/$f" ] || { echo "check: the AppImage has no $f" >&2; exit 1; }
done
# The program finds the bundled SDL2 with AppRun's library path, and that copy is the one of step 1.
X=/work/x/squashfs-root
LD_LIBRARY_PATH="$X/usr/lib" ldd "$X/usr/bin/cm0102_recomp" | grep -q "=> $X/usr/lib/libSDL2-2.0.so.0" \
    || { echo "check: the program does not take the bundled SDL2" >&2; exit 1; }
cmp -s "$X/usr/lib/libSDL2-2.0.so.0" "$SDLP/lib/libSDL2-2.0.so.0" || { echo "check: the bundled SDL2 is not $SDL_VER" >&2; exit 1; }
echo "check: SDL2 $SDL_VER bundled"
python3 -I - /manifest.tsv /work/x/squashfs-root/usr/share/game <<'PY'
import hashlib, os, sys
want = {}
for l in open(sys.argv[1]):
    if l.startswith("#") or not l.strip(): continue
    h, n, p = l.rstrip("\n").split("\t", 2)
    want[p] = (h, int(n))
have = {}
for d, _, fs in os.walk(sys.argv[2]):
    for f in fs:
        full = os.path.join(d, f)
        have[os.path.relpath(full, sys.argv[2])] = full
bad = [p for p in want if p not in have]
bad += [p for p in have if p not in want]
bad += [p for p in want if p in have and (os.path.getsize(have[p]) != want[p][1]
        or hashlib.sha256(open(have[p], "rb").read()).hexdigest() != want[p][0])]
if bad:
    print("check: files differ from the manifest:", bad[:10]); sys.exit(1)
print("check: %d game files match the manifest" % len(want))
PY
ls -la "/out/$NAME"

# 6. Smoke run with the SDL dummy drivers (only with --smoke), with the bundled SDL2 as AppRun gives it.
if [ -n "${SMOKE:-}" ]; then
    LD_LIBRARY_PATH="$X/usr/lib" python3 -I /src/tests/smoke/smoke_run.py /work/x/squashfs-root/usr/bin/cm0102_recomp /game /smoke-ref --mode classic --tolerance 1
fi
