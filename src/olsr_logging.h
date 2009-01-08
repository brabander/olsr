#ifndef OLSR_LOGGING_H_
#define OLSR_LOGGING_H_

#include "defs.h"

#define LOGBUFFER_SIZE 1024
#define MAX_LOG_HANDLER 8

enum log_source {
  LOG_ALL,
  LOG_ROUTING,
  LOG_SCHEDULER,

  /* this one must be the last of the enums ! */
  LOG_SOURCE_COUNT
};

enum log_severity {
  SEVERITY_INFO,
  SEVERITY_WARN,
  SEVERITY_ERROR,

  /* this one must be the last of the enums ! */
  LOG_SEVERITY_COUNT
};

#ifdef REMOVE_LOG_INFO
#define OLSR_INFO(source, format, args...) do { } while(0)
#else
#define OLSR_INFO(source, format, args...) olsr_log(SEVERITY_INFO, source, __FILE__, __LINE__, format, ##args)
#endif

#ifdef REMOVE_LOG_WARN
#define OLSR_WARN(source, format, args...) do { } while(0)
#else
#define OLSR_WARN(source, format, args...) olsr_log(SEVERITY_WARN, source, __FILE__, __LINE__, format, ##args)
#endif

#ifdef REMOVE_LOG_ERROR
#define OLSR_ERROR(source, format, args...) do { } while(0)
#else
#define OLSR_ERROR(source, format, args...) olsr_log(SEVERITY_ERROR, source, __FILE__, __LINE__, format, ##args)
#endif

void EXPORT(olsr_log_init) (void);
void EXPORT(olsr_log_addhandler) (void (*handler)(enum log_severity, enum log_source, const char *, int,
    char *, int, bool));
void EXPORT(olsr_log_removehandler) (void (*handler)(enum log_severity, enum log_source, const char *, int,
    char *, int, bool));

void EXPORT(olsr_log) (enum log_severity, enum log_source, const char *, int, const char * , ...)
    __attribute__((format(printf, 5, 6)));

#endif /* OLSR_LOGGING_H_ */
