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

#ifndef _OLSR_GEN_MSG
#define _OLSR_GEN_MSG

#include "interfaces.h"

/* Functions */

int
olsr_set_hello_interval(float);

int
olsr_set_hello_nw_interval(float);

int
olsr_set_tc_interval(float);

int
olsr_set_mid_interval(float);

int
olsr_set_hna_interval(float);

void
generate_hello();

void
generate_hello_nw();

void
generate_mid();

void
generate_hna();

void
generate_tc();

void
generate_tabledisplay();

#endif
