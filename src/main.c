
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

#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "ipcalc.h"
#include "defs.h"
#include "olsr.h"
#include "log.h"
#include "scheduler.h"
#include "parser.h"
#include "generate_msg.h"
#include "plugin_loader.h"
#include "apm.h"
#include "net_os.h"
#include "build_msg.h"
#include "net_olsr.h"
#include "ipc_frontend.h"
#include "misc.h"
#include "olsr_cfg_gen.h"
#include "common/string.h"

#if defined linux
#include <linux/types.h>
#include <linux/rtnetlink.h>
#include <fcntl.h>
#endif

#ifdef WIN32
int __stdcall SignalHandler(unsigned long signo);
void DisableIcmpRedirects(void);
#else
static void signal_shutdown(int);
#endif
static void olsr_shutdown(void);

/*
 * Local function prototypes
 */
#ifndef WIN32
static void signal_reconfigure(int);
#endif

volatile enum app_state app_state = STATE_RUNNING;

static char copyright_string[] __attribute__ ((unused)) =
  "The olsr.org Optimized Link-State Routing daemon(olsrd) Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org) All rights reserved.";

/**
 * Main entrypoint
 */

int
main(int argc, char *argv[])
{
  /* Some cookies for stats keeping */
  static struct olsr_cookie_info *pulse_timer_cookie = NULL;

  char conf_file_name[FILENAME_MAX];
  struct ipaddr_str buf;
#ifdef WIN32
  WSADATA WsaData;
  size_t len;
#endif

  /* paranoia checks */
  assert(sizeof(uint8_t) == 1);
  assert(sizeof(uint16_t) == 2);
  assert(sizeof(uint32_t) == 4);
  assert(sizeof(int8_t) == 1);
  assert(sizeof(int16_t) == 2);
  assert(sizeof(int32_t) == 4);

  debug_handle = stdout;
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

#ifndef WIN32
  /* Check if user is root */
  if (geteuid()) {
    fprintf(stderr, "You must be root(uid = 0) to run olsrd!\nExiting\n\n");
    exit(EXIT_FAILURE);
  }
#else
  DisableIcmpRedirects();

  if (WSAStartup(0x0202, &WsaData)) {
    fprintf(stderr, "Could not initialize WinSock.\n");
    olsr_exit(__func__, EXIT_FAILURE);
  }
#endif

  /* Open syslog */
  olsr_openlog("olsrd");

  /* Initialize timers and scheduler part */
  olsr_init_timers();

  printf("\n *** %s ***\n Build date: %s on %s\n http://www.olsr.org\n\n", olsrd_version, build_date, build_host);

  /* Using PID as random seed */
  srandom(getpid());

  /*
   * Set configfile name and
   * check if a configfile name was given as parameter
   */
#ifdef WIN32
#ifndef WINCE
  GetWindowsDirectory(conf_file_name, sizeof(conf_file_name) - 1 - strlen(OLSRD_CONF_FILE_NAME));
#else
  conf_file_name[0] = 0;
#endif

  len = strlen(conf_file_name);

  if (len == 0 || conf_file_name[len - 1] != '\\') {
    conf_file_name[len++] = '\\';
  }

  strscpy(conf_file_name + len, OLSRD_CONF_FILE_NAME, sizeof(conf_file_name) - len);
#else
  strscpy(conf_file_name, OLSRD_GLOBAL_CONF_FILE, sizeof(conf_file_name));
#endif

  /*
   * set up configuration prior to processing commandline options
   */
  if (NULL == (olsr_cnf = olsr_parse_cnf(argc, argv, conf_file_name))) {
    printf("Using default config values(no configfile)\n");
    olsr_cnf = olsr_get_default_cnf();
  }

  /* Set avl tree comparator */
  if (olsr_cnf->ipsize == 4) {
    avl_comp_default = avl_comp_ipv4;
    avl_comp_prefix_default = avl_comp_ipv4_prefix;
  } else {
    avl_comp_default = avl_comp_ipv6;
    avl_comp_prefix_default = avl_comp_ipv6_prefix;
  }

  /* Initialize tick resolution */
#ifndef WIN32
  olsr_cnf->system_tick_divider = 1000 / sysconf(_SC_CLK_TCK);
#else
  olsr_cnf->system_tick_divider = 1;
#endif

  /* Initialize net */
  init_net();

  /* Sanity check configuration */
  if (olsr_sanity_check_cnf(olsr_cnf) < 0) {
    olsr_exit(__func__, EXIT_FAILURE);
  }

  /*
   * Print configuration
   */
  if (olsr_cnf->debug_level > 1) {
    olsr_print_cnf(olsr_cnf);
  }
#ifndef WIN32
  /* Disable redirects globally */
  disable_redirects_global(olsr_cnf->ip_version);
#endif

  /*
   * socket for ioctl calls
   */
  olsr_cnf->ioctl_s = socket(olsr_cnf->ip_version, SOCK_DGRAM, 0);
  if (olsr_cnf->ioctl_s < 0) {
    olsr_syslog(OLSR_LOG_ERR, "ioctl socket: %m");
    olsr_exit(__func__, 0);
  }
#if defined linux
  olsr_cnf->rts_linux = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  if (olsr_cnf->rts_linux < 0) {
    olsr_syslog(OLSR_LOG_ERR, "rtnetlink socket: %m");
    olsr_exit(__func__, 0);
  }
  set_nonblocking(olsr_cnf->rts_linux);
#endif

/*
 * create routing socket
 */
#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
  olsr_cnf->rts_bsd = socket(PF_ROUTE, SOCK_RAW, 0);
  if (olsr_cnf->rts_bsd < 0) {
    olsr_syslog(OLSR_LOG_ERR, "routing socket: %m");
    olsr_exit(__func__, 0);
  }
#endif

  /*
   *enable ip forwarding on host
   */
  enable_ip_forwarding(olsr_cnf->ip_version);

  /* Initialize parser */
  olsr_init_parser();

  /* Initialize route-exporter */
  olsr_init_export_route();

  /* Initialize message sequencnumber */
  init_msg_seqno();

  /* Initialize dynamic willingness calculation */
  olsr_init_willingness();

  /*
   *Set up willingness/APM
   */
  if (olsr_cnf->willingness_auto) {
    if (apm_init() < 0) {
      OLSR_PRINTF(1, "Could not read APM info - setting default willingness(%d)\n", WILL_DEFAULT);

      olsr_syslog(OLSR_LOG_ERR, "Could not read APM info - setting default willingness(%d)\n", WILL_DEFAULT);

      olsr_cnf->willingness_auto = 0;
      olsr_cnf->willingness = WILL_DEFAULT;
    } else {
      olsr_cnf->willingness = olsr_calculate_willingness();

      OLSR_PRINTF(1, "Willingness set to %d - next update in %.1f secs\n", olsr_cnf->willingness, olsr_cnf->will_int);
    }
  }

  /* Initializing networkinterfaces */
  if (!ifinit()) {
    if (olsr_cnf->allow_no_interfaces) {
      fprintf(stderr,
              "No interfaces detected! This might be intentional, but it also might mean that your configuration is fubar.\nI will continue after 5 seconds...\n");
      sleep(5);
    } else {
      fprintf(stderr, "No interfaces detected!\nBailing out!\n");
      olsr_exit(__func__, EXIT_FAILURE);
    }
  }

  /* Print heartbeat to stdout */

#if !defined WINCE
  if (olsr_cnf->debug_level > 0 && isatty(STDOUT_FILENO)) {
    pulse_timer_cookie = olsr_alloc_cookie("Pulse", OLSR_COOKIE_TYPE_TIMER);
    olsr_start_timer(STDOUT_PULSE_INT, 0, OLSR_TIMER_PERIODIC, &generate_stdout_pulse, NULL, pulse_timer_cookie->ci_id);
  }
#endif

  /* Initialize the IPC socket */

  if (olsr_cnf->ipc_connections > 0) {
    ipc_init();
  }
  /* Initialisation of different tables to be used. */
  olsr_init_tables();

  /* daemon mode */
#ifndef WIN32
  if (olsr_cnf->debug_level == 0 && !olsr_cnf->no_fork) {
    printf("%s detaching from the current process...\n", olsrd_version);
    if (daemon(0, 0) < 0) {
      printf("daemon(3) failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
#endif

  /* Load plugins */
  olsr_load_plugins();

  OLSR_PRINTF(1, "Main address: %s\n\n", olsr_ip_to_string(&buf, &olsr_cnf->main_addr));

  /* Start syslog entry */
  olsr_syslog(OLSR_LOG_INFO, "%s successfully started", olsrd_version);

  /*
   *signal-handlers
   */

  /* ctrl-C and friends */
#ifdef WIN32
#ifndef WINCE
  SetConsoleCtrlHandler(SignalHandler, true);
#endif
#else
  {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = signal_reconfigure;
    sigaction(SIGHUP, &act, NULL);
    act.sa_handler = signal_shutdown;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
//  sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
  }
#endif

  link_changes = false;

  /* Starting scheduler */
  olsr_scheduler();

  switch (app_state) {
  case STATE_RUNNING:
    olsr_syslog(OLSR_LOG_ERR, "terminating and got \"running\"?");
    return 1;
#ifndef WIN32
  case STATE_RECONFIGURE:
    /* if we are started with -nofork, we do not weant to go into the
     * background here. So we can simply be the child process.
     */
    switch (olsr_cnf->no_fork ? 0 : fork()) {
      int i;
    case 0:
      /* child process */
      for (i = sysconf(_SC_OPEN_MAX); --i > STDERR_FILENO;) {
        close(i);
      }
      sleep(1);
      printf("Restarting %s\n", argv[0]);
      execv(argv[0], argv);
      /* if we reach this, the exev() failed */
      olsr_syslog(OLSR_LOG_ERR, "execv() failed: %s", strerror(errno));
      /* and we simply shutdown */
      olsr_cnf->exit_value = EXIT_FAILURE;
      break;
    case -1:
      /* fork() failes */
      olsr_syslog(OLSR_LOG_ERR, "fork() failed: %s", strerror(errno));
      /* and we simply shutdown */
      olsr_cnf->exit_value = EXIT_FAILURE;
      break;
    default:
      /* parent process */
      printf("RECONFIGURING!\n");
      break;
    }
    /* fall through */
#endif
  case STATE_SHUTDOWN:
    olsr_shutdown();
    break;
  };

  return olsr_cnf->exit_value;
}                               /* main */

#ifndef WIN32

/**
 * Request reconfiguration of olsrd.
 *
 *@param signal the signal that triggered this callback
 */
static void
signal_reconfigure(int signo)
{
  const int save_errno = errno;
  olsr_syslog(OLSR_LOG_INFO, "Received signal %d - requesting reconfiguration", signo);
  app_state = STATE_RECONFIGURE;
  errno = save_errno;
}

#endif

/**
 *Function called at shutdown. Signal handler
 *
 * @param signal the signal that triggered this call
 */
#ifdef WIN32
int __stdcall
SignalHandler(unsigned long signo)
#else
static void
signal_shutdown(int signo)
#endif
{
  const int save_errno = errno;
  olsr_syslog(OLSR_LOG_INFO, "Received signal %d - requesting shutdown", (int)signo);
  app_state = STATE_SHUTDOWN;
  errno = save_errno;
#ifdef WIN32
  return 0;
#endif
}

static void
olsr_shutdown(void)
{
  struct interface *ifn;
  struct tc_entry *tc;

  olsr_delete_all_kernel_routes();

  /* Flush link state database */
  OLSR_FOR_ALL_TC_ENTRIES(tc) {
    olsr_delete_tc_entry(tc);
  } OLSR_FOR_ALL_TC_ENTRIES_END(tc);

  OLSR_PRINTF(1, "Closing sockets...\n");

  /* front-end IPC socket */
  if (olsr_cnf->ipc_connections > 0) {
    shutdown_ipc();
  }

  /* OLSR sockets */
  OLSR_FOR_ALL_INTERFACES(ifn) {
    CLOSESOCKET(ifn->olsr_socket);
  } OLSR_FOR_ALL_INTERFACES_END(ifn);

  /* Closing plug-ins */
  olsr_close_plugins();

  /* Stop and delete all timers. */
  olsr_flush_timers();

  /* Reset network settings */
  restore_settings(olsr_cnf->ip_version);

  /* ioctl socket */
  CLOSESOCKET(olsr_cnf->ioctl_s);

#if defined linux
  CLOSESOCKET(olsr_cnf->rts_linux);
#endif

#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
  /* routing socket */
  CLOSESOCKET(olsr_cnf->rts_bsd);
#endif

  /* Free cookies and memory pools attached. */
  olsr_delete_all_cookies();

  olsr_syslog(OLSR_LOG_INFO, "%s stopped", olsrd_version);

  OLSR_PRINTF(1, "\n <<<< %s - terminating >>>>\n           http://www.olsr.org\n", olsrd_version);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
