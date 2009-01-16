
/*
 * olsr_ip_prefix_list.h
 *
 *  Created on: 06.01.2009
 *      Author: henning
 */

#ifndef OLSR_IP_PREFIX_LIST_H_
#define OLSR_IP_PREFIX_LIST_H_

#include "defs.h"
#include "olsr_types.h"
#include "common/list.h"

struct ip_prefix_entry {
  struct list_node node;
  struct olsr_ip_prefix net;
};

/* inline to recast from node back to ip_prefix_entry */
LISTNODE2STRUCT(list2ipprefix, struct ip_prefix_entry, node);

/* deletion safe macro for ip_prefix traversal */
#define OLSR_FOR_ALL_IPPREFIX_ENTRIES(ipprefix_head, ipprefix_node) \
{ \
  struct list_node *link_head_node, *link_node, *next_link_node; \
  link_head_node = ipprefix_head; \
  for (link_node = link_head_node->next; \
    link_node != link_head_node; link_node = next_link_node) { \
    next_link_node = link_node->next; \
    ipprefix_node = list2ipprefix(link_node);
#define OLSR_FOR_ALL_IPPREFIX_ENTRIES_END() }}

//struct ip_prefix_list {
//  struct olsr_ip_prefix net;
//  struct ip_prefix_list *next;
//};

/*
 * List functions
 */
void EXPORT(ip_prefix_list_add) (struct list_node *, const union olsr_ip_addr *, uint8_t);
int EXPORT(ip_prefix_list_remove) (struct list_node *, const union olsr_ip_addr *, uint8_t, int);
void ip_prefix_list_flush (struct list_node *);
struct ip_prefix_entry *ip_prefix_list_find(struct list_node *, const union olsr_ip_addr *, uint8_t, int);


#endif /* OLSR_IP_PREFIX_LIST_H_ */
