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
 * $Id: olsr_plugin_io.h,v 1.8 2004/11/07 12:19:58 kattemat Exp $
 *
 */

/*
 * REVISIONS(starting from 0.4.6):
 * 0.4.5 - 0.4.6 : GETD_S removed. The socket entries now reside within the 
 *                 interface struct.
 *                 Added GETF__ADD_IFCHGF and GETF__DEL_IFCHGF.
 *                 - Andreas
 *         0.4.8 : GETF__APM_READ added
 *                 GETD__OLSR_CNF added
 *                 GETD_PACKET removed
 *                 GETD_MAXMESSAGESIZE removed
 *                 GETD_OUTPUTSIZE removed
 *                 GETF__NET_OUTBUFFER_PUSH added
 *                 - Andreas
 */

/*
 * IO commands
 *
 * NAMING CONVENTION:
 * - DATAPOINTERS
 *   Commands to get datapointers MUST have the prefix
 *   GETD__ added to the full name of the variable/pointer
 *   in all upper cases.
 *   Example: A command to get a pointer to a variable called
 *   "myvar" in olsrd must be called GETD__MYVAR
 *
 * - FUNCTIONS
 *   Commands to get pointers to olsrd functions MUST have
 *   the prefix GETF__ added to the full name of the runction
 *   in uppercases.
 *   Example: A command to get a pointer to the function
 *   "my_function" must be named GETF__MY_FUNCTION
 *
 *
 *   New commands can be added - BUT EXISTING COMMANDS MUST
 *   _NEVER_ CHANGE VALUE!
 */

#ifndef _OLSR_PLUGIN_IO
#define _OLSR_PLUGIN_IO

/* Data fetching - starts at 100 (used to anyway) */
#define GETD__IFNET                                102
#define GETD__NOW                                  103
#define GETD__PARSER_ENTRIES                       104
#define GETD__OLSR_SOCKET_ENTRIES                  105
#define GETD__NEIGHBORTABLE                        108
#define GETD__TWO_HOP_NEIGHBORTABLE                109
#define GETD__TC_TABLE                             110
#define GETD__HNA_SET                              111
#define GETD__OLSR_CNF                             112

/* Function fetching - starts at 500 */
#define GETF__OLSR_REGISTER_SCHEDULER_EVENT        500
#define GETF__OLSR_REMOVE_SCHEDULER_EVENT          501
#define GETF__OLSR_PARSER_ADD_FUNCTION             502
#define GETF__OLSR_PARSER_REMOVE_FUNCTION          503
#define GETF__OLSR_REGISTER_TIMEOUT_FUNCTION       504
#define GETF__OLSR_REMOVE_TIMEOUT_FUNCTION         505
#define GETF__GET_MSG_SEQNO                        506
#define GETF__OLSR_CHECK_DUP_TABLE_PROC            507
#define GETF__NET_OUTPUT                           508
#define GETF__OLSR_FORWARD_MESSAGE                 509
#define GETF__ADD_OLSR_SOCKET                      510
#define GETF__REMOVE_OLSR_SOCKET                   511
#define GETF__CHECK_NEIGHBOR_LINK                  512
#define GETF__OLSR_PRINTF                          513
#define GETF__OLSR_MALLOC                          514
#define GETF__DOUBLE_TO_ME                         515
#define GETF__ME_TO_DOUBLE                         516
#define GETF__ADD_LOCAL_HNA4_ENTRY                 517
#define GETF__REMOVE_LOCAL_HNA4_ENTRY              518
#define GETF__ADD_LOCAL_HNA6_ENTRY                 519
#define GETF__REMOVE_LOCAL_HNA6_ENTRY              520
#define GETF__OLSR_INPUT                           521
#define GETF__ADD_PTF                              522
#define GETF__DEL_PTF                              523
#define GETF__IF_IFWITHSOCK                        524
#define GETF__IF_IFWITHADDR                        525
#define GETF__PARSE_PACKET                         526
#define GETF__REGISTER_PCF                         527
#define GETF__OLSR_HASHING                         528
#define GETF__ADD_IFCHGF                           529
#define GETF__DEL_IFCHGF                           530
#define GETF__APM_READ                             531
#define GETF__NET_OUTBUFFER_PUSH                   532

#endif
