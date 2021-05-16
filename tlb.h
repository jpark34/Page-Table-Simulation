/* 
 * Authors: Benjamin Medoff, Jarrett Parker, Stephen Galucy
 * TLB - TLB acts like a LRU cache for the page table
 */
#include "dataStructures.h"
#include "pageTable.h"

typedef struct TLB_entry{
    int valid; // 1 for valid 0 for invalid
    int age;
    int vpn;
    int ppn;
} tlb_entry;

void tlbInit(systemParameters params);

void tlbFlush(systemParameters params);

int tlbRead(int virtualAddress, int *physicalAddress);

int tlbFetch(int virtualAddress, PCBNode* process);

void tlbDestroy();