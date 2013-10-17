/**
 * \file
 *         Source file for the Contiki radio neighbourhood management
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */


#include <limits.h>
#include <stdio.h>

#include "contiki.h"
#include "lib/memb.h"
#include "lib/list.h"

#include "libp-neighbour.h"
#include "libp.h"

#ifdef LIBP_NEIGHBOUR_CONF_MAX_LIBP_NEIGHBOURS
#define MAX_LIBP_NEIGHBOURS COLLECT_NEIGHBOR_CONF_MAX_LIBP_NEIGHBOURS
#else /* COLLECT_NEIGHBOR_CONF_MAX_LIBP_NEIGHBOURS */
#define MAX_LIBP_NEIGHBOURS 8
#endif /* COLLECT_NEIGHBOR_CONF_MAX_LIBP_NEIGHBOURS */

#define RTMETRIC_MAX LIBP_MAX_DEPTH

MEMB(libp_neighbours_mem, struct libp_neighbour, MAX_LIBP_NEIGHBOURS);

#define MAX_AGE                      180
#define MAX_LM_AGE                   10
#define PERIODIC_INTERVAL            CLOCK_SECOND * 60

#define EXPECTED_CONGESTION_DURATION CLOCK_SECOND * 240
#define CONGESTION_PENALTY           8 * LIBP_LINK_METRIC_UNIT

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
static void
periodic(void *ptr)
{
  struct libp_neighbour_list *neighbour_list;
  struct libp_neighbour *n;

  neighbour_list = ptr;

  /* Go through all libp_neighbours and increase their age. */
  for(n = list_head(neighbour_list->list); n != NULL; n = list_item_next(n)) {
    n->age++;
    n->lm_age++;
  }
  for(n = list_head(neighbour_list->list); n != NULL; n = list_item_next(n)) {
    if(n->lm_age == MAX_LM_AGE) {
      libp_link_metric_new(&n->lm);
      n->lm_age = 0;
    }
    if(n->age == MAX_AGE) {
      memb_free(&libp_neighbours_mem, n);
      list_remove(neighbour_list->list, n);
      n = list_head(neighbour_list->list);
    }
  }
  ctimer_set(&neighbour_list->periodic, PERIODIC_INTERVAL,
             periodic, neighbour_list);
}
/*---------------------------------------------------------------------------*/

void libp_neighbour_init(void)
{
    static uint8_t initialized = 0;
    if(initialized == 0) {
        initialized = 1;
        memb_init(&libp_neighbours_mem);
    }
}

list_t libp_neighbour_list(struct libp_neighbour_list *neighbour_list)
{
    if(neighbour_list == NULL) {
        return NULL;
    }

    return neighbour_list->list;
}

void libp_neighbour_list_new(struct libp_neighbour_list *neighbours_list)
{
LIST_STRUCT_INIT(neighbours_list, list);
  list_init(neighbours_list->list);
  ctimer_set(&neighbours_list->periodic, CLOCK_SECOND, periodic, neighbours_list);
}

int libp_neighbour_list_add(struct libp_neighbour_list *neighbours_list,const rimeaddr_t *addr, uint16_t nrtmetric)
{
   struct libp_neighbour *n;

  if(addr == NULL) {
    PRINTF("libp_neighbor_list_add: attempt to add NULL addr\n");
    return 0;
  }

  if(neighbours_list == NULL) {
    return 0;
  }

  PRINTF("libp_neighbor_add: adding %d.%d\n", addr->u8[0], addr->u8[1]);

  /* Check if the libp_neighbor is already on the list. */
  for(n = list_head(neighbours_list->list); n != NULL; n = list_item_next(n)) {
    if(rimeaddr_cmp(&n->addr, addr)) {
      PRINTF("libp_neighbor_add: already on list %d.%d\n",
             addr->u8[0], addr->u8[1]);
      break;
    }
  }

  /* If the libp_neighbor was not on the list, we try to allocate memory
     for it. */
  if(n == NULL) {
    PRINTF("libp_neighbor_add: not on list, allocating %d.%d\n",
           addr->u8[0], addr->u8[1]);
    n = memb_alloc(&libp_neighbours_mem);
    if(n != NULL) {
      list_add(neighbours_list->list, n);
    }
  }

  /* If we could not allocate memory, we try to recycle an old
     neighbor. XXX Should also look for the one with the worst
     rtmetric (not link esimate). XXX Also make sure that we don't
     replace a neighbor with a neighbor that has a worse metric. */
  if(n == NULL) {
    uint16_t worst_rtmetric;
    struct libp_neighbour *worst_neighbour;

    /* Find the neighbor that has the highest rtmetric. This is the
       neighbor that we are least likely to be using in the
       future. But we also need to make sure that the neighbor we are
       currently adding is not worst than the one we would be
       replacing. If so, we don't put the new neighbor on the list. */
    worst_rtmetric = 0;
    worst_neighbour = NULL;

    for(n = list_head(neighbours_list->list);
        n != NULL; n = list_item_next(n)) {
      if(n->rtmetric > worst_rtmetric) {
        worst_neighbour = n;
        worst_rtmetric = n->rtmetric;
      }
    }

    /* Only add this new neighbor if its rtmetric is lower than the
       one it would replace. */
    if(nrtmetric < worst_rtmetric) {
      n = worst_neighbour;
    }
    if(n != NULL) {
      PRINTF("libp_neighbor_add: not on list, not allocated, recycling %d.%d\n",
             n->addr.u8[0], n->addr.u8[1]);
    }
  }

  if(n != NULL) {
    n->age = 0;
    rimeaddr_copy(&n->addr, addr);
    n->rtmetric = nrtmetric;
    libp_link_metric_new(&n->lm);
    n->lm_age = 0;
    return 1;
  }
  return 0;
}

void libp_neighbour_list_remove(struct libp_neighbour_list *neighbours_list,const rimeaddr_t *addr)
{
 struct libp_neighbor *n;

  if(neighbours_list == NULL) {
    return;
  }

  n = libp_neighbour_list_find(neighbours_list, addr);

  if(n != NULL) {
    list_remove(neighbours_list->list, n);
    memb_free(&libp_neighbours_mem, n);
  }
}

struct libp_neighbour *libp_neighbour_list_find(struct libp_neighbour_list *neighbours_list, const rimeaddr_t *addr)
{
    struct libp_neighbour *n;
  if(neighbours_list == NULL) {
    return NULL;
  }
  for(n = list_head(neighbours_list->list); n != NULL; n = list_item_next(n)) {
    if(rimeaddr_cmp(&n->addr, addr)) {
      return n;
    }
  }
  return NULL;
}

uint16_t libp_neighbour_rtmetric_link_metric(struct libp_neighbour *n)
{
    if(n == NULL) {
        return 0;
    }
    return n->rtmetric + libp_link_metric(&n->lm);
}

struct libp_neighbour *libp_neighbour_list_best(struct libp_neighbour_list *neighbours_list)
{
    int found;
  struct libp_neighbour *n, *best;
  uint16_t rtmetric;

  rtmetric = RTMETRIC_MAX;
  best = NULL;
  found = 0;

  if(neighbours_list == NULL) {
    return NULL;
  }

  /*  PRINTF("%d: ", node_id);*/
  PRINTF("libp_neighbor_best: ");

  /* Find the neighbor with the lowest rtmetric + linkt estimate. */
  for(n = list_head(neighbours_list->list); n != NULL; n = list_item_next(n)) {
    PRINTF("%d.%d %d+%d=%d, ",
           n->addr.u8[0], n->addr.u8[1],
           n->rtmetric, libp_neighbour_link_metric(n),
           libp_neighbour_rtmetric(n));
    if(libp_neighbour_rtmetric_link_metric(n) < rtmetric) {
      rtmetric = libp_neighbour_rtmetric_link_metric(n);
      best = n;
    }
  }
  PRINTF("\n");

  return best;
}

int libp_neighbour_list_num(struct libp_neighbour_list *neighbours_list)
{
    if(neighbours_list == NULL) {
    return 0;
  }

  PRINTF("libp_neighbor_num %d\n", list_length(neighburs_list->list));
  return list_length(neighbours_list->list);
}

struct libp_neighbour *libp_neighbour_list_get(struct libp_neighbour_list *neighbours_list, int num)
{
    int i;
  struct libp_neighbour *n;

  if(neighbours_list == NULL) {
    return NULL;
  }

  PRINTF("libp_neighbor_get %d\n", num);

  i = 0;
  for(n = list_head(neighbours_list->list); n != NULL; n = list_item_next(n)) {
    if(i == num) {
      PRINTF("libp_neighbor_get found %d.%d\n",
             n->addr.u8[0], n->addr.u8[1]);
      return n;
    }
    i++;
  }
  return NULL;
}

void libp_neighbour_list_purge(struct libp_neighbour_list *neighbour_list)
{
    if(neighbour_list == NULL) {
        return;
    }

    while(list_head(neighbour_list->list) != NULL) {
        memb_free(&libp_neighbours_mem, list_pop(neighbour_list->list));
    }
}

void libp_neighbour_update_rtmetric(struct libp_neighbour *n, uint16_t rtmetric)
{
    if(n != NULL) {
    PRINTF("%d.%d: libp_neighbour_update %d.%d rtmetric %d\n",
           rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
           n->addr.u8[0], n->addr.u8[1], rtmetric);
    n->rtmetric = rtmetric;
    n->age = 0;
  }
}

void libp_neighbour_tx(struct libp_neighbour *n, uint16_t num_tx)
{
  if(n == NULL) {
    return;
  }
  libp_link_metric_update_tx(&n->lm, num_tx);
  n->lm_age = 0;
  n->age = 0;
}
void libp_neighbour_rx(struct libp_neighbour *n)
{
 if(n == NULL) {
    return;
  }
  libp_link_metric_update_rx(&n->lm);
  n->age = 0;
}
void libp_neighbour_tx_fail(struct libp_neighbour *n, uint16_t num_tx)
{
if(n == NULL) {
    return;
  }
  libp_link_metric_update_tx_fail(&n->lm, num_tx);
  n->lm_age = 0;
  n->age = 0;
}
void libp_neighbour_set_congested(struct libp_neighbour *n)
{
    if(n == NULL) {
    return;
  }
  timer_set(&n->congested_timer, EXPECTED_CONGESTION_DURATION);
}
int libp_neighbour_is_congested(struct libp_neighbour *n)
{
    if(n == NULL) {
    return 0;
  }

  if(timer_expired(&n->congested_timer)) {
    return 0;
  } else {
    return 1;
  }
}
uint16_t libp_neighbour_link_metric(struct libp_neighbour *n)
{
    if(n == NULL) {
    return 0;
  }
  if(libp_neighbour_is_congested(n)) {
    /*    printf("Congested %d.%d, sould return %d, returning %d\n",
           n->addr.u8[0], n->addr.u8[1],
           collect_link_estimate(&n->le),
           collect_link_estimate(&n->le) + CONGESTION_PENALTY);*/
    return libp_link_metric(&n->lm) + CONGESTION_PENALTY;
  } else {
    return libp_link_metric(&n->lm);
  }
}



uint16_t libp_neighbour_rtmetric(struct libp_neighbour *n)
{
    if(n == NULL) {
    return 0;
  }

  return n->rtmetric;
}
