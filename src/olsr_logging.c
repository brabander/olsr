#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "olsr_cfg.h"
#include "olsr_logging.h"


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

void olsr_log (enum log_severity severity, enum log_source source, const char *file, int line, const char *format, ...) {
  static char logbuffer[LOGBUFFER_SIZE];

  if (olsr_cnf->log_event[severity][source]) {
    va_list ap;
    int p1,p2;

    va_start(ap, format);

    p1 = snprintf(logbuffer, LOGBUFFER_SIZE, "%s %s (%d): ", LOG_SEVERITY_NAMES[severity], file, line);
    p2 = vsnprintf(&logbuffer[p1], LOGBUFFER_SIZE - p1, format, ap);

    assert (p1+p2 < LOGBUFFER_SIZE);

    // TODO switch between different output methods
    fputs(logbuffer, stderr);

    va_end(ap);
  }
}
