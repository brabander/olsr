/*
 * $Id: FrontendDlg.h,v 1.4 2004/11/20 23:17:47 tlopatic Exp $
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

#if !defined(AFX_FRONTENDDLG_H__7D68FBC0_7448_479B_81F0_3FBBDE291395__INCLUDED_)
#define AFX_FRONTENDDLG_H__7D68FBC0_7448_479B_81F0_3FBBDE291395__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

#include "MyTabCtrl.h"
#include "NodeEntry.h"

#define PIPE_MODE_RUN 0
#define PIPE_MODE_INT 1

class CFrontendDlg : public CDialog
{
public:
	CFrontendDlg(CWnd* pParent = NULL);

	//{{AFX_DATA(CFrontendDlg)
	enum { IDD = IDD_FRONTEND_DIALOG };
	CButton m_StopButton;
	CButton m_StartButton;
	MyTabCtrl m_TabCtrl;
	//}}AFX_DATA

	unsigned int LogThreadFunc(void);
	unsigned int NetThreadFunc(void);

	CString ConfigFile;

	//{{AFX_VIRTUAL(CFrontendDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CFrontendDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnOK();
	afx_msg void OnCancel();
	afx_msg void OnStartButton();
	afx_msg void OnStopButton();
	afx_msg void OnExitButton();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	int GetInterfaces(void);

	HANDLE Event;

	CString StoredTempFile;

	SOCKET SockHand;

	int StartOlsrd(void);
	int StopOlsrd(void);

	int PipeMode;
	int ExecutePipe(const char *, HANDLE *, HANDLE *, HANDLE *);

	CWinThread *LogThread;
	CWinThread *NetThread;

	CStringArray Interfaces;
	CStringArray Addresses;
	CStringArray IsWlan;

	HANDLE OutRead, InWrite;
	HANDLE ShimProc;

	void HandleIpcRoute(struct IpcRoute *);
	void HandleIpcConfig(struct IpcConfig *);
	void HandleOlsrHello(struct OlsrHello *, int);
	void HandleOlsrTc(struct OlsrTc *, int);
	void HandleOlsrMid(struct OlsrHeader *);
	void HandleOlsrHna(struct OlsrHeader *);

	void AddNode(unsigned int, unsigned int);
	void AddMpr(unsigned int, unsigned int, unsigned int);
	void AddMid(unsigned int, unsigned int, unsigned int);
	void AddHna(unsigned int, unsigned int, unsigned int, unsigned int);

	CList<class NodeEntry, class NodeEntry &> NodeList;

	void Timeout(void);

	unsigned __int64 Now;

	unsigned int LocalHost;
};

//{{AFX_INSERT_LOCATION}}

#endif

