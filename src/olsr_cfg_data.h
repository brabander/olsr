
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

#ifndef OLSR_CFG_DATA_H_
#define OLSR_CFG_DATA_H_

/**
 * defines the source of a logging event
 */
enum log_source {
  LOG_ALL,                             //!< LOG_ALL
  LOG_LOGGING,                         //!< LOG_LOGGING
  LOG_IPC,                             //!< LOG_IPC
  LOG_MAIN,                            //!< LOG_MAIN
  LOG_NETWORKING,                      //!< LOG_NETWORKING
  LOG_PACKET_CREATION,                 //!< LOG_PACKET_CREATION
  LOG_PACKET_PARSING,                  //!< LOG_PACKET_PARSING
  LOG_ROUTING,                         //!< LOG_ROUTING
  LOG_SCHEDULER,                       //!< LOG_SCHEDULER
  LOG_PLUGINS,                         //!< LOG_PLUGINS
  LOG_LQ_PLUGINS,                      //!< LOG_LQ_PLUGINS
  LOG_LL_PLUGINS,                      //!< LOG_LL_PLUGINS
  LOG_LINKS,                           //!< LOG_LINKS
  LOG_NEIGHTABLE,                      //!< LOG_NEIGHTABLE
  LOG_MPR,                             //!< LOG_MPR
  LOG_MPRS,                            //!< LOG_MPRS
  LOG_2NEIGH,                          //!< LOG_2NEIGH
  LOG_TC,                              //!< LOG_TC
  LOG_HNA,                             //!< LOG_HNA
  LOG_MID,                             //!< LOG_MID
  LOG_DUPLICATE_SET,                   //!< LOG_DUPLICATE_SET
  LOG_COOKIE,                          //!< LOG_COOKIE

  /* this one must be the last of the enums ! */
  LOG_SOURCE_COUNT                     //!< LOG_SOURCE_COUNT
};

/**
 * defines the severity of a logging event
 */
enum log_severity {
  SEVERITY_DEBUG,                      //!< SEVERITY_DEBUG
  SEVERITY_INFO,                       //!< SEVERITY_INFO
  SEVERITY_WARN,                       //!< SEVERITY_WARN
  SEVERITY_ERR,                        //!< SEVERITY_ERR

  /* this one must be the last of the enums ! */
  LOG_SEVERITY_COUNT                   //!< LOG_SEVERITY_COUNT
};

/**
 * defines the mode of the interface.
 *
 * - Mesh: default behavior
 * - Ether: an interface with nearly no packet loss and a "closed" broadcast
 *   domain. This means packages received through this interface does not need
 *   to be forwarded through the interface again.
 */
enum interface_mode {
  IF_MODE_MESH,                        //!< IF_MODE_MESH
  IF_MODE_ETHER,                       //!< IF_MODE_ETHER

  /* this must be the last entry */
  IF_MODE_COUNT                        //!< IF_MODE_COUNT
};


extern const char *LOG_SOURCE_NAMES[];
extern const char *LOG_SEVERITY_NAMES[];
extern const char *INTERFACE_MODE_NAMES[];

#endif /* OLSR_CFG_DATA_H_ */
