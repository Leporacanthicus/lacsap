OBJECTS = lexer.o token.o expr.o parser.o types.o constants.o builtin.o binary.o lacsap.o \
	  namedobject.o semantics.o trace.o stack.o

#LLVM_DIR = /usr/local/llvm-only
LLVM_DIR ?= /usr/local/llvm-debug

#For now at least.
USECLANG=1

ifdef USECLANG
  CC = ${LLVM_DIR}/bin/clang
  CXX = ${LLVM_DIR}/bin/clang++
endif

LD = ${CXX}

CXXFLAGS  = -g -Wall -Werror -Wextra -std=c++11 -O0
CXXFLAGS += -fno-exceptions -fno-rtti
ifeq (${CC},clang)
  CXXFLAGS += -Qunused-arguments -fstandalone-debug
endif
CXXFLAGS += `${LLVM_DIR}/bin/llvm-config --cxxflags`
#CXX_EXTRA = --analyze

LDFLAGS  = -g -rdynamic

ifeq (${CC},clang)
LDFLAGS += -fstandalone-debug
endif
LDFLAGS += `${LLVM_DIR}/bin/llvm-config --ldflags`
LLVMLIBS  = `${LLVM_DIR}/bin/llvm-config --libs`
LLVMLIBS += `${LLVM_DIR}/bin/llvm-config --system-libs`

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})

all: lacsap .depends tests runtime_lib

.cpp.o:
	${CXX} ${CXXFLAGS} ${CXX_EXTRA} -c -o $@ $<

lacsap: ${OBJECTS} .depends
	${LD} ${LDFLAGS} -o $@ ${OBJECTS} ${LLVMLIBS}

.phony: tests
tests: runtime_lib
	${MAKE} -C test CC=${CC} CXX=${CXX}

.phony: runtime_lib
runtime_lib:
	${MAKE} -C runtime CC=${CC}

.phony: runtests
runtests: fulltests

.phony: fulltests 
fulltests: lacsap tests
	${MAKE} -C test fulltests

.phony: fasttests 
fasttests: lacsap tests
	${MAKE} -C test fasttests

llvmversion:
	${LLVM_DIR}/bin/clang --version | head -1 | \
	awk -e '{ print "git clone " substr($$6, 2) " llvm && cd llvm && git checkout " substr($$7, 0, length($$7)-1); }' > $@

.phony: llvmversion


clean:
	rm -f ${OBJECTS} libruntime.a llvmversion
	make -C test clean
	make -C runtime clean .depends

include .depends

.depends: Makefile ${SOURCES}
	${CXX} -MM ${CXXFLAGS} ${SOURCES} > $@
