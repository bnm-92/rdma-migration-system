// paging.tpp

inline
Pages::Pages(uintptr_t start_address, size_t memory_size, size_t page_size) : local_pages(0) {
    this->start_address = start_address;
    this->memory_size = memory_size;
    this->end_address = (uintptr_t)((char*) start_address + memory_size);
    this->page_size = page_size;

    this->num_pages = (memory_size % page_size == 0) ? memory_size/page_size : memory_size/page_size + 1;

    this->pages = vector<Page>(num_pages);
}

inline
Pages::~Pages() {}

inline
void* Pages::getPageAddress(int page_id){
    size_t offset = page_size * page_id;
    return ((char*)start_address + offset); 
}

inline
void* Pages::getPageAddress(void* address){
    return (void*) ((uintptr_t)address & ~(page_size - 1));
}

inline
size_t Pages::getPageSize(int page_id) {
    if(page_id < (num_pages - 1))
        return page_size;
    return (memory_size % page_size == 0)? page_size : memory_size - (page_size * (num_pages - 1));
}

inline
size_t Pages::getPageSize(void* address) {
    uintptr_t end_of_page = (uintptr_t) ((char*)address + page_size);
    if(end_of_page > end_address )
        return (end_of_page - end_address);
    return page_size;
}

inline
void Pages::setPageState(int page_id, Page::PageState state){
    pages.at(page_id).pagestate = state;
    if(state == Page::PageState::Local)
        local_pages++;
}

inline
void Pages::setPageState(void* address, Page::PageState state){
    int page_id = ((uintptr_t)address - start_address)/page_size;
    pages.at(page_id).pagestate = state;
    local_pages++;
}

inline
Page::PageState Pages::getPageState(int page_id){
    return pages.at(page_id).pagestate;
}

inline
Page::PageState Pages::getPageState(void* address){
    int page_id = ((uintptr_t)address - start_address)/page_size;
    return pages.at(page_id).pagestate;
}
