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
    head = NULL;
    tail = NULL;
}


void queue_push(struct Node * node)
{
    struct Item *i;
    i = (struct Item *)malloc(sizeof(struct Item));
    i->node = node;
    i->next = NULL;

    if(tail == NULL)
    {
        tail = i;
    }
    else
    {

        //tail->next = i;
        tail->next = i;
        tail = i;
    }
    if(head == NULL)
    {
        head = i;
    }
    //printf("node %d pushed onto q\n",node->id);
    elements = elements + 1;

}

void print_queue()
{
    struct Item *tmp = head;
    while(tmp)
    {
        printf("-%d-\n",tmp->node->id);
        tmp = tmp->next;
    }
}

int queue_dequeue()
{
    //print_queue();
    struct Item *tmp = head;
    int ret;
    ret = head->node->id;

    head = head->next;
    free(tmp);
    elements = elements - 1;
    return ret;

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
