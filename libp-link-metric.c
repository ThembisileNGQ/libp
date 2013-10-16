/**
 * \file
 *         Source file for the libp link metric
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */

#include "libp.h"
#include "libp-link-metric.h"


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


void libp_link_metric_new(struct libp_link_metric *lm)
{

}


void libp_link_metric_update_tx(struct libp_link_metric *lm, uint8_t num_tx)
{

}


void libp_link_metric_update_tx_fail(struct libp_link_metric *lm,uint8_t num_tx)
{

}

void libp_link_metric_update_rx(struct libp_link_metric *lm)
{

}



uint16_t libp_link_metric(struct libp_link_metric *lm)
{
    return 0;
}

int libp_link_metric_num_metrics(struct libp_link_metric *lm)
{
    return 0;
}
