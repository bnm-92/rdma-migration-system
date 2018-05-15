.PHONY: clean
# extensions to clean 
CLEANEXTS = o a d

# CPP and LD FLAGS
CXX = g++
CXXFLAGS := -Wall -g -std=c++11 -MMD -I./include/ -I./src/ -lrdmacm -libverbs -lpthread #-lzookeeper_mt 
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread #-lzookeeper_mt

NETWORK_INCLUDE_DIR = include/rdma-network
NETWORK_DIR = src/rdma-network
NETWORK_HDR = $(NETWORK_INCLUDE_DIR)/util.hpp $(NETWORK_INCLUDE_DIR)/rdma_server_prototype.hpp $(wildcard $(NETWORK_INCLUDE_DIR)/*.hpp)
NETWORK_SRC = $(NETWORK_DIR)/util.cpp $(NETWORK_DIR)/rdma_server_prototype.cpp $(wildcard $(NETWORK_DIR)/*.cpp)
NETWORK_O = $(NETWORK_SRC:.cpp=.o)

# RAMP_INCLUDE_DIR = include/distributed-allocator
# RAMP_DIR = src/distributed-allocator
# RAMP_HDR = $(wildcard $(RAMP_INCLUDE_DIR)/*.hpp)
# RAMP_SRC = $(wildcard $(RAMP_DIR)/*.cpp)
# RAMP_O = $(RAMP_SRC:.cpp=.o)

# RAMP_P_INCLUDE_DIR = include/paging
# RAMP_P_DIR = src/paging
# RAMP_P_HDR = $(wildcard $(RAMP_P_INCLUDE_DIR)/*.hpp)
# RAMP_P_SRC = $(wildcard $(RAMP_P_DIR)/*.cpp)
# RAMP_P_O = $(RAMP_P_SRC:.cpp=.o)

# CONTAINER_INCLUDE_DIR = include/c++-containers
# CONTAINER_DIR = src/c++-containers
# CONTAINER_HDR = $(wildcard $(CONTAINER_INCLUDE_DIR)/*.hpp)
# CONTAINER_SRC = $(wildcard $(CONTAINER_DIR)/*.cpp)
# CONTAINER_O = $(CONTAINER_SRC:.cpp=.o)

# ZOOKEEPER_INCLUDE_DIR = include/zookeeper
# ZOOKEEPER_DIR = src/zookeeper
# ZOOKEEPER_HDR = $(wildcard $(ZOOKEEPER_INCLUDE_DIR)/*.hpp)
# ZOOKEEPER_SRC = $(wildcard $(ZOOKEEPER_DIR)/*.cpp)
# ZOOKEEPER_O = $(ZOOKEEPER_SRC:.cpp=.o)

OUTPUTFILE = libramp.a
# build the lib
# $(OUTPUTFILE): $(NETWORK_O) $(RAMP_O) $(RAMP_P_O) $(ZOOKEEPER_O) $(CONTAINER_O)
# 	ar ru $@ $^
# 	ranlib $@

$(OUTPUTFILE): $(NETWORK_O)
	ar rcs $@ $^
	# ranlib $@

$(NETWORK_DIR)/%.o: $(NETWORK_HDR)
# $(ZOOKEEPER_DIR)/%.o: $(ZOOKEEPER_HDR)
# $(RAMP_DIR)/%.o: $(NETWORK_HDR) $(RAMP_HDR)
# $(RAMP_P_DIR)/%.o: $(NETWORK_HDR) $(RAMP_HDR) $(RAMP_P_HDR)
# $(CONTAINER_DIR)/%.o: $(NETWORK_HDR) $(RAMP_HDR) $(RAMP_P_HDR) $(CONTAINER_HDR)

clean:
	for file in $(CLEANEXTS); do rm -f *.$$file; done
	for file in $(CLEANEXTS); do rm -f $(NETWORK_DIR)/*.$$file; done
	# for file in $(CLEANEXTS); do rm -f $(RAMP_DIR)/*.$$file; done
	# for file in $(CLEANEXTS); do rm -f $(RAMP_P_DIR)/*.$$file; done
	# for file in $(CLEANEXTS); do rm -f $(CONTAINER_DIR)/*.$$file; done

all: libramp.a