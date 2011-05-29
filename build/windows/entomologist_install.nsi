; Entomologist_install.nsi 
; This script for NSIS creates an installer for Entomologist. 
;  It registers entomologist with the registry, adds it to the start menu and creates
;  an Uninstall script.
;--------------------------------

; The name of the installer
Name "Entomologist Bug Tracker Installer"

; The file to write
OutFile "entomologist_install.exe"

; The default installation directory
InstallDir $PROGRAMFILES\entomologist

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\entomologist" "Install_Dir"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------

; Pages

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "entomologist (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  CreateDirectory sqldrivers
  ; Put dependant dlls in Directory
  File "entomologist.exe"
  File "QtGui4.dll"
  File "QtCore4.dll"
  File "QtNetwork4.dll"
  File "QtSql4.dll"
  File "QtXml4.dll"
  File "mingwm10.dll"
  File "libgcc_s_dw2-1.dll"
  File "libeay32.dll"
  File "libssl32.dll"
  File "ssleay32.dll"

  SetOutPath $INSTDIR\sqldrivers
  File "qsqlite4.dll"

  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\entomologist "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\entomologist" "DisplayName" "entomologist"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\entomologist" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\entomologist" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\entomologist" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\entomologist"
  CreateShortCut "$SMPROGRAMS\entomologist\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\entomologist\entomologist.lnk" "$INSTDIR\entomologist.exe" "" "$INSTDIR\entomologist.exe" 0
  
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\entomologist"
  DeleteRegKey HKLM SOFTWARE\entomologist

  ; Remove files and uninstaller
  Delete $INSTDIR\entomologist.exe
  Delete $INSTDIR\QtCore4.dll
  Delete $INSTDIR\QtGui4.dll
  Delete $INSTDIR\QtNetwork4.dll
  Delete $INSTDIR\QtXml4.dll
  Delete $INSTDIR\QtSql4.dll
  Delete $INSTDIR\sqldrivers\qsqlite4.dll
  Delete $INSTDIR\mingwm10.dll
  Delete $INSTDIR\libgcc_s_dw2-1.dll
  Delete $INSTDIR\uninstall.exe

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\entomologist\*.*"

  ; Remove directories used
  RMDir "$INSTDIR\sqldrivers"
  RMDir "$SMPROGRAMS\entomologist"
  RMDir "$INSTDIR"

SectionEnd
