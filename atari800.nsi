!include "MUI2.nsh"

!ifndef VERSION
  !define VERSION "7.0.0"
!endif
!ifndef PLATFORM
  !define PLATFORM "win64"
!endif

Name "Atari800 ${VERSION}"
OutFile "atari800-${VERSION}-${PLATFORM}.exe"
InstallDir "$PROGRAMFILES\Atari800"
InstallDirRegKey HKLM "Software\Atari800" ""

!define MUI_ICON "data\atari800.ico"
!define MUI_UNICON "data\atari800.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "pkg\doc\COPYING.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\atari800.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Atari800" SecMain
  SetOutPath "$INSTDIR"
  File /r "pkg\*"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\Atari800"
  CreateShortCut "$SMPROGRAMS\Atari800\Atari800.lnk" "$INSTDIR\atari800.exe"
  CreateShortCut "$SMPROGRAMS\Atari800\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atari800" \
    "DisplayName" "Atari800 ${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atari800" \
    "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atari800" \
    "DisplayVersion" "${VERSION}"
SectionEnd

Section "Uninstall"
  RMDir /r "$INSTDIR"
  RMDir /r "$SMPROGRAMS\Atari800"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atari800"
  DeleteRegKey HKLM "Software\Atari800"
SectionEnd
