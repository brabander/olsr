/*
 * Windows GUI for olsr.org
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

#if !defined(AFX_MYDIALOG3_H__1A381668_A36B_4C51_9B79_643BC2A59D88__INCLUDED_)
#define AFX_MYDIALOG3_H__1A381668_A36B_4C51_9B79_643BC2A59D88__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

#include "NodeEntry.h"

class NodeInfo
{
public:
	CStringArray MprList;
	CStringArray MidList;
	CStringArray HnaList;
};

class MyDialog3 : public CDialog
{
public:
	MyDialog3(CWnd* pParent = NULL);

	BOOL Create(CWnd *Parent);

	void UpdateNodeInfo(CList<class NodeEntry, class NodeEntry &> &);
	void ClearNodeInfo(void);

	//{{AFX_DATA(MyDialog3)
	enum { IDD = IDD_DIALOG3 };
	CListCtrl	m_HnaList;
	CListCtrl	m_MidList;
	CListCtrl	m_MprList;
	CListCtrl	m_NodeList;
	//}}AFX_DATA


	//{{AFX_VIRTUAL(MyDialog3)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

protected:
	unsigned __int64 LastUpdate;
	class NodeInfo *Info;

	//{{AFX_MSG(MyDialog3)
	afx_msg void OnOK();
	afx_msg void OnCancel();
	virtual BOOL OnInitDialog();
	afx_msg void OnItemchangedNodeList(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}

#endif
