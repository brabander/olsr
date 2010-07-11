
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


/* System includes */
#include <stddef.h>             /* NULL */
#include <sys/types.h>          /* ssize_t */
#include <string.h>             /* strerror() */
#include <stdarg.h>             /* va_list, va_start, va_end */
#include <errno.h>              /* errno */
#include <assert.h>             /* assert() */
#include <linux/if_ether.h>     /* ETH_P_IP */
#include <linux/if_packet.h>    /* struct sockaddr_ll, PACKET_MULTICAST */
#include <signal.h>             /* sigset_t, sigfillset(), sigdelset(), SIGINT */
#include <netinet/ip.h>         /* struct ip */
#include <netinet/udp.h>        /* struct udphdr */
#include <unistd.h>             /* close() */
#include <stdlib.h>             /* free() */

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <fcntl.h>              /* fcntl() */

/* OLSRD includes */
#include "plugin_util.h"        /* set_plugin_int */
#include "defs.h"               /* olsr_cnf, //OLSR_PRINTF */
#include "ipcalc.h"
#include "olsr.h"               /* //OLSR_PRINTF */
#include "mid_set.h"            /* mid_lookup_main_addr() */
#include "link_set.h"           /* get_best_link_to_neighbor() */
#include "net_olsr.h"           /* ipequal */
#include "olsr_logging.h"

/* plugin includes */
#include "obamp.h"
#include "common/list.h"


#define OLSR_FOR_ALL_OBAMPNODE_ENTRIES(n, iterator) list_for_each_element_safe(&ListOfObampNodes, n, list, iterator.loop, iterator.safe)
#define OLSR_FOR_ALL_OBAMPSNIFF_ENTRIES(s, iterator) list_for_each_element_safe(&ListOfObampSniffingIf, s, list, iterator.loop, iterator.safe)

struct ObampNodeState *myState;        //Internal state of the OBAMP node

/*
List of all other OBAMP nodes
if there is a mesh link the isMesh flag is set to 1
if the link is used in the distribution tree the isTree flag is set to 1
*/
struct list_entity ListOfObampNodes;

//List of Non OLSR Interfaces to capture and send multicast traffic to
struct list_entity ListOfObampSniffingIf;

//udp socket used for OBAMP signalling
int sdudp = -1;

/*
When Outer Tree Create is triggered, a OBAMP node uses this function to
determine the closer OBAMP node that has a TreeLink
*/
static struct ObampNode *
select_tree_anchor(void)
{

  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct ObampNode *best;
  struct list_iterator iterator;
  struct rt_entry *rt;                 //"rt->rt_best->rtp_metric.cost" is the value you are looking for, a 32 bit

  unsigned int mincost = 15;

  best = NULL;

  if (list_is_empty(&ListOfObampNodes) == 0) {     //if the list is NOT empty

    //Scroll the list
    OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
      if (tmp->status == 1) {
        rt = olsr_lookup_routing_table(&tmp->neighbor_ip_addr);

        if (rt == NULL) {       //route is not present yet
          OLSR_DEBUG(LOG_PLUGINS, "No route present to this anchor");
          continue;
        }
        //update best neighbor
        if ((rt->rt_best->rtp_metric.cost / 65536) < mincost)
          best = tmp;
      }
    }
    return best;

  } else {
    OLSR_DEBUG(LOG_PLUGINS, "List empty can't create Overlay Mesh");
  }
  return NULL;

}

//Creates a OBAMP_DATA message and sends it to the specified destination
static int
SendOBAMPData(struct in_addr *addr, unsigned char *ipPacket, int nBytes)
{

  struct sockaddr_in si_other;
  struct OBAMP_data_message4 data_msg;

  memset(&data_msg, 0, sizeof(data_msg));
  data_msg.MessageID = OBAMP_DATA;
  data_msg.router_id = (u_int32_t) myState->myipaddr.v4.s_addr;
  data_msg.last_hop = (u_int32_t) myState->myipaddr.v4.s_addr;

  data_msg.CoreAddress = (u_int32_t) myState->CoreAddress.v4.s_addr;

  data_msg.SequenceNumber = myState->DataSequenceNumber;
  myState->DataSequenceNumber++;

  if (nBytes >= 1471) {
    OLSR_DEBUG(LOG_PLUGINS, "PACKET DROPPED: %d bytes are too much",nBytes);
    return 1;
  }

  memcpy(data_msg.data, ipPacket, nBytes);

  data_msg.datalen = nBytes;

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(OBAMP_SIGNALLING_PORT);
  si_other.sin_addr = *addr;
  //sendto(sdudp, data_msg, sizeof(struct OBAMP_data_message), 0, (struct sockaddr *)&si_other, sizeof(si_other));
  //TODO: this 17 magic number is okay only for IPv4, we do not worry about this now
  sendto(sdudp, &data_msg, 17+data_msg.datalen, 0, (struct sockaddr *)&si_other, sizeof(si_other));
  return 0;

}

static int
CreateCaptureSocket(const char *ifName)
{
  int ifIndex = if_nametoindex(ifName);
  struct packet_mreq mreq;
  struct ifreq req;
  struct sockaddr_ll bindTo;
  int skfd = 0;

  /* Open cooked IP packet socket */
  if (olsr_cnf->ip_version == AF_INET) {
    skfd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
  } else {
    skfd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IPV6));
  }
  if (skfd < 0) {
    OLSR_DEBUG(LOG_PLUGINS, "socket(PF_PACKET) error");
    return -1;
  }

  /* Set interface to promiscuous mode */
  memset(&mreq, 0, sizeof(struct packet_mreq));
  mreq.mr_ifindex = ifIndex;
  mreq.mr_type = PACKET_MR_PROMISC;
  if (setsockopt(skfd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    OLSR_DEBUG(LOG_PLUGINS, "setsockopt(PACKET_MR_PROMISC) error");
    close(skfd);
    return -1;
  }

  /* Get hardware (MAC) address */
  memset(&req, 0, sizeof(struct ifreq));
  strncpy(req.ifr_name, ifName, IFNAMSIZ - 1);
  req.ifr_name[IFNAMSIZ - 1] = '\0';    /* Ensures null termination */
  if (ioctl(skfd, SIOCGIFHWADDR, &req) < 0) {
    OLSR_DEBUG(LOG_PLUGINS, "error retrieving MAC address");
    close(skfd);
    return -1;
  }

  /* Bind the socket to the specified interface */
  memset(&bindTo, 0, sizeof(bindTo));
  bindTo.sll_family = AF_PACKET;
  if (olsr_cnf->ip_version == AF_INET) {
    bindTo.sll_protocol = htons(ETH_P_IP);
  } else {
    bindTo.sll_protocol = htons(ETH_P_IPV6);
  }
  bindTo.sll_ifindex = ifIndex;
  memcpy(bindTo.sll_addr, req.ifr_hwaddr.sa_data, IFHWADDRLEN);
  bindTo.sll_halen = IFHWADDRLEN;

  if (bind(skfd, (struct sockaddr *)&bindTo, sizeof(bindTo)) < 0) {
    OLSR_DEBUG(LOG_PLUGINS, "bind() error");
    close(skfd);
    return -1;
  }

  /* Set socket to blocking operation */
  if (fcntl(skfd, F_SETFL, fcntl(skfd, F_GETFL, 0) & ~O_NONBLOCK) < 0) {
    OLSR_DEBUG(LOG_PLUGINS, "fcntl() error");
    close(skfd);
    return -1;
  }

  add_olsr_socket(skfd, &EncapFlowInObamp, NULL, NULL, SP_PR_READ);

  return skfd;
}                               /* CreateCaptureSocket */


static int
CreateObampSniffingInterfaces(void)
{

  struct ObampSniffingIf *tmp;
  struct list_iterator iterator;
  OLSR_DEBUG(LOG_PLUGINS, "CreateObampSniffingInterfaces");

  if (list_is_empty(&ListOfObampSniffingIf) == 0) {        //if the list is NOT empty
    OLSR_DEBUG(LOG_PLUGINS, "adding interfaces");

    OLSR_FOR_ALL_OBAMPSNIFF_ENTRIES(tmp, iterator) {
      tmp->skd = CreateCaptureSocket(tmp->ifName);
    }
  } else
    OLSR_DEBUG(LOG_PLUGINS, "List of sniffin interfaces was empty");


  return 0;
}


static int
IsMulticast(union olsr_ip_addr *ipAddress)
{
  assert(ipAddress != NULL);

  return (ntohl(ipAddress->v4.s_addr) & 0xF0000000) == 0xE0000000;
}



static void
activate_tree_link(struct OBAMP_tree_link_ack *ack)
{
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *tmp;
  struct list_iterator iterator;

  if (memcmp(&myState->CoreAddress.v4, &ack->CoreAddress.v4, sizeof(struct in_addr)) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Discarding message with no coherent core address");
    return;
  }

  if (ack->SequenceNumber != myState->tree_req_sn - 1) {

    OLSR_DEBUG(LOG_PLUGINS, "ACK DISCARDED WRONG SEQ NUMBER");
    return;
  } else {

    OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
      if (tmp->neighbor_ip_addr.v4.s_addr == ack->router_id.v4.s_addr) {

        tmp->isTree = 1;
        myState->TreeHeartBeat = TREE_HEARTBEAT;
        OLSR_DEBUG(LOG_PLUGINS, "Tree link to %s activated", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
        return;

      }
    }
  }

}



/*
When we select a other OBAMP node as a overlay neighbor, we start to send HELLOs to him
to inform we are using the unicast path from us to him  as a overlay mesh link
*/
static void
obamp_hello(struct in_addr *addr)
{

  struct OBAMP_hello hello;
  struct sockaddr_in si_other;

  memset(&hello, 0, sizeof(hello));
  hello.MessageID = OBAMP_HELLO;
  //TODO: refresh IP address
  hello.router_id = myState->myipaddr;
  hello.CoreAddress = myState->CoreAddress;

  //TODO: implement sequence number
  //hello->HelloSequenceNumber = myState->something;
  //myState->something++;

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(OBAMP_SIGNALLING_PORT);
  si_other.sin_addr = *addr;
  sendto(sdudp, &hello, sizeof(struct OBAMP_hello), 0, (struct sockaddr *)&si_other, sizeof(si_other));
}


//Destroy a Tree Link
static void
send_destroy_tree_link(struct in_addr *addr)
{
  struct OBAMP_tree_destroy msg;
  struct sockaddr_in si_other;
  #if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
  #endif
 

  OLSR_DEBUG(LOG_PLUGINS, "Sending tree destroy to: %s", ip4_to_string(&buf, *addr));
 
  memset(&msg, 0, sizeof(msg));
  msg.MessageID = OBAMP_TREE_DESTROY;
  //TODO: refresh IP address
  msg.router_id = myState->myipaddr;
  msg.CoreAddress = myState->CoreAddress;

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(OBAMP_SIGNALLING_PORT);
  si_other.sin_addr = *addr;
  sendto(sdudp, &msg, sizeof(struct OBAMP_tree_destroy), 0, (struct sockaddr *)&si_other, sizeof(si_other));
}


//Request a Tree Link
static void
tree_link_req(struct in_addr *addr)
{
  if (myState->TreeRequestDelay == 0) { 
  struct OBAMP_tree_link_req req;
  struct sockaddr_in si_other;
  #if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
  #endif
 

  OLSR_DEBUG(LOG_PLUGINS, "Sending tree request to: %s", ip4_to_string(&buf, *addr));
 
  memset(&req, 0, sizeof(req));
  req.MessageID = OBAMP_TREE_REQ;
  //TODO: refresh IP address
  req.router_id = myState->myipaddr;
  req.CoreAddress = myState->CoreAddress;

  req.SequenceNumber = myState->tree_req_sn;
  myState->tree_req_sn++;

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(OBAMP_SIGNALLING_PORT);
  si_other.sin_addr = *addr;
  myState->TreeRequestDelay=_TREE_REQUEST_DELAY_;
  sendto(sdudp, &req, sizeof(struct OBAMP_tree_link_req), 0, (struct sockaddr *)&si_other, sizeof(si_other));
}
else OLSR_DEBUG(LOG_PLUGINS,"Do not send Tree Link Request because there is another one running");
}

static void
tree_link_ack(struct OBAMP_tree_link_req *req)
{

  struct sockaddr_in si_other;
  struct in_addr addr;

  struct ObampNode *tmp;
  struct list_iterator iterator;

#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

  struct OBAMP_tree_link_ack ack;

  //Check Core Address
  if (memcmp(&myState->CoreAddress.v4, &req->CoreAddress.v4, sizeof(struct in_addr)) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Discarding message with no coherent core address");
    return;
  }
  //TODO: other checks ?


  memset(&ack, 0, sizeof(ack));
  ack.MessageID = OBAMP_TREE_ACK;
  //TODO: refresh IP address
  ack.router_id = myState->myipaddr;
  ack.CoreAddress = myState->CoreAddress;

  ack.SequenceNumber = req->SequenceNumber;

  addr = req->router_id.v4;

  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (tmp->neighbor_ip_addr.v4.s_addr == req->router_id.v4.s_addr) {

      tmp->isTree = 1;
      OLSR_DEBUG(LOG_PLUGINS, "Tree link to %s activated", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
      break;
    }
  }

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(OBAMP_SIGNALLING_PORT);
  si_other.sin_addr = addr;
  sendto(sdudp, &ack, sizeof(struct OBAMP_tree_link_req), 0, (struct sockaddr *)&si_other, sizeof(si_other));
}


static void
init_overlay_neighbor(struct ObampNode *n)
{

  n->Texpire = _Texpire_;       //If this value expires the OBAMP node is removed from the list
  n->isMesh = 0;
  n->wasMesh = 0;
  n->MeshLock = 0;
  n->isTree = 0;
  n->outerTreeLink = 0;
  n->DataSeqNumber = 0;
}

static void
tree_create_forward_to(struct in_addr *addr, struct OBAMP_tree_create *mytc)
{

  struct sockaddr_in si_other;
  struct OBAMP_tree_create temptc;

  memset(&temptc, 0, sizeof(temptc));
  memcpy(&temptc, mytc, sizeof(struct OBAMP_tree_create));

  //Check Core Address
  if (memcmp(&myState->CoreAddress.v4, &temptc.CoreAddress.v4, sizeof(struct in_addr)) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Discarding message with no coherent core address");
    return;
  }
  //Update router id
  temptc.router_id = myState->myipaddr;

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(OBAMP_SIGNALLING_PORT);
  si_other.sin_addr = *addr;

  sendto(sdudp, &temptc, sizeof(struct OBAMP_tree_create), 0, (struct sockaddr *)&si_other, sizeof(si_other));
}

static void
tree_create_gen(struct in_addr *addr)
{
  struct OBAMP_tree_create mytc;
  struct sockaddr_in si_other;

  OLSR_DEBUG(LOG_PLUGINS, "Calling tree_create_gen\n");

  memset(&mytc, 0, sizeof(mytc));
  mytc.MessageID = OBAMP_TREECREATE;
  mytc.router_id = myState->myipaddr;
  mytc.CoreAddress = myState->CoreAddress;
  myState->TreeCreateSequenceNumber++;
  mytc.SequenceNumber = myState->TreeCreateSequenceNumber;

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(OBAMP_SIGNALLING_PORT);
  si_other.sin_addr = *addr;
  sendto(sdudp, &mytc, sizeof(struct OBAMP_tree_create), 0, (struct sockaddr *)&si_other, sizeof(si_other));
}


static void
printObampNodesList(void)
{

  int i = 1;
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *tmp;
  struct list_iterator iterator;

  if (!list_is_empty(&ListOfObampNodes)) {     //if the list is NOT empty


    OLSR_DEBUG(LOG_PLUGINS, "--------------------NODE STATUS---------");
    OLSR_DEBUG(LOG_PLUGINS, "---Current Core: %s", ip4_to_string(&buf, myState->CoreAddress.v4));
    OLSR_DEBUG(LOG_PLUGINS, "---Current Parent: %s", ip4_to_string(&buf, myState->ParentId.v4));


    OLSR_DEBUG(LOG_PLUGINS, "Number \t IP \t\t IsMesh \t IsTree \t MeshLock \t outerTreeLink");

    OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
      OLSR_DEBUG(LOG_PLUGINS, "%d \t\t %s \t %d \t\t %d \t\t %d \t\t %d", i, ip4_to_string(&buf, tmp->neighbor_ip_addr.v4),
                 tmp->isMesh, tmp->isTree, tmp->MeshLock, tmp->outerTreeLink);

      i++;
    }
    OLSR_DEBUG(LOG_PLUGINS, "----------------------------------------");

  }
}

static int
DoIHaveATreeLink(void)
{

  //struct ipaddr_str buf;
  struct ObampNode *tmp;
  struct list_iterator iterator;

  if (!list_is_empty(&ListOfObampNodes)) {     //if the list is NOT empty

    OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
      if (tmp->isTree == 1)
        return 1;

    }
  }
  return 0;
}


void
unsolicited_tree_destroy(void *x)
{

  //struct ipaddr_str buf;
  struct ObampNode *tmp;
  struct list_iterator iterator;

  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (tmp->isTree == 0) {
      send_destroy_tree_link(&tmp->neighbor_ip_addr.v4);
    }
  }
  x = NULL;
}

static int
DoIHaveAMeshLink(void)
{

  //struct ipaddr_str buf;
  struct ObampNode *tmp;
  struct list_iterator iterator;

  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (tmp->isMesh == 1) {
      return 1;
    }
  }
  return 0;
}

static void
reset_tree_links(void)
{
  struct ObampNode *tmp;
  struct list_iterator iterator;

  OLSR_DEBUG(LOG_PLUGINS,"Reset Tree Links Now");

  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    tmp->isTree = 0;
    tmp->outerTreeLink = 0;
  }

  memset(&myState->ParentId.v4, 0, sizeof(myState->ParentId.v4));
  memset(&myState->OldParentId.v4, 1, sizeof(myState->OldParentId.v4));
  myState->TreeCreateSequenceNumber=0;
};


//I received a request to deactivate a tree link
static void
deactivate_tree_link(struct OBAMP_tree_destroy *destroy_msg)
{
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *tmp;
  struct list_iterator iterator;

  if (memcmp(&myState->CoreAddress.v4, &destroy_msg->CoreAddress.v4, sizeof(struct in_addr)) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Discarding message with no coherent core address");
    return;
  }

  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (tmp->neighbor_ip_addr.v4.s_addr == destroy_msg->router_id.v4.s_addr) {
      tmp->isTree = 0;
      OLSR_DEBUG(LOG_PLUGINS, "Tree link to %s deactivated", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
        
      if (memcmp(&tmp->neighbor_ip_addr.v4, &myState->ParentId.v4, sizeof(struct in_addr)) == 0) {
        OLSR_DEBUG(LOG_PLUGINS,"RESET TREE LINKS: I lost tree link with my PARENT");
        reset_tree_links();
      }
      return;
    }
  }
}

//Core Election. The Core is the one with the smallest IP Address
static void
CoreElection(void)
{
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *tmp;
  struct list_iterator iterator;
  u_int32_t smallestIP = 0xFFFFFFFF;

  //Update my current IP address
  memcpy(&myState->myipaddr.v4, &olsr_cnf->router_id, olsr_cnf->ipsize);

  OLSR_DEBUG(LOG_PLUGINS, "SETUP: my IP - %s", ip4_to_string(&buf, myState->myipaddr.v4));


  if (list_is_empty(&ListOfObampNodes)) {     //if the list is empty
    OLSR_DEBUG(LOG_PLUGINS, "CoreElection: I'm alone I'm the core");
    myState->CoreAddress = myState->myipaddr;
    myState->iamcore = 1;
    myState->TreeCreateSequenceNumber = 0;
    OLSR_DEBUG(LOG_PLUGINS,"Calling Reset Tree Links");
    reset_tree_links();
    return;
  }

  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (tmp->neighbor_ip_addr.v4.s_addr < smallestIP) {
      smallestIP = tmp->neighbor_ip_addr.v4.s_addr;
      OLSR_DEBUG(LOG_PLUGINS, "CoreElection: current smallest IP is - %s", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
    };
  }

  //Check if I'm the core.
  if (myState->myipaddr.v4.s_addr < smallestIP) {     //I'm the core
    if (myState->myipaddr.v4.s_addr == myState->CoreAddress.v4.s_addr) {      //I'm was already the core
      return;
    }

    //I'm becoming core
    myState->CoreAddress = myState->myipaddr;
    myState->iamcore = 1;
    myState->TreeCreateSequenceNumber = 0;
    OLSR_DEBUG(LOG_PLUGINS,"Calling Reset Tree Links"); 
    reset_tree_links();
    OLSR_DEBUG(LOG_PLUGINS, "I'm the core");
  } else {
    if (myState->CoreAddress.v4.s_addr == smallestIP) {       //the core did not change
      return;
    }

    //core changed
    myState->iamcore = 0;
    myState->CoreAddress.v4.s_addr = smallestIP;
    OLSR_DEBUG(LOG_PLUGINS,"Calling Reset Tree Links");
    reset_tree_links();
    OLSR_DEBUG(LOG_PLUGINS, "CoreElection: current Core is - %s", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
  }
}

//Starts a UDP listening port for OBAMP signalling
static int
UdpServer(void)
{
  struct sockaddr_in addr;

  if ((sdudp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
    OLSR_DEBUG(LOG_PLUGINS, "Socket UDP error");
    return -1;
  }

  memset((void *)&addr, 0, sizeof(addr));       /* clear server address */
  addr.sin_family = AF_INET;    /* address type is INET */
  addr.sin_port = htons(OBAMP_SIGNALLING_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);     /* connect from anywhere */
  /* bind socket */
  if (bind(sdudp, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Socket UDP BIND error");
    return (-1);
  }

  add_olsr_socket(sdudp, &ObampSignalling, NULL, NULL, SP_PR_READ);

  return 0;

}

static void
decap_data(char *buffer)
{
  struct ObampSniffingIf *tmp;         //temp pointers used when parsing the list
  struct list_iterator iterator;
  unsigned char *ipPacket;
  int stripped_len;
  struct ip *ipHeader;
  struct ip6_hdr *ip6Header;
  struct OBAMP_data_message4 *msg;
  int nBytesWritten;
  struct sockaddr_ll dest;
  msg = (struct OBAMP_data_message4 *)buffer;

  ipPacket = msg->data;

  ipHeader = (struct ip *)ipPacket;
  ip6Header = (struct ip6_hdr *)ipPacket;

  OLSR_FOR_ALL_OBAMPSNIFF_ENTRIES(tmp, iterator) {
    memset(&dest, 0, sizeof(dest));
    dest.sll_family = AF_PACKET;
    if ((ipPacket[0] & 0xf0) == 0x40) {
      dest.sll_protocol = htons(ETH_P_IP);
      stripped_len = ntohs(ipHeader->ip_len);
    }
    if ((ipPacket[0] & 0xf0) == 0x60) {
      dest.sll_protocol = htons(ETH_P_IPV6);
      stripped_len = 40 + ntohs(ip6Header->ip6_plen); //IPv6 Header size (40) + payload_len
    }
    //TODO: if packet is not IP die here

    dest.sll_ifindex = if_nametoindex(tmp->ifName);
    dest.sll_halen = IFHWADDRLEN;

    /* Use all-ones as destination MAC address. When the IP destination is
     * a multicast address, the destination MAC address should normally also
     * be a multicast address. E.g., when the destination IP is 224.0.0.1,
     * the destination MAC should be 01:00:5e:00:00:01. However, it does not
     * seem to matter when the destination MAC address is set to all-ones
     * in that case. */
    memset(dest.sll_addr, 0xFF, IFHWADDRLEN);

    nBytesWritten = sendto(tmp->skd, ipPacket, stripped_len, 0, (struct sockaddr *)&dest, sizeof(dest));
    if (nBytesWritten != stripped_len) {
      OLSR_DEBUG(LOG_PLUGINS, "sendto() error forwarding unpacked encapsulated pkt on \"%s\"", tmp->ifName);
    } else {
      OLSR_DEBUG(LOG_PLUGINS, "OBAMP: --> unpacked and forwarded on \"%s\"\n", tmp->ifName);
    }
  }
}



static int
CheckDataFromTreeLink(char *buffer)
{

  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct list_iterator iterator;
  struct OBAMP_data_message4 *data_msg;

  data_msg = (struct OBAMP_data_message4 *)buffer;

  //Scroll the list
  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    //Scroll the list until we find the entry relative to the last hop of the packet we are processing
    if (memcmp(&tmp->neighbor_ip_addr.v4, &data_msg->last_hop, sizeof(struct in_addr)) == 0) {
      if (tmp->isTree == 1) {  //OK we receive data from a neighbor that we have a tree link with
        return 1;
      }
      OLSR_DEBUG(LOG_PLUGINS, "DISCARDING DATA PACKET SENT BY NOT TREE NEIGHBOR");
      send_destroy_tree_link(&tmp->neighbor_ip_addr.v4);
      return 0;
    }
  }
  return 0;
}


static int
CheckDupData(char *buffer)
{

  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct list_iterator iterator;
  struct OBAMP_data_message4 *data_msg;

  data_msg = (struct OBAMP_data_message4 *)buffer;

  //Scroll the list
  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (memcmp(&tmp->neighbor_ip_addr.v4, &data_msg->router_id, sizeof(struct in_addr)) == 0) {
      OLSR_DEBUG(LOG_PLUGINS, "Processing Data Packet %d - Last seen was %d",data_msg->SequenceNumber, tmp->DataSeqNumber);

      if (tmp->DataSeqNumber == 0) {  //First packet received from this host
        tmp->DataSeqNumber = data_msg->SequenceNumber;
        return 1;
      }

      if ( ((data_msg->SequenceNumber > tmp->DataSeqNumber) && ((data_msg->SequenceNumber - tmp->DataSeqNumber ) <= 127)) || ((tmp->DataSeqNumber > data_msg->SequenceNumber) && ((tmp->DataSeqNumber - data_msg->SequenceNumber) > 127 ))) //data_msg->SequenceNumber > tmp->DataSeqNumber
      {
        tmp->DataSeqNumber = data_msg->SequenceNumber;
        return 1;
      }
      OLSR_DEBUG(LOG_PLUGINS, "DISCARDING DUP PACKET");
      return 0;
    }
  }
  return 1;
}

static void
forward_obamp_data(char *buffer)
{

#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
  struct ipaddr_str buf2;
#endif
  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct list_iterator iterator;
  struct OBAMP_data_message4 *data_msg;
  struct sockaddr_in si_other;
  struct in_addr temporary;


  data_msg = (struct OBAMP_data_message4 *)buffer;
  temporary.s_addr  = data_msg->last_hop;
  data_msg->last_hop = myState->myipaddr.v4.s_addr;

  //Scroll the list
  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (tmp->isTree == 1 && memcmp(&tmp->neighbor_ip_addr.v4, &temporary.s_addr, 4) != 0) {
      OLSR_DEBUG(LOG_PLUGINS, "FORWARDING OBAMP DATA TO node %s because come from %s", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4), ip4_to_string(&buf2,temporary));

      //FORWARD DATA
      memset((char *)&si_other, 0, sizeof(si_other));
      si_other.sin_family = AF_INET;
      si_other.sin_port = htons(OBAMP_SIGNALLING_PORT);
      si_other.sin_addr = tmp->neighbor_ip_addr.v4;

      //sendto(sdudp, data_msg, sizeof(struct OBAMP_data_message), 0, (struct sockaddr *)&si_other, sizeof(si_other));
      sendto(sdudp, data_msg, 17+data_msg->datalen, 0, (struct sockaddr *)&si_other, sizeof(si_other));
    }
  }
}


static void
manage_hello(char *packet)
{

  struct OBAMP_hello *hello;

  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct list_iterator iterator;

  hello = (struct OBAMP_hello *)packet;

  //FIRST OF ALL CHECK CORE
  if (memcmp(&myState->CoreAddress.v4, &hello->CoreAddress.v4, sizeof(struct in_addr)) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Discarding message with no coherent core address");
    return;
  }

  if (!list_is_empty(&ListOfObampNodes)) {     //if the list is empty
    OLSR_DEBUG(LOG_PLUGINS, "Very strange, list cannot be empty here !");
    return;
  }

  //Scroll the list
  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
     if (memcmp(&tmp->neighbor_ip_addr.v4, &hello->router_id.v4, sizeof(struct in_addr)) == 0) {       //I search in the list the neighbor I received the hello from
      tmp->isMesh = 1;
      tmp->MeshLock = _MESH_LOCK_;
    }
  }
}

static void
manage_tree_create(char *packet)
{

  struct OBAMP_tree_create *msg;

#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
  struct ipaddr_str buf2;              //buf to print debug infos
#endif

  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct list_iterator iterator;

  OLSR_DEBUG(LOG_PLUGINS,"manage_tree_create");
  msg = (struct OBAMP_tree_create *)packet;

  if (msg->MessageID != OBAMP_TREECREATE) {
    OLSR_DEBUG(LOG_PLUGINS, "BIG PROBLEM, I'M IN THIS FUNCTION BUT MESSAGE IS NOT TREE CREATE");
    return;
  }

  //FIRST OF ALL CHECK CORE
  if (memcmp(&myState->CoreAddress.v4, &msg->CoreAddress.v4, sizeof(struct in_addr)) != 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Discarding message with no coherent core address");
    return;
  }
  if (myState->iamcore == 1) {        //I'm core and receiving tree create over a loop
    return;
  }

  // TODO: cleanup this statement
  if ( (((msg->SequenceNumber > myState->TreeCreateSequenceNumber) && ((msg->SequenceNumber - myState->TreeCreateSequenceNumber ) <= 127))
      || ((myState->TreeCreateSequenceNumber > msg->SequenceNumber) && ((myState->TreeCreateSequenceNumber - msg->SequenceNumber) > 127 ))
       /*myState->TreeCreateSequenceNumber < msg->SequenceNumber*/) || myState->TreeCreateSequenceNumber == 0 ) {
    //If tree create is not a duplicate
    OLSR_DEBUG(LOG_PLUGINS, "myState->TreeCreateSequenceNumber < msg->SequenceNumber --- %d < %d",myState->TreeCreateSequenceNumber,msg->SequenceNumber);
    myState->TreeCreateSequenceNumber = msg->SequenceNumber;

    // A bug was fixed here a battlemeshv3
    // myState->OldParentId.v4 = myState->ParentId.v4;
    // myState->ParentId.v4 = msg->router_id.v4;

    // if (memcmp(&myState->OldParentId.v4, &myState->ParentId.v4, sizeof(struct in_addr)) != 0)       //If it changed
    if (memcmp(&msg->router_id.v4, &myState->ParentId.v4, sizeof(struct in_addr)) != 0)       //If it changed
    {
      OLSR_DEBUG(LOG_PLUGINS, "Receiving a tree message from a link that is not parent");
      if (DoIHaveATreeLink() == 0 ) {
        OLSR_DEBUG(LOG_PLUGINS,"Calling Reset Tree Links");
        reset_tree_links();
        myState->ParentId.v4 = msg->router_id.v4;
        myState->OldParentId.v4 = myState->ParentId.v4;
        tree_link_req(&msg->router_id.v4);
      }
      else {
        // I have a tree link already, evaluate new parent ??
      }
    }
    else { //Receiving a TreeCreate from my parent so I can refresh hearthbeat
      myState->TreeHeartBeat = TREE_HEARTBEAT;
        
      // FORWARD the tree message on the mesh
      if (list_is_empty(&ListOfObampNodes)) {       //if the list is empty
        OLSR_DEBUG(LOG_PLUGINS, "Very strange, list cannot be empty here !");
      }

      //Scroll the list
      OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
        if (tmp->isMesh == 1 && memcmp(&tmp->neighbor_ip_addr.v4, &msg->router_id.v4, sizeof(struct in_addr)) != 0) {
          //Is neighbor and not the originator of this tree create
          if (DoIHaveATreeLink() == 1 ) { //I forward only if I have a link tree to the core ready
            tree_create_forward_to(&tmp->neighbor_ip_addr.v4, msg);
            OLSR_DEBUG(LOG_PLUGINS, "FORWARDING TREE CREATE ORIGINATOR %s TO node %s ", ip4_to_string(&buf2, msg->router_id.v4),
                       ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
          }
        }
      }
    } //END IF TREE CREATE IS NOT A DUPLICATE
  }
  else {
   OLSR_DEBUG(LOG_PLUGINS, "myState->TreeCreateSequenceNumber < msg->SequenceNumber --- %d < %d",myState->TreeCreateSequenceNumber,msg->SequenceNumber);
   OLSR_DEBUG(LOG_PLUGINS, "DISCARDING DUP TREE CREATE");
  }
}

void
ObampSignalling(int skfd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{

  char buffer[1500];
  char text_buffer[300];
  struct sockaddr_in addr;
  int n = 0;
  socklen_t len;
  u_int8_t MessageID;

  memset(&addr, 0, sizeof(struct sockaddr_in));
  len = sizeof(addr);

  if (skfd > 0) {

    //OLSR_DEBUG(LOG_PLUGINS,"INCOMING OBAMP SIGNALLING");

    n = recvfrom(skfd, buffer, 1500, 0, (struct sockaddr *)&addr, &len);

    if (n < 0) {
      OLSR_DEBUG(LOG_PLUGINS, "recvfrom error");
    }

    inet_ntop(AF_INET, &addr.sin_addr, text_buffer, sizeof(text_buffer));
    //OLSR_DEBUG(LOG_PLUGINS,"Request from host %s, port %d\n", text_buffer, ntohs(addr->sin_port));

    MessageID = buffer[0];

    switch (MessageID) {

    case OBAMP_DATA:
      OLSR_DEBUG(LOG_PLUGINS, "OBAMP Received OBAMP_DATA from host %s, port %d\n", text_buffer, ntohs(addr.sin_port));

      if (CheckDupData(buffer) && CheckDataFromTreeLink(buffer) ) {
        forward_obamp_data(buffer);
        decap_data(buffer);
      }

      break;

    case OBAMP_HELLO:
      OLSR_DEBUG(LOG_PLUGINS, "OBAMP Received OBAMP_HELLO from host %s, port %d\n", text_buffer, ntohs(addr.sin_port));
      manage_hello(buffer);
      break;

    case OBAMP_TREECREATE:
      //do here
      OLSR_DEBUG(LOG_PLUGINS, "OBAMP Received OBAMP_TREECREATE from host %s, port %d\n", text_buffer, ntohs(addr.sin_port));
      manage_tree_create(buffer);

      break;

    case OBAMP_TREE_REQ:
      OLSR_DEBUG(LOG_PLUGINS, "OBAMP Received OBAMP_TREE_REQ from host %s, port %d\n", text_buffer, ntohs(addr.sin_port));
      tree_link_ack((struct OBAMP_tree_link_req *)buffer);
      break;

    case OBAMP_TREE_ACK:
      //do here
      OLSR_DEBUG(LOG_PLUGINS, "OBAMP Received OBAMP_TREE_ACK from host %s, port %d\n", text_buffer, ntohs(addr.sin_port));
      activate_tree_link((struct OBAMP_tree_link_ack *)buffer);
      break;


    case OBAMP_TREE_DESTROY:
      //do here
      OLSR_DEBUG(LOG_PLUGINS, "OBAMP Received OBAMP_TREE_DESTROY from host %s, port %d\n", text_buffer, ntohs(addr.sin_port));
      deactivate_tree_link((struct OBAMP_tree_destroy *)buffer);
      break;
    }


  }                             //if skfd<0

}


/*
adds a IPv4 ObampNode in the list if it is new
If a new node is added CoreElection is called to update the current core
*/
int
addObampNode4(struct in_addr *ipv4, u_int8_t status)
{
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *neighbor_to_add;
  struct ObampNode *tmp;
  struct list_iterator iterator;
  neighbor_to_add = olsr_malloc(sizeof(struct ObampNode), "OBAMPNode");

//OLSR_DEBUG(LOG_PLUGINS,"Adding to list node - %s\n",ip4_to_string(&buf,*ipv4));


  if (list_is_empty(&ListOfObampNodes)) {     //Empty list
    //OLSR_DEBUG(LOG_PLUGINS,"List is empty %d adding first node\n",listold_empty(&ListOfObampNodes));

    memcpy(&neighbor_to_add->neighbor_ip_addr.v4, ipv4, sizeof(neighbor_to_add->neighbor_ip_addr.v4));
    list_add_head(&ListOfObampNodes, &(neighbor_to_add->list));

    init_overlay_neighbor(neighbor_to_add);
    neighbor_to_add->status = status;

    OLSR_DEBUG(LOG_PLUGINS, "Added to list node as first node- %s\n", ip4_to_string(&buf, *ipv4));

    CoreElection();
    return 0;
  }

  //Some node already in list

  //Scroll the list to check if the element already exists
  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (memcmp(&tmp->neighbor_ip_addr.v4, ipv4, sizeof(tmp->neighbor_ip_addr.v4)) == 0) {
      //OLSR_DEBUG(LOG_PLUGINS,"Node already present in list %s\n",ip4_to_string(&buf,*ipv4));
      tmp->Texpire = _Texpire_;       //Refresh Texpire
      neighbor_to_add->status = status;
      free(neighbor_to_add);
      return 1;
    }
  }

  //Add element to list
  // neighbor_to_add->Texpire=_Texpire_; //Refresh Texpire
  OLSR_DEBUG(LOG_PLUGINS, "Adding to list node (NOT FIRST)- %s\n", ip4_to_string(&buf, *ipv4));
  memcpy(&neighbor_to_add->neighbor_ip_addr.v4, ipv4, sizeof(neighbor_to_add->neighbor_ip_addr.v4));
  list_add_tail(&ListOfObampNodes, &(neighbor_to_add->list));
  init_overlay_neighbor(neighbor_to_add);
  neighbor_to_add->status = status;
  CoreElection();
  return 0;
}                               //End AddObampNode



/* -------------------------------------------------------------------------
 * Function   : PacketReceivedFromOLSR
 * Description: Handle a received packet from a OLSR message
 * Input      : Obamp Message
 * Output     : none
 * Return     : none
 * ------------------------------------------------------------------------- */
static void
PacketReceivedFromOLSR(union olsr_ip_addr *originator, const uint8_t *obamp_message, int len)
{
  u_int8_t MessageID = obamp_message[0];
  const struct OBAMP_alive *alive;
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

  struct in_addr *myOriginator = (struct in_addr *)originator;

//TODO: this is useless now
  len = 0;

//See obamp.h
  switch (MessageID) {

  case OBAMP_ALIVE:
    OLSR_DEBUG(LOG_PLUGINS, "OBAMP Received OBAMP_ALIVE from %s\n", ip4_to_string(&buf, *myOriginator));
    alive = (const struct OBAMP_alive *)obamp_message;
    addObampNode4(myOriginator, alive->status);
    printObampNodesList();

    break;
  }


}                               /* PacketReceivedFromOLSR */


//OLSR parser, received OBAMP messages
void
olsr_parser(struct olsr_message *msg, struct interface *in_if
            __attribute__ ((unused)), union olsr_ip_addr *ipaddr, enum duplicate_status status __attribute__ ((unused)))
{
  //OLSR_DEBUG(LOG_PLUGINS, "OBAMP PLUGIN: Received msg in parser\n");

  if (msg->type != MESSAGE_TYPE) {
    return;
  }

  /* Check if message originated from this node.
   *         If so - back off */
  if (olsr_ipcmp(&msg->originator, &olsr_cnf->router_id) == 0)
    return;

  /* Check that the neighbor this message was received from is symmetric.
   *         If not - back off*/
  if (check_neighbor_link(ipaddr) != SYM_LINK) {
    //struct ipaddr_str strbuf;
    return;
  }

  PacketReceivedFromOLSR(&msg->originator, msg->payload, msg->end - msg->payload);
}

//Sends a packet in the OLSR network
void
olsr_obamp_gen(void *packet, int len)
{
  /* send buffer: huge */
  uint8_t buffer[10240];
  struct olsr_message msg;
  struct interface *ifn;
  struct list_iterator iterator;
  uint8_t *curr, *sizeptr;

  /* fill message */
  msg.type = MESSAGE_TYPE;
  msg.vtime = OBAMP_VALID_TIME * MSEC_PER_SEC;
  msg.originator = olsr_cnf->router_id;
  msg.ttl = MAX_TTL;
  msg.hopcnt = 0;
  msg.seqno = get_msg_seqno();
  msg.size = 0; /* put in later */

  curr = buffer;
  sizeptr = olsr_put_msg_hdr(&curr, &msg);
  memcpy(curr, packet, len);

  len = curr - buffer + len;
  pkt_put_u16(&sizeptr, len);

  /* looping trough interfaces */
  OLSR_FOR_ALL_INTERFACES(ifn, iterator) {
    if (net_outbuffer_push(ifn, buffer, len) != len) {
      /* send data and try again */
      net_output(ifn);
      if (net_outbuffer_push(ifn, buffer, len) != len) {
        OLSR_DEBUG(LOG_PLUGINS, "OBAMP PLUGIN: could not send on interface: %s\n", ifn->int_name);
      }
    }
  }
}

void
outer_tree_create(void *x __attribute__ ((unused)))
{
  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct list_iterator iterator;

  if (!((DoIHaveATreeLink() == 0) && (myState->iamcore == 0))) {   //If there are tree links
    return;
  }

  if (list_is_empty(&ListOfObampNodes) == 0) {   //if the list is empty
    OLSR_DEBUG(LOG_PLUGINS, "List empty can't send OUTER_TREE_CREATE");
  }

  if (DoIHaveAMeshLink() == 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Weird, no mesh links. Maybe other OBAMP nodes are very far (HUGE ETX)");
    return;
  }

  // Update my current IP address
  memcpy(&myState->myipaddr.v4, &olsr_cnf->router_id, olsr_cnf->ipsize);
  //OLSR_DEBUG(LOG_PLUGINS,"SETUP: my IP - %s",ip4_to_string(&buf,myState->myipaddr.v4));

  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if ((tmp->neighbor_ip_addr.v4.s_addr < myState->myipaddr.v4.s_addr) && (tmp->isMesh == 1)) {
      return;             //I have a neighbor that will send a outer tree create for me
    }
  }

  //tree create
  OLSR_DEBUG(LOG_PLUGINS, "OUTER TREE CREATE");
  tmp = select_tree_anchor();
  if (tmp == NULL) {
    OLSR_DEBUG(LOG_PLUGINS, "CANT FIND ANCHOR");
    return;
  }

  tmp->isMesh = 1;
  tmp->outerTreeLink = 1;
  myState->OldParentId.v4 = tmp->neighbor_ip_addr.v4;
  myState->ParentId.v4 = tmp->neighbor_ip_addr.v4;
  myState->TreeHeartBeat = TREE_HEARTBEAT;
  tree_link_req(&tmp->neighbor_ip_addr.v4);
}


void
tree_create(void *x __attribute__ ((unused)))
{
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct list_iterator iterator;

  // Check if I'm core
  if (myState->iamcore != 1) {
    return;
  }

  if (list_is_empty(&ListOfObampNodes)) {   //if the list is empty
    OLSR_DEBUG(LOG_PLUGINS, "List empty can't send TREE_CREATE");
    return;
  }

  //Scroll the list
  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    if (tmp->isMesh == 1) { //Is neighbor
      //send tree create
      tree_create_gen(&tmp->neighbor_ip_addr.v4);

      OLSR_DEBUG(LOG_PLUGINS, "CORE SENDS TREE CREATE TO node %s ", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
    }
  }
}




void
mesh_create(void *x __attribute__ ((unused)))
{
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *tmp;               //temp pointers used when parsing the list
  struct list_iterator iterator;

  struct rt_entry *rt;                 //"rt->rt_best->rtp_metric.cost" is the value you are looking for, a 32 bit

  unsigned int mincost = 5;

  int meshchanged = 0;

  if (list_is_empty(&ListOfObampNodes)) {     //if the list is empty
    OLSR_DEBUG(LOG_PLUGINS, "List empty can't create Overlay Mesh");
    return;
  }

  //Scroll the list
  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    //set every OBAMP node to be NOT neighbor
    tmp->wasMesh = tmp->isMesh;

    //MeshLock in case mesh link is requested from neighbor
    if (tmp->MeshLock == 0 && tmp->outerTreeLink == 0) {
      tmp->isMesh = 0;
    }

    rt = olsr_lookup_routing_table(&tmp->neighbor_ip_addr);
    if (rt == NULL) {         //route is not present yet
      continue;
    }

    //OLSR_DEBUG(LOG_PLUGINS,"ROUTING TO node %s costs %u",ip4_to_string(&buf,tmp->neighbor_ip_addr.v4),rt->rt_best->rtp_metric.cost/65536);

    if (rt->rt_best->rtp_metric.cost / 65536 > 5) {
      continue;               //we not not consider links that are poorer than ETX=5
    }

    //update min cost
    if ((rt->rt_best->rtp_metric.cost / 65536) < mincost) {
      mincost = (rt->rt_best->rtp_metric.cost / 65536);
    }
  } //end for each

  //now that I know the mincost to the closer OBAMP node I choose my neighbor set
  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    rt = olsr_lookup_routing_table(&tmp->neighbor_ip_addr);
    if (rt == NULL) {         //route is not present yet
      continue;
    }

    if (rt->rt_best->rtp_metric.cost / 65536 > 5) {
      continue;               //we not not consider links that are poorer than ETX=5
    }

    if ((rt->rt_best->rtp_metric.cost / 65536) - 1 < mincost) {       //Choose for mesh
      tmp->isMesh = 1;
      OLSR_DEBUG(LOG_PLUGINS, "Choosed Overlay Neighbor node %s costs %u", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4),
                 rt->rt_best->rtp_metric.cost / 65536);

      obamp_hello(&tmp->neighbor_ip_addr.v4);
    }

    if (tmp->outerTreeLink == 1) {
      obamp_hello(&tmp->neighbor_ip_addr.v4);
    }

    if (tmp->isMesh != tmp->wasMesh) {
      meshchanged++;
      if (tmp->isMesh == 0 && tmp->isTree == 1) {
        tmp->isTree = 0;
        send_destroy_tree_link(&tmp->neighbor_ip_addr.v4);

        if (memcmp(&tmp->neighbor_ip_addr.v4, &myState->ParentId.v4, sizeof(struct in_addr)) == 0) {
          OLSR_DEBUG(LOG_PLUGINS,"RESET TREE LINKS: I lost tree link with my PARENT");
          reset_tree_links();
        }
      }
    }
  }                           //end for each

  if (meshchanged) {
    //trigger signalling
  }
}


void
purge_nodes(void *x __attribute__ ((unused)))
{
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *tmp;
  struct list_iterator iterator;
  int nodesdeleted = 0;

  if (myState->TreeHeartBeat > 0) {
    myState->TreeHeartBeat--;
  }

  if (myState->TreeRequestDelay > 0) {
    myState->TreeRequestDelay--;
  }

  if (myState->TreeHeartBeat == 0 && myState->iamcore == 0){ 
    OLSR_DEBUG(LOG_PLUGINS,"Calling Reset Tree Links"); 
    reset_tree_links();
  }

  //OLSR_DEBUG(LOG_PLUGINS,"OBAMP: Timer Expired Purging Nodes");
  if (list_is_empty(&ListOfObampNodes)) {     //if the list is empty
    //OLSR_DEBUG(LOG_PLUGINS,"OBAMP: List empty");
    return;
  }

  OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
    tmp->Texpire--;
    if (tmp->MeshLock != 0) {
      tmp->MeshLock--;
    }

    //OLSR_DEBUG(LOG_PLUGINS,"Updating node %s with Texpire %d",ip4_to_string(&buf,tmp->neighbor_ip_addr.v4),tmp->Texpire);
    if (tmp->Texpire == 0) {  //purge
      OLSR_DEBUG(LOG_PLUGINS, "Purging node %s", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
      list_remove(&tmp->list);
      //OLSR_DEBUG(LOG_PLUGINS,"OBAMP CHECK EMPTY %d",listold_empty(&ListOfObampNodes));

      free(tmp);
      nodesdeleted++;
    }
  }

  if (nodesdeleted != 0) {
    CoreElection();
  }
}


void
obamp_alive_gen(void *x __attribute__ ((unused)))
{
  struct OBAMP_alive myAlive;
  OLSR_DEBUG(LOG_PLUGINS, "Calling obamp_alive_gen\n");

  memset(&myAlive, 0, sizeof(myAlive));
  myAlive.MessageID = OBAMP_ALIVE;
  myAlive.status = DoIHaveATreeLink();
  olsr_obamp_gen(&myAlive, sizeof(struct OBAMP_alive));
}



/*
When a packet is captured on the sniffing interfaces, it is called EncapFlowInObamp
here we check if the packet is multicast and we forward it to the overlay neighbors if we have a link tree
*/

void
EncapFlowInObamp(int skfd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  unsigned char ipPacket[1500];        //TODO: optimize me

#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif
  struct ObampNode *tmp;
  struct list_iterator iterator;

  union olsr_ip_addr dst;              /* Destination IP address in captured packet */
  struct ip *ipHeader;                 /* The IP header inside the captured IP packet */
  struct ip6_hdr *ipHeader6;           /* The IP header inside the captured IP packet */

  struct sockaddr_ll pktAddr;
  socklen_t addrLen;
  int nBytes;

  if (skfd < 0) {
    return;
  }

  addrLen = sizeof(pktAddr);

  nBytes = recvfrom(skfd, ipPacket, 1500,     //TODO: optimize me
                    0, (struct sockaddr *)&pktAddr, &addrLen);
  if (nBytes < 0) {
    return;
  }

  /* Check if the number of received bytes is large enough for an IP
   * packet which contains at least a minimum-size IP header.
   * Note: There is an apparent bug in the packet socket implementation in
   * combination with VLAN interfaces. On a VLAN interface, the value returned
   * by 'recvfrom' may (but need not) be 4 (bytes) larger than the value
   * returned on a non-VLAN interface, for the same ethernet frame. */
  if (nBytes < (int)sizeof(struct ip)) {
    OLSR_DEBUG(LOG_PLUGINS, "Captured frame too short");
    return;                   /* for */
  }

  if (pktAddr.sll_pkttype == PACKET_OUTGOING || pktAddr.sll_pkttype == PACKET_MULTICAST)      // ||
    //pktAddr.sll_pkttype == PACKET_BROADCAST)
  {
    //do here
    if ((ipPacket[0] & 0xf0) == 0x40) {       //IPV4
      ipHeader = (struct ip *)ipPacket;

      dst.v4 = ipHeader->ip_dst;

      /* Only forward multicast packets. If configured, also forward local broadcast packets */
      if (!IsMulticast(&dst)) {
        return;
      }

      if (ipHeader->ip_p != SOL_UDP) {
        /* Not UDP */
        OLSR_DEBUG(LOG_PLUGINS, "NON UDP PACKET\n");
        return;               /* for */
      }

      //Forward the packet to tree links
      OLSR_FOR_ALL_OBAMPNODE_ENTRIES(tmp, iterator) {
        if (tmp->isTree == 1) {
          OLSR_DEBUG(LOG_PLUGINS, "Pushing data to Tree link to %s", ip4_to_string(&buf, tmp->neighbor_ip_addr.v4));
          SendOBAMPData(&tmp->neighbor_ip_addr.v4, ipPacket, nBytes);
        }
      }
    }                         //END IPV4
    else if ((ipPacket[0] & 0xf0) == 0x60) {  //IPv6
      ipHeader6 = (struct ip6_hdr *)ipPacket;
      if (ipHeader6->ip6_dst.s6_addr[0] != 0xff) {     //Multicast
        return;               //not multicast
      }

      if (ipHeader6->ip6_nxt != SOL_UDP) {
        /* Not UDP */
        OLSR_DEBUG(LOG_PLUGINS, "NON UDP PACKET\n");
        return;               /* for */
      }
    }                         //END IPV6
    else {
      return;                 //Is not IP packet
    }

    // send the packet to OLSR forward mechanism
    // SendOBAMPData(&tmp->neighbor_ip_addr.v6,ipPacket,nBytes);
  }                           /* if (pktAddr.sll_pkttype == ...) */
}                               /* EncapFlowInObamp */

//This function is called from olsrd_plugin.c and adds to the list the interfaces specified in the configuration file to sniff multicast traffic
int
AddObampSniffingIf(const char *ifName,
                   void *data __attribute__ ((unused)), set_plugin_parameter_addon addon __attribute__ ((unused)))
{

  struct ObampSniffingIf *ifToAdd;

  OLSR_DEBUG(LOG_PLUGINS, "AddObampSniffingIf");


  assert(ifName != NULL);

  ifToAdd = olsr_malloc(sizeof(struct ObampSniffingIf), "OBAMPSniffingIf");

  strncpy(ifToAdd->ifName, ifName, 16); //TODO: 16 fix this
  ifToAdd->ifName[15] = '\0';   /* Ensures null termination */

  OLSR_DEBUG(LOG_PLUGINS, "Adding interface to list");

  list_add_tail(&ListOfObampSniffingIf, &(ifToAdd->list));

  OLSR_DEBUG(LOG_PLUGINS, "Adding if %s to list of ObampSniffingIfaces", ifToAdd->ifName);

  return 0;
}


int
PreInitOBAMP(void)
{
  list_init_head(&ListOfObampSniffingIf);
  return 0;

}

//Start here !!

int
InitOBAMP(void)
{
#if !defined(REMOVE_LOG_DEBUG)
  struct ipaddr_str buf;
#endif

//Structs necessary for timers

  struct timer_entry *OBAMP_alive_timer;
  struct timer_entry *purge_nodes_timer;
  struct timer_entry *mesh_create_timer;
  struct timer_entry *tree_create_timer;
  struct timer_entry *outer_tree_create_timer;
  struct timer_entry *unsolicited_tree_destroy_timer;



  struct olsr_cookie_info *OBAMP_alive_gen_timer_cookie = NULL;
  struct olsr_cookie_info *purge_nodes_timer_cookie = NULL;
  struct olsr_cookie_info *mesh_create_timer_cookie = NULL;
  struct olsr_cookie_info *tree_create_timer_cookie = NULL;
  struct olsr_cookie_info *outer_tree_create_timer_cookie = NULL;
  struct olsr_cookie_info *unsolicited_tree_destroy_timer_cookie = NULL;

  if (olsr_cnf->ip_version == AF_INET6) {
    OLSR_ERROR(LOG_PLUGINS, "OBAMP does not support IPv6 at the moment.");
    return 1;
  }

//Setting OBAMP node state
  myState = olsr_malloc(sizeof(struct ObampNodeState), "OBAMPNodeState");
  myState->iamcore = 1;
  myState->TreeHeartBeat = 0;
  myState->TreeRequestDelay = 0;
  memcpy(&myState->myipaddr.v4, &olsr_cnf->router_id, olsr_cnf->ipsize);
  myState->CoreAddress = myState->myipaddr;

  myState->DataSequenceNumber = 0;
  myState->TreeCreateSequenceNumber = 0;
  myState->tree_req_sn = 0;     //TODO: start from random number ?

  memset(&myState->ParentId.v4, 0, sizeof(myState->ParentId.v4));
  memset(&myState->OldParentId.v4, 1, sizeof(myState->OldParentId.v4));

  OLSR_DEBUG(LOG_PLUGINS, "SETUP: my IP - %s", ip4_to_string(&buf, myState->myipaddr.v4));

  list_init_head(&ListOfObampNodes);

//OLSR cookies stuff for timers
  OBAMP_alive_gen_timer_cookie = olsr_alloc_cookie("OBAMP Alive Generation", OLSR_COOKIE_TYPE_TIMER);
  purge_nodes_timer_cookie = olsr_alloc_cookie("purge nodes Generation", OLSR_COOKIE_TYPE_TIMER);
  mesh_create_timer_cookie = olsr_alloc_cookie("mesh create Generation", OLSR_COOKIE_TYPE_TIMER);
  tree_create_timer_cookie = olsr_alloc_cookie("tree create Generation", OLSR_COOKIE_TYPE_TIMER);
  outer_tree_create_timer_cookie = olsr_alloc_cookie("outer tree create Generation", OLSR_COOKIE_TYPE_TIMER);
  unsolicited_tree_destroy_timer_cookie = olsr_alloc_cookie("tree destroy Generation", OLSR_COOKIE_TYPE_TIMER);


//Tells OLSR to launch olsr_parser when the packets for this plugin arrive
  olsr_parser_add_function(&olsr_parser, PARSER_TYPE);

// start to send alive messages to appear in other joined lists
  OBAMP_alive_timer =
    olsr_start_timer(OBAMP_ALIVE_EIVAL * MSEC_PER_SEC, OBAMP_JITTER, OLSR_TIMER_PERIODIC, &obamp_alive_gen, NULL,
                     OBAMP_alive_gen_timer_cookie);

// start timer to purge nodes from list in softstate fashion
  purge_nodes_timer =
    olsr_start_timer(_Texpire_timer_ * MSEC_PER_SEC, OBAMP_JITTER, OLSR_TIMER_PERIODIC, &purge_nodes, NULL,
                     purge_nodes_timer_cookie);

//start timer to create mesh
  mesh_create_timer =
    olsr_start_timer(OBAMP_MESH_CREATE_IVAL * MSEC_PER_SEC, OBAMP_JITTER, OLSR_TIMER_PERIODIC, &mesh_create, NULL,
                     mesh_create_timer_cookie);

//start timer for tree create procedure
  tree_create_timer =
    olsr_start_timer(OBAMP_TREE_CREATE_IVAL * MSEC_PER_SEC, OBAMP_JITTER, OLSR_TIMER_PERIODIC, &tree_create, NULL,
                     tree_create_timer_cookie);

//start timer for tree create procedure
  outer_tree_create_timer =
    olsr_start_timer(OBAMP_OUTER_TREE_CREATE_IVAL * MSEC_PER_SEC, OBAMP_JITTER, OLSR_TIMER_PERIODIC, &outer_tree_create, NULL,
                     tree_create_timer_cookie);

//start timer for tree create procedure
  unsolicited_tree_destroy_timer =
    olsr_start_timer(30 * MSEC_PER_SEC, OBAMP_JITTER, OLSR_TIMER_PERIODIC, &unsolicited_tree_destroy, NULL,
                     unsolicited_tree_destroy_timer_cookie);

//Create udp server socket for OBAMP signalling and register it to the scheduler
  OLSR_DEBUG(LOG_PLUGINS, "Launch Udp Servers");
  if (UdpServer() < 0) {
    OLSR_DEBUG(LOG_PLUGINS, "Problem in Launch Udp Servers");

  };

//Creates a socket for sniffing multicast traffic and registers it to the scheduler
  CreateObampSniffingInterfaces();

  return 0;
}

/* -------------------------------------------------------------------------
 * Function   : CloseOBAMP
 * Description: Close the OBAMP plugin and clean up
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  :
 * ------------------------------------------------------------------------- */
void
CloseOBAMP(void)
{
//do something here
}
