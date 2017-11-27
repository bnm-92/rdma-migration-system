#include <iostream>
#include <csignal>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <limits.h>    /* for PAGESIZE */
#include <cstring>

#include "RDMAMemory.hpp"

using namespace std;

RDMAMemoryManager* manager; // this will be initialized by the started program
struct sigaction act;

void sigsegv_advance(int signum, siginfo_t *info_, void* ptr) { 
    // memory location of the fault
    void* addr = info_->si_addr;
    RDMAMemory* memory = manager->getRDMAMemory(addr);
    
    fprintf(stderr, "got sigsegv at %p\n", p1);
    
    //do something with this

    void * addr = (void*) ((uintptr_t)p & ~(PAGESIZE - 1));
    if(mprotect(addr, 1024, PROT_READ | PROT_WRITE)) {
        perror("couldnt mprotect3");
        exit(errno);
    }


}



