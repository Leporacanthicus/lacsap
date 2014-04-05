all: lacsap .depends runtime.o tests

OBJECTS = lexer.o token.o expr.o parser.o types.o constants.o builtin.o binary.o lacsap.o namedobject.o

CXX = clang++
CC  = clang
LD  = clang++

LLVM_DIR = /usr/local/llvm-debug

CFLAGS    = -g -Wall -Werror -Wextra -std=c99
INCLUDES  = `${LLVM_DIR}/bin/llvm-config --includedir`
CXXFLAGS  = -g -Wall -Werror -Wextra -Wno-unused-private-field -std=c++11 -O0
CXXFLAGS += -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
CXXFLAGS += -I ${INCLUDES}
# llvm-config --cxxflags gives flags like -fno-exception, which doesn't work for this project.
#CXXFLAGS += `${LLVM_DIR}/bin/llvm-config --cxxflags`

LDFLAGS  = -g -rdynamic
LDFLAGS += `${LLVM_DIR}/bin/llvm-config --ldflags`

LLVMLIBS = `${LLVM_DIR}/bin/llvm-config --libs` -lz

OTHERLIBS = -lpthread -ldl -lcurses

LIBS = ${LLVMLIBS} ${OTHERLIBS}

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})

lacsap: ${OBJECTS} .depends
	${LD} ${LDFLAGS} -o $@ ${OBJECTS} ${LIBS}

.phony: tests
tests:
	${MAKE} -C test

.phony: runtests
runtests: 
	${MAKE} -C test runtests

clean:
	rm -f ${OBJECTS}

runtime.o : runtime.c
	echo ${CXXFLAGS}
	${CC} ${CFLAGS} -c $< -o $@

include .depends

.depends: Makefile ${SOURCES}
	${CXX} -MM ${CXXFLAGS} ${SOURCES} > $@
