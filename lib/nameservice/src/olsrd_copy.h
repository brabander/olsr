#ifndef _OLSRD_COPY
#define _OLSRD_COPY

// these functions are copied from the main olsrd source
// TODO: there must be a better way!!!

olsr_u32_t olsr_hashing(union olsr_ip_addr *address);

int olsr_timed_out(struct timeval *timer);

void olsr_init_timer(olsr_u32_t time_value, struct timeval *hold_timer);

void olsr_get_timestamp(olsr_u32_t delay, struct timeval *hold_timer);

char * olsr_ip_to_string(union olsr_ip_addr *addr);

#endif
