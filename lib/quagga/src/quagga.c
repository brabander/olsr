/*
 * OLSRd Quagga plugin
 *
 * Copyright (C) 2006-2008 Immo 'FaUl' Wehrenberg <immo@chaostreff-dortmund.de>
 * Copyright (C) 2007-2010 Vasilis Tsiligiannis <acinonyxs@yahoo.gr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation or - at your option - under
 * the terms of the GNU General Public Licence version 2 but can be
 * linked to any BSD-Licenced Software with public available sourcecode
 *
 */

/* -------------------------------------------------------------------------
 * File               : quagga.c
 * Description        : functions to interface to the zebra daemon
 * ------------------------------------------------------------------------- */

#define HAVE_SOCKLEN_T

#include "quagga.h"
#include "olsr.h" /* olsr_exit
                     olsr_malloc */
#include "log.h" /* olsr_syslog */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>

/* prototypes intern */
static struct {
  char status;                         // internal status
  char options;                        // internal options
  int sock;                            // Socket to zebra...
  char redistribute[ZEBRA_ROUTE_MAX];
  char distance;
  char flags;
  char *sockpath;
  unsigned int port;
  char version;
} zebra;

static void *my_realloc(void *, size_t, const char *);
static void zebra_connect(void);
static unsigned char *try_read(ssize_t *);
static int zebra_send_command(unsigned char *);
static unsigned char *zebra_route_packet(uint16_t, struct zebra_route *);
static unsigned char *zebra_redistribute_packet(uint16_t, unsigned char);
static void zebra_enable_redistribute(void);
static void zebra_disable_redistribute(void);
static struct zebra_route *zebra_parse_route(unsigned char *);
static void zebra_reconnect(void);
static void free_ipv4_route(struct zebra_route *);

static void *
my_realloc(void *buf, size_t s, const char *c)
{
  buf = realloc(buf, s);
  if (!buf) {
    OLSR_PRINTF(1, "(QUAGGA) OUT OF MEMORY: %s\n", strerror(errno));
    olsr_syslog(OLSR_LOG_ERR, "olsrd: out of memory!: %m\n");
    olsr_exit(c, EXIT_FAILURE);
  }
  return buf;
}

void
init_zebra(void)
{

  memset(&zebra, 0, sizeof zebra);
  zebra.sockpath = olsr_malloc(sizeof ZEBRA_SOCKPATH  + 1, "zebra_sockpath");
  strscpy(zebra.sockpath, ZEBRA_SOCKPATH, sizeof ZEBRA_SOCKPATH);

}

void
zebra_cleanup(void)
{
  struct rt_entry *tmp;

  if (zebra.options & OPTION_EXPORT) {
    OLSR_FOR_ALL_RT_ENTRIES(tmp) {
      zebra_del_route(tmp);
    }
    OLSR_FOR_ALL_RT_ENTRIES_END(tmp);
  }
  zebra_disable_redistribute();
  free(zebra.sockpath);

}

static void
zebra_reconnect(void)
{
  struct rt_entry *tmp;

  zebra_connect();
  if (!(zebra.status & STATUS_CONNECTED))
    return;                     // try again next time

  if (zebra.options & OPTION_EXPORT) {
    OLSR_FOR_ALL_RT_ENTRIES(tmp) {
      zebra_add_route(tmp);
    }
    OLSR_FOR_ALL_RT_ENTRIES_END(tmp);
  }
  zebra_enable_redistribute();

}

/* Connect to the zebra-daemon, returns a socket */
static void
zebra_connect(void)
{

  int ret;
  union {
    struct sockaddr_in sin;
    struct sockaddr_un sun;
  } sockaddr;

  if (close(zebra.sock) < 0)
    olsr_exit("(QUAGGA) Could not close socket!", EXIT_FAILURE);
  zebra.sock = socket(zebra.port ? AF_INET : AF_UNIX, SOCK_STREAM, 0);

  if (zebra.sock < 0)
    olsr_exit("(QUAGGA) Could not create socket!", EXIT_FAILURE);

  memset(&sockaddr, 0, sizeof sockaddr);

  if (zebra.port) {
    sockaddr.sin.sin_family = AF_INET;
    sockaddr.sin.sin_port = htons(zebra.port);
    sockaddr.sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ret = connect(zebra.sock, (struct sockaddr *)&sockaddr.sin, sizeof sockaddr.sin);
  } else {
    sockaddr.sun.sun_family = AF_UNIX;
    strscpy(sockaddr.sun.sun_path, zebra.sockpath, sizeof(sockaddr.sun.sun_path));
    ret = connect(zebra.sock, (struct sockaddr *)&sockaddr.sun, sizeof sockaddr.sun);
  }

  if (ret < 0)
    zebra.status &= ~STATUS_CONNECTED;
  else
    zebra.status |= STATUS_CONNECTED;
}

/* Sends a command to zebra, command is
   the command defined in zebra.h, options is the packet-payload,
   optlen the length, of the payload */
static int
zebra_send_command(unsigned char *options)
{

  unsigned char *pnt;
  uint16_t len;
  int ret;

  if (!(zebra.status & STATUS_CONNECTED))
    return 0;

  pnt = options;
  memcpy(&len, pnt, 2);

  len = ntohs(len);

  do {
    ret = write(zebra.sock, pnt, len);
    if (ret < 0) {
      if ((errno == EINTR) || (errno == EAGAIN)) {
        errno = 0;
        ret = 0;
        continue;
      } else {
        OLSR_PRINTF(1, "(QUAGGA) Disconnected from zebra\n");
        zebra.status &= ~STATUS_CONNECTED;
        /* TODO: Remove HNAs added from redistribution */
        free(options);
        return -1;
      }
    }
    pnt = pnt + ret;
  }
  while ((len -= ret));
  free(options);
  return 0;
}

/* Creates a Route-Packet-Payload, needs address, netmask, nexthop,
   distance, and a pointer of an size_t */
static unsigned char *
zebra_route_packet(uint16_t cmd, struct zebra_route *r)
{

  int count;
  uint8_t len;
  uint16_t size;
  uint32_t ind, metric;

  unsigned char *cmdopt, *t;

  cmdopt = olsr_malloc(ZEBRA_MAX_PACKET_SIZ, "zebra add_v4_route");

  t = &cmdopt[2];
  if (zebra.version) {
    *t++ = ZEBRA_HEADER_MARKER;
    *t++ = zebra.version;
    cmd = htons(cmd);
    memcpy(t, &cmd, sizeof cmd);
    t += sizeof cmd;
  } else
      *t++ = (unsigned char) cmd;
  *t++ = r->type;
  *t++ = r->flags;
  *t++ = r->message;
  *t++ = r->prefixlen;
  len = (r->prefixlen + 7) / 8;
  memcpy(t, &r->prefix.v4.s_addr, len);
  t = t + len;

  if (r->message & ZAPI_MESSAGE_NEXTHOP) {
    *t++ = r->nexthop_num + r->ifindex_num;

      for (count = 0; count < r->nexthop_num; count++) {
        *t++ = ZEBRA_NEXTHOP_IPV4;
        memcpy(t, &r->nexthop[count].v4.s_addr, sizeof r->nexthop[count].v4.s_addr);
        t += sizeof r->nexthop[count].v4.s_addr;
      }
      for (count = 0; count < r->ifindex_num; count++) {
        *t++ = ZEBRA_NEXTHOP_IFINDEX;
        ind = htonl(r->ifindex[count]);
        memcpy(t, &ind, sizeof ind);
        t += sizeof ind;
      }
  }
  if ((r->message & ZAPI_MESSAGE_DISTANCE) > 0)
    *t++ = r->distance;
  if ((r->message & ZAPI_MESSAGE_METRIC) > 0) {
    metric = htonl(r->metric);
    memcpy(t, &metric, sizeof metric);
    t += sizeof metric;
  }
  size = htons(t - cmdopt);
  memcpy(cmdopt, &size, 2);

  return cmdopt;
}

/* Check wether there is data from zebra aviable */
void
zebra_parse(void *foo __attribute__ ((unused)))
{
  unsigned char *data, *f;
  uint16_t command;
  uint16_t length;
  ssize_t len;
  struct zebra_route *route;

  if (!(zebra.status & STATUS_CONNECTED)) {
    zebra_reconnect();
    return;
  }
  data = try_read(&len);
  if (data) {
    f = data;
    do {
      memcpy(&length, f, sizeof length);
      length = ntohs (length);
      if (!length) // something wired happened
        olsr_exit("(QUAGGA) Zero message length??? ", EXIT_FAILURE);
      if (zebra.version) {
        if ((f[2] != ZEBRA_HEADER_MARKER) || (f[3] != zebra.version))
          olsr_exit("(QUAGGA) Invalid zebra header received!", EXIT_FAILURE);
        memcpy(&command, &f[4], sizeof command);
        command = ntohs (command);
      } else
          command = f[2];
OLSR_PRINTF(1, "(QUAGGA) DEBUG: %d\n", command);
      switch (command) {
        case ZEBRA_IPV4_ROUTE_ADD:
          route = zebra_parse_route(f);
          ip_prefix_list_add(&olsr_cnf->hna_entries, &route->prefix, route->prefixlen);
          free_ipv4_route(route);
          free(route);
          break;
        case ZEBRA_IPV4_ROUTE_DELETE:
          route = zebra_parse_route(f);
          ip_prefix_list_remove(&olsr_cnf->hna_entries, &route->prefix, route->prefixlen);
          free_ipv4_route(route);
          free(route);
          break;
        default:
          break;
      }
      f += length;
    }
    while ((f - data) < len);
    free(data);
  }
}

// tries to read a packet from zebra_socket
// if there is something to read - make sure to read whole packages
static unsigned char *
try_read(ssize_t * size)
{
  unsigned char *buf;
  ssize_t bytes, bufsize;
  uint16_t length, offset;
  int sockstatus;

  /* initialize variables */
  buf = NULL;
  offset = 0;
  *size = 0;
  bufsize = 0;

  /* save socket status and set non-blocking for read */
  sockstatus = fcntl(zebra.sock, F_GETFL);
  fcntl(zebra.sock, F_SETFL, sockstatus|O_NONBLOCK);

  /* read whole packages */
  do {

    /* (re)allocate buffer */
    if (*size == bufsize) {
      bufsize += BUFSIZE;
      buf = my_realloc(buf, bufsize, "Zebra try_read");
    }

    /* read from socket */
    bytes = read(zebra.sock, buf + *size, bufsize - *size);
    /* handle broken packet */
    if (!bytes) {
      free(buf);
      return NULL;
    }
    /* handle no data available */
    if (bytes < 0) {
      /* handle disconnect */
      if (errno != EAGAIN) {    // oops - we got disconnected
        OLSR_PRINTF(1, "(QUAGGA) Disconnected from zebra\n");
        zebra.status &= ~STATUS_CONNECTED;
        /* TODO: Remove HNAs added from redistribution */
      }
      free(buf);
      return NULL;
    }

    *size += bytes;

    /* detect zebra packet fragmentation */
    do {
      memcpy(&length, buf + offset, sizeof length);
      length = ntohs(length);
      offset += length;
    }
    while (*size >= (ssize_t) (offset + sizeof length));
    /* set blocking socket on fragmented packet */
    if (*size != offset)
      fcntl(zebra.sock, F_SETFL, sockstatus);

  }
  while (*size != offset);

  /* restore socket status */
  fcntl(zebra.sock, F_SETFL, sockstatus);

  return buf;
}

/* Parse an ipv4-route-packet recived from zebra
 */
static struct zebra_route
*zebra_parse_route(unsigned char *opt)
{
  struct zebra_route *r;
  int c;
  size_t size;
  uint16_t length;
  unsigned char *pnt;

  memcpy(&length, opt, sizeof length);
  length = ntohs (length);

  r = olsr_malloc(sizeof *r, "zebra_parse_route");
  pnt = (zebra.version ? &opt[6] : &opt[3]);
  r->type = *pnt++;
  r->flags = *pnt++;
  r->message = *pnt++;
  r->prefixlen = *pnt++;
  r->prefix.v4.s_addr = 0;

  size = (r->prefixlen + 7) / 8;
  memcpy(&r->prefix.v4.s_addr, pnt, size);
  pnt += size;

  switch (zebra.version) {
    case 0:
    case 1:
      if (r->message & ZAPI_MESSAGE_NEXTHOP) {
        r->nexthop_num = *pnt++;
        r->nexthop = olsr_malloc((sizeof *r->nexthop) * r->nexthop_num, "quagga: zebra_parse_route");
        for (c = 0; c < r->nexthop_num; c++) {
          memcpy(&r->nexthop[c].v4.s_addr, pnt, sizeof r->nexthop[c].v4.s_addr);
          pnt += sizeof r->nexthop[c].v4.s_addr;
        }
      }

      if (r->message & ZAPI_MESSAGE_IFINDEX) {
        r->ifindex_num = *pnt++;
        r->ifindex = olsr_malloc(sizeof(uint32_t) * r->ifindex_num, "quagga: zebra_parse_route");
        for (c = 0; c < r->ifindex_num; c++) {
          memcpy(&r->ifindex[c], pnt, sizeof r->ifindex[c]);
          r->ifindex[c] = ntohl (r->ifindex[c]);
          pnt += sizeof r->ifindex[c];
        }
      }
      break;
    default:
      OLSR_PRINTF(1, "(QUAGGA) Unsupported zebra packet version!\n");
      break;
  }

  if (r->message & ZAPI_MESSAGE_DISTANCE) {
    r->distance = *pnt++;
  }

// Quagga v0.98.6 BUG workaround: metric is always sent by zebra
// even without ZAPI_MESSAGE_METRIC message.
//  if (r.message & ZAPI_MESSAGE_METRIC) {
    memcpy(&r->metric, pnt, sizeof (uint32_t));
    r->metric = ntohl(r->metric);
    pnt += sizeof r->metric;
//  }

  if (pnt - opt != length) {
    olsr_exit("(QUAGGA) length does not match ??? ", EXIT_FAILURE);
  }

  return r;
}

static unsigned char
*zebra_redistribute_packet (uint16_t cmd, unsigned char type)
{
  unsigned char *data, *pnt;
  uint16_t size;

  data = olsr_malloc(ZEBRA_MAX_PACKET_SIZ , "zebra_redistribute_packet");

  pnt = &data[2];
  if (zebra.version) {
    *pnt++ = ZEBRA_HEADER_MARKER;
    *pnt++ = zebra.version;
    cmd = htons(cmd);
    memcpy(pnt, &cmd, sizeof cmd);
    pnt += sizeof cmd;
  } else
      *pnt++ = (unsigned char) cmd;
  *pnt++ = type;
  size = htons(pnt - data);
  memcpy(data, &size, 2);

  return data;
}

/* start redistribution FROM zebra */
int
zebra_redistribute(unsigned char type)
{

  if (type > ZEBRA_ROUTE_MAX - 1)
    return -1;
  zebra.redistribute[type] = 1;

  return 0;

}

/* start redistribution FROM zebra */
static void
zebra_enable_redistribute(void)
{
  unsigned char type;

  for (type = 0; type < ZEBRA_ROUTE_MAX; type++)
    if (zebra.redistribute[type]) {
      if (zebra_send_command(zebra_redistribute_packet(ZEBRA_REDISTRIBUTE_ADD, type)) < 0)
        olsr_exit("(QUAGGA) could not send redistribute add command", EXIT_FAILURE);
    }

}

/* end redistribution FROM zebra */
static void
zebra_disable_redistribute(void)
{
  unsigned char type;

  for (type = 0; type < ZEBRA_ROUTE_MAX; type++)
    if (zebra.redistribute[type]) {
      if (zebra_send_command(zebra_redistribute_packet(ZEBRA_REDISTRIBUTE_DELETE, type)) < 0)
        olsr_exit("(QUAGGA) could not send redistribute delete command", EXIT_FAILURE);
    }

}

static void
free_ipv4_route(struct zebra_route *r)
{

  if(r->ifindex_num)
    free(r->ifindex);
  if(r->nexthop_num)
    free(r->nexthop);

}

int
zebra_add_route(const struct rt_entry *r)
{

  struct zebra_route route;
  int retval;

  route.distance = 0;
  route.type = ZEBRA_ROUTE_OLSR;
  route.flags = zebra.flags;
  route.message = ZAPI_MESSAGE_NEXTHOP | ZAPI_MESSAGE_METRIC;
  route.prefixlen = r->rt_dst.prefix_len;
  route.prefix.v4.s_addr = r->rt_dst.prefix.v4.s_addr;
  route.ifindex_num = 0;
  route.ifindex = NULL;
  route.nexthop_num = 0;
  route.nexthop = NULL;

  if (r->rt_best->rtp_nexthop.gateway.v4.s_addr == r->rt_dst.prefix.v4.s_addr && route.prefixlen == 32) {
    return 0;			/* Quagga BUG workaround: don't add routes with destination = gateway
				   see http://lists.olsr.org/pipermail/olsr-users/2006-June/001726.html */
    route.ifindex_num++;
    route.ifindex = olsr_malloc(sizeof *route.ifindex, "zebra_add_route");
    *route.ifindex = r->rt_best->rtp_nexthop.iif_index;
  } else {
    route.nexthop_num++;
    route.nexthop = olsr_malloc(sizeof *route.nexthop, "zebra_add_route");
    route.nexthop->v4.s_addr = r->rt_best->rtp_nexthop.gateway.v4.s_addr;
  }

  route.metric = r->rt_best->rtp_metric.hops;

  if (zebra.distance) {
    route.message |= ZAPI_MESSAGE_DISTANCE;
    route.distance = zebra.distance;
  }

  retval = zebra_send_command(zebra_route_packet(ZEBRA_IPV4_ROUTE_ADD, &route));

  return retval;
}

int
zebra_del_route(const struct rt_entry *r)
{

  struct zebra_route route;
  int retval;
  route.distance = 0;
  route.type = ZEBRA_ROUTE_OLSR;
  route.flags = zebra.flags;
  route.message = ZAPI_MESSAGE_NEXTHOP | ZAPI_MESSAGE_METRIC;
  route.prefixlen = r->rt_dst.prefix_len;
  route.prefix.v4.s_addr = r->rt_dst.prefix.v4.s_addr;
  route.ifindex_num = 0;
  route.ifindex = NULL;
  route.nexthop_num = 0;
  route.nexthop = NULL;

  if (r->rt_nexthop.gateway.v4.s_addr == r->rt_dst.prefix.v4.s_addr && route.prefixlen == 32) {
    return 0;			/* Quagga BUG workaround: don't delete routes with destination = gateway
				   see http://lists.olsr.org/pipermail/olsr-users/2006-June/001726.html */
    route.ifindex_num++;
    route.ifindex = olsr_malloc(sizeof *route.ifindex, "zebra_del_route");
    *route.ifindex = r->rt_nexthop.iif_index;
  } else {
    route.nexthop_num++;
    route.nexthop = olsr_malloc(sizeof *route.nexthop, "zebra_del_route");
    route.nexthop->v4.s_addr = r->rt_nexthop.gateway.v4.s_addr;
  }

  route.metric = 0;

  if (zebra.distance) {
    route.message |= ZAPI_MESSAGE_DISTANCE;
    route.distance = zebra.distance;
  }

  retval = zebra_send_command(zebra_route_packet(ZEBRA_IPV4_ROUTE_DELETE, &route));

  return retval;
}

void
zebra_olsr_distance(unsigned char dist)
{
  zebra.distance = dist;
}

void
zebra_olsr_localpref(void)
{
  zebra.flags &= ZEBRA_FLAG_SELECTED;
}

void
zebra_export_routes(unsigned char t)
{
  if (t)
    zebra.options |= OPTION_EXPORT;
  else
    zebra.options &= ~OPTION_EXPORT;
}

void
zebra_sockpath(char *sockpath)
{
  size_t len;

  len = strlen(sockpath) + 1;
  zebra.sockpath = my_realloc(zebra.sockpath, len, "zebra_sockpath");
  memcpy(zebra.sockpath, sockpath, len);

}

void
zebra_port(unsigned int port)
{

  zebra.port = port;

}

void
zebra_version(char version)
{

  zebra.version = version;

}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
