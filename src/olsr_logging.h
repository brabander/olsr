
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

#ifndef OLSR_LOGGING_H_
#define OLSR_LOGGING_H_

#include "defs.h"
#include "olsr_cfg_data.h"

#define LOGBUFFER_SIZE 1024
#define MAX_LOG_HANDLER 8

/**
 * these four macros should be used to generate OLSR logging output
 *
 * OLSR_DEBUG should be used for all output that is only usefull for debugging a specific
 * part of the code. This could be information about the internal progress of a function,
 * state of variables, ...
 *
 * OLSR_INFO should be used for all output that does not inform the user about a
 * problem/error in OLSR. Examples would be "SPF run triggered" or "Hello package received
 * from XXX.XXX.XXX.XXX".
 *
 * OLSR_WARN should be used for all error messages that do not prevent OLSR from continue
 * to work.
 *
 * OLSR_ERROR is reserved for error output to the user just before the OLSR agent will be
 * terminated.
 */
#ifdef REMOVE_LOG_DEBUG
#define OLSR_DEBUG(source, format, args...) do { } while(0)
#define OLSR_DEBUG_NH(source, format, args...) do { } while(0)
#else
#define OLSR_DEBUG(source, format, args...) olsr_log(SEVERITY_DEBUG, source, false, __FILE__, __LINE__, format, ##args)
#define OLSR_DEBUG_NH(source, format, args...) olsr_log(SEVERITY_DEBUG, source, true, __FILE__, __LINE__, format, ##args)
#endif

#ifdef REMOVE_LOG_INFO
#define OLSR_INFO(source, format, args...) do { } while(0)
#define OLSR_INFO_NH(source, format, args...) do { } while(0)
#else
#define OLSR_INFO(source, format, args...) olsr_log(SEVERITY_INFO, source, false, __FILE__, __LINE__, format, ##args)
#define OLSR_INFO_NH(source, format, args...) olsr_log(SEVERITY_INFO, source, true, __FILE__, __LINE__, format, ##args)
#endif

#ifdef REMOVE_LOG_WARN
#define OLSR_WARN(source, format, args...) do { } while(0)
#define OLSR_WARN_NH(source, format, args...) do { } while(0)
#else
#define OLSR_WARN(source, format, args...) olsr_log(SEVERITY_WARN, source, false, __FILE__, __LINE__, format, ##args)
#define OLSR_WARN_NH(source, format, args...) olsr_log(SEVERITY_WARN, source, true, __FILE__, __LINE__, format, ##args)
#endif

#ifdef REMOVE_LOG_ERROR
#define OLSR_ERROR(source, format, args...) do { } while(0)
#define OLSR_ERROR_NH(source, format, args...) do { } while(0)
#else
#define OLSR_ERROR(source, format, args...) olsr_log(SEVERITY_ERR, source, false, __FILE__, __LINE__, format, ##args)
#define OLSR_ERROR_NH(source, format, args...) olsr_log(SEVERITY_ERR, source, true, __FILE__, __LINE__, format, ##args)
#endif

void EXPORT(olsr_log_init) (void);
void EXPORT(olsr_log_cleanup) (void);
void EXPORT(olsr_log_addhandler) (void (*handler) (enum log_severity, enum log_source, bool,
                                                   const char *, int, char *, int),
                                  bool(*mask)[LOG_SEVERITY_COUNT][LOG_SOURCE_COUNT]);
void EXPORT(olsr_log_removehandler) (void (*handler) (enum log_severity, enum log_source, bool, const char *, int, char *, int));
void EXPORT(olsr_log_updatemask) (void);

void EXPORT(olsr_log) (enum log_severity, enum log_source, bool, const char *, int, const char *, ...)
  __attribute__ ((format(printf, 6, 7)));

#endif /* OLSR_LOGGING_H_ */
