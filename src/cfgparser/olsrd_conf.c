/*
 * OLSR ad-hoc routing table management protocol config parser
 * Copyright (C) 2004 Andreas Tønnesen (andreto@olsr.org)
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
 * $Id: olsrd_conf.c,v 1.7 2004/10/19 19:23:00 kattemat Exp $
 *
 */


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "olsrd_conf.h"


extern FILE *yyin;
extern int yyparse(void);


#ifdef MAKELIB

/* Build as DLL */

void __attribute__ ((constructor)) 
my_init(void);

void __attribute__ ((destructor)) 
my_fini(void);


/**
 *Constructor
 */
void
my_init()
{
  /* Print plugin info to stdout */
  printf("olsrd config file parser %s loaded\n", SOFTWARE_VERSION);

  return;
}

/**
 *Destructor
 */
void
my_fini()
{
  printf("See you around!\n");
  return;
}

#else

#ifdef MAKEBIN

/* Build as standalone binary */
int 
main(int argc, char *argv[])
{
  struct olsrd_config *cnf;

  if(argc == 1)
    {
      fprintf(stderr, "Usage: olsrd_cfgparser [filename] -print\n\n");
      exit(EXIT_FAILURE);
    }

  if((cnf = olsrd_parse_cnf(argv[1])) != NULL)
    {
      if((argc > 2) && (!strcmp(argv[2], "-print")))
	{
	  olsrd_print_cnf(cnf);  
	  olsrd_write_cnf(cnf, "./out.conf");
	}
      else
        printf("Use -print to view parsed values\n");
      printf("Configfile parsed OK\n");
    }
  else
    {
      printf("Failed parsing \"%s\"\n", argv[1]);
    }

  return 0;
}

#else

/* Build as part of olsrd */


#endif

#endif

struct olsrd_config *
olsrd_parse_cnf(char *filename)
{
  struct olsr_if *in;
  struct if_config_options *io;

  cnf = malloc(sizeof(struct olsrd_config));
  if (cnf == NULL)
    {
      fprintf(stderr, "Out of memory %s\n", __func__);
      return NULL;
  }

  set_default_cnf(cnf);

  printf("Parsing file: \"%s\"\n", filename);

  yyin = fopen(filename, "r");
  
  if (yyin == NULL)
    {
      fprintf(stderr, "Cannot open configuration file '%s': %s.\n",
	      filename, strerror(errno));
      free(cnf);
      return NULL;
  }

  current_line = 1;

  if (yyparse() != 0)
    {
      fclose(yyin);
      olsrd_free_cnf(cnf);
      exit(0);
    }
  
  fclose(yyin);

  /* Add default ruleset */

  io = get_default_if_config();
  io->name = malloc(strlen(DEFAULT_IF_CONFIG_NAME) + 1);
  strcpy(io->name, DEFAULT_IF_CONFIG_NAME);

  /* Queue */
  io->next = cnf->if_options;
  cnf->if_options = io;

  /* Verify and set up interface rulesets */
  in = cnf->interfaces;

  while(in)
    {
      in->cnf = find_if_rule_by_name(cnf->if_options, in->config);

      if(in->cnf == NULL)
	{
	  fprintf(stderr, "ERROR: Could not find a matching ruleset \"%s\" for %s\n", in->config, in->name);
	  olsrd_free_cnf(cnf);
	  exit(0);
	}
      /* set various stuff */
      in->index = cnf->ifcnt++;
      in->configured = 0;
      in->interf = NULL;
      /* Calculate max jitter */
      in = in->next;
    }


  return cnf;
}




struct olsrd_config *
olsrd_get_default_cnf()
{
  cnf = malloc(sizeof(struct olsrd_config));
  if (cnf == NULL)
    {
      fprintf(stderr, "Out of memory %s\n", __func__);
      return NULL;
  }

  set_default_cnf(cnf);

  cnf->if_options = get_default_if_config();
  cnf->if_options->name = malloc(strlen(DEFAULT_IF_CONFIG_NAME) + 1);
  strcpy(cnf->if_options->name, DEFAULT_IF_CONFIG_NAME);

  return cnf;
}







void
olsrd_free_cnf(struct olsrd_config *cnf)
{
  struct hna4_entry        *h4d, *h4 = cnf->hna4_entries;
  struct hna6_entry        *h6d, *h6 = cnf->hna6_entries;
  struct olsr_if           *ind, *in = cnf->interfaces;
  struct plugin_entry      *ped, *pe = cnf->plugins;
  struct if_config_options *iod, *io = cnf->if_options;
  
  while(h4)
    {
      h4d = h4;
      h4 = h4->next;
      free(h4d);
    }

  while(h6)
    {
      h6d = h6;
      h6 = h6->next;
      free(h6d);
    }

  while(in)
    {
      ind = in;
      in = in->next;
      free(ind->name);
      free(ind->config);
      free(ind);
    }

  while(pe)
    {
      ped = pe;
      pe = pe->next;
      free(ped->name);
      free(ped);
    }

  while(io)
    {
      iod = io;
      io = io->next;
      free(iod->name);
      free(iod);
    }

  return;
}



void
set_default_cnf(struct olsrd_config *cnf)
{
    memset(cnf, 0, sizeof(struct olsrd_config));
    
    cnf->debug_level = 1;
    cnf->ip_version  = AF_INET;
    cnf->allow_no_interfaces = 1;
    cnf->tos = 16;
    cnf->willingness_auto = 1;
    cnf->open_ipc = 0;

    cnf->use_hysteresis = 1;
    cnf->hysteresis_param.scaling = HYST_SCALING;
    cnf->hysteresis_param.thr_high = HYST_THRESHOLD_HIGH;
    cnf->hysteresis_param.thr_low = HYST_THRESHOLD_LOW;

    cnf->pollrate = 0.1;

    cnf->tc_redundancy = TC_REDUNDANCY;
    cnf->mpr_coverage = MPR_COVERAGE;
}




struct if_config_options *
get_default_if_config()
{
  struct if_config_options *io = malloc(sizeof(struct if_config_options));
  struct in6_addr in6;
 
  memset(io, 0, sizeof(struct if_config_options));

  io->ipv6_addrtype = 1;

  if(inet_pton(AF_INET6, OLSR_IPV6_MCAST_SITE_LOCAL, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", OLSR_IPV6_MCAST_SITE_LOCAL);
      exit(EXIT_FAILURE);
    }
  memcpy(&io->ipv6_multi_site.v6, &in6, sizeof(struct in6_addr));

  if(inet_pton(AF_INET6, OLSR_IPV6_MCAST_GLOBAL, &in6) < 0)
    {
      fprintf(stderr, "Failed converting IP address %s\n", OLSR_IPV6_MCAST_GLOBAL);
      exit(EXIT_FAILURE);
    }
  memcpy(&io->ipv6_multi_glbl.v6, &in6, sizeof(struct in6_addr));


  io->hello_params.emission_interval = HELLO_INTERVAL;
  io->hello_params.validity_time = NEIGHB_HOLD_TIME;
  io->tc_params.emission_interval = TC_INTERVAL;
  io->tc_params.validity_time = TOP_HOLD_TIME;
  io->mid_params.emission_interval = MID_INTERVAL;
  io->mid_params.validity_time = MID_HOLD_TIME;
  io->hna_params.emission_interval = HNA_INTERVAL;
  io->hna_params.validity_time = HNA_HOLD_TIME;

  return io;

}





int
olsrd_write_cnf(struct olsrd_config *cnf, char *fname)
{
  struct hna4_entry        *h4 = cnf->hna4_entries;
  struct hna6_entry        *h6 = cnf->hna6_entries;
  struct olsr_if           *in = cnf->interfaces;
  struct plugin_entry      *pe = cnf->plugins;
  struct if_config_options *io = cnf->if_options;
  char ipv6_buf[100];             /* buffer for IPv6 inet_htop */
  struct in_addr in4;

  FILE *fd;

  fd = fopen(fname, "w");

  if(fd == NULL)
    {
      fprintf(stderr, "Could not open file %s for writing\n%s\n", fname, strerror(errno));
      return -1;
    }

  printf("Writing config to file \"%s\".... ", fname);

  fprintf(fd, "#\n# Configuration file for olsr.org olsrd\n# automatically generated by olsrd-cnf %s\n#\n\n\n", SOFTWARE_VERSION);

  /* Debug level */
  fprintf(fd, "# Debug level(0-9)\n# If set to 0 the daemon runs in the background\n\nDebugLevel\t%d\n\n", cnf->debug_level);

  /* IP version */
  fprintf(fd, "# IP version to use (4 or 6)\n\nIpVersion\t%d\n\n", cnf->ip_version);


  /* HNA IPv4 */
  fprintf(fd, "# HNA IPv4 routes\n# syntax: netaddr netmask\n# Example Internet gateway:\n# 0.0.0.0 0.0.0.0\n\nHna4\n{\n");
  while(h4)
    {
      in4.s_addr = h4->net.v4;
      fprintf(fd, "    %s ", inet_ntoa(in4));
      in4.s_addr = h4->netmask.v4;
      fprintf(fd, "%s\n", inet_ntoa(in4));
      h4 = h4->next;
    }
  fprintf(fd, "}\n\n");


  /* HNA IPv6 */
  fprintf(fd, "# HNA IPv6 routes\n# syntax: netaddr prefix\n# Example Internet gateway:\nHna6\n{\n");
  while(h6)
    {
      fprintf(fd, "    %s/%d\n", (char *)inet_ntop(AF_INET6, &h6->net.v6, ipv6_buf, sizeof(ipv6_buf)), h6->prefix_len);
      h6 = h6->next;
    }

  fprintf(fd, "}\n\n");


  /* Interfaces */
  fprintf(fd, "# Interfaces and their rulesets\n\n");
  /* Interfaces */
  if(in)
    {
      while(in)
	{
	  fprintf(fd, "Interface \"%s\"\n{\n", in->name);
	  fprintf(fd, "    Setup\"%s\"\n", in->config);
	  fprintf(fd, "}\n\n");
	  in = in->next;
	}
    }
  fprintf(fd, "\n");


  /* No interfaces */
  fprintf(fd, "# Should olsrd keep on running even if there are\n# no interfaces available? This is a good idea\n# for a PCMCIA/USB hotswap environment.\n# \"yes\" OR \"no\"\n\nAllowNoInt\t");
  if(cnf->allow_no_interfaces)
    fprintf(fd, "yes\n\n");
  else
    fprintf(fd, "no\n\n");

  /* TOS */
  fprintf(fd, "# TOS(type of service) value for\n# the IP header of control traffic.\n# default is 16\n\n");
  fprintf(fd, "TosValue\t%d\n\n", cnf->tos);

  /* Willingness */
  fprintf(fd, "# The fixed willingness to use(0-7)\n# or \"auto\" to set willingness dynammically\n# based on battery/power status\n\n");
  if(cnf->willingness_auto)
    fprintf(fd, "Willingness\tauto\n\n");
  else
    fprintf(fd, "Willingness%d\n\n", cnf->willingness);

  /* IPC */
  fprintf(fd, "# Allow processes like the GUI front-end\n# to connect to the daemon. 'yes' or 'no'\n\n");
  if(cnf->open_ipc)
    fprintf(fd, "IpcConnect\tyes\n\n");
  else
    fprintf(fd, "IpcConnect\tno\n\n");



  /* Hysteresis */
  fprintf(fd, "# Wether to use hysteresis or not\n# Hysteresis adds more robustness to the\n# link sensing but delays neighbor registration.\n# Used by default. 'yes' or 'no'\n\n");

  if(cnf->use_hysteresis)
    {
      fprintf(fd, "UseHysteresis\tyes\n\n");
      fprintf(fd, "# Hysteresis parameters\n# Do not alter these unless you know \n# what you are doing!\n# Set to auto by default. Allowed\n# values are floating point values\n# in the interval 0,1\n# THR_LOW must always be lower than\n# THR_HIGH!!\n\n");
      fprintf(fd, "HystScaling\t%0.2f\n", cnf->hysteresis_param.scaling);
      fprintf(fd, "HystThrHigh\t%0.2f\n", cnf->hysteresis_param.thr_high);
      fprintf(fd, "HystThrLow\t%0.2f\n", cnf->hysteresis_param.thr_low);
    }
  else
    fprintf(fd, "UseHysteresis\tno\n\n");

  fprintf(fd, "\n\n");

  /* Pollrate */
  fprintf(fd, "# Polling rate in seconds(float).\n# Auto uses default value 0.1 sec\n\n");
  fprintf(fd, "Pollrate\t%0.2f\n", cnf->pollrate);

  /* TC redundancy */
  fprintf(fd, "# TC redundancy\n# Specifies how much neighbor info should\n# be sent in TC messages\n# Possible values are:\n# 0 - only send MPR selectors\n# 1 - send MPR selectors and MPRs\n# 2 - send all neighbors\n#\n# defaults to 0\n\n");
  fprintf(fd, "TcRedundancy\t%d\n\n", cnf->tc_redundancy);

  /* MPR coverage */
  fprintf(fd, "# MPR coverage\n# Specifies how many MPRs a node should\n# try select to reach every 2 hop neighbor\n# Can be set to any integer >0\n# defaults to 1\n\n");

  fprintf(fd, "MprCoverage\t%d\n\n", cnf->mpr_coverage);
   


  /* Plugins */
  fprintf(fd, "# Olsrd plugins to load\n# This must be the absolute path to the file\n# or the loader will use the following scheme:\n# - Try the paths in the LD_LIBRARY_PATH \n#   environment variable.\n# - The list of libraries cached in /etc/ld.so.cache\n# - /lib, followed by /usr/lib\n\n");
  if(pe)
    {
      while(pe)
	{
	  fprintf(fd, "LoadPlugin \"%s\"\n{\n", pe->name);
	  pe = pe->next;
	  fprintf(fd, "}\n");
	}
    }
	  fprintf(fd, "\n");

  /* Rulesets */
  while(io)
    {
      fprintf(fd, "IfSetup \"%s\"\n{\n", io->name);

      
      fprintf(fd, "    # IPv4 broadcast address to use. The\n    # one usefull example would be 255.255.255.255\n    # If not defined the broadcastaddress\n    # every card is configured with is used\n\n");

      if(io->ipv4_broadcast.v4)
	{
	  in4.s_addr = io->ipv4_broadcast.v4;
	  fprintf(fd, "    Ip4Broadcast\t %s\n\n", inet_ntoa(in4));
	}
      else
	{
	  fprintf(fd, "    #Ip4Broadcast\t255.255.255.255\n\n");
	}


      fprintf(fd, "    # IPv6 address scope to use.\n    # Must be 'site-local' or 'global'\n\n");
      if(io->ipv6_addrtype)
	fprintf(fd, "    Ip6AddrType \tsite-local\n\n");
      else
	fprintf(fd, "    Ip6AddrType \tglobal\n\n");

      fprintf(fd, "    # IPv6 multicast address to use when\n    # using site-local addresses.\n    # If not defined, ff05::15 is used\n");
      fprintf(fd, "    Ip6MulticastSite\t%s\n\n", (char *)inet_ntop(AF_INET6, &io->ipv6_multi_site.v6, ipv6_buf, sizeof(ipv6_buf)));
      fprintf(fd, "    # IPv6 multicast address to use when\n    # using global addresses\n    # If not defined, ff0e::1 is used\n");
      fprintf(fd, "    Ip6MulticastGlobal\t%s\n\n", (char *)inet_ntop(AF_INET6, &io->ipv6_multi_glbl.v6, ipv6_buf, sizeof(ipv6_buf)));



      fprintf(fd, "    # Emission intervals.\n    # If not defined, RFC proposed values will\n    # in most cases be used.\n\n");


      fprintf(fd, "    HelloInterval\t%0.2f\n", io->hello_params.emission_interval);
      fprintf(fd, "    HelloValidityTime\t%0.2f\n", io->hello_params.validity_time);
      fprintf(fd, "    TcInterval\t\t%0.2f\n", io->tc_params.emission_interval);
      fprintf(fd, "    TcValidityTime\t%0.2f\n", io->tc_params.validity_time);
      fprintf(fd, "    MidInterval\t\t%0.2f\n", io->mid_params.emission_interval);
      fprintf(fd, "    MidValidityTime\t%0.2f\n", io->mid_params.validity_time);
      fprintf(fd, "    HnaInterval\t\t%0.2f\n", io->hna_params.emission_interval);
      fprintf(fd, "    HnaValidityTime\t%0.2f\n", io->hna_params.validity_time);



      io = io->next;

      fprintf(fd, "}\n\n\n");
    }

  fprintf(fd, "\n# END AUTOGENERATED CONFIG\n");

  fclose(fd);
  printf("DONE\n");

  return 1;
}





void
olsrd_print_cnf(struct olsrd_config *cnf)
{
  struct hna4_entry        *h4 = cnf->hna4_entries;
  struct hna6_entry        *h6 = cnf->hna6_entries;
  struct olsr_if           *in = cnf->interfaces;
  struct plugin_entry      *pe = cnf->plugins;
  struct if_config_options *io = cnf->if_options;
  char ipv6_buf[100];             /* buffer for IPv6 inet_htop */
  struct in_addr in4;

  printf(" *** olsrd configuration ***\n");

  printf("Debug Level      : %d\n", cnf->debug_level);
  printf("IpVersion        : %d\n", cnf->ip_version);
  if(cnf->allow_no_interfaces)
    printf("No interfaces    : ALLOWED\n");
  else
    printf("No interfaces    : NOT ALLOWED\n");
  printf("TOS              : 0x%02x\n", cnf->tos);
  if(cnf->willingness_auto)
    printf("Willingness      : AUTO\n");
  else
    printf("Willingness      : %d\n", cnf->willingness);

  if(cnf->open_ipc)
    printf("IPC              : ENABLED\n");
  else
    printf("IPC              : DISABLED\n");

  printf("Pollrate         : %0.2f\n", cnf->pollrate);

  printf("TC redundancy    : %d\n", cnf->tc_redundancy);

  printf("MPR coverage     : %d\n", cnf->mpr_coverage);
   
  /* Interfaces */
  if(in)
    {
      printf("Interfaces:\n");
      while(in)
	{
	  printf("\tdev: \"%s\" ruleset: \"%s\"\n", in->name, in->config);
	  in = in->next;
	}
    }

  /* Rulesets */
  while(io)
    {
      printf("Interface ruleset \"%s\":\n", io->name);

      
      if(io->ipv4_broadcast.v4)
	{
	  in4.s_addr = io->ipv4_broadcast.v4;
	  printf("\tIPv4 broadcast        : %s\n", inet_ntoa(in4));
	}
      else
	{
	  printf("\tIPv4 broadcast        : AUTO\n");
	}

      if(io->ipv6_addrtype)
	printf("\tIPv6 addrtype         : site-local\n");
      else
	printf("\tIPv6 addrtype         : global\n");

      //union olsr_ip_addr       ipv6_multi_site;
      //union olsr_ip_addr       ipv6_multi_glbl;
      printf("\tIPv6 multicast site   : %s\n", (char *)inet_ntop(AF_INET6, &io->ipv6_multi_site.v6, ipv6_buf, sizeof(ipv6_buf)));
      printf("\tIPv6 multicast global : %s\n", (char *)inet_ntop(AF_INET6, &io->ipv6_multi_glbl.v6, ipv6_buf, sizeof(ipv6_buf)));

      printf("\tHELLO emission int    : %0.2f\n", io->hello_params.emission_interval);
      printf("\tHELLO validity time   : %0.2f\n", io->hello_params.validity_time);
      printf("\tTC emission int       : %0.2f\n", io->tc_params.emission_interval);
      printf("\tTC validity time      : %0.2f\n", io->tc_params.validity_time);
      printf("\tMID emission int      : %0.2f\n", io->mid_params.emission_interval);
      printf("\tMID validity time     : %0.2f\n", io->mid_params.validity_time);
      printf("\tHNA emission int      : %0.2f\n", io->hna_params.emission_interval);
      printf("\tHNA validity time     : %0.2f\n", io->hna_params.validity_time);



      io = io->next;
    }

  /* Plugins */
  if(pe)
    {
      printf("Plugins:\n");

      while(pe)
	{
	  printf("\tName: \"%s\"\n", pe->name);
	  pe = pe->next;
	}
    }

  /* Hysteresis */
  if(cnf->use_hysteresis)
    {
      printf("Using hysteresis:\n");
      printf("\tScaling : %0.2f\n", cnf->hysteresis_param.scaling);
      printf("\tThr high: %0.2f\n", cnf->hysteresis_param.thr_high);
      printf("\tThr low : %0.2f\n", cnf->hysteresis_param.thr_low);
    }
  else
    printf("Not using hysteresis\n");

  /* HNA IPv4 */
  if(h4)
    {

      printf("HNA4 entries:\n");
      while(h4)
	{
	  in4.s_addr = h4->net.v4;
	  printf("\t%s/", inet_ntoa(in4));
	  in4.s_addr = h4->netmask.v4;
	  printf("%s\n", inet_ntoa(in4));

	  h4 = h4->next;
	}
    }

  /* HNA IPv6 */
  if(h6)
    {
      printf("HNA6 entries:\n");
      while(h6)
	{
	  printf("\t%s/%d\n", (char *)inet_ntop(AF_INET6, &h6->net.v6, ipv6_buf, sizeof(ipv6_buf)), h6->prefix_len);
	  h6 = h6->next;
	}
    }
}




struct if_config_options *
find_if_rule_by_name(struct if_config_options *io, char *name)
{

  while(io)
    {
      if(strcmp(io->name, name) == 0)
	return io;
      io = io->next;
    }
  return NULL;
}
