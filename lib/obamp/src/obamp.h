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


#ifndef _OBAMP_OBAMP_H
#define _OBAMP_OBAMP_H

#include "list.h"

#include "plugin.h"             /* union set_plugin_parameter_addon */

#include "parser.h"

#define MESSAGE_TYPE 		133 			//TODO: check if this number is ok
#define PARSER_TYPE		MESSAGE_TYPE
#define EMISSION_INTERVAL       10      		/* seconds */
#define EMISSION_JITTER         25      		/* percent */
#define OBAMP_VALID_TIME	1800   			/* seconds */

/* OBAMP plugin data */
#define PLUGIN_NAME 		"OLSRD OBAMP plugin"
#define PLUGIN_NAME_SHORT 	"OLSRD OBAMP"
#define PLUGIN_VERSION 		"1.0.0 (" __DATE__ " " __TIME__ ")"
#define PLUGIN_COPYRIGHT 	"  (C) Ninux.org"
#define PLUGIN_AUTHOR 		"  Saverio Proto (zioproto@gmail.com)"
#define MOD_DESC PLUGIN_NAME " " PLUGIN_VERSION "\n" PLUGIN_COPYRIGHT "\n" PLUGIN_AUTHOR
#define PLUGIN_INTERFACE_VERSION 5

#define PLUGIN_DESCR 		"OBAMP"

#define OBAMP_JITTER         	25 			/* percent */
#define OBAMP_ALIVE_EIVAL    	5
#define OBAMP_MESH_CREATE_IVAL	5 			//Seconds
#define OBAMP_TREE_CREATE_IVAL	10 			//seconds
#define TREE_HEARTBEAT    	25 			//seconds
#define OBAMP_OUTER_TREE_CREATE_IVAL    30 		//seconds
#define _MESH_LOCK_		10			//seconds

#define _Texpire_		15			 //time in seconds before expire a neighbor
#define _Texpire_timer_		1			//time in seconds to parse the list decrement and purge


/*OBAMP Protocol MESSAGE IDs */

#define OBAMP_DATA 		0
#define OBAMP_HELLO 		1 
#define OBAMP_TREECREATE 	2 
#define OBAMP_ALIVE 		3
#define OBAMP_TREE_REQ 		4
#define OBAMP_TREE_ACK 		5

#define OBAMP_SIGNALLING_PORT 	6226



extern struct ObampNodeState* myState; //Internal state of the running OBAMP node

/* Forward declaration of OLSR interface type */
struct interface;

union olsr_ip_addr *MainAddressOf(union olsr_ip_addr *ip);

void ObampSignalling(int sd, void *x, unsigned int y);

//Gets packets from sniffing interfaces, checks if are multicast, and pushes them to OBAMP tree links
void EncapFlowInObamp(int sd, void * x, unsigned int y);

//Used to add Sniffing Interfaces read from config file to the list
int AddObampSniffingIf(const char *ifName, void *data, set_plugin_parameter_addon addon);


int InitOBAMP(void);
int PreInitOBAMP(void);
void CloseOBAMP(void);

void olsr_obamp_gen(unsigned char *packet, int len);

void obamp_hello_gen(void *para);
void obamp_alive_gen(void *para);
void purge_nodes(void *para);
void mesh_create(void *para);
void tree_create(void *para);
void outer_tree_create(void *para);

int addObampNode4(struct in_addr * ipv4, u_int8_t status);


/* Parser function to register with the scheduler */
void olsr_parser(union olsr_message *, struct interface *, union olsr_ip_addr *, enum duplicate_status);

//Struct to describe the other OBAMP nodes in the mesh network
struct ObampNode {

	union olsr_ip_addr neighbor_ip_addr; //IP address

	int isMesh; //The consider the path from us to this OBAMP node as an overlay mesh link
	int wasMesh;
	int outerTreeLink; //I'm using this OBAMP node as an anchor
	int isTree;//it identifies if it is a link of tree
	int MeshLock; //Is mesh because requested from neighbor	with hello messages

	u_int8_t status; //indicates if this OBAMP node has at least a tree link
	
	int Texpire;// TTL to softstate expire

	u_int8_t DataSeqNumber;

  	struct list_head list;
};

//Interfaces of the router not talking OLSR, where we capture the multicast traffic
struct ObampSniffingIf {

int skd; 		//Socket descriptor
char ifName[16]; 	//Interface name
struct list_head list;

};


//Internal State of the OBAMP NoDE
struct ObampNodeState {
    
    union olsr_ip_addr myipaddr; //IP ADDRESS
    union olsr_ip_addr CoreAddress; //CORE IP ADDRESS

    int iamcore; //Indicates if I'm the core
    
    u_int8_t TreeCreateSequenceNumber;
    u_int8_t tree_req_sn;
    u_int8_t DataSequenceNumber;

    union olsr_ip_addr ParentId;	//Tree link towards the core
    union olsr_ip_addr OldParentId;

    /*
	TTL to check if I'm receiving tree_create messages from core
	if this expires there is a problem with the spanning tree

	*/
    int TreeHeartBeat;	
};

// OBAMP message types


struct OBAMP_data_message {

u_int8_t MessageID;
union olsr_ip_addr router_id;
union olsr_ip_addr last_hop;
u_int8_t SequenceNumber;
union olsr_ip_addr CoreAddress;
u_int8_t datalen;
unsigned char data[1280]; //TODO:fix me

};

struct OBAMP_tree_create {

u_int8_t MessageID;
union olsr_ip_addr router_id;
u_int8_t SequenceNumber;
union olsr_ip_addr CoreAddress;

};

struct OBAMP_tree_link_req {

u_int8_t MessageID;
union olsr_ip_addr router_id;
u_int8_t SequenceNumber;
union olsr_ip_addr CoreAddress;
};

struct OBAMP_tree_link_ack {

u_int8_t MessageID;
union olsr_ip_addr router_id;
u_int8_t SequenceNumber;
union olsr_ip_addr CoreAddress;
};


struct OBAMP_hello {

u_int8_t MessageID;
union olsr_ip_addr router_id;
u_int8_t HelloSequenceNumber;
union olsr_ip_addr CoreAddress;

};

struct OBAMP_alive {

u_int8_t MessageID;
u_int8_t status;
u_int32_t CoreAddress;
//REMEMBER:Pad to 4 bytes this is a OLSR message

};

#endif /* _OBAMP_OBAMP_H */

