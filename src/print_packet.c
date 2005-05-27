/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * * Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in 
 *   the documentation and/or other materials provided with the 
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its 
 *   contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 * $Id: print_packet.c,v 1.3 2005/05/27 07:00:32 kattemat Exp $
 */

#include "print_packet.h"
#include "mantissa.h"
#include "defs.h"
#include "olsr.h"
#include "lq_packet.h"

static void
print_messagedump(FILE *, olsr_u8_t *, olsr_16_t);

static void
print_midmsg(FILE *, olsr_u8_t *, olsr_16_t);

static void
print_hnamsg(FILE *, olsr_u8_t *, olsr_16_t);

static void
print_tcmsg(FILE *, olsr_u8_t *, olsr_16_t);

static void
print_tcmsg_lq(FILE *, olsr_u8_t *, olsr_16_t);

static void
print_hellomsg(FILE *, olsr_u8_t *, olsr_16_t);

/* Entire packet */
olsr_8_t
print_olsr_serialized_packet(FILE *handle, union olsr_packet *pkt, 
			     olsr_u16_t size, union olsr_ip_addr *from_addr)
{
  olsr_16_t remainsize = size - OLSR_HEADERSIZE;
  union olsr_message *msg;

  /* Print packet header (no IP4/6 difference) */
  fprintf(handle, "  ============== OLSR PACKET ==============\n   source: %s\n   length: %d bytes\n   seqno: %d\n\n",
	  from_addr ? olsr_ip_to_string(from_addr) : "UNKNOWN",
	  ntohs(pkt->v4.olsr_packlen), ntohs(pkt->v4.olsr_seqno));

  /* Check size */
  if(size != ntohs(pkt->v4.olsr_packlen))
    fprintf(handle, "   SIZE MISSMATCH(%d != %d)!\n", size, ntohs(pkt->v4.olsr_packlen));

  msg = (union olsr_message *)pkt->v4.olsr_msg;

  /* Print all messages */
  while((remainsize > 0) && ntohs(msg->v4.olsr_msgsize))
    {
      print_olsr_serialized_message(handle, msg);
      remainsize -= ntohs(msg->v4.olsr_msgsize);
      msg = (union olsr_message *)((int)msg + (int)ntohs(msg->v4.olsr_msgsize));
    }

  /* Done */
  fprintf(handle, "  =========================================\n\n");
  return 1;
}

/* Single message */
olsr_8_t
print_olsr_serialized_message(FILE *handle, union olsr_message *msg)
{

  fprintf(handle, "   ------------ OLSR MESSAGE ------------\n");
  fprintf(handle, "    Sender main addr: %s\n", 
	  olsr_ip_to_string((union olsr_ip_addr *)&msg->v4.originator));
  fprintf(handle, "    Type: %s, size: %d, vtime: %0.2f\n", 
	  olsr_msgtype_to_string(msg->v4.olsr_msgtype), 
	  ntohs(msg->v4.olsr_msgsize),
	  ME_TO_DOUBLE(msg->v4.olsr_vtime));
  fprintf(handle, "    TTL: %d, Hopcnt: %d, seqno: %d\n",
	  (olsr_cnf->ip_version == AF_INET) ? msg->v4.ttl : msg->v6.ttl,
	  (olsr_cnf->ip_version == AF_INET) ? msg->v4.hopcnt : msg->v6.hopcnt,
	  ntohs((olsr_cnf->ip_version == AF_INET) ? msg->v4.seqno : msg->v6.seqno));

  switch(msg->v4.olsr_msgtype)
    {
      /* Print functions for individual messagetypes */
    case(MID_MESSAGE):
      print_midmsg(handle,
		   (olsr_cnf->ip_version == AF_INET) ? 
		   (olsr_u8_t *)&msg->v4.message : (olsr_u8_t *)&msg->v6.message,
		   ntohs(msg->v4.olsr_msgsize));
      break;
    case(HNA_MESSAGE):
      print_hnamsg(handle,
		   (olsr_cnf->ip_version == AF_INET) ? 
		   (olsr_u8_t *)&msg->v4.message : (olsr_u8_t *)&msg->v6.message,
		   ntohs(msg->v4.olsr_msgsize));
      break;
    case(TC_MESSAGE):
      print_tcmsg(handle,
		  (olsr_cnf->ip_version == AF_INET) ? 
		  (olsr_u8_t *)&msg->v4.message : (olsr_u8_t *)&msg->v6.message,
		  ntohs(msg->v4.olsr_msgsize));
      break;
    case(LQ_TC_MESSAGE):
      print_tcmsg_lq(handle,
		     (olsr_cnf->ip_version == AF_INET) ? 
		     (olsr_u8_t *)&msg->v4.message : (olsr_u8_t *)&msg->v6.message,
		     ntohs(msg->v4.olsr_msgsize));
      break;
    case(HELLO_MESSAGE):
      print_hellomsg(handle,
		     (olsr_cnf->ip_version == AF_INET) ? 
		     (olsr_u8_t *)&msg->v4.message : (olsr_u8_t *)&msg->v6.message,
		     ntohs(msg->v4.olsr_msgsize));
      break;
    case(LQ_HELLO_MESSAGE):
    default:
      print_messagedump(handle, (olsr_u8_t *)msg, ntohs(msg->v4.olsr_msgsize));
    }

  fprintf(handle, "   --------------------------------------\n\n");
  return 1;
}


static void
print_messagedump(FILE *handle, olsr_u8_t *msg, olsr_16_t size)
{
  int i, x = 0;

  fprintf(handle, "     Data dump:\n     ");
  for(i = 0; i < size; i++)
    {
      if(x == 4)
	{
	  x = 0;
	  fprintf(handle, "\n     ");
	}
      x++;
      if(olsr_cnf->ip_version == AF_INET)
	fprintf(handle, " %-3i ", (u_char) msg[i]);
      else
	fprintf(handle, " %-2x ", (u_char) msg[i]);
    }
  fprintf(handle, "\n");
}


static void
print_hellomsg(FILE *handle, olsr_u8_t *data, olsr_16_t totsize)
{
  int remsize = totsize - ((olsr_cnf->ip_version == AF_INET) ? OLSR_MSGHDRSZ_IPV4 : OLSR_MSGHDRSZ_IPV6);

  data +=2 ;
  remsize -= 2;
  fprintf(handle, "    +Htime: %0.2f\n", ME_TO_DOUBLE(*data));

  data += 1;
  remsize -= 1;
  fprintf(handle, "    +Willingness: %d\n", *data);

  /* ToDo: print neighor sets */

  /* TESTING TESTING */
}

static void
print_tcmsg_lq(FILE *handle, olsr_u8_t *data, olsr_16_t totsize)
{
  int remsize = totsize - ((olsr_cnf->ip_version == AF_INET) ? OLSR_MSGHDRSZ_IPV4 : OLSR_MSGHDRSZ_IPV6);
  
  fprintf(handle, "    +ANSN: %d\n", htons(((struct tcmsg *)data)->ansn));

  data += 4;
  remsize -= 4;

  while(remsize)
    {
      fprintf(handle, "    +Neighbor: %s\n", olsr_ip_to_string((union olsr_ip_addr *) data));
      data += ipsize;
      fprintf(handle, "    +LQ: %d, ", *data);
      data += 1;
      fprintf(handle, "RLQ: %d\n", *data);
      data += 2;
      remsize -= (ipsize + 4);
    }

}


static void
print_tcmsg(FILE *handle, olsr_u8_t *data, olsr_16_t totsize)
{
  int remsize = totsize - ((olsr_cnf->ip_version == AF_INET) ? OLSR_MSGHDRSZ_IPV4 : OLSR_MSGHDRSZ_IPV6);
  
  fprintf(handle, "    +ANSN: %d\n", htons(((struct tcmsg *)data)->ansn));

  data += 4;
  remsize -= 4;

  while(remsize)
    {
      fprintf(handle, "    +Neighbor: %s\n", olsr_ip_to_string((union olsr_ip_addr *) data));
      data += ipsize;

      remsize -= ipsize;
    }

}


static void
print_hnamsg(FILE *handle, olsr_u8_t *data, olsr_16_t totsize)
{
  int remsize = totsize - ((olsr_cnf->ip_version == AF_INET) ? OLSR_MSGHDRSZ_IPV4 : OLSR_MSGHDRSZ_IPV6);

  while(remsize)
    {
      fprintf(handle, "    +Network: %s\n", olsr_ip_to_string((union olsr_ip_addr *) data));
      data += ipsize;
      fprintf(handle, "    +Netmask: %s\n", olsr_ip_to_string((union olsr_ip_addr *) data));
      data += ipsize;

      remsize -= (ipsize*2);
    }

}

static void
print_midmsg(FILE *handle, olsr_u8_t *data, olsr_16_t totsize)
{
  int remsize = totsize - ((olsr_cnf->ip_version == AF_INET) ? OLSR_MSGHDRSZ_IPV4 : OLSR_MSGHDRSZ_IPV6);

  while(remsize)
    {
      fprintf(handle, "    +Alias: %s\n", olsr_ip_to_string((union olsr_ip_addr *) data));
      data += ipsize;
      remsize -= ipsize;
    }
}
