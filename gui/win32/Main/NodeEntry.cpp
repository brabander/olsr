/*
 * $Id: NodeEntry.cpp,v 1.2 2004/09/15 11:18:41 tlopatic Exp $
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
#include "NodeEntry.h"

class NodeEntry &NodeEntry::operator=(class NodeEntry &Src)
{
	Addr = Src.Addr;
	Timeout = Src.Timeout;

	MprList.RemoveAll();
	MprList.AddHead(&Src.MprList);

	HnaList.RemoveAll();
	HnaList.AddHead(&Src.HnaList);

	MidList.RemoveAll();
	MidList.AddHead(&Src.MidList);

	return *this;
}

BOOL NodeEntry::operator==(const class NodeEntry &Comp) const
{
	return Addr == Comp.Addr;
}

void NodeEntry::AddMpr(unsigned int MprAddr, unsigned __int64 Timeout)
{
	class MprEntry NewEntry;
	POSITION Pos;

	NewEntry.Addr = MprAddr;

	Pos = MprList.Find(NewEntry);

	if (Pos != NULL)
	{
		class MprEntry &OldEntry = MprList.GetAt(Pos);
		OldEntry.Timeout = Timeout;
	}

	else
	{
		NewEntry.Timeout = Timeout;
		MprList.AddTail(NewEntry);
	}
}

void NodeEntry::AddMid(unsigned int IntAddr, unsigned __int64 Timeout)
{
	class MidEntry NewEntry;
	POSITION Pos;

	NewEntry.Addr = IntAddr;

	Pos = MidList.Find(NewEntry);

	if (Pos != NULL)
	{
		class MidEntry &OldEntry = MidList.GetAt(Pos);
		OldEntry.Timeout = Timeout;
	}

	else
	{
		NewEntry.Timeout = Timeout;
		MidList.AddTail(NewEntry);
	}
}

void NodeEntry::AddHna(unsigned int NetAddr, unsigned int NetMask,
					   unsigned __int64 Timeout)
{
	class HnaEntry NewEntry;
	POSITION Pos;

	NewEntry.Addr = NetAddr;
	NewEntry.Mask = NetMask;

	Pos = HnaList.Find(NewEntry);

	if (Pos != NULL)
	{
		class HnaEntry &OldEntry = HnaList.GetAt(Pos);
		OldEntry.Timeout = Timeout;
	}

	else
	{
		NewEntry.Timeout = Timeout;
		HnaList.AddTail(NewEntry);
	}
}
