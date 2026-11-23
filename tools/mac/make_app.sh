#!/bin/bash
# Makes "CM 01-02 Bosman Edition.app" and the .dmg (slice 5 of the release spec).
#
# Usage:
#   tools/mac/make_app.sh --game <folder> [--exe <cm0102.exe>] [--icns <file.icns>]
#       [--sdl2-arm64 <prefix>] [--sdl2-x86_64 <prefix>] [--out <folder>] [--version <x.y.z>]
#
#   --game          The folder with the clean files (cm0102.exe, Data, Pictures). It is copied into
#                   Contents/Resources/game. Required. For a local test use a scratch copy of the fan
#                   pack. Never put the fan pack in a download.
#   --exe           The cm0102.exe that the recompiler reads. Default: <game>/cm0102.exe.
#   --dmg-logo      Optional: the logo PNG for the .dmg window background (not in git).
#   --icns          Optional icon. The script copies it to Contents/Resources/AppIcon.icns. No icon is
#                   committed to git. Empty: the script makes the original's ball icon from --exe.
#   --sdl2-arm64    Prefix with lib/ and include/ of SDL2 for arm64. Default /opt/homebrew.
#   --sdl2-x86_64   Prefix of SDL2 built for x86_64. If given, the app is universal. If empty, the app is
#                   arm64 only and the script says so.
#   --out           Output folder. Default: build/mac in the repo.
#   --version       Default: the VERSION in project() of CMakeLists.txt.
#
# Old output is moved to the Trash, never deleted.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GAME=""; EXE=""; ICNS=""; DMG_LOGO=""; SDL_ARM64="/opt/homebrew"; SDL_X86_64=""; OUT="$REPO/build/mac"; VERSION=""
while [ $# -gt 0 ]; do
    case "$1" in
        --game) GAME="${2:-}"; shift 2 ;;
        --exe) EXE="${2:-}"; shift 2 ;;
        --icns) ICNS="${2:-}"; shift 2 ;;
        --dmg-logo) DMG_LOGO="${2:-}"; shift 2 ;;
        --sdl2-arm64) SDL_ARM64="${2:-}"; shift 2 ;;
        --sdl2-x86_64) SDL_X86_64="${2:-}"; shift 2 ;;
        --out) OUT="${2:-}"; shift 2 ;;
        --version) VERSION="${2:-}"; shift 2 ;;
        *) echo "make_app.sh: unknown option $1" >&2; exit 2 ;;
    esac
done

if [ -z "$VERSION" ]; then
    VERSION="$(sed -n 's/^project([^)]*VERSION \([0-9][0-9.]*\).*/\1/p' "$REPO/CMakeLists.txt" | head -n 1)"
fi
[ -n "$VERSION" ] || { echo "make_app.sh: cannot find the version." >&2; exit 2; }
[ -n "$GAME" ] || { echo "make_app.sh: --game <folder with the clean files> is required." >&2; exit 2; }
[ -f "$GAME/cm0102.exe" ] && [ -d "$GAME/Data" ] || {
    echo "make_app.sh: $GAME must hold cm0102.exe and Data." >&2; exit 2; }
GAME="$(cd "$GAME" && pwd)"
[ -n "$EXE" ] || EXE="$GAME/cm0102.exe"
[ -f "$EXE" ] || { echo "make_app.sh: $EXE not found." >&2; exit 2; }
if [ -n "$ICNS" ] && [ ! -f "$ICNS" ]; then echo "make_app.sh: icon $ICNS not found." >&2; exit 2; fi
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

APP_NAME="CM 01-02 Bosman Edition"
APP="$OUT/$APP_NAME.app"
DMG="$OUT/CM-01-02-Bosman-Edition-$VERSION-mac.dmg"
STAMP="$(date +%Y%m%d-%H%M%S)"

trash() {  # Move a path to the Trash with a time stamp.
    [ -e "$1" ] || return 0
    mkdir -p "$HOME/.Trash"
    mv "$1" "$HOME/.Trash/$(basename "$1").$STAMP"
}

# 1. One build per architecture.
ARCHS=(arm64)
[ -n "$SDL_X86_64" ] && ARCHS+=(x86_64)
if [ -z "$SDL_X86_64" ]; then
    echo "make_app.sh: no --sdl2-x86_64 prefix, so the app is arm64 only (not universal)." >&2
fi
prefix_for() { if [ "$1" = arm64 ]; then echo "$SDL_ARM64"; else echo "$SDL_X86_64"; fi; }
for a in "${ARCHS[@]}"; do
    B="$OUT/work-$a"
    # SDL2_DIR names the exact SDL2 for this architecture, so that no other copy is found first.
    cmake -S "$REPO" -B "$B" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$a" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_PREFIX_PATH="$(prefix_for "$a")" \
        -DSDL2_DIR="$(prefix_for "$a")/lib/cmake/SDL2" -DCM0102_EXE="$EXE" >/dev/null
    cmake --build "$B" -j --target cm0102_recomp
done

# 2. Bundle the libraries of each architecture. A library outside /usr/lib and /System is copied to
# Contents/Frameworks and every reference to it points there.
bundle_deps() {  # <thin binary> <frameworks folder> <prefix>
    local bin="$1" fw="$2" prefix="$3" dep base src
    mkdir -p "$fw"
    while IFS= read -r dep; do
        case "$dep" in /usr/lib/*|/System/*|"") continue ;; esac
        base="$(basename "$dep")"
        case "$dep" in
            @rpath/*|@loader_path/*|@executable_path/*) src="$prefix/lib/$base" ;;
            *) src="$dep" ;;
        esac
        if [ ! -f "$fw/$base" ]; then
            [ -f "$src" ] || { echo "make_app.sh: cannot find $dep" >&2; exit 1; }
            cp -L "$src" "$fw/$base"
            chmod u+w "$fw/$base"
            install_name_tool -id "@rpath/$base" "$fw/$base"
            bundle_deps "$fw/$base" "$fw" "$prefix"
            # sdl2-compat loads libSDL3.dylib with dlopen, so otool -L does not list it.
            if [ "$(strings -a "$fw/$base" | grep -c 'libSDL3\.dylib')" -gt 0 ]; then
                if [ ! -f "$fw/libSDL3.dylib" ]; then
                    [ -f "$prefix/lib/libSDL3.dylib" ] || { echo "make_app.sh: sdl2-compat needs $prefix/lib/libSDL3.dylib" >&2; exit 1; }
                    cp -L "$prefix/lib/libSDL3.dylib" "$fw/libSDL3.dylib"
                    chmod u+w "$fw/libSDL3.dylib"
                    install_name_tool -id "@rpath/libSDL3.dylib" "$fw/libSDL3.dylib"
                    bundle_deps "$fw/libSDL3.dylib" "$fw" "$prefix"
                fi
            fi
        fi
        install_name_tool -change "$dep" "@rpath/$base" "$bin"
    done < <(otool -L "$bin" | tail -n +2 | awk '{print $1}')
}

THIN="$OUT/thin"
mkdir -p "$THIN"
for a in "${ARCHS[@]}"; do
    d="$(mktemp -d "$THIN/$a.XXXXXX")"
    cp "$OUT/work-$a/cm0102_recomp" "$d/cm0102_recomp"
    bundle_deps "$d/cm0102_recomp" "$d/Frameworks" "$(prefix_for "$a")"
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$d/cm0102_recomp"
    # SDL2 builds that start in @loader_path need the same for the frameworks themselves.
    for f in "$d"/Frameworks/*.dylib; do install_name_tool -add_rpath "@loader_path" "$f" 2>/dev/null || true; done
    if [ "$a" = arm64 ]; then THIN_arm64="$d"; else THIN_x86_64="$d"; fi
done

# 3. The bundle.
trash "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
if [ ${#ARCHS[@]} -eq 2 ]; then
    lipo -create "$THIN_arm64/cm0102_recomp" "$THIN_x86_64/cm0102_recomp" -output "$APP/Contents/MacOS/cm0102_recomp"
    for f in "$THIN_arm64"/Frameworks/*.dylib; do
        [ -e "$f" ] || continue
        n="$(basename "$f")"
        if [ ! -f "$THIN_x86_64/Frameworks/$n" ]; then
            echo "make_app.sh: $n is only in the arm64 build." >&2; exit 1
        fi
        lipo -create "$f" "$THIN_x86_64/Frameworks/$n" -output "$APP/Contents/Frameworks/$n"
    done
    for f in "$THIN_x86_64"/Frameworks/*.dylib; do
        [ -e "$f" ] || continue
        n="$(basename "$f")"
        [ -f "$THIN_arm64/Frameworks/$n" ] || echo "make_app.sh: warning: $n is only in the x86_64 build, so it is left out." >&2
    done
else
    cp "$THIN_arm64/cm0102_recomp" "$APP/Contents/MacOS/cm0102_recomp"
    for f in "$THIN_arm64"/Frameworks/*.dylib; do
        [ -e "$f" ] || continue
        cp "$f" "$APP/Contents/Frameworks/"
    done
fi

sed -e "s/@VERSION@/$VERSION/g" "$REPO/tools/mac/Info.plist.in" > "$APP/Contents/Info.plist"
if [ -z "$ICNS" ]; then  # The ball icon from the player's own exe (tools/icon/make_icons.py).
    python3 -I "$REPO/tools/icon/make_icons.py" "$EXE" "$OUT/icons" >/dev/null
    ICNS="$OUT/icons/cm0102.icns"
fi
if [ -n "$ICNS" ]; then
    cp "$ICNS" "$APP/Contents/Resources/AppIcon.icns"
    /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string AppIcon" "$APP/Contents/Info.plist"
fi
printf 'APPL????' > "$APP/Contents/PkgInfo"

rsync -a --exclude .DS_Store "$GAME/" "$APP/Contents/Resources/game/"
# The licences of the libraries in the program (mbedTLS, the secure connection of online play).
mkdir -p "$APP/Contents/Resources/licences"
cp "$REPO/packaging/licences/"*.txt "$APP/Contents/Resources/licences/"

# 4. Ad hoc signature, inside out.
for f in "$APP"/Contents/Frameworks/*.dylib; do codesign --force --sign - "$f"; done
codesign --force --sign - "$APP/Contents/MacOS/cm0102_recomp"
codesign --force --sign - "$APP"
codesign --verify --strict "$APP"

# 5. The .dmg with a link to Applications. With dmgbuild (pip, in the venv) the window gets the
#    background picture (tools/mac/dmg_background.py) and a fixed layout (tools/mac/dmg_settings.py).
#    Without it, a plain .dmg.
trash "$DMG"
PY="$HOME/.venvs/cm0102/bin/python"
[ -x "$PY" ] || PY=python3
if "$PY" -c "import dmgbuild, PIL" 2>/dev/null; then
    BG="$OUT/dmg-background.tiff"
    LOGO_ARGS=()
    [ -n "$DMG_LOGO" ] && LOGO_ARGS=(--logo "$DMG_LOGO")
    "$PY" "$REPO/tools/mac/dmg_background.py" "$BG" ${LOGO_ARGS[@]+"${LOGO_ARGS[@]}"} >/dev/null
    "$PY" -m dmgbuild -s "$REPO/tools/mac/dmg_settings.py" -D app="$APP" -D background="$BG" \
        "$APP_NAME $VERSION" "$DMG" >/dev/null
else
    echo "make_app.sh: dmgbuild or Pillow is missing, so the .dmg window has no background." >&2
    STAGE="$(mktemp -d "$OUT/dmg.XXXXXX")"
    cp -R "$APP" "$STAGE/"
    ln -s /Applications "$STAGE/Applications"
    hdiutil create -volname "$APP_NAME $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
    echo "Staging folder $STAGE can be moved to the Trash."
fi
echo "App: $APP"
echo "Dmg: $DMG"
