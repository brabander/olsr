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
 * $Id: olsrd_conf.c,v 1.22 2004/11/20 21:52:09 kattemat Exp $
 *
 */


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
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
  printf("olsrd config file parser %s loaded\n", PARSER_VERSION);

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


  in = cnf->interfaces;

  while(in)
    {
      /* set various stuff */
      in->index = cnf->ifcnt++;
      in->configured = OLSR_FALSE;
      in->interf = NULL;
      in = in->next;
    }


  return cnf;
}


int
olsrd_sanity_check_cnf(struct olsrd_config *cnf)
{
  struct olsr_if           *in = cnf->interfaces;
  struct if_config_options *io;

  /* Debug level */
  if(cnf->debug_level < MIN_DEBUGLVL ||
     cnf->debug_level > MAX_DEBUGLVL)
    {
      fprintf(stderr, "Debuglevel %d is not allowed\n", cnf->debug_level);
      return -1;
    }

  /* IP version */
  if(cnf->ip_version != AF_INET &&
     cnf->ip_version != AF_INET6)
    {
      fprintf(stderr, "Ipversion %d not allowed!\n", cnf->ip_version);
      return -1;
    }

  /* TOS */
  if(//cnf->tos < MIN_TOS ||
     cnf->tos > MAX_TOS)
    {
      fprintf(stderr, "TOS %d is not allowed\n", cnf->tos);
      return -1;
    }

  if(cnf->willingness_auto == OLSR_FALSE &&
     (cnf->willingness < MIN_WILLINGNESS ||
      cnf->willingness > MAX_WILLINGNESS))
    {
      fprintf(stderr, "Willingness %d is not allowed\n", cnf->willingness);
      return -1;
    }

  /* Hysteresis */
  if(cnf->use_hysteresis == OLSR_TRUE)
    {
      if(cnf->hysteresis_param.scaling < MIN_HYST_PARAM ||
	 cnf->hysteresis_param.scaling > MAX_HYST_PARAM)
	{
	  fprintf(stderr, "Hyst scaling %0.2f is not allowed\n", cnf->hysteresis_param.scaling);
	  return -1;
	}

      if(cnf->hysteresis_param.thr_high <= cnf->hysteresis_param.thr_low)
	{
	  fprintf(stderr, "Hyst upper(%0.2f) thr must be bigger than lower(%0.2f) threshold!\n", cnf->hysteresis_param.thr_high, cnf->hysteresis_param.thr_low);
	  return -1;
	}

      if(cnf->hysteresis_param.thr_high < MIN_HYST_PARAM ||
	 cnf->hysteresis_param.thr_high > MAX_HYST_PARAM)
	{
	  fprintf(stderr, "Hyst upper thr %0.2f is not allowed\n", cnf->hysteresis_param.thr_high);
	  return -1;
	}

      if(cnf->hysteresis_param.thr_low < MIN_HYST_PARAM ||
	 cnf->hysteresis_param.thr_low > MAX_HYST_PARAM)
	{
	  fprintf(stderr, "Hyst lower thr %0.2f is not allowed\n", cnf->hysteresis_param.thr_low);
	  return -1;
	}
    }

  /* Pollrate */

  if(cnf->pollrate < MIN_POLLRATE ||
     cnf->pollrate > MAX_POLLRATE)
    {
      fprintf(stderr, "Pollrate %0.2f is not allowed\n", cnf->pollrate);
      return -1;
    }

  /* TC redundancy */

  if(//cnf->tc_redundancy < MIN_TC_REDUNDANCY ||
     cnf->tc_redundancy > MAX_TC_REDUNDANCY)
    {
      fprintf(stderr, "TC redundancy %d is not allowed\n", cnf->tc_redundancy);
      return -1;
    }

  /* MPR coverage */
  if(cnf->mpr_coverage < MIN_MPR_COVERAGE ||
     cnf->mpr_coverage > MAX_MPR_COVERAGE)
    {
      fprintf(stderr, "MPR coverage %d is not allowed\n", cnf->mpr_coverage);
      return -1;
    }

  if(cnf->allow_no_interfaces == OLSR_FALSE &&
     in == NULL)
    {
      fprintf(stderr, "No interfaces configured - and allow no int set to FALSE!\n");
      return -1;
    }

  /* Interfaces */
  while(in)
    {
      io = in->cnf;

      if(in->name == NULL || !strlen(in->name))
	{
	  fprintf(stderr, "Interface has no name!\n");
	  return -1;
	}

      if(io == NULL)
	{
	  fprintf(stderr, "Interface %s has no configuration!\n", in->name);
	  return -1;
	}

      /* HELLO interval */
      if(io->hello_params.emission_interval < cnf->pollrate ||
	 io->hello_params.emission_interval > io->hello_params.validity_time)
	{
	  fprintf(stderr, "Bad HELLO parameters! (em: %0.2f, vt: %0.2f)\n", io->hello_params.emission_interval, io->hello_params.validity_time);
	  return -1;
	}

      /* TC interval */
      if(io->tc_params.emission_interval < cnf->pollrate ||
	 io->tc_params.emission_interval > io->tc_params.validity_time)
	{
	  fprintf(stderr, "Bad TC parameters! (em: %0.2f, vt: %0.2f)\n", io->tc_params.emission_interval, io->tc_params.validity_time);
	  return -1;
	}

      /* MID interval */
      if(io->mid_params.emission_interval < cnf->pollrate ||
	 io->mid_params.emission_interval > io->mid_params.validity_time)
	{
	  fprintf(stderr, "Bad MID parameters! (em: %0.2f, vt: %0.2f)\n", io->mid_params.emission_interval, io->mid_params.validity_time);
	  return -1;
	}

      /* HNA interval */
      if(io->hna_params.emission_interval < cnf->pollrate ||
	 io->hna_params.emission_interval > io->hna_params.validity_time)
	{
	  fprintf(stderr, "Bad HNA parameters! (em: %0.2f, vt: %0.2f)\n", io->hna_params.emission_interval, io->hna_params.validity_time);
	  return -1;
	}

      in = in->next;
    }

  return 0;
}


void
olsrd_free_cnf(struct olsrd_config *cnf)
{
  struct hna4_entry        *h4d, *h4 = cnf->hna4_entries;
  struct hna6_entry        *h6d, *h6 = cnf->hna6_entries;
  struct olsr_if           *ind, *in = cnf->interfaces;
  struct plugin_entry      *ped, *pe = cnf->plugins;
  
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
      free(in->cnf);
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

  return;
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

  return cnf;
}




void
set_default_cnf(struct olsrd_config *cnf)
{
    memset(cnf, 0, sizeof(struct olsrd_config));
    
    cnf->debug_level = DEF_DEBUGLVL;
    cnf->ip_version  = AF_INET;
    cnf->allow_no_interfaces = DEF_ALLOW_NO_INTS;
    cnf->tos = DEF_TOS;
    cnf->willingness_auto = DEF_WILL_AUTO;
    cnf->ipc_connections = DEF_IPC_CONNECTIONS;
    cnf->open_ipc = cnf->ipc_connections ? OLSR_TRUE : OLSR_FALSE;

    cnf->use_hysteresis = DEF_USE_HYST;
    cnf->hysteresis_param.scaling = HYST_SCALING;
    cnf->hysteresis_param.thr_high = HYST_THRESHOLD_HIGH;
    cnf->hysteresis_param.thr_low = HYST_THRESHOLD_LOW;

    cnf->pollrate = DEF_POLLRATE;

    cnf->tc_redundancy = TC_REDUNDANCY;
    cnf->mpr_coverage = MPR_COVERAGE;
    cnf->lq_level = DEF_LQ_LEVEL;
    cnf->lq_wsize = DEF_LQ_WSIZE;
    cnf->clear_screen = DEF_CLEAR_SCREEN;
}




struct if_config_options *
get_default_if_config()
{
  struct if_config_options *io = malloc(sizeof(struct if_config_options));
  struct in6_addr in6;
 
  memset(io, 0, sizeof(struct if_config_options));

  io->ipv6_addrtype = 1; /* XXX - FixMe */

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
  struct plugin_param      *pp;
  struct ipc_host          *ih = cnf->ipc_hosts;
  struct ipc_net           *ie = cnf->ipc_nets;

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

  fprintf(fd, "#\n# Configuration file for olsr.org olsrd\n# automatically generated by olsrd-cnf %s\n#\n\n\n", PARSER_VERSION);

  /* Debug level */
  fprintf(fd, "# Debug level(0-9)\n# If set to 0 the daemon runs in the background\n\nDebugLevel\t%d\n\n", cnf->debug_level);

  /* IP version */
  if(cnf->ip_version == AF_INET6)
    fprintf(fd, "# IP version to use (4 or 6)\n\nIpVersion\t6\n\n");
  else
    fprintf(fd, "# IP version to use (4 or 6)\n\nIpVersion\t4\n\n");


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
  fprintf(fd, "# The fixed willingness to use(0-7)\n# If not set willingness will be calculated\n# dynammically based on battery/power status\n\n");
  if(cnf->willingness_auto)
    fprintf(fd, "#Willingness\t4\n\n");
  else
    fprintf(fd, "Willingness%d\n\n", cnf->willingness);

  /* IPC */
  fprintf(fd, "# Allow processes like the GUI front-end\n# to connect to the daemon.\n\n");
  fprintf(fd, "IpcConnect\n{\n");
  fprintf(fd, "   MaxConnections  %d\n\n", cnf->ipc_connections);

  while(ih)
    {
      in4.s_addr = ih->host.v4;
      fprintf(fd, "   Host          %s\n", inet_ntoa(in4));
      ih = ih->next;
    }
  fprintf(fd, "\n");
  while(ie)
    {
      in4.s_addr = ie->net.v4;
      fprintf(fd, "   Net           %s ", inet_ntoa(in4));
      in4.s_addr = ie->mask.v4;
      fprintf(fd, "%s\n", inet_ntoa(in4));
      ie = ie->next;
    }

  fprintf(fd, "}\n\n");



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
  fprintf(fd, "# Polling rate in seconds(float).\n# Auto uses default value 0.05 sec\n\n");
  fprintf(fd, "Pollrate\t%0.2f\n", cnf->pollrate);

  /* TC redundancy */
  fprintf(fd, "# TC redundancy\n# Specifies how much neighbor info should\n# be sent in TC messages\n# Possible values are:\n# 0 - only send MPR selectors\n# 1 - send MPR selectors and MPRs\n# 2 - send all neighbors\n#\n# defaults to 0\n\n");
  fprintf(fd, "TcRedundancy\t%d\n\n", cnf->tc_redundancy);

  /* MPR coverage */
  fprintf(fd, "# MPR coverage\n# Specifies how many MPRs a node should\n# try select to reach every 2 hop neighbor\n# Can be set to any integer >0\n# defaults to 1\n\n");

  fprintf(fd, "MprCoverage\t%d\n\n", cnf->mpr_coverage);

  fprintf(fd, "# Link quality level\n# 0 = do not use link quality\n# 1 = use link quality for MPR selection\n# 2 = use link quality for MPR selection and routing\n\n");
  fprintf(fd, "LinkQualityLevel\t%d\n\n", cnf->lq_level);

  fprintf(fd, "# Link quality window size\n\n");
  fprintf(fd, "LinkQualityWinSize\t%d\n\n", cnf->lq_wsize);

  fprintf(fd, "# Clear screen when printing debug output?\n\n");
  fprintf(fd, "ClearScreen\t%s\n\n", cnf->clear_screen ? "yes" : "no");

  /* Plugins */
  fprintf(fd, "# Olsrd plugins to load\n# This must be the absolute path to the file\n# or the loader will use the following scheme:\n# - Try the paths in the LD_LIBRARY_PATH \n#   environment variable.\n# - The list of libraries cached in /etc/ld.so.cache\n# - /lib, followed by /usr/lib\n\n");
  if(pe)
    {
      while(pe)
	{
	  fprintf(fd, "LoadPlugin \"%s\"\n{\n", pe->name);
          pp = pe->params;
          while(pp)
            {
              fprintf(fd, "    PlParam \"%s\" \"%s\"\n", pp->key, pp->value);
              pp = pp->next;
            }
	  fprintf(fd, "}\n");
	  pe = pe->next;
	}
    }
  fprintf(fd, "\n");

  
  

  /* Interfaces */
  fprintf(fd, "# Interfaces\n\n");
  /* Interfaces */
  if(in)
    {
      while(in)
	{
	  fprintf(fd, "Interface \"%s\"\n{\n", in->name);
	  fprintf(fd, "\n");
      
	  fprintf(fd, "    # IPv4 broadcast address to use. The\n    # one usefull example would be 255.255.255.255\n    # If not defined the broadcastaddress\n    # every card is configured with is used\n\n");

	  if(in->cnf->ipv4_broadcast.v4)
	    {
	      in4.s_addr = in->cnf->ipv4_broadcast.v4;
	      fprintf(fd, "    Ip4Broadcast\t %s\n\n", inet_ntoa(in4));
	    }
	  else
	    {
	      fprintf(fd, "    #Ip4Broadcast\t255.255.255.255\n\n");
	    }
	  
	  
	  fprintf(fd, "    # IPv6 address scope to use.\n    # Must be 'site-local' or 'global'\n\n");
	  if(in->cnf->ipv6_addrtype)
	    fprintf(fd, "    Ip6AddrType \tsite-local\n\n");
	  else
	    fprintf(fd, "    Ip6AddrType \tglobal\n\n");
	  
	  fprintf(fd, "    # IPv6 multicast address to use when\n    # using site-local addresses.\n    # If not defined, ff05::15 is used\n");
	  fprintf(fd, "    Ip6MulticastSite\t%s\n\n", (char *)inet_ntop(AF_INET6, &in->cnf->ipv6_multi_site.v6, ipv6_buf, sizeof(ipv6_buf)));
	  fprintf(fd, "    # IPv6 multicast address to use when\n    # using global addresses\n    # If not defined, ff0e::1 is used\n");
	  fprintf(fd, "    Ip6MulticastGlobal\t%s\n\n", (char *)inet_ntop(AF_INET6, &in->cnf->ipv6_multi_glbl.v6, ipv6_buf, sizeof(ipv6_buf)));
	  
	  
	  
	  fprintf(fd, "    # Emission and validity intervals.\n    # If not defined, RFC proposed values will\n    # in most cases be used.\n\n");
	  
	  
	  if(in->cnf->hello_params.emission_interval != HELLO_INTERVAL)
	    fprintf(fd, "    HelloInterval\t%0.2f\n", in->cnf->hello_params.emission_interval);
	  else
	    fprintf(fd, "    #HelloInterval\t%0.2f\n", in->cnf->hello_params.emission_interval);
	  if(in->cnf->hello_params.validity_time != NEIGHB_HOLD_TIME)
	    fprintf(fd, "    HelloValidityTime\t%0.2f\n", in->cnf->hello_params.validity_time);
	  else
	    fprintf(fd, "    #HelloValidityTime\t%0.2f\n", in->cnf->hello_params.validity_time);
	  if(in->cnf->tc_params.emission_interval != TC_INTERVAL)
	    fprintf(fd, "    TcInterval\t\t%0.2f\n", in->cnf->tc_params.emission_interval);
	  else
	    fprintf(fd, "    #TcInterval\t\t%0.2f\n", in->cnf->tc_params.emission_interval);
	  if(in->cnf->tc_params.validity_time != TOP_HOLD_TIME)
	    fprintf(fd, "    TcValidityTime\t%0.2f\n", in->cnf->tc_params.validity_time);
	  else
	    fprintf(fd, "    #TcValidityTime\t%0.2f\n", in->cnf->tc_params.validity_time);
	  if(in->cnf->mid_params.emission_interval != MID_INTERVAL)
	    fprintf(fd, "    MidInterval\t\t%0.2f\n", in->cnf->mid_params.emission_interval);
	  else
	    fprintf(fd, "    #MidInterval\t%0.2f\n", in->cnf->mid_params.emission_interval);
	  if(in->cnf->mid_params.validity_time != MID_HOLD_TIME)
	    fprintf(fd, "    MidValidityTime\t%0.2f\n", in->cnf->mid_params.validity_time);
	  else
	    fprintf(fd, "    #MidValidityTime\t%0.2f\n", in->cnf->mid_params.validity_time);
	  if(in->cnf->hna_params.emission_interval != HNA_INTERVAL)
	    fprintf(fd, "    HnaInterval\t\t%0.2f\n", in->cnf->hna_params.emission_interval);
	  else
	    fprintf(fd, "    #HnaInterval\t%0.2f\n", in->cnf->hna_params.emission_interval);
	  if(in->cnf->hna_params.validity_time != HNA_HOLD_TIME)
	    fprintf(fd, "    HnaValidityTime\t%0.2f\n", in->cnf->hna_params.validity_time);	  
	  else
	    fprintf(fd, "    #HnaValidityTime\t%0.2f\n", in->cnf->hna_params.validity_time);	  
	  
	  
	  
	  fprintf(fd, "}\n\n");
	  in = in->next;
	}

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
  struct ipc_host          *ih = cnf->ipc_hosts;
  struct ipc_net           *ie = cnf->ipc_nets;
  char ipv6_buf[100];             /* buffer for IPv6 inet_htop */
  struct in_addr in4;

  printf(" *** olsrd configuration ***\n");

  printf("Debug Level      : %d\n", cnf->debug_level);
  if(cnf->ip_version == AF_INET6)
    printf("IpVersion        : 6\n");
  else
    printf("IpVersion        : 4\n");
  if(cnf->allow_no_interfaces)
    printf("No interfaces    : ALLOWED\n");
  else
    printf("No interfaces    : NOT ALLOWED\n");
  printf("TOS              : 0x%02x\n", cnf->tos);
  if(cnf->willingness_auto)
    printf("Willingness      : AUTO\n");
  else
    printf("Willingness      : %d\n", cnf->willingness);

  printf("IPC connections  : %d\n", cnf->ipc_connections);

  while(ih)
    {
      in4.s_addr = ih->host.v4;
      printf("\tHost %s\n", inet_ntoa(in4));
      ih = ih->next;
    }
  
  while(ie)
    {
      in4.s_addr = ie->net.v4;
      printf("\tNet %s/", inet_ntoa(in4));
      in4.s_addr = ie->mask.v4;
      printf("%s\n", inet_ntoa(in4));
      ie = ie->next;
    }


  printf("Pollrate         : %0.2f\n", cnf->pollrate);

  printf("TC redundancy    : %d\n", cnf->tc_redundancy);

  printf("MPR coverage     : %d\n", cnf->mpr_coverage);
   
  printf("LQ level         : %d\n", cnf->lq_level);

  printf("LQ window size   : %d\n", cnf->lq_wsize);

  printf("Clear screen     : %s\n", cnf->clear_screen ? "yes" : "no");

  /* Interfaces */
  if(in)
    {
      printf("Interfaces:\n");
      while(in)
	{
	  printf(" dev: \"%s\"\n", in->name);
	  
	  if(in->cnf->ipv4_broadcast.v4)
	    {
	      in4.s_addr = in->cnf->ipv4_broadcast.v4;
	      printf("\tIPv4 broadcast        : %s\n", inet_ntoa(in4));
	    }
	  else
	    {
	      printf("\tIPv4 broadcast        : AUTO\n");
	    }
	  
	  if(in->cnf->ipv6_addrtype)
	    printf("\tIPv6 addrtype         : site-local\n");
	  else
	    printf("\tIPv6 addrtype         : global\n");
	  
	  //union olsr_ip_addr       ipv6_multi_site;
	  //union olsr_ip_addr       ipv6_multi_glbl;
	  printf("\tIPv6 multicast site   : %s\n", (char *)inet_ntop(AF_INET6, &in->cnf->ipv6_multi_site.v6, ipv6_buf, sizeof(ipv6_buf)));
	  printf("\tIPv6 multicast global : %s\n", (char *)inet_ntop(AF_INET6, &in->cnf->ipv6_multi_glbl.v6, ipv6_buf, sizeof(ipv6_buf)));
	  
	  printf("\tHELLO emission int    : %0.2f\n", in->cnf->hello_params.emission_interval);
	  printf("\tHELLO validity time   : %0.2f\n", in->cnf->hello_params.validity_time);
	  printf("\tTC emission int       : %0.2f\n", in->cnf->tc_params.emission_interval);
	  printf("\tTC validity time      : %0.2f\n", in->cnf->tc_params.validity_time);
	  printf("\tMID emission int      : %0.2f\n", in->cnf->mid_params.emission_interval);
	  printf("\tMID validity time     : %0.2f\n", in->cnf->mid_params.validity_time);
	  printf("\tHNA emission int      : %0.2f\n", in->cnf->hna_params.emission_interval);
	  printf("\tHNA validity time     : %0.2f\n", in->cnf->hna_params.validity_time);
	  
	  
	  
	  in = in->next;

	}
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

void *olsrd_cnf_malloc(unsigned int len)
{
  return malloc(len);
}

void olsrd_cnf_free(void *addr)
{
  free(addr);
}
