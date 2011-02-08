/*
 * olsr_callbacks.h
 *
 *  Created on: Feb 8, 2011
 *      Author: henning
 */

#ifndef OLSR_CALLBACKS_H_
#define OLSR_CALLBACKS_H_

#include "common/list.h"
#include "common/avl.h"
#include "defs.h"

struct olsr_callback_provider {
  struct avl_node node;
  char *name;

  struct list_entity callbacks;
  uint32_t obj_count;

  const char *(*getKey)(void *);
};

struct olsr_callback_consumer {
  struct list_entity node;
  struct olsr_callback_provider *provider;
  char *name;

  void (*add)(void *);
  void (*change)(void *);
  void (*remove)(void *);
};

void olsr_callback_init(void);
void olsr_callback_cleanup(void);

int EXPORT(olsr_callback_prv_create)(struct olsr_callback_provider *, const char *);
void EXPORT(olsr_callback_prv_destroy)(struct olsr_callback_provider *);

struct olsr_callback_provider *EXPORT(olsr_callback_cons_register)(
    const char *, const char *, struct olsr_callback_consumer *);
void EXPORT(olsr_callback_cons_unregister)(struct olsr_callback_consumer *);

void EXPORT(olsr_callback_add_object)(struct olsr_callback_provider *, void *);
void EXPORT(olsr_callback_change_object)(struct olsr_callback_provider *, void *);
void EXPORT(olsr_callback_remove_object)(struct olsr_callback_provider *, void *);

#define OLSR_FOR_ALL_CALLBACK_PROVIDERS(provider, iterator) avl_for_each_element_safe(&callback_provider_tree, provider, node, iterator)
#define OLSR_FOR_ALL_CALLBACK_CONSUMERS(provider, consumer, iterator) list_for_each_element_safe(&provider->callbacks, consumer, node, iterator)

extern struct avl_tree EXPORT(callback_provider_tree);
#endif /* OLSR_CALLBACKS_H_ */
