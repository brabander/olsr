/*************************************************************
 *                 TOOLS COPIED FROM OLSRD                   *
 *************************************************************/

// these functions are copied from the main olsrd source
// TODO: there must be a better way!!!

#include "olsrd_plugin.h"

/**
 *Hashing function. Creates a key based on
 *an 32-bit address.
 *@param address the address to hash
 *@return the hash(a value in the 0-31 range)
 */
olsr_u32_t
olsr_hashing(union olsr_ip_addr *address)
{
  olsr_u32_t hash;
  char *tmp;

  if(ipversion == AF_INET)
    /* IPv4 */  
    hash = (ntohl(address->v4));
  else
    {
      /* IPv6 */
      tmp = (char *) &address->v6;
      hash = (ntohl(*tmp));
    }

  //hash &= 0x7fffffff; 
  hash &= HASHMASK;

  return hash;
}



/**
 *Checks if a timer has times out. That means
 *if it is smaller than present time.
 *@param timer the timeval struct to evaluate
 *@return positive if the timer has not timed out,
 *0 if it matches with present time and negative
 *if it is timed out.
 */
int
olsr_timed_out(struct timeval *timer)
{
  return(timercmp(timer, now, <));
}



/**
 *Initiates a "timer", wich is a timeval structure,
 *with the value given in time_value.
 *@param time_value the value to initialize the timer with
 *@param hold_timer the timer itself
 *@return nada
 */
void
olsr_init_timer(olsr_u32_t time_value, struct timeval *hold_timer)
{ 
  olsr_u16_t  time_value_sec;
  olsr_u16_t  time_value_msec;

  time_value_sec = time_value/1000;
  time_value_msec = time_value-(time_value_sec*1000);

  hold_timer->tv_sec = time_value_sec;
  hold_timer->tv_usec = time_value_msec*1000;   
}





/**
 *Generaties a timestamp a certain number of milliseconds
 *into the future.
 *
 *@param time_value how many milliseconds from now
 *@param hold_timer the timer itself
 *@return nada
 */
void
olsr_get_timestamp(olsr_u32_t delay, struct timeval *hold_timer)
{ 
  olsr_u16_t  time_value_sec;
  olsr_u16_t  time_value_msec;

  time_value_sec = delay/1000;
  time_value_msec= delay - (delay*1000);

  hold_timer->tv_sec = now->tv_sec + time_value_sec;
  hold_timer->tv_usec = now->tv_usec + (time_value_msec*1000);   
}


/**
 *Converts a olsr_ip_addr to a string
 *Goes for both IPv4 and IPv6
 *
 *NON REENTRANT! If you need to use this
 *function twice in e.g. the same printf
 *it will not work.
 *You must use it in different calls e.g.
 *two different printfs
 *
 *@param the IP to convert
 *@return a pointer to a static string buffer
 *representing the address in "dots and numbers"
 *
 */
char *
olsr_ip_to_string(union olsr_ip_addr *addr)
{

  char *ret;
  struct in_addr in;
  
  if(ipversion == AF_INET)
    {
      in.s_addr=addr->v4;
      ret = inet_ntoa(in);
    }
  else
    {
      /* IPv6 */
      ret = (char *)inet_ntop(AF_INET6, &addr->v6, ipv6_buf, sizeof(ipv6_buf));
    }

  return ret;
}
