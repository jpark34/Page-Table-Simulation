#include "simulator.h"
#include <string.h>

void init()
{
    current_time = 0;
    nextQuanta = current_time + quantum;
    readyProcess = gll_init();
    runningProcess= gll_init();
    blockedProcess = gll_init();
    diskInterrupts = gll_init();

    processList = gll_init();
    traceptr = openTrace(traceFileName);

    sysParam = readSysParam(traceptr);

    //read traces from trace file and put them in the processList
    struct PCB* temp = readNextTrace(traceptr);
    if(temp == NULL)
    {
        printf("No data in file. Exit.\n");
        exit(1);
    }

    while(temp != NULL)
    {
        ptInit(temp, *sysParam);
        gll_pushBack(processList, temp);
        temp = readNextTrace(traceptr);
    }

    //transfer ready processes from processList to readyProcess list
    temp = gll_first(processList);
    
    while((temp!= NULL) && ( temp->start_time <= current_time))
    {
        struct NextMem* tempAddr;
        temp->memoryFile = openTrace(temp->memoryFilename);
        temp->numOfIns = readNumIns(temp->memoryFile);
        tempAddr = readNextMem(temp->memoryFile);
        while(tempAddr!= NULL)
        {
            gll_pushBack(temp->memReq, tempAddr);
            tempAddr = readNextMem(temp->memoryFile);
        }
        numProcs++;
        gll_pushBack(readyProcess, temp);
        gll_pop(processList);

        temp = gll_first(processList);
    }

    tlbInit( *sysParam );
    dramInit();
    
}

void finishAll()
{
    if((gll_first(readyProcess)!= NULL) || (gll_first(runningProcess)!= NULL) || (gll_first(blockedProcess)!= NULL) || (gll_first(processList)!= NULL))
    {
        printf("Something is still pending\n");
    }
    gll_destroy(readyProcess);
    gll_destroy(runningProcess);
    gll_destroy(blockedProcess);
    gll_destroy(processList);

//TODO: Anything else you want to destroy

    // Add in destroy for TLB, PT, LL
    tlbDestroy();
    //destroyPT(); Need to destroy in every process
    // TODO: Need to send this function a list
    //lnk_destroy();

    closeTrace(traceptr);
}

void statsinit()
{
    // statsList = gll_init();
    resultStats.perProcessStats = gll_init();
    resultStats.executionOrder = gll_init();
    resultStats.start_time = current_time;
    
}

void statsUpdate()
{
    resultStats.OSModetime = OSTime;
    resultStats.userModeTime  = userTime;   
    resultStats.numberOfContextSwitch = numberContextSwitch;
    resultStats.end_time = current_time;
}

//returns 1 on success, 0 if trace ends, -1 if page fault
int readPage(struct PCB* p, uint64_t stopTime)
{
    struct NextMem* addr = gll_first(p->memReq);
    uint64_t timeAvailable = stopTime - current_time;
    
    if(addr == NULL)
    {
        return 0;
    }
    if(debug == 1)
    {
        printf("Request::%s::%s::\n", addr->type, addr->address);
    }

    if(strcmp(addr->type, "NONMEM") == 0)
    {
        uint64_t timeNeeded = (p->fracLeft > 0)? p->fracLeft: sysParam->non_mem_inst_length;
    
        if(timeAvailable < timeNeeded)
        {
            current_time += timeAvailable;
            userTime += timeAvailable;
            p->user_time += timeAvailable;
            p->fracLeft = timeNeeded - timeAvailable;
        }
        else{
            gll_pop(p->memReq);
            current_time += timeNeeded; 
            userTime += timeNeeded;
            p->user_time += timeNeeded;
            p->fracLeft = 0;
        }

        if(gll_first(p->memReq) == NULL)
        {
            return 0;
        }
        return 1;
    }
    else{
        //convert the address to an int
        int address = (int)strtol(addr->address, NULL, 0);
        int physicalAddress;

        //if we do not have enough time for a TLB check, just skip
        if(timeAvailable < sysParam->TLB_latency){
            current_time += timeAvailable;
            userTime += timeAvailable;
            p->user_time += timeAvailable;
            return 1;
        }
    
        int hit = tlbRead(address, &physicalAddress);
        if(!hit){
            //Check if we have time for DRAM
            if(timeAvailable < sysParam->DRAM_latency+sysParam->TLB_latency){
                current_time += timeAvailable;
                userTime += timeAvailable;
                p->user_time += timeAvailable;
                return 1;
            }

            p->tlbMiss += 1;
            resultStats.totalTLBmiss += 1;

            //Page table
            hit = ptRead(p, address, &physicalAddress);

            current_time += sysParam->DRAM_latency + sysParam->TLB_latency;
            userTime += sysParam->DRAM_latency + sysParam->TLB_latency;
            p->user_time += sysParam->DRAM_latency + sysParam->TLB_latency;
            
            if(!hit){              
                current_time += sysParam->Page_fault_trap_handling_time;
                OSTime += sysParam->Page_fault_trap_handling_time;
                p->OS_time += sysParam->Page_fault_trap_handling_time + sysParam->Swap_interrupt_handling_time;
                
                if(gll_first(diskInterrupts) == NULL){
                    uint64_t nextInter = current_time + sysParam->Swap_latency;
                    gll_pushBack(diskInterrupts, (void*)nextInter);
                }
                else{
                    uint64_t nextInter = (uint64_t)(gll_last(diskInterrupts)) + sysParam->Swap_latency;
                    gll_pushBack(diskInterrupts, (void*)nextInter);
                }

                //TODO - clock here
                lru(p, address);
                tlbFetch(address, p);
                p->missCount += 1;
                resultStats.totalPgFaults += 1;
                return -1;
            }
            else{
                p->hitCount += 1;
                tlbFetch(address, p);
                gll_pop(p->memReq);
                return 1;
            }
        }
        else{
            p->hitCount += 1;
            p->tlbHits += 1;
            resultStats.totalTLBhit += 1;
            current_time += sysParam->TLB_latency;
            userTime += sysParam->TLB_latency;
            p->user_time += sysParam->TLB_latency;
            gll_pop(p->memReq);
        }

        return 1;

        //printf("Mem trace not handled\n");
        //exit(1);
    }
}

void schedulingRR(int pauseCause)
{
    //move first readyProcess to running
    gll_push(runningProcess, gll_first(readyProcess));
    gll_pop(readyProcess);

    if(gll_first(runningProcess) != NULL)
    {
        current_time = current_time + contextSwitchTime;
        OSTime += contextSwitchTime;
        numberContextSwitch++;
        struct PCB* temp = gll_first(runningProcess);
        temp->numContextSwitches += 1;
        gll_pushBack(resultStats.executionOrder, temp->name);
    }
}

/*runs a process. returns 0 if page fault, 1 if quanta finishes, -1 if traceFile ends, 2 if no running process, 4 if disk Interrupt*/
int processSimulator()
{
    uint64_t stopTime = nextQuanta;
    int stopCondition = 1;
    if(gll_first(runningProcess)!=NULL)
    {
        // if a disk interrupt is going to occur set stop time to interrupt

        long int nextDskInt = (long int)gll_first(diskInterrupts);
        if(nextDskInt != 0 && nextDskInt <= nextQuanta)
        {
            stopTime = nextDskInt;
            stopCondition = 4;
        }

        while(current_time < stopTime)
        {
            
            int read = readPage(gll_first(runningProcess), stopTime);
            if(debug == 1){
                printf("Read: %d\n", read);
                printf("Current Time %" PRIu64 ", Next Quanta Time %" PRIu64 " %" PRIu64 "\n",current_time, nextQuanta, stopTime);
            }
            if(read == 0)
            {
                return -1;
                break;
            }
            else if(read == -1) //page fault
            {
                if(gll_first(runningProcess) != NULL)
                {
                    gll_pushBack(blockedProcess, gll_first(runningProcess));
                    PCBNode* p = gll_pop(runningProcess);
                    tlbFlush(*sysParam);

                    p->blockTimeStart = current_time;
                    return 0;
                }
                
            }
        }
        if(debug == 1)
        {
            printf("Stop condition found\n");
            printf("Current Time %" PRIu64 ", Next Quanta Time %" PRIu64 "\n",current_time, nextQuanta);
        }
        return stopCondition;
    }
    if(debug == 1)
    {
        printf("No running process found\n");
    }
    return 2;
}

void cleanUpProcess(struct PCB *p)
{
    //struct PCB* temp = gll_first(runningProcess);
    struct PCB* temp = p;
   //TODO: Adjust the amount of available memory as this process is finishing
    
    struct Stats* s = malloc(sizeof(stats));
    s->processName = temp->name;
    s->hitCount = temp->hitCount;
    s->missCount = temp->missCount;
    s->user_time = temp->user_time;
    s->OS_time = temp->OS_time;
    s->numberOfPgFaults = temp->missCount;
    s->numberOfTLBmiss = temp->tlbMiss;
    s->numTLBHits = temp->tlbHits;
    s->blockedStateDuration = temp->blockedTime;
    s->numContextSwitches = temp->numContextSwitches;
    s->numDiskInterrupts = temp->numDiskInterrupts;

    s->duration = current_time - temp->start_time;
    
    gll_pushBack(resultStats.perProcessStats, s);
    //Free the page table for the process
    destroyPT(temp);
    gll_destroy(temp->memReq);
    closeTrace(temp->memoryFile);

}

void printPCB(void* v)
{
    struct PCB* p = v;
    if(p!=NULL){
        printf("%s, %" PRIu64 "\n", p->name, p->start_time);
    }
}

void printStats(void* v)
{
    struct Stats* s = v;
    if(s!=NULL){
        double hitRatio = s->hitCount / (1.0* s->hitCount + 1.0 * s->missCount);
        printf("\n\nProcess: %s: \nHit Ratio = %lf \tProcess completion time = %" PRIu64 
                "\tuser time = %" PRIu64 "\tOS time = %" PRIu64 "\n", s->processName, hitRatio, s->duration, s->user_time, s->OS_time) ;
        printf("Context Switches: %d \tDisk Interrupts: %d\n", s->numContextSwitches, s->numDiskInterrupts);
        printf("TLB Miss (Absolute): %d \tPage Faults (Absolute): %d \tFraction of Time in Blocked State: %f \n", s->numberOfTLBmiss, s->numberOfPgFaults, (float)(s->blockedStateDuration)/(float)(s->duration));
        printf("TLB Miss (Ratio): %f \tPage Faults (Ratio): %f \t", (float)(s->numberOfTLBmiss) / (float)(s->numberOfTLBmiss + s->numTLBHits), (float)(s->numberOfPgFaults) / (float)(s->hitCount + s->missCount));
    }
}

void printExecOrder(void* v)
{
    char* c = v;
    if(c!=NULL){
        printf("%s\n", c) ;
    }
}



void diskToMemory()
{
    // : Move requests from disk to memory
    gll_pop(diskInterrupts);
    current_time += sysParam->Swap_interrupt_handling_time;
    OSTime += sysParam->Swap_interrupt_handling_time;
    
    resultStats.numberOfDiskInt += 1;

    // : move appropriate blocked process to ready process
    struct PCB *proc = gll_first(blockedProcess);
    gll_pushBack(readyProcess, proc);
    PCBNode* p = gll_pop(blockedProcess);
    p->blockedTime += current_time - p->blockTimeStart;
    p->numDiskInterrupts += 1;
    resultStats.totalBlockedStateDuration += current_time - p->blockTimeStart;

    if(debug == 1)
    {
        printf("Done diskToMemory\n");
    }
}


void simulate()
{
    init();
    statsinit();

    //get the first ready process to running state
    struct PCB* temp = gll_first(readyProcess);
    gll_pushBack(runningProcess, temp);
    gll_pop(readyProcess);

    struct PCB* temp2 = gll_first(runningProcess);
    gll_pushBack(resultStats.executionOrder, temp2->name);

    while(1)
    {
        int simPause = processSimulator();
        if(current_time == nextQuanta)
        {
            nextQuanta = current_time + quantum;
        }

        //transfer ready processes from processList to readyProcess list
        struct PCB* temp = gll_first(processList);
        
        while((temp!= NULL) && ( temp->start_time <= current_time))
        {
            temp->memoryFile = openTrace(temp->memoryFilename);
            temp->numOfIns = readNumIns(temp->memoryFile);

            struct NextMem* tempAddr = readNextMem(temp->memoryFile);

	        while(tempAddr!= NULL)
            {
                gll_pushBack(temp->memReq, tempAddr);
                tempAddr = readNextMem(temp->memoryFile);
            }
            numProcs++;
            gll_pushBack(readyProcess, temp);
            gll_pop(processList);

            temp = gll_first(processList);
        }

        //move elements from disk to memory
        if(simPause == 4){
            diskToMemory();
            nextQuanta = current_time + quantum;
        }

        //on page fault current time will be out of sync 
        if(simPause == 0)
            nextQuanta = current_time + quantum;

        //This memory trace done
        if(simPause == -1)
        {
            //finish up this process
            cleanUpProcess(gll_first(runningProcess));
            gll_pop(runningProcess);

        }

        //move running process to readyProcess list
        int runningProcessNUll = 0;
        if(simPause == 1 || simPause == 4)
        {
            if(gll_first(runningProcess) != NULL)
            {
                gll_pushBack(readyProcess, gll_first(runningProcess));
                gll_pop(runningProcess);
            }
            else{
                runningProcessNUll = 1;
            }
            if(simPause == 1)
            {
                nextQuanta = current_time + quantum;
            }
        }

        schedulingRR(simPause);

        //Nothing in running or ready. need to increase time to next timestamp when a process becomes ready.
        if((gll_first(runningProcess) == NULL) && (gll_first(readyProcess) == NULL))
        {
            if(debug == 1)
            {
                printf("\nNothing in running or ready\n");
            }
            if((gll_first(blockedProcess) == NULL) && (gll_first(processList) == NULL))
            {

                    if(debug == 1)
                    {
                        printf("\nAll done\n");
                    }
                    break;
            }
            struct PCB* tempProcess = gll_first(processList);
            struct PCB* tempBlocked = gll_first(blockedProcess);

            //: Set correct value of timeOfNextPendingDiskInterrupt
            
            uint64_t timeOfNextPendingDiskInterrupt =(uint64_t)gll_first(diskInterrupts);

            if(tempBlocked == NULL)
            {
                if(debug == 1)
                {
                    printf("\nGoing to move from process list to ready\n");
                }
                struct NextMem* tempAddr;
                tempProcess->memoryFile = openTrace(tempProcess->memoryFilename);
                tempProcess->numOfIns = readNumIns(tempProcess->memoryFile);
                tempAddr = readNextMem(tempProcess->memoryFile);
                while(tempAddr!= NULL)
                {
                    gll_pushBack(tempProcess->memReq, tempAddr);
                    tempAddr = readNextMem(tempProcess->memoryFile);
                }
                gll_pushBack(readyProcess, tempProcess);
                gll_pop(processList);
                
                while(nextQuanta < tempProcess->start_time)
                {   
                    
                    current_time = nextQuanta;
                    nextQuanta = current_time + quantum;
                }
                OSTime += (tempProcess->start_time-current_time);
                current_time = tempProcess->start_time; 
            }
            else
            {
                if(tempProcess == NULL)
                {
                    if(debug == 1)
                    {
                        printf("\nGoing to move from blocked list to ready\n");
                    }
                    OSTime += (timeOfNextPendingDiskInterrupt-current_time);
                    current_time = timeOfNextPendingDiskInterrupt;
                    while (nextQuanta < current_time)
                    {
                        nextQuanta = nextQuanta + quantum;
                    }
                    diskToMemory();
                }
                else if(tempProcess->start_time >= timeOfNextPendingDiskInterrupt)
                {
                    if(debug == 1)
                    {
                        printf("\nGoing to move from blocked list to ready\n");
                    }
                    OSTime += (timeOfNextPendingDiskInterrupt-current_time);
                    current_time = timeOfNextPendingDiskInterrupt;
                    while (nextQuanta < current_time)
                    {
                        nextQuanta = nextQuanta + quantum;
                    }
                    diskToMemory();
                }
                else{
                    struct NextMem* tempAddr;
                    if(debug == 1)
                    {
                        printf("\nGoing to move from proess list to ready\n");
                    }
                    tempProcess->memoryFile = openTrace(tempProcess->memoryFilename);
                    tempProcess->numOfIns = readNumIns(tempProcess->memoryFile);
                    tempAddr = readNextMem(tempProcess->memoryFile);
                    while(tempAddr!= NULL)
                    {
                        gll_pushBack(tempProcess->memReq, tempAddr);
                        tempAddr = readNextMem(tempProcess->memoryFile);
                    }
                    gll_pushBack(readyProcess, tempProcess);
                    gll_pop(processList);
                    
                    while(nextQuanta < tempProcess->start_time)
                    {   
                        current_time = nextQuanta;
                        nextQuanta = current_time + quantum;
                    }
                    OSTime += (tempProcess->start_time-current_time);
                    current_time = tempProcess->start_time; 
                }
            }   
        }
    }
}

int main(int argc, char** argv)
{
    if(argc == 1)
    {
        printf("No file input\n");
        exit(1);
    }
    traceFileName = argv[1];
    outputFileName = argv[2];

    simulate();
    finishAll();
    statsUpdate();

    if(writeToFile(outputFileName, resultStats) == 0)
    {
        printf("Could not write output to file\n");
    }
    printf("User time = %" PRIu64 "\nOS time = %" PRIu64 "\n", resultStats.userModeTime, resultStats.OSModetime);
    printf("Context switched = %d\n", resultStats.numberOfContextSwitch);
    printf("Start time = 0\nEnd time = %lu\n", current_time);
    printf("Total Number of Disk Interrupts = %d\n", resultStats.numberOfDiskInt);
    printf("Total TLB Misses (Absolute) = %d\nTotal TLB Misses (Ratio) = %f\n", resultStats.totalTLBmiss, ((1.0 * resultStats.totalTLBmiss)/(1.0 * (resultStats.totalTLBhit + resultStats.totalTLBmiss))));
    printf("Total Page Faults (Absolute) = %d\nTotal Page Faults (Ratio) = %f\n", resultStats.totalPgFaults, (1.0 * resultStats.totalPgFaults) / (1.0 * resultStats.totalTLBmiss));
    printf("Total Fraction of Time in Blocked State = %f\n", (1.0 * ((float)(resultStats.totalBlockedStateDuration)/(float)(resultStats.end_time*numProcs))));
    gll_each(resultStats.perProcessStats, &printStats);

    // printf("\nExec Order:\n");
    // gll_each(resultStats.executionOrder, &printExecOrder);
    printf("\n");
}
