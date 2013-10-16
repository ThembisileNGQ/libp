/**
 * \file
 *         Example of how the LIBP paradigm works.
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */

#include "contiki.h"
#include "lib/random.h"
#include "net/rime.h"
#include "libp.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"

#include "net/netstack.h"


#include <stdio.h>

#define BEACONING_PERIOD 30
#define CHANNEL 130

static struct libp_conn lc;

/*---------------------------------------------------------------------------*/
PROCESS(example_libp_process, "Test LIBP process");
AUTOSTART_PROCESSES(&example_libp_process);
/*---------------------------------------------------------------------------*/
static void
recv(const rimeaddr_t *originator, uint8_t seqno, uint8_t hops)
{
  printf("Sink got message from %d.%d, seqno %d, hops %d: len %d '%s'\n",
	 originator->u8[0], originator->u8[1],
	 seqno, hops,
	 packetbuf_datalen(),
	 (char *)packetbuf_dataptr());
}
/*---------------------------------------------------------------------------*/
static const struct collect_callbacks callbacks = { recv };
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_libp_process, ev, data)
{
  static struct etimer periodic;
  static struct etimer et;

  PROCESS_BEGIN();
  libp_open(&lc, CHANNEL, LIBP_ROUTER, &callbacks);

  if(rimeaddr_node_addr.u8[0] == 1 &&
     rimeaddr_node_addr.u8[1] == 0) {
    clock_time_t period;
    period = CLOCK_SECOND * BEACONING_PERIOD;
	printf("I am sink\n");
	libp_set_sink(&lc, 1);
	libp_set_beacon_period(&lc, period);

  }

  /* Allow some time for the network to settle. */
  etimer_set(&et, 120 * CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&et));

  while(1) {

    /* Send a packet every 30 seconds. */
   /* if(etimer_expired(&periodic)) {
      etimer_set(&periodic, CLOCK_SECOND * 30);
      etimer_set(&et, random_rand() % (CLOCK_SECOND * 30));
    }

    PROCESS_WAIT_EVENT();


    if(etimer_expired(&et)) {
      static rimeaddr_t oldparent;
      const rimeaddr_t *parent;

      printf("Sending\n");
      packetbuf_clear();
      packetbuf_set_datalen(sprintf(packetbuf_dataptr(),
				  "%s", "Hello") + 1);
      libp_send(&tc, 15);

      parent = libp_parent(&tc);
      if(!rimeaddr_cmp(parent, &oldparent)) {
        if(!rimeaddr_cmp(&oldparent, &rimeaddr_null)) {
          printf("#L %d 0\n", oldparent.u8[0]);
        }
        if(!rimeaddr_cmp(parent, &rimeaddr_null)) {
          printf("#L %d 1\n", parent->u8[0]);
        }
        rimeaddr_copy(&oldparent, parent);
      }
    }*/

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
