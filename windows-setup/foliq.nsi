; Foliq NSIS installation script for Windows
; Derived from the Xournal++ installer (The Xournal++ Team), adapted for Foliq 2026-11-11

;--------------------------------
; NSIS setup

Unicode true

;--------------------------------
; Includes

!ifdef APP_ICON
!define MUI_ICON "${APP_ICON}"
!define MUI_UNICON "${APP_ICON}"
!endif
!include "MUI2.nsh"
!include x64.nsh
!include "${SCRIPT_DIR}\FileAssociation.nsh"
!include nsDialogs.nsh

; Options for MultiUser plugin
!define MULTIUSER_INSTALLMODE_INSTDIR "Foliq"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "Software\Foliq"

!define MULTIUSER_EXECUTIONLEVEL Highest ; Mixed-mode installer that can both be per-machine or per-user
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_USE_PROGRAMFILES64
!include MultiUser.nsh


;--------------------------------
; Initialization

Function .onInit
	${If} ${RunningX64}
		# 64 bit code
		SetRegView 64
	${Else}
		# 32 bit code
		MessageBox MB_OK "Foliq requires 64-bit Windows. Sorry!"
		Abort
	${EndIf}

	!insertmacro MULTIUSER_INIT
FunctionEnd

Function un.onInit
	${If} ${RunningX64}
		# 64 bit code
		SetRegView 64
	${Else}
		# 32 bit code
		MessageBox MB_OK "Foliq requires 64-bit Windows. Sorry!"
		Abort
	${EndIf}

    !insertmacro MULTIUSER_UNINIT
FunctionEnd

; Name and file
Name "Foliq ${XOURNALPP_VERSION}"
OutFile "${OUTPUT_INSTALLER_FILE}"

;--------------------------------
; Global Variables

Var StartMenuFolder

;--------------------------------
; Interface Settings

!define MUI_ABORTWARNING

;--------------------------------
; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SCRIPT_DIR}\..\LICENSE"
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY

;Start Menu Folder Page Configuration
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "SHCTX"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Foliq"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "StartMenuEntry"
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Foliq"

!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder

!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
; Languages

!insertmacro MUI_LANGUAGE "English"

;-------------------------------
; Uninstall previous version

Section "" SecUninstallPrevious
	ReadRegStr $R0 SHCTX "Software\Foliq" ""
	${If} $R0 == ""
		SetRegView 64
	${ENDIF}
	${If} $R0 != ""
        DetailPrint "Removing previous version located at $R0"
		ExecWait '"$R0\Uninstall.exe /S"'

    ${EndIf}
SectionEnd

;-------------------------------
; File association macros
; see https://docs.microsoft.com/en-us/windows/win32/shell/fa-file-types#registering-a-file-type

!macro SetDefaultExt EXT PROGID
	WriteRegStr SHCTX "Software\Classes\${EXT}" "" "${PROGID}"
!macroend

!macro RegisterExt EXT PROGID
	WriteRegStr SHCTX "Software\Classes\${EXT}\OpenWithProgIds" "${PROGID}" ""
!macroend

!macro AddProgId PROGID CMD DESC
	; Define ProgId. See https://docs.microsoft.com/en-us/windows/win32/shell/fa-progids
	WriteRegStr SHCTX "Software\Classes\${PROGID}" "" "${DESC}"
	WriteRegStr SHCTX "Software\Classes\${PROGID}\DefaultIcon" "" '"${CMD}",0'
	WriteRegStr SHCTX "Software\Classes\${PROGID}\shell" "" "open"
	WriteRegStr SHCTX "Software\Classes\${PROGID}\shell\open\command" "" '"${CMD}" "%1"'
	WriteRegStr SHCTX "Software\Classes\${PROGID}\shell\edit" "" "Edit with Foliq"
	WriteRegStr SHCTX "Software\Classes\${PROGID}\shell\edit\command" "" '"${CMD}" "%1"'
!macroend

!macro DeleteProgId PROGID
	; See https://docs.microsoft.com/en-us/windows/win32/shell/fa-file-types#deleting-registry-information-during-uninstallation
	DeleteRegKey SHCTX "Software\Classes\${PROGID}"
!macroend

!define SHCNE_ASSOCCHANGED 0x08000000
!define SHCNE_CREATE 0x2
!define SHCNE_DELETE 0x4

!define SHCNF_IDLIST 0x0
!define SHCNF_PATH 0x1
!define SHCNF_FLUSH 0x1000

!macro RefreshShellIconCreate FILEPATH
	DetailPrint "Refreshing shell icon create ${FILEPATH}"
	System::Call 'shell32::SHChangeNotify(i ${SHCNE_CREATE}, i ${SHCNF_FLUSH} | ${SHCNF_PATH}, w "${FILEPATH}", i 0)'
!macroend

!macro RefreshShellIconDelete FILEPATH
	DetailPrint "Refreshing shell icon delete ${FILEPATH}"
	System::Call 'shell32::SHChangeNotify(i ${SHCNE_DELETE}, i ${SHCNF_FLUSH} | ${SHCNF_PATH}, w "${FILEPATH}", i 0)'
!macroend

;-------------------------------
; Installer Sections

Section "Associate .xopp files with Foliq" SecFileXopp
	!insertmacro SetDefaultExt ".xopp" "Foliq.File"
SectionEnd

Section "Associate .xopt files with Foliq" SecFileXopt
	!insertmacro SetDefaultExt ".xopt" "Foliq.Template"
SectionEnd

Section "Associate .xoj files with Foliq" SecFileXoj
	!insertmacro SetDefaultExt ".xoj" "Foliq.Xournal"
SectionEnd

Section "Foliq" SecXournalpp
	; Required
	SectionIn RO

	SetOutPath "$INSTDIR"

	; Files to put into the setup
	File /r ${SETUP_DIR}\*

	; Set install information
	WriteRegStr SHCTX "Software\Foliq" "" '"$INSTDIR"'

	; Set program information

	; Add file type information
	!insertmacro RegisterExt ".xopp" "Foliq.File"
	!insertmacro RegisterExt ".xopt" "Foliq.Template"
	!insertmacro RegisterExt ".xoj" "Foliq.Xournal"
	!insertmacro RegisterExt ".pdf" "Foliq.AnnotatePdf"
	push $R0
	StrCpy $R0 "$INSTDIR\bin\xournalpp.exe"
	!insertmacro AddProgId "Foliq.File" "$R0" "Foliq file"
	!insertmacro AddProgId "Foliq.Template" "$R0" "Foliq template file"
	!insertmacro AddProgId "Foliq.Xournal" "$R0" "Xournal file"
	!insertmacro AddProgId "Foliq.AnnotatePdf" "$R0" "PDF file"
	pop $R0

	; Create uninstaller
	WriteUninstaller "$INSTDIR\Uninstall.exe"
	; Add uninstall entry. See https://docs.microsoft.com/en-us/windows/win32/msi/uninstall-registry-key
	WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "DisplayIcon" '"$INSTDIR\bin\xournalpp.exe"'
	WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "DisplayName" "Foliq"
	WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "DisplayVersion" "${XOURNALPP_VERSION}"
	WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "Publisher" "Foliq (based on Foliq by The Foliq Team)"
	WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "URLInfoAbout" "https://github.com/leotrax3d/foliq"
	WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "InstallLocation" '"$INSTDIR"'
	WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "UninstallString" '"$INSTDIR\Uninstall.exe"'
	WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "NoModify" 1
	WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq" "NoRepair" 1

	!insertmacro MUI_STARTMENU_WRITE_BEGIN Application
		;Create shortcuts
		CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
		CreateShortcut "$SMPROGRAMS\$StartMenuFolder\Foliq.lnk" '"$INSTDIR\bin\xournalpp.exe"'
		CreateShortcut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" '"$INSTDIR\Uninstall.exe"'
		
		!insertmacro RefreshShellIconCreate "$SMPROGRAMS\$StartMenuFolder\Foliq.lnk"
	!insertmacro MUI_STARTMENU_WRITE_END

	!insertmacro RefreshShellIcons
SectionEnd

;--------------------------------
; Descriptions

; Language strings
LangString DESC_SecXournalpp ${LANG_ENGLISH} "Foliq executable"
LangString DESC_SecFileXopp ${LANG_ENGLISH} "Open .xopp files with Foliq"
LangString DESC_SecFileXopt ${LANG_ENGLISH} "Open .xopt files with Foliq"
LangString DESC_SecFileXoj ${LANG_ENGLISH} "Open .xoj files with Foliq"

; Assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${SecXournalpp} $(DESC_SecXournalpp)
	!insertmacro MUI_DESCRIPTION_TEXT ${SecFileXopp} $(DESC_SecFileXopp)
	!insertmacro MUI_DESCRIPTION_TEXT ${SecFileXopt} $(DESC_SecFileXopt)
	!insertmacro MUI_DESCRIPTION_TEXT ${SecFileXoj} $(DESC_SecFileXoj)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
; Uninstaller

Section "Uninstall"

	SetRegView 64

	; FIXME: ask if the user wants to uninstall the user or system wide install
	ReadRegStr $0 HKCU "Software\Foliq" ""
	${IF} $0 == ""
		SetShellVarContext all
	${ELSE}
		SetShellVarContext current
	${ENDIF}

	; Remove registry keys
	DeleteRegKey SHCTX "Software\Foliq"
	DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Foliq"

	!insertmacro DeleteProgId "Foliq.File"
	!insertmacro DeleteProgId "Foliq.Template"
	!insertmacro DeleteProgId "Foliq.Xournal"
	!insertmacro DeleteProgId "Foliq.AnnotatePdf"

	; Clean up start menu
	!insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
	Delete "$SMPROGRAMS\$StartMenuFolder\Foliq.lnk"
	Delete "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk"
	RMDir "$SMPROGRAMS\$StartMenuFolder"

	; Remove files
	RMDir /r "$INSTDIR\bin"
	RMDir /r "$INSTDIR\lib"
	RMDir /r "$INSTDIR\share"
	RMDir /r "$INSTDIR\ui"
	Delete "$INSTDIR\Uninstall.exe"
	RMDir "$INSTDIR"

	!insertmacro RefreshShellIconDelete "$SMPROGRAMS\$StartMenuFolder\Foliq.lnk"
	!insertmacro RefreshShellIcons
SectionEnd
