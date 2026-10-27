# Toolchain for the 64-bit Windows build on the Mac with mingw-w64 (Homebrew).
# cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=packaging/windows/mingw-w64-x86_64.cmake \
#       -DSDL2_DIR=<SDL2-devel-2.x-mingw>/x86_64-w64-mingw32/lib/cmake/SDL2 -DCM0102_EXE=<cm0102.exe>
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
