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

#define ACK_FLAGS_CONGESTED             0x80
#define ACK_FLAGS_DROPPED               0x40
#define ACK_FLAGS_LIFETIME_EXCEEDED     0x20
#define ACK_FLAGS_PARENT_CHOSEN         0xb
#define ACK_FLAGS_PARENT_REMOVED        0xa
#define ACK_FLAGS_RTMETRIC_NEEDS_UPDATE 0x10

/* The recent_packets list holds the sequence number, the originator,
   and the connection for packets that have been recently
   forwarded. This list is maintained to avoid forwarding duplicate
   packets. */
#define NUM_RECENT_PACKETS 16

#define MAX_HOPLIM 15

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

#define SIGNIFICANT_RTMETRIC_PARENT_CHANGE (LIBP_LINK_METRIC_UNIT +  \
                                            LIBP_LINK_METRIC_UNIT / 2)

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
#define DEBUG 0
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
/*static void update_parent(struct libp_conn *c);
static void rtmetric_compute(struct libp_conn *c);*/

static struct libp_conn *l;

MEMB(send_queue_memb, struct packetqueue_item, MAX_SENDING_QUEUE);

static const struct packetbuf_attrlist attributes[] =
  {
    COLLECT_ATTRIBUTES
    PACKETBUF_ATTR_LAST
  };

  struct recent_packet {
  struct libp_conn *conn;
  rimeaddr_t originator;
  uint8_t eseqno;
};

static struct recent_packet recent_packets[NUM_RECENT_PACKETS];
static uint8_t recent_packet_ptr;

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
send_ack(struct libp_conn *tc, const rimeaddr_t *to, int flags)
{
  struct ack_msg *ack;
  uint16_t packet_seqno = packetbuf_attr(PACKETBUF_ATTR_PACKET_ID);

  packetbuf_clear();
  packetbuf_set_datalen(sizeof(struct ack_msg));
  ack = packetbuf_dataptr();
  memset(ack, 0, sizeof(struct ack_msg));
  ack->rtmetric = tc->rtmetric;
  ack->flags = flags;

  packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, to);
  packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_PACKET_TYPE_ACK);
  packetbuf_set_attr(PACKETBUF_ATTR_RELIABLE, 0);
  packetbuf_set_attr(PACKETBUF_ATTR_ERELIABLE, 0);
  packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, packet_seqno);
  packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS, MAX_ACK_MAC_REXMITS);
  unicast_send(&tc->unicast_conn, to);

  PRINTF("%d.%d: libp: Sending ACK to %d.%d for %d (epacket_id %d)\n",
         rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
         to->u8[0], to->u8[1], packet_seqno,
         packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID));

  RIMESTATS_ADD(acktx);
  stats.acksent++;
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static void
send_next_packet(struct libp_conn *tc)
{
  /* Remove the first packet on the queue, the packet that was just sent. */
  packetqueue_dequeue(&tc->send_queue);
  tc->seqno = (tc->seqno + 1) % (1 << COLLECT_PACKET_ID_BITS);

  /* Cancel retransmission timer. */
  ctimer_stop(&tc->retransmission_timer);
  tc->sending = 0;
  tc->transmissions = 0;

  PRINTF("sending next packet, seqno %d, queue len %d\n",
         tc->seqno, packetqueue_len(&tc->send_queue));

  /* Send the next packet in the queue, if any. */
  send_queued_packet(tc);
}
/*---------------------------------------------------------------------------*/

static void
handle_ack(struct libp_conn *tc)
{
  struct ack_msg msg;
  struct libp_neighbour *n;

  PRINTF("handle_ack: sender %d.%d current_parent %d.%d, id %d seqno %d\n",
         packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[0],
         packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[1],
         tc->current_parent.u8[0], tc->current_parent.u8[1],
         packetbuf_attr(PACKETBUF_ATTR_PACKET_ID), tc->seqno);
  if(rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_SENDER),
                  &tc->current_parent) &&
     packetbuf_attr(PACKETBUF_ATTR_PACKET_ID) == tc->seqno) {

    /*    PRINTF("rtt %d / %d = %d.%02d\n",
           (int)(clock_time() - tc->send_time),
           (int)CLOCK_SECOND,
           (int)((clock_time() - tc->send_time) / CLOCK_SECOND),
           (int)(((100 * (clock_time() - tc->send_time)) / CLOCK_SECOND) % 100));*/

    stats.ackrecv++;
    memcpy(&msg, packetbuf_dataptr(), sizeof(struct ack_msg));

    /* It is possible that we receive an ACK for a packet that we
       think we have not yet sent: if our transmission was received by
       the other node, but the link-layer ACK was lost, our
       transmission counter may still be zero. If this is the case, we
       play it safe by believing that we have sent MAX_MAC_REXMITS
       transmissions. */
    if(tc->transmissions == 0) {
      tc->transmissions = MAX_MAC_REXMITS;
    }
    PRINTF("Updating link estimate with %d transmissions\n",
           tc->transmissions);
    n = libp_neighbour_list_find(&tc->neighbour_list,
                                   packetbuf_addr(PACKETBUF_ADDR_SENDER));

    if(n != NULL) {
      libp_neighbour_tx(n, tc->transmissions);
      libp_neighbour_update_rtmetric(n, msg.rtmetric);
      update_rtmetric(tc);
    }

    PRINTF("%d.%d: ACK from %d.%d after %d transmissions, flags %02x, rtmetric %d\n",
           rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
           tc->current_parent.u8[0], tc->current_parent.u8[1],
           tc->transmissions,
           msg.flags,
           msg.rtmetric);

    /* The ack contains information about the state of the packet and
       of the node that received it. We do different things depending
       on whether or not the packet was dropped. First, we check if
       the receiving node was congested. If so, we add a maximum
       transmission number to its routing metric, which increases the
       chance that another parent will be chosen. */
    if(msg.flags & ACK_FLAGS_CONGESTED) {
      PRINTF("ACK flag indicated parent was congested.\n");
      if(n != NULL) {
	libp_neighbour_set_congested(n);
	libp_neighbour_tx(n, tc->max_rexmits * 2);
      }
      update_rtmetric(tc);
    }
    if((msg.flags & ACK_FLAGS_DROPPED) == 0) {
      /* If the packet was successfully received, we send the next packet. */
      send_next_packet(tc);
    } else {
      /* If the packet was lost due to its lifetime being exceeded,
         there is not much more we can do with the packet, so we send
         the next one instead. */
      if((msg.flags & ACK_FLAGS_LIFETIME_EXCEEDED)) {
        send_next_packet(tc);
      } else {
        /* If the packet was dropped, but without the node being
           congested or the packets lifetime being exceeded, we
           penalize the parent and try sending the packet again. */
        PRINTF("ACK flag indicated packet was dropped by parent.\n");
        libp_neighbour_tx(n, tc->max_rexmits);
        update_rtmetric(tc);

        ctimer_set(&tc->retransmission_timer,
                   REXMIT_TIME + (random_rand() % (REXMIT_TIME)),
                   retransmit_callback, tc);
      }
    }

    /* Our neighbor's rtmetric needs to be updated, so we bump our
       advertisements. */
    if(msg.flags & ACK_FLAGS_RTMETRIC_NEEDS_UPDATE) {
      bump_advertisement(tc);
    }
    //set_keepalive_timer(tc);
  } else {
    stats.badack++;
  }
}
/*---------------------------------------------------------------------------*/
static void
add_packet_to_recent_packets(struct libp_conn *tc)
{
  /* Remember that we have seen this packet for later, but only if
     it has a length that is larger than zero. Packets with size
     zero are keepalive or proactive link estimate probes, so we do
     not record them in our history. */
  if(packetbuf_datalen() > sizeof(struct data_msg_hdr)) {
    recent_packets[recent_packet_ptr].eseqno =
      packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
    rimeaddr_copy(&recent_packets[recent_packet_ptr].originator,
                  packetbuf_addr(PACKETBUF_ADDR_ESENDER));
    recent_packets[recent_packet_ptr].conn = tc;
    recent_packet_ptr = (recent_packet_ptr + 1) % NUM_RECENT_PACKETS;
  }
}
static void
node_packet_received(struct unicast_conn *c, const rimeaddr_t *from)
{
    struct libp_conn *tc = (struct libp_conn *)
    ((char *)c - offsetof(struct libp_conn, unicast_conn));
    int i;
    struct data_msg_hdr hdr;
    uint8_t ackflags = 0;
    struct libp_neighbour *n;

    memcpy(&hdr, packetbuf_dataptr(), sizeof(struct data_msg_hdr));

    /* First update the neighbors rtmetric with the information in the
     packet header. */
  PRINTF("node_packet_received: from %d.%d rtmetric %d\n",
         from->u8[0], from->u8[1], hdr.rtmetric);
  n = libp_neighbour_list_find(&tc->neighbour_list,
                                 packetbuf_addr(PACKETBUF_ADDR_SENDER));
  if(n != NULL) {
    libp_neighbour_update_rtmetric(n, hdr.rtmetric);
    update_rtmetric(tc);
  }

  /* To protect against sending duplicate packets, we keep a list of
     recently forwarded packet seqnos. If the seqno of the current
     packet exists in the list, we immediately send an ACK and drop
     the packet. */
  if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
     PACKETBUF_ATTR_PACKET_TYPE_DATA) {
    rimeaddr_t ack_to;
    uint8_t packet_seqno;

    stats.datarecv++;

    /* Remember to whom we should send the ACK, since we reuse the
       packet buffer and its attributes when sending the ACK. */
    rimeaddr_copy(&ack_to, packetbuf_addr(PACKETBUF_ADDR_SENDER));
    packet_seqno = packetbuf_attr(PACKETBUF_ATTR_PACKET_ID);

    /* If the queue is more than half filled, we add the CONGESTED
       flag to our outgoing acks. */

    if(packetqueue_len(&tc->send_queue) >= MAX_SENDING_QUEUE / 2) {
      ackflags |= ACK_FLAGS_CONGESTED;
    }

    for(i = 0; i < NUM_RECENT_PACKETS; i++) {
      if(recent_packets[i].conn == tc &&
         recent_packets[i].eseqno == packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID) &&
         rimeaddr_cmp(&recent_packets[i].originator,
                      packetbuf_addr(PACKETBUF_ADDR_ESENDER))) {
        /* This is a duplicate of a packet we recently received, so we
           just send an ACK. */
        PRINTF("%d.%d: found duplicate packet from %d.%d with seqno %d, via %d.%d\n",
               rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
               recent_packets[i].originator.u8[0], recent_packets[i].originator.u8[1],
               packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
               packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[0],
               packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[1]);
        send_ack(tc, &ack_to, ackflags);
        stats.duprecv++;
        return;
      }
    }

    /* If we are the sink, the packet has reached its final
       destination and we call the receive function. */
    if(tc->rtmetric == RTMETRIC_SINK) {
      struct queuebuf *q;

      add_packet_to_recent_packets(tc);

      /* We first send the ACK. We copy the data packet to a queuebuf
         first. */
      q = queuebuf_new_from_packetbuf();
      if(q != NULL) {
        send_ack(tc, &ack_to, 0);
        queuebuf_to_packetbuf(q);
        queuebuf_free(q);
      } else {
        PRINTF("%d.%d: collect: could not send ACK to %d.%d for %d: no queued buffers\n",
               rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
               ack_to.u8[0], ack_to.u8[1],
               packet_seqno);
        stats.ackdrop++;
      }


      PRINTF("%d.%d: sink received packet %d from %d.%d via %d.%d\n",
             rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
             packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
             packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[0],
             packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[1],
             from->u8[0], from->u8[1]);

      packetbuf_hdrreduce(sizeof(struct data_msg_hdr));
      /* Call receive function. */
      if(packetbuf_datalen() > 0 && tc->cb->recv != NULL) {
        tc->cb->recv(packetbuf_addr(PACKETBUF_ADDR_ESENDER),
                     packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
                     packetbuf_attr(PACKETBUF_ATTR_HOPS));
      }
      return;
    } else if(packetbuf_attr(PACKETBUF_ATTR_TTL) > 1 &&
              tc->rtmetric != RTMETRIC_MAX) {
      /* If we are not the sink, we forward the packet to our best
         neighbor. First, we make sure that the packet comes from a
         neighbor that has a higher rtmetric than we have. If not, we
         have a loop and we inform the sender that its rtmetric needs
         to be updated. Second, we set our rtmetric in the outgoing
         packet to let the next hop know what our rtmetric is. Third,
         we update the hop count and ttl. */

      if(hdr.rtmetric <= tc->rtmetric) {
        ackflags |= ACK_FLAGS_RTMETRIC_NEEDS_UPDATE;
      }

      packetbuf_set_attr(PACKETBUF_ATTR_HOPS,
                         packetbuf_attr(PACKETBUF_ATTR_HOPS) + 1);
      packetbuf_set_attr(PACKETBUF_ATTR_TTL,
                         packetbuf_attr(PACKETBUF_ATTR_TTL) - 1);


      PRINTF("%d.%d: packet received from %d.%d via %d.%d, sending %d, max_rexmits %d\n",
             rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
             packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[0],
             packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[1],
             from->u8[0], from->u8[1], tc->sending,
             packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT));

      /* We try to enqueue the packet on the outgoing packet queue. If
         we are able to enqueue the packet, we send a positive ACK. If
         we are unable to enqueue the packet, we send a negative ACK
         to inform the sender that the packet was dropped due to
         memory problems. We first check the size of our sending queue
         to ensure that we always have entries for packets that
         are originated by this node. */
      if(packetqueue_len(&tc->send_queue) <= MAX_SENDING_QUEUE - MIN_AVAILABLE_QUEUE_ENTRIES &&
         packetqueue_enqueue_packetbuf(&tc->send_queue,
                                       FORWARD_PACKET_LIFETIME_BASE *
                                       packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT),
                                       tc)) {
        add_packet_to_recent_packets(tc);
        send_ack(tc, &ack_to, ackflags);
        send_queued_packet(tc);
      } else {
        send_ack(tc, &ack_to,
                 ackflags | ACK_FLAGS_DROPPED | ACK_FLAGS_CONGESTED);
        PRINTF("%d.%d: packet dropped: no queue buffer available\n",
                  rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
        stats.qdrop++;
      }
    } else if(packetbuf_attr(PACKETBUF_ATTR_TTL) <= 1) {
      PRINTF("%d.%d: packet dropped: ttl %d\n",
             rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
             packetbuf_attr(PACKETBUF_ATTR_TTL));
      send_ack(tc, &ack_to, ackflags |
               ACK_FLAGS_DROPPED | ACK_FLAGS_LIFETIME_EXCEEDED);
      stats.ttldrop++;
    }
  } else if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
            PACKETBUF_ATTR_PACKET_TYPE_ACK) {
    PRINTF("Collect: incoming ack %d from %d.%d (%d.%d) seqno %d (%d)\n",
           packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE),
           packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[0],
           packetbuf_addr(PACKETBUF_ADDR_SENDER)->u8[1],
           tc->current_parent.u8[0],
           tc->current_parent.u8[1],
           packetbuf_attr(PACKETBUF_ATTR_PACKET_ID),
           tc->seqno);
    handle_ack(tc);
    stats.ackrecv++;
  }
  return;
}



/*---------------------------------------------------------------------------*/
static void
timedout(struct libp_conn *c)
{
  struct libp_neighbour *n;
  PRINTF("%d.%d: timedout after %d retransmissions to %d.%d (max retransmissions %d): packet dropped\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], c->transmissions,
         c->current_parent.u8[0], c->current_parent.u8[1],
         c->max_rexmits);
  PRINTF("%d.%d: timedout after %d retransmissions to %d.%d (max retransmissions %d): packet dropped\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], c->transmissions,
         c->current_parent.u8[0], c->current_parent.u8[1],
         c->max_rexmits);

  c->sending = 0;
  n = libp_neighbour_list_find(&c->neighbour_list,
                                 &c->current_parent);
  if(n != NULL) {
    libp_neighbour_tx_fail(n, c->max_rexmits);
  }
  update_rtmetric(c);
  send_next_packet(c);
  //set_keepalive_timer(c);
}
/*---------------------------------------------------------------------------*/


static void
node_packet_sent(struct unicast_conn *c, int status, int transmissions)
{
     struct libp_conn *tc = (struct libp_conn *)
    ((char *)c - offsetof(struct libp_conn, unicast_conn));

  /* For data packets, we record the number of transmissions */
  if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
     PACKETBUF_ATTR_PACKET_TYPE_DATA) {

    tc->transmissions += transmissions;
    PRINTF("tx %d\n", tc->transmissions);
    PRINTF("%d.%d: MAC sent %d transmissions to %d.%d, status %d, total transmissions %d\n",
           rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
           transmissions,
           tc->current_parent.u8[0], tc->current_parent.u8[1],
           status, tc->transmissions);
    if(tc->transmissions >= tc->max_rexmits) {
      timedout(tc);
      stats.timedout++;
    } else {
      clock_time_t time = REXMIT_TIME / 2 + (random_rand() % (REXMIT_TIME / 2));
      PRINTF("retransmission time %lu\n", time);
      ctimer_set(&tc->retransmission_timer, time,
                 retransmit_callback, tc);
    }
  }
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

    PRINTF("received announcement from %d.%d \n", from->u8[0], from->u8[1]);
}

static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
    PRINTF("beacon received from %d.%d \n",from->u8[0], from->u8[1]);
    //PRINTF("current parent: %d.%d \n", l->parent.u8[0], l->parent.u8[1]);

    if(!l->is_sink) { //fix
        clock_time_t period = REBROADCAST_TIME*CLOCK_SECOND;
        libp_set_beacon_period(l,period);
    }
}

/*---------------------------------------------------------------------------*/
static int
enqueue_dummy_packet(struct libp_conn *c, int rexmits)
{
  struct libp_neighbor *n;

  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, c->eseqno - 1);
  packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &rimeaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 1);
  packetbuf_set_attr(PACKETBUF_ATTR_TTL, 1);
  packetbuf_set_attr(PACKETBUF_ATTR_MAX_REXMIT, rexmits);

  PRINTF("%d.%d: enqueueing dummy packet %d, max_rexmits %d\n",
         rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
         packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
         packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT));

  /* Allocate space for the header. */
  packetbuf_hdralloc(sizeof(struct data_msg_hdr));

  n = libp_neighbour_list_find(&c->neighbour_list, &c->parent);
  if(n != NULL) {
    return packetqueue_enqueue_packetbuf(&c->send_queue,
                                         FORWARD_PACKET_LIFETIME_BASE * rexmits,
                                         c);
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
proactive_probing_callback(void *ptr)
{
    struct libp_conn *c = ptr;
    struct packetqueue_item *i;

    ctimer_set(&c->proactive_probing_timer, PROACTIVE_PROBING_INTERVAL,
             proactive_probing_callback, ptr);

  /* Only do proactive link probing if we are not the sink and if we
     have a route. */
  if(c->rtmetric != RTMETRIC_SINK && c->rtmetric != RTMETRIC_MAX) {
  /* Grab the first packet on the send queue to see if the queue is
     empty or not. */
  i = packetqueue_first(&c->send_queue);
  if(i == NULL) {
    /* If there are no packets to send, we go through the list of
       neighbors to find a potential parent for which we do not have a
       link estimate and send a dummy packet to it. This allows us to
       quickly gauge the link quality of neighbors that we do not
       currently use as parents. */
      struct libp_neighbour *n;

      /* Find the neighbor with the lowest number of estimates. */
      for(n = list_head(libp_neighbour_list(&c->neighbour_list));
          n != NULL; n = list_item_next(n)) {
        if(n->rtmetric + LIBP_LINK_METRIC_UNIT < c->rtmetric &&
           libp_link_metric_num_metrics(&n->lm) == 0) {
          rimeaddr_t current_parent;

          PRINTF("proactive_probing_callback: found neighbor with no link estimate, %d.%d\n",
                 n->addr.u8[RIMEADDR_SIZE - 2], n->addr.u8[RIMEADDR_SIZE - 1]);

          rimeaddr_copy(&current_parent, &c->parent);
          rimeaddr_copy(&c->parent, &n->addr);
          if(enqueue_dummy_packet(c, PROACTIVE_PROBING_REXMITS)) {
            send_queued_packet(c);
          }
          rimeaddr_copy(&c->parent, &current_parent);
          return;
        }
      }
    }
    PRINTF("%d.%d: nothing on queue\n",
           rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    return;
  }
}
/*---------------------------------------------------------------------------*/
static void
send_packet(struct libp_conn *c, struct libp_neighbour *n)
{
  clock_time_t time;

  PRINTF("Sending packet to %d.%d, %d transmissions\n",
         n->addr.u8[0], n->addr.u8[1],
         c->transmissions);
  /* Defensive programming: if a bug in the MAC/RDC layers will cause
     it to not call us back, we'll set up the retransmission timer
     with a high timeout, so that we can cancel the transmission and
     send a new one. */
  time = 16 * REXMIT_TIME;
  ctimer_set(&c->retransmission_timer, time,
             retransmit_not_sent_callback, c);
  c->send_time = clock_time();

  unicast_send(&c->unicast_conn, &n->addr);
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called to retransmit the first packet on the send
 * queue.
 *
 */
static void
retransmit_current_packet(struct libp_conn *c)
{
  struct queuebuf *q;
  struct libp_neighbour *n;
  struct packetqueue_item *i;
  struct data_msg_hdr hdr;
  int max_mac_rexmits;

  /* Grab the first packet on the send queue, which is the one we are
     about to retransmit. */
  i = packetqueue_first(&c->send_queue);
  if(i == NULL) {
      PRINTF("%d.%d: nothing on queue\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    /* No packet on the queue, so there is nothing for us to send. */
    return;
  }

  /* Get hold of the queuebuf. */
  q = packetqueue_queuebuf(i);
  if(q != NULL) {

    update_rtmetric(c);

    /* Place the queued packet into the packetbuf. */
    queuebuf_to_packetbuf(q);

    /* Pick the neighbor to which to send the packet. If we have found
       a better parent while we were transmitting this packet, we
       chose that neighbor instead. If so, we need to attribute the
       transmissions we made for the parent to that neighbor. */
    if(!rimeaddr_cmp(&c->current_parent, &c->parent)) {
      /*      struct collect_neighbor *current_neighbor;
      current_neighbor = collect_neighbor_list_find(&c->neighbor_list,
                                                    &c->current_parent);
      if(current_neighbor != NULL) {
        collect_neighbor_tx(current_neighbor, c->max_rexmits);
        }*/

      PRINTF("parent change from %d.%d to %d.%d after %d tx\n",
             c->current_parent.u8[0], c->current_parent.u8[1],
             c->parent.u8[0], c->parent.u8[1],
             c->transmissions);

      rimeaddr_copy(&c->current_parent, &c->parent);
      c->transmissions = 0;
    }
    n = libp_neighbour_list_find(&c->neighbour_list, &c->current_parent);

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
      packetbuf_set_attr(PACKETBUF_ATTR_RELIABLE, 1);
      max_mac_rexmits = c->max_rexmits - c->transmissions > MAX_MAC_REXMITS?
        MAX_MAC_REXMITS : c->max_rexmits - c->transmissions;
      packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS, max_mac_rexmits);
      packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, c->seqno);

      /* Copy our rtmetric into the packet header of the outgoing
         packet. */
      memset(&hdr, 0, sizeof(hdr));
      hdr.rtmetric = c->rtmetric;
      memcpy(packetbuf_dataptr(), &hdr, sizeof(struct data_msg_hdr));

      /* Send the packet. */
      send_packet(c, n);
    }
  }

}
/*---------------------------------------------------------------------------*/


static void
retransmit_not_sent_callback(void *ptr)
{
  struct libp_conn *c = ptr;

  PRINTF("retransmit not sent, %d transmissions\n", c->transmissions);
  c->transmissions += MAX_MAC_REXMITS + 1;
  retransmit_callback(c);
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called from a ctimer that is setup when a packet
 * is sent. The purpose of this function is to either retransmit the
 * current packet, or timeout the packet. The descision is made
 * depending on how many times the packet has been transmitted. The
 * ctimer is set up in the function node_packet_sent().
 */
static void
retransmit_callback(void *ptr)
{
  struct libp_conn *c = ptr;

  PRINTF("retransmit, %d transmissions\n", c->transmissions);
  if(c->transmissions >= c->max_rexmits) {
    timedout(c);
    stats.timedout++;
  } else {
    c->sending = 0;
    retransmit_current_packet(c);
  }
}
/*---------------------------------------------------------------------------*/


static const struct unicast_callbacks unicast_callbacks = {node_packet_received,
                                                           node_packet_sent};

static const struct broadcast_callbacks broadcast_call = { broadcast_recv};
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
    struct libp_neighbour *current;
    struct libp_neighbour *best;

  /* We grab the collect_neighbor struct of our current parent. */
  current = libp_neighbour_list_find(&c->neighbour_list, &c->parent);

  /* We call the collect_neighbor module to find the current best
     parent. */
  best = libp_neighbour_list_best(&c->neighbour_list);

  /* We check if we need to switch parent. Switching parent is done in
     the following situations:

     * We do not have a current parent.
     * The best parent is significantly better than the current parent.

     If we do not have a current parent, and have found a best parent,
     we simply use the new best parent.

     If we already have a current parent, but have found a new parent
     that is better, we employ a heuristic to avoid switching parents
     too often. The new parent must be significantly better than the
     current parent. Being "significantly better" is defined as having
     an rtmetric that is has a difference of at least 1.5 times the
     COLLECT_LINK_ESTIMATE_UNIT. This is derived from the experience
     by Gnawali et al (SenSys 2009). */
     if(best != NULL) {
    rimeaddr_t previous_parent;

    if(DRAW_TREE) {
      rimeaddr_copy(&previous_parent, &c->parent);
    }

    if(current == NULL) {
      /* New parent. */
      PRINTF("update_parent: new parent %d.%d\n",
             best->addr.u8[0], best->addr.u8[1]);
      rimeaddr_copy(&c->parent, &best->addr);
      stats.foundroute++;
      bump_advertisement(c);
    } else {
      if(DRAW_TREE) {
        PRINTF("#A e=%d\n", libp_neighbour_link_metric(best));
      }
      if(libp_neighbour_rtmetric_link_metric(best) +
         SIGNIFICANT_RTMETRIC_PARENT_CHANGE <
         libp_neighbour_rtmetric_link_metric(current)) {

        /* We switch parent. */
        PRINTF("update_parent: new parent %d.%d (%d) old parent %d.%d (%d)\n",
               best->addr.u8[0], best->addr.u8[1],
               libp_neighbour_rtmetric(best),
               c->parent.u8[0], c->parent.u8[1],
               libp_neighbour_rtmetric(current));
        rimeaddr_copy(&c->parent, &best->addr);
        stats.newparent++;
        /* Since we now have a significantly better or worse rtmetric than
           we had before, we let our neighbors know this quickly. */
        bump_advertisement(c);

        if(DRAW_TREE) {
          PRINTF("#A e=%d\n", libp_neighbour_link_metric(best));
          /*          {
            int i;
            int etx = 0;
            PRINTF("#A l=");
            for(i = 0; i < 8; i++) {
              PRINTF("%d ", best->le.history[(best->le.historyptr - 1 - i) & 7]);
              etx += current->le.history[i];
            }
            PRINTF("\n");
            }*/
        }
      } else {
        if(DRAW_TREE) {
          PRINTF("#A e=%d\n", libp_neighbour_link_metric(current));
          /*          {
            int i;
            int etx = 0;
            PRINTF("#A l=");
            for(i = 0; i < 8; i++) {
              PRINTF("%d ", current->le.history[(current->le.historyptr - 1 - i) & 7]);
              etx += current->le.history[i];
            }
            PRINTF("\n");
            }*/
        }
      }
    }
    if(DRAW_TREE) {
      if(!rimeaddr_cmp(&previous_parent, &c->parent)) {
        if(!rimeaddr_cmp(&previous_parent, &rimeaddr_null)) {
          PRINTF("#L %d 0\n", previous_parent.u8[0]);
        }
        PRINTF("#L %d 1\n", c->parent.u8[0]);
      }
    }
  } else {
    /* No parent. */
    if(!rimeaddr_cmp(&c->parent, &rimeaddr_null)) {
      if(DRAW_TREE) {
        PRINTF("#L %d 0\n", c->parent.u8[0]);
      }
      stats.routelost++;
    }
    rimeaddr_copy(&c->parent, &rimeaddr_null);
  }

}

static uint16_t
rtmetric_compute(struct libp_conn *c)
{
  struct libp_neighbour *n;
  uint16_t rtmetric = RTMETRIC_MAX;

  /* This function computes the current rtmetric for this node. It
     uses the rtmetric of the parent node in the tree and adds the
     current link estimate from us to the parent node. */

  /* The collect connection structure stores the address of its
     current parent. We look up the neighbor identification struct in
     the collect-neighbor list. */
  n = libp_neighbour_list_find(&c->neighbour_list, &c->parent);

  /* If n is NULL, we have no best neighbor. Thus our rtmetric is
     then COLLECT_RTMETRIC_MAX. */
  if(n == NULL) {
    rtmetric = RTMETRIC_MAX;
  } else {
    /* Our rtmetric is the rtmetric of our parent neighbor plus
       the expected transmissions to reach that neighbor. */
    rtmetric = libp_neighbour_rtmetric_link_metric(n);
  }

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
    announcement_remove(&c->announcement);

    unicast_close(&c->unicast_conn);

    broadcast_close(&c->broadcast_conn);

    while(packetqueue_first(&c->send_queue) != NULL) {
    packetqueue_dequeue(&c->send_queue);
  }
}

int libp_send(struct libp_conn *c, int rexmits)
{
    struct libp_neighbour *n;
    int ret;

    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, c->eseqno);

  /* Increase the sequence number for the packet we send out. We
     employ a trick that allows us to see that a node has been
     rebooted: if the sequence number wraps to 0, we set it to half of
     the sequence number space. This allows us to detect reboots,
     since if a sequence number is less than half of the sequence
     number space, the data comes from a node that was recently
     rebooted. */

    c->eseqno = (c->eseqno + 1) % (1 << COLLECT_PACKET_ID_BITS);

    if(c->eseqno == 0) {
    c->eseqno = ((int)(1 << COLLECT_PACKET_ID_BITS)) / 2;
  }
  packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &rimeaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 1);
  packetbuf_set_attr(PACKETBUF_ATTR_TTL, MAX_HOPLIM);
  if(rexmits > MAX_REXMITS) {
    packetbuf_set_attr(PACKETBUF_ATTR_MAX_REXMIT, MAX_REXMITS);
  } else {
    packetbuf_set_attr(PACKETBUF_ATTR_MAX_REXMIT, rexmits);
  }

  PRINTF("%d.%d: originating packet %d, max_rexmits %d\n",
         rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
         packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
         packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT));

  if(c->rtmetric == RTMETRIC_SINK) {
    packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 0);
    if(c->cb->recv != NULL) {
      c->cb->recv(packetbuf_addr(PACKETBUF_ADDR_ESENDER),
		   packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
		   packetbuf_attr(PACKETBUF_ATTR_HOPS));
    }
    return 1;
  } else {

    /* Allocate space for the header. */
    packetbuf_hdralloc(sizeof(struct data_msg_hdr));

    if(packetqueue_enqueue_packetbuf(&c->send_queue,
                                     FORWARD_PACKET_LIFETIME_BASE *
                                     packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT),
                                     c)) {
      send_queued_packet(c);
      ret = 1;
    } else {
      PRINTF("%d.%d: drop originated packet: no queuebuf\n",
             rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
      PRINTF("%d.%d: drop originated packet: no queuebuf\n",
             rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
      ret = 0;
    }


    n = libp_neighbour_list_find(&c->neighbour_list, &c->parent);
    if(n != NULL) {
      PRINTF("%d.%d: sending to %d.%d\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	     n->addr.u8[0], n->addr.u8[1]);
    } else {
      PRINTF("%d.%d: did not find any neighbor to send to\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

        }
    }
    return ret;
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
