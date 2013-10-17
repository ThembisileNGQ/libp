/**
 * \file
 *         Header file for hop-by-hop reliable data collection using the LIBP primitive
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */

#ifndef __LIBP_H__
#define __LIBP_H__

#include "net/rime/announcement.h"
#include "net/rime/runicast.h"
#include "net/rime/neighbor-discovery.h"
#include "libp-neighbour.h"
#include "net/packetqueue.h"
#include "sys/ctimer.h"
#include "lib/list.h"

struct libp_callbacks {
  void (* recv)(const rimeaddr_t *originator, uint8_t seqno,
		uint8_t hops);
};

struct libp_conn {
  struct unicast_conn unicast_conn;
  struct broadcast_conn broadcast_conn;
  struct announcement announcement;
  struct ctimer transmit_after_scan_timer;
  const struct libp_callbacks *cb;
  struct ctimer retransmission_timer;
  LIST_STRUCT(send_queue_list);
  struct packetqueue send_queue;
  struct libp_neighbour_list neighbour_list;

  struct ctimer beacon_timer;
  clock_time_t beacon_period;


  struct ctimer proactive_probing_timer;

  rimeaddr_t parent, current_parent;
  uint16_t rtmetric;
  uint8_t seqno;
  uint8_t sending, transmissions, max_rexmits;
  uint8_t eseqno;
  uint8_t is_router;
  uint8_t is_sink;

  clock_time_t send_time;
};

enum {
  LIBP_NO_ROUTER,
  LIBP_ROUTER,
};

void libp_open(struct libp_conn *c, uint16_t channels,
                  uint8_t is_router,
                  const struct libp_callbacks *callbacks);
void libp_close(struct libp_conn *c);

int libp_send(struct libp_conn *c, int rexmits);

void libp_set_sink(struct libp_conn *c, int should_be_sink);

void libp_set_beacon_period(struct libp_conn *c, clock_time_t period);

/*void libp_print_stats(void);*/

#define LIBP_MAX_DEPTH (LIBP_LINK_METRIC_UNIT * 64 - 1)

#endif
