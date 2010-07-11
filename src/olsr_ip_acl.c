
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
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

#include "ipcalc.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_ip_acl.h"

void
ip_acl_init(struct ip_acl *acl)
{
  list_init_head(&acl->accept);
  list_init_head(&acl->reject);
  acl->default_accept = false;
  acl->first_accept = false;
}

void
ip_acl_flush(struct ip_acl *acl)
{
  ip_prefix_list_flush(&acl->accept);
  ip_prefix_list_flush(&acl->reject);
}

void
ip_acl_add(struct ip_acl *acl, const union olsr_ip_addr *net, uint8_t prefix_len, bool reject)
{
  ip_prefix_list_add(reject ? &acl->reject : &acl->accept, net, prefix_len);
}

void
ip_acl_remove(struct ip_acl *acl, const union olsr_ip_addr *net, uint8_t prefix_len, bool reject, int ip_version)
{
  ip_prefix_list_remove(reject ? &acl->reject : &acl->accept, net, prefix_len, ip_version);
}

bool
ip_acl_acceptable(struct ip_acl *acl, const union olsr_ip_addr *ip, int ip_version)
{
  struct list_entity *first, *second;
  struct ip_prefix_entry *entry;
  struct list_iterator iterator;

  first = acl->first_accept ? &acl->accept : &acl->reject;
  second = acl->first_accept ? &acl->reject : &acl->accept;

  /* first run */
  OLSR_FOR_ALL_IPPREFIX_ENTRIES(first, entry, iterator) {
    if (ip_in_net(ip, &entry->net, ip_version)) {
      return acl->first_accept;
    }
  }

  /* second run */
  OLSR_FOR_ALL_IPPREFIX_ENTRIES(second, entry, iterator) {
    if (ip_in_net(ip, &entry->net, ip_version)) {
      return !acl->first_accept;
    }
  }

  /* just use default */
  return acl->default_accept;
}

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
