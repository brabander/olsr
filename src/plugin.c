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
 * $Id: plugin.c,v 1.9 2004/11/07 12:19:58 kattemat Exp $
 *
 */


#include "olsr_plugin_io.h"
#include <stdio.h>
#include "plugin.h"
#include "olsr.h"
#include "defs.h"
#include "parser.h"
#include "scheduler.h"
#include "duplicate_set.h"
#include "link_set.h"
#include "mantissa.h"
#include "local_hna_set.h"
#include "socket_parser.h"
#include "neighbor_table.h"
#include "two_hop_neighbor_table.h"
#include "tc_set.h"
#include "hna_set.h"
#include "apm.h"

/**
 * Multi-purpose function for plugins
 * Syntax much the same as the ioctl(2) call 
 *
 *@param cmd the command
 *@param data pointer to memory to put/get data
 *@param size size of the memory pointed to
 *
 *@return negative if unknown command
 */

int
olsr_plugin_io(int cmd, void *data, size_t size)
{
  void *ptr;

  olsr_printf(3, "olsr_plugin_io(%d)\n", cmd);

  switch(cmd)
    {

      /* Data fetching */
    case(GETD__IFNET):
      *((struct interface **)data) = ifnet;
      break;
    case(GETD__NOW):
      *((struct timeval **)data) = &now;
      break;
    case(GETD__PARSER_ENTRIES):
      *((struct parse_function_entry **)data) = parse_functions;
      break;
    case(GETD__OLSR_SOCKET_ENTRIES):
      *((struct olsr_socket_entry **)data) = olsr_socket_entries;
      break;
    case(GETD__NEIGHBORTABLE):
      *((struct neighbor_entry **)data) = neighbortable;
      break;
    case(GETD__TWO_HOP_NEIGHBORTABLE):
      *((struct neighbor_2_entry **)data) = two_hop_neighbortable;
      break;
     case(GETD__TC_TABLE):
      *((struct tc_entry **)data) = tc_table;
      break;
     case(GETD__HNA_SET):
      *((struct hna_entry **)data) = hna_set;
      break;
     case(GETD__OLSR_CNF):
      *((struct olsrd_config **)data) = olsr_cnf;
      break;

      /* Function fetching */

    case(GETF__OLSR_PRINTF):
      ptr = &olsr_printf;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_MALLOC):
      ptr = &olsr_malloc;
      memcpy(data, &ptr, size);
      break;
    case(GETF__DOUBLE_TO_ME):
      ptr = &double_to_me;
      memcpy(data, &ptr, size);
      break;
    case(GETF__ME_TO_DOUBLE):
      ptr = &me_to_double;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_REGISTER_SCHEDULER_EVENT):
      ptr = &olsr_register_scheduler_event;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_REMOVE_SCHEDULER_EVENT):
      ptr = &olsr_remove_scheduler_event;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_PARSER_ADD_FUNCTION):
      ptr = &olsr_parser_add_function;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_PARSER_REMOVE_FUNCTION):
      ptr = &olsr_parser_remove_function;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_REGISTER_TIMEOUT_FUNCTION):
      ptr = &olsr_register_timeout_function;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_REMOVE_TIMEOUT_FUNCTION):
      ptr = &olsr_remove_timeout_function;
      memcpy(data, &ptr, size);
      break;
    case(GETF__GET_MSG_SEQNO):
      ptr = &get_msg_seqno;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_CHECK_DUP_TABLE_PROC):
      ptr = &olsr_check_dup_table_proc;
      memcpy(data, &ptr, size);
      break;
    case(GETF__NET_OUTPUT):
      ptr =  &net_output;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_FORWARD_MESSAGE):
      ptr = &olsr_forward_message;
      memcpy(data, &ptr, size);
      break;
    case(GETF__ADD_OLSR_SOCKET):
      ptr = &add_olsr_socket;
      memcpy(data, &ptr, size);
      break;
    case(GETF__REMOVE_OLSR_SOCKET):
      ptr = &remove_olsr_socket;
      memcpy(data, &ptr, size);
      break;
    case(GETF__CHECK_NEIGHBOR_LINK):
      ptr = &check_neighbor_link;
      memcpy(data, &ptr, size);
      break;
    case(GETF__ADD_LOCAL_HNA4_ENTRY):
      ptr = &add_local_hna4_entry;
      memcpy(data, &ptr, size);
      break;
    case(GETF__REMOVE_LOCAL_HNA4_ENTRY):
      ptr = &remove_local_hna4_entry;
      memcpy(data, &ptr, size);
      break;
    case(GETF__ADD_LOCAL_HNA6_ENTRY):
      ptr = &add_local_hna6_entry;
      memcpy(data, &ptr, size);
      break;
    case(GETF__REMOVE_LOCAL_HNA6_ENTRY):
      ptr = &remove_local_hna6_entry;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_INPUT):
      ptr = &olsr_input;
      memcpy(data, &ptr, size);
      break;
    case(GETF__ADD_PTF):
      ptr = &add_ptf;
      memcpy(data, &ptr, size);
      break;
    case(GETF__DEL_PTF):
      ptr = &del_ptf;
      memcpy(data, &ptr, size);
      break;
    case(GETF__IF_IFWITHSOCK):
      ptr = &if_ifwithsock;
      memcpy(data, &ptr, size);
      break;
    case(GETF__IF_IFWITHADDR):
      ptr = &if_ifwithaddr;
      memcpy(data, &ptr, size);
      break;
    case(GETF__PARSE_PACKET):
      ptr = &parse_packet;
      memcpy(data, &ptr, size);
      break;
    case(GETF__REGISTER_PCF):
      ptr = &register_pcf;
      memcpy(data, &ptr, size);
      break;
    case(GETF__OLSR_HASHING):
      ptr = &olsr_hashing;
      memcpy(data, &ptr, size);
      break;
    case(GETF__ADD_IFCHGF):
      ptr = &add_ifchgf;
      memcpy(data, &ptr, size);
      break;
    case(GETF__DEL_IFCHGF):
      ptr = &del_ifchgf;
      memcpy(data, &ptr, size);
      break;
    case(GETF__APM_READ):
      ptr = &apm_read;
      memcpy(data, &ptr, size);
      break;
    case(GETF__NET_OUTBUFFER_PUSH):
      ptr = &net_outbuffer_push;
      memcpy(data, &ptr, size);
      break;

    default:
      return -1;
    }

  return 1;
}
