#ifndef __MISCUTILS_HPP__
#define __MISCUTILS_HPP__

#include <iostream>
#include <sys/time.h>
#include <stdexcept>
#include <queue> 
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <assert.h> 
#include <cstddef>
#include <map>


/**
 * enables demand paging and sets up pages and sigsegv handler
*/
#define PAGING 1

/**
 * enable fault tolerance
*/
#define FAULT_TOLERANT 0


/**
 * allows automatic prefetching of the entire memory segment on a new thread
*/
#define PREFETCHING 0
#define ASYNC_PREFETCHING 0

#define ASCII_STARS "**********************************************************************"
/**
 * DEBUG and LEVEL signify how much tracing is followed in the system, 
 * TODO: update to take in a file parameter instead of spitting to stderr stream
*/
#define DEBUG 1
#define LEVEL 0
#define LogMessage(Level, SEVERITY, ...) do { if (DEBUG && Level <= LEVEL) {fprintf(stderr, "%s %s:%03u in %s : ", SEVERITY, __FILE__, __LINE__, __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr);}if(Level == 0) {fflush(stderr); /*exit(1);*/}} while(false)

#define LogError(...) LogMessage(0, "ERROR", __VA_ARGS__)
#define LogInfo(...) LogMessage(3, "INFO", __VA_ARGS__)
#define LogWarning(...) LogMessage(2, "WARNING", __VA_ARGS__)
#define LogAssertionError(...) LogMessage(0, "ASSERT ERROR", __VA_ARGS__);
#define LogAssert(COND, ...) if (!(COND)) { LogAssertionError(__VA_ARGS__) }

/**
 * max async prefetching limitation, async prefetcher will wait until the callback is executed after
 * max_async_pending operations
*/

static int max_async_pending = 1;
#if FAULT_TOLERANT || PAGING
class RDMAMemoryManager; // forward decleration
static RDMAMemoryManager* manager = nullptr;
#endif


/**
 * For fault tolerance branch, we are using ALLOCATABLE_RANGE_START
 * as a starting point for zookeeper, each allocation will go read this and increment it
*/

static const ptrdiff_t TOP_BOTTOM_MARGIN = \
// This is 2^44, or 0x 1000 0000 0000, or about 16 TiB of space.
(ptrdiff_t)1 << 44;
static const uintptr_t ALLOCATABLE_RANGE_START = \
// This is hex: 0x 1000 0000 0000
// which is at 16 TiB into the address space.
(uintptr_t)0 + TOP_BOTTOM_MARGIN;
const static uintptr_t ALLOCATABLE_RANGE_END = \
// This is hex: 0x 7000 0000 0000
// which is at 112 TiB into the address space.
((uintptr_t)1 << 47) - TOP_BOTTOM_MARGIN;

class MultiTimer {
public:
    MultiTimer() : started(false), stopped(false), times() {}

    void start() {
        if (started) {
            throw std::logic_error("start() called on already started timer!");
        }
        begin_st = std::chrono::high_resolution_clock::now();
        started = true;
    }
    void stop() {
        if (stopped or not started) {
            throw std::logic_error("stop() called on a stopped or not-started timer!");
        }
        end_st = std::chrono::high_resolution_clock::now();
        
        stopped = true;
        double elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_st - begin_st).count();
        times.push_back(elapsed);
        this->reset();
    }
    
    double get_duration_usec() {
        double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end_st - begin_st).count();
        return elapsed;
    }

    void reset() {
        started = false;
        stopped = false;
    }

    std::vector<double> getTime() {
        return times;
    }

private:
    bool started;
    bool stopped;
    std::chrono::high_resolution_clock::time_point begin_st;
    std::chrono::high_resolution_clock::time_point end_st;
    std::vector<double> times;

};

static MultiTimer multi_timer;

class TestTimer {
public:
    TestTimer() : started(false), stopped(false) {}

    void start() {
        if (started) {
            throw std::logic_error("start() called on already started timer!");
        }
        begin_st = std::chrono::high_resolution_clock::now();
        started = true;
    }
    void stop() {
        if (stopped or not started) {
            throw std::logic_error("stop() called on a stopped or not-started timer!");
        }
        end_st = std::chrono::high_resolution_clock::now();
        stopped = true;
    }
    double get_duration_usec() {
        double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end_st - begin_st).count();
        return elapsed;
    }

    double get_duration_nsec() {
        double elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_st - begin_st).count();
        return elapsed;
    }

    void reset() {
        started = false;
        stopped = false;
    }

private:
    bool started;
    bool stopped;
    std::chrono::high_resolution_clock::time_point begin_st;
    std::chrono::high_resolution_clock::time_point end_st;
};

static TestTimer timer;

// A threadsafe-queue, from stack overflow.
template <class T>
class ThreadsafeQueue {
public:
    ThreadsafeQueue() : q(), m(), c() {}
    ~ThreadsafeQueue() {}

    // Add an element to the queue.
    void enqueue(T t) {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
        c.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait utill a element is avaiable.
    T dequeue(void) {
        std::unique_lock<std::mutex> lock(m);
        while (q.empty()) {
          // release lock as long as the wait and reaquire it afterwards.
            c.wait(lock);
        }
        T val = q.front();
        q.pop();
        return val;
    }

    T peek(void) {
        std::lock_guard<std::mutex> lock(m);
        if (q.empty())
            return NULL;
        return q.front();
    }

    bool empty(void) {
        std::unique_lock<std::mutex> lock(m);
        return q.empty();
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};

class RNode {
public:
    RNode(): id(-1), ip("0.0.0.0"), port(5000) {}
    ~RNode(){};

    RNode(int id, std::string ip, int port) :
     id(-1), ip("0.0.0.0"), port(5000) {
        this->id = id;
        this->ip = ip;
        this->port = port;
    }

    int id;
    std::string ip;
    int port;
};

class ConfigParser{
/*
    file formatting for parsing would be
    line1: number of servers
    line 2 - n+1: serverid ip:port (can leave ip open to interpretation so the nodes that join the mesh would just spin up and join the live servers)
*/

public:
    ConfigParser() : path(""), num_servers(0) {}
    ~ConfigParser(){
        #if FAULT_TOLERANT
            if(zk_errorfile != nullptr) {
                fclose(zk_errorfile);
            }
        #endif
    }

    RNode* getNode(int id) {
        return this->map.at(id);
    }

    void parse(std::string path) {
        
        std::fstream is_stream(path);

        std::string line;
        try {
            if(!is_stream)
                LogError("file error");

            num_servers = 0;
            if(std::getline(is_stream, line)) {
                num_servers = std::stoi(line);
            }

            if(num_servers == 0) {
                LogError("Number of servers not specified or Equal to Zero");
            } else {
                LogInfo("Config has %d servers", num_servers);
            }

            for (int i=0; i<num_servers; i++) {
                if(std::getline(is_stream, line)) {
                    std::stringstream stm(line);
                    char delim = ' ';
                    std::string token;

                    if(!std::getline(stm, token, delim))
                        LogError("delim error");

                    int id = stoi(token);

                    if(!std::getline(stm, token, delim))
                        LogError("delim error");

                    std::string ip = token;
                    
                    if(!std::getline(stm, token, delim))
                    LogError("delim error");

                    int port = stoi(token);
                    
                    LogInfo("server id %d with ip:port %s:%d", id, ip.c_str(), port);

                    map.insert({id, new RNode(id, ip, port)});

                } else {
                    LogError("Could not find expected server information for server %d", i);
                }
            }
            #if FAULT_TOLERANT
            // lets take zookeeper parameters, zookeeperserverlist,heartbeats_timeout,logFile
            if(std::getline(is_stream, line)) {
                LogInfo("zookeeper config is %s", line.c_str());
                std::stringstream stm(line);
                char delim = ',';
                std::string token;

                if(!std::getline(stm, token, delim))
                    LogError("delim error");
                
                this->zookeeper_servers = token;

                if(!std::getline(stm, token, delim))
                    LogError("delim error");
                
                this->heartbeat = atoi(token.c_str());

                if(!std::getline(stm, token, delim))
                    LogError("delim error");
                
                // if(token == "null") {
                //     this->zk_errorfile = nullptr;
                // } else {
                //     this->zk_errorfile = fopen(token.c_str(), "w");
                // }

                this->zk_errorfile = token == "null" ? nullptr : fopen(token.c_str(), "w");
            }

            #endif
             

            is_stream.close();

        } catch(std::exception e) {
            throw std::runtime_error(e.what());
        }
    
    }

    int getNumServers() {
        return num_servers;
    }

    uintptr_t getStartAddress() {
        return start_address;
    }

    uintptr_t getEndAddress() {
        return end_address;
    }

    #if FAULT_TOLERANT
    std::string getZKServerList() {
        return zookeeper_servers;
    }

    int getZKHeartbeatTimeout() {
        return heartbeat;
    }

    FILE* getZKLogFileHandle() {
        return zk_errorfile;
    }
    
    #endif

private:
    std::string path;
    int num_servers;
    std::unordered_map<int, RNode*> map;

    std::string zookeeper_servers;
    int heartbeat; // in milliseconds
    FILE *zk_errorfile;

    /*
        region of address that is shared
    */
    // possibly make these const
    uintptr_t start_address = ALLOCATABLE_RANGE_START;
    uintptr_t end_address = ALLOCATABLE_RANGE_END;

};

#if FAULT_TOLERANT

std::map<std::string, std::string> ZStoMAP(std::string input) {
    int len = input.size();
    int start = 0;
    int end = 0;
    std::map<std::string, std::string> res;
    int i = 0;
    while(i < len) {
        //find a key
        std::string key;
        start = i;
        while(input.at(i) != ',') {
            i++;
        }
        end = i;

        key = input.substr(start, end - start);
        i++;

        //find the value
        std::string val;
        start = i;
        while(input.at(i) != ',') {
            i++;
        }
        end = i;
        val = input.substr(start, end - start);
        i++;

        //add to map
        res[key] = val;
    }
    return res;
}

std::string MAPtoZS(std::map<std::string, std::string> map) {
    std::string res;
    for(auto it = map.begin(); it != map.end(); it++) {
        res.append(it->first.c_str());
        res.append(",");
        res.append(it->second.c_str());
        res.append(",");
    }
    return res;
}

std::vector<int64_t> ZStoProcessList(std::string str) {
    std::vector<int64_t> res;
    int start = 0;
    int end = 0;
    int i = 0;
    int len = str.size();

    while( i < len) {
        std::string id;
        start = i;
        while(str.at(i) != ',') {
            i++;
        }
        end = i;
        i++;
        id = str.substr(start, end - start);
        res.push_back(atol(id.c_str()));
    }

    return res;
}

std::string ProcessListtoZS(std::vector<int64_t> vec) {
    std::string res;
    for(auto it = vec.begin(); it != vec.end(); it++) {
        res.append(std::to_string(*it));
        res.append(",");
    }
    return res;
}

#endif //FAULT_TOLERANT
#endif // __MISCUTILS_HPP__
