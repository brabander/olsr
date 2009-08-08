
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

#include "lq_plugin.h"
#include "tc_set.h"
#include "link_set.h"
#include "olsr_spf.h"
#include "lq_packet.h"
#include "olsr.h"
#include "olsr_cookie.h"
#include "common/avl.h"
#include "common/string.h"
#include "olsr_logging.h"

#include "assert.h"

struct lq_handler *active_lq_handler = NULL;

static struct olsr_cookie_info *tc_edge_mem_cookie = NULL;
static struct olsr_cookie_info *lq_hello_neighbor_mem_cookie = NULL;
static struct olsr_cookie_info *link_entry_mem_cookie = NULL;

void
init_lq_handler(void)
{
  if (NULL == active_lq_handler) {
    OLSR_ERROR(LOG_LQ_PLUGINS, "You removed the static linked LQ plugin and don't provide another one.\n");
    olsr_exit(1);
  }

  OLSR_INFO(LOG_LQ_PLUGINS, "Initializing LQ handler %s...\n", active_lq_handler->name);

  tc_edge_mem_cookie = olsr_alloc_cookie("tc_edge", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(tc_edge_mem_cookie, active_lq_handler->size_tc_edge);

  lq_hello_neighbor_mem_cookie = olsr_alloc_cookie("lq_hello_neighbor", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(lq_hello_neighbor_mem_cookie, active_lq_handler->size_lq_hello_neighbor);

  link_entry_mem_cookie = olsr_alloc_cookie("link_entry", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(link_entry_mem_cookie, active_lq_handler->size_link_entry);

  if (active_lq_handler->initialize) {
    active_lq_handler->initialize();
  }
}

void
deinit_lq_handler(void)
{
  if (NULL != active_lq_handler) {
    if (active_lq_handler->deinitialize) {
      active_lq_handler->deinitialize();
    }
    active_lq_handler = NULL;
  }
}

/*
 * olsr_calc_tc_cost
 *
 * this function calculates the linkcost of a tc_edge_entry
 *
 * @param pointer to the tc_edge_entry
 * @return linkcost
 */
olsr_linkcost
olsr_calc_tc_cost(struct tc_edge_entry *tc_edge)
{
  return active_lq_handler->calc_tc_edge_entry_cost(tc_edge);
}

/*
 * olsr_is_relevant_costchange
 *
 * decides if the difference between two costs is relevant
 * (for changing the route for example)
 *
 * @param first linkcost value
 * @param second linkcost value
 * @return boolean
 */
bool
olsr_is_relevant_costchange(olsr_linkcost c1, olsr_linkcost c2)
{
  return active_lq_handler->is_relevant_costchange(c1, c2);
}

/*
 * olsr_serialize_hello_lq_pair
 *
 * this function converts the lq information of a lq_hello_neighbor into binary package
 * format
 *
 * @param pointer to binary buffer to write into
 * @param pointer to lq_hello_neighbor
 * @return number of bytes that have been written
 */
int
olsr_serialize_hello_lq_pair(unsigned char *buff, struct lq_hello_neighbor *neigh)
{
  return active_lq_handler->serialize_hello_lq(buff, neigh);
}

/*
 * olsr_deserialize_hello_lq_pair
 *
 * this function reads the lq information of a binary package into a hello_neighbor
 * It also initialize the cost variable of the hello_neighbor
 *
 * @param pointer to the current buffer pointer
 * @param pointer to hello_neighbor
 */
void
olsr_deserialize_hello_lq_pair(const uint8_t ** curr, struct lq_hello_neighbor *neigh)
{
  active_lq_handler->deserialize_hello_lq(curr, neigh);
  neigh->cost = active_lq_handler->calc_lq_hello_neighbor_cost(neigh);
}

/*
 * olsr_serialize_tc_lq
 *
 * this function converts the lq information of a olsr_serialize_tc_lq_pair
 * into binary package format
 *
 * @param pointer to binary buffer to write into
 * @param pointer to link_entry
 */
void
olsr_serialize_tc_lq(unsigned char **curr, struct link_entry *lnk)
{
  active_lq_handler->serialize_tc_lq(curr, lnk);
}

/*
 * olsr_deserialize_tc_lq_pair
 *
 * this function reads the lq information of a binary package into a tc_edge_entry
 *
 * @param pointer to the current buffer pointer
 * @param pointer to tc_edge_entry
 */
void
olsr_deserialize_tc_lq_pair(const uint8_t ** curr, struct tc_edge_entry *edge)
{
  active_lq_handler->deserialize_tc_lq(curr, edge);
}

/*
 * olsr_update_packet_loss_worker
 *
 * this function is called every times a hello package for a certain link_entry
 * is lost (timeout) or received. This way the lq-plugin can update the links link
 * quality value.
 *
 * @param pointer to link_entry
 * @param true if hello package was lost
 */
void
olsr_update_packet_loss_worker(struct link_entry *entry, bool lost)
{
  olsr_linkcost lq;
  lq = active_lq_handler->packet_loss_handler(entry, lost);

  if (olsr_is_relevant_costchange(lq, entry->linkcost)) {
    entry->linkcost = lq;

    if (olsr_cnf->lq_dlimit > 0) {
      changes_neighborhood = true;
      changes_topology = true;
    }

    else
      OLSR_DEBUG(LOG_LQ_PLUGINS, "Skipping Dijkstra (1)\n");

    /* XXX - we should check whether we actually announce this neighbour */
    signal_link_changes(true);
  }
}

/*
 * olsr_memorize_foreign_hello_lq
 *
 * this function is called to copy the link quality information from a received
 * hello package into a link_entry.
 *
 * @param pointer to link_entry
 * @param pointer to hello_neighbor, if NULL the neighbor link quality information
 * of the link entry has to be reset to "zero"
 */
void
olsr_memorize_foreign_hello_lq(struct link_entry *local, struct lq_hello_neighbor *foreign)
{
  if (foreign) {
    active_lq_handler->memorize_foreign_hello(local, foreign);
  } else {
    active_lq_handler->memorize_foreign_hello(local, NULL);
  }
}

/*
 * get_linkcost_text
 *
 * This function transforms an olsr_linkcost value into it's text representation and copies
 * the result into a buffer.
 *
 * @param linkcost value
 * @param true to transform the cost of a route, false for a link
 * @param pointer to buffer
 * @param size of buffer
 * @return pointer to buffer filled with text
 */
const char *
olsr_get_linkcost_text(olsr_linkcost cost, bool route, char *buffer, size_t bufsize)
{
  static const char *infinite = "INF";

  if (route) {
    if (cost == ROUTE_COST_BROKEN) {
      strscpy(buffer, infinite, bufsize);
      return buffer;
    }
  } else {
    if (cost >= LINK_COST_BROKEN) {
      strscpy(buffer, infinite, bufsize);
      return buffer;
    }
  }
  return active_lq_handler->print_cost(cost, buffer, bufsize);
}

/*
 * olsr_get_linkdata_text
 *
 * this function returns the text representation of one linkquality parameter.
 *
 * @param pointer to link_entry
 * @param index of linkquality parameter
 * @param buffer for output
 * @param size of buffer
 * @return pointer to a buffer with the text representation
 */
const char *
olsr_get_linkdata_text(struct link_entry *entry, int idx, char *buffer, size_t bufsize)
{
  if (idx == 0) {
    return olsr_get_linkcost_text(entry->linkcost, false, buffer, bufsize);
  }
  return active_lq_handler->print_link_entry_lq(entry, idx, buffer, bufsize);
}

/*
 * olsr_get_linklabel
 *
 * this function returns the label of a linkquality parameter
 *
 * @param index of lq parameter (0 is always "linkcost")
 * @return pointer to static string with label
 */
const char *
olsr_get_linklabel (int idx) {
  assert (idx >= 0 && idx < active_lq_handler->linkdata_hello_count);
  return active_lq_handler->linkdata_hello[idx].name;
}

/*
 * olsr_get_linklabel_maxlength
 *
 * this function returns the maximum length (in characters)
 * of a linkquality parameter value
 *
 * @param index of lq parameter (0 is always "linkcost")
 * @return number of bytes necessary to store a lq value string
 */
size_t
olsr_get_linklabel_maxlength (int idx) {
  assert (idx >= 0 && idx < active_lq_handler->linkdata_hello_count);
  return active_lq_handler->linkdata_hello[idx].name_maxlen;
}

/*
 * olsr_get_linklabel_count
 *
 * @return number of linkquality parameters including linkcost
 */
size_t olsr_get_linklabel_count (void) {
  return active_lq_handler->linkdata_hello_count;
}

/*
 * olsr_get_linkdata_quality
 *
 * This function allows you to judge the quality of a certain linkquality parameter
 * (in the categories BEST, GOOD, AVERAGE, BAD and WORST)
 *
 * @param link entry
 * @param index of linkquality parameter
 * @return enum with linkquality estinate
 */
enum lq_linkdata_quality olsr_get_linkdata_quality (struct link_entry *link, int idx) {
  struct lq_linkdata_type *linkdata;
  int value;

  assert (idx >= 0 && idx < active_lq_handler->linkdata_hello_count);
  linkdata = &active_lq_handler->linkdata_hello[idx];
  value = active_lq_handler->get_link_entry_data(link, idx);

  if (value == linkdata->best) {
    return LQ_QUALITY_BEST;
  }
  if (value == linkdata->worst) {
    return LQ_QUALITY_WORST;
  }

  if (linkdata->best < linkdata->worst) {
    if (value > linkdata->good) {
      return LQ_QUALITY_GOOD;
    }
    if (value > linkdata->average) {
      return LQ_QUALITY_AVERAGE;
    }
  }
  else {
    if (value < linkdata->good) {
      return LQ_QUALITY_GOOD;
    }
    if (value < linkdata->average) {
      return LQ_QUALITY_AVERAGE;
    }
  }
  return LQ_QUALITY_BAD;
}

/*
 * olsr_copy_hello_lq
 *
 * this function copies the link quality information from a link_entry to a
 * lq_hello_neighbor.
 *
 * @param pointer to target lq_hello_neighbor
 * @param pointer to source link_entry
 */
void
olsr_copy_hello_lq(struct lq_hello_neighbor *target, struct link_entry *source)
{
  active_lq_handler->copy_link_lq_into_neighbor(target, source);
}

/*
 * olsr_copylq_link_entry_2_tc_edge_entry
 *
 * this function copies the link quality information from a link_entry to a
 * tc_edge_entry.
 *
 * @param pointer to tc_edge_entry
 * @param pointer to link_entry
 */
void
olsr_copylq_link_entry_2_tc_edge_entry(struct tc_edge_entry *target, struct link_entry *source)
{
  active_lq_handler->copy_link_entry_lq_into_tc_edge_entry(target, source);
}

/*
 * olsr_malloc_tc_edge_entry
 *
 * this function allocates memory for an tc_mpr_addr inclusive
 * linkquality data.
 *
 * @return pointer to tc_mpr_addr
 */
struct tc_edge_entry *
olsr_malloc_tc_edge_entry(void)
{
  struct tc_edge_entry *t;

  t = olsr_cookie_malloc(tc_edge_mem_cookie);
  if (active_lq_handler->clear_tc_edge_entry)
    active_lq_handler->clear_tc_edge_entry(t);
  return t;
}

/*
 * olsr_malloc_lq_hello_neighbor
 *
 * this function allocates memory for an lq_hello_neighbor inclusive
 * linkquality data.
 *
 * @return pointer to lq_hello_neighbor
 */
struct lq_hello_neighbor *
olsr_malloc_lq_hello_neighbor(void)
{
  struct lq_hello_neighbor *h;

  h = olsr_cookie_malloc(lq_hello_neighbor_mem_cookie);
  if (active_lq_handler->clear_lq_hello_neighbor)
    active_lq_handler->clear_lq_hello_neighbor(h);
  return h;
}

/*
 * olsr_malloc_link_entry
 *
 * this function allocates memory for an link_entry inclusive
 * linkquality data.
 *
 * @return pointer to link_entry
 */
struct link_entry *
olsr_malloc_link_entry(void)
{
  struct link_entry *h;

  h = olsr_cookie_malloc(link_entry_mem_cookie);
  if (active_lq_handler->clear_link_entry)
    active_lq_handler->clear_link_entry(h);
  return h;
}

/**
 * olsr_free_link_entry
 *
 * this functions free a link_entry inclusive linkquality data
 *
 * @param pointer to link_entry
 */
void
olsr_free_link_entry(struct link_entry *link)
{
  olsr_cookie_free(link_entry_mem_cookie, link);
}

/**
 * olsr_free_lq_hello_neighbor
 *
 * this functions free a lq_hello_neighbor inclusive linkquality data
 *
 * @param pointer to lq_hello_neighbor
 */
void
olsr_free_lq_hello_neighbor(struct lq_hello_neighbor *neigh)
{
  olsr_cookie_free(lq_hello_neighbor_mem_cookie, neigh);
}

/**
 * olsr_free_tc_edge_entry
 *
 * this functions free a tc_edge_entry inclusive linkquality data
 *
 * @param pointer to tc_edge_entry
 */
void
olsr_free_tc_edge_entry(struct tc_edge_entry *edge)
{
  olsr_cookie_free(tc_edge_mem_cookie, edge);
}

/**
 * olsr_get_Hello_MessageId
 *
 * @return olsr id of hello message
 */
uint8_t
olsr_get_Hello_MessageId(void)
{
  return active_lq_handler->messageid_hello;
}

/**
 * olsr_get_TC_MessageId
 *
 * @return olsr id of tc message
 */
uint8_t
olsr_get_TC_MessageId(void)
{
  return active_lq_handler->messageid_tc;
}

/**
 * olsr_sizeof_HelloLQ
 *
 * @return number of bytes necessary to store HelloLQ data
 */
size_t
olsr_sizeof_HelloLQ(void) {
  return active_lq_handler->serialized_lqhello_size;
}

/**
 * olsr_sizeof_TCLQ
 *
 * @return number of bytes necessary to store TCLQ data
 */
size_t
olsr_sizeof_TCLQ(void) {
  return active_lq_handler->serialized_lqtc_size;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
