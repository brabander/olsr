/*
 * olsr_cfg_data.h
 *
 *  Created on: 14.01.2009
 *      Author: rogge
 */

#ifndef OLSR_CFG_DATA_H_
#define OLSR_CFG_DATA_H_

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

extern const char *LOG_SOURCE_NAMES[];
extern const char *LOG_SEVERITY_NAMES[];

#endif /* OLSR_CFG_DATA_H_ */
