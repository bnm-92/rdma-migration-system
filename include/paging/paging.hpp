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

struct sigaction act;
static RDMAMemoryManager* manager = nullptr;

void sigsegv_advance(int signum, siginfo_t *info_, void* ptr) { 
    if(manager == nullptr) {
        LogError("Manager is null");
        perror("Manager not declared for static access in sigsegv fault handler");
        exit(errno);
    }

    // memory location of the fault
    void* addr = info_->si_addr;
    RDMAMemory* memory = manager->getRDMAMemory(addr);
    size_t page_size = memory->page_size; 
    
    //do something with this
    addr = (void*) ((uintptr_t)addr & ~(page_size - 1));
    if(mprotect(addr, page_size, PROT_READ | PROT_WRITE)) {
        perror("couldnt mprotect3");
        exit(errno);
    }

    manager->Pull(addr, page_size, memory->pair);

}



