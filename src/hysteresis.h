/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2003 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
 *
 * olsrd-unik is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsrd-unik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef _OLSR_HYSTERESIS
#define _OLSR_HYSTERESIS

#include "link_set.h"



inline float
olsr_hyst_calc_stability(float);

inline int
olsr_process_hysteresis(struct link_entry *);

float
olsr_hyst_calc_instability(float);

void
olsr_update_hysteresis_hello(struct link_entry *, double);

void
update_hysteresis_incoming(union olsr_ip_addr *, union olsr_ip_addr *, olsr_u16_t);

extern int
olsr_printf(int, char *, ...);


#endif
