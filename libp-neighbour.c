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

#ifdef COLLECT_NEIGHBOR_CONF_MAX_COLLECT_NEIGHBORS
#define MAX_COLLECT_NEIGHBORS COLLECT_NEIGHBOR_CONF_MAX_COLLECT_NEIGHBORS
#else /* COLLECT_NEIGHBOR_CONF_MAX_COLLECT_NEIGHBORS */
#define MAX_COLLECT_NEIGHBORS 8
#endif /* COLLECT_NEIGHBOR_CONF_MAX_COLLECT_NEIGHBORS */

#define RTMETRIC_MAX COLLECT_MAX_DEPTH

MEMB(libp_neighbours_mem, struct libp_neighbour, MAX_COLLECT_NEIGHBORS);

#define MAX_AGE                      180
#define MAX_LE_AGE                   10
#define PERIODIC_INTERVAL            CLOCK_SECOND * 60

#define EXPECTED_CONGESTION_DURATION CLOCK_SECOND * 240
#define CONGESTION_PENALTY           8 * COLLECT_LINK_ESTIMATE_UNIT

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


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

void libp_neighbour_list_new(struct libp_neighbour_list *neighbour_list)
{

}

int libp_neighbour_list_add(struct libp_neighbour_list *neighbor_list,const rimeaddr_t *addr, uint16_t rtmetric)
{
   return 0;
}

void libp_neighbour_list_remove(struct libp_neighbour_list *neighbor_list,const rimeaddr_t *addr)
{

}

struct libp_neighbour *libp_neighbour_list_find(struct libp_neighbour_list *neighbour_list, const rimeaddr_t *addr)
{

}

struct libp_neighbour *libp_neighbour_list_best(struct libp_neighbour_list *neighbour_list)
{
    return NULL;
}

int libp_neighbour_list_num(struct libp_neighbour_list *neighbour_list)
{
    return 0;
}

struct libp_neighbour *libp_neighbour_list_get(struct libp_neighbour_list *neighbour_list, int num)
{
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
    return 0;
}


uint16_t libp_neighbour_link_estimate(struct libp_neighbour *n)
{
    return 0;
}

uint16_t libp_neighbour_rtmetric_link_estimate(struct libp_neighbour *n)
{
    return 0;
}

uint16_t libp_neighbour_rtmetric(struct libp_neighbour *n)
{
    return 0;
}
