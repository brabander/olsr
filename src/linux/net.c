/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tønnesen(andreto@olsr.org)
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
 * $Id: net.c,v 1.10 2004/11/21 10:50:52 kattemat Exp $
 */


/*
 * Linux spesific code
 */

#include "net.h"
#include "../defs.h"
#include "../net_os.h"

/**
 *Bind a socket to a device
 *
 *@param sock the socket to bind
 *@param dev_name name of the device
 *
 *@return negative if error
 */

int
bind_socket_to_device(int sock, char *dev_name)
{
  /*
   *Bind to device using the SO_BINDTODEVICE flag
   */
  olsr_printf(3, "Binding socket %d to device %s\n", sock, dev_name);
  return setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, dev_name, strlen(dev_name)+1);

}




/**
 *Enable IP forwarding.
 *Just writing "1" to the /proc/sys/net/ipv4/ip_forward
 *if using IPv4 or /proc/sys/net/ipv6/conf/all/forwarding
 *if using IPv6.
 *Could probably drop the check for
 *"0" here and write "1" anyways.
 *
 *@param version IP version.
 *
 *@return 1 on sucess 0 on failiure
 */ 
int
enable_ip_forwarding(int version)
{
  FILE *proc_fwd;
  char procfile[FILENAME_MAX];

  if(version == AF_INET)
    {
      strcpy(procfile, "/proc/sys/net/ipv4/ip_forward");
    }
  else
    if(version == AF_INET6)
      {
	strcpy(procfile, "/proc/sys/net/ipv6/conf/all/forwarding");
      }
    else
      return -1;


  if ((proc_fwd=fopen(procfile, "r"))==NULL)
    {
      /* IPv4 */
      if(version == AF_INET)
	fprintf(stderr, "WARNING! Could not open the %s file to check/enable IP forwarding!\nAre you using the procfile filesystem?\nDoes your system support IPv4?\nI will continue(in 3 sec) - but you should mannually ensure that IP forwarding is enabeled!\n\n", procfile);
      /* IPv6 */
      else
	fprintf(stderr, "WARNING! Could not open the %s file to check/enable IP forwarding!\nAre you using the procfile filesystem?\nDoes your system support IPv6?\nI will continue(in 3 sec) - but you should mannually ensure that IP forwarding is enabeled!\n\n", procfile);
      
      sleep(3);
      return 0;
    }
  
  else
    {
      orig_fwd_state = fgetc(proc_fwd);
      fclose(proc_fwd);
      if(orig_fwd_state == '1')
	{
	  olsr_printf(3, "\nIP forwarding is enabled on this system\n");
	}
      else
	{
	  if ((proc_fwd=fopen(procfile, "w"))==NULL)
	    {
	      fprintf(stderr, "Could not open %s for writing!\n", procfile);
	      fprintf(stderr, "I will continue(in 3 sec) - but you should mannually ensure that IP forwarding is enabeled!\n\n");
	      sleep(3);
	      return 0;
	    }
	  else
	    {
	      syslog(LOG_INFO, "Writing \"1\" to %s\n", procfile);
	      fputs("1", proc_fwd);
	    }
	  fclose(proc_fwd);

	}
    }
  return 1;
      
}


/**
 *
 *@return 1 on sucess 0 on failiure
 */ 
int
disable_redirects(char *if_name, int index, int version)
{
  FILE *proc_redirect;
  char procfile[FILENAME_MAX];

  if(version == AF_INET6)
    return -1;

  /* Generate the procfile name */
  sprintf(procfile, REDIRECT_PROC, if_name);


  if((proc_redirect = fopen(procfile, "r")) == NULL)
    {
      fprintf(stderr, "WARNING! Could not open the %s file to check/disable ICMP redirects!\nAre you using the procfile filesystem?\nDoes your system support IPv4?\nI will continue(in 3 sec) - but you should mannually ensure that ICMP redirects are disabled!\n\n", procfile);
      
      sleep(3);
      return 0;
    }
  else
    {
      nic_states[index].redirect = fgetc(proc_redirect);
      fclose(proc_redirect);
      
    }

  if ((proc_redirect = fopen(procfile, "w"))==NULL)
    {
      fprintf(stderr, "Could not open %s for writing!\n", procfile);
      fprintf(stderr, "I will continue(in 3 sec) - but you should mannually ensure that ICMP redirect is disabeled!\n\n");
      sleep(3);
      return 0;
    }
  else
    {
      syslog(LOG_INFO, "Writing \"0\" to %s", procfile);
      fputs("0", proc_redirect);
    }
  fclose(proc_redirect);

  return 1;
}



/**
 *
 *@return 1 on sucess 0 on failiure
 */ 
int
deactivate_spoof(char *if_name, int index, int version)
{
  FILE *proc_spoof;
  char procfile[FILENAME_MAX];

  if(version == AF_INET6)
    return -1;


  /* Generate the procfile name */
  sprintf(procfile, SPOOF_PROC, if_name);


  if((proc_spoof = fopen(procfile, "r")) == NULL)
    {
      fprintf(stderr, "WARNING! Could not open the %s file to check/disable the IP spoof filter!\nAre you using the procfile filesystem?\nDoes your system support IPv4?\nI will continue(in 3 sec) - but you should mannually ensure that IP spoof filtering is disabled!\n\n", procfile);
      
      sleep(3);
      return 0;
    }
  else
    {
      nic_states[index].spoof = fgetc(proc_spoof);
      fclose(proc_spoof);
      
    }

  if ((proc_spoof = fopen(procfile, "w")) == NULL)
    {
      fprintf(stderr, "Could not open %s for writing!\n", procfile);
      fprintf(stderr, "I will continue(in 3 sec) - but you should mannually ensure that IP spoof filtering is disabeled!\n\n");
      sleep(3);
      return 0;
    }
  else
    {
      syslog(LOG_INFO, "Writing \"0\" to %s", procfile);
      fputs("0", proc_spoof);
    }
  fclose(proc_spoof);

  return 1;
}



/**
 *Resets the spoof filter and ICMP redirect settings
 */

int
restore_settings(int version)
{
  FILE *proc_fd;
  char procfile[FILENAME_MAX];
  struct interface *ifs;

  olsr_printf(1, "Restoring network state\n");

  /* Restore IP forwarding to "off" */
  if(orig_fwd_state == '0')
    {
      if(version == AF_INET)
	{
	  strcpy(procfile, "/proc/sys/net/ipv4/ip_forward");
	}
      else if(version == AF_INET6)
	{
	  strcpy(procfile, "/proc/sys/net/ipv6/conf/all/forwarding");
	}

      if ((proc_fd = fopen(procfile, "w")) == NULL)
	{
	  fprintf(stderr, "Could not open %s for writing!\nSettings not restored!\n", procfile);
	}
      else
	{
	  syslog(LOG_INFO, "Resetting %s to %c\n", procfile, orig_fwd_state);
	  fputc(orig_fwd_state, proc_fd);
	  fclose(proc_fd);
	}

    }

  if(version == AF_INET6)
    return 0;

  for(ifs = ifnet; ifs != NULL; ifs = ifs->int_next)
    {
      /* ICMP redirects */
      
      /* Generate the procfile name */
      sprintf(procfile, REDIRECT_PROC, ifs->int_name);
      
      if ((proc_fd = fopen(procfile, "w")) == NULL)
	{
	  fprintf(stderr, "Could not open %s for writing!\nSettings not restored!\n", procfile);
	}
      else
	{
	  syslog(LOG_INFO, "Resetting %s to %c\n", procfile, nic_states[ifs->if_nr].redirect);

	  fputc(nic_states[ifs->if_nr].redirect, proc_fd);
	  fclose(proc_fd);
	}

      
      /* Spoof filter */
      
      /* Generate the procfile name */
      sprintf(procfile, SPOOF_PROC, ifs->int_name);

      if ((proc_fd = fopen(procfile, "w")) == NULL)
	{
	  fprintf(stderr, "Could not open %s for writing!\nSettings not restored!\n", procfile);
	}
      else
	{
	  syslog(LOG_INFO, "Resetting %s to %c\n", procfile, nic_states[ifs->if_nr].spoof);

	  fputc(nic_states[ifs->if_nr].spoof, proc_fd);
	  fclose(proc_fd);
	}

    }
  return 1;

}



/**
 *Creates a nonblocking broadcast socket.
 *@param sa sockaddr struct. Used for bind(2).
 *@return the FD of the socket or -1 on error.
 */
int
getsocket(struct sockaddr *sa, int bufspace, char *int_name)
{
  struct sockaddr_in *sin=(struct sockaddr_in *)sa;
  int sock, on = 1;



  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
      perror("socket");
      syslog(LOG_ERR, "socket: %m");
      return (-1);
    }



#ifdef SO_BROADCAST
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) < 0)
    {
      perror("setsockopt");
      syslog(LOG_ERR, "setsockopt SO_BROADCAST: %m");
      close(sock);
      return (-1);
    }
#endif

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      return (-1);
    }



#ifdef SO_RCVBUF

  for (on = bufspace; ; on -= 1024) 
    {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		     &on, sizeof (on)) == 0)
	break;
      if (on <= 8*1024) 
	{
	  perror("setsockopt");
	  syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
	  break;
	}
    }


#endif


  /*
   * WHEN USING KERNEL 2.6 THIS MUST HAPPEN PRIOR TO THE PORT BINDING!!!!
   */

  /* Bind to device */
  if(bind_socket_to_device(sock, int_name) < 0)
    {
      fprintf(stderr, "Could not bind socket to device... exiting!\n\n");
      syslog(LOG_ERR, "Could not bind socket to device... exiting!\n\n");
      return -1;
    }


  if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) 
    {
      perror("bind");
      syslog(LOG_ERR, "bind: %m");
      close(sock);
      return (-1);
    }

  /*
   *One should probably fetch the flags first
   *using F_GETFL....
   */
  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");

  return (sock);
}


/**
 *Creates a nonblocking IPv6 socket
 *@param sin sockaddr_in6 struct. Used for bind(2).
 *@return the FD of the socket or -1 on error.
 */
int
getsocket6(struct sockaddr_in6 *sin, int bufspace, char *int_name)
{
  int sock, on = 1;



  if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) 
    {
      perror("socket");
      syslog(LOG_ERR, "socket: %m");
      return (-1);
    }



  //#ifdef SO_BROADCAST
  /*
  if (setsockopt(sock, SOL_SOCKET, SO_MULTICAST, &on, sizeof (on)) < 0)
    {
      perror("setsockopt");
      syslog(LOG_ERR, "setsockopt SO_BROADCAST: %m");
      close(sock);
      return (-1);
    }
  */
  //#endif




#ifdef SO_RCVBUF
  for (on = bufspace; ; on -= 1024) 
    {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		     &on, sizeof (on)) == 0)
	break;
      if (on <= 8*1024) 
	{
	  perror("setsockopt");
	  syslog(LOG_ERR, "setsockopt SO_RCVBUF: %m");
	  break;
	}
    }


#endif

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) 
    {
      perror("SO_REUSEADDR failed");
      return (-1);
    }


  /*
   * WHEN USING KERNEL 2.6 THIS MUST HAPPEN PRIOR TO THE PORT BINDING!!!!
   */

  /* Bind to device */
  if(bind_socket_to_device(sock, int_name) < 0)
    {
      fprintf(stderr, "Could not bind socket to device... exiting!\n\n");
      syslog(LOG_ERR, "Could not bind socket to device... exiting!\n\n");
      return -1;
    }


  if (bind(sock, (struct sockaddr *)sin, sizeof (*sin)) < 0) 
    {
      perror("bind");
      syslog(LOG_ERR, "bind: %m");
      close(sock);
      return (-1);
    }

  /*
   *One should probably fetch the flags first
   *using F_GETFL....
   */
  if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    syslog(LOG_ERR, "fcntl O_NONBLOCK: %m\n");



  return (sock);
}


/*
 *From net-tools lib/interface.c
 *
 */

int
get_ipv6_address(char *ifname, struct sockaddr_in6 *saddr6, int scope_in)
{
  char addr6[40], devname[IFNAMSIZ];
  char addr6p[8][5];
  int plen, scope, dad_status, if_idx;
  FILE *f;
  struct sockaddr_in6 tmp_sockaddr6;

  if ((f = fopen(_PATH_PROCNET_IFINET6, "r")) != NULL) 
    {
      while (fscanf(f, "%4s%4s%4s%4s%4s%4s%4s%4s %02x %02x %02x %02x %20s\n",
		    addr6p[0], addr6p[1], addr6p[2], addr6p[3],
		    addr6p[4], addr6p[5], addr6p[6], addr6p[7],
		    &if_idx, &plen, &scope, &dad_status, devname) != EOF) 
	{
	  if (!strcmp(devname, ifname)) 
	    {
	      sprintf(addr6, "%s:%s:%s:%s:%s:%s:%s:%s",
		      addr6p[0], addr6p[1], addr6p[2], addr6p[3],
		      addr6p[4], addr6p[5], addr6p[6], addr6p[7]);
	      olsr_printf(2, "\tinet6 addr: %s\n", addr6);
	      olsr_printf(2, "\tScope:");
	      if(scope == scope_in)
		{
		  olsr_printf(3, "IPv6 addr:\n");
		  inet_pton(AF_INET6,addr6,&tmp_sockaddr6);
		  memcpy(&saddr6->sin6_addr, &tmp_sockaddr6, sizeof(struct in6_addr));	  
		  fclose(f);
		  return 1;
		}
	    }
	}
      fclose(f);
    }
  
  return 0;
}
