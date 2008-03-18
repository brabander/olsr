#ifndef LQ_PLUGIN_DEFAULT_H_
#define LQ_PLUGIN_DEFAULT_H_

#include "olsr_types.h"

#define LQ_PLUGIN_LC_MULTIPLIER 1024
#define LQ_PLUGIN_RELEVANT_COSTCHANGE 16

struct default_lq {
	float lq, nlq;
};

olsr_linkcost default_calc_cost(const void *lq);

olsr_bool default_olsr_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2);

olsr_linkcost default_packet_loss_worker(void *lq, olsr_bool lost);
void default_olsr_memorize_foreign_hello_lq(void *local, void *foreign);

int default_olsr_serialize_hello_lq_pair(unsigned char *buff, void *lq);
void default_olsr_deserialize_hello_lq_pair(const olsr_u8_t **curr, void *lq);
int default_olsr_serialize_tc_lq_pair(unsigned char *buff, void *lq);
void default_olsr_deserialize_tc_lq_pair(const olsr_u8_t **curr, void *lq);

void default_olsr_copy_link_lq_into_tc(void *target, void *source);
void default_olsr_clear_lq(void *target);

char *default_olsr_print_lq(void *ptr);
char *default_olsr_print_cost(olsr_linkcost cost);

#endif /*LQ_PLUGIN_DEFAULT_H_*/
