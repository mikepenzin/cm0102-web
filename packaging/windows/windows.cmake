# The 64-bit Windows build of cm0102_recomp and its installer. CMakeLists.txt includes this file when
# it cross-compiles with packaging/windows/mingw-w64-x86_64.cmake.
#
# Installer: cmake --build build-win --target windows_installer, with -DCM0102_GAME_DIR=<clean files>.

enable_language(RC)

# The exe properties carry the one version number of the project.
set(CM0102_PRODUCT "CM 01/02: Bosman Edition")
# The icon is the original's ball, taken from the player's own cm0102.exe at build time (not in git).
set(CM0102_ICO "${CMAKE_BINARY_DIR}/icons/cm0102.ico")
add_custom_command(OUTPUT "${CM0102_ICO}"
    COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/icon/make_icons.py" "${CM0102_EXE}" "${CMAKE_BINARY_DIR}/icons"
    DEPENDS "${CM0102_EXE}" "${CMAKE_SOURCE_DIR}/tools/icon/make_icons.py"
    COMMENT "Making the icon from ${CM0102_EXE}" VERBATIM)
configure_file("${CMAKE_CURRENT_LIST_DIR}/version.rc.in" "${CMAKE_BINARY_DIR}/version.rc" @ONLY)
set_source_files_properties("${CMAKE_BINARY_DIR}/version.rc" PROPERTIES OBJECT_DEPENDS "${CM0102_ICO}")
target_sources(cm0102_recomp PRIVATE "${CMAKE_BINARY_DIR}/version.rc")

# SDL2 and the C runtime go into the exe, so that it needs no DLL beside it. The exe has no symbols.
# The main thread gets the same deep stack as on the Mac.
target_link_options(cm0102_recomp PRIVATE -static -s "LINKER:--stack,0x10000000")
target_link_libraries(cm0102_recomp PRIVATE SDL2::SDL2main SDL2::SDL2-static)
# The sockets of the network game (src/recomp_rt/host/net_win32.c), and the system's root certificates for the
# secure connection (crypt32).
target_link_libraries(cm0102_recomp PRIVATE ws2_32 iphlpapi crypt32)

set(CM0102_GAME_DIR "" CACHE PATH "The folder of the clean files that the Windows installer holds")
find_program(MAKENSIS makensis REQUIRED)
set(CM0102_SETUP "CM0102-Bosman-Edition-${PROJECT_VERSION}-Setup.exe")
# makensis on macOS stops with std::bad_alloc when the locale is not set. The log (-V4) lists each packed
# file: tools/windows/check_installer.py --nsis-log checks it against the manifest of the download.
add_custom_target(windows_installer
    COMMAND "${CMAKE_COMMAND}" -E env LC_ALL=en_US.UTF-8 "${MAKENSIS}" -V4 "-O${CMAKE_BINARY_DIR}/installer-log.txt"
        -INPUTCHARSET UTF8
        "-DVERSION=${PROJECT_VERSION}"
        "-DEXE=$<TARGET_FILE:cm0102_recomp>"
        "-DGAME_DIR=${CM0102_GAME_DIR}"
        "-DOUT_FILE=${CMAKE_BINARY_DIR}/${CM0102_SETUP}"
        "-DICON=${CM0102_ICO}"
        "-DLICENCES=${CMAKE_SOURCE_DIR}/packaging/licences"
        "${CMAKE_CURRENT_LIST_DIR}/installer.nsi"
    DEPENDS cm0102_recomp "${CMAKE_CURRENT_LIST_DIR}/installer.nsi" "${CM0102_ICO}"
    COMMENT "Making ${CM0102_SETUP}"
    VERBATIM)
