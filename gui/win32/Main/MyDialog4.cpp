/*
 * $Id: MyDialog4.cpp,v 1.2 2004/09/15 11:18:41 tlopatic Exp $
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
#include "MyDialog4.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

MyDialog4::MyDialog4(CWnd* pParent)
	: CDialog(MyDialog4::IDD, pParent)
{
	//{{AFX_DATA_INIT(MyDialog4)
	//}}AFX_DATA_INIT
}

BOOL MyDialog4::Create(CWnd *Parent)
{
	return CDialog::Create(MyDialog4::IDD, Parent);
}

void MyDialog4::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(MyDialog4)
	DDX_Control(pDX, IDC_LIST1, m_RoutingTable);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(MyDialog4, CDialog)
	//{{AFX_MSG_MAP(MyDialog4)
	ON_BN_CLICKED(IDOK, OnOK)
	ON_BN_CLICKED(IDCANCEL, OnCancel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void MyDialog4::OnOK()
{
}

void MyDialog4::OnCancel()
{
}

BOOL MyDialog4::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_RoutingTable.InsertColumn(0, "Destination", LVCFMT_LEFT, 110, 0);
	m_RoutingTable.InsertColumn(1, "Gateway", LVCFMT_LEFT, 110, 1);
	m_RoutingTable.InsertColumn(2, "Metric", LVCFMT_LEFT, 68, 2);
	m_RoutingTable.InsertColumn(3, "Interface", LVCFMT_LEFT, 67, 3);

	return TRUE;
}

void MyDialog4::AddRoute(unsigned int Dest, unsigned int Gate, int Metric,
						 char *Int)
{
	CString DestStr;
	CString GateStr;
	CString MetricStr;
	CString IntStr;
	int Idx;

	DestStr.Format("%d.%d.%d.%d",
		((unsigned char *)&Dest)[0], ((unsigned char *)&Dest)[1],
		((unsigned char *)&Dest)[2], ((unsigned char *)&Dest)[3]);

	GateStr.Format("%d.%d.%d.%d",
		((unsigned char *)&Gate)[0], ((unsigned char *)&Gate)[1],
		((unsigned char *)&Gate)[2], ((unsigned char *)&Gate)[3]);

	MetricStr.Format("%d", Metric);

	IntStr.Format("%c%c%c%c", Int[0], Int[1], Int[2], Int[3]);
	IntStr.MakeUpper();

	Idx = m_RoutingTable.GetItemCount();

	m_RoutingTable.InsertItem(Idx, DestStr);

	m_RoutingTable.SetItemText(Idx, 1, GateStr);
	m_RoutingTable.SetItemText(Idx, 2, MetricStr);
	m_RoutingTable.SetItemText(Idx, 3, IntStr);
}

void MyDialog4::DeleteRoute(unsigned int Dest)
{
	CString DestStr;
	int Idx, Num;

	DestStr.Format("%d.%d.%d.%d",
		((unsigned char *)&Dest)[0], ((unsigned char *)&Dest)[1],
		((unsigned char *)&Dest)[2], ((unsigned char *)&Dest)[3]);

	Num = m_RoutingTable.GetItemCount();

	for (Idx = 0; Idx < Num; Idx++)
	{
		if (m_RoutingTable.GetItemText(Idx, 0) == DestStr)
		{
			m_RoutingTable.DeleteItem(Idx);
			break;
		}
	}
}

void MyDialog4::ClearRoutes(void)
{
	m_RoutingTable.DeleteAllItems();
}
