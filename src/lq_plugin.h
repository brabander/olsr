#ifndef LQPLUGIN_H_
#define LQPLUGIN_H_

#include "tc_set.h"
#include "link_set.h"
#include "lq_route.h"
#include "lq_packet.h"
#include "packet.h"

#define LINK_COST_BROKEN 65535
#define ROUTE_COST_BROKEN (0xffffffff)
#define ZERO_ROUTE_COST 0

struct lq_handler {
  olsr_linkcost (*calc_hello_cost)(const void *lq);
  olsr_linkcost (*calc_tc_cost)(const void *lq);
  
  olsr_bool (*is_relevant_costchange)(olsr_linkcost c1, olsr_linkcost c2);
  
  olsr_linkcost (*packet_loss_handler)(void *lq, olsr_bool lost);
  
  void (*memorize_foreign_hello)(void *local, void *foreign);
  void (*copy_link_lq_into_tc)(void *target, void *source);
  void (*clear_hello)(void *target);
  void (*clear_tc)(void *target);
  
  int (*serialize_hello_lq)(unsigned char *buff, void *lq);
  int (*serialize_tc_lq)(unsigned char *buff, void *lq);
  void (*deserialize_hello_lq)(const olsr_u8_t **curr, void *lq);
  void (*deserialize_tc_lq)(const olsr_u8_t **curr, void *lq);
  
  char *(*print_hello_lq)(void *ptr);
  char *(*print_tc_lq)(void *ptr);
  char *(*print_cost)(olsr_linkcost cost);
  
  size_t hello_lq_size;
  size_t tc_lq_size;
};

void set_lq_handler(struct lq_handler *handler, char *name);

olsr_linkcost olsr_calc_tc_cost(const struct tc_edge_entry *);
olsr_bool olsr_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2);

int olsr_serialize_hello_lq_pair(unsigned char *buff, struct lq_hello_neighbor *neigh);
void olsr_deserialize_hello_lq_pair(const olsr_u8_t **curr, struct hello_neighbor *neigh);
int olsr_serialize_tc_lq_pair(unsigned char *buff, struct tc_mpr_addr *neigh);
void olsr_deserialize_tc_lq_pair(const olsr_u8_t **curr, struct tc_edge_entry *edge);

void olsr_update_packet_loss_worker(struct link_entry *entry, olsr_bool lost);
void olsr_memorize_foreign_hello_lq(struct link_entry *local, struct hello_neighbor *foreign);

char *get_link_entry_text(struct link_entry *entry);
char *get_tc_edge_entry_text(struct tc_edge_entry *entry);
const char *get_linkcost_text(olsr_linkcost cost, olsr_bool route);

void olsr_copy_hello_lq(struct lq_hello_neighbor *target, struct link_entry *source);
void olsr_copylq_link_entry_2_tc_mpr_addr(struct tc_mpr_addr *target, struct link_entry *source);
void olsr_copylq_link_entry_2_tc_edge_entry(struct tc_edge_entry *target, struct link_entry *source);
void olsr_clear_tc_lq(struct tc_mpr_addr *target);

struct hello_neighbor *olsr_malloc_hello_neighbor(const char *id);
struct tc_mpr_addr *olsr_malloc_tc_mpr_addr(const char *id);
struct lq_hello_neighbor *olsr_malloc_lq_hello_neighbor(const char *id);
struct link_entry *olsr_malloc_link_entry(const char *id);
struct tc_edge_entry *olsr_malloc_tc_edge_entry(const char *id);

#endif /*LQPLUGIN_H_*/
