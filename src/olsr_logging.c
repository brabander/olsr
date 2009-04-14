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
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "olsr_cfg.h"
#include "olsr_cfg_data.h"
#include "olsr_logging.h"
#include "log.h"

struct log_handler_entry {
  void (*handler)
      (enum log_severity, enum log_source, bool, const char *, int, char *, int);
  bool (*bitmask)[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT];
};

static struct log_handler_entry log_handler[MAX_LOG_HANDLER];
static int log_handler_count = 0;

static bool log_global_mask[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT];

static bool log_initialized = false;
static FILE *log_fileoutput = NULL;

static void olsr_log_stderr (enum log_severity severity, enum log_source source,
    bool no_header, const char *file, int line, char *buffer, int prefixLength);
static void olsr_log_syslog (enum log_severity severity, enum log_source source,
    bool no_header, const char *file, int line, char *buffer, int prefixLength);
static void olsr_log_file (enum log_severity severity, enum log_source source,
    bool no_header, const char *file, int line, char *buffer, int prefixLength);

/**
 * Called by main method just after configuration options have been parsed
 */
void olsr_log_init(void) {
  char error[256];
  bool printError = false;
  int i,j;

  /* clear global mask */
  for (j=0; j<LOG_SEVERITY_COUNT; j++) {
    for (i=0; i<LOG_SOURCE_COUNT; i++) {
      log_global_mask[j][i] = false;
    }
  }

  if (olsr_cnf->log_target_file) {
    log_fileoutput = fopen(olsr_cnf->log_target_file, "a");
    if (log_fileoutput) {
      olsr_log_addhandler(&olsr_log_file, NULL);
    }
    else {
      /* handle file error for logging output */
      bool otherLog = olsr_cnf->log_target_stderr || olsr_cnf->log_target_syslog;

      /* delay output of error until other loggers are initialized */
      snprintf(error, sizeof(error), "Cannot open log output file %s. %s\n",
          olsr_cnf->log_target_file, otherLog ? "" : "Falling back to stderr logging.");
      printError = true;

      /* activate stderr logger if no other logger is chosen by user */
      if (!otherLog) {
        olsr_cnf->log_target_stderr = true;
      }
    }
  }
  if (olsr_cnf->log_target_syslog) {
    olsr_open_syslog("olsrd");
    olsr_log_addhandler(&olsr_log_syslog, NULL);
  }
  if (olsr_cnf->log_target_stderr) {
    olsr_log_addhandler(&olsr_log_stderr, NULL);
  }
  log_initialized = true;

  if (printError) {
    OLSR_WARN(LOG_LOGGING, "%s", error);
  }
  else {
    OLSR_INFO(LOG_LOGGING, "Initialized Logger...\n");
  }
}

/**
 * Called just before olsr_shutdown finishes
 */
void olsr_log_cleanup(void) {
  if (log_fileoutput) {
    fflush(log_fileoutput);
    fclose(log_fileoutput);
  }
}

/**
 * Call this function to register a custom logevent handler
 * @param handler pointer to handler function
 * @param mask pointer to custom event filter or NULL if handler use filter
 *   from olsr_cnf
 */
void olsr_log_addhandler(void (*handler)(enum log_severity, enum log_source, bool,
    const char *, int, char *, int), bool (*mask)[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT]) {

  assert (log_handler_count < MAX_LOG_HANDLER);

  log_handler[log_handler_count].handler = handler;
  log_handler[log_handler_count].bitmask = mask;
  log_handler_count ++;

  olsr_log_updatemask();
}

/**
 * Call this function to remove a logevent handler
 * @param handler pointer to handler function
 */
void olsr_log_removehandler(void (*handler)(enum log_severity, enum log_source, bool,
    const char *, int, char *, int)) {
  int i;
  for (i=0; i<log_handler_count;i++) {
    if (handler == log_handler[i].handler) {
      if (i < log_handler_count-1) {
        memmove(&log_handler[i], &log_handler[i+1], (log_handler_count-i-1) * sizeof(*log_handler));
      }
      log_handler_count--;
    }
  }

  olsr_log_updatemask();
}

/**
 * Recalculate the combination of the olsr_cnf log event mask and all (if any)
 * custom masks of logfile handlers. Must be called every times a event mask
 * changes.
 */
void olsr_log_updatemask(void) {
  int i,j,k;

  for (k=0; k<LOG_SEVERITY_COUNT; k++) {
    for (j=0; j<LOG_SOURCE_COUNT; j++) {
      log_global_mask[k][j] = olsr_cnf->log_event[k][j];

      for (i=0; i<log_handler_count; i++) {
        if (log_handler[i].bitmask != NULL && (*(log_handler[i].bitmask))[k][j]) {
          log_global_mask[k][j] = true;
          break;
        }
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
void olsr_log (enum log_severity severity, enum log_source source, bool no_header,
    const char *file, int line, const char *format, ...) {
  static char logbuffer[LOGBUFFER_SIZE];
  va_list ap;
  int p1 = 0,p2 = 0, i;
  struct tm now, *tm_ptr;
  struct timeval timeval;

  /* test if event is consumed by any log handler */
  if (!log_global_mask[severity][source])
    return; /* no log handler is interested in this event, so drop it */

  va_start(ap, format);

  /* calculate local time */
  gettimeofday(&timeval, NULL);

  /* there is no localtime_r in win32 */
  tm_ptr = localtime ( (time_t *) &timeval.tv_sec);
  now = *tm_ptr;

  /* generate log string (insert file/line in DEBUG mode) */
  if (!no_header) {
    p1 = snprintf(logbuffer, LOGBUFFER_SIZE, "%d:%02d:%02d.%03ld %s(%s) %s %d: ",
        now.tm_hour, now.tm_min, now.tm_sec, (long)(timeval.tv_usec / 1000),
        LOG_SEVERITY_NAMES[severity], LOG_SOURCE_NAMES[source], file, line);
  }
  p2 = vsnprintf(&logbuffer[p1], LOGBUFFER_SIZE - p1, format, ap);

  assert (p1+p2 < LOGBUFFER_SIZE);

  /* remove \n at the end of the line if necessary */
  if (logbuffer[p1+p2-1] == '\n') {
    logbuffer[p1+p2-1] = 0;
    p2--;
  }

  /* output all events to stderr if logsystem has not been initialized */
  if (!log_initialized) {
#if DEBUG
    fputs(logbuffer, stderr);
#endif
    return;
  }

  /* call all log handlers */
  for (i=0; i<log_handler_count; i++) {
    log_handler[i].handler(severity, source, no_header, file, line, logbuffer, p1);
  }
  va_end(ap);
}

static void olsr_log_stderr (enum log_severity severity, enum log_source source,
    bool no_header __attribute__((unused)),
    const char *file __attribute__((unused)), int line __attribute__((unused)),
    char *buffer, int prefixLength __attribute__((unused))) {
  if (olsr_cnf->log_event[severity][source]) {
    fprintf(stderr, "%s\n", buffer);
  }
}

static void olsr_log_file (enum log_severity severity, enum log_source source,
    bool no_header __attribute__((unused)),
    const char *file __attribute__((unused)), int line __attribute__((unused)),
    char *buffer, int prefixLength __attribute__((unused))) {
  if (olsr_cnf->log_event[severity][source]) {
    fprintf (log_fileoutput, "%s\n", buffer);
  }
}

static void olsr_log_syslog (enum log_severity severity, enum log_source source,
    bool no_header __attribute__((unused)),
    const char *file __attribute__((unused)), int line __attribute__((unused)),
    char *buffer, int prefixLength __attribute__((unused))) {
  if (olsr_cnf->log_event[severity][source]) {
    olsr_print_syslog(severity, "%s\n", buffer);
  }
}
