/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org daemon.
 *
 * The olsr.org daemon is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The olsr.org daemon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $ Id $
 *
 */

/*
 * Defines an interface for socket-based IPC to set uOLSRd
 * variables during runtime
 */

#ifndef _IPC_OLSRDSET
#define _IPC_OLSRDSET

/*
 * REQUESTS
 */
#define OLSR_GET_ROUTES    0x01
#define OLSR_GET_VARIABLES 0x02

/*
 * COMMANDS
 */

/* Set intervals */
#define OLSRD_SET_HELLO_INTERVAL  0x11
#define OLSRD_SET_TC_INTERVAL     0x12
#define OLSRD_SET_MID_INTERVAL    0x13
#define OLSRD_SET_HNA_INTERVAL    0x14

/* Multiplier of HELLO emisiion for nonWLAN NICs */
#define OLSRD_SET_HELLO_NW_MULT   0x15

/* Set holding times */
#define OLSRD_SET_HELLO_HOLD      0x21
#define OLSRD_SET_TC_HOLD         0x22
#define OLSRD_SET_MID_HOLD        0x23
#define OLSRD_SET_HNA_HOLD        0x24

/* Set typo of service value */
#define OLSRD_SET_TOS             0x31

/* set debuglevel */
#define OLSRD_SET_DEBUG_LVL       0x40

/*
 * Packet formats
 */

struct olsrset_req
{
  olsr_u8_t req;
}

struct olsrset_cmd
{
  olsr_u8_t cmd;
  u_char      data[16];
}


#endif
