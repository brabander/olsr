/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
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
 * 
 * $Id: net.c,v 1.1 2004/11/05 02:06:14 tlopatic Exp $
 *
 */

#include "../defs.h"
#include "net.h"

int enable_ip_forwarding(int version)
{
  return 1;
}

int disable_redirects(char *if_name, int index, int version)
{
  return 1;
}

int deactivate_spoof(char *if_name, int index, int version)
{
  return 1;
}

int restore_settings(int version)
{
  return 1;
}

int
getsocket(struct sockaddr *sa, int bufspace, char *int_name)
{
  struct sockaddr_in *sin = (struct sockaddr_in *)sa;
  int sock, on = 1;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
      perror("socket");
      syslog(LOG_ERR, "socket: %m");
      return (-1);
    }

  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) < 0)
    {
      perror("setsockopt");
      syslog(LOG_ERR, "setsockopt SO_BROADCAST: %m");
      close(sock);
      return (-1);
    }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      return (-1);
    }

  for (on = bufspace; ; on -= 1024) 
    {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		     &on, sizeof (on)) == 0)
	break;
      if (on <= 8*1024) 
	{
	  perror("setsockopt");
	  syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
	  break;
	}
    }

  if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) 
    {
      perror("bind");
      syslog(LOG_ERR, "bind: %m");
      close(sock);
      return (-1);
    }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");

  return (sock);
}

int getsocket6(struct sockaddr_in6 *sin, int bufspace, char *int_name)
{
  int sock, on = 1;

  if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) 
    {
      perror("socket");
      syslog(LOG_ERR, "socket: %m");
      return (-1);
    }

  for (on = bufspace; ; on -= 1024) 
    {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		     &on, sizeof (on)) == 0)
	break;
      if (on <= 8*1024) 
	{
	  perror("setsockopt");
	  syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
	  break;
	}
    }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      return (-1);
    }

  if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) 
    {
      perror("bind");
      syslog(LOG_ERR, "bind: %m");
      close(sock);
      return (-1);
    }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");

  return (sock);
}

int get_ipv6_address(char *ifname, struct sockaddr_in6 *saddr6, int scope_in)
{
  return 0;
}
