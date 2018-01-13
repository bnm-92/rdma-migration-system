.PHONY: clean

CXX = g++
CXXFLAGS := -Wall -g -rdynamic -std=c++11 -MMD -I../../include/ -I../../src

LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread -lzookeeper_mt
# -std=c++11: Compile with the C++11 standard.
# -MMD: Autogenerate dependency files (.d).
APPS := test_connect
DEPENDS = test_connect.d

all: ${APPS}

test_connect: test_connect.o 
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

# zookeeper.o:
# 	${CXX} -o $@ ${CXXFLAGS} -c ../../src/zookeeper/zookeeper.cpp ${LDFLAGS} 

-include ${DEPENDS}

clean:
	rm ${APPS} *.o core ${DEPENDS}
