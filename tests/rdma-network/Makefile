.PHONY: clean

CXX := g++
CXXFLAGS := -Wall -g -std=c++11 -MMD -I../../include -I../../include -I../../src/
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread -lzookeeper_mt

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

