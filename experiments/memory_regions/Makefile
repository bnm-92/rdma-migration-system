.PHONY: clean

CXX = g++
CXXFLAGS := -Wall -g -rdynamic -std=c++11 -MMD -I../../include/utils -I../../include/rdma-network -I../../src/rdma-network -I../../include/distributed-allocator -I../../src/distributed-allocator -I../../include/c++-containers -I../../src/c++-containers -I../../include/paging -I../../src/paging

LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread
APPS := expRegionTransfer
DEPENDS = expRegionTransfer.d

all: ${APPS}

expRegionTransfer: expRegionTransfer.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

-include ${DEPENDS}

clean:
	rm ${APPS} *.o core ${DEPENDS}
