/*
 * init.c
 *
 *  Created on: Oct 20, 2010
 *      Author: rogge
 */

#include "os_system.h"
#include "unix/unix_log.h"

#if defined linux
#include "linux/linux_net.h"
#include "linux/linux_apm.h"
#else
#include "bsd/bsd_net.h"
#endif

void os_init(void) {
  os_syslog_init("olsrd");
  os_init_global_ifoptions();
  os_apm_init();
}

void os_cleanup(void) {
  os_cleanup_global_ifoptions();
  os_clear_console();
}
