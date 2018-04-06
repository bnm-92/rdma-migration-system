.PHONY: clean

CXX = g++
CXXFLAGS := -Wall -g -rdynamic -std=c++11 -MMD -I../../include/ -I../../src/

LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread -lzookeeper_mt
APPS := expRegionTransfer
DEPENDS = expRegionTransfer.d

all: ${APPS}

expRegionTransfer: expRegionTransfer.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

-include ${DEPENDS}

clean:
	rm ${APPS} *.o core ${DEPENDS}
