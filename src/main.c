/*
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Andreas Tønnesen (andreto@ifi.uio.no)
 *
 * This file is part of olsrd-unik.
 *
 * UniK olsrd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UniK olsrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsrd-unik; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 *This implementation was based on the INRIA OLSR implementation. 
 *
 *The INRIA code carries the following copyright:
 *
 * This Copyright notice is in French. An English summary is given
 * but the referee text is the French one.
 *
 * Copyright (c) 2000, 2001 Adokoe.Plakoo@inria.fr, INRIA Rocquencourt,
 *                          Anis.Laouiti@inria.fr, INRIA Rocquencourt.
 */


#include "main.h"
#include "interfaces.h"
//#include "ifnet.h"
#include "configfile.h"
#include "mantissa.h"
#include "local_hna_set.h"
#include "olsr.h"
#include "scheduler.h"
#include "parser.h"
#include "generate_msg.h"
#include "plugin_loader.h"
#include "socket_parser.h"
#include "apm.h"
#include "link_layer.h"

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

/**
 *The main funtion does a LOT of things. It should 
 *probably be much shorter
 *
 *After things are set up and the scheduler thread
 *is started, the main thread goes into a typical 
 *select(2) loop listening
 */

int
main(int argc, char *argv[])
{
  //struct interface *ifp;
  struct in_addr in;

  /* The thread for the scheduler */
  pthread_t thread;

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
      olsr_exit(__func__, EXIT_FAILURE);
    }
#else
  if (WSAStartup(0x0202, &WsaData))
    {
      fprintf(stderr, "Could not initialize WinSock.\n");
      olsr_exit(__func__, EXIT_FAILURE);
    }
#endif

  /* Open syslog */
  openlog("olsrd", LOG_PID | LOG_ODELAY, LOG_DAEMON);
  setlogmask(LOG_UPTO(LOG_INFO));

  /*
   * Start syslog entry
   */
  syslog(LOG_INFO, "%s started", SOFTWARE_VERSION);

  /* Set default values */
  set_default_values();

  /* Initialize network functions */
  init_net();

  /* Initialize plugin loader */
  olsr_init_plugin_loader();
 
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
      argv++, argc--;
      if(argc == 1)
	{
	  fprintf(stderr, "You must provide a filename when using the -f switch!\n");
	  olsr_exit(__func__, EXIT_FAILURE);
	}

      if (stat(argv[1], &statbuf) < 0)
	{
	  fprintf(stderr, "Could not finc specified config file %s!\n%s\n\n", argv[1], strerror(errno));
	  olsr_exit(__func__, EXIT_FAILURE);
	}
		 
      strncpy(conf_file_name, argv[1], FILENAME_MAX);
      argv++, argc--;

    }

  /*
   * Reading configfile options prior to processing commandline options
   */

  read_config_file(conf_file_name);
  
  /*
   * Process olsrd options.
   */
  
  argv++, argc--;
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
      if (strcmp(*argv, "-f") == 0) 
	{
	  fprintf(stderr, "Configfilename must ALWAYS be first argument!\n\n");
	  olsr_exit(__func__, EXIT_FAILURE);
	}

      /*
       *Use IP version 6
       */
      if (strcmp(*argv, "-ipv6") == 0) 
	{
	  ipversion = AF_INET6;
	  argv++, argc--;
	  continue;
	}


      /*
       *Broadcast address
       */
      if (strcmp(*argv, "-bcast") == 0) 
	{
	  argv++, argc--;
	  if(argc == 0)
	    {
	      fprintf(stderr, "You must provide a broadcastaddr when using the -bcast switch!\n");
	      olsr_exit(__func__, EXIT_FAILURE);
	    }

	  if (inet_aton(*argv, &in) == 0)
	    {
	      printf("Invalid broadcast address! %s\nSkipping it!\n", *argv);
	      continue;
	    }

	  bcast_set = 1;
		 
	  memcpy(&bcastaddr.sin_addr, &in.s_addr, sizeof(olsr_u32_t));


	  continue;
	}


      /*
       * Enable additional debugging information to be logged.
       */
      if (strcmp(*argv, "-d") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%d", &debug_level);
	  argv++, argc--;
	  continue;
	}



		
      /*
       * Interfaces to be used by olsrd.
       */
      if (strcmp(*argv, "-i") == 0) 
	{
	  option_i = 1;
	  argv++, argc--;
	  queue_if(*argv);
	  argv++, argc--;

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
	  argv++, argc--;
	  sscanf(*argv,"%f",&hello_int);
	  argv++, argc--;
	  continue;
	}

      /*
       * Set the hello interval to be used by olsrd.
       * on nonwireless interfaces
       */
      if (strcmp(*argv, "-hintn") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%f",&hello_int_nw);
	  argv++, argc--;
	  continue;
	}

      /*
       * Set the HNA interval to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-hnaint") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%f", &hna_int);
	  argv++, argc--;
	  continue;
	}

      /*
       * Set the MID interval to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-midint") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%f", &mid_int);
	  argv++, argc--;
	  continue;
	}

      /*
       * Set the tc interval to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-tcint") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%f",&tc_int);
	  argv++, argc--;
	  continue;
	}

      /*
       * Set the tos bits to be used by olsrd.
       * 
       */
      if (strcmp(*argv, "-tos") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%d",(int *)&tos);
	  argv++, argc--;
	  continue;
	}


      /*
       * Set the polling interval to be used by olsrd.
       */
      if (strcmp(*argv, "-T") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%f",&polling_int);
	  argv++, argc--;
	  continue;
	}

      /*
       * Set the vtime miltiplier
       */
      if (strcmp(*argv, "-hhold") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%d",&neighbor_timeout_mult);
	  argv++, argc--;
	  continue;
	}

      /*
       * Set the vtime miltiplier for non-WLAN cards
       */
      if (strcmp(*argv, "-nhhold") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%d",&neighbor_timeout_mult_nw);
	  argv++, argc--;
	  continue;
	}

      /*
       * Set the TC vtime multiplier
       */
      if (strcmp(*argv, "-thold") == 0) 
	{
	  argv++, argc--;
	  sscanf(*argv,"%d",&topology_timeout_mult);
	  argv++, argc--;
	  continue;
	}

      /*
       * Should we display the contents of packages beeing sent?
       */
      if (strcmp(*argv, "-dispin") == 0) 
	{
	  argv++, argc--;
	  disp_pack_in = 1;
	  continue;
	}

      /*
       * Should we display the contents of incoming packages?
       */
      if (strcmp(*argv, "-dispout") == 0) 
	{
	  argv++, argc--;
	  disp_pack_out = 1;
	  continue;
	}


      /*
       * Should we set up and send on a IPC socket for the front-end?
       */
      if (strcmp(*argv, "-ipc") == 0) 
	{
	  argv++, argc--;
	  use_ipc = 1;
	  continue;
	}


      /*
       * Display link-layer info(experimental)
       */
      if (strcmp(*argv, "-llinfo") == 0) 
	{
	  argv++, argc--;
	  llinfo = 1;
	  continue;
	}

      /*
       * Use Internet gateway tunneling?
       */
      if (strcmp(*argv, "-tnl") == 0) 
	{
	  argv++, argc--;
	  use_tunnel = 1;

	  continue;
	}


      /*
       * IPv6 multicast addr
       */
      if (strcmp(*argv, "-multi") == 0) 
	{
	  argv++, argc--;
	  strncpy(ipv6_mult, *argv, 50);

	  argv++, argc--;

	  continue;
	}


      /*
       * Should we display the contents of packages beeing sent?
       */
      if (strcmp(*argv, "-delgw") == 0) 
	{
	  argv++, argc--;
	  del_gws = 1;
	  continue;
	}


      print_usage();
      olsr_exit(__func__, EXIT_FAILURE);
    }


  /*
   *Interfaces need to be specified
   */
  if(!option_i)
    {
      fprintf(stderr, "OLSRD: no interfaces specified!\nuse the -i switch to specify interface(s)\nor set interface(s) in the configuration file!\n");
      print_usage();
      olsr_exit(__func__, EXIT_FAILURE);
    }

  /*
   *socket for icotl calls
   */
  if ((ioctl_s = socket(ipversion, SOCK_DGRAM, 0)) < 0) 
    {
      syslog(LOG_ERR, "ioctl socket: %m");
      close(ioctl_s);
      olsr_exit(__func__, 0);
    }


  /* Type of service */
  precedence = IPTOS_PREC(tos);
  tos_bits = IPTOS_TOS(tos);


  /*
   *enable ip forwarding on host
   */
  enable_ip_forwarding(ipversion);




  /* Initialize scheduler MUST HAPPEN BEFORE REGISTERING ANY FUNCTIONS! */
  init_scheduler(polling_int);

  /* Initialize parser */
  olsr_init_parser();

  /* Initialize message sequencnumber */
  init_msg_seqno();

  /* Initialize dynamic willingness calculation */
  olsr_init_willingness();


  /* Initialize values for emission data 
   * This also initiates message generation
   */
  olsr_set_hello_interval(hello_int);
  olsr_set_hello_nw_interval(hello_int_nw);
  olsr_set_tc_interval(tc_int);
  olsr_set_mid_interval(mid_int);
  olsr_set_hna_interval(hna_int);

  /* Print tables to stdout */
  if(debug_level > 0)
    olsr_register_scheduler_event(&generate_tabledisplay, hello_int, 0, NULL);


  /* printout settings */
  olsr_printf(1, "\n\
hello interval = %0.2f       hello int nonwireless = %0.2f \n\
tc interval = %0.2f          polling interval = %0.2f \n\
neighbor_hold_time = %0.2f   neighbor_hold_time_nw = %0.2f \n\
topology_hold_time = %0.2f  tos setting = %d \n\
hna_interval = %0.2f         mid_interval = %0.2f\n\
tc_redunadancy = %d          mpr coverage = %d\n", 
	      hello_int, hello_int_nw, \
	      tc_int, polling_int, \
	      neighbor_hold_time, neighbor_hold_time_nw, topology_hold_time, \
	      tos, hna_int, mid_int, \
	      tc_redundancy, mpr_coverage);
      
  if(use_hysteresis)
    {
      olsr_printf(1, "hysteresis scaling factor = %0.2f\nhysteresis threshold high = %0.2f\nhysteresis threshold low  = %0.2f\n\n",
		  hyst_scaling,
		  hyst_threshold_high,
		  hyst_threshold_low);

      if(hyst_threshold_high <= hyst_threshold_low)
	{
	  printf("Hysteresis threshold high lower than threshold low!!\nEdit the configuration file to fix this!\n\n");
	  olsr_exit(__func__, EXIT_FAILURE);
	}
    }

  if(ipversion == AF_INET)
    {
      if(bcast_set)
	olsr_printf(2, "Using %s broadcast\n", olsr_ip_to_string((union olsr_ip_addr *) &bcastaddr.sin_addr));
      else
	olsr_printf(2, "Using broadcastaddresses fetched from interfaces\n");
    }

  /*
   *Set up willingness/APM
   */
  if(!willingness_set)
    {
      if(apm_init() < 0)
	{
	  olsr_printf(1, "Could not read APM info - setting default willingness(%d)\n", WILL_DEFAULT);

	  syslog(LOG_ERR, "Could not read APM info - setting default willingness(%d)\n", WILL_DEFAULT);

	  willingness_set = 1;
	  my_willingness = WILL_DEFAULT;
	}
      else
	{
	  my_willingness = olsr_calculate_willingness();

	  olsr_printf(1, "Willingness set to %d - next update in %.1f secs\n", my_willingness, will_int);
	}
    }

  /**
   *Set ipsize and minimum packetsize
   */
  if(ipversion == AF_INET6)
    {
      olsr_printf(1, "Using IP version 6\n");
      ipsize = sizeof(struct in6_addr);

      /* Set multicast address */
      if(ipv6_addrtype == IPV6_ADDR_SITELOCAL)
	{
	  /* Site local */
	  strncpy(ipv6_mult, ipv6_mult_site, 50);
	}
      else
	{
	  /* Global */
	  strncpy(ipv6_mult, ipv6_mult_global, 50);
	}

      olsr_printf(1, "Using multicast address %s\n", ipv6_mult);

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
      if(allow_no_int)
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


  gettimeofday(&now, NULL);


  /* Initialize the IPC socket */

  if(use_ipc)
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
  if(use_tunnel && inet_gw)
    set_up_gw_tunnel(&main_addr);

  olsr_printf(1, "Main address: %s\n\n", olsr_ip_to_string(&main_addr));

  olsr_printf(1, "NEIGHBORS: l=linkstate, m=MPR, w=willingness\n\n");


  /* daemon mode */
#ifndef WIN32
  if (debug_level == 0)
    {
      printf("%s detattching from the current process...\n", SOFTWARE_VERSION);
      if (fork() != 0)
	{
	  exit(1);
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
  if(use_ipc)
    shutdown_ipc();

  /* OLSR sockets */
  for (ifn = ifnet; ifn; ifn = ifn->int_next) 
    close(ifn->olsr_socket);

  /* Closing plug-ins */
  olsr_close_plugins();

  /* Reset network settings */
  restore_settings(ipversion);

  /* ioctl socket */
  close(ioctl_s);

  syslog(LOG_INFO, "%s stopped", SOFTWARE_VERSION);

  olsr_printf(1, "\n <<<< %s - terminating >>>>\n           http://www.olsr.org\n", SOFTWARE_VERSION);

  exit(exit_value);
}






/**
 *Sets the default values of variables at startup
 */
void
set_default_values()
{
  memset(&main_addr, 0, sizeof(union olsr_ip_addr));
  memset(&null_addr6, 0, sizeof (union olsr_ip_addr));

  allow_no_int = 1;

  exit_value = EXIT_SUCCESS; 
  /* If the application exits by signal it is concidered success,
   * if not, exit_value is set by the function calling olsr_exit.
   */

  tos = 16;

  if_names = NULL;

  sending_tc = 0;

  queued_ifs = 0;

  mpr_coverage = MPR_COVERAGE;

  maxmessagesize = MAXMESSAGESIZE;

  ipv6_addrtype = IPV6_ADDR_SITELOCAL;

  /* Default multicastaddresses */
  strncpy(ipv6_mult_site, OLSR_IPV6_MCAST_SITE_LOCAL, strlen(OLSR_IPV6_MCAST_SITE_LOCAL));
  strncpy(ipv6_mult_global, OLSR_IPV6_MCAST_GLOBAL, strlen(OLSR_IPV6_MCAST_GLOBAL));

  /* EMISSION/HOLD INTERVALS */

  hello_int = HELLO_INTERVAL;
  hello_int_nw = HELLO_INTERVAL;
  tc_int = TC_INTERVAL;
  hna_int = 2 * TC_INTERVAL;
  polling_int = 0.1;
  mid_int = MID_INTERVAL;
  will_int = 10 * HELLO_INTERVAL; /* Willingness update interval */

  neighbor_timeout_mult = 3;
  topology_timeout_mult = 3;
  neighbor_timeout_mult_nw = 3;
  mid_timeout_mult = 3;
  hna_timeout_mult = 3;

  topology_hold_time = TOP_HOLD_TIME;
  neighbor_hold_time = NEIGHB_HOLD_TIME;
  neighbor_hold_time_nw = NEIGHB_HOLD_TIME;
  mid_hold_time = MID_HOLD_TIME;
  hna_hold_time = 2 * (3 * TC_INTERVAL);
  dup_hold_time = DUP_HOLD_TIME;

  /* TC redundancy */
  tc_redundancy = TC_REDUNDANCY;

  /* Hysteresis */
  use_hysteresis = 1;
  hyst_scaling = HYST_SCALING;
  hyst_threshold_low = HYST_THRESHOLD_LOW;
  hyst_threshold_high = HYST_THRESHOLD_HIGH;

  use_ipc = 0;
  llinfo = 0;
  bcast_set = 0;
  del_gws = 0;
  /* DEBUG ON BY DEFAULT */
  debug_level = 1;

#ifndef WIN32
  /* Get main thread ID */
  main_thread = pthread_self();
#endif

  /* local HNA set must be initialized before reading options */
  olsr_init_local_hna_set();

  /*
   * set fixed willingness off by default
   */
  willingness_set = 0;

  ipv6_mult[0] = 0;

  /* Gateway tunneling */
  use_tunnel = 0;
  inet_tnl_added = 0;
  gw_tunnel = 0;

  /* Display packet content */
  disp_pack_in = 0;
  disp_pack_out = 0;

  ipversion = AF_INET;
}




void
print_usage()
{

  fprintf(stderr, "An error occured somwhere between your keyboard and your chair!\n"); 
  fprintf(stderr, "usage: olsrd [-f <configfile>] [ -i interface1 interface2 ... ]\n");
  fprintf(stderr, "  [-d <debug_level>] [-ipv6] [-tnl] [-multi <IPv6 multicast address>]\n"); 
  fprintf(stderr, "  [-bcast <broadcastaddr>] [-ipc] [-dispin] [-dispout] [-delgw]\n");
  fprintf(stderr, "  [-midint <mid interval value (secs)>] [-hnaint <hna interval value (secs)>]\n");
  fprintf(stderr, "  [-hint <hello interval value (secs)>] [-tcint <tc interval value (secs)>]\n");
  fprintf(stderr, "  [-hhold <HELLO validity time as a multiplier of the HELLO interval>]\n");
  fprintf(stderr, "  [-nhhold <HELLO validity time on non-wireless interfaces>]\n");
  fprintf(stderr, "  [-thold <TC validity time as a multiplier of the TC interval>]\n");
  fprintf(stderr, "  [-tos value (int)] [-nhint <hello interval value (secs) for non-WLAN>]\n");
  fprintf(stderr, "  [-T <Polling Rate (secs)>]\n"); 

}
