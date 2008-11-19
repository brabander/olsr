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

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */

#include "olsr.h"
#include "olsrd_plugin.h"
#include "olsr_cfg.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#ifndef WIN32
#include <arpa/nameser.h>
#endif

#include "olsrd_httpinfo.h"


int http_port = 0;
int resolve_ip_addresses = 0;
struct ip_prefix_list *allowed_nets = NULL;

static void my_init(void) __attribute__ ((constructor));
static void my_fini(void) __attribute__ ((destructor));

static int add_plugin_ipnet4(const char *value, void *data, set_plugin_parameter_addon);
static int add_plugin_ipnet6(const char *value, void *data, set_plugin_parameter_addon);
static int add_plugin_ipaddr4(const char *value, void *data, set_plugin_parameter_addon);
static int add_plugin_ipaddr6(const char *value, void *data, set_plugin_parameter_addon);

static int insert_plugin_ipnet(sa_family_t ip_version, const char *sz_net, const char *sz_mask, struct ip_prefix_list **all_nets);
static int add_plugin_ipnet(sa_family_t ip_version, const char *value, struct ip_prefix_list **all_nets);

/*
 * Defines the version of the plugin interface that is used
 * THIS IS NOT THE VERSION OF YOUR PLUGIN!
 * Do not alter unless you know what you are doing!
 */
int olsrd_plugin_interface_version(void)
{
    return PLUGIN_INTERFACE_VERSION;
}

/**
 *Constructor
 */
static void my_init(void)
{
    /* Print plugin info to stdout */
    printf("%s\n", MOD_DESC);
}

/**
 *Destructor
 */
static void my_fini(void)
{
    /* Calls the destruction function
     * olsr_plugin_exit()
     * This function should be present in your
     * sourcefile and all data destruction
     * should happen there - NOT HERE!
     */
    olsr_plugin_exit();
}

static const struct olsrd_plugin_parameters plugin_parameters[] = {
    { .name = "port",    .set_plugin_parameter = &set_plugin_port,     .data = &http_port },
    { .name = "host4",   .set_plugin_parameter = &add_plugin_ipaddr4,  .data = &allowed_nets },
    { .name = "net4",    .set_plugin_parameter = &add_plugin_ipnet4,   .data = &allowed_nets },
    { .name = "host",    .set_plugin_parameter = &add_plugin_ipaddr4,  .data = &allowed_nets },
    { .name = "net",     .set_plugin_parameter = &add_plugin_ipnet4,   .data = &allowed_nets },
    { .name = "host6",   .set_plugin_parameter = &add_plugin_ipaddr6,  .data = &allowed_nets },
    { .name = "net6",    .set_plugin_parameter = &add_plugin_ipnet6,   .data = &allowed_nets },
    { .name = "resolve", .set_plugin_parameter = &set_plugin_boolean,  .data = &resolve_ip_addresses },
};

void olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
    *params = plugin_parameters;
    *size = sizeof(plugin_parameters)/sizeof(*plugin_parameters);
}

static int insert_plugin_ipnet(sa_family_t ip_version, const char *sz_net, const char *sz_mask, struct ip_prefix_list **all_nets)
{
    union olsr_ip_addr net;
    olsr_u8_t prefix_len;
    
    if (inet_pton(ip_version, sz_net, &net) <= 0) {
        return 1;
    }
    if (ip_version == AF_INET) {
        union olsr_ip_addr netmask;
        if(inet_pton(AF_INET, sz_mask, &netmask) <= 0) {
            return 1;
        }
        prefix_len = olsr_netmask_to_prefix(&netmask);
    } else {
        char *endptr;
        prefix_len = strtoul(sz_mask, &endptr, 10);
        if(*endptr != '\0' || prefix_len > 128) {
            return 1;
        }
    }

    if (ip_version != olsr_cnf->ip_version) {
        if (ip_version == AF_INET) {
            memmove(&net.v6.s6_addr[12], &net.v4.s_addr, sizeof(in_addr_t));
            memset(&net.v6.s6_addr[0], 0x00, 10 * sizeof(uint8_t));
            memset(&net.v6.s6_addr[10], 0xff, 2 * sizeof(uint8_t));
            prefix_len += 96;
        }
        else {
            return 0;
        }
    }

    ip_prefix_list_add(all_nets, &net, prefix_len);

    return 0;
}

static int add_plugin_ipnet(sa_family_t ip_version, const char *value, struct ip_prefix_list **all_nets)
{
    char sz_net[100], sz_mask[100]; /* IPv6 in the future */

    if(sscanf(value, "%99s %99s", sz_net, sz_mask) != 2) {
        return 1;
    }
    return insert_plugin_ipnet(ip_version, sz_net, sz_mask, all_nets);
}

static int add_plugin_ipnet4(const char *value, void *data, set_plugin_parameter_addon addon __attribute__((unused)))
{
    return add_plugin_ipnet(AF_INET, value, data);
}

static int add_plugin_ipnet6(const char *value, void *data, set_plugin_parameter_addon addon __attribute__((unused)))
{
    return add_plugin_ipnet(AF_INET6, value, data);
}

static int add_plugin_ipaddr4(const char *value, void *data, set_plugin_parameter_addon addon __attribute__((unused)))
{
    return insert_plugin_ipnet(AF_INET, value, "255.255.255.255", data);
}

static int add_plugin_ipaddr6(const char *value, void *data, set_plugin_parameter_addon addon __attribute__((unused)))
{
    return insert_plugin_ipnet(AF_INET6, value, "128", data);
}

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
