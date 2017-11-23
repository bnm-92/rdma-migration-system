.PHONY: clean

CXX := g++
CXXFLAGS := -Wall -g -std=c++11 -MMD -I../../include/utils -I../../include/rdma-network
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread

APPS := test-server test-client test-transfer
DEPENDS := test-server.d test-client.d test-transfer.d

all: ${APPS}

test-server: test-server.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

test-client: test-client.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

test-transfer: test-transfer.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

-include ${DEPENDS}

clean:
	rm ${APPS} *.o *.d

