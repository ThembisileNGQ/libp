/**
 * \file
 *         Header file for queue methods and handlers designed to be used in conjunction with libp.c
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "node.h"

/**
 * \struct Item
 * \properties:
 *	node - the node
 *      metric - next item attached to this node
 */

struct Item
{
    struct Node *node;
    struct Item *next;
};

struct Item *head; //holds the head of the queue
struct Item *tail; //holds the tail of the queue
int elements; //holds the queue size

/**
 * \brief      Initialize the queue
 *
 *             This function initializes the queue
 */
void queue_init();

/**
 * \brief      adds node to queue
 * \param node The node that needs to be pushed into the back of the queue
 *
 *	       The node is added to the back of the queue and the tail is set accordingly
 */
void queue_push(struct Node * node);

/**
 * \brief      Dequeues the queue
 * \returns    The head of the queue.
 *             This function pops the top off the queue
 */
int queue_dequeue();

/**
 * \brief      Getter for queue size
 * \returns    The size of the queue
 *             This function returns the size of the queue
 */
int queue_getSize();

/**
 * \brief      Getter for queue head
 * \returns    The queue head
 *             This function returns the head node of the queue
 */
struct Item * get_head();

/**
 * \brief      Getter for queue tail
 * \returns    The queue head
 *             This function returns the head node of the tails
 */
struct Item * get_tail();

#endif
