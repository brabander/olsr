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


/* general notes:
 * 
 * memory allocation: we are completely independent of the memory manager!
 * Therefore you have to allocate all memory yourself and in general pass pointers 
 * to your memory region for each function 
*/

struct olsr_callback_provider {
  struct avl_node node;
  char *name;			/* key for avl node . The consumers need this name as key */

  struct list_entity callbacks;
  uint32_t obj_count;	/* bookkeeping */
  bool in_use;			/* protection against recursive callbacks. Set to true if we are in a callback */

  /* convert pointer to object into char * (name of the key). 
   * This helps for debugging. You can now print identifiers 
   * of the object pointed to */
  const char *(*getKey)(void *);
};

struct olsr_callback_consumer {
  struct list_entity node;	
  struct olsr_callback_provider *provider;		/* backptr to the provider */
  char *name;			/* name of the consumer */

  void (*add)(void *);		/* callback ptr when something gets added . Parameter is pointer to the object */
  void (*change)(void *);	/* same, but for change */
  void (*remove)(void *);	/* same, but for remove */
};


void olsr_callback_init(void);
void olsr_callback_cleanup(void);

int EXPORT(olsr_callback_prv_create)(struct olsr_callback_provider *, const char *);
void EXPORT(olsr_callback_prv_destroy)(struct olsr_callback_provider *);

int EXPORT(olsr_callback_cons_register)(
    const char *, const char *, struct olsr_callback_consumer *);
void EXPORT(olsr_callback_cons_unregister)(struct olsr_callback_consumer *);

void EXPORT(olsr_callback_add_object)(struct olsr_callback_provider *, void *);
void EXPORT(olsr_callback_change_object)(struct olsr_callback_provider *, void *);
void EXPORT(olsr_callback_remove_object)(struct olsr_callback_provider *, void *);

#define OLSR_FOR_ALL_CALLBACK_PROVIDERS(provider, iterator) avl_for_each_element_safe(&callback_provider_tree, provider, node, iterator)
#define OLSR_FOR_ALL_CALLBACK_CONSUMERS(provider, consumer, iterator) list_for_each_element_safe(&provider->callbacks, consumer, node, iterator)

extern struct avl_tree EXPORT(callback_provider_tree);
#endif /* OLSR_CALLBACKS_H_ */
