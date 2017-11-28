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

#ifndef __PAGING_HPP
#define __PAGING_HPP

class Page{
    public:
        enum PageState{
                Remote,
                InFlight,
                Local
            } pagestate = PageState::Remote;
};

class Pages{
    public:
        Pages(uintptr_t start_address, size_t memory_size, size_t page_size);
        ~Pages();

        void* getPageAddress(int page_id);
        void* getPageAddress(void* address);
        
        size_t getPageSize(int page_id);
        size_t getPageSize(void* address);

        void setPageState(int page_id, Page::PageState state);
        void setPageState(void* address, Page::PageState state);

        Page::PageState getPageState(int page_id);
        Page::PageState getPageState(void* address);

        vector<Page> pages;
    private:
        uintptr_t start_address;
        uintptr_t end_address;
        size_t memory_size;
        size_t page_size;
        int num_pages;
        int local_pages;
};

#include <paging.tpp>

#endif //__PAGING_HPP