#include <stdio.h>
#include <stdlib.h>
#include "olsr.h"
#include "olsr_cfg.h"
#include "olsr_cfg_gen.h"

bool disp_pack_out = false;
FILE *debug_handle = NULL;
struct olsr_config *olsr_cnf = NULL;

void *olsr_malloc(size_t size, const char *id __attribute__ ((unused)))
{
  return calloc(1, size);
}

char *olsr_strdup(const char *s)
{
  char *ret = olsr_malloc(1 + strlen(s), "olsr_strdup");
  strcpy(ret, s);
  return ret;
}

char *olsr_strndup(const char *s, size_t n)
{
  size_t len = n < strlen(s) ? n : strlen(s);
  char *ret = olsr_malloc(1 + len, "olsr_strndup");
  strncpy(ret, s, len);
  ret[len] = 0;
  return ret;
}

void parser_set_disp_pack_in(bool val __attribute__ ((unused)))
{
}

int main(int argc, char *argv[])
{
  int i, ret = 0;
  for (i = 1; i < argc; i++) {
    struct olsr_config *cfg_tmp;
    char cfg_msg[FILENAME_MAX + 256];
    
    printf("Verifying argv[%d]=%s\n", i, argv[i]);
    if (CFG_ERROR != olsr_parse_cfg(0, NULL, argv[i], cfg_msg, &cfg_tmp)) {
      printf("%s verified: %s\n", argv[i], olsr_sanity_check_cfg(cfg_tmp) ? "yes" : "no");
      printf("DebugLevel=%d\n", cfg_tmp->debug_level);
    }
    else {
      fprintf(stderr, "%s not verified. %s\n", argv[i], cfg_msg);
      ret = 1;
    }
    olsr_free_cfg(cfg_tmp);
  }
  return ret;
}
