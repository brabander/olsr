

How to write your own metric plugin
===================================
(by Leon Aaron Kaplan <aaron@lo-res.org> and Henning Rogge <rogge@fgan.de>)

Overview
=========
The idea behind the lq_plugin_* is that you can write your own Link Quality (LQ) 
metric mostly independant of the main OLSR code.
What is a metric? A metric is a notion of "distance" between nodes in the mesh.
Mobile adhoc network (MANET) research identified the area of finding proper metrics
for a MANET as one of the most important routing decisions.
A metric basically determines the path that packets will take.
The Shortest Path First (SPF, also sometimes called Dijkstra Algorithm) will chose
the shortest sum of all possible paths. 
So what paths do we want to chose in wifi MANET networks?
In general we want to have a little collisions as possible since 802.11a/b/g is a 
broadcast medium by nature. When you compare the very old ethernet systems
(coax cable, http://en.wikipedia.org/wiki/10BASE2) you can observe that at a certain
percentage of cable utilization (as low as 40%), you would get many collisions (since the
coax cable is a broadcast medium just as WIFI!) and the total throughput would
collapse to nearly zero.

Another choice for metrics might be signal strenght.

Currently OLSR.org as of 2008/10 implements the following metric:
 - RFC 3626 conforming hop count (each link has the metric value "1")
 - ETX_default_ff (ETX from MIT roofnet, with FunkFeuer.at extensions)
 - ETX_default_float (ETX with floating point arithmetic)
 - ETX_default_fpm   (ETX with fixed point maths (integer). Much faster on small embedded devices! Courtesy of Sven-Ola Tuecke)
  

How to get started
==================

Start by reading src/lq_plugin.c and lq_plugin.h

struct lq_handler:

is a pseudo C++ structure ;-)
there is a pointer table to functions so that the plugin system knows which 
functions it should jump to.

struct lq_handler {
 ...
  size_t hello_lq_size;  // number of bytes we need per hello in the hello DB for this specific metric. This has nothing to do wht is being sent over the net. This is only so that you can use the structure itself to store your metric data
  size_t tc_lq_size;
};

Almost all functions accept a void pointer. This now points to the beginning of the custom memory area that you allocated with the tc_lq_size or hello_lq_size  above.


Now... if you want to write your own metric then you must define your own struct lq_handler and pass that variable to 

void register_lq_handler(struct lq_handler *handler, const char *name);


for example:
  struct lq_handler my_lq_metric;

  ... // init my_lq_metric fully (every function must be assigned)

  register_lq_handler(&my_lq_metric, "my_lq_metric");


for another example, take a look at lq_plugin_default_float.[ch]
There it is done like this:

/* Default lq plugin settings */
struct lq_handler lq_etx_float_handler = {
  &default_lq_initialize_float,
  
  &default_lq_calc_cost_float,
  &default_lq_calc_cost_float,
  
  &default_lq_is_relevant_costchange_float,
  
  &default_lq_packet_loss_worker_float,
  &default_lq_memorize_foreign_hello_float,
  &default_lq_copy_link2tc_float,
  &default_lq_clear_float,
  &default_lq_clear_float,

....
  sizeof(struct default_lq_float),
  sizeof(struct default_lq_float)
};

This means , we use the sizeof(struct default_lq_float) as extra memory space in the hello database and a sizeof(struct default_lq_fload) as extra space in the TC database.
The other entries here are pointers to functions which are defined in the .h file.




Now lets have a look at the functions.

  void (*initialize)(void);
-----------------------------
  This callback will be called when the olsrd.conf selects this routing 
  metric plugin.
  for example: 
  #LinkQualityAlgorithm    "etx_ff"
  
  The etx_ff initialize function for example starts some timers and it reserves a callback which is triggered for each packet.
  In general you can use any initialization code in this callback.

  olsr_linkcost(*calc_hello_cost) (const void *lq);
---------------------------------
  This callback will be called whenever a packet is processed.
  Called after:
  Called before:

  You will get a pointer to your part of the hello database and your plugin 
  is supposed to generate a hello cost in the standard format from the internal representation:
  the core format is a 32 bit unsigned integer . This then is used for 
  adding up the costs in the Dijkstra calculation.
  So be sure that you do not use values greater than 2^24 . Otherwise for 
  very long routes (256 hops) this will then add up in the internal representation so that there will be integer overflows. So stay below 2^24 (*).
  The base of the cost is your business. For example, to represent fixed point arithmetic you might multiply your (float) hello costs by 256 and treat the least significant byte as the digits behind the point.
  
  (*) or actually even better: us the constant LINK_COST_BROKEN from lq_plugin.h


  olsr_linkcost(*calc_tc_cost) (const void *lq);
---------------------------------
  This callback will be called whenever a packet is processed.
  Called after:
  Called before:

  same as for calc_hello_cost but just for TC costs and not for hellos


  bool(*is_relevant_costchange) (olsr_linkcost c1, olsr_linkcost c2);
---------------------------------
  This callback is called after the main loop whenever the core has 
  to decide if a new dijkstra needs to be calculated. In principle for every 
  packet.

  If a link quality does only change slightly, there will be no recalculation 
  of the Dijkstra algorithm which costs a lot of CPU. 
  Since only the plugin knows the range of the link quality values, only
  the plugin can decide if the cost change is relevant enough.

  return values:
    true  
    false 


  olsr_linkcost(*packet_loss_handler) (struct link_entry * entry, void *lq,
                       bool lost);
---------------------------------
  This callback is called 


  returns:

  void (*memorize_foreign_hello) (void *local, void *foreign);
---------------------------------
  This callback is called when


  void (*copy_link_lq_into_tc) (void *target, void *source);
---------------------------------
  This callback is called when Hello packet's LQ is copied over into a TC.
  Since this can have a different representation, this function can be used 
  to convert representations.

  void (*clear_hello) (void *target);
---------------------------------
  This callback is called when

  void (*clear_tc) (void *target);
---------------------------------
  This callback is called when

  int (*serialize_hello_lq) (unsigned char *buff, void *lq);
---------------------------------
  This callback is called when
  

  int (*serialize_tc_lq) (unsigned char *buff, void *lq);
---------------------------------
  This callback is called when

  void (*deserialize_hello_lq) (const uint8_t ** curr, void *lq);
---------------------------------
  This callback is called when

  void (*deserialize_tc_lq) (const uint8_t ** curr, void *lq);
---------------------------------
  This callback is called whenever a packet needs to be deserialized (for every packet 
  and there for every neighour in the TC)
  
  This function reads the lq information of a binary packet into a TC
  
  @param pointer to the current buffer pointer
  @param pointer to 



  const char *(*print_hello_lq) (void *ptr, char separator, struct lqtextbuffer * buffer);
---------------------------------
  This callback is called whenever any other part of the code wants to 
  convert your internal metric representation of Hellos into a printable
  version. So it can be called at any time

  ptr is a void pointer to the link quality data in the Hello DB.
  seperator is a character which is printed whenever there are multiple values (for example ',' or '/'
  buffer is the buffer you want to sprintf into. Please do not forget to \0 terminate!

  returns: the buffer
  In case of errors: please return "error" as string. Because print_hello_lq
  will often directly be put into a printf()


  const char *(*print_tc_lq) (void *ptr, char separator, struct lqtextbuffer * buffer);
---------------------------------
  same as print_hello_lq

  const char *(*print_cost) (olsr_linkcost cost, struct lqtextbuffer * buffer);
---------------------------------
  same as print_hello_lq. But for an arbitrary link cost.

