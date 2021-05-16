/* 
 * Authors: Benjamin Medoff, Jarrett Parker, Stephen Galucy
 * TLB - TLB acts like a LRU cache for the page table
 */

#include "tlb.h"

systemParameters sysParams;

//array holding tlb entries
tlb_entry *tlb;

//size of the page offset in a virtual address
int tlbOffsetSize;

void tlbInit(systemParameters params){
    sysParams = params;
    tlb = (tlb_entry*) malloc(sizeof(tlb_entry)*sysParams.TLB_size_in_entries);
    
    tlbOffsetSize = (sysParams.virtual_addr_size_in_bits-(sysParams.N1_in_bits +
     sysParams.N2_in_bits + sysParams.N3_in_bits));

    //make sure each entry starts as invalid
    for (int i = 0; i < params.TLB_size_in_entries; i++){
        tlb[i].valid = 0;
        tlb[i].age = 0;
    }
}

void tlbFlush(systemParameters params){
    for(int i = 0; i < params.TLB_size_in_entries; i++){
        tlb[i].valid = 0;
        tlb[i].age = 0;
    }
}

//search tlb for the provided address, returns 1 on hit, 0 on miss
int tlbRead(int virtualAddress, int *physicalAddress){
    *physicalAddress = -1;
    for(int i = 0; i < sysParams.TLB_size_in_entries; i++){

        if(tlb[i].valid && tlb[i].vpn == virtualAddress >> tlbOffsetSize){
            *physicalAddress = (tlb[i].ppn << tlbOffsetSize) + (virtualAddress | ((1 << tlbOffsetSize) - 1));
            tlb[i].age = 0;
            //note we do not return here because we need to increment every entries age
        }
        tlb[i].age += 1;
    }
    if(*physicalAddress == -1)
        return 0;
    return 1;
}

//fetch the physical address from memory and put it in the tlb, returns the index the address was put in or -1 on fail.
int tlbFetch(int virtualAddress, PCBNode* process){
    //linear search through tlb to find LRU entry
    int oldestIndex = 0;
    for(int i = 1; i < sysParams.TLB_size_in_entries; i++){
        if(tlb[i].age > tlb[oldestIndex].age){
            oldestIndex = i;
        }
    }

    int physicalAddress;
    //TODO - replace with actual ptread
    int hit = ptRead(process, virtualAddress, &physicalAddress); //may need to check the value ptRead returns
    if(hit == 0){
        return -1;
    }
    tlb[oldestIndex].age = 0;
    tlb[oldestIndex].valid = 1;
    tlb[oldestIndex].ppn = physicalAddress;
    tlb[oldestIndex].vpn = virtualAddress >> tlbOffsetSize;
    return oldestIndex;
}

// free all the memory that was allocated for the TLB
void tlbDestroy() {
    free(tlb);
}
