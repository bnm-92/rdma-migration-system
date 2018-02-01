#ifndef __RDMA_UNORDERED_MAP_HPP__
#define __RDMA_UNORDERED_MAP_HPP__

#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "rdma-network/rdma_server_prototype.hpp"

#include "distributed-allocator/mempool.hpp"
#include "c++-containers/rdma_container_base.hpp"
#include "utils/miscutils.hpp"

template <
    class Key,
    class T,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>
> class RDMAUnorderedMap : public RDMAContainerBase<std::pair<const Key, T> > {

    // Typedef for our allocation data type.
    typedef std::pair<const Key, T> ValueT;
    // Typedef for our underlying custom allocated STL container.
    typedef std::unordered_map<
        Key, T, Hash, KeyEqual, typename RDMAContainerBase<ValueT>::ScopedAllocT
    > ContT;
    // Non-scoped allocator version of the container.
    typedef std::unordered_map<
        Key, T, Hash, KeyEqual, typename RDMAContainerBase<ValueT>::AllocT
    > UnscopedContT;

protected:
    ContT* cont;

public:
    // Constructor.
    // Makes an empty, invalid RDMA unordered_map for later instantiation.
    // Note: This also creates a default PoolBasedAllocator (which is
    // more or less the same as an std::allocator) which will get wiped away
    // upon instantiation.
    RDMAUnorderedMap(RDMAMemoryManager* manager)
    : RDMAContainerBase<ValueT>(manager) {}

    // Instantiator.
    // Usage: Instead of calling
    //      std::unordered_map my_cont(args...);
    // call
    //      RDMAUnorderedMap my_cont(mempool_factory);
    //      my_cont.instantiate(args...);
    template <class ...Args>
    void instantiate(Args&& ...args) {
        //std::lock_guard<std::mutex> guard(cont_mutex);
        // Initialize the allocator.
        this->set_up_allocator();
        // Allocate space for the unordered_map from the allocator.
        // TODO: make sure this rebind is correct -.-
        using rebound_alloc_t =
            typename RDMAContainerBase<ValueT>::ScopedAllocT::template rebind<ContT>::other;
        auto rebound = rebound_alloc_t(this->alloc);
        cont = static_cast<ContT*>(rebound.allocate(1));
        // Now construct the unordered_map :)
        rebound.construct(cont, std::forward<Args>(args)...);
    }

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

    template <class ...Args>
    void remote_instantiate(Args&& ...args) {
        // Initialize the allocator.
        LogAssert(this->rdma_memory != nullptr, "RDMAMemory not received");
        void* memory = this->rdma_memory->vaddr;
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
    RDMAUnorderedMap(const RDMAUnorderedMap&) = delete;
    RDMAUnorderedMap& operator=(const RDMAUnorderedMap&) = delete;

    // Methods (mostly just forwarding):
    typename ContT::iterator begin() noexcept { return cont->begin(); }
    typename ContT::const_iterator begin() const noexcept { return cont->begin(); }
    typename ContT::const_iterator cbegin() const noexcept { return cont->cbegin(); }
    typename ContT::iterator end() noexcept { return cont->end(); }
    typename ContT::const_iterator end() const noexcept { return cont->end(); }
    typename ContT::const_iterator cend() const noexcept { return cont->cend(); }
    bool empty() const noexcept { return cont->empty(); }
    typename ContT::size_type size() const noexcept { return cont->size(); }
    typename ContT::size_type max_size() const noexcept { return cont->max_size(); }
    void clear() noexcept { cont->clear(); }
    std::pair<typename ContT::iterator, bool> insert(const typename ContT::value_type& value) { return cont->insert(value); }
    template <class P> std::pair<typename ContT::iterator, bool> insert(P&& value) { return cont->insert(value); }
    typename ContT::iterator insert(typename ContT::const_iterator hint, const typename ContT::value_type& value) { return cont->insert(hint, value); }
    template <class P> typename ContT::iterator insert(typename ContT::const_iterator hint, P&& value) { return cont->insert(hint, value); }
    template <class InputIt> void insert(InputIt first, InputIt last) { cont->insert(first, last); }
    void insert(std::initializer_list<typename ContT::value_type> ilist) { cont->insert(ilist); }
    template <class... Args> std::pair<typename ContT::iterator, bool> emplace(Args&&... args) { return cont->emplace(std::forward<Args>(args)...); }
    template <class... Args> typename ContT::iterator emplace_hint(typename ContT::const_iterator hint, Args&&... args) { return cont->emplace_hint(hint, std::forward<Args>(args)...); }
    typename ContT::iterator erase(typename ContT::const_iterator pos) { return cont->erase(pos); }
    typename ContT::iterator erase(typename ContT::const_iterator first, typename ContT::const_iterator last) { return cont->erase(first, last); }
    typename ContT::size_type erase(const typename ContT::key_type& key) { return cont->erase(key); }
    T& at(const Key& key) { return cont->at(key); }
    const T& at(const Key& key) const { return cont->at(key); }
    T& operator[](const Key& key) { return cont->operator[](key); }
    T& operator[](Key&& key) { return cont->operator[](key); }
    typename ContT::size_type count(const Key& key) const { return cont->count(key); }
    typename ContT::iterator find(const Key& key) { return cont->find(key); }
    typename ContT::const_iterator find(const Key& key) const { return cont->find(key); }
    std::pair<typename ContT::iterator, typename ContT::iterator> equal_range(const Key& key) { return cont->equal_range(key); }
    std::pair<typename ContT::const_iterator, typename ContT::const_iterator> equal_range(const Key& key) const { return cont->equal_range(key); }
    typename ContT::local_iterator begin(typename ContT::size_type n) { return cont->begin(n); }
    typename ContT::const_local_iterator begin(typename ContT::size_type n) const { return cont->begin(n); }
    typename ContT::const_local_iterator cbegin(typename ContT::size_type n) const { return cont->cbegin(n); }
    typename ContT::local_iterator end(typename ContT::size_type n) { return cont->end(n); }
    typename ContT::const_local_iterator end(typename ContT::size_type n) const { return cont->end(n); }
    typename ContT::const_local_iterator cend(typename ContT::size_type n) const { return cont->cend(n); }
    typename ContT::size_type bucket_count() const { return cont->bucket_count(); }
    typename ContT::size_type max_bucket_count() const { return cont->max_bucket_count(); }
    typename ContT::size_type bucket_size(typename ContT::size_type n) const { return cont->bucket_size(n); }
    typename ContT::size_type bucket(const Key& key) const { return cont->bucket(key); }
    float load_factor() const { return cont->load_factor(); }
    float max_load_factor() const { return cont->max_load_factor(); }
    void max_load_factor(float ml) { cont->max_load_factor(ml); }
    void rehash(typename ContT::size_type count) { cont->rehash(count); }
    void reserve(typename ContT::size_type count) { cont->reserve(count); }
    typename ContT::hasher hash_function() const { return cont->hash_function(); }
    typename ContT::key_equal key_eq() const { return cont->key_eq(); }


    // unimplemented:
    // unordered_map::swap
    // std::swap
    // operator!=, ==

protected:
    void set_container_address(void* container_addr) {
        LogInfo("Setting container address to %p", container_addr);
        cont = (ContT*) container_addr;
    }
    void* get_container_address() {
        LogInfo("Retrieving container address: %p", (void*) cont);
        return (void*) cont;
    }

};

#endif // __RDMA_UNORDERED_MAP_HPP__
