/**
 * \file
 *         Source file for the tree methods and bread first search methods to be used in the tree
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */


#include <stdio.h>
#include <stdlib.h>

#include "tree.h"
#include "node.h"
#include "queue.h"


/*
USAGE

initialize the tree with tree_init()
add nodes repeatedly using add_node(n)

then run bfs_tree(v) to do a bfs to get a supporting children count

*/




void tree_free()
{
    int k = 0;
    for(k = 0; k <128; k++)
    {
        if(nodes[k] != NULL)
        {
            free(nodes[k]);
        }

    }
}

void init_visited()
{
    int k = 0;
    for(k = 0; k <128; k++)
    {
        visited[k] = 0;
    }
}

void bfs(struct Node * v)
{
	queue_init();
	queue_push(v);
	bfs_count = bfs_count + 1;
	visited[v->id] = bfs_count;
	int queue_size = queue_getSize();
	while(queue_size > 0)
	{
		//get all children incident to parent
		int kids;
		kids = 0;
		struct Item * dq = queue_dequeue();
		struct Node *c = dq->node->firstchild;
		while(c != NULL)
		{
			if(visited[c->id] == 0);
			{
				bfs_count = bfs_count + 1;
				visited[c->id] = bfs_count;
				queue_push(c);
				kids++;
			}
			struct Node *tmp = c;
			c = tmp->nextsibling;
		}
		printf("Number of kids for Node %d is %d\n",dq->node->id,kids);
		calculated[dq->node->id] = kids;
		queue_size = queue_getSize();
	}
}

void tree_bfs(struct Node * v)
{
	bfs_count = 0;
	init_visited();
	int k;
	for(k = 0; k < 128; k++)
	{
		if(nodes[k] != NULL)
		{
			if(visited[k] == 0)
			{			
				bfs(nodes[k]);
			}
		}
	}
	
	
}

void tree_init()
{
    tree_free();
    root = NULL;
    int k = 0;
    for(k = 0; k <128; k++)
    {
        //created[k] = 0;
	advertised[k] = -1;
	calculated[k] = -1;
        nodes[k] = NULL;
    }
    struct Node *r;
    r = (struct Node *)malloc(sizeof(struct Node));
    r->id = 0;
    r->metric = -1;
    r->firstchild = NULL;
    r->nextsibling = NULL;
    root = r;
    nodes[0] = r;
    init_visited();

}

void add_node(int parent, int metric, int id)
{
    struct Node *n;
    n = (struct Node *)malloc(sizeof(struct Node));
    n->id = id;
    n->metric = metric;
    //printf("------ adding node %d ------\n", id);
    nodes[id] = n;
    if(nodes[parent] == NULL)
    {
        //create parent placeholder
        struct Node *p;
        p = (struct Node *)malloc(sizeof(struct Node));
	p->id = parent;
	p->metric = -1;
	p->firstchild = n;
	p->nextsibling = NULL;
	//{parent,-1, n, NULL}; //add child to parent as well
        nodes[parent] = p;
	//printf("added to parent placeholder node:%d parent:%d \n", id, parent);
    }
    else
    {
        if(nodes[parent]->firstchild == NULL)
        {
            //add to first child
            nodes[parent]->firstchild = n;
	    //printf("added to first child node:%d parent:%d \n", id, parent);
        }
        else
        {
            //add to child's next sibling
            struct Node *child = nodes[parent]->firstchild;
           
            while(child->nextsibling != NULL)
            {
                child = child->nextsibling;
            }
            child->nextsibling = n;
	    //printf("added to next sibling node:%d parent:%d \n", id, parent);
            
        }
    }
    
}

struct Node * get_root()
{
	return root;
}



void change_node_metric(int id, int metric)
{
    if(nodes[id] != NULL)
    {
        nodes[id]->metric = metric;
    }
}

void change_node_parent(int id, int new_parent)
{
	//TODO
}



