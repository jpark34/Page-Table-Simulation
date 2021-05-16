/*
 * Authors: Benjamin Medoff, Jarrett Parker, Stephen Galucy
 * 
 * Contains functions for a linked list built for the needs of a page table
 *
 * TODO: Need to implement:
 *      init, set, get, add, find node, destroy
 */

#include <stdio.h>
#include <stdlib.h>
#include "linklist.h"

static lnkNode *lnk_initNode(ptArray array);
//static lnkNode *lnk_findNode();

//Initialize the page table linked list (based on gll)
lnkList *lnk_init() {
    lnkList *list = (lnkList *) malloc(sizeof(lnkList));
    list->size = 0;
    list->first = NULL;
    list->last = NULL;
    return list;
}

void *lnk_set(lnkList *list, ptArray array, int pos) {

}

void *lnk_get() {

}

/* May or may not need
static lnkNode *lnk_findNode() {

}*/

//Initialize node information for each linked list node
static lnkNode *lnk_initNode(ptArray array) {
    lnkNode *node = (lnkNode *) malloc(sizeof(lnkNode));
    node->array = &array;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

//Add a node to the linked list
int lnk_add(lnkList *list, ptArray array, int pos) {
    if (pos > list->size) return -1;
    
    lnkNode *currNode;
    lnkNode *newNode;

    newNode = lnk_initNode(array);
    
    //Check if there are no nodes present
    if (list->size == 0) {
        list->first = newNode;
        list->last = newNode;
        list->size++;
        return 0;
    }
    
    //Add node to end of list
    list->last->next = newNode;
    newNode->prev = list->last;
    list->last = newNode;
    list->size++;
    return 0;
}

//Free the list
void lnk_destroy(lnkList *list) {
    lnkNode *currNode = list->first;
    lnkNode *nextNode;

    while(currNode != NULL) {
        nextNode = currNode->next;
        free(currNode);
        currNode = nextNode;
    }
    free(list);
}