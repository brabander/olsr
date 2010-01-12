
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
static struct olsr_cookie_info *event_timer_cookie;
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
	char pingable;
	char is_announced;
 } guest_client;



typedef struct {
	guest_client *client;
	struct client_list *list;
} client_list;


guest_client foobar;

client_list *list;
struct interface *ifn;



void ping(guest_client *);

void
olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
  *params = plugin_parameters;
  *size = ARRAYSIZE(plugin_parameters);
}



void *PrintHello(void *threadid)
{
   long tid;
   tid = (long)threadid;
   printf("Hello World! It's me, thread #%ld!\n", tid);
   pthread_exit(NULL);
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
  event_timer_cookie = olsr_alloc_cookie("cl roam: Event", OLSR_COOKIE_TYPE_TIMER);

  /* Register the GW check */
  olsr_start_timer(3 * MSEC_PER_SEC, 0, OLSR_TIMER_PERIODIC, &olsr_event, NULL, event_timer_cookie);
  //system("echo \"1\" ");
  //pthread_create(&ping_thread, NULL, &do_ping, NULL);
  //system("echo \"2\" ");
  //CreateThread(NULL, 0, do_ping, NULL, 0, &ThreadId);
  //if (pthread_create(&ping_thread, NULL, do_ping, NULL) != 0) {
   // OLSR_WARN(LOG_PLUGINS, "pthread_create() error");
   // return 0;
  //}







  return 1;
}



void ping_thread(guest_client * target)
{

    char ping_command[50];


    //snprintf(ping_command, sizeof(ping_command), "ping -I ath0 -c 1 -q %s", inet_ntoa(target->ip));
    snprintf(ping_command, sizeof(ping_command), "arping -I ath0 -w 1 -c 1 -q %s", inet_ntoa(target->ip));
    if (system(ping_command) == 0) {
      system("echo \"ping erfolgreich\" ");
      OLSR_DEBUG(LOG_PLUGINS, "\nDo ping on %s ... ok\n", inet_ntoa(target->ip));
      target->pingable=1;
    }
    else
    {
      system("echo \"ping erfolglos\" ");
      OLSR_DEBUG(LOG_PLUGINS, "\nDo ping on %s ... failed\n", inet_ntoa(target->ip));
      target->pingable=0;
    }

}



void ping_thread_infinite(guest_client * target)
{

	while (1) {
		char ping_command[50];


		//snprintf(ping_command, sizeof(ping_command), "ping -I ath0 -c 1 -q %s", inet_ntoa(target->ip));
		snprintf(ping_command, sizeof(ping_command), "arping -I ath0 -w 1 -c 1 -q %s", inet_ntoa(target->ip));
		if (system(ping_command) == 0) {
		  system("echo \"ping erfolgreich\" ");
		  OLSR_DEBUG(LOG_PLUGINS, "\nDo ping on %s ... ok\n", inet_ntoa(target->ip));
		  target->pingable=1;
		}
		else
		{
		  system("echo \"ping erfolglos\" ");
		  OLSR_DEBUG(LOG_PLUGINS, "\nDo ping on %s ... failed\n", inet_ntoa(target->ip));
		  target->pingable=0;
		}
		sleep (2);
	}
}



void ping(guest_client * target)
{
	int rc;
	pthread_t thread;
    rc = pthread_create(&thread, NULL, ping_thread, (void *) target   );
    if (rc){
       printf("ERROR; return code from pthread_create() is %d\n", rc);
       exit(-1);
    }
}

void ping_infinite(guest_client * target)
{
	int rc;
	pthread_t thread;
    rc = pthread_create(&thread, NULL, ping_thread_infinite, (void *) target   );
    if (rc){
       printf("ERROR; return code from pthread_create() is %d\n", rc);
       exit(-1);
    }
}








struct olsr_message msg;







void check_for_route(guest_client * host)
{
  if (host->pingable && ! host->is_announced) {
      OLSR_DEBUG(LOG_PLUGINS, "Adding Route\n");
      system("echo \"Setze route\" ");
      ip_prefix_list_add(&olsr_cnf->hna_entries, &(host->ip), olsr_netmask_to_prefix(&gw_netmask));
      host->is_announced=1;
  } else if ((! host->pingable) &&  host->is_announced) {
      OLSR_DEBUG(LOG_PLUGINS, "Removing Route\n");
      system("echo \"Entferne route\" ");
      ip_prefix_list_remove(&olsr_cnf->hna_entries, &(host->ip), olsr_netmask_to_prefix(&gw_netmask), olsr_cnf->ip_version);
      host->is_announced=0;
}
}

void check_client_list(client_list * clist) {
  if (clist!=NULL && clist->client!=NULL) {

    //ping(clist->client);
    check_for_route(clist->client);


    check_client_list(clist->list);
  }
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


void check_leases(client_list * clist){
  FILE * fp = fopen("/var/dhcp.leases", "r");
  //FILE * fp = fopen("/home/raphael/tmp/leases", "r");
  char s1[50];
  char s2[50];
  char s3[50];
  char s4[50];
  char s5[50];


  while (1) { 
    int parse = fscanf (fp, "%s %s %s %s %s", s1, s2, s3, s4, s5);
    if (parse==EOF)
      break;
    if(parse==5) {
    guest_client* user;
    //printf ("String 3 = %s\n", s3);
    user = (guest_client*)malloc( sizeof(guest_client) );
    inet_aton(s3, &(user->ip));
    add_client_to_list(clist, user);

}
  }
  fclose(fp);
  

}




void check_remote_leases(client_list * clist){
  FILE * fp = fopen("/tmp/otherclient", "r");
  //FILE * fp = fopen("/home/raphael/tmp/leases", "r");
  char s1[50];
  char s2[50];
  char s3[50];
  char s4[50];
  char s5[50];


  while (1) {
    int parse = fscanf (fp, "%s %s %s %s %s", s1, s2, s3, s4, s5);
    if (parse==EOF)
      break;
    if(parse==5) {
    guest_client* user;
    //printf ("String 3 = %s\n", s3);
    user = (guest_client*)malloc( sizeof(guest_client) );
    inet_aton(s3, &(user->ip));
    add_client_to_list(clist, user);

}
  }
  fclose(fp);


}










// Will be handy to identify when a client roamed to us. Not used yet.
void check_associations(client_list * clist){
  FILE * fp = fopen("/proc/net/madwifi/ath0/associated_sta", "r");
  //FILE * fp = fopen("/home/raphael/tmp/leases", "r");
  char s1[50];
  char s2[50];
  char s3[50];
  char s4[50];
  char s5[50];
  int rssi;
  float last_rx;
  int parse;

  while (1) {
    parse = fscanf (fp, "macaddr: %s\n", s1);
    if (parse==EOF)
      break;
    printf ("macaddr: %s\n", s1);
    parse = fscanf (fp, " rssi %s\n", s2);
    if (parse==EOF)
          break;
    printf ("rssi = %s\n", s2);
    parse = fscanf (fp, " last_rx %s\n", s3);
    if (parse==EOF)
          break;
    printf ("last_rx = %s\n", s3);

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
      printf("added something\n");
    } else {
      clist->client=host;
      printf("added something\n");
    }
    ping_infinite(host);
  }
}





void check_for_new_clients(client_list * clist) {
    check_leases(clist);
    check_associations(clist);
}





void
olsr_event(void *foo __attribute__ ((unused)))
{
	struct nbr_entry *nbr;
	check_for_new_clients(list);
    check_client_list(list);





    OLSR_FOR_ALL_NBR_ENTRIES(nbr) {
    	union olsr_ip_addr foobar = nbr->nbr_addr;

		char wget_command[70];


		snprintf(wget_command, sizeof(wget_command), "wget -O /tmp/otherclient http://%s/dhcp.leases", inet_ntoa(foobar.v4));

		system(wget_command);

		check_remote_leases(list);

        }OLSR_FOR_ALL_NBR_ENTRIES_END();









}







