
This repository holds the interface and implementations of the migratable memory allocator design.

The goal of migratable memory allocators is to be able to migrate complex data structures by directly moving memory (i.e. via RDMA) without any form of serialization.

The simplest way to do this, in short, is to place the memory at the same virtual addresses in the destination machine as they were in the originating machine. This comes with many nuances and challenges; the largest one being ensuring the target virtual addresses in the destination machine remain free, via some form of coodination.

This repository provides a memory pool allocator (`mempool_factory.hpp`) that allocates migratable memory pools. The factory relies on some sort of centralized address space coordination to deal with the nuances mentioned above; this is abstracted away in `abstract_vaddr_coordinator.hpp'. See the comments there for more details.

For a test demonstration of how the pool can be used, see `test_peer_a` and `test_peer_b`.
