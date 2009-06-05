
/*
OLSR MDNS plugin.
Written by Saverio Proto <zioproto@gmail.com> and Claudio Pisa <clauz@ninux.org>.

    This file is part of OLSR MDNS PLUGIN.

    The OLSR MDNS PLUGIN is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The OLSR MDNS PLUGIN is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.


 */

/* System includes */
#include <assert.h>             /* assert() */
#include <stddef.h>             /* NULL */

/* OLSRD includes */
#include "plugin.h"
#include "plugin_util.h"
#include "defs.h"               /* uint8_t, olsr_cnf */
#include "scheduler.h"          /* olsr_start_timer() */
#include "olsr_cfg.h"           /* olsr_cnf() */
#include "olsr_cookie.h"        /* olsr_alloc_cookie() */

/* BMF includes */
#include "mdns.h"               /* InitBmf(), CloseBmf() */
#include "NetworkInterfaces.h"  /* AddNonOlsrBmfIf(), SetBmfInterfaceIp(), ... */
#include "Address.h"            /* DoLocalBroadcast() */

static void __attribute__ ((constructor)) my_init(void);
static void __attribute__ ((destructor)) my_fini(void);

//static struct olsr_cookie_info *prune_packet_history_timer_cookie;

void olsr_plugin_exit(void);

/* -------------------------------------------------------------------------
 * Function   : olsrd_plugin_interface_version
 * Description: Plugin interface version
 * Input      : none
 * Output     : none
 * Return     : BMF plugin interface version number
 * Data Used  : none
 * Notes      : Called by main OLSRD (olsr_load_dl) to check plugin interface
 *              version
 * ------------------------------------------------------------------------- */
int
olsrd_plugin_interface_version(void)
{
  return PLUGIN_INTERFACE_VERSION;
}

/* -------------------------------------------------------------------------
 * Function   : olsrd_plugin_init
 * Description: Plugin initialisation
 * Input      : none
 * Output     : none
 * Return     : fail (0) or success (1)
 * Data Used  : olsr_cnf
 * Notes      : Called by main OLSRD (init_olsr_plugin) to initialize plugin
 * ------------------------------------------------------------------------- */
int
olsrd_plugin_init(void)
{
  /* Clear the packet history */
  //InitPacketHistory();

  /* Register ifchange function */
  //add_ifchgf(&InterfaceChange);

  /* create the cookie */
  //prune_packet_history_timer_cookie = olsr_alloc_cookie("BMF: Prune Packet History", OLSR_COOKIE_TYPE_TIMER);

  /* Register the duplicate registration pruning process */
  //olsr_start_timer(3 * MSEC_PER_SEC, 0, OLSR_TIMER_PERIODIC,
  //                 &PrunePacketHistory, NULL, prune_packet_history_timer_cookie->ci_id);


  return InitMDNS(NULL);
}

/* -------------------------------------------------------------------------
 * Function   : olsr_plugin_exit
 * Description: Plugin cleanup
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  : none
 * Notes      : Called by my_fini() at unload of shared object
 * ------------------------------------------------------------------------- */
void
olsr_plugin_exit(void)
{
  CloseMDNS();
}

static const struct olsrd_plugin_parameters plugin_parameters[] = {
  {.name = "NonOlsrIf",.set_plugin_parameter = &AddNonOlsrBmfIf,.data = NULL},
  {.name = "MDNS_TTL", .set_plugin_parameter = &set_MDNS_TTL, .data = NULL },
  //{ .name = "DoLocalBroadcast", .set_plugin_parameter = &DoLocalBroadcast, .data = NULL },
  //{ .name = "BmfInterface", .set_plugin_parameter = &SetBmfInterfaceName, .data = NULL },
  //{ .name = "BmfInterfaceIp", .set_plugin_parameter = &SetBmfInterfaceIp, .data = NULL },
  //{ .name = "CapturePacketsOnOlsrInterfaces", .set_plugin_parameter = &SetCapturePacketsOnOlsrInterfaces, .data = NULL },
  //{ .name = "BmfMechanism", .set_plugin_parameter = &SetBmfMechanism, .data = NULL },
  //{ .name = "FanOutLimit", .set_plugin_parameter = &SetFanOutLimit, .data = NULL },
  //{ .name = "BroadcastRetransmitCount", .set_plugin_parameter = &set_plugin_int, .data = &BroadcastRetransmitCount},
};

/* -------------------------------------------------------------------------
 * Function   : olsrd_get_plugin_parameters
 * Description: Return the parameter table and its size
 * Input      : none
 * Output     : params - the parameter table
 *              size - its size in no. of entries
 * Return     : none
 * Data Used  : plugin_parameters
 * Notes      : Called by main OLSR (init_olsr_plugin) for all plugins
 * ------------------------------------------------------------------------- */
void
olsrd_get_plugin_parameters(const struct olsrd_plugin_parameters **params, int *size)
{
  *params = plugin_parameters;
  *size = ARRAYSIZE(plugin_parameters);
}

/* -------------------------------------------------------------------------
 * Function   : my_init
 * Description: Plugin constructor
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  : none
 * Notes      : Called at load of shared object
 * ------------------------------------------------------------------------- */
static void
my_init(void)
{
  /* Print plugin info to stdout */
  printf("%s\n", MOD_DESC);

  return;
}

/* -------------------------------------------------------------------------
 * Function   : my_fini
 * Description: Plugin destructor
 * Input      : none
 * Output     : none
 * Return     : none
 * Data Used  : none
 * Notes      : Called at unload of shared object
 * ------------------------------------------------------------------------- */
static void
my_fini(void)
{
  olsr_plugin_exit();
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
