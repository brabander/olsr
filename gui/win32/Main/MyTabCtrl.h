/*
 * $Id: MyTabCtrl.h,v 1.3 2004/09/15 11:18:41 tlopatic Exp $
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

#if !defined(AFX_MYTABCTRL_H__D443FF52_C52D_4C89_AB4B_19B09687EBAE__INCLUDED_)
#define AFX_MYTABCTRL_H__D443FF52_C52D_4C89_AB4B_19B09687EBAE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

#include "MyDialog1.h"
#include "MyDialog2.h"
#include "MyDialog3.h"
#include "MyDialog4.h"

class MyTabCtrl : public CTabCtrl
{
public:
	MyTabCtrl();

	class MyDialog1 m_Dialog1;
	class MyDialog2 m_Dialog2;
	class MyDialog3 m_Dialog3;
	class MyDialog4 m_Dialog4;

	void InitTabDialogs(CStringArray *, CStringArray *, CStringArray *);
	void DisplayTabDialog(void);

	//{{AFX_VIRTUAL(MyTabCtrl)
	//}}AFX_VIRTUAL

	virtual ~MyTabCtrl();

protected:
	CDialog *Dialogs[4];
	int Sel;

	//{{AFX_MSG(MyTabCtrl)
	afx_msg void OnSelchange(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}

#endif
