/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
 *
 * UniK olsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UniK olsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef _OLSR_SOCKET_PARSER
#define _OLSR_SOCKET_PARSER

#include <pthread.h>


struct olsr_socket_entry
{
  int fd;
  void(*process_function)(int);
  struct olsr_socket_entry *next;
};

struct olsr_socket_entry *olsr_socket_entries;

pthread_mutex_t mutex; /* Mutex for thread */


void
add_olsr_socket(int, void(*)(int));

int
remove_olsr_socket(int, void(*)(int));

void
listen_loop();

#endif
