

#ifndef _OLSRD_CONF_H
#define _OLSRD_CONF_H

#include "olsr_protocol.h"
#include "olsrd_cfgparser.h"

#define SOFTWARE_VERSION "0.1.1"


int current_line;

struct olsrd_config *cnf;

struct conf_token
{
  olsr_u32_t integer;
  float      floating;
  olsr_u8_t  boolean;
  char       *string;
};

void
set_default_cnf(struct olsrd_config *);

struct if_config_options *
find_if_rule_by_name(struct if_config_options *, char *);

struct conf_token *
get_conf_token();

struct if_config_options *
get_default_if_config();

#endif
