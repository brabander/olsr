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
 */

#include "rebuild_packet.h"
#include "ipcalc.h"
#include "defs.h"
#include "olsr.h"
#include "mid_set.h"
#include "mantissa.h"
#include "net_olsr.h"

/**
 *Process/rebuild a message of unknown type. Converts the OLSR
 *packet to the internal unknown_message format.
 *@param umsg the unknown_message struct in wich infomation
 *is to be put.
 *@param m the entire OLSR message revieved.
 *@return negative on error
 */

void
unk_chgestruct(struct unknown_message *umsg, const union olsr_message *m)
{

  /* Checking if everything is ok */
  if (!m)
    return;


  if(olsr_cnf->ip_version == AF_INET)
    {
      /* IPv4 */
      /* address */
      umsg->originator.v4.s_addr = m->v4.originator;
      /*seq number*/
      umsg->seqno = ntohs(m->v4.seqno);
      /* type */
      umsg->type = m->v4.olsr_msgtype;
    }
  else
    {
      /* IPv6 */
      /* address */
      umsg->originator.v6 = m->v6.originator;
      /*seq number*/
      umsg->seqno = ntohs(m->v6.seqno);
      /* type */
      umsg->type = m->v4.olsr_msgtype;
    }
  
}
