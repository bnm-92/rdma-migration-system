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
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./data_transfer path_to_config id container_size(in bytes) bucket_size_in_milli_seconds" << std::endl;
        return 1;
    }
    
    int id = atoi(argv[2]);
    size_t size = (size_t)atoi(argv[3]);
    int bucket_size = atoi(argv[4]);
    
    //manager    
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    #if PAGING
    manager = memory_manager;
    initialize();
    #endif
    using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;
    // key val pair
    //key/val 8 bytes + 128 bytes
    int64_t key = 0;
    String val = "";
    while(val.size() != 128) {
        val.append("a");
    }

    //number of keys
    int num_entries = (size*0.60)/(136);

    //uniform number generator
    const int range_from  = 0;
    const int range_to    = num_entries - 1;
    std::random_device                  rand_dev;
    std::mt19937                        generator(rand_dev());
    std::uniform_int_distribution<int>  distr(range_from, range_to);

    // std::vector<int> workload;
    // for (int i=0; i<num_entries; i++) {
    //     workload.push_back(i);
    // }

    // for (int i=0; i<workload.size(); i++) {
    //     srand(time(NULL));
    //     int swap = rand() % (workload.size() - 1) ;
    //     int tmp = workload[i];
    //     workload[i] = workload[swap];
    //     workload[swap] = tmp;
    // }

    // distr(generator) for key generation

    if(id == 0) {
        // node A
        //initialize the container
        RDMAUnorderedMap<int64_t, String> map(memory_manager);
        map.SetContainerSize(size);
        map.instantiate();
        // map.reserve(num_entries);

        //prefill keys
        printf("prefilling %d keys\n", num_entries);
        while (key < num_entries) {
            map[key] = val;
            key++;
        }
    
        std::cout << "bucket count" << map.bucket_count() << std::endl;
        std::cout << "load factor" << map.load_factor() << std::endl;

        for (unsigned int i=0; i<map.bucket_count(); i++) {
            if(map.bucket_size(i) > 1) {
                std::cout << map.bucket_size(i) << std::endl;
            }
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
        std::vector<std::vector<double>> bins;
    
        std::vector<int> bins_for_calls;
            
        RDMAUnorderedMap<int64_t, String> map(memory_manager);
        map.SetContainerSize(size);
        while(!map.PollForTransfer()) {}
        map.remote_instantiate();

        //timer
        String valr;


        double offset = 0.0;
        #if !PAGING
            // auto it = bins.begin();
            // std::vector<double> stop_and_copy; 
            // stop_and_copy.push_back(timer.get_duration_nsec());
            
            // std::cout << stop_and_copy << std::endl;
            // bins.push_back(stop_and_copy);
            offset = timer.get_duration_usec();
        #endif
        int bin = 0;
        bins.push_back(*(new std::vector<double>()));
        bins_for_calls.push_back(0);

        // int index = 0;
        double time_interval = bucket_size * 1000; // in micro seconds
        double time_passed = bucket_size * 1000; //starting from the first one
        auto start = std::chrono::high_resolution_clock::now();

        while(true) {
            auto end = std::chrono::high_resolution_clock::now();
            
            if (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() > (2300 - offset/(1000))) {
                break;
            } else if (std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() > time_passed) {
                //bin it
                bins.push_back(*(new std::vector<double>()));
                bins_for_calls.push_back(0);
                bin++;
                time_passed += time_interval; 
                double x = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() + offset;
                times.push_back(x);
            }

            auto s1 = std::chrono::high_resolution_clock::now();
            valr = map[distr(generator)];
            // valr = map.at(workload[index]);
            auto e1 = std::chrono::high_resolution_clock::now();
            // index = (index >= (workload.size() - 1))? 0 : index + 1;
            bins[bin].push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(e1 - s1).count());

            #if PAGING
            if(remote_call){
                bins_for_calls[bin]++;
                remote_call = false;
            }
            #endif            
        }
        
        map.Close();

        // std::cout << times.at(0) << std::endl;
        printf("Container_size, %lu,key_space, %d\n",size,num_entries);

        if (id != 0) {
            std::vector<double> mean_list;
            std::vector<double> nfivep_list;
            
            for (std::vector<double> bin : bins) {
                //sum of each bin
                double sum = 0.0;
                for (double x : bin)
                    sum += x;
                
                double mean = sum/((double)bin.size());
                mean_list.push_back(mean);
                std::sort(bin.begin(), bin.end());
                double nfivep = bin[((bin.size()*95)/100)];
                nfivep_list.push_back(nfivep);
            }
 
            // //final vals
            printf("time in milli second, mean_latency,95 percentile, throuhgput\n");
            for (unsigned int i=0; i< mean_list.size() - 2; i++) {
                printf(" %f, %f, %f, %d, %d\n", (double) (times[i]/1000), (double) (mean_list.at(i)/1000), (double) (nfivep_list.at(i)/1000), (int)bins[i].size(), bins_for_calls.at(i));
            }
        }
    }
    return 0;
}

