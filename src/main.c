/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 
 * $Id: main.c,v 1.21 2004/11/02 21:14:12 kattemat Exp $
 *
 */

#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#include "defs.h"
#include "interfaces.h"
#include "local_hna_set.h"
#include "scheduler.h"
#include "parser.h"
#include "generate_msg.h"
#include "plugin_loader.h"
#include "socket_parser.h"
#include "apm.h"
#include "link_layer.h"
#include "net_os.h"

#ifdef linux
#include "linux/tunnel.h"
#elif defined WIN32
#define close(x) closesocket(x)
#include "win32/tunnel.h"
int __stdcall SignalHandler(unsigned long signal);
void ListInterfaces(void);
#else
#       error "Unsupported system"
#endif

/*
 * Local function prototypes
 */

static void
print_usage(void);

static void
set_default_values(void);

static int
set_default_ifcnfs(struct olsr_if *, struct if_config_options *);

static void
olsr_shutdown(int);

/*
 * Local variable declarations 
 */

extern pthread_mutex_t mutex;
pthread_t main_thread;


/**
 * Main entrypoint
 */

int
main(int argc, char *argv[])
{
  /* For address convertions */
  struct in_addr in;
  struct in6_addr in6;

  /* The thread for the scheduler */
  pthread_t thread;

  struct if_config_options *default_ifcnf;

  struct stat statbuf;
  char conf_file_name[FILENAME_MAX];
  
#ifdef WIN32
  WSADATA WsaData;
  int len;
#endif

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  /* Initialize socket list */
  olsr_socket_entries = NULL;

#ifndef WIN32
  /* Check if user is root */
  if(getuid() || getgid())
    {
      fprintf(stderr, "You must be root(uid = 0) to run olsrd!\nExiting\n\n");
      exit(EXIT_FAILURE);
    }
#else
  if (WSAStartup(0x0202, &WsaData))
    {
      fprintf(stderr, "Could not initialize WinSock.\n");
      olsr_exit(__func__, EXIT_FAILURE);
    }
#endif

  /* Open syslog */
  olsr_openlog("olsrd");

  /*
   * Start syslog entry
   */
  olsr_syslog(OLSR_LOG_INFO, "%s started", SOFTWARE_VERSION);

  /* Set default values */
  set_default_values();

  /* Initialize network functions */
  init_net();
 
  /* Get initial timestep */
  nowtm = NULL;
  while (nowtm == NULL)
    {
      nowtm = gmtime(&now.tv_sec);
    }
    
  /* The port to use for OLSR traffic */
  olsr_udp_port = htons(OLSRPORT);
    
  printf("\n *** %s ***\n Build date: %s\n http://www.olsr.org\n\n", 
	 SOFTWARE_VERSION, 
	 __DATE__);
    
  /* Using PID as random seed */
  srandom(getpid());


  /*
   * Set configfile name and
   * check if a configfile name was given as parameter
   */
#ifdef WIN32
  GetWindowsDirectory(conf_file_name, FILENAME_MAX - 11);
  
  len = strlen(conf_file_name);
  
  if (conf_file_name[len - 1] != '\\')
    conf_file_name[len++] = '\\';
  
  strcpy(conf_file_name + len, "olsrd.conf");
#else
  strncpy(conf_file_name, OLSRD_GLOBAL_CONF_FILE, FILENAME_MAX);
#endif

  if ((argc > 1) && (strcmp(argv[1], "-f") == 0)) 
    {
      argv++; argc--;
      if(argc == 1)
	{
	  fprintf(stderr, "You must provide a filename when using the -f switch!\n");
	  exit(EXIT_FAILURE);
	}

      if (stat(argv[1], &statbuf) < 0)
	{
	  fprintf(stderr, "Could not find specified config file %s!\n%s\n\n", argv[1], strerror(errno));
	  exit(EXIT_FAILURE);
	}
		 
      strncpy(conf_file_name, argv[1], FILENAME_MAX);
      argv++; argc--;

    }

  /*
   * set up configuration prior to processing commandline options
   */
  if((olsr_cnf = olsrd_parse_cnf(conf_file_name)) == NULL)
    {
      printf("Using default config values(no configfile)\n");
      olsr_cnf = olsrd_get_default_cnf();
    }
  if((default_ifcnf = get_default_if_config()) == NULL)
    {
      fprintf(stderr, "No default ifconfig found!\n");
      exit(EXIT_FAILURE);
    }

  /*
   * Process olsrd options.
   */
  
  argv++; argc--;
  while (argc > 0 && **argv == '-')
    {
#ifdef WIN32
      /*
       *Interface list
       */
      if (strcmp(*argv, "-int") == 0)
        {
          ListInterfaces();
          exit(0);
        }
#endif

      /*
       *Configfilename
       */
      if(strcmp(*argv, "-f") == 0) 
	{
	  fprintf(stderr, "Configfilename must ALWAYS be first argument!\n\n");
	  olsr_exit(__func__, EXIT_FAILURE);
	}

      /*
       *Use IP version 6
       */
      if(strcmp(*argv, "-ipv6") == 0) 
	{
	  olsr_cnf->ip_version = AF_INET6;
	  argv++; argc--;
	  continue;
	}

      /*
       *Broadcast address
       */
      if(strcmp(*argv, "-bcast") == 0) 
	{
	  argv++; argc--;
	  if(!argc)
	    {
	      fprintf(stderr, "You must provide a broadcastaddr when using the -bcast switch!\n");
	      olsr_exit(__func__, EXIT_FAILURE);
	    }
	  if (inet_aton(*argv, &in) == 0)
	    {
	      printf("Invalid broadcast address! %s\nSkipping it!\n", *argv);
	      continue;
	    }
	  memcpy(&default_ifcnf->ipv4_broadcast.v4, &in.s_addr, sizeof(olsr_u32_t));  
	  continue;
	}
      
      /*
       * Enable additional debugging information to be logged.
       */
      if (strcmp(*argv, "-d") == 0) 
	{
	  argv++; argc--;
	  sscanf(*argv,"%d", &olsr_cnf->debug_level);
	  argv++; argc--;
	  continue;
	}

		
      /*
       * Interfaces to be used by olsrd.
       */
      if (strcmp(*argv, "-i") == 0) 
	{
	  argv++; argc--;
	  if(!argc || (*argv[0] == '-'))
	    {
	      fprintf(stderr, "You must provide an interface label!\n");
	      olsr_exit(__func__, EXIT_FAILURE);
	    }

	  queue_if(*argv);
	  argv++; argc--;

	  while((argc) && (**argv != '-'))
	    {
	      queue_if(*argv);
	      argv++; argc--;
	    }

	  continue;
	}
      /*
       * Set the hello interval to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-hint") == 0) 
	{
	  argv++; argc--;
	  sscanf(*argv,"%f", &default_ifcnf->hello_params.emission_interval);
          default_ifcnf->hello_params.validity_time = default_ifcnf->hello_params.emission_interval * 3;
	  argv++; argc--;
	  continue;
	}

      /*
       * Set the HNA interval to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-hnaint") == 0) 
	{
	  argv++; argc--;
	  sscanf(*argv,"%f", &default_ifcnf->hna_params.emission_interval);
          default_ifcnf->hna_params.validity_time = default_ifcnf->hna_params.emission_interval * 3;
	  argv++; argc--;
	  continue;
	}

      /*
       * Set the MID interval to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-midint") == 0) 
	{
	  argv++; argc--;
	  sscanf(*argv,"%f", &default_ifcnf->mid_params.emission_interval);
          default_ifcnf->mid_params.validity_time = default_ifcnf->mid_params.emission_interval * 3;
	  argv++; argc--;
	  continue;
	}

      /*
       * Set the tc interval to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-tcint") == 0) 
	{
	  argv++; argc--;
	  sscanf(*argv,"%f", &default_ifcnf->tc_params.emission_interval);
          default_ifcnf->tc_params.validity_time = default_ifcnf->tc_params.emission_interval * 3;
	  argv++; argc--;
	  continue;
	}

      /*
       * Set the tos bits to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-tos") == 0) 
	{
	  argv++; argc--;
	  sscanf(*argv,"%d",(int *)&olsr_cnf->tos);
	  argv++; argc--;
	  continue;
	}


      /*
       * Set the polling interval to be used by olsrd.
       */
      if (strcmp(*argv, "-T") == 0) 
	{
	  argv++; argc--;
	  sscanf(*argv,"%f",&olsr_cnf->pollrate);
	  argv++; argc--;
	  continue;
	}


      /*
       * Should we display the contents of packages beeing sent?
       */
      if (strcmp(*argv, "-dispin") == 0) 
	{
	  argv++; argc--;
	  disp_pack_in = 1;
	  continue;
	}

      /*
       * Should we display the contents of incoming packages?
       */
      if (strcmp(*argv, "-dispout") == 0) 
	{
	  argv++; argc--;
	  disp_pack_out = 1;
	  continue;
	}


      /*
       * Should we set up and send on a IPC socket for the front-end?
       */
      if (strcmp(*argv, "-ipc") == 0) 
	{
	  argv++; argc--;
	  olsr_cnf->open_ipc = 1;
	  continue;
	}


      /*
       * Display link-layer info(experimental)
       */
      if (strcmp(*argv, "-llinfo") == 0) 
	{
	  argv++; argc--;
	  llinfo = 1;
	  continue;
	}

      /*
       * Use Internet gateway tunneling?
       */
      if (strcmp(*argv, "-tnl") == 0) 
	{
	  argv++; argc--;
	  use_tunnel = 1;

	  continue;
	}

      /*
       * IPv6 multicast addr
       */
      if (strcmp(*argv, "-multi") == 0) 
	{
	  argv++; argc--;
	  if(inet_pton(AF_INET6, *argv, &in6) < 0)
	    {
	      fprintf(stderr, "Failed converting IP address %s\n", *argv);
	      exit(EXIT_FAILURE);
	    }

	  memcpy(&default_ifcnf->ipv6_multi_glbl, &in6, sizeof(struct in6_addr));

	  argv++; argc--;

	  continue;
	}


      /*
       * Should we display the contents of packages beeing sent?
       */
      if (strcmp(*argv, "-delgw") == 0) 
	{
	  argv++; argc--;
	  del_gws = 1;
	  continue;
	}


      print_usage();
      olsr_exit(__func__, EXIT_FAILURE);
    }


  /*
   *Interfaces need to be specified
   */
  if(olsr_cnf->interfaces == NULL)
    {
      fprintf(stderr, "OLSRD: no interfaces specified!\nuse the -i switch to specify interface(s)\nor set interface(s) in the configuration file!\n");
      print_usage();
      olsr_exit(__func__, EXIT_FAILURE);
    }

  /*
   * Set configuration for command-line specified interfaces
   */
  set_default_ifcnfs(olsr_cnf->interfaces, default_ifcnf);

  /*
   * Print configuration 
   */
  olsrd_print_cnf(olsr_cnf);  

  /*
   *socket for icotl calls
   */
  if ((ioctl_s = socket(olsr_cnf->ip_version, SOCK_DGRAM, 0)) < 0) 
    {
      olsr_syslog(OLSR_LOG_ERR, "ioctl socket: %m");
      close(ioctl_s);
      olsr_exit(__func__, 0);
    }

  /*
   *enable ip forwarding on host
   */
  enable_ip_forwarding(olsr_cnf->ip_version);

  /* Initialize scheduler MUST HAPPEN BEFORE REGISTERING ANY FUNCTIONS! */
  init_scheduler(olsr_cnf->pollrate);

  /* Initialize parser */
  olsr_init_parser();

  /* Initialize message sequencnumber */
  init_msg_seqno();

  /* Initialize dynamic willingness calculation */
  olsr_init_willingness();

  /* Sanity check for hysteresis values */
  if((olsr_cnf->use_hysteresis) &&
     (olsr_cnf->hysteresis_param.thr_high <= olsr_cnf->hysteresis_param.thr_low))
    {
      printf("Hysteresis threshold high lower than threshold low!!\nEdit the configuration file to fix this!\n\n");
      olsr_exit(__func__, EXIT_FAILURE);
    }

  /*
   *Set up willingness/APM
   */
  if(olsr_cnf->willingness_auto)
    {
      if(apm_init() < 0)
	{
	  olsr_printf(1, "Could not read APM info - setting default willingness(%d)\n", WILL_DEFAULT);

	  olsr_syslog(OLSR_LOG_ERR, "Could not read APM info - setting default willingness(%d)\n", WILL_DEFAULT);

	  olsr_cnf->willingness_auto = 0;
	  olsr_cnf->willingness = WILL_DEFAULT;
	}
      else
	{
	  olsr_cnf->willingness = olsr_calculate_willingness();

	  olsr_printf(1, "Willingness set to %d - next update in %.1f secs\n", olsr_cnf->willingness, will_int);
	}
    }

  /**
   *Set ipsize and minimum packetsize
   */
  if(olsr_cnf->ip_version == AF_INET6)
    {
      olsr_printf(1, "Using IP version 6\n");
      ipsize = sizeof(struct in6_addr);

      minsize = (int)sizeof(olsr_u8_t) * 7; /* Minimum packetsize IPv6 */
    }
  else
    {
      olsr_printf(1, "Using IP version 4\n");
      ipsize = sizeof(olsr_u32_t);

      minsize = (int)sizeof(olsr_u8_t) * 4; /* Minimum packetsize IPv4 */
    }


  /* Initializing networkinterfaces */

  if(!ifinit())
    {
      if(olsr_cnf->allow_no_interfaces)
	{
	  fprintf(stderr, "No interfaces detected! This might be intentional, but it also might mean that your configuration is fubar.\nI will continue after 5 seconds...\n");
	  sleep(5);
	}
      else
	{
	  fprintf(stderr, "No interfaces detected!\nBailing out!\n");
	  olsr_exit(__func__, EXIT_FAILURE);
	}
    }

  /* Print tables to stdout */
  if(olsr_cnf->debug_level > 0)
    olsr_register_scheduler_event(&generate_tabledisplay, NULL, HELLO_INTERVAL, 0, NULL);
  
  
  gettimeofday(&now, NULL);


  /* Initialize the IPC socket */

  if(olsr_cnf->open_ipc)
      ipc_init();

#ifndef WIN32
  /* Initialize link-layer notifications */
  if(llinfo)
    init_link_layer_notification();
#endif

  /* Initialisation of different tables to be used.*/
  olsr_init_tables();

  /* Load plugins */
  olsr_load_plugins();

  /* Set up recieving tunnel if Inet gw */
  if(use_tunnel && check_inet_gw())
    set_up_gw_tunnel(&main_addr);

  olsr_printf(1, "Main address: %s\n\n", olsr_ip_to_string(&main_addr));

  olsr_printf(1, "NEIGHBORS: l=linkstate, m=MPR, w=willingness\n\n");


  /* daemon mode */
#ifndef WIN32
  if (olsr_cnf->debug_level == 0)
    {
      printf("%s detattching from the current process...\n", SOFTWARE_VERSION);
      if (fork() != 0)
	{
	  exit(EXIT_SUCCESS);
	}
      setsid();
    }
#endif
  /* Starting scheduler */
  start_scheduler(&thread);

  /*
   *signal-handlers
   */

  /* ctrl-C and friends */
#ifdef WIN32
  SetConsoleCtrlHandler(SignalHandler, TRUE);
#else
  signal(SIGINT, olsr_shutdown);  
  signal(SIGTERM, olsr_shutdown);  
#endif

  /* Go into listenloop */
  listen_loop();

  /* Like we're ever going to reach this ;-) */
  return 1;

} /* main */






/**
 *Function called at shutdown
 *
 */
#ifdef WIN32
int __stdcall
SignalHandler(unsigned long signal)
#else
void
olsr_shutdown(int signal)
#endif
{
  struct interface *ifn;
#ifndef WIN32
  if(main_thread != pthread_self())
    {
      pthread_exit(0);
    }
#endif

  olsr_printf(1, "Received signal %d - shutting down\n", signal);

  olsr_delete_all_kernel_routes();

  olsr_printf(1, "Closing sockets...\n");

  /* front-end IPC socket */
  if(olsr_cnf->open_ipc)
    shutdown_ipc();

  /* OLSR sockets */
  for (ifn = ifnet; ifn; ifn = ifn->int_next) 
    close(ifn->olsr_socket);

  /* Closing plug-ins */
  olsr_close_plugins();

  /* Reset network settings */
  restore_settings(olsr_cnf->ip_version);

  /* ioctl socket */
  close(ioctl_s);

  olsr_syslog(OLSR_LOG_INFO, "%s stopped", SOFTWARE_VERSION);

  olsr_printf(1, "\n <<<< %s - terminating >>>>\n           http://www.olsr.org\n", SOFTWARE_VERSION);

  exit(exit_value);
}





/**
 *Sets the default values of variables at startup
 */
static void
set_default_values()
{
  memset(&main_addr, 0, sizeof(union olsr_ip_addr));
  memset(&null_addr6, 0, sizeof (union olsr_ip_addr));

  exit_value = EXIT_SUCCESS; 
  /* If the application exits by signal it is concidered success,
   * if not, exit_value is set by the function calling olsr_exit.
   */

  max_jitter = 0;
  max_tc_vtime = 0;
  dup_hold_time = DUP_HOLD_TIME;

  will_int = 10 * HELLO_INTERVAL; /* Willingness update interval */


#ifndef WIN32
  /* Get main thread ID */
  main_thread = pthread_self();
#endif

  /* Gateway tunneling */
  use_tunnel = 0;
  inet_tnl_added = 0;
  gw_tunnel = 0;

  llinfo = 0;
  del_gws = 0;

  /* Display packet content */
  disp_pack_in = 0;
  disp_pack_out = 0;
}




static void
print_usage()
{

  fprintf(stderr, "An error occured somwhere between your keyboard and your chair!\n"); 
  fprintf(stderr, "usage: olsrd [-f <configfile>] [ -i interface1 interface2 ... ]\n");
  fprintf(stderr, "  [-d <debug_level>] [-ipv6] [-tnl] [-multi <IPv6 multicast address>]\n"); 
  fprintf(stderr, "  [-bcast <broadcastaddr>] [-ipc] [-dispin] [-dispout] [-delgw]\n");
  fprintf(stderr, "  [-hint <hello interval value (secs)>] [-tcint <tc interval value (secs)>]\n");
  fprintf(stderr, "  [-midint <mid interval value (secs)>] [-hnaint <hna interval value (secs)>]\n");
  fprintf(stderr, "  [-tos value (int)] [-T <Polling Rate (secs)>]\n"); 

}


/**
 * Sets the provided configuration on all unconfigured
 * interfaces
 */
int
set_default_ifcnfs(struct olsr_if *ifs, struct if_config_options *cnf)
{
  int changes = 0;

  while(ifs)
    {
      if(ifs->cnf == NULL)
	{
	  ifs->cnf = olsr_malloc(sizeof(struct if_config_options), "Set default config");
	  *ifs->cnf = *cnf;
	  changes++;
	}
      ifs = ifs->next;
    }
  return changes;
}
