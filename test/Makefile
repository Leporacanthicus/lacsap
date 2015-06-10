all: testrunner

OBJECTS = testrunner.o

CXX = g++
LD  = g++

LLVM_DIR = /usr/local/llvm-debug

CXXFLAGS  = -g -Wall -Werror -Wextra -Wno-unused-private-field -std=c++11

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})

testrunner: ${OBJECTS}
	${LD} ${LDFLAGS} -o $@ ${OBJECTS}

libruntime.a: ../libruntime.a
	cp $< .

libruntime-m32.a: ../libruntime-m32.a
	cp $< .

fulltests: testrunner libruntime.a libruntime-m32.a
	./testrunner

fasttests: testrunner libruntime.a
	./testrunner -O1

clean:
	rm -f ${OBJECTS}
