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
    using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;

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

    int access = 0;
    String valr;

    std::vector<double> times;        
    std::vector<std::vector<double>> bins;
    std::vector<int> bins_for_calls;

    int bin = 0;
    bins.push_back(*(new std::vector<double>()));
    bins_for_calls.push_back(0);


    double time_interval = 1000; // in micro seconds
    double time_passed = 1000; //starting from the first one
    auto start = std::chrono::high_resolution_clock::now();

    while(true) {
        auto end = std::chrono::high_resolution_clock::now();
        
        if (std::chrono::duration_cast<std::chrono::seconds>(end - start).count() > 5) {
            break;
        } else if (std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() > time_passed) {
            //bin it
            bins.push_back(*(new std::vector<double>()));
            bins_for_calls.push_back(0);
            bin++;
            time_passed += time_interval; 
            double x = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            times.push_back(x);
        }

        auto s1 = std::chrono::high_resolution_clock::now();
        valr = map.at(distr(generator));
        auto e1 = std::chrono::high_resolution_clock::now();

        bins[bin].push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(e1 - s1).count());

    }


    printf("Container_size, %lu,key_space, %d\n",size,num_entries);

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
    printf("time in milli second, mean_latency,95 percentile, throuhgput\n");
    for (unsigned int i=0; i< mean_list.size() - 1; i++) {
        printf(" %f, %f, %f, %d, %d\n", (double) (times[i]/1000), (double) (mean_list.at(i)/1000), (double) (nfivep_list.at(i)/1000), (int)bins[i].size(), bins_for_calls.at(i));
    }
    
    return 0;
}

