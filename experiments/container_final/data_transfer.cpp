#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>

#include <random>
#include <chrono>
#include <thread>

#include "distributed-allocator/mempool.hpp"
#include "c++-containers/rdma_vector.hpp"
#include "c++-containers/rdma_unordered_map.hpp"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./data_transfer path_to_config id container_size(in bytes) runs" << std::endl;
        return 1;
    }
    
    int id = atoi(argv[2]);
    size_t size = (size_t)atoi(argv[3]);
    int runs = atoi(argv[4]);
    
    //manager    
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    manager = memory_manager;
    initialize();
    using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;
    // key val pair
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

        
    if(id == 0) {
        // node A
        //initialize the container
        for (int num_run = 0; num_run<runs; num_run++) {
            RDMAUnorderedMap<int64_t, String> map(memory_manager);
            map.SetContainerSize(size);
            map.instantiate();

            //prefill keys
            printf("prefilling %d keys\n", num_entries);
            while (key < num_entries) {
                map[key] = val;
                key++;
            }
        
            //warmup ~ 1 million access
            printf("warming up\n");
            int access = 0;
            String valr;
            while(access < 3000000) {
                valr = map[distr(generator)];
                access++;
            }

            //actual experiment,simply send it
            map.Prepare(1);
            while(!map.PollForAccept()) {}
            
            map.Transfer();
            while(!map.PollForClose()) {}
            printf("experiment completed\n");
        }

    } else {
        // node B
        std::vector<std::vector<double>> collection;
        for (int num_run = 0; num_run<runs; num_run++) {
            
            RDMAUnorderedMap<int64_t, String> map(memory_manager);
            map.SetContainerSize(size);
            while(!map.PollForTransfer()) {}
            map.remote_instantiate();
            int access = 0;
            int total_access = 0;
            // printf("starting experiment %d for size %lu\n", num_run, size);
            //timer
            std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
            std::chrono::high_resolution_clock::time_point end;
            std::vector<double> group;
            String valr;
            while(total_access < 2000000) {
                valr = map[distr(generator)];
                // valr = map[total_access%(num_entries-1)];
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
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
            map.Close();
            // printf("experiment completed\n");
            collection.push_back(group);
        }

        printf("Container_size, %lu,key_space, %d\n",size,num_entries);
        // for (int i=0; i<(int)group.size() - 1; i++) {
        //     auto x = group.at(i);
        //         std::cout << x/1000 << std::endl;
            
        // }

        if (id != 0) {
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
            printf("request bin, mean_latency, sd\n");
            for (unsigned int i=0; i<total_entries; i++) {
                printf("%d, %f, %f\n", i, (double) (mean.at(i)/1000), (double) (sd.at(i)/1000) );
            }        
            
        }

    }
    return 0;
}

