
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

#include "cl_roam.h"
#include "olsr_types.h"
#include "ipcalc.h"
#include "scheduler.h"
#include "olsr.h"
#include "olsr_cookie.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_logging.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/route.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "neighbor_table.h"
#include "olsr.h"
#include "olsr_cfg.h"
#include "interfaces.h"
#include "olsr_protocol.h"
#include "net_olsr.h"
#include "link_set.h"
#include "ipcalc.h"
#include "lq_plugin.h"
#include "olsr_cfg_gen.h"
#include "common/string.h"
#include "olsr_ip_prefix_list.h"
#include "olsr_logging.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PLUGIN_INTERFACE_VERSION 5

static int has_inet_gateway;
static struct olsr_cookie_info *event_timer_cookie1;
static struct olsr_cookie_info *event_timer_cookie2;
static union olsr_ip_addr gw_netmask;

/**
 * Plugin interface version
 * Used by main olsrd to check plugin interface version
 */
int
olsrd_plugin_interface_version(void)
{
  return PLUGIN_INTERFACE_VERSION;
}

static const struct olsrd_plugin_parameters plugin_parameters[] = {
};



typedef struct { 
	struct in_addr ip;
	u_int64_t mac;
	struct in_addr from_node;
	char is_announced;
	float last_seen;
	pthread_t ping_thread;
	pthread_t ping_thread_add;
	struct olsr_cookie_info *arping_timer_cookie;
	char ping_thread_done;
	struct timer_entry *arping_timer;
 } guest_client;



typedef struct {
	guest_client *client;
	struct client_list *list;
} client_list;



void update_routes_now()
{
	//Recalculate our own routing table RIGHT NOW

	  OLSR_INFO(LOG_PLUGINS, "Forcing recalculation of routing-table\n");

    //sets timer to zero


    spf_backoff_timer = NULL;
    ///printf("timer stopped\n");
    //actual calculation
    olsr_calculate_routing_table();
    //printf("updated\n");
}


client_list *list;
struct interface *ifn;


void ping(guest_client *);

void
olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
  *params = plugin_parameters;
  *size = ARRAYSIZE(plugin_parameters);
}






guest_client * get_client_by_ip(struct in_addr ip) {
  if (list!=NULL && list->client!=NULL) {

    if (strcmp(inet_ntoa(list->client->ip), inet_ntoa(ip))==0)
    	return list->client;
    else
    	return get_client_by_ip(ip);
  }
  else
	  return NULL;
}



void
olsr_parser(struct olsr_message *msg, struct interface *in_if __attribute__ ((unused)),
    union olsr_ip_addr *ipaddr, enum duplicate_status status __attribute__ ((unused)))
{
	const uint8_t *curr;
// my MessageType
  if (msg->type != 134) {
	  printf("recieved something else\n");
    return;
  }
  OLSR_INFO(LOG_PLUGINS, "Recieved roaming-messagetype\n");
  curr = msg->payload;
  struct in_addr ip;
  pkt_get_ipaddress(&curr, &ip);


  guest_client * guest;

  guest=get_client_by_ip(ip);



  if (guest!=NULL) {
	  if ((guest->is_announced)!=0) {
		  OLSR_INFO(LOG_PLUGINS, "Having to revoke announcement for %s\n", inet_ntoa(guest->ip));
		  guest->is_announced=0;
		  guest->last_seen=30.0;

		  ip_prefix_list_remove(&olsr_cnf->hna_entries, &(guest->ip), olsr_netmask_to_prefix(&gw_netmask), olsr_cnf->ip_version);
		  char route_command[50];
		  snprintf(route_command, sizeof(route_command), "route del %s dev ath0 metric 0", inet_ntoa(guest->ip));
		  system(route_command);
		  single_hna(&ip, 0);

		  }
	 // else
		//  printf("Not revoking \n");
  }
  //else
	//  printf("Not revoking\n");




  update_routes_now();


}









/**
 * Initialize plugin
 * Called after all parameters are passed
 */
int
olsrd_plugin_init(void)
{


  
  list=(client_list*)malloc( sizeof(client_list) );



  OLSR_INFO(LOG_PLUGINS, "OLSRD automated Client Roaming Plugin\n");


  gw_netmask.v4.s_addr = inet_addr("255.255.255.255");

  has_inet_gateway = 0;


  /* create the cookie */
  event_timer_cookie1 = olsr_alloc_cookie("cl roam: Event1", OLSR_COOKIE_TYPE_TIMER);
  event_timer_cookie2 = olsr_alloc_cookie("cl roam: Event2", OLSR_COOKIE_TYPE_TIMER);

  /* Register the GW check */
  olsr_start_timer(1 * MSEC_PER_SEC, 0, OLSR_TIMER_PERIODIC, &olsr_event1, NULL, event_timer_cookie1);
  olsr_start_timer(10 * MSEC_PER_SEC, 0, OLSR_TIMER_PERIODIC, &olsr_event2, NULL, event_timer_cookie2);


  /* register functions with olsrd */
  // somehow my cool new message.....
  olsr_parser_add_function(&olsr_parser, 134);



  return 1;
}



int ping_thread(void* guest) {

	guest_client * target = (guest_client *) guest;
    target->ping_thread_done=0;

    char ping_command[50];

    snprintf(ping_command, sizeof(ping_command), "arping -I ath0 -w 1 -c 1 -q %s", inet_ntoa(target->ip));
    //printf("%s\n",ping_command);
    int result = system(ping_command);
    target->ping_thread_done=1;
    //printf("ping thread finished\n");
    return result;

}



void ping_thread_infinite(guest_client * target)
{
    char ping_command[50];

    while (1) {
		snprintf(ping_command, sizeof(ping_command), "arping -I ath0 -q -c 10 %s", inet_ntoa(target->ip));
		system(ping_command);
		pthread_testcancel();
    }

}



void ping_infinite(guest_client * target)
{
	int rc;
	pthread_t thread;
	if (target->ping_thread==NULL) {
		rc = pthread_create(&thread, NULL, ping_thread_infinite, (void *) target   );
		if (rc){
		   printf("ERROR; return code from pthread_create() is %d\n", rc);
		   exit(-1);
		}
		target->ping_thread=thread;
		OLSR_INFO(LOG_PLUGINS, "Set up ping-thread for %s\n", inet_ntoa(target->ip));
	}
	else
		OLSR_INFO(LOG_PLUGINS, "Ping-thread for %s already exists!\n", inet_ntoa(target->ip));
}








struct olsr_message msg;

void
check_ping_result(void *foo )
{
	guest_client * host = (guest_client *) foo;

	if (! host->ping_thread_done) {
		OLSR_DEBUG(LOG_PLUGINS, "Ping-thread for %s not finished\n", inet_ntoa(host->ip));

	}
	else {
		olsr_stop_timer(host->arping_timer);
		int ping_res;
		pthread_join(host->ping_thread_add, &ping_res);
		host->arping_timer=NULL;
		host->ping_thread_done=0;
		if(ping_res==0) {
			OLSR_INFO(LOG_PLUGINS, "Adding Route for %s\n", inet_ntoa(host->ip));

			ip_prefix_list_add(&olsr_cnf->hna_entries, &(host->ip), olsr_netmask_to_prefix(&gw_netmask));
			host->is_announced=1;


			char route_command[50];
			snprintf(route_command, sizeof(route_command), "route add %s dev ath0 metric 0", inet_ntoa(host->ip));
			system(route_command);




			spread_host(host);
			//This really big time will be overwritten by the next normal hna-announcement
			single_hna(&host->ip, -1);


			//Append to buffer, will be send at some time. In case it didn't arrive at the old master
			spread_host(host);



		}
		else {
		  host->last_seen+=0.5;
		  check_for_route(host);
	}
	}

}







void check_for_route(guest_client * host)
{
//printf("%f, %i, %x\n",host->last_seen , host->is_announced  ,host->ping_thread_add );
  if (host->last_seen < 5.0 && ! host->is_announced  && host->arping_timer==NULL) {

	  //printf("maybe add something\n");
	  if (host->arping_timer_cookie==NULL)
		  host->arping_timer_cookie = olsr_alloc_cookie("cl roam: Maybe add something", OLSR_COOKIE_TYPE_TIMER);
	  //printf("timer started\n");
	  host->arping_timer = olsr_start_timer(250, 5, OLSR_TIMER_PERIODIC, &check_ping_result, host, host->arping_timer_cookie);
	  int rc = pthread_create(&(host->ping_thread_add), NULL, ping_thread, (void *)host);



  } else if ((host->last_seen > 20.0) &&  host->is_announced) {
      OLSR_INFO(LOG_PLUGINS, "Removing Route for %s\n", inet_ntoa(host->ip));
      ip_prefix_list_remove(&olsr_cnf->hna_entries, &(host->ip), olsr_netmask_to_prefix(&gw_netmask), olsr_cnf->ip_version);
		char route_command[50];
		snprintf(route_command, sizeof(route_command), "route del %s dev ath0 metric 0", inet_ntoa(host->ip));
		system(route_command);
      host->is_announced=0;
}
}

void check_client_list(client_list * clist) {
  if (clist!=NULL && clist->client!=NULL) {

    //ping(clist->client);
	  if(clist->client->is_announced==1 || clist->client->ping_thread!=NULL) {
		  if( ! check_if_associcated(clist->client)) {
			  if(clist->client->is_announced==1) {
				  clist->client->last_seen+=5;
			  }
			  if (clist->client->ping_thread!=NULL) {
				  //printf("attempting to kill ping-thread\n");
				  pthread_detach(clist->client->ping_thread);
				  //pthread_kill(clist->client->ping_thread, 1);
				  //pthread_cancel(clist->client->ping_thread);
				  pthread_cancel(clist->client->ping_thread);
				  //printf("killed ping-thread\n");
				  OLSR_INFO(LOG_PLUGINS, "Killed ping-thread for %s\n", inet_ntoa(clist->client->ip));
				  clist->client->ping_thread=NULL;
			  }
		  }
	  }


    check_for_route(clist->client);
    check_client_list(clist->list);
  }
}



guest_client * get_client_by_mac(client_list * clist, u_int64_t mac) {
  if (clist!=NULL && clist->client!=NULL) {

    if (clist->client->mac == mac)
    	return clist->client;
    else
    	return get_client_by_mac(clist->list, mac);
  }
  else
	  return NULL;
}







int ip_is_in_guest_list(client_list * list, guest_client * host) {
if (list==NULL)
  return 0;
if (list->client==NULL)
  return 0;
else if (inet_lnaof(list->client->ip) == inet_lnaof(host->ip))
  return 1;
else
  return ip_is_in_guest_list(list->list, host);
}


void check_local_leases(client_list * clist){
  check_leases(clist,"/var/dhcp.leases" , 1.0);
}


void check_leases(client_list * clist, char file[], float def_last_seen) {
	  FILE * fp = fopen(file, "r");
	  if(fp == NULL) {
	          printf("failed to open %s\n", file);
	          return EXIT_FAILURE;
	      }
	  char s1[50];
	  char s2[50];
	  char s3[50];
	  long long int one, two, three, four, five, six;

	  while (1) {
		  int parse = fscanf (fp, "%s %llx:%llx:%llx:%llx:%llx:%llx %s %*s %*s\n", s1, &one, &two, &three, &four, &five, &six, s3);
	    if (parse==EOF)
	      break;
	    if(parse==8) {
	    guest_client* user;
	    //printf ("String 3 = %s\n", s3);
	    user = (guest_client*)malloc( sizeof(guest_client) );
	    inet_aton(s3, &(user->ip));
	    user->mac= six | five<<8 | four<<16 | three<<24 | two<<32 | one<<40;
	    user->last_seen=def_last_seen;
	    user->is_announced=0;
	    user->ping_thread=NULL;
	    //printf("last seen on Add %f\n",user->last_seen);
	    add_client_to_list(clist, user);

	}
	  }
	  fclose(fp);


	}




void check_remote_leases(client_list * clist){
	  FILE * fp = fopen("/tmp/otherclient", "r");
	  if(fp == NULL) {
	          printf("failed to open %s\n", "/tmp/otherclient");
	          return EXIT_FAILURE;
	      }
	  FILE * my_leases =fopen ("/var/dhcp.leases", "a");
	  if(my_leases == NULL) {
	          printf("failed to open %s\n","/var/dhcp.leases");
	          return EXIT_FAILURE;
	      }
	  char s1[50];
	  char s2[50];
	  char s3[50];
	  long long int one, two, three, four, five, six;

	  while (1) {
		  int parse = fscanf (fp, "%s %llx:%llx:%llx:%llx:%llx:%llx %s %*s %*s\n", s1, &one, &two, &three, &four, &five, &six, s3);
	    if (parse==EOF)
	      break;
	    if(parse==8) {
	    guest_client* user;
	    //printf ("String 3 = %s\n", s3);
	    user = (guest_client*)malloc( sizeof(guest_client) );
	    inet_aton(s3, &(user->ip));
	    user->mac= six | five<<8 | four<<16 | three<<24 | two<<32 | one<<40;
	    user->last_seen=30.0;
	    user->is_announced=0;
	    user->ping_thread=NULL;
	    //printf("last seen on Add %f\n",user->last_seen);

	    if (!(ip_is_in_guest_list(clist,  user))) {

	    	char leases[80];

	    	// Why the fuck cant I do it in one step!?
	    	 snprintf(leases, sizeof(leases), "%s %llx:%llx:%llx:%llx:%llx:%llx", s1, one, two, three, four, five, six);
	    	 snprintf(leases, sizeof(leases), "%s %s * *\n", leases, s3);
	    	//printf(leases);
	    	fprintf(my_leases, leases);
	    }


	    add_client_to_list(clist, user);

	}
	  }
	  fclose(fp);
	  fclose(my_leases);


	}





int check_if_associcated(guest_client *client)
{
  FILE * fp = fopen("/proc/net/madwifi/ath0/associated_sta", "r");
  if(fp == NULL) {
          printf("failed to open %s\n", "/proc/net/madwifi/ath0/associated_sta");
          return EXIT_FAILURE;
      }
  //FILE * fp = fopen("/home/raphael/tmp/leases", "r");
  char s1[50];
  char s2[50];
  char s3[50];
  int rssi;
  float last_rx;
  int parse;
  uint64_t mac;
  long long int one, two, three, four, five, six;

  while (1) {
    parse = fscanf (fp, "macaddr: <%llx:%llx:%llx:%llx:%llx:%llx>\n", &one, &two, &three, &four, &five, &six);
    if (parse==EOF || parse!=6)
      break;
    mac = six | five<<8 | four<<16 | three<<24 | two<<32 | one<<40;
    //printf ("am at %llx\n", mac);
    if (mac==client->mac) {
      //printf("found it!\n");
      fclose(fp);
      return 1;
    }
    parse = fscanf (fp, " rssi %s\n", s2);
    if (parse==EOF)
          break;
    parse = fscanf (fp, " last_rx %f\n", &last_rx);
    if (parse==EOF)
          break;
  }

  fclose(fp);

  return 0;

}








// Will be handy to identify when a client roamed to us. Not used yet.
void check_associations(client_list * clist){
  FILE * fp = fopen("/proc/net/madwifi/ath0/associated_sta", "r");
  if(fp == NULL) {
           printf("failed to open %s\n", "/proc/net/madwifi/ath0/associated_sta");
           return EXIT_FAILURE;
       }
  //FILE * fp = fopen("/home/raphael/tmp/leases", "r");
  char s1[50];
  char s2[50];
  char s3[50];
  int rssi;
  float last_rx;
  int parse;

  long long int one, two, three, four, five, six;

  while (1) {
    parse = fscanf (fp, "macaddr: <%llx:%llx:%llx:%llx:%llx:%llx>\n", &one, &two, &three, &four, &five, &six);
    if (parse==EOF || parse!=6)
      break;
    uint64_t mac = six | five<<8 | four<<16 | three<<24 | two<<32 | one<<40;
    //printf ("macaddr: %llx\n", mac);
    parse = fscanf (fp, " rssi %s\n", s2);
    if (parse==EOF)
          break;
    //printf ("rssi = %s\n", s2);
    parse = fscanf (fp, " last_rx %f\n", &last_rx);
    if (parse==EOF)
          break;
    //printf ("last_rx = %f\n", last_rx);
    guest_client* node = get_client_by_mac(clist, mac);
    if (node!=NULL) {
    	//printf("Sichtung!\n");
    	node->last_seen=last_rx;
    	if(node->ping_thread==NULL) {
    		ping_infinite(node);
    		    	}
    }

  }
  fclose(fp);


}









void add_client_to_list(client_list * clist, guest_client * host) {
  if (ip_is_in_guest_list(clist,  host)) {
    free(host);
  }
  else {
    if (clist->client!=NULL) {
      client_list * this_one;
      this_one = (client_list *)malloc( sizeof(client_list) );
      this_one->client = host;
      this_one->list=clist->list;
      clist->list=this_one;
      OLSR_INFO(LOG_PLUGINS, "Keeping track of %s\n", inet_ntoa(host->ip));
    } else {
      clist->client=host;
      OLSR_INFO(LOG_PLUGINS, "Keeping track of %s\n", inet_ntoa(host->ip));
    }
    ping_infinite(host);
  }
}





void check_for_new_clients(client_list * clist) {
    check_local_leases(clist);
    check_associations(clist);
}



void check_neighbour_host(void* neighbour) {
	struct nbr_entry *nbr = (struct nbr_entry*) neighbour;
	union olsr_ip_addr foobar = nbr->nbr_addr;

	char wget_command[70];


	snprintf(wget_command, sizeof(wget_command), "wget -q -O /tmp/otherclient http://%s/dhcp.leases", inet_ntoa(foobar.v4));
	if (system(wget_command)==0){

	check_remote_leases(list);
	}
}










void
spread_host(guest_client * host) {
  struct interface *ifp;
  struct ip_prefix_entry *h;
  uint8_t msg_buffer[MAXMESSAGESIZE - OLSR_HEADERSIZE] __attribute__ ((aligned));
  uint8_t *curr = msg_buffer;
  uint8_t *length_field, *last;
  bool sendHNA = false;

  OLSR_INFO(LOG_PACKET_CREATION, "Building HNA\n-------------------\n");

  // My message Type 134:
  pkt_put_u8(&curr, 134);
  // hna-validity
  pkt_put_reltime(&curr, 20000);

  length_field = curr;
  pkt_put_u16(&curr, 0); /* put in real messagesize later */

  pkt_put_ipaddress(&curr, &olsr_cnf->router_id);

  // TTL
  pkt_put_u8(&curr, 2);
  pkt_put_u8(&curr, 0);
  pkt_put_u16(&curr, get_msg_seqno());

  last = msg_buffer + sizeof(msg_buffer) - olsr_cnf->ipsize;




// Actual message
	pkt_put_ipaddress(&curr, &(host->ip));



	//Put in Message size
  pkt_put_u16(&length_field, curr - msg_buffer);

  OLSR_FOR_ALL_INTERFACES(ifp) {
	    if (net_outbuffer_bytes_left(ifp) < curr - msg_buffer) {
	      net_output(ifp);
	      set_buffer_timer(ifp);
	    }
	// buffer gets pushed out in single_hna, or at flush-time
    net_outbuffer_push(ifp, msg_buffer, curr - msg_buffer);
  } OLSR_FOR_ALL_INTERFACES_END(ifp)


}









// sends packet immedeately!
void
single_hna(struct in_addr * ip, uint32_t time) {
	  //printf("single hna %x with time %i\n",*ip, time);




  struct interface *ifp;
  struct ip_prefix_entry *h;
  uint8_t msg_buffer[MAXMESSAGESIZE - OLSR_HEADERSIZE] __attribute__ ((aligned));
  uint8_t *curr = msg_buffer;
  uint8_t *length_field, *last;
  bool sendHNA = false;

  OLSR_INFO(LOG_PACKET_CREATION, "Building HNA\n-------------------\n");

  // My message Type 134:
  pkt_put_u8(&curr, HNA_MESSAGE);
  // hna-validity
  pkt_put_reltime(&curr, time);

  length_field = curr;
  pkt_put_u16(&curr, 0); /* put in real messagesize later */

  pkt_put_ipaddress(&curr, &olsr_cnf->router_id);

  // TTL
  pkt_put_u8(&curr, 2);
  pkt_put_u8(&curr, 0);
  pkt_put_u16(&curr, get_msg_seqno());

  last = msg_buffer + sizeof(msg_buffer) - olsr_cnf->ipsize;



  struct in_addr subnet;
  	  inet_aton("255.255.255.255", &subnet);
	pkt_put_ipaddress(&curr, ip);
	pkt_put_ipaddress(&curr, &subnet);


  pkt_put_u16(&length_field, curr - msg_buffer);




  OLSR_FOR_ALL_INTERFACES(ifp) {
    net_output(ifp);
    net_outbuffer_push(ifp, msg_buffer, curr - msg_buffer);
    net_output(ifp);
    set_buffer_timer(ifp);
  } OLSR_FOR_ALL_INTERFACES_END(ifp)
}










void
olsr_event1(void *foo )
{
	check_for_new_clients(list);
    check_client_list(list);
}


void
olsr_event2(void *foo )
{
	struct nbr_entry *nbr;

    OLSR_FOR_ALL_NBR_ENTRIES(nbr) {
    	int rc;
    	pthread_t thread;
        rc = pthread_create(&thread, NULL, check_neighbour_host, (void *) nbr   );
        if (rc){
           printf("ERROR; return code from pthread_create() is %d\n", rc);
           exit(-1);
		}

        }OLSR_FOR_ALL_NBR_ENTRIES_END();
}




