/*
 * olsr_callback.c
 *
 *  Created on: Feb 8, 2011
 *      Author: henning
 */

#include <stdlib.h>
#include <string.h>

#include "common/list.h"
#include "common/avl.h"
#include "common/avl_olsr_comp.h"
#include "olsr_logging.h"
#include "olsr_callbacks.h"

static const char *unknown_key(void *);

struct avl_tree callback_provider_tree;

/**
 * Initialize the internal data of the callback provider system
 */
void
olsr_callback_init(void) {
  avl_init(&callback_provider_tree, avl_comp_strcasecmp, false, NULL);
}

/**
 * Cleanup the internal data of the callback provider system
 */
void
olsr_callback_cleanup(void) {
  struct olsr_callback_provider *prv, *iterator;

  OLSR_FOR_ALL_CALLBACK_PROVIDERS(prv, iterator) {
    olsr_callback_prv_destroy(prv);
  }
}

/**
 * Create a new callback provider
 * @param prv pointer to uninitialized callback provider datastructure
 * @param name pointer to name of provider
 * @return 0 if provider was registered sucessfully, 1 otherwise
 */
int
olsr_callback_prv_create(struct olsr_callback_provider *prv, const char *name) {
  /* Clear provider entry first */
  memset(prv, 0, sizeof(*prv));

  /* check if provider already exists */
  if (avl_find(&callback_provider_tree, name) != NULL) {
    OLSR_WARN(LOG_CALLBACK, "Provider '%s' already exists. Not creating.\n", name);
    return 1;
  }

  OLSR_DEBUG(LOG_CALLBACK, "Create callback provider '%s'\n", name);

  prv->name = strdup(name);
  prv->node.key = prv->name;
  avl_insert(&callback_provider_tree, &prv->node);

  prv->getKey = unknown_key;
  list_init_head(&prv->callbacks);
  return 0;
}

/**
 * Cleans up an existing registered callback provider
 * @param pointer to initialized callback provider
 */
void
olsr_callback_prv_destroy(struct olsr_callback_provider *prv) {
  struct olsr_callback_consumer *cons, *iterator;

  OLSR_DEBUG(LOG_CALLBACK, "Destroying callback provider '%s' (object count %u)\n",
      prv->name, prv->obj_count);

  OLSR_FOR_ALL_CALLBACK_CONSUMERS(prv, cons, iterator) {
    olsr_callback_cons_unregister(cons);
  }

  avl_delete(&callback_provider_tree, &prv->node);
  free(prv->name);
  prv->name = NULL;
}

/**
 * Registers a new callback consumer to an existing provider
 * @param prv_name name of callback provider
 * @param cons_name name of new callback consumer
 * @param cons pointer to uninitialized callback consumer
 * @return 0 if sucessfully registered, 1 otherwise
 */
int
olsr_callback_cons_register(const char *prv_name, const char *cons_name,
    struct olsr_callback_consumer *cons) {
  struct olsr_callback_provider *prv;

  prv = avl_find_element(&callback_provider_tree, prv_name, prv, node);
  if (prv == NULL) {
    OLSR_WARN(LOG_CALLBACK, "Could not find callback provider '%s'\n", prv_name);
    return 1;
  }

  OLSR_DEBUG(LOG_CALLBACK, "Register callback '%s' with provider '%s'\n",
      cons_name, prv_name);
  memset(cons, 0, sizeof(*cons));
  cons->provider = prv;
  cons->name = strdup(cons_name);
  list_add_tail(&prv->callbacks, &cons->node);
  return 0;
}

/**
 * Unregistered an initialized callback consumer
 * @param cons pointer to callback consumer
 */
void
olsr_callback_cons_unregister(struct olsr_callback_consumer *cons) {
  if (cons->node.next != NULL && cons->node.prev) {
    OLSR_DEBUG(LOG_CALLBACK, "Unregister callback '%s' with provider '%s'\n",
        cons->name, cons->provider->name);

    list_remove(&cons->node);
    free (cons->name);
    cons->name = NULL;
    cons->provider = NULL;
  }
}

/**
 * Fire a callback for an added object
 * @param pointer to callback provider
 * @param pointer to object
 */
void
olsr_callback_add_object(struct olsr_callback_provider *prv, void *obj) {
  struct olsr_callback_consumer *cons, *iterator;

  if (prv->in_use) {
    OLSR_WARN(LOG_CALLBACK, "Warning, recursive use of callback %s. Skipping.\n",
        prv->name);
    return;
  }

  prv->in_use = true;
  prv->obj_count++;
  OLSR_DEBUG(LOG_CALLBACK, "Adding object %s (%u) to callback '%s'\n",
      prv->getKey(obj), prv->obj_count, prv->name);

  OLSR_FOR_ALL_CALLBACK_CONSUMERS(prv, cons, iterator) {
    if (cons->add) {
      OLSR_DEBUG(LOG_CALLBACK, "Calling '%s' add callback\n", cons->name);
      cons->add(obj);
    }
  }
  prv->in_use = false;
}

/**
 * Fire a callback for a changed object
 * @param pointer to callback provider
 * @param pointer to object
 */
void
olsr_callback_change_object(struct olsr_callback_provider *prv, void *obj) {
  struct olsr_callback_consumer *cons, *iterator;

  if (prv->in_use) {
    OLSR_WARN(LOG_CALLBACK, "Warning, recursive use of callback %s\n",
        prv->name);
    return;
  }

  prv->in_use = true;
  OLSR_DEBUG(LOG_CALLBACK, "Changing object %s (%u) of callback '%s'\n",
      prv->getKey(obj), prv->obj_count, prv->name);

  OLSR_FOR_ALL_CALLBACK_CONSUMERS(prv, cons, iterator) {
    if (cons->change) {
      OLSR_DEBUG(LOG_CALLBACK, "Calling '%s' change callback\n", cons->name);
      cons->change(obj);
    }
  }
  prv->in_use = false;
}

/**
 * Fire a callback for a removed object
 * @param pointer to callback provider
 * @param pointer to object
 */
void
olsr_callback_remove_object(struct olsr_callback_provider *prv, void *obj) {
  struct olsr_callback_consumer *cons, *iterator;

  if (prv->in_use) {
    OLSR_WARN(LOG_CALLBACK, "Warning, recursive use of callback %s\n",
        prv->name);
    return;
  }

  prv->in_use = true;
  OLSR_DEBUG(LOG_CALLBACK, "Removing object %s (%u) from callback '%s'\n",
      prv->getKey(obj), prv->obj_count, prv->name);

  OLSR_FOR_ALL_CALLBACK_CONSUMERS(prv, cons, iterator) {
    if (cons->remove) {
      OLSR_DEBUG(LOG_CALLBACK, "Calling '%s' remove callback\n", cons->name);
      cons->remove(obj);
    }
  }
  prv->obj_count--;
  prv->in_use = false;
}

/**
 * Helper function to display a name of an object.
 * @param pointer to object
 * @return string representation of objects hexadecimal address
 */
static const char *
unknown_key(void *obj) {
  static char buffer[32];

  snprintf(buffer, sizeof(buffer), "0x%zx", (size_t)obj);
  return buffer;
}
