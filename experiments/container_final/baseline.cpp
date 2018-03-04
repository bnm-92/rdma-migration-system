#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>

#include <random>
#include <chrono>

#include "distributed-allocator/mempool.hpp"
#include "c++-containers/rdma_vector.hpp"
#include "c++-containers/rdma_unordered_map.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./basline_container path_to_config container_size(in bytes)" << std::endl;
        return 1;
    }

    size_t size = (size_t)atoi(argv[2]);
    
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


    std::unordered_map<int64_t, String> map;
    
    // prefill
    while (key < num_entries) {
        map[key] = val;
        key++;
    }


    //warmup ~ 3 million access
    // printf("warming up\n");
    int access = 0;
    String valr;
    while(access < num_entries) {
        valr = map.at(access);
        access++;
    }

    access = 0;
    while(access < 3000000) {
        valr = map.at(distr(generator));
        access++;
    }

    access = 0;
    int64_t total_access = 0;

    std::vector<double> times;        
    while(total_access < 500000) {
        auto s1 = std::chrono::high_resolution_clock::now();
        valr = map.at(distr(generator));
        auto e1 = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(e1 - s1).count());
        access++;
        total_access++;
    }

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
    // printf("request access, mean_latency,95 percentile,\n");
    for (unsigned int i=0; i<mean_list.size(); i++) {
        printf(" %d, %f, %f,\n", i, (double) (mean_list.at(i)/1000), (double) (nfivep_list.at(i)/1000) );
    }        
    
    return 0;
}

