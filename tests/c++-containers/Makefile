CXX = g++
CXXFLAGS = -g -std=c++11 -MMD -Wall -lrdmacm -libverbs -lpthread -lzookeeper_mt -I../../include/  -I../../src/ 
# -std=c++11: Compile with the C++11 standard.
# -MMD: Autogenerate dependency files (.d).
EXEC = exec
OBJECTS = main.o
DEPENDS = ${OBJECTS:.o=.d}

${EXEC}: ${OBJECTS}
	${CXX} ${CXXFLAGS} ${OBJECTS} -o ${EXEC}

-include ${DEPENDS}

.PHONY: clean

clean:
	rm ${EXEC} ${OBJECTS} ${DEPENDS}
