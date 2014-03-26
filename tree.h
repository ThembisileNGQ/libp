/**
 * \file
 *         Header file for the tree methods and bread first search methods to be used in the tree
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */


#ifndef TREE_H
#define TREE_H

#include "tree.h"
#include "node.h"

struct Node *root; //gateway Node
struct Node *nodes[128]; //holds an ordered list of all the nodes in the network
int visited[128]; //keeps track of visited nodes in bfs algorithm
int advertised[128]; //holds the children weight for the advertised weight
int calculated[128]; //holds the children weight for the calculated bfs amount
int bfs_count; //placeholder variable for the visited calculations of bfs

/**
 * \brief      Initialize the queue
 *
 *             This function initializes the queue
 */
void tree_init();

/**
 * \brief      Initialize the queue
 * \param
 *      parent the parent
 *      metric the metric/weight of the node with id param:id
 *      id     the id of the node
 *
 *             This function adds a node to the tree structure
 */
void add_node(int parent, int metric, int id);

/**
 * \brief      Setter for node metric
 * \param
 *      metric the metric/weight of the node with id param:id
 *      id     the id of the node
 *
 *             sets the node metric for a given id param:id
 */
void change_node_metric(int id, int metric);

/**
 * \brief      Setter for node parent
 * \param
 *      parent the parent of the node with id param:id
 *      id     the id of the node
 *
 *             sets the node new parent for a given id param:id
 */
void change_node_parent(int id, int new_parent);

/**
 * \brief      starts the bfs process

 *
 *             Starts a breadth first search
 */
void tree_bfs();

/**
 * \brief      bfs process
 * \param
 *      v      The vertex to begin the bfs from
 *
 *             Starts a breadth first search from this node (used by tree_bfs() should not be called otherwise
 */
void bfs(struct Node * v);

/**
 * \brief      Getter for tree root node
 * \return
 *      root   The root node of the tree
 *
 *             Returns the root node of the tree
 */
struct Node * get_root();

/**
 * \brief      Clears the tree of all nodes
 * \return
 *      root   The root node of the tree
 *
 *             Clears the tree of all nodes and free()'s the space created for the node structs
 */
void clear_tree();

#endif
