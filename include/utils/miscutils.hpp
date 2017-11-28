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

#define PAGING 1
#define ASCII_STARS "**********************************************************************"
#define DEBUG 0
#define LEVEL 0
#define LogMessage(Level, SEVERITY, ...) do { if (DEBUG && Level >= LEVEL) {fprintf(stderr, "%s %s:%03u in %s : ", SEVERITY, __FILE__, __LINE__, __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr);}if(Level == 0) exit(1);} while(false)

#define LogError(...) LogMessage(0, "ERROR", __VA_ARGS__)
#define LogInfo(...) LogMessage(3, "INFO", __VA_ARGS__)
#define LogWarning(...) LogMessage(2, "WARNING", __VA_ARGS__)
#define LogAssertionError(...) LogMessage(0, "ASSERT ERROR", __VA_ARGS__);
#define LogAssert(COND, ...) if (!(COND)) { LogAssertionError(__VA_ARGS__) }

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



class TestTimer {
public:
    TestTimer() : started(false), stopped(false) {}

    void start() {
        if (started) {
            throw std::logic_error("start() called on already started timer!");
        }
        int error = gettimeofday(&begin_st, NULL);
        if (error) {
            throw std::runtime_error("gettimeofday failed!");
        }
        started = true;
    }
    void stop() {
        if (stopped or not started) {
            throw std::logic_error("stop() called on a stopped or not-started timer!");
        }
        int error = gettimeofday(&end_st, NULL);
        if (error) {
            throw std::runtime_error("gettimeofday failed!");
        }
        stopped = true;
    }
    double get_duration_usec() {
        double elapsed = \
            (end_st.tv_usec - begin_st.tv_usec)
            + ((end_st.tv_sec - begin_st.tv_sec) * 1000000.0);
        return elapsed;
    }

    void reset() {
        started = false;
        stopped = false;
    }

private:
    bool started;
    bool stopped;
    struct timeval begin_st;
    struct timeval end_st;

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
    ~ConfigParser(){};

    RNode* getNode(int id) {
        return this->map.at(id);
    }

    void parse(std::string path) {
        
        std::fstream is_stream(path);

        std::string line;
        try {
            if(!is_stream)
                LogError("file error");

            // if(!std::getline(is_stream, line))
            //     LogError("file error");

            
            /*
                can potentially add user inputted addresses as values for start_address and end_address
            */

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

private:
    std::string path;
    int num_servers;
    std::unordered_map<int, RNode*> map;
    /*
        region of address that is shared
    */
    // possibly make these const
    uintptr_t start_address = ALLOCATABLE_RANGE_START;
    uintptr_t end_address = ALLOCATABLE_RANGE_END;

};

#endif // __MISCUTILS_HPP__
