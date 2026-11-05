# The browser build of the recompiled game (web/README.md), included by CMakeLists.txt under Emscripten:
#   emcmake cmake -S . -B build-web -DCM0102_EXE=<clean files>/cm0102.exe -DRECOMP_PYTHON=python3
#   cmake --build build-web -j 8 --target cm0102_recomp seasons_rules
# It writes build-web/cm0102_recomp.js and cm0102_recomp.wasm, and the season tables in build-web/seasons, which
# web/server.mjs serves to the page.

set(CM0102_WEB_SUSPEND "jspi" CACHE STRING
    "How the game hands time to the page: jspi (Chrome 137 and later) or asyncify (every browser, larger and slower)")
set_property(CACHE CM0102_WEB_SUSPEND PROPERTY STRINGS jspi asyncify)

# <SDL.h> and the SDL2 library come from Emscripten's port: it draws on the page's canvas, takes its input and plays
# its sound.
target_compile_options(cm0102_recomp PRIVATE -sUSE_SDL=2)

set(CM0102_WEB_LINK
    -sUSE_SDL=2
    -sENVIRONMENT=web
    -sINVOKE_RUN=0                        # the page starts main once the game files are in the file system
    -sEXIT_RUNTIME=1
    -sFORCE_FILESYSTEM=1
    -lidbfs.js                            # the user folder (saves, settings.txt) is kept in the browser
    "-sEXPORTED_RUNTIME_METHODS=callMain,FS,ENV,addRunDependency,removeRunDependency"
    -sGLOBAL_BASE=553648128               # 0x21000000: the host's data in the free gap of guest memory (host/web.c)
    -sINITIAL_MEMORY=805306368            # 768 MB; host/web.c grows the memory to the guest heap at 0x70000000
    -sALLOW_MEMORY_GROWTH=1
    -sMAXIMUM_MEMORY=4GB
    -sSTACK_SIZE=8MB                      # every call of the original is a host call (CMakeLists.txt gives 256 MB on a Mac)
    -O2
    --profiling-funcs                     # names of the original's functions in the browser's stack traces
)
if(CM0102_WEB_SUSPEND STREQUAL "asyncify")
    list(APPEND CM0102_WEB_LINK -sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576)
else()
    list(APPEND CM0102_WEB_LINK -sJSPI)
endif()
target_link_options(cm0102_recomp PRIVATE ${CM0102_WEB_LINK})
set_target_properties(cm0102_recomp PROPERTIES SUFFIX ".js")
