/*
 * $Id: MyEdit.cpp,v 1.2 2004/09/15 11:18:41 tlopatic Exp $
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
#include "frontend.h"
#include "MyEdit.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

MyEdit::MyEdit()
{
}

MyEdit::~MyEdit()
{
}


BEGIN_MESSAGE_MAP(MyEdit, CEdit)
	//{{AFX_MSG_MAP(MyEdit)
	ON_WM_CHAR()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void MyEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	static unsigned char *Allowed = (unsigned char *)"0123456789.";
	int i;
	CString Text;

	if (nChar >= 32)
	{
		for (i = 0; Allowed[i] != 0; i++)
			if (nChar == Allowed[i])
				break;

		if (Allowed[i] == 0)
			return;

		GetWindowText(Text);

		if (nChar == '.' && Text.Find('.') >= 0)
			return;

		if (Text.GetLength() > 2 && Text.Find('.') < 0 && nChar != '.')
			return;
	}
	
	CEdit::OnChar(nChar, nRepCnt, nFlags);
}

void MyEdit::OnKillFocus(CWnd* pNewWnd) 
{
	CString Text;
	int Index;
	int Len;

	GetWindowText(Text);

	Index = Text.Find('.');

	Len = Text.GetLength();

	if (Len == 0)
		SetWindowText("0.0");

	else if (Index < 0)
		SetWindowText(Text + ".0");

	else if (Index == Len - 1)
		SetWindowText(Text + "0");

	CEdit::OnKillFocus(pNewWnd);
}
