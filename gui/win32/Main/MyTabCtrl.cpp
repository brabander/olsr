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

#include "stdafx.h"
#include "Frontend.h"
#include "MyTabCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

MyTabCtrl::MyTabCtrl()
{
}

MyTabCtrl::~MyTabCtrl()
{
}

BEGIN_MESSAGE_MAP(MyTabCtrl, CTabCtrl)
	//{{AFX_MSG_MAP(MyTabCtrl)
	ON_NOTIFY_REFLECT(TCN_SELCHANGE, OnSelchange)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void MyTabCtrl::InitTabDialogs(CStringArray *Interfaces,
							   CStringArray *Addresses,
							   CStringArray *IsWlan)
{
	int i;
	CRect Client;
	CRect Win;

	m_Dialog2.Interfaces = Interfaces;
	m_Dialog2.Addresses = Addresses;
	m_Dialog2.IsWlan = IsWlan;

	m_Dialog1.Create(GetParent());
	m_Dialog2.Create(GetParent());
	m_Dialog3.Create(GetParent());
	m_Dialog4.Create(GetParent());

	Dialogs[0] = &m_Dialog2;
	Dialogs[1] = &m_Dialog1;
	Dialogs[2] = &m_Dialog3;
	Dialogs[3] = &m_Dialog4;

	Sel = -1;

	for (i = 0; i < 4; i++)
	{
		GetClientRect(Client);
		AdjustRect(FALSE, Client);

		GetWindowRect(Win);
		GetParent()->ScreenToClient(Win);

		Client.OffsetRect(Win.left, Win.top);

		Dialogs[i]->SetWindowPos(&wndTop, Client.left, Client.top,
			Client.Width(), Client.Height(), SWP_HIDEWINDOW);
	}

	DisplayTabDialog();
}

void MyTabCtrl::DisplayTabDialog()
{
	if (Sel != -1)
		Dialogs[Sel]->ShowWindow(SW_HIDE);

	Sel = GetCurSel();

	Dialogs[Sel]->ShowWindow(SW_SHOW);
}

void MyTabCtrl::OnSelchange(NMHDR* pNMHDR, LRESULT* pResult) 
{
	pNMHDR = pNMHDR;

	DisplayTabDialog();

	*pResult = 0;
}
