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
#include "MyDialog1.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

MyDialog1::MyDialog1(CWnd* pParent)
	: CDialog(MyDialog1::IDD, pParent)
{
	//{{AFX_DATA_INIT(MyDialog1)
	//}}AFX_DATA_INIT
}

BOOL MyDialog1::Create(CWnd *Parent)
{
	return CDialog::Create(MyDialog1::IDD, Parent);
}

void MyDialog1::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(MyDialog1)
	DDX_Control(pDX, IDC_BUTTON4, m_SaveButton);
	DDX_Control(pDX, IDC_BUTTON3, m_FreezeButton);
	DDX_Control(pDX, IDC_BUTTON2, m_ContinueButton);
	DDX_Control(pDX, IDC_BUTTON1, m_ClearButton);
	DDX_Control(pDX, IDC_EDIT1, m_OutputWindow);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(MyDialog1, CDialog)
	//{{AFX_MSG_MAP(MyDialog1)
	ON_BN_CLICKED(IDC_BUTTON1, OnClearButton)
	ON_BN_CLICKED(IDC_BUTTON2, OnContinueButton)
	ON_BN_CLICKED(IDC_BUTTON3, OnFreezeButton)
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED(IDOK, OnOK)
	ON_BN_CLICKED(IDCANCEL, OnCancel)
	ON_BN_CLICKED(IDC_BUTTON4, OnSaveButton)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void MyDialog1::OnOK()
{
}

void MyDialog1::OnCancel()
{
}

void MyDialog1::AddOutputLine(CString Line)
{
	CritSect.Lock();
	Output += Line + "\r\n";
	CritSect.Unlock();

	if (Frozen == 0)
	{
		m_OutputWindow.SetWindowText(Output);
		m_OutputWindow.SetSel(Output.GetLength(), Output.GetLength());
	}
}

BOOL MyDialog1::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	CClientDC DevCont(&m_OutputWindow);

	EditFont.CreatePointFont(100, "Courier", NULL);
	m_OutputWindow.SetFont(&EditFont);

	m_FreezeButton.EnableWindow(FALSE);
	m_ContinueButton.EnableWindow(FALSE);

	Frozen = 0;

	WhiteBrush.CreateSolidBrush(RGB(255, 255, 255));

	return TRUE;
}

void MyDialog1::OnClearButton() 
{
	CritSect.Lock();
	Output.Empty();
	CritSect.Unlock();

	m_OutputWindow.SetWindowText(Output);
	m_OutputWindow.SetSel(0, 0);
}

void MyDialog1::OnContinueButton()
{
	CString Copy;
	int Len;

	m_FreezeButton.EnableWindow(TRUE);
	m_ContinueButton.EnableWindow(FALSE);

	Frozen = 0;

	CritSect.Lock();
	Copy = Output;
	Len = Output.GetLength();
	CritSect.Unlock();

	m_OutputWindow.SetWindowText(Copy);
	m_OutputWindow.SetSel(Len, Len);
}

void MyDialog1::OnFreezeButton() 
{
	m_FreezeButton.EnableWindow(FALSE);
	m_ContinueButton.EnableWindow(TRUE);

	Frozen = 1;
}

void MyDialog1::HandleStart(void)
{
	CString Copy;
	int Len;

	m_FreezeButton.EnableWindow(TRUE);
	m_ContinueButton.EnableWindow(FALSE);

	Frozen = 0;

	CritSect.Lock();
	Copy = Output;
	Len = Output.GetLength();
	CritSect.Unlock();

	m_OutputWindow.SetWindowText(Copy);
	m_OutputWindow.SetSel(Len, Len);
}

void MyDialog1::HandleStop(void)
{
	CString Copy;
	int Len;

	m_FreezeButton.EnableWindow(FALSE);
	m_ContinueButton.EnableWindow(FALSE);

	Frozen = 0;

	CritSect.Lock();
	Copy = Output;
	Len = Output.GetLength();
	CritSect.Unlock();

	m_OutputWindow.SetWindowText(Copy);
	m_OutputWindow.SetSel(Len, Len);
}

HBRUSH MyDialog1::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	if (pWnd == &m_OutputWindow)
	{
		pDC->SetBkColor(RGB(255, 255, 255));
		hbr = WhiteBrush;
	}
	
	return hbr;
}

void MyDialog1::OnSaveButton()
{
	CString Copy;
	int Len;
	CFileDialog FileDialog(FALSE);
	CString FileName = "OLSR log.txt";
	CString PathName;
	CFile File;

	CritSect.Lock();
	Copy = Output;
	Len = Output.GetLength();
	CritSect.Unlock();

	FileDialog.m_ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0";
	FileDialog.m_ofn.nFilterIndex = 1;

	FileDialog.m_ofn.lpstrFile = FileName.GetBuffer(500);
	FileDialog.m_ofn.nMaxFile = 500;

	if (FileDialog.DoModal() == IDOK)
	{
		PathName = FileDialog.GetPathName();

		if (File.Open(PathName, CFile::modeCreate | CFile::modeWrite |
			CFile::shareExclusive) == 0)
			AfxMessageBox("Cannot open logfile '" + PathName + "'.");

		else
		{
			File.Write((const char *)Copy, Len);
			File.Close();
		}
	}

	FileName.ReleaseBuffer();
}
