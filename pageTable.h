/*
 * Authors: Benjamin Medoff, Jarrett Parker, Stephen Galucy
 * 
 * Header file for our page table implementation.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "dataStructures.h"
#include <math.h>

int clock();
void ptInit(PCBNode* process, systemParameters sysParam);
void dramInit();
int ptRead(PCBNode* process, int virtualAddress, int* physicalAddress);
void addEntry(PCBNode* process, int virtualAddress);
void destroyPT(PCBNode* process);
void lru(PCBNode* process, int virtualAddress);
