.PHONY: clean

CXX = g++
CXXFLAGS := -Wall -g -rdynamic -std=c++11 -MMD -I../../include/ -I../../src/
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread -lzookeeper_mt
# -std=c++11: Compile with the C++11 standard.
# -MMD: Autogenerate dependency files (.d).
APPS := test_RDMAMemoryNode test_RDMAMemoryTransfer
DEPENDS = test_RDMAMemoryNode.d test_RDMAMemoryTransfer.d

all: ${APPS}

test_RDMAMemoryNode: test_RDMAMemoryNode.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

# test_RDMAMemory: test_RDMAMemory.o
# 	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

test_RDMAMemoryTransfer: test_RDMAMemoryTransfer.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

-include ${DEPENDS}

clean:
	rm ${APPS} *.o core ${DEPENDS}
