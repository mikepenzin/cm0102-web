#!/bin/sh
# Builds the SDL probe (web/test/sdlprobe.c) for web/test/sdl-check.cjs: SDL2's sources compiled with emcc, then the
# probe linked as the page expects the engine (cm0102_recomp.js and .wasm in the out folder).
#
#   web/test/build-probe.sh <SDL2 source folder> <out folder>
#
# The SDL2 source folder is an unpacked release of libsdl.org (SDL2-2.32.10 here). Only its C files are compiled; none
# of its build scripts run. The file list is the one of Emscripten's own SDL2 port, for the files that this release
# has. Needs emcc on the PATH (any version with Asyncify).
set -eu
SDL=$1
OUT=$2
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT/obj"

SRCS="SDL.c SDL_assert.c SDL_dataqueue.c SDL_error.c SDL_guid.c SDL_hints.c SDL_list.c SDL_log.c SDL_utils.c
atomic/SDL_atomic.c atomic/SDL_spinlock.c audio/SDL_audio.c audio/SDL_audiocvt.c audio/SDL_audiodev.c
audio/SDL_audiotypecvt.c audio/SDL_mixer.c audio/SDL_wave.c cpuinfo/SDL_cpuinfo.c dynapi/SDL_dynapi.c
events/SDL_clipboardevents.c events/SDL_displayevents.c events/SDL_dropevents.c events/SDL_events.c
events/SDL_gesture.c events/SDL_keyboard.c events/SDL_keysym_to_scancode.c events/SDL_mouse.c events/SDL_quit.c
events/SDL_scancode_tables.c events/SDL_touch.c events/SDL_windowevents.c file/SDL_rwops.c haptic/SDL_haptic.c
joystick/SDL_gamecontroller.c joystick/SDL_joystick.c joystick/SDL_steam_virtual_gamepad.c joystick/controller_type.c
power/SDL_power.c render/SDL_d3dmath.c render/SDL_render.c render/SDL_yuv_sw.c
render/opengles2/SDL_render_gles2.c render/opengles2/SDL_shaders_gles2.c
render/software/SDL_blendfillrect.c render/software/SDL_blendline.c render/software/SDL_blendpoint.c
render/software/SDL_drawline.c render/software/SDL_drawpoint.c render/software/SDL_render_sw.c
render/software/SDL_rotate.c render/software/SDL_triangle.c sensor/SDL_sensor.c sensor/dummy/SDL_dummysensor.c
stdlib/SDL_crc16.c stdlib/SDL_crc32.c stdlib/SDL_getenv.c stdlib/SDL_iconv.c stdlib/SDL_malloc.c stdlib/SDL_mslibc.c
stdlib/SDL_qsort.c stdlib/SDL_stdlib.c stdlib/SDL_string.c stdlib/SDL_strtokr.c thread/SDL_thread.c
timer/SDL_timer.c video/SDL_RLEaccel.c video/SDL_blit.c video/SDL_blit_0.c video/SDL_blit_1.c video/SDL_blit_A.c
video/SDL_blit_N.c video/SDL_blit_auto.c video/SDL_blit_copy.c video/SDL_blit_slow.c video/SDL_bmp.c
video/SDL_clipboard.c video/SDL_egl.c video/SDL_fillrect.c video/SDL_pixels.c video/SDL_rect.c video/SDL_shape.c
video/SDL_stretch.c video/SDL_surface.c video/SDL_video.c video/SDL_yuv.c
video/emscripten/SDL_emscriptenevents.c video/emscripten/SDL_emscriptenframebuffer.c
video/emscripten/SDL_emscriptenmouse.c video/emscripten/SDL_emscriptenopengles.c
video/emscripten/SDL_emscriptenvideo.c audio/emscripten/SDL_emscriptenaudio.c video/dummy/SDL_nullevents.c
video/dummy/SDL_nullframebuffer.c video/dummy/SDL_nullvideo.c video/yuv2rgb/yuv_rgb_std.c video/yuv2rgb/yuv_rgb.c
audio/disk/SDL_diskaudio.c audio/dummy/SDL_dummyaudio.c loadso/dlopen/SDL_sysloadso.c
power/emscripten/SDL_syspower.c joystick/emscripten/SDL_sysjoystick.c filesystem/emscripten/SDL_sysfilesystem.c
timer/unix/SDL_systimer.c haptic/dummy/SDL_syshaptic.c main/dummy/SDL_dummy_main.c locale/SDL_locale.c
locale/emscripten/SDL_syslocale.c misc/SDL_url.c misc/emscripten/SDL_sysurl.c
thread/generic/SDL_syscond.c thread/generic/SDL_sysmutex.c thread/generic/SDL_syssem.c
thread/generic/SDL_systhread.c thread/generic/SDL_systls.c"

# EM_JS_DEPS only names JavaScript helpers to link in. An Emscripten older than this SDL expects (3.1.6 of Debian and
# Ubuntu, for one) does not have it; there it is defined away, and the helpers it names are always linked in.
OLD_EMCC=""
if ! printf '#include <emscripten.h>\n#ifndef EM_JS_DEPS\n#error none\n#endif\n' | emcc -E -x c - >/dev/null 2>&1; then
  OLD_EMCC="-DEM_JS_DEPS(tag,deps)="
fi

OBJS=""
for s in $SRCS; do
  [ -f "$SDL/src/$s" ] || continue                 # a file that this release of SDL does not have
  o="$OUT/obj/$(echo "$s" | tr '/' '_').o"
  [ -f "$o" ] || emcc -c "$SDL/src/$s" -o "$o" -I"$SDL/include" -O2 -w $OLD_EMCC
  OBJS="$OBJS $o"
done

# The link options of web/web.cmake that the page relies on (not the memory layout: the probe has no guest memory).
emcc "$HERE/sdlprobe.c" $OBJS -I"$SDL/include" -O1 -sASYNCIFY -sENVIRONMENT=web -sINVOKE_RUN=0 -sEXIT_RUNTIME=1 \
  -sFORCE_FILESYSTEM=1 -lidbfs.js -sALLOW_MEMORY_GROWTH=1 -sMAX_WEBGL_VERSION=2 \
  "-sEXPORTED_RUNTIME_METHODS=callMain,ccall,FS,ENV,addRunDependency,removeRunDependency" \
  "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=\$autoResumeAudioContext,\$dynCall" \
  -o "$OUT/cm0102_recomp.js"
ls -la "$OUT/cm0102_recomp.js" "$OUT/cm0102_recomp.wasm"
