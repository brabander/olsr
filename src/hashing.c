/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
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
 * $Id: hashing.c,v 1.5 2004/09/21 19:08:57 kattemat Exp $
 *
 */


#include "olsr_protocol.h"
#include "hashing.h"


/**
 *Hashing function. Creates a key based on
 *an 32-bit address.
 *@param address the address to hash
 *@return the hash(a value in the 0-31 range)
 */
olsr_u32_t
olsr_hashing(union olsr_ip_addr *address)
{
  olsr_u32_t hash;
  char *tmp;

  if(ipversion == AF_INET)
    /* IPv4 */  
    hash = (ntohl(address->v4));
  else
    {
      /* IPv6 */
      tmp = (char *) &address->v6;
      hash = (ntohl(*tmp));
    }

  /* REMOVE */
#ifdef DEBUG
#warning Remove debug output in hash.c
  olsr_printf(1, "HASH %s->%d:", olsr_ip_to_string(address), hash);
#endif

  //hash &= 0x7fffffff; 
  hash &= HASHMASK;

#ifdef DEBUG
  olsr_printf(1, "%d\n", hash);
#endif


  return hash;
}
