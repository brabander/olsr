
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/list.h"
#include "olsr.h"
#include "olsr_cfg.h"
#include "os_system.h"
#include "os_time.h"
#include "olsr_logging.h"

#define FOR_ALL_LOGHANDLERS(handler, iterator) list_for_each_element_safe(&log_handler_list, handler, node, iterator)

bool log_global_mask[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT];

static struct list_entity log_handler_list;
static FILE *log_fileoutput = NULL;

const char *LOG_SEVERITY_NAMES[] = {
  "DEBUG",
  "INFO",
  "WARN",
  "ERROR"
};

static void olsr_log_stderr(enum log_severity severity, enum log_source source,
                            bool no_header, const char *file, int line, char *buffer,
                            int timeLength, int prefixLength);
static void olsr_log_syslog(enum log_severity severity, enum log_source source,
                            bool no_header, const char *file, int line, char *buffer,
                            int timeLength, int prefixLength);
static void olsr_log_file(enum log_severity severity, enum log_source source,
                          bool no_header, const char *file, int line, char *buffer,
                          int timeLength, int prefixLength);

/**
 * Called by main method just after configuration options have been parsed
 */
void
olsr_log_init(void)
{
  int i,j;

  list_init_head(&log_handler_list);

  /* clear global mask */
  for (j = 0; j < LOG_SEVERITY_COUNT; j++) {
    for (i = 0; i < LOG_SOURCE_COUNT; i++) {
#ifdef DEBUG
      log_global_mask[j][i] = j >= SEVERITY_INFO;
#else
      log_global_mask[j][i] = j >= SEVERITY_WARN;
#endif
    }
  }
}

/**
 * Called just before olsr_shutdown finishes
 */
void
olsr_log_cleanup(void)
{
  struct log_handler_entry *h, *iterator;

  /* remove all handlers */
  FOR_ALL_LOGHANDLERS(h, iterator) {
    olsr_log_removehandler(h);
  }

  /* close file output if necessary */
  if (log_fileoutput) {
    fflush(log_fileoutput);
    fclose(log_fileoutput);
  }
}

/**
 * Configure logger according to olsr settings
 */
void
olsr_log_applyconfig(void) {
  if (olsr_cnf->log_target_file) {
    log_fileoutput = fopen(olsr_cnf->log_target_file, "a");
    if (log_fileoutput) {
      olsr_log_addhandler(&olsr_log_file, &olsr_cnf->log_event);
    } else {
      OLSR_WARN(LOG_LOGGING, "Cannot open log output file %s.", olsr_cnf->log_target_file);
    }
  }
  if (olsr_cnf->log_target_syslog) {
    olsr_log_addhandler(&olsr_log_syslog, &olsr_cnf->log_event);
  }
  if (olsr_cnf->log_target_stderr) {
    olsr_log_addhandler(&olsr_log_stderr, &olsr_cnf->log_event);
  }
}

/**
 * Registers a custom logevent handler
 * @param handler pointer to handler function
 * @param mask pointer to custom event filter or NULL if handler use filter
 *   from olsr_cnf
 */
struct log_handler_entry *
olsr_log_addhandler(void (*handler) (enum log_severity, enum log_source, bool,
                                     const char *, int, char *, int, int),
                    bool(*mask)[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT])
{
  struct log_handler_entry *h;

  /*
   * The logging system is used in the memory cookie manager, so the logging
   * system has to allocate its memory directly. Do not try to use
   * olsr_memcookie_malloc() here.
   */
  h = olsr_malloc(sizeof(*h), "Log handler");
  h->handler = handler;
  h->bitmask_ptr = mask;

  list_add_tail(&log_handler_list, &h->node);
  olsr_log_updatemask();

  return h;
}

/**
 * Call this function to remove a logevent handler
 * @param handler pointer to handler function
 */
void
olsr_log_removehandler(struct log_handler_entry *h)
{
  list_remove(&h->node);
  olsr_log_updatemask();

  free(h);
}

/**
 * Recalculate the combination of the olsr_cnf log event mask and all (if any)
 * custom masks of logfile handlers. Must be called every times a event mask
 * changes.
 */
void
olsr_log_updatemask(void)
{
  int i, j;
  struct log_handler_entry *h, *iterator;

  /* first copy bitmasks to internal memory */
  FOR_ALL_LOGHANDLERS(h, iterator) {
    memcpy (&h->int_bitmask, h->bitmask_ptr, sizeof(h->int_bitmask));
  }

  /* second propagate source ALL to all other sources for each logger */
  FOR_ALL_LOGHANDLERS(h, iterator) {
    for (j = 0; j < LOG_SEVERITY_COUNT; j++) {
      if (h->int_bitmask[j][LOG_ALL]) {
        for (i = 0; i < LOG_SOURCE_COUNT; i++) {
          h->int_bitmask[j][i] = true;
        }
      }
    }
  }

  /* third, propagate events from debug to info to warn to error */
  FOR_ALL_LOGHANDLERS(h, iterator) {
    for (j = 0; j < LOG_SOURCE_COUNT; j++) {
      bool active = false;

      for (i = 0; i < LOG_SEVERITY_COUNT; i++) {
        active |= h->int_bitmask[i][j];
        h->int_bitmask[i][j] = active;
      }
    }
  }

  /* finally calculate the global logging bitmask */
  for (j = 0; j < LOG_SEVERITY_COUNT; j++) {
    for (i = 0; i < LOG_SOURCE_COUNT; i++) {
      log_global_mask[j][i] = false;

      FOR_ALL_LOGHANDLERS(h, iterator) {
        log_global_mask[j][i] |= h->int_bitmask[j][i];
      }
    }
  }
}

/**
 * This function should not be called directly, use the macros OLSR_{DEBUG,INFO,WARN,ERROR} !
 *
 * Generates a logfile entry and calls all log handler to store/output it.
 *
 * @param severity severity of the log event (LOG_DEBUG to LOG_ERROR)
 * @param source source of the log event (LOG_LOGGING, ... )
 * @param file filename where the logging macro have been called
 * @param line line number where the logging macro have been called
 * @param format printf format string for log output plus a variable number of arguments
 */
void
olsr_log(enum log_severity severity, enum log_source source, bool no_header, const char *file, int line, const char *format, ...)
{
  static char logbuffer[LOGBUFFER_SIZE];
  struct log_handler_entry *h, *iterator;
  va_list ap;
  int p1 = 0, p2 = 0, p3 = 0;
  struct tm now, *tm_ptr;
  struct timeval timeval;

  /* test if event is consumed by any log handler */
  if (!log_global_mask[severity][source])
    return;                     /* no log handler is interested in this event, so drop it */

  va_start(ap, format);

  /* calculate local time */
  os_gettimeofday(&timeval, NULL);

  /* there is no localtime_r in win32 */
  tm_ptr = localtime((time_t *) & timeval.tv_sec);
  now = *tm_ptr;

  /* generate log string (insert file/line in DEBUG mode) */
  if (!no_header) {
    p1 = snprintf(logbuffer, LOGBUFFER_SIZE, "%d:%02d:%02d.%03ld ",
                  now.tm_hour, now.tm_min, now.tm_sec, (long)(timeval.tv_usec / 1000));

    p2 = snprintf(&logbuffer[p1], LOGBUFFER_SIZE - p1, "%s(%s) %s %d: ",
        LOG_SEVERITY_NAMES[severity], LOG_SOURCE_NAMES[source], file, line);
  }
  p3 = vsnprintf(&logbuffer[p1+p2], LOGBUFFER_SIZE - p1 - p2, format, ap);

  assert(p1 + p2 +p3 < LOGBUFFER_SIZE);

  /* remove \n at the end of the line if necessary */
  if (logbuffer[p1 + p2 + p3 - 1] == '\n') {
    logbuffer[p1 + p2 + p3 - 1] = 0;
    p3--;
  }

  /* use stderr logger if nothing has been configured */
  if (list_is_empty(&log_handler_list)) {
    olsr_log_stderr(severity, source, no_header, file, line, logbuffer, p1, p2-p1);
    return;
  }

  /* call all log handlers */
  FOR_ALL_LOGHANDLERS(h, iterator) {
    if (h->int_bitmask[severity][source]) {
      h->handler(severity, source, no_header, file, line, logbuffer, p1, p2-p1);
    }
  }
  va_end(ap);
}

static void
olsr_log_stderr(enum log_severity severity __attribute__ ((unused)),
                enum log_source source __attribute__ ((unused)),
                bool no_header __attribute__ ((unused)),
                const char *file __attribute__ ((unused)), int line __attribute__ ((unused)),
                char *buffer,
                int timeLength __attribute__ ((unused)),
                int prefixLength __attribute__ ((unused)))
{
  fprintf(stderr, "%s\n", buffer);
}

static void
olsr_log_file(enum log_severity severity __attribute__ ((unused)),
              enum log_source source __attribute__ ((unused)),
              bool no_header __attribute__ ((unused)),
              const char *file __attribute__ ((unused)), int line __attribute__ ((unused)),
              char *buffer,
              int timeLength __attribute__ ((unused)),
              int prefixLength __attribute__ ((unused)))
{
  fprintf(log_fileoutput, "%s\n", buffer);
}

static void
olsr_log_syslog(enum log_severity severity __attribute__ ((unused)),
                enum log_source source __attribute__ ((unused)),
                bool no_header __attribute__ ((unused)),
                const char *file __attribute__ ((unused)), int line __attribute__ ((unused)),
                char *buffer, int timeLength,
                int prefixLength __attribute__ ((unused)))
{
  os_printline(severity, &buffer[timeLength]);
}
