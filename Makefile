OBJECTS = lexer.o source.o location.o token.o expr.o parser.o types.o constants.o builtin.o \
	  binary.o lacsap.o namedobject.o semantics.o trace.o stack.o utils.o callgraph.o

LLVM_DIR ?= /usr/local
#LLVM_DIR = ../llvm-github/LLVM_Binaries

# If not specified, use clang and enable 32-bit build - debug enabled
USECLANG ?= 1
M32 ?= 1
NDEBUG ?= 0

ifeq (${USECLANG}, 1)
  CC = clang
  CXX = clang++
endif

LD = ${CXX}

ifeq (${NDEBUG}, 0)
  DEBUG ?= -g -O0
else
  DEBUG ?= -DNDEBUG=1 -O2
endif

CXXFLAGS  = ${DEBUG} -Wall -Werror -Wextra -Wno-unused-parameter -std=c++17
CXXFLAGS += $(shell ${LLVM_DIR}/bin/llvm-config --cxxflags)
CXXFLAGS += -fno-exceptions -fno-rtti
ifeq (${CC},clang)
  CXXFLAGS += -Qunused-arguments -fstandalone-debug
endif
ifeq (${M32}, 0)
  CXXFLAGS += -DM32_DISABLE=1
endif

#CXX_EXTRA = --analyze

LDFLAGS  = -g -rdynamic

ifeq (${CC},clang)
  LDFLAGS += -fstandalone-debug -fuse-ld=lld
endif
LDFLAGS += $(shell ${LLVM_DIR}/bin/llvm-config --ldflags)
LLVMLIBS  = $(shell ${LLVM_DIR}/bin/llvm-config --libs)
LLVMLIBS += $(shell ${LLVM_DIR}/bin/llvm-config --system-libs)

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})

all: lacsap .depends tests runtime_lib llvmversion

.cpp.o:
	${CXX} ${CXXFLAGS} ${CXX_EXTRA} -c -o $@ $<

lacsap: ${OBJECTS} .depends
	${LD} ${LDFLAGS} -o $@ ${OBJECTS} ${LLVMLIBS}

.phony: tests
tests: runtime_lib
	${MAKE} -C test CC=${CC} CXX=${CXX} M32=${M32}

.phony: runtime_lib
runtime_lib:
	${MAKE} -C runtime CC=${CC} M32=${M32}

.phony: runtests
runtests: fulltests

.phony: fulltests 
fulltests: lacsap tests
	${MAKE} -C test fulltests M32=${M32}

.phony: fasttests 
fasttests: lacsap tests
	${MAKE} -C test fasttests M32=${M32}

.phony: debugtests
debugtests: lacsap tests
	${MAKE} -C test debugtests M32=${M32}


.phony: llvmversion
llvmversion:
	./llvm_version_info.sh > $@

clean:
	rm -f ${OBJECTS} libruntime.a llvmversion
	make -C test clean
	make -C runtime clean .depends

include .depends

.depends: Makefile ${SOURCES}
	${CXX} -MM ${CXXFLAGS} ${SOURCES} > $@
