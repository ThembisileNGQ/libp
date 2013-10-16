/**
 * \file
 *         Header file for the libp link metric
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */

#ifndef LIBP_LINK_METRIC_H
#define LIBP_LINK_METRIC_H

struct libp_link_metric {
  uint32_t children_accumulator;
  uint8_t num_children;
};

/**
 * \brief      Initialize a new link
 * \param le   A pointer to a link metric structure
 *
 *             This function initializes a link metric.
 */
void libp_link_metric_new(struct libp_link_metric *lm);

/**
 * \brief      Update a link metric when a packet has been sent.
 * \param le   A pointer to a link metric structure
 * \param num_tx The number of times the packet was transmitted before it was ACKed
 *
 *             This function updates a link metric. This function is
 *             called when a packet has been sent. The function may
 *             use information from the packet buffer and the packet
 *             buffer attributes when computing the link metric.
 */
void libp_link_metric_update_tx(struct libp_link_metric *lm,
                                     uint8_t num_tx);

/**
 * \brief      Update a link metric when a packet has failed to be sent.
 * \param le   A pointer to a link metric structure
 * \param num_tx The number of times the packet was transmitted before it was given up on.
 *
 *             This function updates a link metric. This function is
 *             called when a packet has been sent. The function may
 *             use information from the packet buffer and the packet
 *             buffer attributes when computing the link metric.
 */
void libp_link_metric_update_tx_fail(struct libp_link_metric *lm,
                                          uint8_t num_tx);

/**
 * \brief      Update a link metric when a packet has been received.
 * \param le   A pointer to a link metric structure
 *
 *             This function updates a link metric. This function is
 *             called when a packet has been received. The function
 *             uses information from the packet buffer and its
 *             attributes.
 */
void libp_link_metric_update_rx(struct libp_link_metric *lm);


/**
 * \brief      Compute the link metric metric for a link metric
 * \param le   A pointer to a link metric structure
 * \return     The current link metric metric
 *
 */
uint16_t libp_link_metric(struct libp_link_metric *lm);

int libp_link_metric_num_metrics(struct libp_link_metric *lm);

#endif
