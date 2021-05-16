/*
 * Authors: Benjamin Medoff, Jarrett Parker, Stephen Galucy
 * 
 * Header file for our linked list implementation
 * 
 */



//Struct for the array of page table entries (only nextLevel value is set unless we are at the last level, rest of values are NULL)
typedef struct pageTableArray {
    int valid; //Valid bit (0 for page fault 1 for page hit)
    void *nextLevel; //Pointer to the next page table level (for multiple levels only)
    int age;
    int ppn;
    int vpn;
} ptArray;

typedef struct DRAM {
    int address;
} dramArray;



 typedef struct listNode {
     struct listNode *next;
     struct listNode *prev;
     ptArray *array;
 } lnkNode;

typedef struct linkedList {
     int size;
     lnkNode *first;
     lnkNode *last;
} lnkList;

//Struct for the entire page table
typedef struct pageTable {
    ptArray* lvlOne;
    lnkList* lvlTwo;
    lnkList* lvlThree;
} pt;

 lnkList *lnk_init();

 void *lnk_set(lnkList *list, ptArray array, int pos);
 void *lnk_get();

 int lnk_add(lnkList *list, ptArray array, int pos);
 void lnk_destroy(lnkList *list);