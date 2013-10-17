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

#define REBROADCAST_TIME 10
#define BEACONING_PERIOD 30
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
static void bump_advertisement(struct libp_conn *c);
static void update_rtmetric(struct libp_conn *c);
static void update_parent(struct libp_conn *c);
static void send_queued_packet(struct libp_conn *c);
static void rtmetric_compute(struct libp_conn *c);

static struct libp_conn *l;

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

struct beacon_message {
    uint8_t flags, dummy;
    uint16_t rtmetric;
    uint8_t seqno;
};

/* Statistics structure */
struct {
  uint32_t foundroute;
  uint32_t newparent;
  uint32_t routelost;

  uint32_t acksent;
  uint32_t datasent;

  uint32_t datarecv;
  uint32_t ackrecv;
  uint32_t badack;
  uint32_t duprecv;

  uint32_t qdrop;
  uint32_t rtdrop;
  uint32_t ttldrop;
  uint32_t ackdrop;
  uint32_t timedout;
} stats;
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
    struct libp_conn *c = (struct libp_conn *)
    ((char *)a - offsetof(struct libp_conn, announcement));
    struct libp_neighbour *n;

    n = libp_neighbour_list_find(&c->neighbour_list, from);

    if(n == NULL) {
    /* only add neighbours with a lower rank than ours */
        if(value < c->rtmetric) {
            libp_neighbour_list_add(&c->neighbour_list, from, value);
            PRINTF("%d.%d: new neighbor %d.%d, rtmetric %d\n",
             rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
             from->u8[0], from->u8[1], value);
        }
    } else {
    /* Check if the advertised rtmetric has changed to
       RTMETRIC_MAX. This may indicate that the neighbor has lost its
       routes or that it has rebooted. In either case, we bump our
       advertisement rate to allow our neighbor to receive a new
       rtmetric from us. If our neighbor already happens to have an
       rtmetric of RTMETRIC_MAX recorded, it may mean that our
       neighbor does not hear our advertisements. If this is the case,
       we should not bump our advertisement rate. */
    if(value == RTMETRIC_MAX &&
       libp_neighbour_rtmetric(n) != RTMETRIC_MAX) {
      bump_advertisement(c);
    }
    libp_neighbour_update_rtmetric(n, value);
    PRINTF("%d.%d: updating neighbor %d.%d, etx %d\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	   n->addr.u8[0], n->addr.u8[1], value);
  }

  update_rtmetric(c);

    PRINTF("received announcement from %d.%d with ");
}

static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
    PRINTF("broadcast received from %d.%d \n",from->u8[0], from->u8[1]);

    if(!l->is_sink) { //fix
        clock_time_t period = REBROADCAST_TIME*CLOCK_SECOND;
        libp_set_beacon_period(l,period);
    }
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

static void update_parent(struct libp_conn *c)
{

}

static void
send_queued_packet(struct libp_conn *c)
{
    struct queuebuf *q;
    struct libp_neighbour *n;
    struct packetqueue_item *i;
    struct data_msg_hdr hdr;
    int max_mac_rexmits;

     /* If we are currently sending a packet, we do not attempt to send
     another one. */
    if(c->sending) {
        PRINTF("%d.%d: queue, c is sending\n",
            rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
        return;
    }

    /* Grab the first packet on the send queue. */
  i = packetqueue_first(&c->send_queue);
  if(i == NULL) {
        PRINTF("%d.%d: nothing on queue\n",
            rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
        return;
    }

    /* We should send the first packet from the queue. */
  q = packetqueue_queuebuf(i);
  if(q != NULL) {
    /* Place the queued packet into the packetbuf. */
    queuebuf_to_packetbuf(q);

    /* Pick the neighbor to which to send the packet. We use the
       parent in the n->parent. */
    n = libp_neighbour_list_find(&c->neighbour_list, &c->parent);

    if(n != NULL) {

      /* If the connection had a neighbor, we construct the packet
         buffer attributes and set the appropriate flags in the
         Collect connection structure and send the packet. */

      PRINTF("%d.%d: sending packet to %d.%d with eseqno %d\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	     n->addr.u8[0], n->addr.u8[1],
             packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID));

      /* Mark that we are currently sending a packet. */
      c->sending = 1;

      /* Remember the parent that we sent this packet to. */
      rimeaddr_copy(&c->current_parent, &c->parent);

      /* This is the first time we transmit this packet, so set
         transmissions to zero. */
      c->transmissions = 0;

      /* Remember that maximum amount of retransmissions we should
         make. This is stored inside a packet attribute in the packet
         on the send queue. */
      c->max_rexmits = packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT);

      /* Set the packet attributes: this packet wants an ACK, so we
         sent the PACKETBUF_ATTR_RELIABLE flag; the MAC should retry
         MAX_MAC_REXMITS times; and the PACKETBUF_ATTR_PACKET_ID is
         set to the current sequence number on the connection. */
      packetbuf_set_attr(PACKETBUF_ATTR_RELIABLE, 1);

      max_mac_rexmits = c->max_rexmits > MAX_MAC_REXMITS?
        MAX_MAC_REXMITS : c->max_rexmits;
      packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS, max_mac_rexmits);
      packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, c->seqno);

      stats.datasent++;

      /* Copy our rtmetric into the packet header of the outgoing
         packet. */
      memset(&hdr, 0, sizeof(hdr));
      hdr.rtmetric = c->rtmetric;
      memcpy(packetbuf_dataptr(), &hdr, sizeof(struct data_msg_hdr));

      /* Send the packet. */
      send_packet(c, n);

    }


}

static uint16_t
rtmetric_compute(struct libp_conn *c)
{
  struct libp_neighbour *n;
  uint16_t rtmetric = RTMETRIC_MAX;

  return rtmetric;
}

/**
 * This function is called whenever there is a chance that the routing
 * metric has changed. The function goes through the list of neighbors
 * to compute the new routing metric. If the metric has changed, it
 * notifies neighbors.
 */
static void
update_rtmetric(struct libp_conn *c)
{
    PRINTF("update_rtmetric: tc->rtmetric %d\n", c->rtmetric);

    if(c->rtmetric != RTMETRIC_SINK) {
        uint16_t old_rtmetric, new_rtmetric;

        old_rtmetric = c->rtmetric;

        update_parent(c);

        new_rtmetric = rtmetric_compute(c);

        if(new_rtmetric == RTMETRIC_SINK) {
            /* Defensive programming: if the new rtmetric somehow got to be
            the rtmetric of the sink, there is a bug somewhere. To avoid
            destroying the network, we simply will not assume this new
            rtmetric. Instead, we set our rtmetric to maximum, to
            indicate that we have no sane route. */
            new_rtmetric = RTMETRIC_MAX;
        }

        c->rtmetric = new_rtmetric;

        if(c->is_router) {
            /* we update the rtmetric:value announcement */
            announcement_set_value(&c->announcement, c->rtmetric);
        }
        PRINTF("%d.%d: new rtmetric %d\n",
           rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
           c->rtmetric);

        /* We got a new, working, route we send any queued packets we may have. */
        if(old_rtmetric == RTMETRIC_MAX && new_rtmetric != RTMETRIC_MAX) {
            PRINTF("Sending queued packet because rtmetric was max\n");
            send_queued_packet(c);
        }

    }
}

void libp_open(struct libp_conn *c, uint16_t channels, uint8_t is_router, const struct libp_callbacks *cb)
{
    unicast_open(&c->unicast_conn, channels + 1, &unicast_callbacks);
    broadcast_open(&c->broadcast_conn, channels - 1, &broadcast_call);
    channel_set_attributes(channels + 1, attributes);
    l = c;
    c->rtmetric = RTMETRIC_MAX;
    c->cb = cb;
    c->is_router = is_router;
    c->is_sink = 0;
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
        c->is_sink = 1;
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
    struct beacon_message msg;
    msg.rtmetric = c->rtmetric;
    packetbuf_copyfrom(&msg, sizeof(struct beacon_message));
    broadcast_send(&c->broadcast_conn);
    PRINTF("Sending beacon\n");
    if(c->is_sink) {
        clock_time_t period = BEACONING_PERIOD * CLOCK_SECOND;
        libp_set_beacon_period(c, period);
        //calling non-static method from static context, but it still works?
    }

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
