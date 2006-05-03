BASIC MULTICAST FORWARDING PLUGIN FOR OLSRD
by Erik Tromp (erik_tromp@hotmail.com)

27-04-2006: Version 1.0.1 - First release.


1. Introduction
---------------

The Basic Multicast Flooding Plugin forwards IP-multicast and
IP-local-broacast traffic over an OLSRD network. It uses the
Multi-Point Relays (MPRs) as identified by the OLSR protocol
to optimize the flooding of multicast and local broadcast packets
to all the nodes in the network. To prevent broadcast storms, a
history of packets is kept; only packets that have not been seen
in the past 3-6 seconds are forwarded.

In the IP header there is room for only two IP-addresses:
* the destination IP address (in our case either a multicast
  IP-address 224.0.0.0...239.255.255.255, or a local broadcast
  address e.g. 192.168.1.255), and
* the source IP address (the originator).

For optimized flooding, however, we need more information. Let's
assume we are the BMF process on one node. We will need to know which
node forwarded the IP packet to us. Since OLSR keeps track of which
nodes select our node as MPR (see the olsr_lookup_mprs_set function),
we can determine if the node that forwarded the packet, has selected us as
MPR. If so, we must also forward the packet, replacing the 'forwarded-by'
IP-address to that of us.

Because we need more information than fits in a normal IP-header, the
original packets are encapsulated into a new IP packet. Encapsulated
packets are transported in UDP, port 50505. The source address of the
encapsulation packet is set to the address of the forwarder instead of
the originator. Of course, the payload of the encapsulation packet is
the original IP packet.

For local reception, each received encapsulated packets is unpacked
and passed into a tuntap interface which is specially created for
this purpose.

Here is in short how the flooding works (see also the
BmfEncapsulatedPacketReceived(...) function; details with respect to
the forwarding towards non-OLSR enabled nodes are omitted):
  
  On all OLSR-enabled interfaces, setup reception of packets
    on UDP port 50505.
  Upon reception of such a packet:
    If the received packet was sent by ourselves, drop it.
    If the packet was recently seen, drop it.
    Unpack the encapsulated packet and send a copy to myself via the
      TunTap device.
    If I am an MPR for the node that forwarded the packet to me,
      forward the packet to all OLSR-enabled interfaces *including*
      the one on which it was received.

As with all good things in life, it's so simple you could have
thought of it yourself.
    

2. How to build and install
---------------------------

Follow the instructions in the base directory README file under
section II. - BUILDING AND RUNNING OLSRD. To be sure to install
the BMF plugin, cd to the base directory and issue the follwing
command at the shell prompt:

  make install_all

Next, turn on the possibility to create a tuntap device (see also
/usr/src/linux/Documentation/networking/tuntap.txt)

  mkdir /dev/net # if it doesn't exist already
  mknod /dev/net/tun c 10 200
  
Set permissions, e.g.:
  chmod 0700 /dev/net/tun
     
Edit the file /etc/olsrd.conf to load the BMF plugin. For example:

  LoadPlugin "olsrd_bmf.so.1.0.1"
  {
    # No PlParam entries required for basic operation
  }


3. How to run
-------------

After building and installing OLSRD with the BMF plugin, run the
olsrd deamon by entering at the shell prompt:
  olsrd

Look at the output; it should list the BMF plugin, e.g.:

  ---------- Plugin loader ----------
  Library: olsrd_bmf.so.1.0.1
  OLSRD Basic Multicast Forwarding plugin 1.0.1 (Apr 29 2006 12:57:57)
    (C) Thales Communications Huizen, Netherlands
    Erik Tromp (erik_tromp@hotmail.com)
  Checking plugin interface version...  4 - OK
  Trying to fetch plugin init function... OK
  Trying to fetch param function... OK
  Sending parameters...
  "NonOlsrIf"/"eth2"... OK
  "Drop"/"00:0C:29:28:0E:CC"... OK
  Running plugin_init function...
  ---------- LIBRARY LOADED ----------


4. How to check if it works
---------------------------

To check that BMF is working, enter the folliwing command on the
command prompt:
  
  ping -I eth1 224.0.0.1
    
Replace eth1 with the name of any OLSR-enabled network interface.

All OLSR-BMF nodes in the MANET should respond. For example:

root@IsdbServer:~# ping -I eth1 224.0.0.1
PING 224.0.0.1 (224.0.0.1) from 192.168.151.50 eth1: 56(84) bytes of data.
64 bytes from 192.168.151.50: icmp_seq=1 ttl=64 time=0.511 ms
64 bytes from 192.168.151.53: icmp_seq=1 ttl=64 time=4.67 ms (DUP!)
64 bytes from 192.168.151.55: icmp_seq=1 ttl=63 time=10.7 ms (DUP!)
64 bytes from 192.168.151.50: icmp_seq=2 ttl=64 time=0.076 ms
64 bytes from 192.168.151.53: icmp_seq=2 ttl=64 time=1.23 ms (DUP!)
64 bytes from 192.168.151.55: icmp_seq=2 ttl=63 time=1.23 ms (DUP!)
64 bytes from 192.168.151.50: icmp_seq=3 ttl=64 time=0.059 ms
64 bytes from 192.168.151.53: icmp_seq=3 ttl=64 time=2.94 ms (DUP!)
64 bytes from 192.168.151.55: icmp_seq=3 ttl=63 time=5.62 ms (DUP!)
64 bytes from 192.168.151.50: icmp_seq=4 ttl=64 time=0.158 ms
64 bytes from 192.168.151.53: icmp_seq=4 ttl=64 time=1.14 ms (DUP!)
64 bytes from 192.168.151.55: icmp_seq=4 ttl=63 time=1.16 ms (DUP!)


5. Adding non-OLSR interfaces to the multicast flooding
-------------------------------------------------------

As a special feature, it is possible to have multicast and local-broadcast
IP packets forwarded also on non-OLSR interfaces.

If you have network interfaces on which OLSR is *not* running, but you *do*
want to forward multicast and local-broadcast IP packets, specify these
interfaces one by one as "NonOlsrIf" parameters in the BMF plugin section
of /etc/olsrd.conf. For example:

  LoadPlugin "olsrd_bmf.so.1.0.1"
  {
    # Non-OLSR interfaces to participate in the multicast flooding
    PlParam     "NonOlsrIf"  "eth2"
    PlParam     "NonOlsrIf"  "eth3"
  }

If an interface is listed both as NonOlsrIf for BMF, and in the
Interfaces { ... } section of olsrd.conf, it will be seen by BMF
as an OLSR-enabled interface. Duh....
  

6. Testing in a lab environment
-------------------------------

Setup IP-tables to drop packets from nodes which are not
direct (1-hop) neigbors. For example, to drop all packets from
a host with MAC address 00:0C:29:28:0E:CC, enter at the shell prompt:

  iptables -A INPUT -m mac --mac-source 00:0C:29:28:0E:CC -j DROP

Edit the file /etc/olsrd.conf, and specify the MAC addresses of the nodes
we do not want to see; even though packets from these nodes are dropped
by iptables, they are still received on network interfaces which are in
promiscuous mode. For example:

  LoadPlugin "olsrd_bmf.so.1.0.1"
  {
    # Drop all packets received from the following MAC sources
    PlParam     "Drop"       "00:0C:29:C6:E2:61" # RemoteClient1
    PlParam     "Drop"       "00:0C:29:61:34:B7" # SimpleClient1
    PlParam     "Drop"       "00:0C:29:28:0E:CC" # SimpleClient2
  }



7. Common problems, FAQ
-----------------------

Question:
When starting OLSRD with the BMF plugin, I can see the following
error messages:

OLSRD Basic Multicast Forwarding plugin: open() error: No such file or directory
OLSRD Basic Multicast Forwarding plugin: error creating local EtherTunTap
OLSRD Basic Multicast Forwarding plugin: could not initialize network interfaces!

Wat to do?

Answer:

Turn on the possibility to create a tuntap device; see section 2 of this
file.

