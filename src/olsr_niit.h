/*
 * olsr_niit.h
 *
 *  Created on: 02.02.2010
 *      Author: henning
 */

#ifndef OLSR_NIIT_H_
#define OLSR_NIIT_H_

#define DEF_NIIT4TO6_IFNAME         "niit4to6"
#define DEF_NIIT6TO4_IFNAME         "niit6to4"

int olsr_init_niit(void);
void olsr_setup_niit_routes(void);
void olsr_cleanup_niit_routes(void);

#endif /* OLSR_NIIT_H_ */
