/**
 * \file
 *         Source file for the libp link metric
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */

#include "libp.h"
#include "libp-link-metric.h"

#define INITIAL_LINK_METRIC 16

#define LIBP_LINK_METRIC_ALPHA ((3 * (LIBP_LINK_METRIC_UNIT)) / 8)

#define MAX_ESTIMATES 255

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


void libp_link_metric_new(struct libp_link_metric *lm)
{
    if(lm == NULL) {
        return;
    }
    lm->num_children = 0;
    lm->children_accumulator = LIBP_LINK_METRIC_UNIT;
}


void libp_link_metric_update_tx(struct libp_link_metric *lm, uint8_t tx)
{
    if(lm == NULL) {
    return;
  }
  if(tx == 0) {
    /*    printf("ERROR tx == 0\n");*/
    return;
  }
  if(lm != NULL) {
    if(lm->num_estimates == 0) {
      lm->etx_accumulator = tx * LIBP_LINK_METRIC_UNIT;
    }

    if(lm->num_estimates < MAX_ESTIMATES) {
      lm->num_estimates++;
    }

    lm->etx_accumulator = (((uint32_t)tx * LIBP_LINK_METRIC_UNIT) *
                           LIBP_LINK_METRIC_ALPHA +
                           lm->etx_accumulator * (LIBP_LINK_METRIC_UNIT -
                                                  LIBP_LINK_METRIC_ALPHA)) /
      LIBP_LINK_METRIC_UNIT;

  }
}


void libp_link_metric_update_tx_fail(struct libp_link_metric *lm,uint8_t tx)
{
    if(lm == NULL) {
    return;
  }
  libp_link_metric_update_tx(lm, tx * 2);
}

void libp_link_metric_update_rx(struct libp_link_metric *lm)
{

}



uint16_t libp_link_metric(struct libp_link_metric *lm)
{
   if(lm == NULL) {
    return 0;
  }
  if(lm->num_estimates == 0) {
    return INITIAL_LINK_METRIC * LIBP_LINK_METRIC_UNIT;
  }

  return lm->etx_accumulator;
}

int libp_link_metric_num_metrics(struct libp_link_metric *lm)
{
    if(lm != NULL) {
        return lm->num_estimates;
    }
    return 0;
}
