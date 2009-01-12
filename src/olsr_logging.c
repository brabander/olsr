/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
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
#include "olsr_logging.h"
#include "log.h"

static void (*log_handler[MAX_LOG_HANDLER])
    (enum log_severity, enum log_source, const char *, int, char *, int, bool);
static int log_handler_count = 0;
static bool log_initialized = false;
static FILE *log_fileoutput = NULL;

/* keep this in the same order as the enums with the same name ! */
const char *LOG_SOURCE_NAMES[] = {
  "all",
  "logging",
};

const char *LOG_SEVERITY_NAMES[] = {
  "DEBUG",
  "INFO",
  "WARN",
  "ERROR"
};

static void olsr_log_stderr (enum log_severity severity, enum log_source source,
    const char *file, int line, char *buffer, int prefixLength, bool visible);
static void olsr_log_syslog (enum log_severity severity, enum log_source source,
    const char *file, int line, char *buffer, int prefixLength, bool visible);
static void olsr_log_file (enum log_severity severity, enum log_source source,
    const char *file, int line, char *buffer, int prefixLength, bool visible);

void olsr_log_init(void) {
  static char error[256];
  bool printError = false;

  if (olsr_cnf->log_target_file) {
    log_fileoutput = fopen(olsr_cnf->log_target_file, "a");
    if (log_fileoutput) {
      olsr_log_addhandler(&olsr_log_file);
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
    olsr_log_addhandler(&olsr_log_syslog);
  }
  if (olsr_cnf->log_target_stderr) {
    olsr_log_addhandler(&olsr_log_stderr);
  }
  log_initialized = true;

  if (printError) {
    OLSR_WARN(LOG_LOGGING, "%s", error);
  }
}

void olsr_log_cleanup(void) {
  if (log_fileoutput) {
    fflush(log_fileoutput);
    fclose(log_fileoutput);
  }
}

void olsr_log_addhandler(void (*handler)(enum log_severity, enum log_source, const char *, int,
    char *, int, bool)) {

  assert (log_handler_count < MAX_LOG_HANDLER);
  log_handler[log_handler_count++] = handler;
}

void olsr_log_removehandler(void (*handler)(enum log_severity, enum log_source, const char *, int,
    char *, int, bool)) {
  int i;
  for (i=0; i<log_handler_count;i++) {
    if (handler == log_handler[i]) {
      if (i < log_handler_count-1) {
        memmove(&log_handler[i], &log_handler[i+1], (log_handler_count-i-1) * sizeof(*log_handler));
      }
      log_handler_count--;
    }
  }
}

void olsr_log (enum log_severity severity, enum log_source source, const char *file, int line, const char *format, ...) {
  static char logbuffer[LOGBUFFER_SIZE];
  va_list ap;
  int p1,p2, i;
  bool display;
  struct tm now;
  struct timeval timeval;

  va_start(ap, format);

  gettimeofday(&timeval, NULL);
  localtime_r ( &timeval.tv_sec, &now );

#if DEBUG
  p1 = snprintf(logbuffer, LOGBUFFER_SIZE, "%d:%02d:%02d.%03ld %s(%s) %s %d: ",
    now.tm_hour, now.tm_min, now.tm_sec, timeval.tv_usec / 1000,
    LOG_SEVERITY_NAMES[severity], LOG_SOURCE_NAMES[source], file, line);
#else
  p1 = snprintf(logbuffer, LOGBUFFER_SIZE, "%d:%02d:%02d.%03ld %s(%s): ",
    now.tm_hour, now.tm_min, now.tm_sec, timeval.tv_usec / 1000,
    LOG_SEVERITY_NAMES[severity], LOG_SOURCE_NAMES[source]);
#endif
  p2 = vsnprintf(&logbuffer[p1], LOGBUFFER_SIZE - p1, format, ap);

  assert (p1+p2 < LOGBUFFER_SIZE);

#if DEBUG
  /* output all events to stderr if logsystem has not been initialized */
  if (!log_initialized) {
    fputs(logbuffer, stderr);
    return;
  }
#endif

  /* calculate visible logging events */
  display = olsr_cnf->log_event[severity][source];
  for (i=0; i<log_handler_count; i++) {
    log_handler[i](severity, source, file, line, logbuffer, p1, display);
  }
  va_end(ap);
}

static void olsr_log_stderr (enum log_severity severity __attribute__((unused)), enum log_source source __attribute__((unused)),
    const char *file __attribute__((unused)), int line __attribute__((unused)),
    char *buffer, int prefixLength __attribute__((unused)), bool visible) {
  if (visible) {
    fputs (buffer, stderr);
  }
}

static void olsr_log_file (enum log_severity severity __attribute__((unused)), enum log_source source __attribute__((unused)),
    const char *file __attribute__((unused)), int line __attribute__((unused)),
    char *buffer, int prefixLength __attribute__((unused)), bool visible) {
  if (visible) {
    fputs (buffer, log_fileoutput);
  }
}

static void olsr_log_syslog (enum log_severity severity, enum log_source source __attribute__((unused)),
    const char *file __attribute__((unused)), int line __attribute__((unused)),
    char *buffer, int prefixLength __attribute__((unused)), bool visible) {
  if (visible) {
    olsr_syslog(severity, "%s", buffer);
  }
}
