
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
 * $ Id $
 *
 */
 

#include "defs.h"
#include "configfile.h"
#include "local_hna_set.h"
#include "olsr.h"
#include "plugin_loader.h"
#include <string.h>
#include <stdlib.h>


/**
 *Funtion that tries to read and parse the config
 *file "filename"
 *@param filename the name(full path) of the config file
 *@return negative on error
 */
int
read_config_file(char *filename)
{
  FILE *conf_file;
  char line[CONFIG_MAX_LINESIZE];
  char *linebuf, *firstbuf;
  char tmp[FILENAME_MAX];
  int ipv, prefix6, tmp_debug_level;
  char addr[20], mask[20];
  char addr6[50];
  struct in_addr in;
  struct in6_addr in6;
  union olsr_ip_addr net;
  union hna_netmask netmask;

  conf_file = fopen(filename, "r");

  olsr_printf(2, "Trying to read configuration from %s...", filename);

  if(conf_file == NULL)
    {
      olsr_printf(2, "failed\n");
      return -1;
    }

  olsr_printf(2, "OK\n\n");

  /*output:
   *Description: (at position 28)<value>
   */

  while(fgets(line, CONFIG_MAX_LINESIZE, conf_file))
    {
      if((line[0] != ' ') && 
	 (line[0] != '#') && 
	 (line[0] != '\n'))
	{
	  olsr_printf(5, "parsing: %s", line);

	  /*
	   * IP version
	   */
	  if(strncmp(line, "IPVERSION", 9) == 0)
	    {
	      sscanf(&line[9], "%1d", &ipv);
	      switch(ipv)
		{
		case(4):
		  ipversion = AF_INET;
		  break;
		case(6):
		  ipversion = AF_INET6;
		  break;
		default:
		  fprintf(stderr, "configfile: IPVERSION parsing failed(%d)!\n", ipv);
		  break;
		}
	      olsr_printf(2, "IPVERSION:                 %d\n", ipv);
	      continue;
	    }

	  /* 
	   * Interfaces 
	   */
	  if(strncmp(line, "INTERFACES", 10) == 0)
	    {

	      linebuf = olsr_malloc(strlen(&line[10] + 1), "Config");
	      firstbuf = linebuf;
	      linebuf = strtok(&line[10], " \t\n");

	      /* Interfaces to run on */
	      while(linebuf != NULL)
		{
		  option_i = 1;
		  /* Insert into queue */
		  queue_if(linebuf);

		  olsr_printf(2, "adding interface:          %s\n", linebuf);

		  linebuf = strtok(NULL, " \t\n");		  
		}

	      free(firstbuf);
	      continue;
	    }

	  /* 
	   * Run if no iterfaces are present? 
	   */
	  if(strncmp(line, "ALLOW_NO_INT", 12) == 0)
	    {
              sscanf(&line[12], "%s", tmp);
              if(strncmp(tmp, "yes", 3) == 0)
		{
		  allow_no_int = 1;
		}
	      else
		{
		  allow_no_int = 0;
		}

	      olsr_printf(2, "Allow no interfaces:       %s\n", tmp);

              continue;
	    }

	  /*
	   * HNA
	   */
	  if(strncmp(line, "HNA4", 4) == 0)
	    {
	      sscanf(&line[4], "%s %s", addr, mask);
	      if (inet_aton(addr, &in) == 0)
		{
		  fprintf(stderr, "Error parsing HNA addr %s from configfile\n", addr);
		  olsr_exit(__func__, EXIT_FAILURE);
		}
	      memcpy(&net, &in.s_addr, sizeof(olsr_u32_t));
	      if (inet_aton(mask, &in) == 0)
		{
		  fprintf(stderr, "Error parsing HNA netmask %s from configfile\n", addr);
		  olsr_exit(__func__, EXIT_FAILURE);
		}
	      memcpy(&netmask, &in.s_addr, sizeof(olsr_u32_t));

	      olsr_printf(2, "HNA:                       %s/%s\n", addr, mask);	    
	      
	      add_local_hna4_entry(&net, &netmask);
	      continue;
	    }


	  /*
	   * HNA IPv6
	   */
	  if(strncmp(line, "HNA6", 4) == 0)
	    {
	      memset(addr6, 0, 50);
	      sscanf(&line[4], "%s %d", addr6, &prefix6);

	      //prefix6 = atol(&line[4 + strlen(addr6)]);
	      if(inet_pton(AF_INET6, addr6, &in6) < 0)
		{
		  fprintf(stderr, "Error parsing HNA INET6 addr %s from configfile\n", addr6);
		  olsr_exit(__func__, EXIT_FAILURE);
		}
	      memcpy(&net, &in6, sizeof(struct in6_addr));

	      /* Create the netmask */
	      if((prefix6 < 0) || (prefix6 > 128))
		{
		  fprintf(stderr, "Error parsing HNA INET6 prefix %d for %s from configfile\n", prefix6, addr6);
		  olsr_exit(__func__, EXIT_FAILURE);
		}	      
	      //olsr_printf(2, "HNA6: %s\n", (char *)inet_ntop(AF_INET6, &in6, addr6, 50));
	      //if (inet_aton(mask, &in) == 0) continue;
	      //memcpy(&h.hna_net.netmask, &in.s_addr, sizeof(olsr_u32_t));
	      

	      olsr_printf(2, "HNA6:                      %s/%d\n", addr6, prefix6);	    

	      netmask.v6 = prefix6;

	      add_local_hna6_entry(&net, &netmask);

	      continue;
	    }


	  /*
	   *Pollrate
	   */
          if(strncmp(line, "POLLRATE", 8) == 0)
            {
              sscanf(&line[8], "%4s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[8], "%4f", &polling_int);

	      olsr_printf(2, "Polling rate:              %s\n", tmp);

              continue;
            }



	  /*
	   *DEBUGLEVEL
	   */
          if(strncmp(line, "DEBUG", 5) == 0)
            {
              sscanf(&line[5], "%d", &tmp_debug_level);
	      if((tmp_debug_level < 0) || (tmp_debug_level > 9))
		{
		  fprintf(stderr, "Wrong DEBUG value \"%d\" in config file!\n", debug_level);
		  olsr_exit(__func__, EXIT_FAILURE);
		}
	      debug_level = tmp_debug_level;
	      olsr_printf(2, "Debug level:               %d\n", debug_level);

              continue;
            }

	  /*
	   * IPv4 broadcast 255.255.255.255 or interface detection
	   */
          if(strncmp(line, "IP4BROAD", 8) == 0)
            {
              sscanf(&line[8], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		{
		  if (inet_aton(tmp, &in) == 0)
		    {
		      olsr_printf(2, "Invalid broadcast address! %s\nSkipping it!\n", tmp);
		      continue;
		    }

		  bcast_set = 1;
		 
		  memcpy(&bcastaddr.sin_addr, &in.s_addr, sizeof(olsr_u32_t));

		}

	      olsr_printf(2, "IPv4 broadcast:            %s\n", tmp);

              continue;
            }


	  /*
	   * IPv6 address to prioritize
	   */
          if(strncmp(line, "IP6ADDRTYPE", 11) == 0)
            {
              sscanf(&line[11], "%s", tmp);
              if(strncmp(tmp, "site-local", 10) == 0)
		{
		  ipv6_addrtype = IPV6_ADDR_SITELOCAL;
		}
              else
		{
		  if(strncmp(tmp, "global", 6) == 0)
		    {
		      ipv6_addrtype = 0;
		    }
		  else
		    {
		      fprintf(stderr, "Error parsing IPv6 type \"%s\" from configfile\n", tmp);
		      exit(1);
		    }
		}

	      olsr_printf(2, "IPv6 addrtype:             %s\n", tmp);

              continue;
            }


	  /*
	   * IPv6 multicast address for site local interfaces
	   */
          if(strncmp(line, "IP6MULTI-SITE", 13) == 0)
            {
              sscanf(&line[13], "%s", tmp);
              if(strncmp(tmp, "auto", 4) == 0)
		strncpy(ipv6_mult_site, OLSR_IPV6_MCAST_SITE_LOCAL, strlen(OLSR_IPV6_MCAST_SITE_LOCAL));
	      else
		strncpy(ipv6_mult_site, tmp, strlen(tmp));

	      olsr_printf(2, "IPv6 multicast site-local: %s\n", ipv6_mult_site);

              continue;
            }


	  /*
	   * IPv6 multicast address for global interfaces
	   */
          if(strncmp(line, "IP6MULTI-GLOBAL", 15) == 0)
            {
              sscanf(&line[15], "%s", tmp);
              if(strncmp(tmp, "auto", 4) == 0)
		strncpy(ipv6_mult_global, OLSR_IPV6_MCAST_GLOBAL, strlen(OLSR_IPV6_MCAST_GLOBAL));
	      else
		strncpy(ipv6_mult_global, tmp, strlen(tmp));

	      olsr_printf(2, "IPv6 multicast global:     %s\n", ipv6_mult_global);

              continue;
            }



	  /*
	   * Hello interval
	   */
          if(strncmp(line, "HELLOINT", 8) == 0)
            {
              sscanf(&line[8], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[8], "%f", &hello_int);


	      olsr_printf(2, "Hello interval:            %s\n", tmp);

              continue;
            }


	  /*
	   * Hello hold multiplier
	   */
          if(strncmp(line, "HELLOMULTI", 10) == 0)
            {
              sscanf(&line[10], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[10], "%d", &neighbor_timeout_mult);


	      olsr_printf(2, "Hello multiplier:          %s\n", tmp);

              continue;
            }


	  /*
	   * MID interval
	   */
          if(strncmp(line, "MIDINT", 6) == 0)
            {
              sscanf(&line[6], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(tmp, "%f", &mid_int);


	      olsr_printf(2, "MID interval:              %s\n", tmp);

              continue;
            }


	  /*
	   * MID hold multiplier
	   */
          if(strncmp(line, "MIDMULTI", 8) == 0)
            {
              sscanf(&line[8], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(tmp, "%d", &hna_timeout_mult);


	      olsr_printf(2, "HNA multiplier:            %s\n", tmp);

              continue;
            }


	  /*
	   * HNA interval
	   */
          if(strncmp(line, "HNAINT", 6) == 0)
            {
              sscanf(&line[6], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(tmp, "%f", &hna_int);


	      olsr_printf(2, "HNA interval:              %s\n", tmp);

              continue;
            }


	  /*
	   * HNA hold multiplier
	   */
          if(strncmp(line, "HNAMULTI", 8) == 0)
            {
              sscanf(&line[8], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(tmp, "%d", &hna_timeout_mult);


	      olsr_printf(2, "HNA multiplier:            %s\n", tmp);

              continue;
            }



	  /*
	   * Hello non-wlan interval
	   */
          if(strncmp(line, "NWHELLOINT", 10) == 0)
            {
              sscanf(&line[10], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		{
		  sscanf(&line[10], "%f", &hello_int_nw);
		}

	      olsr_printf(2, "Non-WLAN HELLO interval:   %s\n", tmp);

              continue;
            }

	  /*
	   * Hello hold multiplier non-WLAN
	   */
          if(strncmp(line, "NWHELLOMULTI", 12) == 0)
            {
              sscanf(&line[12], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[12], "%d", &neighbor_timeout_mult_nw);


	      olsr_printf(2, "Hello multiplier non-WLAN: %s\n", tmp);

              continue;
            }



	  /*
	   * TC interval
	   */
          if(strncmp(line, "TCINT", 5) == 0)
            {
              sscanf(&line[5], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[5], "%f", &tc_int);


	      olsr_printf(2, "TC interval:               %s\n", tmp);

              continue;
            }


	  /*
	   * TC hold multiplier
	   */
          if(strncmp(line, "TCMULTI", 7) == 0)
            {
              sscanf(&line[7], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[7], "%d", &topology_timeout_mult);


	      olsr_printf(2, "TC multiplier:             %s\n", tmp);

              continue;
            }


	  /*
	   * Type Of Service
	   */
          if(strncmp(line, "TOSVALUE", 8) == 0)
            {
              sscanf(&line[8], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[8], "%d", (int *)&tos);


	      olsr_printf(2, "TOS:                       %s\n", tmp);

              continue;
            }

	  /*
	   * Willingness
	   */
          if(strncmp(line, "WILLINGNESS", 11) == 0)
            {
              sscanf(&line[11], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		{
		  sscanf(&line[11], "%d", &my_willingness);
		  if((0 > my_willingness) || (my_willingness > 7))
		    {
		      fprintf(stderr, "Error setting willingness! Bad value: %d\n", my_willingness);
		    }
		  else
		    {
		      willingness_set = 1;
		    }
		}

	      olsr_printf(2, "Willingness:               %s\n", tmp);

              continue;
            }


	  /*
	   * Willingness
	   */
          if(strncmp(line, "IPC-CONNECT", 11) == 0)
            {
              sscanf(&line[11], "%s", tmp);
              if(strncmp(tmp, "yes", 3) == 0)
		{
		  use_ipc = 1;
		}
	      olsr_printf(2, "IPC connections:           %s\n", tmp);

              continue;
            }



	  /*
	   *Hysteresis usage
	   */
          if(strncmp(line, "USE_HYSTERESIS", 14) == 0)
            {
              sscanf(&line[14], "%s", tmp);
              if(strncmp(tmp, "yes", 3) == 0)
		use_hysteresis = 1;
              if(strncmp(tmp, "no", 2) == 0)
		use_hysteresis = 0;

	      olsr_printf(2, "Use hysteresis:            %s\n", tmp);

              continue;
            }


	  /*
	   *Hysteresis scaling
	   */
          if(strncmp(line, "HYST_SCALING", 12) == 0)
            {
              sscanf(&line[12], "%4s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[12], "%4f", &hyst_scaling);

	      olsr_printf(2, "Hyst scaling:              %s\n", tmp);

              continue;
            }


	  /*
	   *Hysteresis low threshold
	   */
          if(strncmp(line, "HYST_THR_LOW", 12) == 0)
            {
              sscanf(&line[12], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[12], "%4f", &hyst_threshold_low);

	      olsr_printf(2, "Hyst threshold low:        %s\n", tmp);

              continue;
            }

	  /*
	   *Hysteresis high threshold
	   */
          if(strncmp(line, "HYST_THR_HIGH", 13) == 0)
            {
              sscanf(&line[13], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[13], "%4f", &hyst_threshold_high);

	      olsr_printf(2, "Hyst threshold high:       %s\n", tmp);

              continue;
            }


	  /*
	   *Topology redundancy
	   */
          if(strncmp(line, "TC_REDUNDANCY", 13) == 0)
            {
              sscanf(&line[13], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[13], "%d", &tc_redundancy);

	      olsr_printf(2, "TC redunanacy:             %s\n", tmp);

              continue;
            }


	  /*
	   *MPR redundancy
	   */
          if(strncmp(line, "MPR_COVERAGE", 12) == 0)
            {
              sscanf(&line[12], "%s", tmp);
              if(strncmp(tmp, "auto", 4) != 0)
		sscanf(&line[12], "%d", &mpr_coverage);

	      olsr_printf(2, "MPR coverage:              %s\n", tmp);

              continue;
            }



	  /*
	   *PLUGIN
	   */
          if(strncmp(line, "LOAD_PLUGIN", 11) == 0)
            {
              sscanf(&line[11], "%s", tmp);

	      olsr_add_plugin(tmp);

	      olsr_printf(2, "PLUGIN:                    %s\n", tmp);

              continue;
            }


	  //olsr_syslog(OLSR_LOG_ERR, "Could not parse config file(%s) line:\"%s\"", filename, line);

	  olsr_printf(1, "Could not parse config file(%s) line: %s", filename, line);
	}
      else
	olsr_printf(5, "Skipping: %s", line);
    }


  fclose(conf_file);
  return 0;
}


