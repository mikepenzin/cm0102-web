# Import inventory: what the original takes from Windows

Date: 2026-10-14. Task: slice 2 of `docs/specs/2026-10-14-recompile-spec.md`.
Source: `/Users/max/Projects/cm0102/clean-3968/cm0102.exe` (SHA256 `0c78e7da…bff86`), read only.

Method:

- The PE import table was parsed with a Python script (`python3 -I`, read only).
- Call sites were found with a byte search for each IAT address over `.text`, then decoded with Capstone. Three forms occur: `call [iat]`, `mov reg,[iat]` followed by `call reg`, and `jmp [iat]` thunks at `0x944c10..0x944c8e`. Calls to a thunk were counted as `E8` calls to the thunk address.
- COM calls were found with a linear sweep of `.text`. For each `call [reg+off]`, the script followed `mov reg,[reg2]` back to `mov reg2,[global]`. The DirectSound object is a field of a C++ object, so its calls were read by hand.
- Assembly was read for every function this file cites as evidence. Pseudocode (REA, Ghidra 12.1.4) was used only for `0x68d370`, `0x90bc50`, `0x90bde0` and to name CRT functions.

Marks used below:

- [code]: the assembly at the cited address shows it.
- [inf]: an inference. The reason follows it.
- R: needs real work before the recompiled game can reach the start screen.
- W: needs real work, but later than the start screen (new game, save, load, sound).
- S: a stub that returns a fixed value is enough.
- X: leave out. The runtime traps it with the import name. It is network, printing, a crash path, or a CRT path that the chosen stub values make unreachable.

All Win32 imports are `__stdcall`: the callee pops `4 × args` bytes. All COM methods are `__stdcall` with `this` as the first stack argument. Argument counts below are the Win32 signatures. Where the call site was read, the pushes agree with them.

## 1. Imported functions by library

Totals: 145 imports from 9 libraries, 483 call sites. KERNEL32 84 imports and 293 sites, USER32 27 and 77, GDI32 10 and 15, WSOCK32 18 and 92, ADVAPI32 2 and 2, DDRAW 1, DSOUND 1, WINSPOOL 1, comdlg32 1.

There is no import of `msvcrt.dll`, `winmm.dll`, `ole32.dll`, `dinput.dll` or `dplayx.dll`. There is no `CreateThread`, `Sleep`, `SetTimer`, `timeGetTime`, `GetTickCount`, `QueryPerformanceCounter`, `TlsAlloc` or `RegCloseKey`.

"Sites" counts call instructions. For a `mov reg,[iat]` site, every following `call reg` before `reg` changes counts once. "First site" gives up to five addresses.

### 1.1 KERNEL32.dll

The CRT is at `0x944c94..0x9533ff` (section 5). Sites in that range are CRT sites.

| Import | IAT | Sites | Args | First sites | Use (evidence) | Replacement | Mark |
|---|---|---|---|---|---|---|---|
| SetEnvironmentVariableA | 967048 | 2 | 2 | 951b71 9533aa | CRT `_chdir` (0x953344) sets the `=C:` variable [inf: MSVC `_chdir` pattern] | return TRUE | S |
| CompareStringW | 96704c | 2 | 6 | 951794 9519b7 | CRT `__crtCompareStringA` (0x951756), only for a non-C locale [inf] | trap | X |
| CompareStringA | 967050 | 2 | 6 | 9517b1 95180f | same | trap | X |
| CreateProcessA | 967054 | 1 | 10 | 9514d0 | CRT spawn (0x951373) for `system()` (0x9473ea). Only caller of `system` is 0x8aefee, network code that runs `ipconfig > ~net.out` (string 0xa8c708) | trap | X |
| WaitForSingleObject | 967058 | 1 | 2 | 951515 | same spawn | trap | X |
| GetExitCodeProcess | 96705c | 1 | 2 | 951522 | same spawn | trap | X |
| CreateFileA | 967060 | 1 | 7 | 95002d | CRT `_open` core (0x94fe81), under `fopen` (0x945b00) and every file the game opens | open a host file. Map `\` to `/`, match names without case, map `D:\` to the folder of the player's cm0102.exe (section 4.6) | R |
| FlushFileBuffers | 967064 | 1 | 1 | 94fe56 | CRT `_commit` (0x94fe2a) | `fsync` or return TRUE | S |
| SetStdHandle | 967068 | 2 | 2 | 94fd4c 94fdc6 | CRT handle table for handles 0..2 | return TRUE | S |
| IsBadCodePtr | 96706c | 1 | 1 | 94f4c9 | CRT pointer checks (0x94f4c1) | return 0 | S |
| IsBadReadPtr | 967070 | 1 | 2 | 94f495 | CRT (0x94f489) | return 0 | S |
| SetUnhandledExceptionFilter | 967074 | 2 | 1 | 94f470 94f482 | CRT (0x94f0f0) | return 0 | S |
| SetEndOfFile | 967078 | 1 | 1 | 952985 | CRT `_chsize` (0x95288c). No direct game caller | `ftruncate` | S |
| GetStringTypeA | 96707c | 2 | 5 | 94edde 94ee12 | CRT `__crtGetStringTypeA` (0x94ed85), ctype setup | trap first. If hit, fill types for code page 1252 | X |
| CreateMutexA | 967080 | 1 | 3 | 68d3a0 | WinMain (0x68d370): single instance, name "Championship Manager 2001/02 Process" (0x9f183c) | return a non-zero handle. `GetLastError` must then return 0, not 0xb7 | R |
| GetEnvironmentStrings | 967084 | 2 | 0 | 94e9a8 94ea5a | CRT `__crtGetEnvironmentStringsA` (0x94e979), builds `environ` for `getenv` (0x947480) | return a block with the wanted `CM3_*` variables, or an empty block (two NULs) | R |
| FreeEnvironmentStringsW | 967088 | 1 | 1 | 94ea47 | same | return TRUE | S |
| FreeEnvironmentStringsA | 96708c | 1 | 1 | 94ea98 | same | return TRUE | S |
| UnhandledExceptionFilter | 967090 | 1 | 1 | 94e5cf | CRT crash path (0x94e497) | trap | X |
| GetFileType | 967094 | 3 | 1 | 94c6eb 94c752 95003a | CRT `_ioinit` (0x94c5ec) and `_open` | 1 (FILE_TYPE_DISK) for files, 2 (CHAR) for std handles | R |
| GetStdHandle | 967098 | 2 | 1 | 94c744 94ec27 | `_ioinit`, CRT error output (0x94eae4) | return small fixed handles | R |
| SetHandleCount | 96709c | 1 | 1 | 94c789 | `_ioinit` | return the argument | S |
| SetFilePointer | 9670a0 | 2 | 4 | 94c5a1 9511e5 | CRT `_lseek` (0x94c552), `fseek` (0x946a22), `ftell` (0x946aae) | `lseek` | R |
| LCMapStringW | 9670a4 | 4 | 6 | 94c140 … | CRT `__crtLCMapStringA` (0x94c0fe), non-C locale only [inf] | trap | X |
| LCMapStringA | 9670a8 | 2 | 6 | 94c15c 94c1a5 | same | trap | X |
| MultiByteToWideChar | 9670ac | 10 | 6 | 94c1dd 94c235 94e375 94e3b4 94ee4a … | CRT locale and `mbtowc` paths | trap first. If hit, convert code page 1252 | X |
| GetDiskFreeSpaceA | 9670b0 | 1 | 5 | 7a1a37 | free disk MB (0x7a1820), used only when `GetVersionExA` reports Windows 95 before OSR2 | trap, or fixed large values | X |
| LoadLibraryA | 9670b4 | 3 | 1 | 7a1780 7a188d 951d9e | 0x7a1750 loads `CM3_DATA\cpuinf32.dll` (not shipped: no DLL in `Data`). 0x7a1820 loads "KERNEL32.DLL" for `GetDiskFreeSpaceExA`. CRT 0x951d8c loads "user32.dll" for its own message box | NULL for cpuinf32.dll and user32.dll. A fake handle for "KERNEL32.DLL", else 0x7a1820 shows an error box (0x7a1899) | W |
| GetProcAddress | 9670b8 | 6 | 2 | 7a1793 7a190f 94a6fd 951db6 951dc7 … | "GetDiskFreeSpaceExA" (0x7a190f, called at 0x7a19a7). "IsProcessorFeaturePresent" (0x94a6fd, called with 0 at 0x94a709). "cpuspeed" only if cpuinf32.dll loaded | return an address in the indirect-call table for a host function: `GetDiskFreeSpaceExA` (4 args), `IsProcessorFeaturePresent` (1 arg, return 0). NULL for others | R |
| FreeLibrary | 9670bc | 4 | 1 | 7a17b5 7a197c 7a19f1 7a1a0a | 0x7a1750, 0x7a1820 | return TRUE | S |
| GetSystemInfo | 9670c0 | 1 | 1 | 7a17ce | CPU class for preference defaults (0x7a1750, read by 0x5d7bc0). Reads `dwProcessorType` (+0x18) | fill `dwProcessorType = 586` (0x24a), as a modern PC reports | S |
| GlobalMemoryStatus | 9670c4 | 2 | 1 | 7a16e8 7a1728 | 0x7a16d0: total physical MB (`sar` on `dwTotalPhys`, signed). 0x7a1710: `(dwAvailPhys + dwAvailPageFile) >> 20`, 140 callers, the memory checks on league choice ("Not enough memory to select that many leagues", 0xa80440) | fill values below 2 GB each, for example 1 GB, so the signed shift at 0x7a16f3 stays positive | R |
| GetVersionExA | 9670c8 | 3 | 1 | 7a1835 8aee60 9489c2 | CRT heap select (0x9489a3): platform 2 and major ≥ 5 gives the system heap (0x9489de). 0x7a1820: disk-space path. 0x8aee60: network | report Windows NT 5.1 (platform 2, 5.1, build 2600). Then the CRT never calls `VirtualAlloc`, `GetEnvironmentVariableA` or the SBH code | R |
| GetCommandLineA | 9670cc | 2 | 0 | 68dbd3 947a87 | CRT start (0x947a87, stored at 0xde4628, parsed for WinMain's `lpCmdLine`). Restart (0x68dbd3) | return "cm0102.exe" plus the Mac command-line options, in flat memory | R |
| GetStringTypeW | 9670d0 | 2 | 4 | 94edc4 94eeb2 | as GetStringTypeA | trap | X |
| WinExec | 9670d4 | 1 | 2 | 68dbda | restart (0x68dbc0): close mutex, `WinExec(GetCommandLineA(), SW_SHOWNORMAL)`, `exit(0)`. Called at 0x762854 and 0x762a94 (save game before restart) | re-launch the Mac program, or trap | W |
| WideCharToMultiByte | 9670d8 | 8 | 8 | 94c30b … | CRT locale and environment (W path) | trap. Return NULL from `GetEnvironmentStringsW` so the CRT takes the A path (0x94e979) | X |
| HeapSize | 9670dc | 1 | 3 | 94bf49 | CRT `_msize` (0x94bef1) | size of the block | R |
| WriteFile | 9670e0 | 3 | 5 | 94b6ae 94b713 94ec2e | CRT `_write` (0x94b5d6), CRT error text (0x94eae4) | `write` | R |
| ReadFile | 9670e4 | 2 | 5 | 94b464 94b535 | CRT `_read` (0x94b3e0), under `fread`, `fgetc`, `fgets` | `read` | R |
| GetLastError | 9670e8 | 21 | 0 | 68d3ab 945bb6 945be2 946448 9467ef … | mutex test (0x68d3ab, compares with 0xb7), CRT `_dosmaperr` after each failed file call | return the last error the replacements set, with Win32 codes (2 file not found, 3 path not found, 18 no more files, 0xb7 already exists) | R |
| CloseHandle | 9670ec | 7 | 1 | 68d503 68dbcb 94aeea 950045 95152b … | mutex, CRT `_close` (0x94ae85) | `close` | R |
| GlobalAlloc | 9670f0 | 3 | 2 | 529cdf 529ddd 529e3c | database load (0x528540) when its argument 2 is non-zero (`DAT_00ae2340`). Start New Game passes 1 (see `2026-10-14-start-new-game-inventory.md`). Flags 0 (GMEM_FIXED) | allocate in flat memory, as `malloc` | W |
| GlobalFree | 9670f4 | 19 | 1 | 527ef4 527f34 52809e 5280ca 5280f6 … | database free (0x528080) | free | W |
| GetLogicalDriveStringsA | 9670f8 | 35 | 2 | 4131b5 442ae4 444553 4c5ffc 4c7073 … | CD check, copied into 35 functions (section 4.6) | write "D:\\" + NUL + NUL, return 4 | W |
| GetEnvironmentStringsW | 9670fc | 2 | 0 | 94e994 94e9d4 | CRT environment (0x94e979) | return NULL | S |
| GetDriveTypeA | 967100 | 37 | 1 | 4131d2 442b07 444572 4c6015 4c7092 … | CD check (35 sites), CRT `_stat` (0x946ea3, 0x947678) | 5 (DRIVE_CDROM) for "D:\\", 3 (FIXED) for others | W |
| IsBadWritePtr | 967104 | 1 | 2 | 94f4b1 | CRT (0x94f4a5) | return 0 | S |
| VirtualAlloc | 967108 | 5 | 4 | 949263 9492ef 9496d4 9496ee 949ac2 | CRT small-block heap, used only when heap mode is 2 or 3 (0x948b10) | trap. Not reached with NT 5 version | X |
| VirtualFree | 96710c | 5 | 3 | 948e13 948e6e 9497af 9497e2 949867 | same | trap | X |
| HeapCreate | 967110 | 1 | 3 | 948afc | CRT `_heap_init` (0x948aeb), handle kept at 0xde4400 | return a fixed non-zero handle | R |
| HeapDestroy | 967114 | 1 | 1 | 948b3b | `_heap_init` failure path | return TRUE | S |
| GetEnvironmentVariableA | 967118 | 1 | 3 | 9489f7 | heap select reads `__MSVCRT_HEAP_SELECT`, only before NT 5 | return 0 | S |
| GetModuleFileNameA | 96711c | 3 | 3 | 948a57 94e74f 94eb51 | CRT `_setargv` (0x94e72c), CRT error text | write a Windows-style path such as "C:\\CM0102\\cm0102.exe" | R |
| GetOEMCP | 967120 | tail jump | 0 | jmp at 948624 | CRT `_setmbcp` (0x94860a) | return 850 | S |
| GetACP | 967124 | tail jump | 0 | jmp at 948639 | same | return 1252 | S |
| GetCPInfo | 967128 | 3 | 2 | 9484b9 9486c4 951860 | `_setmbcp` | fill `MaxCharSize = 1`, no lead bytes, return TRUE | R |
| GetVersion | 96712c | 1 | 0 | 947a39 | CRT start, `_osver` and `_winver` (0xde2f98..0xde2fa4) | return 0x0A280105 (NT 5.1, build 2600) | S |
| RaiseException | 967130 | 1 | 4 | 952be7 | CRT `_CxxThrowException` (0x9529d2). Its only caller is 0x950f5e in the CRT. No game code calls it [code: no `E8` and no address reference outside the CRT] | trap | X |
| GetStartupInfoA | 967134 | 2 | 1 | 947ab2 94c645 | CRT start (`nCmdShow`), `_ioinit` | zero the struct, `cb = 0x44` | R |
| GetModuleHandleA | 967138 | 3 | 1 | 947ad5 948980 94a6ed | NULL: `hInstance` for WinMain (0x947ad5), later used by `LoadCursorA` and `LoadIconA`. "KERNEL32" at 0x94a6ed | 0x400000 for NULL, a fake handle for "KERNEL32" | R |
| GetFileAttributesA | 96713c | 1 | 1 | 9478dc | CRT `_access` (0x9478d8), game file test at 0x90c127 | `stat` on the mapped path, 0x80 or 0x10, or -1 | R |
| SetCurrentDirectoryA | 967140 | 1 | 1 | 953350 | CRT `_chdir` (0x953344), called once, by WinMain with the registry path (0x68d43f) | `chdir` on the mapped path | S |
| GetSystemTimeAsFileTime | 967144 | 2 | 1 | 9472a3 9472e6 | CRT `clock()` (0x947299) and its start time (0x9472dc). `clock()` is the only millisecond timer: the pump tick 0xb67a08, the 100 ms and 1000 ms text timers, the 300 ms hover delay, the random seed without "-seed" (0x81b95e) | 100 ns units from a monotonic host clock plus a fixed base. Must advance in real time | R |
| GetLocalTime | 967148 | 1 | 1 | 946ec8 | CRT `time()` (0x946ebb), 19 game callers | host local time | R |
| GetSystemTime | 96714c | 1 | 1 | 946ed2 | same | host UTC time | R |
| GetTimeZoneInformation | 967150 | 2 | 1 | 946f27 94c9fb | `time()`, `_tzset` | return 0 (TIME_ZONE_ID_UNKNOWN) with the host bias, or bias 0 | R |
| HeapAlloc | 967154 | 7 | 3 | 944ef0 945575 94572d 945855 948b55 … | `malloc` (0x944e46 → 0x944ef0, size rounded up to 16, flags 0). `calloc` (0x945575, flags 8 = HEAP_ZERO_MEMORY). `realloc` (0x945667) | allocator inside the flat 32-bit block. Honour flag 8 | R |
| RtlUnwind | 967158 | tail jump | 4 | jmp at 9532a0 | `_global_unwind2` (0x9451dc), used by `longjmp` (0x94791c), and the C++ frame handler (0x944f3a) | unwind the `fs:[0]` chain in flat memory down to the target frame, calling each handler with the unwinding flag | R |
| HeapFree | 96715c | 5 | 3 | 94532a 948e80 94927a 9497c6 949818 | `free` (0x9452ca) | free | R |
| HeapReAlloc | 967160 | 4 | 4 | 945777 945894 9458e2 949215 | `realloc` | realloc in flat memory | R |
| ExitProcess | 967164 | 2 | 1 | 9459e7 947b4b | CRT `exit` (0x945934), fatal start error | end the program | R |
| TerminateProcess | 967168 | 1 | 2 | 94596d | CRT `_exit` path (0x945956) | end the program | S |
| GetCurrentProcess | 96716c | 1 | 0 | 945966 | same | return -1 | S |
| MoveFileA | 967170 | 1 | 2 | 945bac | CRT `rename` (0x945ba4), 1 game caller | `rename` on mapped paths | W |
| CreateDirectoryA | 967174 | 1 | 2 | 945bd8 | CRT `_mkdir` (0x945bd2), 3 game callers | `mkdir` | W |
| DeleteFileA | 967178 | 1 | 1 | 94643e | CRT `remove` (0x94643a), 24 game callers (temp files, a bad game.cfg at 0x5d7953) | `unlink` | W |
| FindFirstFileA | 96717c | 2 | 2 | 9467e2 94761c | CRT `_findfirst` (0x9467ce, one game caller: the wrapper 0x4c0aa0) and `_stat` (0x9475b5) | `opendir` and pattern match, with `*` and `?`, on the mapped folder. Fill `WIN32_FIND_DATAA` | W |
| FindNextFileA | 967180 | 1 | 2 | 9468ab | `_findnext` (0x946898) | next match, error 18 at end | W |
| FindClose | 967184 | 3 | 1 | 946961 947809 94785b | `_findclose` (0x94695d), `_stat` | `closedir` | W |
| FileTimeToSystemTime | 967188 | 4 | 2 | 9469a7 9476f8 947764 9477d2 | file times in `_findfirst` and `_stat` (0x94697c) | convert | W |
| FileTimeToLocalFileTime | 96718c | 4 | 2 | 946995 9476e2 94774e 9477bc | same | add the time zone bias | W |
| GetCurrentDirectoryA | 967190 | 3 | 2 | 946e22 9473bf 953366 | `_getcwd` (0x946db3), `_getdrive` (0x9473a7, game caller 0x826b14 for the "Disk Full" text), `_chdir` | return "C:\\" plus the mapped folder | W |
| GetFullPathNameA | 967194 | 2 | 4 | 946e0e 94e1a2 | `_getcwd`, `_fullpath` (0x94e159) | join with the current folder | W |

### 1.2 USER32.dll

| Import | IAT | Sites | Args | First sites | Use (evidence) | Replacement | Mark |
|---|---|---|---|---|---|---|---|
| ChangeDisplaySettingsA | 96719c | 2 | 2 | 68d838 68d8e4 | window procedure, full screen only. On deactivate: `(NULL,0)` then `CloseWindow`. On activate: 800×600×16 (`DEVMODE` fields at 0x68d8b4..0x68d8d9), then `ShowWindow(5)` | return 0 (DISP_CHANGE_SUCCESSFUL) | S |
| UpdateWindow | 9671a0 | 1 | 1 | 5e2887 | display init (0x5e2620) | return TRUE | S |
| CreateWindowExA | 9671a4 | 1 | 12 | 5e2847 | display init. Class "main" (0x9cd9b4). Full screen: style 0x80080000, size of the screen. Windowed: style 0xa0000, centred, client 800×600 plus frame | create the SDL window (800×600). Return a fake HWND. The code keeps it at 0xb67a2c | R |
| GetWindowRect | 9671a8 | 2 | 2 | 5e278f 5e2fdc | desktop size (0x5e278f). Window position for the windowed flip (0x5e2fdc) | fixed rect (0,0,800,600) or the desktop size | S |
| GetDesktopWindow | 9671ac | 1 | 0 | 5e2788 | display init | fake HWND | S |
| GetSystemMetrics | 9671b0 | 7 | 1 | 5e2762 5e276b 5e2774 5e27cf 5e27d4 … | indexes 7, 8, 4 (frame and caption sizes, kept at 0xae3180, 0xae318c, 0xae3144), 0 and 1 (screen size) | fixed values: 0 for frames and caption, 800 and 600 for the screen | S |
| RegisterClassA | 9671b4 | 1 | 1 | 5e2754 | display init. `lpfnWndProc = 0x68d630` (0x5e271d) | store the window procedure address. `DispatchMessageA` calls it | R |
| LoadIconA | 9671b8 | 1 | 2 | 5e2731 | icon resource 104 (0x68) | return a fake handle. Optional: set the SDL window icon from RT_GROUP_ICON 104 | S |
| SetCursor | 9671bc | 10 | 1 | 5e2708 5e2cce 5e81a2 5e81b6 5e81ca … | cursor shape (0x5e8170, 0x5e81e0, see `2026-10-13-input-inventory.md`) | `SDL_SetCursor` with the cursor made at `LoadCursorA` | R |
| ShowCursor | 9671c0 | 18 | 1 | 5e2700 5e270f 5e2f87 5e30b9 5e818d … | hide and show around the flip when the software cursor flag 0xaebe14 is set (0x5e2f25..0x5e2f87). Cursor changes | keep the Windows counter (return the new count). `SDL_ShowCursor` when it crosses 0 | R |
| LoadCursorA | 9671c4 | 6 | 2 | 5e2675 5e268d 5e26a5 5e26ba 5e26cf … | resources 105 → 0xaebe1c, 101 → 0xaebe24, 103 → 0xaebe20 (0x5e2663..0x5e26a7). Fallbacks IDC_WAIT (0x7f02) and IDC_ARROW (0x7f00) | build SDL cursors from RT_CURSOR in `.rsrc` (group cursors 101, 103, 105 exist at 0x9e5418, 0x9e5568, 0x9e5870), or system cursors | R |
| GetCursor | 9671c8 | 4 | 0 | 5e2653 5e81f1 5e821b 5e8246 | save the cursor at start (0xaebe18), compare in 0x5e81e0 | return the current handle | S |
| DestroyWindow | 9671cc | 1 | 1 | 5e2d63 | display release (0x5e2cc0) | destroy the SDL window | S |
| DestroyCursor | 9671d0 | 3 | 1 | 5e2cea 5e2cfc 5e2d0e | display release | free | S |
| MessageBoxA | 9671d4 | 1 | 4 | 5e8890 | error box 0x5e8290 when the display is not up (0xb67a30 == 0 or 0xaebe0c != 0, test at 0x5e82a3..0x5e82be). Example: "Already Running" (0x68d3c2). Type 0 (MB_OK) | `SDL_ShowSimpleMessageBox`, return 1 (IDOK) | R |
| GetKeyboardLayout | 9671d8 | 1 | 1 | 66e7c0 | language choice in 0x66e790 before `CM3_DATA\language.ldb` loads. Primary language from the low word (0x66e7c6..0x66e7d8, jump table 0x66e7e9). English US gives 1, others 0 (`2026-10-14-start-screen-helpers.md`) | return a fixed layout, 0x08090809 (English UK) [inf: the UK release]. Make it a setting | S |
| DispatchMessageA | 9671dc | 1 | 1 | 68d58d | pump 0x68d520 | call the generated function for 0x68d630 with (hwnd, msg, wParam, lParam). This is the one place where an import calls back into game code | R |
| TranslateMessage | 9671e0 | 1 | 1 | 68d586 | pump | queue WM_CHAR from SDL text input. Or let the event mapper make WM_CHAR, and return TRUE | R |
| GetMessageA | 9671e4 | 1 | 4 | 68d57b | pump. Called only after `PeekMessageA` saw a message, so it never blocks | pop the next queued message. Return 0 only for WM_QUIT | R |
| MoveWindow | 9671e8 | 1 | 6 | 5e29c4 | display re-init | return TRUE | S |
| SetWindowLongA | 9671ec | 2 | 3 | 5e2947 5e2975 | display re-init (GWL_STYLE) | return 0 | S |
| PeekMessageA | 9671f0 | 2 | 5 | 68d558 68d59c | pump, PM_NOREMOVE (flags 0) | poll SDL events, turn them into Windows messages in a queue, return TRUE if the queue is not empty | R |
| DefWindowProcA | 9671f4 | 1 | 4 | 68da4d | window procedure default path | return 0 | S |
| BeginPaint | 9671f8 | 1 | 2 | 68d79e | WM_PAINT: full redraw `0x55f300(-1,-1,-1,-1)` | return a fake HDC | S |
| EndPaint | 9671fc | 1 | 2 | 68d7e9 | WM_PAINT | return TRUE | S |
| CloseWindow | 967200 | 1 | 1 | 68d849 | minimise on deactivate, full screen only | return TRUE | S |
| ShowWindow | 967204 | 5 | 2 | 5e2896 5e28bc 5e29d3 5e2d53 68d8f4 | show (5) and hide (0) | `SDL_ShowWindow`, `SDL_HideWindow` | S |

### 1.3 GDI32.dll, WINSPOOL.DRV, comdlg32.dll: printing

All ten GDI32 imports are in one print function, 0x84c830 (from 0x84c830 to 0x84ca6e). It prints text lines in "Courier" (0xa827d4) under the document name "Championship Manager 2000/01" (0xa827dc). Its callers are 0x567833 and 0x56786c. `EnumPrintersA` (0x84c7f0, thunk 0x944c10) asks whether a printer exists. Its only caller is the preference setup 0x5d7c0f, which sets preference flag 0x1000000 when a printer exists. `PrintDlgA` (thunk 0x944c16) opens the print dialog at 0x84c8a1.

| Import | IAT | Sites | Args | First sites | Replacement | Mark |
|---|---|---|---|---|---|---|
| GetDeviceCaps | 96701c | 5 | 2 | 84c910 84c985 84c998 84c9e8 84ca10 | trap | X |
| CreateFontA | 967020 | 1 | 14 | 84c91f | trap | X |
| GetTextMetricsA | 967024 | 1 | 2 | 84c93c | trap | X |
| EndPage | 967028 | 2 | 1 | 84c9ab 84ca4c | trap | X |
| StartPage | 96702c | 1 | 1 | 84c9b6 | trap | X |
| SelectObject | 967030 | 1 | 2 | 84c9db | trap | X |
| TextOutA | 967034 | 1 | 5 | 84ca18 | trap | X |
| DeleteObject | 967038 | 1 | 1 | 84ca57 | trap | X |
| EndDoc | 96703c | 1 | 1 | 84ca63 | trap | X |
| StartDocA | 967040 | 1 | 2 | 84c8d4 | trap | X |
| EnumPrintersA (WINSPOOL) | 96720c | 1 (thunk 944c10) | 7 | 84c80a | return FALSE. Then 0x84c7f0 returns 0 and the print flag stays clear. This matches a PC with no printer [inf: the print button then stays off, not checked] | S |
| PrintDlgA (comdlg32) | 967260 | 1 (thunk 944c16) | 1 | 84c8a1 | return FALSE (cancelled) | S |

A later port could write the print text to a file. The string "~printout.txt" (0xa78a40) exists, so the original may already have a text-file path [inf, not read].

### 1.4 ADVAPI32.dll

| Import | IAT | Sites | Args | First sites | Use | Replacement | Mark |
|---|---|---|---|---|---|---|---|
| RegOpenKeyExA | 967000 | 1 | 5 | 68d3f3 | section 4.4 | return 2 (ERROR_FILE_NOT_FOUND). Or open a fake key if the port wants the chdir | S |
| RegQueryValueExA | 967004 | 1 | 6 | 68d418 | section 4.4 | not reached if the open fails | S |

### 1.5 DDRAW.dll and DSOUND.dll

| Import | IAT | Sites | Args | First sites | Use | Replacement | Mark |
|---|---|---|---|---|---|---|---|
| DirectDrawCreate | 96700c | 1 (thunk 944c1c) | 3 | 5e28a5 | display init: `DirectDrawCreate(NULL, &0xae3194, NULL)` | build a fake IDirectDraw in flat memory (section 2.1) | R |
| DirectSoundCreate (ordinal 1) | 967014 | 1 (thunk 944c22) | 3 | 84ca9c | sound object ctor 0x84ca80: `DirectSoundCreate(NULL, &obj+0x28, NULL)` | build a fake IDirectSound (section 2.2) | R (silent stub first) |

Ordinal 1 of dsound.dll is `DirectSoundCreate` [inf: the standard dsound.dll export table; the import has no name]. The call passes 3 arguments, which matches.

### 1.6 WSOCK32.dll: network play only

WSOCK32 imports by ordinal. The names come from the standard wsock32 ordinals [inf]. Every call goes through a thunk at 0x944c28..0x944c8e. Every caller is in 0x77a7d0..0x77aa77 (network game setup) or 0x8ae100..0x8af4e6 (the socket layer). `WSAStartup` is called only by 0x77a7d0 and 0x77a9f0 [code: Ghidra callers]. The start screen shows a "Network Play" item when 0xa801c4 is non-zero (0x81bbf3). "-nonetwork" clears 0xa801c4 (0x81b9d6).

| Import | Ordinal | IAT | Sites | Args |
|---|---|---|---|---|
| gethostname | 57 | 967214 | 1 | 2 |
| WSAStartup | 115 | 967218 | 4 | 2 |
| WSACleanup | 116 | 96721c | 1 | 0 |
| closesocket | 3 | 967220 | 29 | 1 |
| accept | 1 | 967224 | 1 | 3 |
| gethostbyname | 51 | 967228 | 1 | 1 |
| WSAGetLastError | 111 | 96722c | 2 | 0 |
| send | 19 | 967230 | 2 | 4 |
| recv | 16 | 967234 | 1 | 4 |
| sendto | 20 | 967238 | 2 | 6 |
| recvfrom | 17 | 96723c | 1 | 6 |
| listen | 13 | 967240 | 2 | 2 |
| bind | 2 | 967244 | 3 | 3 |
| inet_ntoa | 12 | 967248 | 4 | 1 |
| connect | 4 | 96724c | 2 | 3 |
| setsockopt | 21 | 967250 | 22 | 5 |
| socket | 23 | 967254 | 5 | 3 |
| htons | 9 | 967258 | 9 | 1 |

All are X. Two choices for the port: keep the "Network Play" item and trap when it is used, or clear 0xa801c4 at start, which hides the item. The second changes the start screen, so it is a decision for Max.

## 2. COM interfaces

The original uses three COM interfaces through vtables: IDirectDraw (version 1, from `DirectDrawCreate`), IDirectDrawSurface (version 1), IDirectDrawClipper, IDirectSound and IDirectSoundBuffer. It never calls `QueryInterface` (no `call [reg+0]` on these objects). It does not use DirectInput, DirectPlay, IDirectDrawPalette or any other COM interface. There is no `CoCreateInstance` (no ole32 import). The two other globals that the scan found with vtable calls, 0xb74434 and 0xae2c74, are game C++ objects [inf: their slots 0, 4 and 0x1c do not match a DirectX method use].

Every replacement object needs a vtable in flat memory. Each slot holds an address that the indirect-call table maps to a host function. Unused slots should trap with the interface and offset.

### 2.1 DirectDraw (display.cpp, 0x5e2620..0x5e8150)

Globals: IDirectDraw at 0xae3194, primary surface at 0xae31a4, back buffer at 0xaebde4, clipper at 0xaebe2c, pixel format copy at 0xae3158 (32 bytes), "display off" flag 0xaebe0c.

The display init `0x5e2620(width 800, height 600, flags 2, font mode, full screen, title)` is called at 0x5d79f7. Argument 5 (full screen) is bit 3 of the preference word at prefs+4 (0x5d79d5..0x5d79e1). Bit 1 of `flags` asks for a separate back buffer (0x5e2ae2). Without it the back buffer is the primary (0x5e2b29).

IDirectDraw:

| Offset | Method | Sites | Call as used | Replacement |
|---|---|---|---|---|
| 0x08 | Release | 5e2d42 | release at exit (0x5e2cc0) | free |
| 0x10 | CreateClipper | 5e2ba8 | windowed only: `(0, &0xaebe2c, NULL)` | return a fake clipper |
| 0x18 | CreateSurface | 5e2ad3, 5e2b1a | primary: `DDSURFACEDESC` size 0x6c, flags 1 (CAPS), caps 0x200 (PRIMARYSURFACE). Back buffer: flags 7 (CAPS, HEIGHT, WIDTH), caps 0x840 (OFFSCREENPLAIN, SYSTEMMEMORY), 800×600 | primary: a surface whose blit target is the SDL texture. Back buffer: an 800×600 16-bit buffer in flat memory, pitch 1600 |
| 0x4c | RestoreDisplayMode | 5e2d37 | release | return 0 |
| 0x50 | SetCooperativeLevel | 5e28d3, 5e29f9, 5e2a5f | full screen 0x13 (FULLSCREEN, ALLOWREBOOT, EXCLUSIVE). Windowed and re-init 8 (NORMAL) | return 0 |
| 0x54 | SetDisplayMode | 5e2a2c, 5e2a7e | version 1 form, 3 args: `(800, 600, 16)`. The windowed path at 0x5e2a7e passes the desktop size | return 0. Never fail, or the init shows "Unable to initialise the graphics…" (0x9cd8f0) |

IDirectDrawSurface:

| Offset | Method | Sites | Call as used | Replacement |
|---|---|---|---|---|
| 0x08 | Release | 5e2901, 5e2911 | display re-init | free |
| 0x14 | Blt | primary 5e3050. Back buffer 5e3cd0 | primary: windowed flip, `Blt(&dst in screen coordinates, back, &src, DDBLT_WAIT 0x1000000, NULL)`. Back buffer: colour fill, flags 0x1000400 (WAIT, COLORFILL), `DDBLTFX` size 0x64, fill colour at +0x50 | primary: copy the source rect of the back buffer to the screen. Back buffer: fill the rect with the 16-bit colour |
| 0x1c | BltFast | 5e2fc8 | full screen flip: `BltFast(x, y, back, &rect, DDBLTFAST_WAIT 0x10)` | copy the rect to the screen and present |
| 0x58 | GetSurfaceDesc | 5e2b5b | primary. The code copies `ddpfPixelFormat` (desc +0x48, 32 bytes) to 0xae3158. 0x5e4560 builds all colours from it | return 16-bit RGB565: size 32, flags 0x40 (RGB), bit count 16, masks 0xF800, 0x07E0, 0x001F. The test harness uses the same (`tests/diff/original.py`) |
| 0x60 | IsLost | 5e2e65, 5e2e83 | on activate (0x5e2e50) | return 0 |
| 0x64 | Lock | back buffer, 14 sites: 5e355f 5e37b7 5e384f 5e3914 5e3d63 5e3e90 5e4052 5e44dd 5e5235 5e5696 5e5ed2 5e73fc 5e78fe 5e7fd3 | `Lock(&rect or NULL, &desc (size 0x6c), flags, NULL)`. Flags 0x21 (WAIT, WRITEONLY) or 0x11 (WAIT, READONLY). Not read at 0x5e5235 and 0x5e78fe. The code reads `lPitch` (+0x10) and `lpSurface` (+0x24) | fill pitch 1600 and the pointer to the rect's top-left pixel, as DirectDraw does. Return 0 |
| 0x6c | Restore | 5e2e74, 5e2e92 | after IsLost | return 0 |
| 0x70 | SetClipper | 5e2c17 | windowed: primary gets the clipper | return 0 |
| 0x80 | Unlock | back buffer, 13 sites: 5e35ef 5e38c3 5e3b38 5e3dbc 5e3fb6 5e40be 5e4547 5e5419 5e591f 5e5ef8 5e741b 5e7a39 5e814d | `Unlock(NULL)` | return 0 |

The screen changes only in the flip function 0x5e2eb0 (BltFast or Blt). The port should present to SDL there.

IDirectDrawClipper:

| Offset | Method | Sites | Call | Replacement |
|---|---|---|---|---|
| 0x08 | Release | 5e28e2, 5e2d22 | re-init, release | free |
| 0x20 | SetHWnd | 5e2be0 | `(0, hwnd)` | return 0 |

### 2.2 DirectSound (sound object, 0x84ca80..0x84cf40)

The sound object is 0x30 bytes. It holds ten buffer slots at +0x00..+0x24, IDirectSound at +0x28 and an "ok" flag at +0x2c. The prefs setup creates it at 0x5d7a37 when preference flag 0x80 is set, and keeps it at 0xdd7eec. If creation fails, the game shows "Unable to initialise the sound…" (0x9cd2cc) and sets preference flag 0x100 (no sound) (0x5d7a6e).

Methods of the sound object:

- 0x84ca80 is the constructor.
- 0x84caf0 releases the object.
- 0x84cb10 stops all slots.
- 0x84cb70 stops one slot.
- 0x84cbc0 plays a file: `(path, pan 0/1/2, loop)`. Pan 0 is -5000, 1 is 5000 and 2 is 0 (0x84cc01..0x84cc1c).
- 0x84cc80 tells whether a slot plays.
- 0x84ccc0 sets the volume of a slot: `(slot, percent 0..100)`, as `(percent - 100) × 100` hundredths of a dB (0x84ccea..0x84ccf7).
- 0x84cd10 loads a WAV.
- 0x84cf00 finds a free slot.

Callers:

- The intro sounds `CM3_SOUNDS\intro1.wav` and `intro2.wav` play from the main loop at 0x5ccdf8 and 0x5cce54.
- The match sound plays at 0x802634.
- One more play is at 0x56751b. Its context was not read.

The WAV loader 0x84cd10 reads a fixed 44-byte header with `fread`. It checks "RIFF", "WAVE" and "fmt", takes the format from the header and the data size from header offset 0x28. Then it creates a static buffer and copies the data in with one `fread`.

IDirectSound:

| Offset | Method | Sites | Call | Replacement |
|---|---|---|---|---|
| 0x08 | Release | 84cace, 84cafd | | free |
| 0x0c | CreateSoundBuffer | 84ce58 | `DSBUFFERDESC` size 0x14, flags 0xe2 (STATIC, CTRLFREQUENCY, CTRLPAN, CTRLVOLUME), bytes = WAV data size, format from the header | make a buffer object that holds the PCM data in host memory |
| 0x18 | SetCooperativeLevel | 84cabe | `(hwnd, 1 = DSSCL_NORMAL)` | return 0 |

IDirectSoundBuffer:

| Offset | Method | Sites | Call | Replacement |
|---|---|---|---|---|
| 0x08 | Release | 84cb50, 84cba6 | | free |
| 0x24 | GetStatus | 84cca5, 84cf26 | tests bits 5 (PLAYING, LOOPING) | report whether the SDL channel plays |
| 0x2c | Lock | 84ce7e | `(0, size, &p1, &n1, NULL, NULL, 0)` | return a pointer into flat memory for the copy |
| 0x30 | Play | 84cc63 | `(0, 0, loop ? 1 : 0)` | start playback on an SDL audio stream or mixer channel |
| 0x34 | SetCurrentPosition | 84cb48, 84cb9e | `(0)` | rewind |
| 0x3c | SetVolume | 84cc4b, 84ccfc | 0x84cbc0 passes 10000 (0x2710). 0x84ccc0 passes -10000..0 | clamp to -10000..0. Return DSERR_INVALIDPARAM (0x80070057) for 10000, as real DirectSound does [inf: documented range; the original ignores the result] |
| 0x40 | SetPan | 84cc3e | -5000, 0, 5000 | apply pan |
| 0x48 | Stop | 84cb3e, 84cb94 | | stop |
| 0x4c | Unlock | 84ceb6 | `(p1, n1, NULL, 0)` | copy from flat memory to the host buffer |

A silent first version is enough for the start screen. Creation succeeds, `GetStatus` reports "not playing", and the other methods return 0. If `DirectSoundCreate` fails instead, the error box above blocks the start until it is closed.

## 3. Use and replacement plan, by need

Section 1 gives the plan for each import. This section groups them.

### 3.1 The game cannot run without these

- The CRT start: `GetVersion`, `GetVersionExA`, `HeapCreate`, `HeapAlloc`, `HeapFree`, `HeapReAlloc`, `HeapSize`, `GetCommandLineA`, `GetEnvironmentStrings`, `GetEnvironmentStringsW` (NULL), `GetStartupInfoA`, `GetStdHandle`, `GetFileType`, `SetHandleCount`, `GetModuleFileNameA`, `GetModuleHandleA`, `GetCPInfo`, `GetACP`, `GetOEMCP`, `GetProcAddress`, `ExitProcess`.
- Files: `CreateFileA`, `ReadFile`, `WriteFile`, `SetFilePointer`, `CloseHandle`, `GetLastError`, `GetFileAttributesA`.
- Time: `GetSystemTimeAsFileTime`, `GetLocalTime`, `GetSystemTime`, `GetTimeZoneInformation`.
- Window and input: `RegisterClassA`, `CreateWindowExA`, `PeekMessageA`, `GetMessageA`, `TranslateMessage`, `DispatchMessageA`, `LoadCursorA`, `SetCursor`, `ShowCursor`, `MessageBoxA`, `CreateMutexA`.
- Display: `DirectDrawCreate` with IDirectDraw, IDirectDrawSurface and IDirectDrawClipper as in section 2.1.
- Sound: `DirectSoundCreate` with the silent objects of section 2.2.
- Unwinding: `RtlUnwind`, for `longjmp`. The callers of `longjmp` and `_setjmp3` are at 0x9371e4..0x943f4a, in the code after the T2K font strings [inf: the T2K font engine, used when the font mode is -2]. The recompiler also needs `fs:[0]` to point to a thread information block in flat memory. The code installs a C++ exception frame 3,440 times (`mov fs:[0], esp`).

### 3.2 Real work, needed after the start screen

- New game and database: `GlobalAlloc`, `GlobalFree`, `GlobalMemoryStatus` (also at start), the CD check (`GetLogicalDriveStringsA`, `GetDriveTypeA`, `FindFirstFileA` on `D:\cm0102.exe`).
- Saves, temp files and folders: `DeleteFileA`, `MoveFileA`, `CreateDirectoryA`, `FindFirstFileA`, `FindNextFileA`, `FindClose`, `FileTimeToSystemTime`, `FileTimeToLocalFileTime`, `GetCurrentDirectoryA`, `GetFullPathNameA`.
- Disk space before a save: `LoadLibraryA("KERNEL32.DLL")` and `GetProcAddress("GetDiskFreeSpaceExA")` (0x7a1820, 136 callers, most in 0x828000..0x82f500). Return a large free space.
- Real sound: the IDirectSoundBuffer methods with SDL audio.
- Restart after a save: `WinExec`.
  Replaced on 2026-11-11 (`src/recomp_rt/win/kernel32.c`, `RT_IMPORT(WinExec)`): restart_game (0x68dbc0) is the only
  caller (0x68dbda); it runs 0x5cc7d0, closes the handle at [0xb67a28] and calls `WinExec(GetCommandLineA(), 10)`, then
  `exit(0)` at 0x68dbe2. The replacement starts the program again with the arguments of this start (`host_restart`:
  `execv` of the program on macOS and Linux, `CreateProcessW` with the command line on Windows) and, in the browser,
  saves the user folder and loads the page again (`Module.rtRestart`, `web/loader.js`). Before, the import had no
  replacement and Restart Game stopped every build ("import WinExec has no replacement (called from 0x68dbe0)", an
  error report of the browser build on 2026-11-11).

### 3.3 Stubs with a fixed value

`SetEnvironmentVariableA`, `FlushFileBuffers`, `SetStdHandle`, `IsBadCodePtr`, `IsBadReadPtr`, `IsBadWritePtr`, `SetUnhandledExceptionFilter`, `SetEndOfFile`, `FreeEnvironmentStringsA`, `FreeEnvironmentStringsW`, `FreeLibrary`, `GetSystemInfo`, `HeapDestroy`, `GetEnvironmentVariableA`, `TerminateProcess`, `GetCurrentProcess`, `SetCurrentDirectoryA`, `ChangeDisplaySettingsA`, `UpdateWindow`, `GetWindowRect`, `GetDesktopWindow`, `GetSystemMetrics`, `LoadIconA`, `GetCursor`, `DestroyWindow`, `DestroyCursor`, `GetKeyboardLayout`, `MoveWindow`, `SetWindowLongA`, `DefWindowProcA`, `BeginPaint`, `EndPaint`, `CloseWindow`, `ShowWindow`, `RegOpenKeyExA`, `RegQueryValueExA`, `EnumPrintersA`, `PrintDlgA`.

### 3.4 Leave out and trap

- Network: all 18 WSOCK32 imports, `CreateProcessA`, `WaitForSingleObject`, `GetExitCodeProcess`.
- Printing: all 10 GDI32 imports.
- CRT locale paths: `CompareStringA`, `CompareStringW`, `LCMapStringA`, `LCMapStringW`, `GetStringTypeA`, `GetStringTypeW`, `MultiByteToWideChar`, `WideCharToMultiByte`. The game never changes the locale from "C" [inf: no game call to a `setlocale` was found. The CRT functions that use these imports have no game callers (section 5)].
- CRT heap modes 2 and 3: `VirtualAlloc`, `VirtualFree`.
- Crash paths: `UnhandledExceptionFilter`, `RaiseException`.
- `GetDiskFreeSpaceA` (only on Windows 95 before OSR2).

## 4. Process structure

### 4.1 Threads

There is one thread. No thread, fiber, timer or TLS import exists, and the CRT is the single-thread static library [inf: no `TlsAlloc` import, and `_setjmp3`/`longjmp` keep no per-thread data].

### 4.2 Timers and the main loop

There is no timer import and no `Sleep`. All waiting is busy polling:

- `clock()` (0x947299) is the only millisecond time: `(GetSystemTimeAsFileTime − start) / 10000` (0x9472a3..0x9472d4). Its start time is set by 0x9472dc [inf: a CRT initialiser].
- The pump 0x68d520 stores `clock()` in 0xb67a08 after each pass (0x68d5a2).
- `time()` (0x946ebb) gives the date and time for 19 game callers, for example 0x5921fd and 0x7fc84e.

Port note: the original never yields the CPU. The `PeekMessageA` replacement may sleep for about 1 ms when the queue is empty. This keeps the Mac cool, and the game sees no change [inf: no code measures how long one pump takes].

### 4.3 Start sequence, message loop and window procedure

1. `WinMainCRTStartup` (0x947a13) runs the CRT start, then `WinMain(hInstance, 0, lpCmdLine, nCmdShow)` = 0x68d370 (call at 0x947adc).
2. WinMain (0x68d370):
   1. It stores `hInstance` at 0xb67a18.
   2. It creates the mutex and shows "Already Running" if `GetLastError() == 0xb7`.
   3. It calls `0x5e8280(1)`, then does the registry chdir (4.4).
   4. It calls `0x5cc6d0(lpCmdLine)`.
3. 0x5cc6d0 copies the command line to 0xae2c98, lowercases it with `_strlwr` (0x9532a6), and builds the preference object (0x5d72c0) and the screen manager (0x5ef0b0, kept at 0xb5ded8). It sets 0xb67a30 = 1.
4. 0x5d72c0 reads `allowed_fonts.txt` and `CM3_PREFS\game.cfg`, applies the command-line options, inits the display (0x5d79f7) and creates the sound object (0x5d7a37).
5. WinMain then loops `while (0x5cc6b0()) pump();`. But 0x5cc6b0 runs the whole game, `0x90d0a0(0)` then `0x5cccd0()`, and returns 0. So the loop body never runs, and the game pumps messages from inside: 14 callers of the pump 0x68d520, for example 0x5ccc3c, 0x5e76fe and 0x7fca76.
6. The pump (0x68d520):

   ```
   while (PeekMessageA(&m, 0, 0, 0, PM_NOREMOVE)) {
       if (!GetMessageA(&m, 0, 0, 0)) { error box; exit; }
       TranslateMessage(&m);
       DispatchMessageA(&m);
   }
   ```

   Then it stores `clock()`.
7. The window procedure is 0x68d630. `RegisterClassA` gets it at 0x5e271d. `2026-10-13-input-inventory.md` lists its messages: WM_KEYDOWN, WM_CHAR, mouse moves and buttons, the wheel, WM_PAINT, WM_ACTIVATE and WM_ACTIVATEAPP, WM_CLOSE.

   The replacement must produce these messages from SDL events. It must call the window procedure with the arguments as 32-bit values and the stdcall `RET 0x10`. The procedure reads key codes as Windows virtual keys and mouse positions as client pixels in `lParam`.

### 4.4 Registry

There is one key, read once by WinMain (0x68d3f3..0x68d43f):

- `RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\Sports Interactive Ltd\Championship Manager 01-02", 0, KEY_ALL_ACCESS 0xf003f, &key)`. Strings at 0x9f17ac.
- `RegQueryValueExA(key, "Location", NULL, &type, buf, &size)`. Buffer 260 bytes, `size = 0x104`, `type` set to 1 (REG_SZ) before the call.
- If both succeed, it calls `_chdir(buf)` and logs "WinMain() - CWD changed to %s" or "WinMain() - chdir(%s) Failed" into 0xde1f64.
- There is no default value. If the key is missing, the current folder stays as it is. The key is never closed.

Port: the Mac program should `chdir` to the game folder (the folder that holds `Data` and `Pictures`) before the recompiled entry point runs. Then `RegOpenKeyExA` can fail.

### 4.5 Command-line options

0x5cc6d0 copies the command line to 0xae2c98 and lowercases it. Each option is a `strstr` (0x945a60) on that copy. Options without a leading dash match anywhere in the line.

| Option | Read at | Effect (code) |
|---|---|---|
| `nomemcheck` | 0x5cc799 | clears 0x9cd1fc. The RAM checks then pass (0x81e001) |
| `-windowed` | 0x5d7973 | `prefs.0x5d7c60(0x10)` [meaning of the bit not read] |
| `-nosound` | 0x5d7993 | `prefs.0x5d7c60(0x100)` (the same bit the sound failure sets) |
| `-defaultfont` | 0x5d79b5 | prefs+0x217 = -2 (font mode -2: the T2K font engine) |
| `-seed <n>` | 0x81b963 | random seed from `sscanf("%d")`, else the `clock()` value |
| `-nonetwork` | 0x81b9ba | 0xa801c4 = 0: hides "Network Play" |
| `-load` | 0x81bdb1 | loads a saved game named after it [inf: the parse at 0x81bdcb..0x81bdd9 was read only in part] |
| `-dontcheckram` | 0x81dfeb, 0x81e1a7, 0x81e332, 0x81e540 | skips the memory check on league choice |
| `-media` | 0x7da357 | writes `media.out` (debug) |
| `-ignoreenv` | 0x90bc52 | the folder lookup ignores `CM3_*` environment variables |
| `-verifypics` | 0x41b4ae, 0x8282f6 | picture checks (debug) |
| `-verifysounds` | 0x6da68c | debug |
| `-verifyhist`, `-verifyplcfg`, `-verifyeurocfg` | 0x828311, 0x82832c, 0x828351 | debug checks |
| `norealplhist` | 0x7c028d | [not read] |
| `checkgks` | 0x7c0424 | [not read] |

### 4.6 Files and folders

Folder lookup 0x90bc50(key) returns the environment variable of that name. With `-ignoreenv` or no variable, it returns a fixed default:

| Key | Default folder |
|---|---|
| SI_DATA, CM3_DATA, CM3_TACTICS | `data` |
| CM3_TEMP, CM3_DEBUG, CM3_PREFS | the current folder (empty string 0xad9c64) |
| CM3_TRAINING | `training` |
| CM3_HISTORY | `history` |
| CM3_PICTURES | `pictures` |
| CM3_SOUNDS | `sounds` |
| CM3_HELP | `help` |
| CM3_QSTART | `qstart` |
| other | NULL |

0x90bde0(key, name) joins folder, `\` (0x9c3730) and name into the static buffer 0xde279c. All paths are relative, with `\`. Every file is opened through the CRT (`fopen` 0x945b00, `_open`, `CreateFileA`). The only exception is `LoadLibraryA` of cpuinf32.dll.

Files by group (string addresses):

- `data`:
  - the database `*.dat`: `index.dat` 0x9b5c48, `player.dat` 0x9b5c98, `nonplayer.dat`, `club.dat` and others;
  - the seven `*.fnt` fonts (0x9cd9bc..0x9cda30);
  - `language.ldb` and `*.lng`;
  - the `*.t2k` fonts;
  - `pics.cfg`, `fancluburls.txt`, `cpuinf32.dll`.
- `pictures`: background and other pictures. `2026-10-13-display-and-scrman-inventory.md` covers them.
- `sounds`: `intro1.wav`, `intro2.wav` (0x9cd25c, 0x9cd268), `fxloop2.wav` (0xa159a0).
- Current folder:
  - `game.cfg` (preferences, read at 0x5d75ef, deleted at 0x5d7953 when bad), `allowed_fonts.txt` (0x5d73b4);
  - `hall_of_fame.bin`, saves `*.sav`, `Preferences.dat`;
  - temp files `*.tmp`, for example `fixtures_%d.tmp` 0x9cc238 and `%s_%d.tmp` 0x99b838;
  - `~printout.txt`, `media.out`, `duff.out`.
- Network: `~net.out` (0xa8c74c).

Which folder key each `.dat` and `.tmp` uses was not read file by file.

CD check: 35 functions build `<drive>cm0102.exe` with `sprintf("%s%c%c%c%c%c%c%c%c%c%c")` (0x987e00). Examples are 0x413180 and 0x68dbf0. They call `_findfirst` through 0x4c0aa0 on every drive of type 5. They loop on "Please insert the CM01/02 CD and click on OK" (0x987dc4) with no limit until a match is found (0x413284, 0x68dcec).

Replacement: report "D:\" as a CD drive and map `D:\cm0102.exe` to the player's own cm0102.exe. `FindFirstFileA` then succeeds. `2026-10-14-game-creation-inventory.md` says the hand port must leave the check out. With a bulk recompile, leaving it out needs a patch of 35 functions, so emulating the drive is simpler.

## 5. The CRT

The CRT is statically linked. There is no msvcrt import. The CRT code sits at 0x944c94..0x9533ff. Ghidra's function ID names it "Visual Studio 1998" (MSVC 6), for example `__strcmpi` at 0x951c50 and `_strncpy` at 0x9466d0. The start code 0x947a13 is the MSVC 6 `WinMainCRTStartup`.

The CRT functions that game code calls, with the number of game call sites (`E8` from below 0x944c00). Names come from Ghidra function ID, from `tests/diff/original.py`, or from reading the code. The names marked [inf] were matched by their shape.

| Address | Function | Game calls | Imports below it |
|---|---|---|---|
| 0x944cff | `_splitpath` | 6409 | none |
| 0x944c9f | `sprintf` | 5195 | none |
| 0x945640 | `_ftol` | 1949 | none (x87) |
| 0x9452ca | `free` | 1761 | HeapFree |
| 0x944cf1 | `operator new` | 1524 | HeapAlloc |
| 0x951c50 | `_strcmpi` | 1491 | none in the C locale |
| 0x944e46 | `malloc` | 1219 | HeapAlloc |
| 0x944c94 | `operator delete` | 843 | HeapFree |
| 0x945bfe | `fread` | 645 | ReadFile |
| 0x945934 | `exit` | 451 | ExitProcess |
| 0x945ce6 | `fwrite` | 388 | WriteFile |
| 0x945333 | `qsort` | 322 | none |
| 0x945a09 | `fclose` | 201 | CloseHandle |
| 0x9464f0 | `fprintf` | 136 | WriteFile |
| 0x945501 | `calloc` | 135 | HeapAlloc (flag 8) |
| 0x9462bb | `eh vector constructor iterator` | 133 | none |
| 0x946fa0 | `strncmp` [inf] | 130 | none |
| 0x9466d0 | `strncpy` | 112 | none |
| 0x945b00 | `fopen` | 101 | CreateFileA, GetFileType |
| 0x945667 | `realloc` | 97 | HeapAlloc, HeapReAlloc |
| 0x945610 | `_chkstk` | 96 | none |
| 0x945ff0 | x87 `pow` helper [inf] | 92 | none |
| 0x9463af | `eh vector constructor iterator`, second form [inf] | 89 | none |
| 0x945a60 | `strstr` | 87 | none |
| 0x946a22 | `fseek` | 85 | SetFilePointer |
| 0x946464 | `bsearch` | 76 | none |
| 0x9469e0 | `fgetc` | 61 | ReadFile |
| 0x94627a | `atexit` | 50 | HeapAlloc, HeapReAlloc (table growth) [inf] |
| 0x945e80 | `_alldiv` | 46 | none |
| 0x946a04 | `rand` (state 0xad6c30) | 39 | none |
| 0x945e40 | `_allmul` | 25 | none |
| 0x94643a | `remove` | 24 | DeleteFileA |
| 0x94791c | `longjmp` | 20 | RtlUnwind |
| 0x946fd8 | `sscanf` | 20 | none |
| 0x946aae | `ftell` | 20 | SetFilePointer |
| 0x946ebb | `time` | 19 | GetLocalTime, GetSystemTime, GetTimeZoneInformation |
| 0x9471cd | `toupper` | 18 | none in the C locale |
| 0x947998 | `_setjmp3` (tag "VC20" at 0x9479b0) | 11 | none |
| 0x94700c | `fgets` [inf] | 7 | ReadFile |
| 0x9469fa | `srand` | 7 | none |
| 0x952f5b | `_strdup` | 6 | HeapAlloc |
| 0x946540 | `strchr` | 4 | none |
| 0x945bd2 | `_mkdir` | 3 | CreateDirectoryA |
| 0x946898 | `_findnext` | 3 | FindNextFileA |
| 0x9532a6 | `_strlwr` | 2 | none in the C locale |
| 0x947480 | `getenv` | 2 | none (reads the copy made at start) |
| 0x9470ce | `vsprintf` [inf] | 2 | none |
| 0x947299 | `clock` | 1 | GetSystemTimeAsFileTime |
| 0x9467ce | `_findfirst` | 1 | FindFirstFileA |
| 0x94695d | `_findclose` | 1 | FindClose |
| 0x9475b5 | `_stat` | 1 | FindFirstFileA, GetDriveTypeA, FileTimeTo… |
| 0x9478d8 | `_access` | 1 | GetFileAttributesA |
| 0x9473a7 | `_getdrive` | 1 | GetCurrentDirectoryA |
| 0x946da0 | `_getcwd` | 1 | GetCurrentDirectoryA, GetFullPathNameA |
| 0x945ba4 | `rename` | 1 | MoveFileA |
| 0x953344 | `_chdir` | 1 | SetCurrentDirectoryA, GetCurrentDirectoryA, SetEnvironmentVariableA |
| 0x9473ea | `system` | 1 (network) | CreateProcessA and others |

The last column gives the main import of each function. A static walk of the CRT call graph also reaches the heap imports from every function that allocates a buffer, for example `fopen` and `fgetc`. It reaches the error-path imports (`GetModuleFileNameA`, `WriteFile` to the standard error handle, `ExitProcess`) from `fprintf` and `atexit`. It reaches the locale imports from `fprintf`. The walk uses function starts taken from call targets, so it can over-report.

The math functions are CRT code on the x87 stack and use no import [inf: no math import exists, and `_ftol` and the `pow` helper call no import].

Game code never reaches into a `FILE` directly. The buffer refill `_filbuf` (0x94b307) and flush `_flsbuf` (0x947b52) have only CRT callers, so no inlined `getc` or `putc` macro exists in game code.

Two ways to treat the CRT:

1. Recompile it with the rest and implement the low-level imports of section 1.1. Output then matches the original by construction: `sprintf` float text, `qsort` order, `rand`, the `_ftol` rounding and the 16-byte size rounding in `malloc` (0x944ee1). This needs about 35 KERNEL32 replacements.
2. Replace selected CRT functions by address with host versions, as the Unicorn harness does. This is faster, but `sprintf`, `sscanf` and `qsort` can differ from MSVC 6.

Recommendation: option 1 for everything, with one exception. The runtime may replace `malloc`, `free`, `realloc`, `calloc`, `operator new` and `operator delete` by address if the Heap replacements prove slow. The Heap path is three functions deep (0x944e46 → 0x944ef0 → HeapAlloc).

## 6. Order of work to reach the start screen

The aim of each step is that the recompiled program runs until the next missing import stops it with that import's name.

1. Runtime core. This is not an import, but every step needs it:
   - flat memory with the image at 0x400000;
   - a thread information block for `fs:[0]`;
   - the indirect-call table;
   - a trap for each import slot that names the import;
   - a way for a host function to call a generated function (needed for the window procedure);
   - a way for generated code to call a host function through a fake address (needed for `GetProcAddress` results and COM vtables).
2. CRT start, in the order 0x947a13 calls them:
   1. `GetVersion`, `HeapCreate`, `GetVersionExA` (NT 5.1);
   2. `HeapAlloc`, `HeapFree`, `HeapReAlloc`, `HeapSize`;
   3. `GetStartupInfoA`, `GetStdHandle`, `GetFileType`, `SetHandleCount`;
   4. `GetCommandLineA`, `GetEnvironmentStringsW` (NULL), `GetEnvironmentStrings`, `FreeEnvironmentStringsA`;
   5. `GetModuleFileNameA`, `GetACP`, `GetOEMCP`, `GetCPInfo`, `GetModuleHandleA`, `GetProcAddress` ("IsProcessorFeaturePresent");
   6. `GetSystemTimeAsFileTime`, `GetLocalTime`, `GetSystemTime`, `GetTimeZoneInformation`;
   7. `ExitProcess`.
3. WinMain: `CreateMutexA`, `GetLastError`, `RegOpenKeyExA` (fail), `CloseHandle`, `MessageBoxA`.
4. File layer: `CreateFileA`, `ReadFile`, `WriteFile`, `SetFilePointer`, `CloseHandle`, `GetFileAttributesA`, `DeleteFileA`, with path mapping (backslash, case, `D:\`). `game.cfg`, `allowed_fonts.txt`, `language.ldb` and the fonts load here.
5. Preference setup: `GetKeyboardLayout`, `GetSystemInfo`, `LoadLibraryA` (NULL for cpuinf32.dll), `FreeLibrary`, `GlobalMemoryStatus`, `EnumPrintersA` (FALSE).
6. Display:
   1. `GetCursor`, `LoadCursorA`, `ShowCursor`, `SetCursor`, `LoadIconA`;
   2. `RegisterClassA`, `GetSystemMetrics`, `GetDesktopWindow`, `GetWindowRect`, `CreateWindowExA`, `UpdateWindow`, `ShowWindow`;
   3. `DirectDrawCreate` and the DirectDraw methods of section 2.1, with the flip shown through SDL.
7. Sound: `DirectSoundCreate` and the silent objects. Or start with `-nosound` until then.
8. Message loop: `PeekMessageA`, `GetMessageA`, `TranslateMessage`, `DispatchMessageA`, `DefWindowProcA`, `BeginPaint`, `EndPaint`, with the SDL event to Windows message mapping of `2026-10-13-input-inventory.md`. The start screen should now show and take clicks.
9. After the start screen, for slice 3's manual check:
   1. `GlobalAlloc` and `GlobalFree`;
   2. the CD drive (`GetLogicalDriveStringsA`, `GetDriveTypeA`, `FindFirstFileA`, `FindNextFileA`, `FindClose`, `FileTimeTo…`);
   3. `LoadLibraryA("KERNEL32.DLL")` with `GetProcAddress("GetDiskFreeSpaceExA")`;
   4. `CreateDirectoryA`, `MoveFileA`, `GetCurrentDirectoryA`, `GetFullPathNameA`;
   5. then real sound.

## 7. Open doubts

1. The scans can miss a site. The IAT byte search finds every direct reference. The `mov reg,[iat]` follow-up counts `call reg` only up to the next write to `reg` or 400 instructions. The COM scan follows only `mov` chains of up to about 45 instructions. The DirectDraw globals were checked against every address reference (0xaebde4: 48 references, 0xae31a4: 12, 0xae3194: 11). Lock flags at 0x5e5235 and 0x5e78fe were not read.
2. The WSOCK32 and DSOUND names come from the standard ordinals, not from the file.
3. The meaning of the preference bits that `-windowed` (0x10), `-nosound` (0x100) and the CPU and RAM classes (0x5d7bc0: 1, 2, 0x10000, 0x20000, 0x40000) set through 0x5d7c60 was not read. The full-screen flag comes from bit 3 of prefs+4. How `-windowed` reaches that bit is unknown.
4. Whether any import other than those in section 6 runs before the start screen is unknown. The start path was read statically, not traced. The game-creation trace began after start-up. A run of the recompiled program with trapping stubs will show it.
5. The `GetKeyboardLayout` value that a UK player had, and so the language table that the original chose, is unknown. Set it from Max's machine or a setting.
6. `SetVolume(10000)` at 0x84cc4b is out of the DirectSound range. What DirectSound did with it on the player's PC is unknown. The safe replacement keeps the volume unchanged.
7. Whether the game ever calls `setlocale`, which would reach the `LCMapString` and `CompareString` imports, is unknown. No game caller of those CRT paths was found.
8. `0x5e8280(1)` in WinMain and `0x5a8f60` on deactivate were not read.
9. Whether network code runs before the start screen is unknown. `WSAStartup` has only the two callers 0x77a7d0 and 0x77a9f0. Their callers were not followed.
10. The exact folder key of each data and temp file was not listed. Only `game.cfg` (CM3_PREFS), the intro sounds (CM3_SOUNDS) and `cpuinf32.dll` (CM3_DATA) were read.
