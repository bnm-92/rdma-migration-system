#ifndef __RDMA_CONTAINER_UTILS_HPP__
#define __RDMA_CONTAINER_UTILS_HPP__

#include <string>

#include "pool_based_allocator.hpp"

// The type for std::strings that are custom allocated with our RDMA
// compatible allocator.
// (Later, can parameterize and template out CharT and Alloc template?)
using RDMAString = std::basic_string
    <char, std::char_traits<char>, PoolBasedAllocator<char>>;

// std::hash is not defined on custom allocated string types,
// so we'll provide our own for convenience..
struct RDMAStringHasher {
    size_t operator() (const RDMAString& s) const {
        // Currently, this just casts the RDMASring& to a std::string&,
        // and feeds it to std::hash.
        // It seems to work for now, but TODO look at this later.
        // return std::hash<std::string>() ((std::string&) s);
        return std::hash<std::string>()(std::string(s.c_str()));
    }
};

#endif
