;
;  The olsr.org Optimized Link-State Routing daemon (olsrd)
;  Copyright (c) 2004, Thomas Lopatic (thomas@lopatic.de)
;  All rights reserved.
;
;  Redistribution and use in source and binary forms, with or without 
;  modification, are permitted provided that the following conditions 
;  are met:
;
;  * Redistributions of source code must retain the above copyright 
;    notice, this list of conditions and the following disclaimer.
;  * Redistributions in binary form must reproduce the above copyright 
;    notice, this list of conditions and the following disclaimer in 
;    the documentation and/or other materials provided with the 
;    distribution.
;  * Neither the name of olsr.org, olsrd nor the names of its 
;    contributors may be used to endorse or promote products derived 
;    from this software without specific prior written permission.
;
;  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
;  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
;  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
;  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
;  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
;  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
;  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
;  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
;  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
;  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
;  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
;  POSSIBILITY OF SUCH DAMAGE.
;
;  Visit http://www.olsr.org for more information.
;
;  If you find this software useful feel free to make a donation
;  to the project. For more information see the website or contact
;  the copyright holders.
;
;  $Id: installer.nsi,v 1.6 2004/11/21 01:21:10 tlopatic Exp $
;

Name olsr.org
OutFile ..\..\..\olsr-setup.exe
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
        File ..\..\..\lib\dot_draw\olsrd_dot_draw.dll

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
        Delete $INSTDIR\olsrd_dot_draw.dll
        Delete $INSTDIR\uninstall.exe

        RMDir $INSTDIR

        Delete "$SMPROGRAMS\olsr.org\OLSR Switch.lnk"
        Delete $SMPROGRAMS\olsr.org\README-WIN32.lnk
        Delete $SMPROGRAMS\olsr.org\Uninstall.lnk

        RMDir $SMPROGRAMS\olsr.org

        Delete "$DESKTOP\OLSR Switch.lnk"

SectionEnd
