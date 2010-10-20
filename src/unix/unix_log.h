/*
 * unix_log.h
 *
 *  Created on: Oct 20, 2010
 *      Author: rogge
 */

#ifndef UNIX_LOG_H_
#define UNIX_LOG_H_

void os_syslog_init(const char *ident);
void os_syslog_cleanup(void);

#endif /* UNIX_LOG_H_ */
