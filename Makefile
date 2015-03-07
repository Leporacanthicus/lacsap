OBJECTS = lexer.o token.o expr.o parser.o types.o constants.o builtin.o binary.o lacsap.o \
	  namedobject.o semantics.o trace.o

LLVM_DIR = /usr/local/llvm-debug
#LLVM_DIR = /usr/local

CXX  = clang++
CC  = clang
LD  = ${CXX}


CXXFLAGS  = -g -Wall -Werror -Wextra -Wno-unused-private-field -std=c++11 -O0
CXXFLAGS += -fstandalone-debug -fno-exceptions -fno-rtti
CXXFLAGS += -Qunused-arguments
CXXFLAGS += `${LLVM_DIR}/bin/llvm-config --cxxflags`
#CXX_EXTRA = --analyze

LDFLAGS  = -g -rdynamic -fstandalone-debug
LDFLAGS += `${LLVM_DIR}/bin/llvm-config --ldflags`

LLVMLIBS = `${LLVM_DIR}/bin/llvm-config --libs` -lz

OTHERLIBS = -lpthread -ldl -lcurses

LIBS = ${LLVMLIBS} ${OTHERLIBS}

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})


all: lacsap .depends tests runtime_lib

.cpp.o: 
	${CXX} ${CXXFLAGS} ${CXX_EXTRA} -c -o $@ $<

lacsap: ${OBJECTS} .depends
	${LD} ${LDFLAGS} -o $@ ${OBJECTS} ${LIBS}

.phony: tests
tests:
	${MAKE} -C test

.phony: runtime_lib
runtime_lib:
	${MAKE} -C runtime

.phony: runtests 
runtests: lacsap tests
	${MAKE} -C test runtests

clean:
	rm -f ${OBJECTS}
	make -C test clean
	make -C runtime clean


include .depends

.depends: Makefile ${SOURCES}
	${CXX} -MM ${CXXFLAGS} ${SOURCES} > $@
