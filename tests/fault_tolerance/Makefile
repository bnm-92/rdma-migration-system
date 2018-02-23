.PHONY: clean

CXX = g++
CXXFLAGS := -Wall -g -rdynamic -std=c++11 -MMD -I../../include/ -I../../src/
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread -lzookeeper_mt
# -std=c++11: Compile with the C++11 standard.
# -MMD: Autogenerate dependency files (.d).
APPS := basic_test migration_test
DEPENDS = basic_test.d migration_test.d

all: ${APPS}

migration_test: migration_test.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

basic_test: basic_test.o
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}


-include ${DEPENDS}

clean:
	rm ${APPS} *.o core ${DEPENDS}
