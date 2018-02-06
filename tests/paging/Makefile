.PHONY: clean

CXX = g++
CXXFLAGS := -Wall -g -rdynamic -std=c++11 -MMD -I../../include/ -I../../src/

LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread -lzookeeper_mt
# -std=c++11: Compile with the C++11 standard.
# -MMD: Autogenerate dependency files (.d).
APPS := testPaging
DEPENDS = testPaging.d

all: ${APPS}

testPaging: testPaging.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

-include ${DEPENDS}

clean:
	rm ${APPS} *.o core ${DEPENDS}
