/**
 * \file
 *         Source file for queue methods and handlers designed to be used in conjunction with libp.c
 * \author
 *         Lutando Ngqakaza <lutando.ngqakaza@gmail.com>
 */
#include <stdio.h>
#include <stdlib.h>

#include "queue.h"
#include "node.h"


void queue_init()
{
	elements = 0;
	while(head != NULL)
	{
		struct Item * tmp = head;
		head = head->next;
		free(tmp);
	}

	head = NULL;
	tail = (struct Item *)malloc(sizeof(struct Item));
}


void queue_push(struct Node * node)
{
	struct Item *i;
	i = (struct Item *)malloc(sizeof(struct Item));
	i->node = node;
	tail->next = i;
	tail = i;
	if(head == NULL)
	{
		head = i;
	}
	elements = elements + 1;
	
}

struct Item * queue_dequeue()
{
	struct Item * tmp = head;
	printf("dequeing head %d\n",head->node->id);
	head = head->next;
	elements = elements - 1;
	return tmp;
	
}

int queue_getSize()
{
	return elements;
}

int queue_empty()
{
	if(elements < 1)
	{
		return 1;
	}
	return 0;
}

struct Item * get_head()
{
	return head;
}

struct Item * get_tail()
{
	return tail;
}
