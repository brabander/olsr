/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
 *
 * UniK olsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UniK olsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $ Id $
 *
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
 */


/* Data fetching - starts at 100 */
#define GETD__PACKET                               100                            
#define GETD__OUTPUTSIZE                           101
#define GETD__IFNET                                102
#define GETD__NOW                                  103
 
/* Function fetching - starts at 500 */
#define GETF__OLSR_REGISTER_SCHEDULER_EVENT        500
#define GETF__OLSR_PARSER_ADD_FUNCTION             501
#define GETF__OLSR_REGISTER_TIMEOUT_FUNCTION       502
#define GETF__GET_MSG_SEQNO                        503
#define GETF__OLSR_CHECK_DUP_TABLE_PROC            504
#define GETF__NET_OUTPUT                           505
#define GETF__OLSR_FORWARD_MESSAGE                 506
#define GETF__ADD_OLSR_SOCKET                      507
#define GETF__REMOVE_OLSR_SOCKET                   508
#define GETF__CHECK_NEIGHBOR_LINK                  509
#define GETF__OLSR_PRINTF                          510
#define GETF__OLSR_MALLOC                          511
#define GETF__DOUBLE_TO_ME                         512
#define GETF__ME_TO_DOUBLE                         513
