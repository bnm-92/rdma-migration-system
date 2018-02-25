#include "RDMAMemory.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        LogError("./testRDMAMemoryTransfer config server_id bytes");
        return 1;
    }
    int server_id = atoi(argv[2]);
    RDMAMemoryManager manager(argv[1], server_id);

    /*
        this files simply tests the transfer ownership of a memory region
    */
    int memory_size = atoi(argv[3]); // bytes

    if(server_id == 0) {
        
        size_t size = memory_size;
        char* memory = (char*)manager.allocate(size);
        LogInfo("allocated memory at %lu", (uintptr_t)memory);

        /*
            Prepare
        */
        manager.Prepare((void*)memory, size, 1);
        
        /*
            work on memory in the meanwhile
        */
        
        int i = 0;
        RDMAMemory* rdma_memory = nullptr;
        while((rdma_memory = manager.PollForAccept()) == nullptr) {
            memory[i] = 'A';
            i++;
            if(i >= (int)size)
                i = 0;
        }
        memory[size-1] = '\0';
        LogInfo("Transfering memory: %s of size %zu", memory, size);

        /*
            Transfer
        */
        manager.Transfer(rdma_memory->vaddr, rdma_memory->size, rdma_memory->pair);
        manager.PollForClose();
    } else {
        /*
            serverid is 1, so wait for a message from any server
        */

        RDMAMemory* rdma_memory  = nullptr;
        while((rdma_memory = manager.PollForTransfer()) == nullptr) {
        }
        

        char* memory = (char*)rdma_memory->vaddr;
        LogInfo("Pulled memory with %p with %s of size %zu", memory, memory, rdma_memory->size);
        manager.close(rdma_memory->vaddr, rdma_memory->size, rdma_memory->pair);
    }

    LogInfo("DONE");
    return 0;
}
