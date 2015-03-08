all: testrunner

OBJECTS = testrunner.o

CXX = clang++
CC  = clang
LD  = clang++

LLVM_DIR = /usr/local/llvm-debug

CXXFLAGS  = -g -Wall -Werror -Wextra -Wno-unused-private-field -std=c++11

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})

testrunner: ${OBJECTS}
	${LD} ${LDFLAGS} -o $@ ${OBJECTS}

libruntime.a: ../libruntime.a
	cp $< .

fulltests: testrunner libruntime.a
	./testrunner

fasttests: testrunner libruntime.a
	./testrunner -O1

clean:
	rm -f ${OBJECTS}
