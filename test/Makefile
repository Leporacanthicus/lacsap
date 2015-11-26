all: testrunner

OBJECTS = testrunner.o

LD  = ${CXX}

LLVM_DIR = /usr/local/llvm-debug

CXXFLAGS  = -g -Wall -Werror -Wextra -Wno-unused-private-field -std=c++11

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})

testrunner: ${OBJECTS}
	${LD} ${LDFLAGS} -o $@ ${OBJECTS}

fulltests: testrunner
	./testrunner

fasttests: testrunner
	./testrunner -O1

clean:
	rm -f ${OBJECTS}
