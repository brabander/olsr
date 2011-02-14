
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include "common/avl.h"
#include "common/avl_olsr_comp.h"
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "link_set.h"
#include "olsr.h"
#include "olsr_memcookie.h"
#include "os_net.h"
#include "os_time.h"
#include "olsr_logging.h"

/* Head of all OLSR used sockets */
struct list_entity socket_head;

void
olsr_socket_init(void) {
  list_init_head(&socket_head);
}

/**
 * Close and free all sockets.
 */
void
olsr_socket_cleanup(void)
{
  struct olsr_socket_entry *entry, *iterator;

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    os_close(entry->fd);
    list_remove(&entry->socket_node);
    free(entry);
  }
}

/**
 * Add a socket and handler to the socketset
 * beeing used in the main select(2) loop
 * in listen_loop
 *
 *@param fd the socket
 *@param pf the processing function
 */
void
olsr_socket_add(int fd, socket_handler_func pf_imm, void *data, unsigned int flags)
{
  struct olsr_socket_entry *new_entry;

  if (fd < 0 || pf_imm == NULL) {
    OLSR_WARN(LOG_SCHEDULER, "Bogus socket entry - not registering...");
    return;
  }
  OLSR_DEBUG(LOG_SCHEDULER, "Adding OLSR socket entry %d\n", fd);

  new_entry = olsr_malloc(sizeof(*new_entry), "Socket entry");

  new_entry->fd = fd;
  new_entry->process_immediate = pf_imm;
  new_entry->data = data;
  new_entry->flags = flags;

  /* Queue */
  list_add_before(&socket_head, &new_entry->socket_node);
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
olsr_socket_remove(int fd, socket_handler_func pf_imm)
{
  struct olsr_socket_entry *entry, *iterator;

  if (fd < 0 || pf_imm == NULL) {
    OLSR_WARN(LOG_SCHEDULER, "Bogus socket entry - not processing...");
    return 0;
  }
  OLSR_DEBUG(LOG_SCHEDULER, "Removing OLSR socket entry %d\n", fd);

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->fd == fd && entry->process_immediate == pf_imm) {
      /* just mark this node as "deleted", it will be cleared later at the end of handle_fds() */
      entry->process_immediate = NULL;
      entry->flags = 0;
      return 1;
    }
  }
  return 0;
}

void
olsr_socket_enable(int fd, socket_handler_func pf_imm, unsigned int flags)
{
  struct olsr_socket_entry *entry, *iterator;

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->fd == fd && entry->process_immediate == pf_imm) {
      entry->flags |= flags;
    }
  }
}

void
olsr_socket_disable(int fd, socket_handler_func pf_imm, unsigned int flags)
{
  struct olsr_socket_entry *entry, *iterator;

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->fd == fd && entry->process_immediate == pf_imm) {
      entry->flags &= ~flags;
    }
  }
}

void
handle_sockets(uint32_t next_interval)
{
  struct olsr_socket_entry *entry, *iterator;
  struct timeval tvp;
  int32_t remaining;

  remaining = olsr_timer_getRelative(next_interval);
  if (remaining <= 0) {
    /* we are already over the interval */
    if (list_is_empty(&socket_head)) {
      /* If there are no registered sockets we do not call select(2) */
      return;
    }
    tvp.tv_sec = 0;
    tvp.tv_usec = 0;
  } else {
    /* we need an absolute time - milliseconds */
    tvp.tv_sec = remaining / MSEC_PER_SEC;
    tvp.tv_usec = (remaining % MSEC_PER_SEC) * USEC_PER_MSEC;
  }

  /* do at least one select */
  for (;;) {
    fd_set ibits, obits;
    int n, hfd = 0, fdsets = 0;
    FD_ZERO(&ibits);
    FD_ZERO(&obits);

    /* Adding file-descriptors to FD set */
    OLSR_FOR_ALL_SOCKETS(entry, iterator) {
      if (entry->process_immediate == NULL) {
        continue;
      }
      if ((entry->flags & OLSR_SOCKET_READ) != 0) {
        fdsets |= OLSR_SOCKET_READ;
        FD_SET((unsigned int)entry->fd, &ibits);        /* And we cast here since we get a warning on Win32 */
      }
      if ((entry->flags & OLSR_SOCKETPOLL_WRITE) != 0) {
        fdsets |= OLSR_SOCKETPOLL_WRITE;
        FD_SET((unsigned int)entry->fd, &obits);        /* And we cast here since we get a warning on Win32 */
      }
      if ((entry->flags & (OLSR_SOCKET_READ | OLSR_SOCKETPOLL_WRITE)) != 0 && entry->fd >= hfd) {
        hfd = entry->fd + 1;
      }
    }

    if (hfd == 0 && (long)remaining <= 0) {
      /* we are over the interval and we have no fd's. Skip the select() etc. */
      return;
    }

    do {
      n = os_select(hfd, fdsets & OLSR_SOCKET_READ ? &ibits : NULL, fdsets & OLSR_SOCKETPOLL_WRITE ? &obits : NULL, NULL, &tvp);
    } while (n == -1 && errno == EINTR);

    if (n == 0) {               /* timeout! */
      break;
    }
    if (n == -1) {              /* Did something go wrong? */
      OLSR_WARN(LOG_SCHEDULER, "select error: %s", strerror(errno));
      break;
    }

    /* Update time since this is much used by the parsing functions */
    olsr_timer_updateClock();
    OLSR_FOR_ALL_SOCKETS(entry, iterator) {
      int flags;
      if (entry->process_immediate == NULL) {
        continue;
      }
      flags = 0;
      if (FD_ISSET(entry->fd, &ibits)) {
        flags |= OLSR_SOCKET_READ;
      }
      if (FD_ISSET(entry->fd, &obits)) {
        flags |= OLSR_SOCKETPOLL_WRITE;
      }
      if (flags != 0) {
        entry->process_immediate(entry->fd, entry->data, flags);
      }
    }

    /* calculate the next timeout */
    remaining = olsr_timer_getRelative(next_interval);
    if (remaining <= 0) {
      /* we are already over the interval */
      break;
    }
    /* we need an absolute time - milliseconds */
    tvp.tv_sec = remaining / MSEC_PER_SEC;
    tvp.tv_usec = (remaining % MSEC_PER_SEC) * USEC_PER_MSEC;
  }

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->process_immediate == NULL) {
      /* clean up socket handler */
      list_remove(&entry->socket_node);
      free(entry);
    }
  }
}


/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
