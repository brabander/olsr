;
; $Id: installer.nsi,v 1.2 2004/09/15 13:38:30 tlopatic Exp $
; Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
;
; This file is part of olsr.org.
;
; olsr.org is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; olsr.org is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with olsr.org; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;

Name olsr.org
OutFile olsr-setup.exe
BrandingText "www.olsr.org"
InstallDir $PROGRAMFILES\olsr.org
LicenseData ..\..\..\gpl.txt
XPStyle on

Page license
Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Function .onInit
        MessageBox MB_YESNO "This will install olsr.org 0.4.7 on your computer. Continue?" IDYES NoAbort
        Abort
NoAbort:
FunctionEnd

Section "Program Files"

        SectionIn RO

        SetOutPath $INSTDIR

        File ..\Main\Release\Switch.exe
        File ..\Shim\Release\Shim.exe
        File ..\..\..\olsrd.exe
        File ..\..\..\README-WIN32.txt
        File linux-manual.txt
        File /oname=Default.olsr ..\..\..\files\olsrd.conf.default.win32

        WriteRegStr HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\olsr.org DisplayName olsr.org
        WriteRegStr HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\olsr.org UninstallString $INSTDIR\uninstall.exe

        WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\olsr.org NoModify 1
        WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\olsr.org NoRepair 1

        WriteUninstaller $INSTDIR\uninstall.exe

SectionEnd

Section "Start Menu Shortcuts"

        CreateDirectory $SMPROGRAMS\olsr.org

        CreateShortCut "$SMPROGRAMS\olsr.org\OLSR Switch.lnk" $INSTDIR\Switch.exe "" $INSTDIR\Switch.exe 0
        CreateShortCut $SMPROGRAMS\olsr.org\README-WIN32.lnk $INSTDIR\README-WIN32.txt
        CreateShortCut $SMPROGRAMS\olsr.org\Uninstall.lnk $INSTDIR\uninstall.exe "" $INSTDIR\uninstall.exe 0

SectionEnd

Section "Desktop Shortcut"

        CreateShortCut "$DESKTOP\OLSR Switch.lnk" $INSTDIR\Switch.exe "" $INSTDIR\Switch.exe 0

SectionEnd

Section "File Association (*.olsr)"

        WriteRegStr HKCR .olsr "" OlsrOrgConfigFile

        WriteRegStr HKCR OlsrOrgConfigFile "" "olsr.org Configuration File"

        WriteRegStr HKCR OlsrOrgConfigFile\shell "" open
        WriteRegStr HKCR OlsrOrgConfigFile\DefaultIcon "" $INSTDIR\Switch.exe,0
        WriteRegStr HKCR OlsrOrgConfigFile\shell\open\command "" '$INSTDIR\Switch.exe "%1"'

SectionEnd

Section "Uninstall"

        DeleteRegKey HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\olsr.org

        DeleteRegKey HKCR .olsr
        DeleteRegKey HKCR OlsrOrgConfigFile

        Delete $INSTDIR\Switch.exe
        Delete $INSTDIR\Shim.exe
        Delete $INSTDIR\olsrd.exe
        Delete $INSTDIR\README-WIN32.txt
        Delete $INSTDIR\linux-manual.txt
        Delete $INSTDIR\Default.olsr
        Delete $INSTDIR\uninstall.exe

        RMDir $INSTDIR

        Delete "$SMPROGRAMS\olsr.org\OLSR Switch.lnk"
        Delete $SMPROGRAMS\olsr.org\README-WIN32.lnk
        Delete $SMPROGRAMS\olsr.org\Uninstall.lnk

        RMDir $SMPROGRAMS\olsr.org

        Delete "$DESKTOP\OLSR Switch.lnk"

SectionEnd
