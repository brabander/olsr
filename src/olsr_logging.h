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

#ifndef OLSR_LOGGING_H_
#define OLSR_LOGGING_H_

#include "defs.h"

#define LOGBUFFER_SIZE 1024
#define MAX_LOG_HANDLER 8

enum log_source {
  LOG_ALL,
  LOG_LOGGING,

  /* this one must be the last of the enums ! */
  LOG_SOURCE_COUNT
};

enum log_severity {
  SEVERITY_DEBUG,
  SEVERITY_INFO,
  SEVERITY_WARN,
  SEVERITY_ERROR,

  /* this one must be the last of the enums ! */
  LOG_SEVERITY_COUNT
};

#ifdef REMOVE_LOG_DEBUG
#define OLSR_DEBUG(source, format, args...) do { } while(0)
#else
#define OLSR_DEBUG(source, format, args...) olsr_log(SEVERITY_DEBUG, source, __FILE__, __LINE__, format, ##args)
#endif

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
void EXPORT(olsr_log_cleanup) (void);
void EXPORT(olsr_log_addhandler) (void (*handler)(enum log_severity, enum log_source, const char *, int,
    char *, int, bool));
void EXPORT(olsr_log_removehandler) (void (*handler)(enum log_severity, enum log_source, const char *, int,
    char *, int, bool));

void EXPORT(olsr_log) (enum log_severity, enum log_source, const char *, int, const char * , ...)
    __attribute__((format(printf, 5, 6)));

extern const char *LOG_SOURCE_NAMES[];
extern const char *LOG_SEVERITY_NAMES[];

#endif /* OLSR_LOGGING_H_ */
