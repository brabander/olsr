/*
 * Copyright (c) 2005, Bruno Randolf <bruno.randolf@4g-systems.biz>
 * Copyright (c) 2004, Andreas Tønnesen(andreto-at-olsr.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * * Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * * Neither the name of the UniK olsr daemon nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software 
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* $Id: nameservice.c,v 1.1 2005/01/16 13:06:00 kattemat Exp $ */

/*
 * Dynamic linked library for UniK OLSRd
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "nameservice.h"
#include "olsrd_copy.h"


/* The database - (using hashing) */
struct name_entry list[HASHSIZE];

/* send buffer: size of IPv6 message + the maximum size of the name */
static char buffer[sizeof(struct olsrmsg6) + MAX_NAME];

olsr_u8_t name_table_changed=0;

char* my_name = "";
olsr_u8_t my_name_len = 0;
char* my_filename = "/var/run/hosts_olsr";


/**
 * Do initialization
 */
int
olsr_plugin_init()
{
	int i;
	/* Init list */
	for(i = 0; i < HASHSIZE; i++)
	{
		list[i].next = &list[i];
		list[i].prev = &list[i];
	}
	
	/* Register functions with olsrd */
	olsr_parser_add_function(&olsr_parser, PARSER_TYPE, 1);
	olsr_register_timeout_function(&olsr_timeout);
	olsr_register_scheduler_event(&olsr_event, NULL, EMISSION_INTERVAL, 0, NULL);

	return 1;
}


/**
 * Called at unload
 */
void
olsr_plugin_exit()
{
	int i;
	struct name_entry *tmp_list;
	struct name_entry *entry_to_delete;
	
	/* free list entries */
	for(i = 0; i < HASHSIZE; i++)
	{
		tmp_list = list[i].next;
		while(tmp_list != &list[i])
		{
			entry_to_delete = tmp_list;
			tmp_list = tmp_list->next;
			entry_to_delete->prev->next = entry_to_delete->next;
			entry_to_delete->next->prev = entry_to_delete->prev;
			free(entry_to_delete->name);
			free(entry_to_delete);
		}
	}
}


/**
 * A timeout function called every time
 * the scheduler is polled: time out old list entries
 */
void
olsr_timeout()
{
	struct name_entry *tmp_list;
	struct name_entry *entry_to_delete;
	int index;

	for(index=0;index<HASHSIZE;index++)
	{
		tmp_list = list[index].next;
		while(tmp_list != &list[index])
		{
			/*Check if the entry is timed out*/
			if(olsr_timed_out(&tmp_list->timer))
			{
				entry_to_delete = tmp_list;
				tmp_list = tmp_list->next;
				olsr_printf(2, "NAME PLUGIN: %s timed out.. deleting\n", 
					olsr_ip_to_string(&entry_to_delete->originator));
				/* Dequeue */
				entry_to_delete->prev->next = entry_to_delete->next;
				entry_to_delete->next->prev = entry_to_delete->prev;

				/* Delete */
				free(entry_to_delete->name);
				free(entry_to_delete);
				
				name_table_changed = 1;
			}
			else
				tmp_list = tmp_list->next;
		}
	}
	return;
}


/**
 * Scheduled event: generate and send NAME packet
 */
void
olsr_event(void *foo)
{
	union olsr_message *message = (union olsr_message*)buffer;
	struct interface *ifn;
	int namesize;
  
	olsr_printf(3, "NAME PLUGIN: Generating packet - ");

	/* looping trough interfaces */
	for (ifn = ifs; ifn ; ifn = ifn->int_next) 
	{
		olsr_printf(3, "[%s]  ", ifn->int_name);
		/* Fill message */
		if(ipversion == AF_INET)
		{
			/* IPv4 */
			message->v4.olsr_msgtype = MESSAGE_TYPE;
			message->v4.olsr_vtime = double_to_me(NAME_VALID_TIME);
			memcpy(&message->v4.originator, main_addr, ipsize);
			message->v4.ttl = MAX_TTL;
			message->v4.hopcnt = 0;
			message->v4.seqno = htons(get_msg_seqno());
		
			namesize = get_namemsg(&message->v4.msg);
			namesize = namesize + sizeof(struct olsrmsg);
		
			message->v4.olsr_msgsize = htons(namesize);
		}
		else
		{
			/* IPv6 */
			message->v6.olsr_msgtype = MESSAGE_TYPE;
			message->v6.olsr_vtime = double_to_me(NAME_VALID_TIME);
			memcpy(&message->v6.originator, main_addr, ipsize);
			message->v6.ttl = MAX_TTL;
			message->v6.hopcnt = 0;
			message->v6.seqno = htons(get_msg_seqno());
	  
			namesize = get_namemsg(&message->v6.msg);
			namesize = namesize + sizeof(struct olsrmsg6);
	  
			message->v6.olsr_msgsize = htons(namesize);
		}
	
		if(net_outbuffer_push(ifn, (olsr_u8_t *)message, namesize) != namesize ) {
			/* Send data and try again */
			net_output(ifn);
			if(net_outbuffer_push(ifn, (olsr_u8_t *)message, namesize) != namesize ) {
				olsr_printf(1, "NAME PLUGIN: could not send on interface: %s\n", ifn->int_name);
			}
		}
	}
	olsr_printf(3, "\n");
	write_name_table();
	return;
}


/**
 * Parse name olsr message of NAME type
 */
void
olsr_parser(union olsr_message *m, struct interface *in_if, union olsr_ip_addr *in_addr)
{
	struct  namemsg *message;
	union olsr_ip_addr originator;
	double vtime;

	/* Fetch the originator of the messsage */
	memcpy(&originator, &m->v4.originator, ipsize);
		
	/* Fetch the message based on IP version */
	if(ipversion == AF_INET) {
		message = &m->v4.msg;
		vtime = me_to_double(m->v4.olsr_vtime);
	}
	else {
		message = &m->v6.msg;
		vtime = me_to_double(m->v6.olsr_vtime);
	}

	/* Check if message originated from this node. 
	If so - back off */
	if(memcmp(&originator, main_addr, ipsize) == 0)
		return;

	/* Check that the neighbor this message was received from is symmetric. 
	If not - back off*/
	if(check_neighbor_link(in_addr) != SYM_LINK) {
		olsr_printf(3, "NAME PLUGIN: Received msg from NON SYM neighbor %s\n", olsr_ip_to_string(in_addr));
		return;
	}

	/* Check if this message has been processed before
	* Remeber that this also registeres the message as
	* processed if nessecary
	*/
	if(!check_dup_proc(&originator, ntohs(m->v4.seqno))) {
		/* If so - do not process */
		goto forward;
	}

	/* Process */
	olsr_printf(3, "NAME PLUGIN: Processing NAME info from %s seqno: %d\n",
		olsr_ip_to_string(&originator),
		ntohs(m->v4.seqno));

	update_name_entry(&originator, message, vtime);


forward:
	/* Forward the message if nessecary
	* default_fwd does all the work for us! */
	default_fwd(m, &originator, ntohs(m->v4.seqno), in_if, in_addr);
}


/**
 * Get a name message. This fills the namemsg struct
 * AND appends the name after that! 
 *
 * It assumed that there is enough space in the buffer to do this!
 *
 * Returns: the length of the name that was appended
 */
int
get_namemsg(struct namemsg *msg)
{
	msg->name_len = my_name_len;
	char* txt = (char*)(msg + sizeof(struct namemsg));
	strncpy(txt, my_name, MAX_NAME); 
	return my_name_len;
}


/**
 * Read a name message and update name_entry if necessary
 *
 * Return: 1 if entry was changed
 */
int
read_namemsg(struct namemsg *msg, struct name_entry *to)
{
	char* txt = (char*)(msg + sizeof(struct namemsg));
	if (to->name == NULL || strncmp(to->name, txt, MAX_NAME) != 0) {
		if (to->name != NULL) {
			free( to->name );
		}
		to->name = olsr_malloc(msg->name_len, "new name_entry name");
		strncpy(to->name, txt, msg->name_len);
		return 1;
	}
	return 0;	
}


/**
 * Update or register a new name entry
 */
int
update_name_entry(union olsr_ip_addr *originator, struct namemsg *message, double vtime)
{
	int hash;
	struct name_entry *entry;
	
	hash = olsr_hashing(originator);
	
	/* Check for the entry */
	for(entry = list[hash].next; entry != &list[hash]; entry = entry->next)
	{
		if(memcmp(originator, &entry->originator, ipsize) == 0) {
			if (read_namemsg( message, entry )) {
				name_table_changed = 1;
			}
			olsr_get_timestamp(vtime * 1000, &entry->timer);
			return 0;
		}
	}

	entry = olsr_malloc(sizeof(struct name_entry), "new name_entry");
	entry->name = NULL;
	memcpy(&entry->originator, originator, ipsize);
	olsr_get_timestamp(vtime * 1000, &entry->timer);
	
	read_namemsg( message, entry );

	olsr_printf(2, "NAME PLUGIN: New entry %s: %s\n", olsr_ip_to_string(originator), entry->name);
		
	/* Queue */
	entry->next = list[hash].next->prev;
	entry->prev = &list[hash];
	list[hash].next->prev = entry;
	list[hash].next = entry;

	name_table_changed = 1;
	return 1;
}


/**
 * write names to a file in /etc/hosts compatible format
 */
void
write_name_table()
{
	int hash;
	struct name_entry *entry;
	char buf[MAX_NAME+17];
	FILE* hosts;

	olsr_printf(2, "NAME PLUGIN: writing hosts file\n");
	
	if(!name_table_changed)
		return;
	      
	hosts = fopen( my_filename, "w" );
	
	fprintf(hosts, "# this /etc/hosts file is overwritten regularly by olsrd\n");
	fprintf(hosts, "# do not edit\n");
	// add own IP and name
	fprintf(hosts, "%s\t%s\n", olsr_ip_to_string(main_addr), my_name );

	for(hash = 0; hash < HASHSIZE; hash++) 
	{
		for(entry = list[hash].next; entry != &list[hash]; entry = entry->next) 
		{
			sprintf(buf, "%s\t%s\n", olsr_ip_to_string(&entry->originator), entry->name);
			fprintf(hosts, "%s", buf);
		}
	}
	
	fclose(hosts);
	name_table_changed = 0;
}
