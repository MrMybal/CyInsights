; Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!ifndef PAYLOAD
  !error "Build through Tools/package_release.py --nsis <makensis.exe>"
!endif
Name "CyInsights ${PRODUCT_VERSION}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\CyInsights"
InstallDirRegKey HKCU "Software\CyInsights\Installer" "InstallDir"
RequestExecutionLevel user
SetCompressor zlib
SetOverwrite on
BrandingText "CyInsights"
VIProductVersion "${NUMERIC_VERSION}.0"
VIAddVersionKey "ProductName" "CyInsights"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "FileDescription" "CyInsights Windows installer"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2026 Cyberalien"
!define MUI_ICON "${PRODUCT_ICON}"
!define MUI_UNICON "${PRODUCT_ICON}"
!define MUI_ABORTWARNING
!define MUI_LANGDLL_REGISTRY_ROOT HKCU
!define MUI_LANGDLL_REGISTRY_KEY "Software\CyInsights\Installer"
!define MUI_LANGDLL_REGISTRY_VALUENAME "Language"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PRODUCT_LICENSE}"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\CyInsightsApp.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"
LangString MainSection ${LANG_ENGLISH} "CyInsights and offline documentation (required)"
LangString MainSection ${LANG_FRENCH} "CyInsights et documentation hors ligne (requis)"
LangString DesktopSection ${LANG_ENGLISH} "Desktop shortcut"
LangString DesktopSection ${LANG_FRENCH} "Raccourci sur le bureau"
LangString OpenWithSection ${LANG_ENGLISH} "Add CyInsights to Open with for trace files"
LangString OpenWithSection ${LANG_FRENCH} "Ajouter CyInsights à Ouvrir avec pour les traces"
LangString WrongArchitecture ${LANG_ENGLISH} "CyInsights requires 64-bit Windows."
LangString WrongArchitecture ${LANG_FRENCH} "CyInsights nécessite Windows 64 bits."
LangString PortableFolder ${LANG_ENGLISH} "This folder contains a portable edition. Choose another installation folder."
LangString PortableFolder ${LANG_FRENCH} "Ce dossier contient une édition portable. Choisissez un autre dossier d'installation."
LangString InUse ${LANG_ENGLISH} "Close CyInsights and its capture tools before installing or uninstalling."
LangString InUse ${LANG_FRENCH} "Fermez CyInsights et ses outils de capture avant l'installation ou la désinstallation."

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "$(WrongArchitecture)"
    SetErrorLevel 1
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext current
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Function .onVerifyInstDir
  SetErrorLevel 0
  IfFileExists "$INSTDIR\cyinsights-portable.txt" 0 allowed
    SetErrorLevel 3
    Abort
  allowed:
  IfFileExists "$INSTDIR\cyinsights-installed.txt" valid
  IfFileExists "$INSTDIR\CyInsightsApp.exe" valid
  StrCpy $1 ""
  ClearErrors
  FindFirst $0 $1 "$INSTDIR\*"
  IfErrors valid
  loop:
    StrCmp $1 "" empty
    StrCmp $1 "." next
    StrCmp $1 ".." next
    FindClose $0
    SetErrorLevel 5
    Abort
  next:
    FindNext $0 $1
    IfErrors empty
    Goto loop
  empty:
    FindClose $0
  valid:
FunctionEnd

!macro CheckUnlocked NAME PREFIX
  IfFileExists "$INSTDIR\${NAME}" 0 ${PREFIX}_unlocked
  System::Call 'kernel32::CreateFileW(w "$INSTDIR\${NAME}", i 0x40000000, i 0, p 0, i 3, i 0, p 0) p .r0'
  ${If} $0 == -1
    MessageBox MB_ICONSTOP "$(InUse)" /SD IDOK
    SetErrorLevel 2
    Abort
  ${EndIf}
  System::Call 'kernel32::CloseHandle(p r0)'
  ${PREFIX}_unlocked:
!macroend

Section "$(MainSection)" Main
  SectionIn RO
  IfFileExists "$INSTDIR\cyinsights-portable.txt" 0 proceed
    MessageBox MB_ICONSTOP "$(PortableFolder)" /SD IDOK
    SetErrorLevel 3
    Abort
  proceed:
  Call .onVerifyInstDir
  !insertmacro CheckUnlocked "CyInsightsApp.exe" app
  !insertmacro CheckUnlocked "CyCaptureTool.exe" capture
  !insertmacro CheckUnlocked "CyInsightsUpdater.exe" updater
  !include "${PAYLOAD}\files.nsh"
  SetOutPath "$INSTDIR"
  FileOpen $0 "$INSTDIR\cyinsights-installed.txt" w
  FileWrite $0 "CyInsights per-user installation$\r$\n"
  FileClose $0
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\CyInsights"
  CreateShortcut "$SMPROGRAMS\CyInsights\CyInsights.lnk" "$INSTDIR\CyInsightsApp.exe"
  CreateShortcut "$SMPROGRAMS\CyInsights\User manual.lnk" "$INSTDIR\Documentation\Documentation.html"
  CreateShortcut "$SMPROGRAMS\CyInsights\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\CyInsights\Installer" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "DisplayName" "CyInsights"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "DisplayIcon" "$INSTDIR\CyInsightsApp.exe,0"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "URLInfoAbout" "https://mrmybal.github.io/CyInsights/"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights" "NoRepair" 1
SectionEnd

Section /o "$(DesktopSection)" Desktop
  CreateShortcut "$DESKTOP\CyInsights.lnk" "$INSTDIR\CyInsightsApp.exe"
  WriteRegDWORD HKCU "Software\CyInsights\Installer" "DesktopShortcut" 1
SectionEnd

Section /o "$(OpenWithSection)" OpenWith
  WriteRegStr HKCU "Software\Classes\CyInsights.Trace" "" "CyInsights trace"
  WriteRegStr HKCU "Software\Classes\CyInsights.Trace\DefaultIcon" "" '$\"$INSTDIR\CyInsightsApp.exe$\",0'
  WriteRegStr HKCU "Software\Classes\CyInsights.Trace\shell\open\command" "" '$\"$INSTDIR\CyInsightsApp.exe$\" $\"%1$\"'
  WriteRegStr HKCU "Software\Classes\.utrace\OpenWithProgids" "CyInsights.Trace" ""
  WriteRegStr HKCU "Software\Classes\.cycapture\OpenWithProgids" "CyInsights.Trace" ""
SectionEnd

Function un.onInit
  SetRegView 64
  SetShellVarContext current
  !insertmacro MUI_UNGETLANGUAGE
  IfFileExists "$INSTDIR\cyinsights-installed.txt" valid
    SetErrorLevel 4
    Abort
  valid:
FunctionEnd

Section "Uninstall"
  !insertmacro CheckUnlocked "CyInsightsApp.exe" unapp
  !insertmacro CheckUnlocked "CyCaptureTool.exe" uncapture
  !insertmacro CheckUnlocked "CyInsightsUpdater.exe" unupdater
  ; Generated explicit file list. Never recursively remove user data/captures.
  !include "${PAYLOAD}\uninstall.nsh"
  Delete "$INSTDIR\cyinsights-installed.txt"
  Delete "$INSTDIR\Uninstall.exe"
  ReadRegStr $0 HKCU "Software\CyInsights\Installer" "InstallDir"
  ${If} $0 == $INSTDIR
    Delete "$SMPROGRAMS\CyInsights\CyInsights.lnk"
    Delete "$SMPROGRAMS\CyInsights\User manual.lnk"
    Delete "$SMPROGRAMS\CyInsights\Uninstall.lnk"
    RMDir "$SMPROGRAMS\CyInsights"
    ReadRegDWORD $0 HKCU "Software\CyInsights\Installer" "DesktopShortcut"
    ${If} $0 == 1
      Delete "$DESKTOP\CyInsights.lnk"
    ${EndIf}
    DeleteRegValue HKCU "Software\Classes\.utrace\OpenWithProgids" "CyInsights.Trace"
    DeleteRegValue HKCU "Software\Classes\.cycapture\OpenWithProgids" "CyInsights.Trace"
    DeleteRegKey HKCU "Software\Classes\CyInsights.Trace"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyInsights"
    DeleteRegKey HKCU "Software\CyInsights\Installer"
  ${EndIf}
  RMDir "$INSTDIR"
SectionEnd
