/*
 * $Id: MyDialog4.h,v 1.2 2004/09/15 11:18:41 tlopatic Exp $
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

#if !defined(AFX_MYDIALOG4_H__1A381668_A36B_4C51_9B79_643BC2A59D88__INCLUDED_)
#define AFX_MYDIALOG4_H__1A381668_A36B_4C51_9B79_643BC2A59D88__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

class MyDialog4 : public CDialog
{
public:
	MyDialog4(CWnd* pParent = NULL);

	BOOL Create(CWnd *Parent);

	void AddRoute(unsigned int, unsigned int, int, char *);
	void DeleteRoute(unsigned int);
	void ClearRoutes(void);

	//{{AFX_DATA(MyDialog4)
	enum { IDD = IDD_DIALOG4 };
	CListCtrl	m_RoutingTable;
	//}}AFX_DATA


	//{{AFX_VIRTUAL(MyDialog4)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

protected:

	//{{AFX_MSG(MyDialog4)
	afx_msg void OnOK();
	afx_msg void OnCancel();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}

#endif
