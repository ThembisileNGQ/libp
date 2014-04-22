/**
 * \file
 *         Header file for the Contiki radio neighborhood management
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */

#ifndef __LIBP_NEIGHBOR_H__
#define __LIBP_NEIGHBOR_H__

#include "net/rime/rimeaddr.h"
#include "libp-link-metric.h"
#include "lib/list.h"

struct libp_neighbour_list {
  LIST_STRUCT(list);
  struct ctimer periodic;
};

struct libp_neighbour {
  struct libp_neighbour *next;
  rimeaddr_t addr;
  uint16_t rtmetric;
  uint16_t age;
  uint16_t lm_age;
  uint16_t penalty;
  struct libp_link_metric lm;
  struct timer congested_timer;
};

void libp_neighbour_init(void);

list_t libp_neighbour_list(struct libp_neighbour_list *neighbor_list);

void libp_neighbour_list_new(struct libp_neighbour_list *neighbor_list);

int libp_neighbour_list_add(struct libp_neighbour_list *neighbor_list,
                              const rimeaddr_t *addr, uint16_t rtmetric);
void libp_neighbour_list_remove(struct libp_neighbour_list *neighbor_list,
                                  const rimeaddr_t *addr);
struct libp_neighbour *libp_neighbour_list_find(struct libp_neighbour_list *neighbor_list,
                                               const rimeaddr_t *addr);
struct libp_neighbour *libp_neighbour_list_best(struct libp_neighbour_list *neighbor_list);
int libp_neighbour_list_num(struct libp_neighbour_list *neighbor_list);
struct libp_neighbour *libp_neighbour_list_get(struct libp_neighbour_list *neighbor_list, int num);
void libp_neighbour_list_purge(struct libp_neighbour_list *neighbor_list);

void libp_neighbour_update_rtmetric(struct libp_neighbour *n,

                                      uint16_t rtmetric);
void libp_neighbour_tx(struct libp_neighbour *n, uint16_t num_tx);
void libp_neighbour_rx(struct libp_neighbour *n);
void libp_neighbour_tx_fail(struct libp_neighbour *n, uint16_t num_tx);
void libp_neighbour_set_congested(struct libp_neighbour *n);
int libp_neighbour_is_congested(struct libp_neighbour *n);


uint16_t libp_neighbour_link_metric(struct libp_neighbour *n);
uint16_t libp_neighbour_rtmetric_link_metric(struct libp_neighbour *n);
uint16_t libp_neighbour_rtmetric(struct libp_neighbour *n);

#endif
