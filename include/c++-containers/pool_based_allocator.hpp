#ifndef __POOL_BASED_ALLOCATOR_HPP__
#define __POOL_BASED_ALLOCATOR_HPP__

/*
A custom allocator that allocates from a MemoryPool.

----

Context:

"Allocators" are C++ objects that expose an allocate() (and deallocate()) method.
You pass in the amount of memory you want to allocate, and it will return you
an address, ready-to-use, for that amount of memory.
Allocators are what is used under-the-hood by STL containers for their
memory management. Unless you specify a custom allocator, STL containers will
use std::allocator, a builtin default.

Q: I'm confused. Why do allocators exist when we have new/malloc?
A: You can think of allocators as being an alternative abstraction that does
two important things. One, it separates the concepts of *allocation*
(requesting memory) from *construction* (putting things into that memory).
This is useful because now we can increase the amount of control we have over
memory allocation simply by modifying our allocator. For instance, we may want
to allocate memory contiguously for better access, or allocate only from a
specified pool (which is what this class actually does!)

For more information, see http://en.cppreference.com/w/cpp/concept/Allocator
*/

#include <iostream>
#include <limits>

#include "mempool.hpp"

// This allocator was started with boilerplate code from:
// https://howardhinnant.github.io/allocator_boilerplate.html
// The author, Howard Hinnant, is the author of libc++.
// Comments are mine.
template <class T>
class PoolBasedAllocator {
public: // Boilerplate.
    int debug_level = 0;

    using value_type = T;
    using pointer = value_type*;
    using const_pointer = typename \
        std::pointer_traits<pointer>::template rebind<value_type const>;
    using void_pointer = typename \
        std::pointer_traits<pointer>::template rebind<void>;
    using const_void_pointer = typename \
        std::pointer_traits<pointer>::template rebind<const void>;
    using reference = value_type&;
    using const_reference = const reference;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;

    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap            = std::false_type;
    using is_always_equal                        = std::is_empty<PoolBasedAllocator>;

    template <class U> friend class PoolBasedAllocator;
    template <class U> struct rebind {typedef PoolBasedAllocator<U> other;};

private:
    // The memory pool to use. The allocator does NOT own the memory pool.
    // If this is NULL, then this is a temporary allocator that should behave
    // as if it were an std::allocator.
    MemoryPool** mempool;

public:
    // Constructor.
    PoolBasedAllocator(MemoryPool** mempool) noexcept : mempool(mempool) {
        // std::cerr << "Creating PoolBasedAllocator using "
        //           << *mempool << std::endl;
    }

    // One thing I noticed about using scoped allocators is that
    // for syntactic cleanliness, it requires the allocators you're scoping
    // to have default constructors.
    //
    // I'll illustrate with an example.
    // Suppose we a custom allocator called MyAlloc:
    //      template <class T> class MyAlloc {...}
    // and we want to make a vector of strings,
    // such that both the vector nodes AND the string data is allocated
    // using the same instance of our custom allocator.
    //
    // Well, first, this is actually impossible for a vector of std::strings,
    // since std::strings are already pre-templated with the std::allocator.
    // (Details: http://en.cppreference.com/w/cpp/string/basic_string)
    // So we can only do this with a custom string class, like this one:
    //      typedef std::basic_string<
    //          char, std::char_traits<char>,
    //          std::scoped_allocator_adaptor<MyAlloc<char>>> CustomString
    // (Note that the string's allocator doesn't need to be scoped, but I will
    // be following the principle that all containers should use scoped
    // allocators for safety of propagation. Also, all scoped allocators
    // can be used as normal allocators anyway.)
    // Great. And the vector that holds these strings will be typed
    //      std::vector<
    //          CustomString,
    //          std::scoped_allocator_adaptor<MyAlloc<CustomString>>>
    // Awesome. Now let's make one of these vectors. First, make the allocator
    //      MyAlloc<CustomString> myAlloc();
    // (Note that this doesn't have to be scoped because it'll get implicitly
    // made into a scoped allocator when we pass it into a container that uses
    // scoped allocators.)
    // and now let's make the vector
    //      std::vector<...> myVec(myAlloc);
    // and let's add a string to it.
    //      myVec.push_back("hello");
    // (Note that "hello") gets implicitly constructed into a CustomString,
    // just like how it would normally be implicitly made into a std::string.
    //
    // Great. This works. Both "hello" and the vector element holding a pointer
    // to it were allocated using myAlloc.
    // But how does this actually happen? First, "hello" gets made,
    // *on the stack*. So CustomString("hello") gets called. But notice
    // CustomString is a custom-allocator-templated type and yet we didn't
    // pass it an allocator!!
    // So the CustomString constructor calls the default (no-args) constructor
    // for the allocator type we templated it with; it calls MyAlloc() to make
    // its own MyAlloc and uses THAT to allocate the space for "hello".
    // So when does it actually use our instance, myAlloc?
    // It uses it when myVec.push_back calls the copy constructor for the
    // object it's pushing back. This call goes through myVec's scoped allocator,
    // and the "hello" *in myVec* is correctly allocated using myAlloc.
    // (Details: https://stackoverflow.com/a/22348825)
    //
    // Hopefully this all makes sense.
    // If it does, it should also now be clear that if a default constructor
    // MyAlloc() did not exist (say the constructor is MyAlloc(arena)), then
    // the above code does not work. We have two options:
    // 1. Use the proper constructor when creating the stack argument. So
    //      myVec.push_back("hello");
    // becomes
    //      myVec.push_back(CustomString("hello", MyAlloc(temp_arena)));
    // There are two problems with this. One, it's ugly. Two, the allocator
    // needs to be fully functional and yet only lives for a very short time.
    // The second problem can't be avoided when using push_back
    // (emplace_back should work just fine, although I haven't tested it yet).
    // 2. The second solution is to wrap this dummy arena logic away in the
    // default constructor:
    //      myAlloc() { arena = TempArena(); }
    // which is what I will choose to do here.
    PoolBasedAllocator() noexcept : debug_level(3), mempool(NULL) {}

    template <class U>
    PoolBasedAllocator(PoolBasedAllocator<U> const& a) noexcept :
        mempool(a.mempool) {}

    pointer allocate(size_type num) {
        if (mempool == NULL) { return this->temp_allocate(num); }
        return static_cast<pointer>(
            (*mempool)->allocate(num * sizeof(value_type)));
    }
    void deallocate(pointer addr, size_type num) noexcept {
        if (mempool == NULL) { return this->temp_deallocate(addr, num); }
        return (*mempool)->deallocate(addr, num * sizeof(value_type));
    }

    // See commment at mempool field for details.
    pointer temp_allocate(size_type num) {
        size_t bytes = num * sizeof(value_type);
        if (debug_level < 2) {
            std::cerr << "WARNING: calling malloc(size = " << bytes
                      << ") via default PoolBasedAllocator()." << std::endl;
        }
        return static_cast<pointer>(malloc(bytes));
    }
    void temp_deallocate(pointer addr, size_type num) noexcept {
        if (debug_level < 2) {
            std::cerr << "WARNING: calling free(ptr = " << static_cast<void*>(addr)
                      << ") via default PoolBasedAllocator()." << std::endl;
        }
        return free(static_cast<void*>(addr));
    }

    template <class U, class ...Args>
    void construct(U* p, Args&& ...args) {
        // This is placement new syntax.
        ::new(p) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U* p) noexcept {
        p->~U();
    }

    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max();
    }

    PoolBasedAllocator select_on_container_copy_construction() const {
        return *this;
    }
};

template <class T, class U>
bool operator==(
    PoolBasedAllocator<T> const& alloc_a, PoolBasedAllocator<U> const& alloc_b
) noexcept {
    // False for safety.
    // Later we should consider returning alloc_a.mempool == alloc_b.mempool.
    return false;
}

template <class T, class U>
bool operator!=(PoolBasedAllocator<T> const& x, PoolBasedAllocator<U> const& y) noexcept {
    return !(x == y);
}

#endif // __POOL_BASED_ALLOCATOR_HPP__
