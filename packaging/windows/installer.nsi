; The Windows installer of CM 01/02: Bosman Edition (NSIS 3, made on the Mac with makensis).
; packaging/windows/windows.cmake runs it with these defines:
;   VERSION   the project version, for example 0.1.0
;   EXE       the 64-bit cm0102_recomp.exe
;   GAME_DIR  the folder of the clean files (cm0102.exe, Data, Pictures and the rest)
;   OUT_FILE  the setup exe to write
;   ICON      optional: the original's ball icon (tools/icon/make_icons.py), for the setup exe
;
; Layout: $INSTDIR holds the program and the uninstaller, $INSTDIR\game the clean files. The program
; finds the game folder beside itself. Saves go to %APPDATA%\cm0102. An update keeps them. The
; uninstaller never deletes them and never asks.

!ifndef VERSION
  !error "Give VERSION with -DVERSION=..."
!endif
!ifndef EXE
  !error "Give EXE with -DEXE=..."
!endif
!ifndef GAME_DIR
  !error "Give GAME_DIR with -DGAME_DIR=..."
!endif
!ifndef OUT_FILE
  !error "Give OUT_FILE with -DOUT_FILE=..."
!endif
!if "${GAME_DIR}" == ""
  !error "Set CM0102_GAME_DIR to the folder of the clean files"
!endif

Unicode true
SetCompressor /SOLID lzma
ManifestDPIAware true

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!define PRODUCT "CM 01/02: Bosman Edition"
!define FILE_NAME "CM 01-02 Bosman Edition"
!define PROGRAM "${FILE_NAME}.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\CM0102BosmanEdition"
!define APP_KEY "Software\CM0102BosmanEdition"

Name "${PRODUCT}"
OutFile "${OUT_FILE}"
InstallDir "$PROGRAMFILES64\${FILE_NAME}"
RequestExecutionLevel admin
BrandingText "${PRODUCT} ${VERSION}"

VIProductVersion "${VERSION}.0"
VIFileVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "${PRODUCT}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "FileDescription" "${PRODUCT} ${VERSION} Setup"
VIAddVersionKey /LANG=1033 "Comments" "An unofficial port of Championship Manager 01/02"
VIAddVersionKey /LANG=1033 "LegalCopyright" "An unofficial fan project"

!ifdef ICON
  !define MUI_ICON "${ICON}"
  !define MUI_UNICON "${ICON}"
!endif
!define MUI_ABORTWARNING
; The game starts through explorer.exe, so that it runs as the logged-in user and not as the
; elevated installer.
!define MUI_FINISHPAGE_RUN ""
!define MUI_FINISHPAGE_RUN_FUNCTION StartGame
!define MUI_FINISHPAGE_RUN_TEXT "Start the game"

!define MUI_PAGE_CUSTOMFUNCTION_LEAVE CheckFolder
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!define MUI_FINISHPAGE_TEXT "${PRODUCT} is removed.$\r$\n$\r$\nYour saved games and settings stay in:$\r$\n$APPDATA\cm0102$\r$\n$\r$\nThe uninstaller does not delete them. Delete that folder yourself to remove them."
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "${PRODUCT} needs 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
  ; An upgrade goes to the folder of the earlier install, also when the player chose it. A folder
  ; given with /D= on the command line wins.
  ${If} $INSTDIR == "$PROGRAMFILES64\${FILE_NAME}"
    ReadRegStr $0 HKLM "${UNINSTALL_KEY}" "InstallLocation"
    ${If} $0 != ""
      StrCpy $INSTDIR $0
    ${EndIf}
  ${EndIf}
FunctionEnd

Function StartGame
  Exec '"$WINDIR\explorer.exe" "$INSTDIR\${PROGRAM}"'
FunctionEnd

; A folder with a "game" folder and no program of ours holds somebody else's files.
Function CheckFolder
  ${If} ${FileExists} "$INSTDIR\game"
  ${AndIfNot} ${FileExists} "$INSTDIR\${PROGRAM}"
    MessageBox MB_OK|MB_ICONSTOP "This folder already holds a folder called $\"game$\". Choose another folder." /SD IDOK
    Abort
  ${EndIf}
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "Install"
  ; A silent install skips the folder page, so check here too.
  Call CheckFolder

  ; Only a folder of ours has a program in it. An update replaces the clean files as a whole.
  ${If} ${FileExists} "$INSTDIR\${PROGRAM}"
    RMDir /r "$INSTDIR\game"
  ${EndIf}
  SetOutPath "$INSTDIR"
  File "/oname=${PROGRAM}" "${EXE}"
  SetOutPath "$INSTDIR\game"
  File /r "${GAME_DIR}/*"
  ; The licences of the libraries in the program (packaging/licences).
  SetOutPath "$INSTDIR\licences"
  File "${LICENCES}\mbedtls-LICENSE.txt"
  SetOutPath "$INSTDIR"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  SetShellVarContext all
  CreateShortCut "$SMPROGRAMS\${FILE_NAME}.lnk" "$INSTDIR\${PROGRAM}" "" "$INSTDIR\${PROGRAM}" 0
  CreateShortCut "$DESKTOP\${FILE_NAME}.lnk" "$INSTDIR\${PROGRAM}" "" "$INSTDIR\${PROGRAM}" 0

  WriteRegStr HKLM "${APP_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "An unofficial fan project"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${PROGRAM}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  Delete "$SMPROGRAMS\${FILE_NAME}.lnk"
  Delete "$DESKTOP\${FILE_NAME}.lnk"

  Delete "$INSTDIR\${PROGRAM}"
  RMDir /r "$INSTDIR\game"
  RMDir /r "$INSTDIR\licences"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "${UNINSTALL_KEY}"
  DeleteRegKey HKLM "${APP_KEY}"

  ; The saves in $APPDATA\cm0102 stay. The last page says where they are.
  SetShellVarContext current

  ; The cm0102:// link of invite links (0.7.0). The game writes it for the user who plays (host/link.c). Remove it
  ; when it starts this copy of the game: a link of another copy stays.
  ReadRegStr $0 HKCU "Software\Classes\cm0102\shell\open\command" ""
  ${If} $0 == '"$INSTDIR\${PROGRAM}" "%1"'
    DeleteRegKey HKCU "Software\Classes\cm0102"
  ${EndIf}
SectionEnd
