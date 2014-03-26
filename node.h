/**
 * \file
 *         Header file for Node struct
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */


#ifndef NODE_H
#define NODE_H

/**
 * \struct Node
 * \properties:
 *	id - identity
 *      metric - node weight
 *      firstchild - the first child of the node/vertex
 *      nextsibling - the immediate sibling of the node/vertex
 */

struct Node
{
    int id;
    int metric;
    struct Node *firstchild;
    struct Node *nextsibling;
};

#endif
