#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
  "routing",
  "scheduler"
};

const char *LOG_SEVERITY_NAMES[] = {
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
  if (olsr_cnf->log_target_stderr) {
    olsr_log_addhandler(&olsr_log_stderr);
  }
  if (olsr_cnf->log_target_syslog) {
    olsr_log_addhandler(&olsr_log_syslog);
  }
  if (olsr_cnf->log_target_file) {
    log_fileoutput = fopen(olsr_cnf->log_target_file, "a");
    olsr_log_addhandler(&olsr_log_file);
  }
  log_initialized = true;
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

  va_start(ap, format);

  p1 = snprintf(logbuffer, LOGBUFFER_SIZE, "%s(%s) %s %d: ",
    LOG_SEVERITY_NAMES[severity], LOG_SOURCE_NAMES[source], file, line);
  p2 = vsnprintf(&logbuffer[p1], LOGBUFFER_SIZE - p1, format, ap);

  assert (p1+p2 < LOGBUFFER_SIZE);

#if DEBUG
  /* output all event before logsystem is initialized */
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
