/*
 * olsr_cfg_data.c
 *
 *  Created on: 14.01.2009
 *      Author: rogge
 */

#include "olsr_logging_data.h"

/*
 * String constants for olsr_log_* as used in olsrd.conf.
 * Keep this in the same order as the log_source and
 * log_severity enums (see olsr_logging_data.h).
 */

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

