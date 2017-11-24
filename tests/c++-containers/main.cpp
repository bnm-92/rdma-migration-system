#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>

#include "mempool.hpp"
#include "rdma_vector.hpp"
#include "rdma_unordered_map.hpp"

// This only works with containers that take exactly two template arguments
// e.g. not map, set, etc.
// There is no way to generalize the number of template arguments in C++98,
// so the only solution for the other types is to make their own functions,
// overloading-style (and at that point it's probably best to just get rid of
// the template entirely). Since this is unwieldly, we'll leave it like this
// and move on to the C++11 implementation, where we bring in scoped
// allocators.
// template <
//     typename T,
//     template<typename type_arg, typename alloc_arg> class container_tm >
// container_tm< T, std::scoped_allocator_adaptor<PoolBasedAllocator<T>> >* new_fixed_addr_container(
//     void* pool_addr, size_t pool_size
// ) {
//     // These type names are really messy.
//     // We'll use cont_t for the container type which we wish to allocate,
//     // which is template-constructed from our container_tm argument.
//     typedef container_tm< T, std::scoped_allocator_adaptor<PoolBasedAllocator<T>> > cont_t;

//     // We'll place the object itself on the given address,
//     // so we'll just return the given address.
//     cont_t* res = static_cast<cont_t*>(pool_addr);
//     // Since we've used up some of our pool space with our object,
//     // we must tell the allocator it can't use this space.
//     void* next = (void*)((char*)pool_addr + sizeof(cont_t));
//     // Now construct the object, with the allocator, and place it
//     // at the address given.
//     new((void*)pool_addr) cont_t(
//         std::scoped_allocator_adaptor<PoolBasedAllocator<T>>(
//             PoolBasedAllocator<T>(pool_addr, pool_size, next)));

//     return res;
// }

// void* addr = (void*)((intptr_t)1<<44 - (intptr_t)1<<5);
// size_t length = (size_t)1<<5;
// int prot = PROT_READ | PROT_WRITE;
// int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
// int fd = -1;
// off_t offset = 0;
// void* map = mmap(addr, length, prot, flags, fd, offset);
// std::cerr << "mmap returned address " << map;
// std::cerr << " and error " << strerror(errno) << std::endl;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "please provide all arguments" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;


    if (id == 0) {
        RDMAUnorderedMap<int, int> map(memory_manager);
        map.instantiate();
        map[0] = 0;
        map[1] = 1;
        map.Prepare(1);
        while(!map.PollForAccept()) {}
        
        std::cerr << "Contents: \n";
        for (auto i : map) {
            std::cerr << i.first << " " << i.second << "\n";
        }
        std::cerr << std::endl;

        map.Transfer();

        while(map.PollForClose() == nullptr) {}

        // RDMAVector<String> vec(memory_manager);
        
        // vec.instantiate();
        // vec.push_back("hello world!");
        // vec.push_back("a reeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeally long string");
        // std::cerr << "Contents: \n";
        // for (const auto& i : vec) {
        //     std::cerr << i << "\n";
        // }
        // std::cerr << std::endl;
        // vec.Prepare(1);
        
        // while(!vec.PollForAccept()){}

        // vec.Transfer();

        // while(vec.PollForClose() == nullptr){}

    } else {
        // got a prepare message therefore we will instantiate our vector with RDMA, also, 
        // a potentiall useful thing to add would be what type of container we are transferring
        // RDMAVector<String> vec(memory_manager); // no need to initialize this, instead we can accept the incoming container
        // RDMAMemory* memory = nullptr;
        // while((memory = vec.PollForTransfer()) == nullptr){}

        // vec.remote_instantiate(memory);

        // LogInfo("transfer complete, lets do a push back");
        // vec.push_back("yolo");

        // std::cerr << "Contents: \n";
        // for (const auto& i : vec) {
        //     std::cerr << i << "\n";
        // }
        // std::cerr << std::endl;
        // vec.Close();

        RDMAUnorderedMap<int, int> map(memory_manager);
        while(!map.PollForTransfer()) {}

        map.remote_instantiate();
        map[3] = 4;

        std::cerr << "Contents: \n";
        for (auto i : map) {
            std::cerr << i.first << " " << i.second << "\n";
        }
        std::cerr << std::endl;

        map.Close();
    }
    LogInfo("EXPERIMENT COMPLETE");
    return 0;
}

