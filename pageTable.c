/*
 * Authors: Benjamin Medoff, Jarrett Parker, Stephen Galucy
 * 
 * This file contains our multi level page table implementation.
 * 
 * TODO - init PPN for each node in ptInit
 *          require going through each node until we run out of dram (for loop to set each nodes page number)
 * TODO - make lru() update the oldest node's vpn
 *          implement addEntry into lru()
 */

#include "pageTable.h"

//Goals
struct SystemParameters parameters;
int offsetSize;
dramArray* dram;

// TODO: possibly need to add check for which algorithm is being run so that we only init that
//       algorithms specific DRAM
void ptInit(PCBNode* process, systemParameters sysParam) {
    //Create linked list for each level, except level one and create arrays in each level for the number of different tables
    //Initialize a single level page table
    parameters = sysParam;
    if (parameters.Num_pagetable_levels == 1) {
        int tableEntries = pow(2, parameters.N1_in_bits);
        process->pTable.lvlOne = (ptArray*) malloc(tableEntries*sizeof(ptArray));
        offsetSize = (parameters.virtual_addr_size_in_bits-(parameters.N1_in_bits +
                parameters.N2_in_bits + parameters.N3_in_bits));
        //Initialize entry data
        for(int i = 0; i < tableEntries; i++) {
            process->pTable.lvlOne[i].valid = 0;
            process->pTable.lvlOne[i].nextLevel = NULL;
            process->pTable.lvlOne[i].age = 0;
            process->pTable.lvlOne[i].ppn = 0;
            process->pTable.lvlOne[i].vpn = 0;
            
            int dramPointer = i*offsetSize;

            if(dramPointer > parameters.DRAM_size_in_MB) {
                process->pTable.lvlOne[i].ppn = dramPointer >> offsetSize;
            } 
            else {
                process->pTable.lvlOne[i].ppn = -1;
            }
        }
    }
    //Initialize two level page table
    if (parameters.Num_pagetable_levels == 2) {
        int levelOneEntries = pow(2, parameters.N1_in_bits);
        int levelTwoEntries = pow(2, parameters.N2_in_bits);
        process->pTable.lvlOne = (ptArray*) malloc(levelOneEntries*sizeof(ptArray));
        offsetSize = (parameters.virtual_addr_size_in_bits-(parameters.N1_in_bits +
                parameters.N2_in_bits + parameters.N3_in_bits));
        //We use an array for the first level then create a linked list for the second level, where each node will contain an array of entries
        process->pTable.lvlTwo = lnk_init();
        //Initialize level one entries
        for(int i = 0; i < levelOneEntries; i++) {
            process->pTable.lvlOne[i].valid = 0;
            process->pTable.lvlOne[i].nextLevel = NULL;
        }
        //Initialize level two enteries
        lnkNode* prevNode; 
        for(int i = 0; i < levelOneEntries; i++){
            lnkNode* newNode = (lnkNode*) malloc(sizeof(lnkNode));
            if(i != 0){
                newNode->prev = prevNode;
                prevNode->next = newNode;
            }
            else if (i == 0) {
                process->pTable.lvlTwo->first = newNode;
            }
            //initialize the array in each node
            newNode->array = (ptArray*) malloc(levelTwoEntries*sizeof(ptArray));
            for(int j = 0; j < levelTwoEntries; j++) {
                newNode->array[j].valid = 0;
                newNode->array[j].nextLevel = NULL;
                newNode->array[j].age = 0;
                newNode->array[j].ppn = 0;
                newNode->array[j].vpn = 0;

                int dramPointer = i*offsetSize;

                if(dramPointer > parameters.DRAM_size_in_MB) {
                    process->pTable.lvlOne[i].ppn = dramPointer >> offsetSize;
                } 
                else {
                    process->pTable.lvlOne[i].ppn = -1;
                }
            }
            prevNode = newNode;   
        }
        // Link the first level to the second level
        // May need to update later to link more than the first couple nodes
        //      ( change k < 3 to k < N1_in_bits )
        lnkNode* currNode;
        currNode = process->pTable.lvlTwo->first;
        for ( int k = 0; k < 3; k++ ) {
            process->pTable.lvlOne[k].nextLevel = currNode;
            currNode = currNode->next;
        }
       
    }
    //Initialize a three level page table
    if (parameters.Num_pagetable_levels == 3) {
        int levelOneEntries = pow(2, parameters.N1_in_bits);
        int levelTwoEntries = pow(2, parameters.N2_in_bits);
        int levelThreeEntries = pow(2, parameters.N3_in_bits);
        process->pTable.lvlOne = (ptArray*) malloc(levelOneEntries*sizeof(ptArray));
        offsetSize = (parameters.virtual_addr_size_in_bits-(parameters.N1_in_bits +
        parameters.N2_in_bits + parameters.N3_in_bits));
        //Same as two level, but adding another linked list for the third level
        process->pTable.lvlTwo = lnk_init();
        process->pTable.lvlThree = lnk_init();
        //Initialize level one entries
        for(int i = 0; i < levelOneEntries; i++) {
            process->pTable.lvlOne[i].valid = 0;
            process->pTable.lvlOne[i].nextLevel = NULL;
        }

        //Init level two entries
        lnkNode* prevNode; 
        for(int i = 0; i < levelOneEntries; i++){
            lnkNode* newNode = (lnkNode*) malloc(sizeof(lnkNode));
            if(i != 0){
                newNode->prev = prevNode;
                prevNode->next = newNode;
            }
            else if (i == 0) {
                process->pTable.lvlTwo->first = newNode;
            }
            //initialize the array in each node
            newNode->array = (ptArray*) malloc(levelTwoEntries*sizeof(ptArray));
            for(int j = 0; j < levelTwoEntries; j++) {
                newNode->array[j].valid = 0;
                newNode->array[j].nextLevel = NULL;
            }
            prevNode = newNode;
      
        }

        //Init level three entries
        lnkNode* prev3Node; 
        for(int i = 0; i < levelTwoEntries; i++){
            lnkNode* newNode = malloc(sizeof(lnkNode*));
            if(i != 0){
                newNode->prev = prevNode;
                prevNode->next = newNode;
            }
            else if (i == 0) {
                process->pTable.lvlThree->first = newNode;
            }
            //initialize the array in each node
            newNode->array = (ptArray*) malloc(levelThreeEntries*sizeof(ptArray));
            for(int j = 0; j < levelThreeEntries; j++) {
                newNode->array[j].valid = 0;
                newNode->array[j].nextLevel = NULL;
                newNode->array[j].age = 0;
                newNode->array[j].ppn = 0;
                newNode->array[j].vpn = 0;

                int dramPointer = i*offsetSize;

                if(dramPointer > parameters.DRAM_size_in_MB) {
                    process->pTable.lvlOne[i].ppn = dramPointer >> offsetSize;
                } 
                else {
                    process->pTable.lvlOne[i].ppn = -1;
                }
            }
            prevNode = newNode;
        }

        //Link the levels of the page table together
        //May need to change to include all entries of the PT
        //      ( change k < 3 to k < N1_in_bits )
        lnkNode* curr2Node;
        lnkNode* curr3Node;
        curr2Node = process->pTable.lvlTwo->first;
        for ( int k = 0; k < 3; k++ ) {
            process->pTable.lvlOne[k].nextLevel = curr2Node;
            curr2Node = curr2Node->next;
        }
        //Link the second level with the third
        curr2Node = process->pTable.lvlTwo->first;
        curr3Node = process->pTable.lvlThree->first;
        for (int k = 0; k < 3; k++) {
            for (int q = 0; q < 3; q++) {
                curr2Node->array[q].nextLevel = curr3Node;
                curr3Node = curr3Node->next;
            }
            curr2Node = curr2Node->next;
        }
    }
}

// Takes a virtual address and transforms it into a physical address
/*int *mmu(int virtualAddress) {
    int *physicalAddress;

    

    return physicalAddress;
}*/

void dramInit() {
    int dramEntries;
    dramEntries = ((pow(2, 20) * parameters.DRAM_size_in_MB)/parameters.virtual_addr_size_in_bits);
    dram = (dramArray*) malloc(dramEntries*sizeof(dramArray));
    for (int i = 0; i < dramEntries; i++) {
        dram[i].address = NULL;
    }
}

void lru(PCBNode* process, int virtualAddress) {
    // TODO: will need to update loops to go through entire page table once we set everything
    int oldestEntry;
    int physicalAddress;
    if (parameters.Num_pagetable_levels == 1) {
        for (int i = 0; i < (parameters.DRAM_size_in_MB*1000000)/(1 << offsetSize); i++) {
            if (i == 0) {
                oldestEntry = i;
            }
            else {
                if (process->pTable.lvlOne[oldestEntry].age < process->pTable.lvlOne[i].age) {
                    oldestEntry = i;
                }
            }
        }
        process->pTable.lvlOne[oldestEntry].age = 0;
        process->pTable.lvlOne[oldestEntry].valid = 1;
        process->pTable.lvlOne[oldestEntry].vpn = virtualAddress >> offsetSize;
        process->pTable.lvlOne[oldestEntry].ppn = physicalAddress;        
    }

    else if ( parameters.Num_pagetable_levels == 2 ) {
        lnkNode* curr2Node;
        curr2Node = process->pTable.lvlTwo->first;
        lnkNode* oldestNode = curr2Node;
        for (int i = 0; i < 3; i++) {
            for (int y = 0; y < pow(2, parameters.N2_in_bits); y++) {
                if (y == 0 && (curr2Node == process->pTable.lvlTwo->first)) {
                    oldestEntry = y;
                }
                else {
                    if (curr2Node->array[oldestEntry].age < curr2Node->array[y].age) {
                        oldestEntry = y;
                        oldestNode = curr2Node;
                    }
                }
            }
            curr2Node = curr2Node->next;
        }
        oldestNode->array[oldestEntry].age = 0;
        oldestNode->array[oldestEntry].valid = 1;
        oldestNode->array[oldestEntry].vpn = virtualAddress >> offsetSize;
        oldestNode->array[oldestEntry].ppn = physicalAddress;  
    }

    else if ( parameters.Num_pagetable_levels == 3 ) {
        lnkNode* curr3Node;
        curr3Node = process->pTable.lvlThree->first;
        lnkNode* oldestNode = curr3Node;
        for (int i = 0; i < 9; i++) {
            for (int y = 0; y < pow(2, parameters.N3_in_bits); y++) {
                if (y == 0 && (curr3Node == process->pTable.lvlThree->first)) {
                    oldestEntry = y;
                }
                else {
                    if (curr3Node->array[oldestEntry].age < curr3Node->array[y].age) {
                        oldestEntry = y;
                        oldestNode = curr3Node;
                    }
                }
            }
            curr3Node = curr3Node->next;
        }
        oldestNode->array[oldestEntry].age = 0;
        oldestNode->array[oldestEntry].valid = 1;
        oldestNode->array[oldestEntry].vpn = virtualAddress >> offsetSize;
        oldestNode->array[oldestEntry].ppn = physicalAddress; 
    }
}

int clock () {
    return 0;
}

//Modify this if we decide to use more than the first few tables for the second and third level
int ptRead(PCBNode* process, int virtualAddress, int* physicalAddress){
    *physicalAddress = -1;

    if (parameters.Num_pagetable_levels == 1) {
        for (int i = 0; i < (parameters.DRAM_size_in_MB*1000000)/(1 << offsetSize); i++){
            if (process->pTable.lvlOne[i].valid && (process->pTable.lvlOne[i].vpn  == virtualAddress >> offsetSize)) {
                    *physicalAddress = (process->pTable.lvlOne[i].ppn << offsetSize) + (virtualAddress | ((1 << offsetSize) -1));
                    process->pTable.lvlOne[i].age = 0;
                }
            process->pTable.lvlOne[i].age += 1;
        }
    }

    else if (parameters.Num_pagetable_levels == 2) {
        lnkNode *currNode;
        currNode = process->pTable.lvlTwo->first;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < pow(2, parameters.N2_in_bits); j++){
                if (currNode->array[j].valid && currNode->array[j].vpn == virtualAddress >> offsetSize) {
                    *physicalAddress = (currNode->array[j].ppn << offsetSize) + (virtualAddress | ((1 << offsetSize) -1));
                    currNode->array[j].age = 0;
                }
                currNode->array[j].age += 1;
            }
            currNode = currNode->next;
        }
    }
    
    else if (parameters.Num_pagetable_levels == 3) {
        lnkNode *currNode;
        currNode = process->pTable.lvlThree->first;
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < pow(2, parameters.N3_in_bits); j++){
                if (currNode->array[j].valid && currNode->array[j].vpn ==  virtualAddress >> offsetSize) {
                    *physicalAddress = (currNode->array[j].ppn << offsetSize) + (virtualAddress | ((1 << offsetSize) -1));
                    currNode->array[j].age = 0;
                }
                currNode->array[j].age += 1;
            }
            currNode = currNode->next;
        }
    }
    
    if (*physicalAddress == -1) {
        return 0;
    }
    return 1;
}

/*
void addEntry(PCBNode* process, int virtualAddress) {
    if (parameters.Num_pagetable_levels == 1) {
        for (int i = 0; i < pow(2, parameters.N1_in_bits); i++) {
            if (!process->pTable.lvlOne[i].valid) {
                process->pTable.lvlOne[i].valid = 1;
                process->pTable.lvlOne[i].vpn = virtualAddress >> offsetSize;
                //Add ppn
            }
        }
    }

    if (parameters.Num_pagetable_levels == 2) {
        lnkNode *currNode;
        currNode = process->pTable.lvlTwo->first;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < pow(2, parameters.N2_in_bits); j++) {
                if (!currNode->array[j].valid) {
                    currNode->array[j].valid = 1;
                    currNode->array[j].vpn = virtualAddress >> offsetSize;
                    //Add ppn
                }
            }
            currNode = currNode->next;
        }
    }

    if (parameters.Num_pagetable_levels == 3) {
        lnkNode *currNode;
        currNode = process->pTable.lvlThree->first;
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < pow(2, parameters.N3_in_bits); j++) {
                if (!currNode->array[j].valid) {
                    currNode->array[j].valid = 1;
                    currNode->array[j].vpn = virtualAddress >> offsetSize;
                    //Add ppn
                }
            }
            currNode = currNode->next;
        }
    }

}
*/

void destroyPT(PCBNode* process) {
    if (parameters.Num_pagetable_levels == 1){
        free(process->pTable.lvlOne);
    }
    else if (parameters.Num_pagetable_levels == 2) {
        free(process->pTable.lvlOne);
        lnk_destroy(process->pTable.lvlTwo);
    }

    else if (parameters.Num_pagetable_levels == 3) {
        free(process->pTable.lvlOne);
        lnk_destroy(process->pTable.lvlTwo);
        lnk_destroy(process->pTable.lvlThree);
    }
}
