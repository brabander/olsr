/*
 * $Id: MyDialog2.cpp,v 1.3 2004/09/15 11:18:41 tlopatic Exp $
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
#include "MyDialog2.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MAXIF 100

MyDialog2::MyDialog2(CWnd* pParent)
	: CDialog(MyDialog2::IDD, pParent)
{
	//{{AFX_DATA_INIT(MyDialog2)
	//}}AFX_DATA_INIT
}

void MyDialog2::SetDebugLevel(int Level)
{
	char LevelText[2];

	LevelText[0] = (char)(Level + '0');
	LevelText[1] = 0;

	DebugLevel = Level;
	m_DebugLevel.SetPos(Level);
	m_DebugLevelText.SetWindowText(LevelText);
}

void MyDialog2::GetInterfaceList(CString &Res)
{
	int Num = m_InterfaceList.GetItemCount();
	int i;
	int AddSpace = 0;

	for (i = 0; i < Num; i++)
		if (m_InterfaceList.GetCheck(i))
		{
			if (AddSpace != 0)
				Res += " ";

			else
				AddSpace = 1;

			Res += m_InterfaceList.GetItemText(i, 0).Mid(0, 4);
		}

	Res.MakeLower();
}

BOOL MyDialog2::Create(CWnd *Parent)
{
	return CDialog::Create(MyDialog2::IDD, Parent);
}

void MyDialog2::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(MyDialog2)
	DDX_Control(pDX, IDC_CHECK4, m_TunnelCheck);
	DDX_Control(pDX, IDC_CHECK3, m_Ipv6Check);
	DDX_Control(pDX, IDC_CHECK2, m_InternetCheck);
	DDX_Control(pDX, IDC_CHECK1, m_HystCheck);
	DDX_Control(pDX, IDC_EDIT13, m_HystThresholdHigh);
	DDX_Control(pDX, IDC_EDIT12, m_HystThresholdLow);
	DDX_Control(pDX, IDC_EDIT11, m_HystScaling);
	DDX_Control(pDX, IDC_EDIT10, m_HnaMult);
	DDX_Control(pDX, IDC_EDIT9, m_MidMult);
	DDX_Control(pDX, IDC_EDIT7, m_PollInt);
	DDX_Control(pDX, IDC_EDIT6, m_TcMult);
	DDX_Control(pDX, IDC_EDIT5, m_TcInt);
	DDX_Control(pDX, IDC_EDIT4, m_HnaInt);
	DDX_Control(pDX, IDC_EDIT3, m_MidInt);
	DDX_Control(pDX, IDC_EDIT2, m_HelloMult);
	DDX_Control(pDX, IDC_EDIT1, m_HelloInt);
	DDX_Control(pDX, IDC_EDIT8, m_ManualWindow);
	DDX_Control(pDX, IDC_LIST1, m_InterfaceList);
	DDX_Control(pDX, IDC_TEXT1, m_DebugLevelText);
	DDX_Control(pDX, IDC_SLIDER2, m_DebugLevel);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(MyDialog2, CDialog)
	//{{AFX_MSG_MAP(MyDialog2)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_CHECK1, OnHystCheck)
	ON_BN_CLICKED(IDC_BUTTON4, OnOpenButton)
	ON_BN_CLICKED(IDC_BUTTON5, OnSaveButton)
	ON_BN_CLICKED(IDOK, OnOK)
	ON_BN_CLICKED(IDCANCEL, OnCancel)
	ON_BN_CLICKED(IDC_BUTTON1, OnResetButton)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void MyDialog2::OnOK()
{
}

void MyDialog2::OnCancel()
{
}

void MyDialog2::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	if (pScrollBar == (CScrollBar *)&m_DebugLevel)
		SetDebugLevel(m_DebugLevel.GetPos());
	
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void MyDialog2::Reset(void)
{
	int i;

	SetDebugLevel(1);

	m_HelloInt.SetWindowText("2.0");
	m_HelloMult.SetWindowText("3.0");

	m_TcInt.SetWindowText("5.0");
	m_TcMult.SetWindowText("3.0");

	m_MidInt.SetWindowText("5.0");
	m_MidMult.SetWindowText("3.0");

	m_HnaInt.SetWindowText("10.0");
	m_HnaMult.SetWindowText("3.0");

	m_PollInt.SetWindowText("0.1");

	m_HystThresholdLow.SetWindowText("0.3");
	m_HystThresholdHigh.SetWindowText("0.8");
	m_HystScaling.SetWindowText("0.5");

	m_HystCheck.SetCheck(TRUE);

	OnHystCheck();

	m_InternetCheck.SetCheck(FALSE);
	m_Ipv6Check.SetCheck(FALSE);
	m_TunnelCheck.SetCheck(FALSE);

	for (i = 0; i < Interfaces->GetSize(); i++)
	{
		if ((*IsWlan)[i] == "-")
			m_InterfaceList.SetCheck(i, FALSE);

		else
			m_InterfaceList.SetCheck(i, TRUE);
	}

	m_ManualWindow.SetWindowText("");
}

BOOL MyDialog2::OnInitDialog() 
{
	int i;

	CDialog::OnInitDialog();
	
	CClientDC DevCont(&m_ManualWindow);

	EditFont.CreatePointFont(100, "Courier", NULL);
	m_ManualWindow.SetFont(&EditFont);

	m_DebugLevel.SetRange(0, 9, TRUE);

	m_InterfaceList.SetExtendedStyle(m_InterfaceList.GetExtendedStyle() |
			LVS_EX_CHECKBOXES);

	for (i = 0; i < Interfaces->GetSize(); i++)
	{
		m_InterfaceList.InsertItem(i,
		(*Interfaces)[i] + " - " + (*Addresses)[i]);
	}

	MIB_IPFORWARDROW IpFwdRow;

	if (::GetBestRoute(0, 0, &IpFwdRow) != NO_ERROR)
		m_InternetCheck.EnableWindow(FALSE);

	Reset();

	return TRUE;
}

void MyDialog2::OnHystCheck() 
{
	BOOL EnaDis = m_HystCheck.GetCheck();

	m_HystThresholdLow.EnableWindow(EnaDis);
	m_HystThresholdHigh.EnableWindow(EnaDis);
	m_HystScaling.EnableWindow(EnaDis);
}


void MyDialog2::WriteParameter(CFile *File, CString Name, CString Value)
{
	CString Line;

	Line = Name + " " + Value + "\r\n";

	File->Write(Line.GetBuffer(0), Line.GetLength());

	Line.ReleaseBuffer();
}

int MyDialog2::OpenConfigFile(CString PathName)
{
	CStdioFile File;
	CString Line;
	int Len;
	CString Manual;
	int NumInt = m_InterfaceList.GetItemCount();
	int i;
	CString Int;

	if (File.Open(PathName, CFile::modeRead | CFile::typeText |
		CFile::shareExclusive) == 0)
		return -1;

	m_InternetCheck.SetCheck(FALSE);

	for (i = 0; i < NumInt; i++)
		m_InterfaceList.SetCheck(i, FALSE);

	m_HystScaling.SetWindowText("0.5");
	m_HystThresholdLow.SetWindowText("0.3");
	m_HystThresholdHigh.SetWindowText("0.8");

	while (File.ReadString(Line))
	{
		Len = Line.Find('#');

		if (Len >= 0)
		{
			Line = Line.Mid(0, Len);
			Line.TrimRight();
		}

		Len = Line.GetLength();

		if (Len == 0)
			continue;

		if (Len > 10 && Line.Mid(0, 10) == "IPVERSION " && Line.Mid(10) == "4")
			;

		else if (Len > 6 && Line.Mid(0, 6) == "DEBUG ")
			SetDebugLevel(Line.GetAt(6) - '0');

		else if (Len > 9 && Line.Mid(0, 9) == "POLLRATE ")
			m_PollInt.SetWindowText(Line.Mid(9));
		
		else if (Len > 9 && Line.Mid(0, 9) == "HELLOINT ")
			m_HelloInt.SetWindowText(Line.Mid(9));
		
		else if (Len > 11 && Line.Mid(0, 11) == "HELLOMULTI ")
			m_HelloMult.SetWindowText(Line.Mid(11));
		
		else if (Len > 6 && Line.Mid(0, 6) == "TCINT ")
			m_TcInt.SetWindowText(Line.Mid(6));
		
		else if (Len > 8 && Line.Mid(0, 8) == "TCMULTI ")
			m_TcMult.SetWindowText(Line.Mid(8));
		
		else if (Len > 7 && Line.Mid(0, 7) == "MIDINT ")
			m_MidInt.SetWindowText(Line.Mid(7));
		
		else if (Len > 9 && Line.Mid(0, 9) == "MIDMULTI ")
			m_MidMult.SetWindowText(Line.Mid(9));
		
		else if (Len > 7 && Line.Mid(0, 7) == "HNAINT ")
			m_HnaInt.SetWindowText(Line.Mid(7));
		
		else if (Len > 9 && Line.Mid(0, 9) == "HNAMULTI ")
			m_HnaMult.SetWindowText(Line.Mid(9));

		else if (Len > 15 && Line.Mid(0, 15) == "USE_HYSTERESIS " &&
			Line.Mid(15) == "yes")
			m_HystCheck.SetCheck(TRUE);

		else if (Len > 15 && Line.Mid(0, 15) == "USE_HYSTERESIS " &&
			Line.Mid(15) == "no")
			m_HystCheck.SetCheck(FALSE);

		else if (Len > 13 && Line.Mid(0, 13) == "HYST_SCALING ")
			m_HystScaling.SetWindowText(Line.Mid(13));

		else if (Len > 13 && Line.Mid(0, 13) == "HYST_THR_LOW ")
			m_HystThresholdLow.SetWindowText(Line.Mid(13));
		
		else if (Len > 14 && Line.Mid(0, 14) == "HYST_THR_HIGH ")
			m_HystThresholdHigh.SetWindowText(Line.Mid(14));

		else if (Len > 5 && Line.Mid(0, 5) == "HNA4 " &&
			Line.Mid(5) == "0.0.0.0 0.0.0.0")
		{
			if (m_InternetCheck.IsWindowEnabled())
				m_InternetCheck.SetCheck(TRUE);
		}

		else if (Len > 11 && Line.Mid(0, 11) == "INTERFACES ")
		{
			Line = Line.Mid(11);
			Line.MakeUpper();

			for (;;)
			{
				Int = Line.Mid(0, 4);

				for (i = 0; i < NumInt; i++)
				{
					if (m_InterfaceList.GetItemText(i, 0).Mid(0, 4) == Int)
						m_InterfaceList.SetCheck(i, TRUE);
				}

				if (Line.GetLength() < 5)
					break;

				Line = Line.Mid(5);
			}
		}
		
		else
		{
			Manual += Line + "\r\n";
			m_ManualWindow.SetWindowText(Manual);
		}
	}

	File.Close();

	OnHystCheck();

	return 0;
}

int MyDialog2::SaveConfigFile(CString PathName)
{
	CFile File;
	char *Prolog;
	CString Para;
	CString IntList;

	if (File.Open(PathName, CFile::modeCreate | CFile::modeWrite |
		CFile::shareExclusive) == 0)
		return -1;

	Prolog = "#\r\n# AUTOMATICALLY GENERATED FILE - DO NOT TOUCH!\r\n#\r\n\r\n";

	File.Write(Prolog, strlen(Prolog));

	WriteParameter(&File, "IPVERSION", "4");

	m_DebugLevelText.GetWindowText(Para);
	WriteParameter(&File, "DEBUG", Para);

	GetInterfaceList(IntList);

	if (!IntList.IsEmpty())
		WriteParameter(&File, "INTERFACES", IntList);

	m_PollInt.GetWindowText(Para);
	WriteParameter(&File, "POLLRATE", Para);

	m_HelloInt.GetWindowText(Para);
	WriteParameter(&File, "HELLOINT", Para);

	m_HelloMult.GetWindowText(Para);
	WriteParameter(&File, "HELLOMULTI", Para);

	m_TcInt.GetWindowText(Para);
	WriteParameter(&File, "TCINT", Para);

	m_TcMult.GetWindowText(Para);
	WriteParameter(&File, "TCMULTI", Para);

	m_MidInt.GetWindowText(Para);
	WriteParameter(&File, "MIDINT", Para);

	m_MidMult.GetWindowText(Para);
	WriteParameter(&File, "MIDMULTI", Para);

	m_HnaInt.GetWindowText(Para);
	WriteParameter(&File, "HNAINT", Para);

	m_HnaMult.GetWindowText(Para);
	WriteParameter(&File, "HNAMULTI", Para);

	if (m_HystCheck.GetCheck())
	{
		WriteParameter(&File, "USE_HYSTERESIS", "yes");

		m_HystScaling.GetWindowText(Para);
		WriteParameter(&File, "HYST_SCALING", Para);

		m_HystThresholdLow.GetWindowText(Para);
		WriteParameter(&File, "HYST_THR_LOW", Para);

		m_HystThresholdHigh.GetWindowText(Para);
		WriteParameter(&File, "HYST_THR_HIGH", Para);
	}

	else
		WriteParameter(&File, "USE_HYSTERESIS", "no");

	if (m_InternetCheck.GetCheck())
		WriteParameter(&File, "HNA4", "0.0.0.0 0.0.0.0");

	m_ManualWindow.GetWindowText(Para);
	File.Write(Para.GetBuffer(0), Para.GetLength());
	Para.ReleaseBuffer();

	File.Close();
	return 0;
}

void MyDialog2::OnSaveButton()
{
	CFileDialog FileDialog(FALSE);
	CString FileName = "Default.olsr";
	CString PathName;

	FileDialog.m_ofn.lpstrFilter = "Configuration file (*.olsr)\0*.olsr\0";
	FileDialog.m_ofn.nFilterIndex = 1;

	FileDialog.m_ofn.lpstrFile = FileName.GetBuffer(500);
	FileDialog.m_ofn.nMaxFile = 500;

	if (FileDialog.DoModal() == IDOK)
	{
		PathName = FileDialog.GetPathName();

		if (SaveConfigFile(PathName) < 0)
			AfxMessageBox("Cannot save configuration file '" + PathName + "'.");
	}

	FileName.ReleaseBuffer();
}

void MyDialog2::OnOpenButton() 
{
	CFileDialog FileDialog(TRUE);
	CString FileName = "Default.olsr";
	CString PathName;

	FileDialog.m_ofn.lpstrFilter = "Configuration file (*.olsr)\0*.olsr\0";
	FileDialog.m_ofn.nFilterIndex = 1;

	FileDialog.m_ofn.lpstrFile = FileName.GetBuffer(500);
	FileDialog.m_ofn.nMaxFile = 500;

	if (FileDialog.DoModal() == IDOK)
	{
		PathName = FileDialog.GetPathName();

		if (OpenConfigFile(PathName) < 0)
			AfxMessageBox("Cannot open configuration file '" + PathName + "'.");
	}

	FileName.ReleaseBuffer();
}

void MyDialog2::OnResetButton() 
{
	Reset();
}
