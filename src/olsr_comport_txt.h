
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
#ifndef OLSR_COMPORT_TXT_H_
#define OLSR_COMPORT_TXT_H_

#include "common/autobuf.h"
#include "common/avl.h"
#include "olsr_ip_acl.h"

#include "olsr_comport.h"

enum olsr_txtcommand_result {
  CONTINUE,
  CONTINOUS,
  QUIT,
  ABUF_ERROR,
  UNKNOWN,
};

typedef enum olsr_txtcommand_result (*olsr_txthandler)
    (struct comport_connection *con, const char *command, const char *parameter);

struct olsr_txtcommand {
  struct avl_node node;
  struct ip_acl *acl;

  olsr_txthandler handler;
};

AVLNODE2STRUCT(txt_tree2cmd, olsr_txtcommand, node);

void olsr_com_init_txt(void);
void olsr_com_destroy_txt(void);

struct olsr_txtcommand *EXPORT(olsr_com_add_normal_txtcommand) (
    const char *command, olsr_txthandler handler);
struct olsr_txtcommand *EXPORT(olsr_com_add_help_txtcommand) (
    const char *command, olsr_txthandler handler);
void EXPORT(olsr_com_remove_normal_txtcommand) (struct olsr_txtcommand *cmd);
void EXPORT(olsr_com_remove_help_txtcommand) (struct olsr_txtcommand *cmd);

enum olsr_txtcommand_result olsr_com_handle_txtcommand(struct comport_connection *con,
    char *command, char *parameter);

#endif /* OLSR_COMPORT_TXT_H_ */
