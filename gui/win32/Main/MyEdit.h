/*
 * $Id: MyEdit.h,v 1.2 2004/09/15 11:18:41 tlopatic Exp $
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

#if !defined(AFX_MYEDIT_H__951EC391_AFE3_428F_865D_24CA55C68C7C__INCLUDED_)
#define AFX_MYEDIT_H__951EC391_AFE3_428F_865D_24CA55C68C7C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

class MyEdit : public CEdit
{
public:

	MyEdit();

	//{{AFX_VIRTUAL(MyEdit)
	//}}AFX_VIRTUAL

	virtual ~MyEdit();

protected:
	//{{AFX_MSG(MyEdit)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}

#endif
