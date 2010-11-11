
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

/*
 * System logging interface for GNU/Linux systems
 */

#include "olsr_logging.h"
#include "os_system.h"
#include "unix/unix_log.h"

#include <syslog.h>

void
os_syslog_init(const char *ident)
{
  openlog(ident, LOG_PID | LOG_ODELAY, LOG_DAEMON);
  setlogmask(LOG_UPTO(LOG_INFO));

  return;
}

void
os_syslog_cleanup(void) {
  closelog();
}

void
os_printline(int level, const char *line)
{

  int linux_level;

  switch (level) {
  case (SEVERITY_DEBUG):
/* LOG_DEBUG does not seem to work
    linux_level = LOG_DEBUG;
    break;
*/
  case (SEVERITY_INFO):
    linux_level = LOG_INFO;
    break;
  case (SEVERITY_WARN):
    linux_level = LOG_WARNING;
    break;
  case (SEVERITY_ERR):
    linux_level = LOG_ERR;
    break;
  default:
    return;
  }

  syslog(linux_level, "%s\n", line);
  return;
}

void
os_clear_console(void)
{
  static int len = -1;
  static char clear_buff[100];
  int i;

  if (len < 0) {
    FILE *pip = popen("clear", "r");
    if (pip == NULL) {
      OLSR_WARN(LOG_MAIN, "Warning, cannot access 'clear' command.\n");
      return;
    }
    for (len = 0; len < (int)sizeof(clear_buff); len++) {
      int c = fgetc(pip);
      if (c == EOF) {
        break;
      }
      clear_buff[len] = c;
    }

    pclose(pip);
  }

  for (i = 0; i < len; i++) {
    fputc(clear_buff[i], stdout);
  }
  fflush(stdout);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
