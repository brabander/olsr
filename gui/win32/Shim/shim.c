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

#include <windows.h>

void EntryPoint(void)
{
	STARTUPINFO StartInfo;
	PROCESS_INFORMATION ProcInfo;
	int i;
	char *CmdLine;
	char *Walker;
	char NewCmdLine[MAX_PATH + 500];
	HANDLE Handles[2];
	unsigned long Res;
	int Quotes;

	Handles[0] = OpenEvent(EVENT_ALL_ACCESS, FALSE, "TheOlsrdShimEvent");

	if (Handles[0] == NULL)
	{
		MessageBox(NULL, "Cannot open event.", "Shim Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
	}

	CmdLine = GetCommandLine();

	Quotes = 0;
	
	while (*CmdLine != 0)
	{
		if (*CmdLine == '"')
			Quotes = !Quotes;

		else if (*CmdLine == ' ' && !Quotes)
			break;

		CmdLine++;
	}

	if (*CmdLine == 0)
	{
		MessageBox(NULL, "Missing arguments.", "Shim Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
	}

	GetModuleFileName(NULL, NewCmdLine, MAX_PATH);

	for (Walker = NewCmdLine; *Walker != 0; Walker++);

	while (*Walker != '\\')
		Walker--;

	Walker[1] = 'o';
	Walker[2] = 'l';
	Walker[3] = 's';
	Walker[4] = 'r';
	Walker[5] = 'd';
	Walker[6] = '.';
	Walker[7] = 'e';
	Walker[8] = 'x';
	Walker[9] = 'e';

	Walker[10] = ' ';

	Walker += 11;

	while ((*Walker++ = *CmdLine++) != 0);

	for (i = 0; i < sizeof (STARTUPINFO); i++)
		((char *)&StartInfo)[i] = 0;

	StartInfo.cb = sizeof (STARTUPINFO);

	if (!CreateProcess(NULL, NewCmdLine, NULL, NULL, TRUE,
		CREATE_NEW_PROCESS_GROUP, NULL, NULL, &StartInfo, &ProcInfo))
	{
		MessageBox(NULL, "Cannot execute OLSR server.", "Shim Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
	}

	Handles[1] = ProcInfo.hProcess;

	Res = WaitForMultipleObjects(2, Handles, FALSE, INFINITE);

	if (Res == WAIT_OBJECT_0)
	{
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, ProcInfo.dwProcessId);
		WaitForSingleObject(ProcInfo.hProcess, INFINITE);
	}
	
	CloseHandle(ProcInfo.hThread);
	CloseHandle(ProcInfo.hProcess);

	ExitProcess(0);
}
