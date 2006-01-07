#include "defs.h"

FILE *debug_handle;

struct olsrd_config *olsr_cnf;

/* Timer data */
clock_t now_times;              /* current idea of times(2) reported uptime */
struct timeval now;		/* current idea of time */
struct tm *nowtm;		/* current idea of time (in tm) */

