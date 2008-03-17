#ifndef DUPLICATE_SET_2_H_
#define DUPLICATE_SET_2_H_

#include "lq_avl.h"
#include "olsr.h"

struct duplicate_entry {
  struct avl_node avl;
  union olsr_ip_addr ip;
  olsr_u16_t seqnr;
  olsr_u16_t too_low_counter;
  olsr_u32_t array;
};

void olsr_init_duplicate_set(void);
struct duplicate_entry *olsr_create_duplicate_entry(void *ip, olsr_u16_t seqnr);
int olsr_shall_process_message(void *ip, olsr_u16_t seqnr);
void olsr_print_duplicate_table(void);

#endif /*DUPLICATE_SET_2_H_*/
