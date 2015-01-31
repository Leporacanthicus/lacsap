OBJECTS = lexer.o token.o expr.o parser.o types.o constants.o builtin.o binary.o lacsap.o \
	  namedobject.o astvisitor.o trace.o

RUNTIME_OBJS = runtime.o

LLVM_DIR = /usr/local/llvm-debug
#LLVM_DIR = /usr/local/

CXX  = clang++
CC  = clang
LD  = ${CXX}


CFLAGS    = -g -Wall -Werror -Wextra -std=c99 -O2
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


all: lacsap .depends tests ${RUNTIME_OBJS}

.cpp.o: 
	${CXX} ${CXXFLAGS} ${CXX_EXTRA} -c -o $@ $<

lacsap: ${OBJECTS} .depends
	${LD} ${LDFLAGS} -o $@ ${OBJECTS} ${LIBS}

.phony: tests
tests:
	${MAKE} -C test

.phony: runtests 
runtests: lacsap tests
	${MAKE} -C test runtests

clean:
	rm -f ${OBJECTS} ${RUNTIME_OBJS}

runtime.o : runtime.c
	${CC} ${CFLAGS} -c $< -o $@

include .depends

.depends: Makefile ${SOURCES}
	${CXX} -MM ${CXXFLAGS} ${SOURCES} > $@
