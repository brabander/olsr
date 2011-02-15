
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

/**
 * All these functions are global
 */

#include "defs.h"
#include "olsr.h"
#include "link_set.h"
#include "tc_set.h"
#include "duplicate_set.h"
#include "mid_set.h"
#include "lq_mpr.h"
#include "olsr_spf.h"
#include "scheduler.h"
#include "neighbor_table.h"
#include "lq_packet.h"
#include "common/avl.h"
#include "net_olsr.h"
#include "lq_plugin.h"
#include "olsr_logging.h"
#include "os_system.h"
#include "os_apm.h"

#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>

static void olsr_update_willingness(void *);

bool changes_topology = false;
bool changes_neighborhood = false;
bool changes_hna = false;
bool changes_force = false;

static uint16_t message_seqno;

/**
 *Initialize the message sequence number as a random value
 */
void
init_msg_seqno(void)
{
  message_seqno = random() & 0xFFFF;
  OLSR_DEBUG(LOG_MAIN, "Settings initial message sequence number to %u\n", message_seqno);
}

/**
 * Get and increment the message sequence number
 *
 *@return the seqno
 */
uint16_t
get_msg_seqno(void)
{
  return message_seqno++;
}

/**
 *Process changes in neighborhood or/and topology.
 *Re-calculates the neighborhood/topology if there
 *are any updates - then calls the right functions to
 *update the routing table.
 *@return 0
 */
void
olsr_process_changes(void)
{
  if (changes_neighborhood)
    OLSR_DEBUG(LOG_MAIN, "CHANGES IN NEIGHBORHOOD\n");
  if (changes_topology)
    OLSR_DEBUG(LOG_MAIN, "CHANGES IN TOPOLOGY\n");
  if (changes_hna)
    OLSR_DEBUG(LOG_MAIN, "CHANGES IN HNA\n");

  if (!changes_force && 0 >= olsr_cnf->lq_dlimit)
    return;

  if (!changes_neighborhood && !changes_topology && !changes_hna)
    return;

  if (olsr_cnf->log_target_stderr && olsr_cnf->clear_screen && isatty(STDOUT_FILENO)) {
    os_clear_console();
    printf("       *** %s (%s on %s) ***\n", olsrd_version, build_date, build_host);
  }

  if (changes_neighborhood) {
    olsr_calculate_lq_mpr();
  }

  /* calculate the routing table */
  if (changes_neighborhood || changes_topology || changes_hna) {
    olsr_calculate_routing_table(false);
  }

  olsr_print_link_set();
  olsr_print_neighbor_table();
  olsr_print_tc_table();
  olsr_print_mid_set();
  olsr_print_duplicate_table();
  olsr_print_hna_set();

  changes_neighborhood = false;
  changes_topology = false;
  changes_hna = false;
  changes_force = false;
}

/**
 * Shared code to write the message header
 */
uint8_t *
olsr_put_msg_hdr(uint8_t **curr, struct olsr_message *msg)
{
  uint8_t *sizeptr;

  assert(msg);
  assert(curr);

  pkt_put_u8(curr, msg->type);
  pkt_put_reltime(curr, msg->vtime);
  sizeptr = *curr;
  pkt_put_u16(curr, msg->size);
  pkt_put_ipaddress(curr, &msg->originator);
  pkt_put_u8(curr, msg->ttl);
  pkt_put_u8(curr, msg->hopcnt);
  pkt_put_u16(curr, msg->seqno);

  return sizeptr;
}

static void
olsr_update_willingness(void *foo __attribute__ ((unused)))
{
  int tmp_will = olsr_cnf->willingness;

  /* Re-calculate willingness */
  olsr_calculate_willingness();

  if (tmp_will != olsr_cnf->willingness) {
    OLSR_INFO(LOG_MAIN, "Local willingness updated: old %d new %d\n", tmp_will, olsr_cnf->willingness);
  }
}

void
olsr_init_willingness(void)
{
  /* Some cookies for stats keeping */
  static struct olsr_timer_info *willingness_timer_info = NULL;

  if (olsr_cnf->willingness_auto) {
    OLSR_INFO(LOG_MAIN, "Initialize automatic willingness...\n");
    /* Run it first and then periodic. */
    olsr_update_willingness(NULL);

    willingness_timer_info = olsr_alloc_timerinfo("Update Willingness", &olsr_update_willingness, true);
    olsr_start_timer(olsr_cnf->will_int, 5, NULL, willingness_timer_info);
  }
}

/**
 *Calculate this nodes willingness to act as a MPR
 *based on either a fixed value or the power status
 *of the node using APM
 *
 *@return a 8bit value from 0-7 representing the willingness
 */

void
olsr_calculate_willingness(void)
{
  struct olsr_apm_info ainfo;
#if !defined(REMOVE_LOG_INFO)
  struct millitxt_buf tbuf;
#endif

  /* If fixed willingness */
  if (!olsr_cnf->willingness_auto)
    return;

  if (os_apm_read(&ainfo) < 1) {
    olsr_cnf->willingness = WILL_DEFAULT;
    olsr_cnf->willingness_auto = false;
    OLSR_WARN(LOG_MAIN, "Cannot read APM info, setting willingness to default value (%d)", olsr_cnf->willingness);
    return;
  }

  os_apm_printinfo(&ainfo);

  /* If AC powered */
  if (ainfo.ac_line_status == OLSR_AC_POWERED) {
    olsr_cnf->willingness = 6;
  }
  else {
  /* If battery powered
   *
   * juice > 78% will: 3
   * 78% > juice > 26% will: 2
   * 26% > juice will: 1
   */
    olsr_cnf->willingness = (ainfo.battery_percentage / 26);
  }
  OLSR_INFO(LOG_MAIN, "Willingness set to %d - next update in %s secs\n",
      olsr_cnf->willingness, olsr_milli_to_txt(&tbuf, olsr_cnf->will_int));
}

/**
 *Termination function to be called whenever a error occures
 *that requires the daemon to terminate
 *
 *@param val the exit code for OLSR
 */

void
olsr_exit(int val)
{
  fflush(stdout);
  olsr_cnf->exit_value = val;
  if (app_state == STATE_INIT) {
    os_exit(val);
  }
  app_state = STATE_SHUTDOWN;
}


/**
 * Wrapper for malloc(3) that does error-checking
 *
 * @param size the number of bytes to allocalte
 * @param caller a string identifying the caller for
 * use in error messaging
 *
 * @return a void pointer to the memory allocated
 */
void *
olsr_malloc(size_t size, const char *id __attribute__ ((unused)))
{
  void *ptr;

  /*
   * Not all the callers do a proper cleaning of memory.
   * Clean it on behalf of those.
   */
  ptr = calloc(1, size);

  if (!ptr) {
    OLSR_ERROR(LOG_MAIN, "Out of memory for id '%s': %s\n", id, strerror(errno));
    olsr_exit(EXIT_FAILURE);
  }
  return ptr;
}

/*
 * Same as strdup but works with olsr_malloc
 */
char *
olsr_strdup(const char *s)
{
  char *ret = olsr_malloc(1 + strlen(s), "olsr_strdup");
  strcpy(ret, s);
  return ret;
}

/*
 * Same as strndup but works with olsr_malloc
 */
char *
olsr_strndup(const char *s, size_t n)
{
  size_t len = n < strlen(s) ? n : strlen(s);
  char *ret = olsr_malloc(1 + len, "olsr_strndup");
  strncpy(ret, s, len);
  ret[len] = 0;
  return ret;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
