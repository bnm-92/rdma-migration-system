#include <iostream>
#include <csignal>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <limits.h>    /* for PAGESIZE */
#include <cstring>
#include <atomic>

#include "RDMAMemory.hpp"

using namespace std;

#ifndef __PAGING_HPP
#define __PAGING_HPP

enum class PageState{
    Remote,
    InFlight,
    Local
};

class Page{
public:
    Page() : ps(PageState::Remote) {};
    ~Page(){};
    Page(const Page &p) {
        ps.store(p.ps);
    };
    std::atomic<PageState> ps;
};

class Pages{
    public:
        Pages(uintptr_t start_address, size_t memory_size, size_t page_size);
        ~Pages();

        void* getPageAddress(int page_id);
        void* getPageAddress(void* address);
        
        size_t getPageSize(int page_id);
        size_t getPageSize(void* address);

        void setPageState(int page_id, PageState state);
        void setPageState(void* address, PageState state);
        bool setPageStateCAS(void* address, PageState old_state, PageState new_state);

        PageState getPageState(int page_id);
        PageState getPageState(void* address);

        void setPageSize(size_t page_size);

        vector<Page> pages;
    private:
        uintptr_t start_address;
        uintptr_t end_address;
        size_t memory_size;
        size_t page_size;
        int num_pages;
        std::atomic<int> local_pages;
};

#include <paging.tpp>

#endif //__PAGING_HPP