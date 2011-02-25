
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
#include <string.h>
#include <errno.h>

#include "common/avl.h"
#include "common/avl_olsr_comp.h"
#include "olsr_logging.h"
#include "olsr_memcookie.h"
#include "os_net.h"
#include "olsr_socket.h"

/* Head of all OLSR used sockets */
struct list_entity socket_head;

static struct olsr_memcookie_info *socket_memcookie;

/* helper function to free socket entry */
static inline void olsr_socket_intfree(struct olsr_socket_entry *sock) {
  list_remove(&sock->node);
  olsr_memcookie_free(socket_memcookie, sock);
}

/**
 * Initialize olsr socket scheduler
 */
void
olsr_socket_init(void) {
  list_init_head(&socket_head);

  socket_memcookie = olsr_memcookie_add("socket entry", sizeof(struct olsr_socket_entry));
}

/**
 * Cleanup olsr socket scheduler.
 * This will close and free all sockets.
 */
void
olsr_socket_cleanup(void)
{
  struct olsr_socket_entry *entry, *iterator;

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    os_close(entry->fd);
    olsr_socket_intfree(entry);
  }
}

/**
 * Add a socket and handler to the socketset
 * beeing used in the main select(2) loop
 *
 * @param fd file descriptor for socket
 * @param pf_imm processing callback
 * @param data custom data
 * @param flags OLSR_SOCKET_READ/OLSR_SOCKET_WRITE (or both)
 * @return pointer to socket_entry
 */
struct olsr_socket_entry *
olsr_socket_add(int fd, socket_handler_func pf_imm, void *data, unsigned int flags)
{
  struct olsr_socket_entry *new_entry;

  if (fd < 0 || pf_imm == NULL) {
    OLSR_WARN(LOG_SCHEDULER, "Bogus socket entry - not registering...");
    return NULL;
  }
  OLSR_DEBUG(LOG_SCHEDULER, "Adding OLSR socket entry %d\n", fd);

  new_entry = olsr_memcookie_malloc(socket_memcookie);

  new_entry->fd = fd;
  new_entry->process = pf_imm;
  new_entry->data = data;
  new_entry->flags = flags;

  /* Queue */
  list_add_before(&socket_head, &new_entry->node);

  return new_entry;
}

/**
 * Remove a socket and handler from the socket scheduler
 * @param sock pointer to socket entry
 */
void
olsr_socket_remove(struct olsr_socket_entry *entry)
{
  OLSR_DEBUG(LOG_SCHEDULER, "Removing OLSR socket entry %d\n", entry->fd);

  entry->process = NULL;
  entry->flags = 0;
}

/**
 * Handle all incoming socket events until a certain time
 * @param next_interval
 */
void
handle_sockets(uint32_t next_interval)
{
  struct olsr_socket_entry *entry, *iterator;
  struct timeval tvp;
  int32_t remaining;

  /* Update time since this is much used by the parsing functions */
  olsr_clock_update();

  remaining = olsr_clock_getRelative(next_interval);
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
      if (entry->process == NULL) {
        continue;
      }
      if ((entry->flags & OLSR_SOCKET_READ) != 0) {
        fdsets |= OLSR_SOCKET_READ;
        FD_SET((unsigned int)entry->fd, &ibits);        /* And we cast here since we get a warning on Win32 */
      }
      if ((entry->flags & OLSR_SOCKET_WRITE) != 0) {
        fdsets |= OLSR_SOCKET_WRITE;
        FD_SET((unsigned int)entry->fd, &obits);        /* And we cast here since we get a warning on Win32 */
      }
      if ((entry->flags & (OLSR_SOCKET_READ | OLSR_SOCKET_WRITE)) != 0 && entry->fd >= hfd) {
        hfd = entry->fd + 1;
      }
    }

    if (hfd == 0 && (long)remaining <= 0) {
      /* we are over the interval and we have no fd's. Skip the select() etc. */
      return;
    }

    do {
      n = os_select(hfd,
          fdsets & OLSR_SOCKET_READ ? &ibits : NULL,
          fdsets & OLSR_SOCKET_WRITE ? &obits : NULL,
          NULL, &tvp);
    } while (n == -1 && errno == EINTR);

    if (n == 0) {               /* timeout! */
      break;
    }
    if (n == -1) {              /* Did something go wrong? */
      OLSR_WARN(LOG_SCHEDULER, "select error: %s (%d)", strerror(errno), errno);
      break;
    }

    /* Update time since this is much used by the parsing functions */
    olsr_clock_update();
    OLSR_FOR_ALL_SOCKETS(entry, iterator) {
      int flags;
      if (entry->process == NULL) {
        continue;
      }
      flags = 0;
      if (FD_ISSET(entry->fd, &ibits)) {
        flags |= OLSR_SOCKET_READ;
      }
      if (FD_ISSET(entry->fd, &obits)) {
        flags |= OLSR_SOCKET_WRITE;
      }
      if (flags != 0) {
        entry->process(entry->fd, entry->data, flags);
      }
    }

    /* calculate the next timeout */
    remaining = olsr_clock_getRelative(next_interval);
    if (remaining <= 0) {
      /* we are already over the interval */
      break;
    }
    /* we need an absolute time - milliseconds */
    tvp.tv_sec = remaining / MSEC_PER_SEC;
    tvp.tv_usec = (remaining % MSEC_PER_SEC) * USEC_PER_MSEC;
  }

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
    if (entry->process == NULL) {
      olsr_socket_intfree(entry);
    }
  }
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
