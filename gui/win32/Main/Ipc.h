/*
 * $Id: Ipc.h,v 1.2 2004/09/15 11:18:41 tlopatic Exp $
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

#if !defined TL_IPC_H
#define TL_IPC_H

#define MSG_TYPE_OLSR_HELLO 1
#define MSG_TYPE_OLSR_TC 2
#define MSG_TYPE_OLSR_MID 3
#define MSG_TYPE_OLSR_HNA 4

#define MSG_TYPE_IPC_ROUTE 11
#define MSG_TYPE_IPC_CONFIG 12

#pragma pack(push, BeforeIpcMessages, 1)

struct OlsrHeader
{
	unsigned char Type;
	unsigned char VTime;
	unsigned short Size;
	unsigned int Orig;
	unsigned char Ttl;
	unsigned char Hops;
	unsigned short SeqNo;
};

struct OlsrHello
{
	struct OlsrHeader Header;

	unsigned short Reserved;
	unsigned char HTime;
	unsigned char Will;
};

struct OlsrHelloLink
{
	unsigned char LinkCode;
	unsigned char Reserved;
	unsigned short Size;
};

struct OlsrTc
{
	struct OlsrHeader Header;

	unsigned short Ansn;
	unsigned short Reserved;
};

union IpcIpAddr
{
	unsigned int v4;
	unsigned char v6[16];
};

struct IpcHeader
{
	unsigned char Type;
	unsigned char Reserved;
	unsigned short Size;
};

struct IpcRoute
{
	struct IpcHeader Header;
	
	unsigned char Metric;
	unsigned char Add;
	unsigned char Reserved[2];
	union IpcIpAddr Dest;
	union IpcIpAddr Gate;
	char Int[4];
};

struct IpcConfig
{
	struct IpcHeader Header;

	unsigned char NumMid;
	unsigned char NumHna;
	unsigned char Reserved1[2];
	unsigned short HelloInt;
	unsigned short WiredHelloInt;
	unsigned short TcInt;
	unsigned short HelloHold;
	unsigned short TcHold;
	unsigned char Ipv6;
	unsigned char Reserved2;
	IpcIpAddr MainAddr;
};

#pragma pack (pop, BeforeIpcMessages)

#endif
