/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: socket_parser.c,v 1.12 2004/11/12 20:17:59 kattemat Exp $
 *
 */

#include <unistd.h>
#include "socket_parser.h"
#include "olsr.h"
#include "defs.h"

#ifdef WIN32
#undef EINTR
#define EINTR WSAEINTR
#undef errno
#define errno WSAGetLastError()
#undef strerror
#define strerror(x) StrError(x)
#endif


static int hfd = 0;

#warning highest FD for select is now set in socket add/remove functions

/**
 * Add a socket and handler to the socketset
 * beeing used in the main select(2) loop
 * in listen_loop
 *
 *@param fd the socket
 *@param pf the processing function
 */
void
add_olsr_socket(int fd, void(*pf)(int))
{
  struct olsr_socket_entry *new_entry;

  if((fd == 0) || (pf == NULL))
    {
      fprintf(stderr, "Bogus socket entry - not registering...\n");
      return;
    }
  olsr_printf(1, "Adding OLSR socket entry %d\n", fd);

  new_entry = olsr_malloc(sizeof(struct olsr_socket_entry), "Socket entry");

  new_entry->fd = fd;
  new_entry->process_function = pf;

  /* Queue */
  new_entry->next = olsr_socket_entries;
  olsr_socket_entries = new_entry;

  if(fd + 1 > hfd)
    hfd = fd + 1;
}

/**
 * Remove a socket and handler to the socketset
 * beeing used in the main select(2) loop
 * in listen_loop
 *
 *@param fd the socket
 *@param pf the processing function
 */
int
remove_olsr_socket(int fd, void(*pf)(int))
{
  struct olsr_socket_entry *entry, *prev_entry;

  if((fd == 0) || (pf == NULL))
    {
      olsr_syslog(OLSR_LOG_ERR, "Bogus socket entry - not processing...\n");
      return 0;
    }
  olsr_printf(1, "Removing OLSR socket entry %d\n", fd);

  entry = olsr_socket_entries;
  prev_entry = NULL;

  while(entry)
    {
      if((entry->fd == fd) && (entry->process_function == pf))
	{
	  if(prev_entry == NULL)
	    {
	      olsr_socket_entries = entry->next;
	      free(entry);
	    }
	  else
	    {
	      prev_entry->next = entry->next;
	      free(entry);
	    }

	  if(hfd == fd + 1)
	    {
	      /* Re-calculate highest FD */
	      entry = olsr_socket_entries;
	      hfd = 0;
	      while(entry)
		{
		  if(entry->fd + 1 > hfd)
		    hfd = entry->fd + 1;
		  entry = entry->next;
		}
	    }
	  return 1;
	}
      prev_entry = entry;
      entry = entry->next;
    }

  return 0;
}





void
listen_loop()
{
  fd_set ibits;
  int n;
  struct olsr_socket_entry *olsr_sockets;
  struct timeval tvp;

  FD_ZERO(&ibits);

  /* Main listening loop */
  for (;;)
    {
      FD_ZERO(&ibits);
      /* Adding file-descriptors to FD set */
      /* Begin critical section */
      pthread_mutex_lock(&mutex);
      olsr_sockets = olsr_socket_entries;
      while(olsr_sockets)
	{
	  FD_SET(olsr_sockets->fd, &ibits);
	  olsr_sockets = olsr_sockets->next;
	}
      /* End critical section */
      pthread_mutex_unlock(&mutex);


      /* If there are no registered sockets we
       * do not call select(2)
       */
      if (hfd == 0)
	{
	  sleep(OLSR_SELECT_TIMEOUT);
	  continue;
	}

      /* Add timeout to ensure update */
      tvp.tv_sec = OLSR_SELECT_TIMEOUT;
      tvp.tv_usec = 0;
      
      /* Runnig select on the FD set */
      n = select(hfd, &ibits, 0, 0, &tvp);
      
      /* Did somethig go wrong? */
      if (n <= 0) 
	{
	  if (n < 0) 
	    {
	      if (errno == EINTR)
		continue;
	      olsr_syslog(OLSR_LOG_ERR, "select: %m");
	      olsr_printf(1, "Error select: %s", strerror(errno));
	    }
	  continue;
	}

      gettimeofday(&now, NULL);      
      
      /* Begin critical section */
      pthread_mutex_lock(&mutex);
      olsr_sockets = olsr_socket_entries;
      while(olsr_sockets)
	{
	  if(FD_ISSET(olsr_sockets->fd, &ibits))
	    {
	      olsr_sockets->process_function(olsr_sockets->fd);
	    }
	  olsr_sockets = olsr_sockets->next;
	}
      /* End critical section */
      pthread_mutex_unlock(&mutex);
  

    } /* for(;;) */
	
} /* main */



