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
    if (argc < 3) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./data_transfer path_to_config id container_size(in bytes)" << std::endl;
        return 1;
    }
    
    int id = atoi(argv[2]);
    size_t size = (size_t)atoi(argv[3]);
    
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
        volatile int access = 0;
        String valr;
        while(access < 4000000) {
            valr = map[distr(generator)];
            access++;
        }

        //actual experiment,simply send it
        map.Prepare(1);
        while(!map.PollForAccept()) {}
        
        map.Transfer();
        while(!map.PollForClose()) {}
        printf("experiment completed\n");

    } else {
        // node B
        std::vector<double> times;        
        
        RDMAUnorderedMap<int64_t, String> map(memory_manager);
        map.SetContainerSize(size);
        while(!map.PollForTransfer()) {}
        // auto s1 = std::chrono::high_resolution_clock::now();
        map.remote_instantiate();
        // auto e1 = std::chrono::high_resolution_clock::now();
        // times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(e1 - s1).count());

        int total_access = 0;
        // printf("starting experiment %d for size %lu\n", num_run, size);
        //timer
        String valr;
        while(total_access < 1000000) {
            auto s1 = std::chrono::high_resolution_clock::now();
            valr = map.at(distr(generator));
            auto e1 = std::chrono::high_resolution_clock::now();
            times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(e1 - s1).count());
            total_access++;
        }

        
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        map.Close();
        auto it = times.begin();
        double stop_and_copy = timer.get_duration_nsec();
        
        times.insert(it, stop_and_copy);
        std::cout << times.at(0) << std::endl;
        printf("Container_size, %lu,key_space, %d\n",size,num_entries);

        if (id != 0) {
            std::vector<std::vector<double>> bins;
            for (unsigned int i=0; i<times.size(); i++) {
                std::vector<double> bin;
                for (int j=0; j<1000; j++) {
                    bin.push_back(times.at(i));
                    i++;
                    if(i >= times.size()) {
                        break;
                    }
                }
                bins.push_back(bin);
            }

            // this is the pulls
            std::vector<double> mean_list;
            std::vector<double> nfivep_list;
            std::vector<double> max_list;
            
            for (std::vector<double> bin : bins) {
                //sum of each bin
                double sum = 0.0;
                for (double x : bin)
                    sum += x;
                
                double mean = sum/((double)bin.size());
                mean_list.push_back(mean);
                std::sort(bin.begin(), bin.end());
                max_list.push_back(bin.at(bin.size()-1));

                double nfivep = bin[((bin.size()*95)/100)];
                nfivep_list.push_back(nfivep);
            }
 
            // //final vals
            // printf("request access, mean_latency,95 percentile,\n");
            for (unsigned int i=0; i<mean_list.size(); i++) {
                printf(" %d, %f, %f\n", i, (double) (mean_list.at(i)/1000), (double) (nfivep_list.at(i)/1000));
            }    
        }
    }
    return 0;
}

