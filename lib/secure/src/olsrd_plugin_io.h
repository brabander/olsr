/*
 * Secure OLSR plugin
 * http://www.olsr.org
 *
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or 
 * without modification, are permitted provided that the following 
 * conditions are met:
 *
 * * Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in 
 *   the documentation and/or other materials provided with the 
 *   distribution.
 * * Neither the name of olsrd, olsr.org nor the names of its 
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
 *POSSIBILITY OF SUCH DAMAGE.
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
