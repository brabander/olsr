/*
 * $Id: tunnel.h,v 1.2 2004/09/15 11:18:42 tlopatic Exp $
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

#if !defined TL_TUNNEL_H_INCLUDED

#define TL_TUNNEL_H_INCLUDED

struct ip_tunnel_parm
{
  int dummy;
};

void set_up_source_tnl(union olsr_ip_addr *, union olsr_ip_addr *, int);
int del_ip_tunnel(struct ip_tunnel_parm *);
int set_up_gw_tunnel();

#define IOCTL_IPINIP_ADD_TUNNEL_INTERFACE 0x00128000
#define IOCTL_IPINIP_DELETE_TUNNEL_INTERFACE 0x00128004
#define IOCTL_IPINIP_SET_TUNNEL_INFO 0x00128008
#define IOCTL_IPINIP_GET_TUNNEL_TABLE 0x0012800c
#define IOCTL_IPINIP_PROCESS_NOTIFICATION 0x00128010

#define STATUS_NON_OPERATIONAL 0
#define STATUS_OPERATIONAL 5

struct IpInIpAddTunnelInterface
{
  // interface index (out)

  unsigned int Index;

  // interface GUID (in)

  unsigned char Guid[16];
};

struct IpInIpDeleteTunnelInterface
{
  // interface index (in)

  unsigned int Index;
};

struct IpInIpSetTunnelInfo
{
  // interface index (in)

  unsigned int Index;

  // interface status (out)
  // can be either STATUS_OPERATIONAL or STATUS_NON_OPERATIONAL

  unsigned int Status;

  // remote tunnel endpoint (in)
  // given in network byte order

  unsigned int RemoteAddr;

  // local tunnel endpoint (in)
  // given in network byte order

  unsigned int LocalAddr;

  // TTL for outbound outer packets (in)

  unsigned char TimeToLive;
};

struct IpInIpTunnelEntry
{
  // interface index

  unsigned int Index;

  // remote tunnel endpoint

  unsigned int RemoteAddr;

  // local tunnel endpoint

  unsigned int LocalAddr;

  // ???
  // can be either 0x08000000 or 0x00000000

  unsigned int Unknown1;

  // TTL for outbound outer packets

  unsigned char TimeToLive;
};

struct IpInIpGetTunnelTable
{
  // number of tunnel entries (out)

  unsigned int Num;

  // array of tunnel entries

  struct IpInIpTunnelEntry Entries[1];
};

struct IpInIpProcessNotification
{
  // interface status
  // 0 = interface is up, 1 = interface is down

  int Down;

  // reason for the status change
  // 0 = ICMP time exceeded, 1 = ICMP destination unreachable,
  // -1 = detected by periodic status poll

  int Reason;

  // interface index

  unsigned int Index;
};

#endif
