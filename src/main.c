
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

#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "defs.h"
#include "common/avl.h"
#include "common/avl_olsr_comp.h"
#include "olsr.h"
#include "ipcalc.h"
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "parser.h"
#include "plugin_loader.h"
#include "os_apm.h"
#include "net_olsr.h"
#include "olsr_cfg_gen.h"
#include "common/string.h"
#include "mid_set.h"
#include "duplicate_set.h"
#include "olsr_comport.h"
#include "neighbor_table.h"
#include "olsr_logging.h"
#include "olsr_callbacks.h"
#include "os_apm.h"
#include "os_net.h"
#include "os_kernel_routes.h"
#include "os_time.h"
#include "os_system.h"

#if defined linux
#include <linux/types.h>
#include <linux/rtnetlink.h>
#include <fcntl.h>
#endif

#define STDOUT_PULSE_INT 600    /* msec */

#ifdef WIN32
int __stdcall SignalHandler(unsigned long signo);
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

/* Global stuff externed in olsr_cfg.h */
struct olsr_config *olsr_cnf;          /* The global configuration */

enum app_state app_state = STATE_INIT;

static char copyright_string[] __attribute__ ((unused)) =
  "The olsr.org Optimized Link-State Routing daemon(olsrd) Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org) All rights reserved.";

static char pulsedata[] = "\\|/-";
static uint8_t pulse_state = 0;

static struct olsr_timer_entry *hna_gen_timer;
static struct olsr_timer_entry *mid_gen_timer;
static struct olsr_timer_entry *tc_gen_timer;

static void
generate_stdout_pulse(void *foo __attribute__ ((unused)))
{
  if (pulsedata[++pulse_state] == '\0') {
    pulse_state = 0;
  }
  fprintf(stderr, "%c\r", pulsedata[pulse_state]);
}

/**
 * Main entrypoint
 */

int
main(int argc, char *argv[])
{
  /* Some cookies for stats keeping */
  static struct olsr_timer_info *pulse_timer_info = NULL;
  static struct olsr_timer_info *tc_gen_timer_info = NULL;
  static struct olsr_timer_info *mid_gen_timer_info = NULL;
  static struct olsr_timer_info *hna_gen_timer_info = NULL;

  char conf_file_name[FILENAME_MAX];
  int exitcode = 0;
#if !defined(REMOVE_LOG_INFO) || !defined(REMOVE_LOG_ERROR)
  struct ipaddr_str buf;
#endif
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

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  /* Using PID as random seed */
  srandom(getpid());

  /* call os dependent argument preprocessor */
  os_arg(&argc, argv);

  /*
   * Set default configfile name
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

  /* initialize logging early */
  olsr_log_init();

  /*
   * set up configuration prior to processing commandline options
   */
  olsr_parse_cfg(argc, argv, conf_file_name, &olsr_cnf);

  /* Set avl tree comparator */
  if (olsr_cnf->ipsize == 4) {
    avl_comp_default = avl_comp_ipv4;
    avl_comp_addr_origin_default = avl_comp_ipv4_addr_origin;
    avl_comp_prefix_default = avl_comp_ipv4_prefix;
    avl_comp_prefix_origin_default = avl_comp_ipv4_prefix_origin;
  } else {
    avl_comp_default = avl_comp_ipv6;
    avl_comp_addr_origin_default = avl_comp_ipv6_addr_origin;
    avl_comp_prefix_default = avl_comp_ipv6_prefix;
    avl_comp_prefix_origin_default = avl_comp_ipv6_prefix_origin;
  }

  /* initialize logging according to configuration */
  olsr_log_applyconfig();

  OLSR_INFO(LOG_MAIN, "\n *** %s ***\n Build date: %s on %s\n http://www.olsr.org\n\n", olsrd_version, build_date, build_host);

  /* Sanity check configuration */
  if (olsr_sanity_check_cfg(olsr_cnf) < 0) {
    olsr_exit(EXIT_FAILURE);
  }

  /* setup os specific global options */
  os_init();

#ifndef WIN32
  /* Check if user is root */
  if (geteuid()) {
    OLSR_ERROR(LOG_MAIN, "You must be root(uid = 0) to run olsrd!\nExiting\n\n");
    olsr_exit(EXIT_FAILURE);
  }
#else
  if (WSAStartup(0x0202, &WsaData)) {
    OLSR_ERROR(LOG_MAIN, "Could not initialize WinSock.\n");
    olsr_exit(EXIT_FAILURE);
  }
#endif

  /* initialize olsr clock */
  olsr_clock_init();

  /* initialize cookie system */
  olsr_memcookie_init();

  /* Initialize timers and scheduler part */
  olsr_timer_init();
  olsr_socket_init();

  /* initialize callback system */
  olsr_callback_init();

  /* generate global timers */
  pulse_timer_info = olsr_timer_add("Stdout pulse", &generate_stdout_pulse, true);
  tc_gen_timer_info = olsr_timer_add("TC generation", &olsr_output_lq_tc, true);
  mid_gen_timer_info = olsr_timer_add("MID generation", &generate_mid, true);
  hna_gen_timer_info = olsr_timer_add("HNA generation", &generate_hna, true);

  /* initialize plugin system */
  olsr_init_pluginsystem();
  olsr_plugins_init(true);

  /* Initialize link set */
  olsr_init_link_set();

  /* Initialize duplicate table */
  olsr_init_duplicate_set();

  /* Initialize neighbor table */
  olsr_init_neighbor_table();

  /* Initialize routing table */
  olsr_init_routing_table();

  /* Initialize topology */
  olsr_init_tc();

  /* Initialize MID set */
  olsr_init_mid_set();

  /* Initialize HNA set */
  olsr_init_hna_set();

  /* enable lq-plugins */
  olsr_plugins_enable(PLUGIN_TYPE_LQ, true);

  /* initialize built in server services */
  olsr_com_init();

  /* Initialize net */
  init_net();

  /* Initialize SPF */
  olsr_init_spf();

  /*
   * socket for ioctl calls
   */
  olsr_cnf->ioctl_s = socket(olsr_cnf->ip_version, SOCK_DGRAM, 0);
  if (olsr_cnf->ioctl_s < 0) {
    OLSR_ERROR(LOG_MAIN, "ioctl socket: %s\n", strerror(errno));
    olsr_exit(EXIT_FAILURE);
  }
#if defined linux
  olsr_cnf->rtnl_s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  if (olsr_cnf->rtnl_s < 0) {
    OLSR_ERROR(LOG_MAIN, "rtnetlink socket: %s\n", strerror(errno));
    olsr_exit(EXIT_FAILURE);
  }
  os_socket_set_nonblocking(olsr_cnf->rtnl_s);

  /* Create rule for RtTable to resolve route insertion problems*/
  if ( ( olsr_cnf->rt_table < 253) & ( olsr_cnf->rt_table > 0 ) ) {
    OLSR_WARN(LOG_NETWORKING,"make sure to have correct policy routing rules (destination based rules are required, or a dummy rule with prio like 65535)");
    /*olsr_netlink_rule(olsr_cnf->ip_version, olsr_cnf->rt_table, RTM_NEWRULE);*/
  }
#endif

/*
 * create routing socket
 */
#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
  olsr_cnf->rts_bsd = socket(PF_ROUTE, SOCK_RAW, 0);
  if (olsr_cnf->rts_bsd < 0) {
    OLSR_ERROR(LOG_MAIN, "routing socket: %s\n", strerror(errno));
    olsr_exit(EXIT_FAILURE);
  }
#endif

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
    olsr_calculate_willingness();
  }

#if defined linux
  /* Initializing lo:olsr if necessary */
  if (olsr_cnf->source_ip_mode) {
    OLSR_INFO(LOG_NETWORKING, "Initializing lo:olsr interface for source ip mode...\n");
    if (olsr_os_localhost_if(&olsr_cnf->router_id, true) <= 0) {
      OLSR_ERROR(LOG_NETWORKING, "Cannot create lo:olsr interface for ip '%s'\n", olsr_ip_to_string(&buf, &olsr_cnf->router_id));
      olsr_exit(EXIT_FAILURE);
    }
  }
#endif

  /* Initializing networkinterfaces */
  if (!init_interfaces()) {
    if (olsr_cnf->allow_no_interfaces) {
      OLSR_INFO(LOG_MAIN,
                "No interfaces detected! This might be intentional, but it also might mean that your configuration is fubar.\nI will continue after 5 seconds...\n");
      os_sleep(5);
    } else {
      OLSR_ERROR(LOG_MAIN, "No interfaces detected!\nBailing out!\n");
      olsr_exit(EXIT_FAILURE);
    }
  }

  /* Print heartbeat to stdout */

#if !defined WINCE
  if (olsr_cnf->log_target_stderr > 0 && isatty(STDOUT_FILENO)) {
    olsr_timer_start(STDOUT_PULSE_INT, 0, NULL, pulse_timer_info);
  }
#endif

  /* daemon mode */
#ifndef WIN32
  if (!olsr_cnf->no_fork) {
    OLSR_INFO(LOG_MAIN, "%s detaching from the current process...\n", olsrd_version);
    if (daemon(0, 0) < 0) {
      OLSR_ERROR(LOG_MAIN, "daemon(3) failed: %s\n", strerror(errno));
      olsr_exit(EXIT_FAILURE);
    }
  }
#endif

  /* activate LQ algorithm */
  init_lq_handler();

  OLSR_INFO(LOG_MAIN, "Main address: %s\n\n", olsr_ip_to_string(&buf, &olsr_cnf->router_id));

  /* Start syslog entry */
  OLSR_INFO(LOG_MAIN, "%s successfully started", olsrd_version);

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
    // Ignoring SIGUSR1 and SIGUSR1 by default to be able to use them in plugins
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);
  }
#endif

  link_changes = false;

  tc_gen_timer =
    olsr_timer_start(olsr_cnf->tc_params.emission_interval, TC_JITTER, NULL, tc_gen_timer_info);
  mid_gen_timer =
    olsr_timer_start(olsr_cnf->mid_params.emission_interval, MID_JITTER, NULL, mid_gen_timer_info);
  hna_gen_timer =
    olsr_timer_start(olsr_cnf->hna_params.emission_interval, HNA_JITTER, NULL, hna_gen_timer_info);

  /* enable default plugins */
  olsr_plugins_enable(PLUGIN_TYPE_DEFAULT, true);

  /* Starting scheduler */
  app_state = STATE_RUNNING;
  while (app_state == STATE_RUNNING) {
    uint32_t next_interval;

    /*
     * Update the global timestamp. We are using a non-wallclock timer here
     * to avoid any undesired side effects if the system clock changes.
     */
    olsr_clock_update();
    next_interval = olsr_clock_getAbsolute(olsr_cnf->pollrate);

    /* Process timers */
    walk_timers();

    /* Update */
    olsr_process_changes();

    /* Read incoming data and handle it immediately */
    olsr_socket_handle(next_interval);
  }

  olsr_timer_stop(tc_gen_timer);
  tc_gen_timer = NULL;

  olsr_timer_stop(mid_gen_timer);
  mid_gen_timer = NULL;

  olsr_timer_stop(hna_gen_timer);
  hna_gen_timer = NULL;

  exitcode = olsr_cnf->exit_value;
  switch (app_state) {
  case STATE_INIT:
    OLSR_ERROR(LOG_MAIN, "terminating and got \"init\"?");
    exitcode = EXIT_FAILURE;
    break;
  case STATE_RUNNING:
    OLSR_ERROR(LOG_MAIN, "terminating and got \"running\"?");
    exitcode = EXIT_FAILURE;
    break;
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
      os_sleep(1);
      OLSR_INFO(LOG_MAIN, "Restarting %s\n", argv[0]);
      execv(argv[0], argv);
      /* if we reach this, the exev() failed */
      OLSR_ERROR(LOG_MAIN, "execv() failed: %s", strerror(errno));
      /* and we simply shutdown */
      exitcode = EXIT_FAILURE;
      break;
    case -1:
      /* fork() failes */
      OLSR_ERROR(LOG_MAIN, "fork() failed: %s", strerror(errno));
      /* and we simply shutdown */
      exitcode = EXIT_FAILURE;
      break;
    default:
      /* parent process */
      OLSR_INFO(LOG_MAIN, "Reconfiguring OLSR\n");
      break;
    }
    /* fall through */
#endif
  case STATE_SHUTDOWN:
    olsr_shutdown();
    break;
  };

  return exitcode;
}                               /* main */

#ifndef WIN32

/**
 * Request reconfiguration of olsrd.
 *
 *@param signal the signal that triggered this callback
 */
static void
signal_reconfigure(int signo __attribute__ ((unused)))
{
  const int save_errno = errno;
  OLSR_INFO(LOG_MAIN, "Received signal %d - requesting reconfiguration", signo);
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
signal_shutdown(int signo __attribute__ ((unused)))
#endif
{
  const int save_errno = errno;
  OLSR_INFO(LOG_MAIN, "Received signal %d - requesting shutdown", (int)signo);
  app_state = STATE_SHUTDOWN;
  errno = save_errno;
#ifdef WIN32
  return 0;
#endif
}

static void
olsr_shutdown(void)
{
  struct mid_entry *mid, *iterator;

  olsr_delete_all_kernel_routes();

  /* Flush MID database */
  OLSR_FOR_ALL_MID_ENTRIES(mid, iterator) {
    olsr_delete_mid_entry(mid);
  }

  /* Flush TC database */
  olsr_delete_all_tc_entries();

  OLSR_INFO(LOG_MAIN, "Closing sockets...\n");

  /* kill http/telnet server */
  olsr_com_destroy();

  /* Flush duplicate set */
  olsr_flush_duplicate_entries();

  /* Shut down LQ plugin */
  deinit_lq_handler();

  /* Closing plug-ins */
  olsr_destroy_pluginsystem();

  /* Remove active interfaces */
  destroy_interfaces();

#if defined linux
  /* delete lo:olsr if neccesarry */
  if (olsr_cnf->source_ip_mode) {
    olsr_os_localhost_if(&olsr_cnf->router_id, false);
  }
#endif

  /* ioctl socket */
  os_close(olsr_cnf->ioctl_s);

#if defined linux
  /*if ((olsr_cnf->rttable < 253) & (olsr_cnf->rttable > 0)) {
    olsr_netlink_rule(olsr_cnf->ip_version, olsr_cnf->rttable, RTM_DELRULE);
  }*/

  os_close(olsr_cnf->rtnl_s);
#endif

#if defined __FreeBSD__ || defined __MacOSX__ || defined __NetBSD__ || defined __OpenBSD__
  /* routing socket */
  os_close(olsr_cnf->rts_bsd);
#endif

  /* Close and delete all sockets */
  olsr_socket_cleanup();

  /* Stop and delete all timers. */
  olsr_timer_cleanup();

  /* Remove parser hooks */
  olsr_deinit_parser();

  /* Remove IP filters */
  deinit_netfilters();

  /* release callback system */
  olsr_callback_cleanup();

  /* Free cookies and memory pools attached. */
  olsr_memcookie_cleanup();

  /* Reset os_specific global options */
  os_cleanup();

  OLSR_INFO(LOG_MAIN, "\n <<<< %s - terminating >>>>\n           http://www.olsr.org\n", olsrd_version);

  /* cleanup logging system */
  olsr_log_cleanup();

  /* Flush config */
  olsr_free_cfg(olsr_cnf);
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
