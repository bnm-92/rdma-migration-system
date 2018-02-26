CXX = g++
CXXFLAGS = -g -std=c++11 -MMD -Wall -lrdmacm -libverbs -lpthread -lzookeeper_mt -I../../include -I../../src
# -std=c++11: Compile with the C++11 standard.
# -MMD: Autogenerate dependency files (.d).
APPS = baseline data_transfer
DEPENDS = baseline.d data_transfer.d

all: ${APPS}

baseline: baseline.o
	${CXX} -o $@ $^ ${CXXFLAGS}

data_transfer: data_transfer.o
	${CXX} -o $@ $^ ${CXXFLAGS}

-include ${DEPENDS}

.PHONY: clean

clean:
	rm ${APPS} *.o core ${DEPENDS}
