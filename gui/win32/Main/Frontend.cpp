/*
 * $Id: Frontend.cpp,v 1.2 2004/09/15 11:18:41 tlopatic Exp $
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

BOOL CFrontendApp::InitInstance()
{
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

	dlg.DoModal();

	return FALSE;
}
