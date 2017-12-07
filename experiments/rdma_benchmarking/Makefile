.PHONY: clean

CXX = g++
CXXFLAGS := -Wall -g -rdynamic -std=c++11 -MMD -I../../include/utils -I../../include/rdma-network -I../../src/rdma-network

LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread
# -std=c++11: Compile with the C++11 standard.
# -MMD: Autogenerate dependency files (.d).
APPS := memory_pinning 
DEPENDS = memory_pinning.d 

all: ${APPS}

memory_pinning: memory_pinning.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

-include ${DEPENDS}

clean:
	rm ${APPS} *.o core ${DEPENDS}
