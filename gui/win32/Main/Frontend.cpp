/*
 * $Id: Frontend.cpp,v 1.3 2004/11/21 00:26:48 tlopatic Exp $
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * This file is part of olsr.org.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "stdafx.h"
#include "Frontend.h"
#include "FrontendDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CFrontendApp, CWinApp)
	//{{AFX_MSG_MAP(CFrontendApp)
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

CFrontendApp::CFrontendApp()
{
}

CFrontendApp theApp;

static int SetEnableRedirKey(unsigned long New)
{
	HKEY Key;
	unsigned long Type;
	unsigned long Len;
	unsigned long Old;
	
	if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
		0, KEY_READ | KEY_WRITE, &Key) != ERROR_SUCCESS)
		return -1;
	
	Len = sizeof (Old);
	
	if (::RegQueryValueEx(Key, "EnableICMPRedirect", NULL, &Type,
		(unsigned char *)&Old, &Len) != ERROR_SUCCESS ||
		Type != REG_DWORD)
		Old = 1;
	
	if (::RegSetValueEx(Key, "EnableICMPRedirect", 0, REG_DWORD,
		(unsigned char *)&New, sizeof (New)))
	{
		::RegCloseKey(Key);
		return -1;
	}
	
	::RegCloseKey(Key);
	return Old;
}

BOOL CFrontendApp::InitInstance()
{
	int Res;

#ifdef _AFXDLL
	Enable3dControls();
#else
	Enable3dControlsStatic();
#endif

	CCommandLineInfo CmdLineInfo;
	ParseCommandLine(CmdLineInfo);

	CFrontendDlg dlg;

	dlg.ConfigFile = CmdLineInfo.m_strFileName;

	m_pMainWnd = &dlg;

	Res = SetEnableRedirKey(0);

	if (Res == 1)
	{
		Res = AfxMessageBox("- WARNING -\n\n"
			"The OLSR software has just switched off the processing of incoming ICMP "
			" redirect messages in your registry.\n\n"
			" Please REBOOT your computer for this change to take effect.\n\n"
			" Do you want to allow the OLSR software to reboot your computer now?\n\n"
			" (Please say \"Yes\".)", MB_YESNO | MB_ICONEXCLAMATION);

		if (Res == IDYES)
		{
			HANDLE Proc;
			HMODULE Lib;
			BOOL (*Open)(HANDLE, DWORD, HANDLE *);
			BOOL (*Lookup)(char *, char *, LUID *);
			BOOL (*Adjust)(HANDLE, BOOL, TOKEN_PRIVILEGES *, DWORD,
				TOKEN_PRIVILEGES *, DWORD *);
			HANDLE Token;

			Lib = ::LoadLibrary("advapi32.dll");

			if (Lib != NULL)
			{
				Open = (BOOL (*)(HANDLE, DWORD, HANDLE *))
					::GetProcAddress(Lib, "OpenProcessToken");

				Lookup = (BOOL (*)(char *, char *, LUID *))
					::GetProcAddress(Lib, "LookupPrivilegeValueA");

				Adjust = (BOOL (*)(HANDLE, BOOL, TOKEN_PRIVILEGES *, DWORD,
					TOKEN_PRIVILEGES *, DWORD *))
					::GetProcAddress(Lib, "AdjustTokenPrivileges");

				if (Open != NULL && Lookup != NULL && Adjust != NULL)
				{
					struct
					{
						DWORD Count;
						LUID_AND_ATTRIBUTES Priv;
					}
					TokPriv;

					Proc = ::GetCurrentProcess();

					if (!Open(Proc, TOKEN_ALL_ACCESS, &Token))
						AfxMessageBox("OpenProcessToken() failed.");

					else if (!Lookup("", "SeShutdownPrivilege", &TokPriv.Priv.Luid))
						AfxMessageBox("LookupPrivilegeValue() failed.");

					else
					{
						TokPriv.Count = 1;
						TokPriv.Priv.Attributes = SE_PRIVILEGE_ENABLED;

						if (!Adjust(Token, FALSE, (TOKEN_PRIVILEGES *)&TokPriv,
							0, NULL, NULL))
							AfxMessageBox("AdjustTokenPrivilege() failed.");
					}
				}

				::FreeLibrary(Lib);
			}

			::ExitWindowsEx(EWX_REBOOT, 0);
			::TerminateProcess(Proc, 0);
		}
	}

	dlg.DoModal();

	return FALSE;
}
