#ifndef _NAMESEVICE_MSG
#define _NAMESEVICE_MSG

struct namemsg
{
  olsr_u8_t name_len;   	// length of the following name filed
  olsr_u8_t pad[3];             // added by the compiler anyway
};

/*
 * OLSR message (several can exist in one OLSR packet)
 */

struct olsrmsg
{
  olsr_u8_t     olsr_msgtype;
  olsr_u8_t     olsr_vtime;
  olsr_u16_t    olsr_msgsize;
  olsr_u32_t    originator;
  olsr_u8_t     ttl;
  olsr_u8_t     hopcnt;
  olsr_u16_t    seqno;

  /* YOUR PACKET GOES HERE */
  struct namemsg msg;
};

/*
 *IPv6
 */

struct olsrmsg6
{
  olsr_u8_t        olsr_msgtype;
  olsr_u8_t        olsr_vtime;
  olsr_u16_t       olsr_msgsize;
  struct in6_addr  originator;
  olsr_u8_t        ttl;
  olsr_u8_t        hopcnt;
  olsr_u16_t       seqno;

  /* YOUR PACKET GOES HERE */
  struct namemsg msg;

};

#endif
