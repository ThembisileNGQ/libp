/**
 * \file
 *         Source file for hop-by-hop reliable data collection using the LIBP primitive
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/rime.h"
#include "libp.h"
#include "libp-neighbour.h"
#include "libp-link-metric.h"

#include "net/packetqueue.h"

#include "dev/radio-sensor.h"

#include "lib/random.h"

#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define COLLECT_ATTRIBUTES  { PACKETBUF_ADDR_ESENDER,     PACKETBUF_ADDRSIZE }, \
                            { PACKETBUF_ATTR_EPACKET_ID,  PACKETBUF_ATTR_BIT * COLLECT_PACKET_ID_BITS }, \
                            { PACKETBUF_ATTR_PACKET_ID,   PACKETBUF_ATTR_BIT * COLLECT_PACKET_ID_BITS }, \
                            { PACKETBUF_ATTR_TTL,         PACKETBUF_ATTR_BIT * 4 }, \
                            { PACKETBUF_ATTR_HOPS,        PACKETBUF_ATTR_BIT * 4 }, \
                            { PACKETBUF_ATTR_MAX_REXMIT,  PACKETBUF_ATTR_BIT * 5 }, \
                            { PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_BIT }, \
                            UNICAST_ATTRIBUTES

#define RTMETRIC_SINK 0
#define RTMETRIC_MAX 511

#define COLLECT_ANNOUNCEMENTS 1 //CONF_WITH_LISTEN is OFF ie 0
/* These are configuration knobs that normally should not be
   tweaked. MAX_MAC_REXMITS defines how many times the underlying CSMA
   MAC layer should attempt to resend a data packet before giving
   up. The MAX_ACK_MAC_REXMITS defines how many times the MAC layer
   should resend ACK packets. The REXMIT_TIME is the lowest
   retransmission timeout at the network layer. It is exponentially
   increased for every new network layer retransmission. The
   FORWARD_PACKET_LIFETIME is the maximum time a packet is held in the
   forwarding queue before it is removed. The MAX_SENDING_QUEUE
   specifies the maximum length of the output queue. If the queue is
   full, incoming packets are dropped instead of being forwarded. */

#define MAX_MAC_REXMITS            2
#define MAX_ACK_MAC_REXMITS        5
#define REXMIT_TIME                (CLOCK_SECOND * 32 / NETSTACK_RDC_CHANNEL_CHECK_RATE)
#define FORWARD_PACKET_LIFETIME_BASE    REXMIT_TIME * 2
#define MAX_SENDING_QUEUE          3 * QUEUEBUF_NUM / 4
#define MIN_AVAILABLE_QUEUE_ENTRIES 4
#define KEEPALIVE_REXMITS          8
#define MAX_REXMITS                31

#define PROACTIVE_PROBING_INTERVAL (random_rand() % CLOCK_SECOND * 60)
#define PROACTIVE_PROBING_REXMITS  15

/* Debug definition: draw routing tree in Cooja. */
#define DRAW_TREE 0
#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/* Forward declarations. */
static void send_queued_packet(struct libp_conn *c);
static void retransmit_callback(void *ptr);
static void retransmit_not_sent_callback(void *ptr);
static void set_beacon_timer(struct libp_conn *c);

MEMB(send_queue_memb, struct packetqueue_item, MAX_SENDING_QUEUE);

static const struct packetbuf_attrlist attributes[] =
  {
    COLLECT_ATTRIBUTES
    PACKETBUF_ATTR_LAST
  };

struct data_msg_hdr {
    uint8_t flags, dummy;
    uint16_t rtmetric;
};

struct ack_msg {
    uint8_t flags, dummy;
    uint16_t rtmetric;
};
/*-----------------------Call backs---------------------------- */
static void
node_packet_received(struct unicast_conn *c, const rimeaddr_t *from)
{

}

static void
node_packet_sent(struct unicast_conn *c, int status, int transmissions)
{

}

static void
proactive_probing_callback(void *ptr)
{

}

static void
received_announcement(struct announcement *a, const rimeaddr_t *from, uint16_t id, uint16_t value)
{

}
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
    printf("broadcast received\n");
}

static const struct unicast_callbacks unicast_callbacks = {node_packet_received,
                                                           node_packet_sent};

static const struct broadcast_callbacks broadcast_call = { broadcast_recv};
/**
 * This function is called when the route advertisements need to be
 * transmitted more rapidly.
 *
 */
static void
bump_advertisement(struct libp_conn *c)
{
  announcement_bump(&c->announcement);
}

static void
update_rtmetric(struct libp_conn *c)
{

}

void libp_open(struct libp_conn *c, uint16_t channels, uint8_t is_router, const struct libp_callbacks *cb)
{
    unicast_open(&c->unicast_conn, channels + 1, &unicast_callbacks);
    broadcast_open(&c->broadcast_conn, channels - 1, &broadcast_call);
    channel_set_attributes(channels + 1, attributes);
    c->rtmetric = RTMETRIC_MAX;
    c->cb = cb;
    c->is_router = is_router;
    c->seqno = 10;
    c->eseqno = 0;
    LIST_STRUCT_INIT(c, send_queue_list);
    libp_neighbour_list_new(&c->neighbour_list);
    c->send_queue.list = &(c->send_queue_list);
    c->send_queue.memb = &send_queue_memb;
    libp_neighbour_init();

    announcement_register(&c->announcement, channels, received_announcement);
    if(c->is_router) {
        announcement_set_value(&c->announcement, RTMETRIC_MAX);
    }

    ctimer_set(&c->proactive_probing_timer, PROACTIVE_PROBING_INTERVAL, proactive_probing_callback, c);
}
void libp_close(struct libp_conn *c)
{

}

int libp_send(struct libp_conn *c, int rexmits)
{
    return 0;
}

void libp_set_sink(struct libp_conn *c, int should_be_sink)
{
    if(should_be_sink) {
        c->is_router = 1;
        c->rtmetric = RTMETRIC_SINK;
        PRINTF("collect_set_sink: c->rtmetric %d\n", c->rtmetric);
        bump_advertisement(c);

        /* Purge the outgoing packet queue. */
        while(packetqueue_len(&c->send_queue) > 0) {
            packetqueue_dequeue(&c->send_queue);
        }

    /* Stop the retransmission timer. */
    ctimer_stop(&c->retransmission_timer);
  } else {
    c->rtmetric = RTMETRIC_MAX;
  }

  announcement_set_value(&c->announcement, c->rtmetric);

  update_rtmetric(c);

  bump_advertisement(c);
}
/*---------------------------------------------------------------------------*/
static void
send_beacon(void *ptr)
{
    struct libp_conn *c = ptr;
    PRINTF("Sending beacon\n");

}
/*---------------------------------------------------------------------------*/
static void
set_beacon_timer(struct libp_conn *c)
{
  if(c->beacon_period != 0) {
    ctimer_set(&c->beacon_timer, (c->beacon_period / 2) +
               (random_rand() % (c->beacon_period / 2)),
               send_beacon, c);
  } else {
    ctimer_stop(&c->beacon_timer);
  }
}

void libp_set_beacon_period(struct libp_conn *c, clock_time_t period)
{
    c->beacon_period = period;
    set_beacon_timer(c);
}

/*---------------------------------------------------------------------------*/
int
libp_depth(struct libp_conn *c)
{
  return c->rtmetric;
}
/*---------------------------------------------------------------------------*/
const rimeaddr_t *
libp_parent(struct libp_conn *c)
{
  return &c->current_parent;
}
/*---------------------------------------------------------------------------*/
void
libp_purge(struct libp_conn *c)
{
  libp_neighbour_list_purge(&c->neighbour_list);
  rimeaddr_copy(&c->parent, &rimeaddr_null);
  update_rtmetric(c);
  if(DRAW_TREE) {
    PRINTF("#L %d 0\n", c->parent.u8[0]);
  }
  rimeaddr_copy(&c->parent, &rimeaddr_null);
}
