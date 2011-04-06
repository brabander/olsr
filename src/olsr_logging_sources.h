/*
 * olsr_logging_sources.h
 *
 *  Created on: Apr 6, 2011
 *      Author: rogge
 */

#ifndef OLSR_LOGGING_SOURCES_H_
#define OLSR_LOGGING_SOURCES_H_

/**
 * defines the source of a logging event
 */
enum log_source {
  LOG_ALL,
  LOG_LOGGING,
  LOG_CONFIG,
  LOG_MAIN,
  LOG_INTERFACE,
  LOG_NETWORKING,
  LOG_PACKET_CREATION,
  LOG_PACKET_PARSING,
  LOG_ROUTING,
  LOG_SCHEDULER,
  LOG_TIMER,
  LOG_PLUGINS,
  LOG_LQ_PLUGINS,
  LOG_LL_PLUGINS,
  LOG_LINKS,
  LOG_NEIGHTABLE,
  LOG_MPR,
  LOG_MPRS,
  LOG_2NEIGH,
  LOG_TC,
  LOG_HNA,
  LOG_MID,
  LOG_DUPLICATE_SET,
  LOG_COOKIE,
  LOG_COMPORT,
  LOG_APM,
  LOG_RTNETLINK,
  LOG_TUNNEL,
  LOG_CALLBACK,

  /* this one must be the last of the enums ! */
  LOG_SOURCE_COUNT
};

extern const char *LOG_SOURCE_NAMES[];

#endif /* OLSR_LOGGING_SOURCES_H_ */
