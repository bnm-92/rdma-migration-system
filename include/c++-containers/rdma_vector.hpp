#ifndef __RDMA_VECTOR_HPP__
#define __RDMA_VECTOR_HPP__

#include <vector>
#include <utility>

#include "rdma-network/rdma_server_prototype.hpp"
#include "distributed-allocator/mempool.hpp"
#include "c++-containers/rdma_container_base.hpp"

template <class T>
class RDMAVector : public RDMAContainerBase<T> {

public:
    // Typedef for our underlying custom allocated STL container.
    typedef std::vector<T, typename RDMAContainerBase<T>::ScopedAllocT> ContT;
    // Non-scoped-allocator version of the container for nesting performance.
    typedef std::vector<T, typename RDMAContainerBase<T>::AllocT> UnscopedContT;


    // Constructor.
    // Makes an empty, invalid RDMA vector for later instantiation.
    // Note: This also creates a default PoolBasedAllocator (which is
    // more or less the same as an std::allocator) which will get wiped away
    // upon instantiation.
    RDMAVector(RDMAMemoryManager* manager)
    : RDMAContainerBase<T>(manager) {}

    // Instantiator.
    // Usage: Instead of calling
    //      std::vector my_vec(args...);
    // call
    //      RDMAVector my_vec(mempool_factory);
    //      my_vec.instantiate(args...);
    
    template <class ...Args>
    void instantiate(Args&& ...args) {
        // Initialize the allocator.
        this->set_up_allocator();
        // Allocate space for the vector from the allocator.
        // TODO: make sure this rebind is correct -.-
        using rebound_alloc_t =
            typename RDMAContainerBase<T>::ScopedAllocT::template rebind<ContT>::other;
        auto rebound = rebound_alloc_t(this->alloc);
        vec = static_cast<ContT*>(rebound.allocate(1));
        // Now construct the vector :)
        rebound.construct(vec, std::forward<Args>(args)...);
    }

    //TODO: pack in the vector address
    template <class ...Args>
    void remote_instantiate(RDMAMemory* rdma_memory, Args&& ...args) {
        // Initialize the allocator.
        this->rdma_memory = rdma_memory;
        void* memory = rdma_memory->vaddr;
        MemoryPool** pool_ = (MemoryPool**)memory;
        *pool_ = (MemoryPool*) ((char*)memory + sizeof(MemoryPool**));
        this->mempool = *pool_;

        this->alloc = PoolBasedAllocator<T>(&(*pool_));
        void** container_address_ = (void**)((char*)memory + sizeof(MemoryPool**) + sizeof(MemoryPool));
        this->set_container_address(*container_address_);
    }

    // Special constructors: copy constructor, etc.
    // Plus swap and other non-member functions?
    // TODO. For now, disable copy constructor.
    RDMAVector(const RDMAVector&) = delete;
    RDMAVector& operator=(const RDMAVector&) = delete;

    // Methods (mostly just forwarding):
    typename ContT::iterator begin() noexcept { return vec->begin(); }
    typename ContT::const_iterator begin() const noexcept { return vec->begin(); }
    typename ContT::iterator end() noexcept { return vec->end(); }
    typename ContT::const_iterator end() const noexcept { return vec->end(); }
    typename ContT::size_type size() const noexcept { return vec->size(); }
    bool empty() const noexcept { return vec->empty(); }
    typename ContT::reference operator[](typename ContT::size_type n) { return vec->operator[](n); }
    typename ContT::reference at(typename ContT::size_type n) { return vec->at(n); }
    typename ContT::const_reference at(typename ContT::size_type n) const { return vec->at(n); }
    typename ContT::reference front(typename ContT::size_type n) { return vec->front(n); }
    typename ContT::const_reference front(typename ContT::size_type n) const { return vec->front(n); }
    typename ContT::reference back(typename ContT::size_type n) { return vec->back(n); }
    typename ContT::const_reference back(typename ContT::size_type n) const { return vec->back(n); }
    void push_back(const typename ContT::value_type& val) { return vec->push_back(val); }
    void pop_back() { return vec->pop_back(); }
    void reserve(typename ContT::size_type new_cap) { return vec->reserve(new_cap); }

protected:
    ContT* vec;

protected:
    void set_container_address(void* container_addr) {
        
    LogInfo("Setting container address to %p", container_addr);

        vec = (ContT*) container_addr;
    }
    void* get_container_address() {

        LogInfo("Retrieving container address: %p", (void*)vec);
        return (void*) vec;
    }

};

#endif // __RDMA_VECTOR_HPP__
