/*
 * $Id: MyDialog2.h,v 1.4 2004/11/20 22:52:49 tlopatic Exp $
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

#if !defined(AFX_MYDIALOG2_H__1A381668_A36B_4C51_9B79_643BC2A59D88__INCLUDED_)
#define AFX_MYDIALOG2_H__1A381668_A36B_4C51_9B79_643BC2A59D88__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

#include "MyEdit.h"

#define MAKELIB
#include <olsr_cfg.h>

class MyDialog2 : public CDialog
{
public:
	MyDialog2(CWnd* pParent = NULL);

	BOOL Create(CWnd *Parent);

	int OpenConfigFile(CString);
	int SaveConfigFile(CString, int);

	CStringArray *Interfaces;
	CStringArray *Addresses;
	CStringArray *IsWlan;

	struct olsrd_config *Conf;

	//{{AFX_DATA(MyDialog2)
	enum { IDD = IDD_DIALOG2 };
	CButton	m_EtxRadio2;
	CButton	m_EtxRadio1;
	CEdit	m_EtxWindowSize;
	CButton	m_EtxCheck;
	CButton	m_Ipv6Check;
	CButton	m_InternetCheck;
	CButton	m_HystCheck;
	MyEdit	m_HystThresholdHigh;
	MyEdit	m_HystThresholdLow;
	MyEdit	m_HystScaling;
	MyEdit	m_HnaHold;
	MyEdit	m_MidHold;
	MyEdit	m_PollInt;
	MyEdit	m_TcHold;
	MyEdit	m_TcInt;
	MyEdit	m_HnaInt;
	MyEdit	m_MidInt;
	MyEdit	m_HelloHold;
	MyEdit	m_HelloInt;
	CListCtrl	m_InterfaceList;
	CStatic	m_DebugLevelText;
	CSliderCtrl	m_DebugLevel;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(MyDialog2)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

protected:

	//{{AFX_MSG(MyDialog2)
	afx_msg void OnOK();
	afx_msg void OnCancel();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	virtual BOOL OnInitDialog();
	afx_msg void OnHystCheck();
	afx_msg void OnOpenButton();
	afx_msg void OnSaveButton();
	afx_msg void OnResetButton();
	afx_msg void OnEtxCheck();
	afx_msg void OnEtxRadio1();
	afx_msg void OnEtxRadio2();
	afx_msg void OnKillfocusEtxWinSize();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CFont EditFont;

	int DebugLevel;
	void SetDebugLevel(int);

	void Reset(void);
};

//{{AFX_INSERT_LOCATION}}

#endif
