.PHONY: clean

CXX = g++
CXXFLAGS := -Wall -g -rdynamic -std=c++11 -MMD -I../../include/ -I../../src

LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread -lzookeeper_mt
# -std=c++11: Compile with the C++11 standard.
# -MMD: Autogenerate dependency files (.d).
APPS := test_basic test_zkconfig
DEPENDS = test_basic.d test_zkconfig.d

all: ${APPS}

test_basic: test_basic.o 
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

test_zkconfig: test_zkconfig.o 
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

-include ${DEPENDS}

clean:
	rm ${APPS} *.o core ${DEPENDS}
