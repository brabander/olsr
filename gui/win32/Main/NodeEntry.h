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

#if !defined TL_NODEENTRY_H
#define TL_NODEENTRY_H

#include "MprEntry.h"
#include "MidEntry.h"
#include "HnaEntry.h"

class NodeEntry
{
public:
	unsigned int Addr;
	unsigned __int64 Timeout;

	CList<class MprEntry, class MprEntry &> MprList;
	CList<class MidEntry, class MidEntry &> MidList;
	CList<class HnaEntry, class HnaEntry &> HnaList;

	void AddMpr(unsigned int, unsigned __int64);
	void AddMid(unsigned int, unsigned __int64);
	void AddHna(unsigned int, unsigned int, unsigned __int64);

	class NodeEntry &NodeEntry::operator=(class NodeEntry &);
	BOOL NodeEntry::operator==(const class NodeEntry &) const;
};

#endif
