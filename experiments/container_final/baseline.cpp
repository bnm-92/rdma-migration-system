#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>

#include <random>
#include <chrono>

#include "mempool.hpp"
#include "rdma_vector.hpp"
#include "rdma_unordered_map.hpp"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./basline_container path_to_config container_size(in bytes) runs" << std::endl;
        return 1;
    }

    size_t size = (size_t)atoi(argv[2]);
    int runs = atoi(argv[3]);
    
    // RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], 0);
    using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;
    // manager = memory_manager;

    //key/val 8 bytes + 128 bytes
    int64_t key = 0;
    String val = "";
    while(val.size() != 128) {
        val.append("a");
    }

    //number of keys
    int num_entries = (size*0.6)/(136);

    //uniform number generator
    const int range_from  = 0;
    const int range_to    = num_entries - 1;
    std::random_device                  rand_dev;
    std::mt19937                        generator(rand_dev());
    std::uniform_int_distribution<int>  distr(range_from, range_to);

    // distr(generator) for key generation

    // while(access < total_access) {
    //     access++;
    //     valr = map[distr(generator)];
    // }

    // auto end = std::chrono::high_resolution_clock::now();
    // std::chrono::duration<double> elapsed_time = end - start;
    // auto time = std::chrono::duration_cast<std::chrono::microseconds>(elapsed_time).count();
                        
    // std::cout << "total accesses: " << total_access << "total time: " << time << std::endl;
    // std::cout << total_access/time << std::endl;
    std::vector<std::vector<double>> collection;
    for (int x = 0; x<runs; x++) {
        std::unordered_map<int64_t, String> map;
        // RDMAUnorderedMap<int64_t, String> map(memory_manager);
        // map.SetContainerSize(size*2);
        // map.instantiate();

// printf("prefilling %d keys\n", num_entries);
        // prefill
        while (key < num_entries) {
            map[key] = val;
            key++;
        }

        //warmup ~ 1 million access
        // printf("warming up\n");
        int access = 0;
        String valr;
        while(access < 3000000) {
            valr = map[distr(generator)];
            access++;
        }

        //actual experiment
        // printf("actual experiment\n");
        access = 0;
        // int64_t total_access = 20000000;
        int64_t total_access = 0;
        std::vector<double> group;
        std::chrono::high_resolution_clock::time_point end;
            
        auto start = std::chrono::high_resolution_clock::now();
        

        while(total_access < 2000000) {
            valr = map[distr(generator)];
            access++;
            total_access++;

            if(access > 999) {
                end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed_time = end - start;
                auto time = std::chrono::duration_cast<std::chrono::microseconds>(elapsed_time).count();
                group.push_back(time);
                access = 0;
                access++;
                start = std::chrono::high_resolution_clock::now();
            }
        }
    
        if(access > 0 && access < 1000) {
            end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_time = end - start;
            auto time = std::chrono::duration_cast<std::chrono::microseconds>(elapsed_time).count();
            group.push_back(time);
        }    
    
        collection.push_back(group);
    }




    // printf("experiment completed\n");
    // printf("experiment completed\n");
    printf("Container_size, %lu,key_space, %d",size,num_entries);
    
    // for (int i=0; i<(int)group.size(); i++) {
    //     auto x = group.at(i);
    //     if(i == (int) group.size()-1) {
    //         int div = access > 0 ? access : 1000; 
    //         std::cout << x/div << std::endl;
    //     } else {
    //         std::cout << x/1000 << std::endl;
    //     }         
        
    // }


    //this is the pulls
    std::vector<double> sum;
    std::vector<double> var;
    std::vector<double> sd;
    std::vector<double> mean;
    
    //insert dummy values
    unsigned int total_entries = collection.at(0).size()-1;
    unsigned int total_collections = collection.size();
    
    for (unsigned int i=0; i<total_entries; i++) {
        sum.push_back(0.0);
        mean.push_back(0.0);
        var.push_back(0.0);
        sd.push_back(0.0);
    }

    //sum
    for(unsigned int i=0; i<total_collections; i++) {
        for (unsigned int j=0; j<total_entries; j++) {
            sum.at(j) += collection.at(i).at(j);
        }
    }

    //mean
    for (unsigned int i=0; i<total_entries; i++) {
        mean.at(i) = ((double)sum.at(i)/(double)total_collections);
    }

    //var
    for(unsigned int i=0; i<total_collections; i++) {
        for (unsigned int j=0; j<total_entries; j++) {
            var.at(j) += (collection.at(i).at(j) - mean.at(j)) * (collection.at(i).at(j) - mean.at(j));
        }
    }

    //var + sd
    for (unsigned int i=0; i<total_entries; i++) {
        var.at(i) /= total_collections;
        sd.at(i) = sqrt(var.at(i));
    }

    //final vals
    printf("request bin, mean_latency,sd,\n");
    for (unsigned int i=0; i<total_entries; i++) {
        printf(" %d, %f, %f,\n", i, (double) (mean.at(i)/1000), (double) (sd.at(i)/1000) );
    }        
    


    return 0;
}

