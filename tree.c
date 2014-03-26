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
		
	    //printf("freed node %d\n", nodes[k]->id);
	
            free(nodes[k]);
	    nodes[k] = NULL;
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
		int id = queue_dequeue();
		struct Node *c = nodes[id]->firstchild;
		
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
		//printf("Number of kids for Node %d is %d\n",id,kids);
		calculated[id] = kids;
		queue_size = queue_getSize();
		//free(dq);
	}
}

void tree_bfs()
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
    //tree_free();
    //free(root);
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

/*void add_node(int parent, int metric, int id)
{
    struct Node *n;
    n = (struct Node *)malloc(sizeof(struct Node));
    n->id = id;
    n->metric = metric;
    printf("------ adding node %d ------\n", id);
    nodes[id] = n;
    //n->firstchild = NULL;
    //n->nextsibling = NULL;
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
	printf("added to parent placeholder node:%d parent:%d \n", id, parent);
    }
    else
    {
        if(nodes[parent]->firstchild == NULL)
        {
            //add to first child
            nodes[parent]->firstchild = n;
	    printf("added to first child node:%d parent:%d \n", id, parent);
        }
        else
        {
	    printf("node %d has first child %d, adding to sibling\n",parent, nodes[parent]->firstchild->id);
            //add to child's next sibling
            struct Node *child = nodes[parent]->firstchild;
           printf("added to next sibling node:%d parent:%d \n", id, parent);
            while(child->nextsibling != NULL)
            {
		printf("sibling %d \n", child->id);
                child = child->nextsibling;
            }
            child->nextsibling = n;
	    
            
        }
    }
    
}*/

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

void dfs_clear(struct Node * n)
{
	int id = n->id;
	bfs_count = bfs_count + 1;
	visited[id] = bfs_count;
	struct Node * c = n->firstchild;
	while(c != NULL)
	{
		if(visited[c->id] == 0);
		{
			dfs_clear(c);
		}
		struct Node *tmp = c;
		c = tmp->nextsibling;
	}
	printf("free-ing %d \n",n->id);
	//n->firstchild = NULL;
	//n->nextsibling = NULL;
	free(n);
	nodes[id] = NULL;
	
}

void clear_tree()
{

	init_visited();
	bfs_count = 0; //actually for DFS but it doesnt matter
	int k;
	for(k = 0; k < 128; k++)
	{
		if(nodes[k] != NULL)
		{
			if(visited[k] == 0)
			{
				printf("%d\n",k);			
				dfs_clear(nodes[k]);
			}
		}
	}
	
	
	
}

void add_node(int parent, int metric, int id)
{
    struct Node *n;
    n = (struct Node *)malloc(sizeof(struct Node));
    n->id = id;
    n->metric = metric;
    //printf("------ adding node %d ------\n", id);
    nodes[id] = n;
    n->firstchild = NULL;
    n->nextsibling = NULL;
    if(!nodes[parent]) //if nodes[parent] == null or if nodes[parent] does not point to something
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
        if(!nodes[parent]->firstchild)
        {
            //add to first child
            nodes[parent]->firstchild = nodes[id];
	    //printf("added to first child node:%d parent:%d \n", id, parent);
        }
        else
        {
	    //printf("node %d has first child %d, adding to sibling\n",parent, nodes[parent]->firstchild->id);
            //add to child's next sibling
            struct Node *child = nodes[parent]->firstchild;
           //printf("added to next sibling node:%d parent:%d \n", id, parent);
            while(child->nextsibling)
            {
		//printf("sibling %d \n", child->id);
                child = child->nextsibling;
            }
            child->nextsibling = nodes[id];
	    
            
        }
    }
    
}

void print_nodes()
{
	int k;
	for(k = 0; k < 128; k++)
	{
		int fc, ns;
		if(nodes[k])
		{
			if(!nodes[k]->firstchild)
			{
				fc = -1;
			}
			else
			{
				fc = nodes[k]->firstchild->id;
			}
			if(!nodes[k]->nextsibling)
			{
				ns = -1;
			}
			else
			{
				ns = nodes[k]->nextsibling->id;
			}
			//printf("Node[%d] fc %d ns %d \n",k,fc,ns);
		}
		
	}
}



