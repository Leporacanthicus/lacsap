all: lacsap .depends runtime.o

OBJECTS = lexer.o token.o expr.o parser.o types.o variables.o builtin.o binary.o lacsap.o 

CXX = clang++
LD = clang++

DEFINES = -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS

CXXFLAGS = -g -Wall -Werror -Wextra -Wno-unused-private-field -std=c++11 ${DEFINES}
LDFLAGS = -g -L/usr/local/llvm-debug/lib -rdynamic

LLVMLIBS = 	-lLLVMLTO -lLLVMObjCARCOpts -lLLVMLinker -lLLVMipo -lLLVMVectorize -lLLVMBitWriter \
		-lLLVMIRReader -lLLVMBitReader -lLLVMAsmParser -lLLVMTableGen -lLLVMDebugInfo \
		-lLLVMOption -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMSelectionDAG \
		-lLLVMAsmPrinter -lLLVMMCParser -lLLVMX86Desc -lLLVMX86Info -lLLVMX86AsmPrinter \
		-lLLVMX86Utils -lLLVMJIT -lLLVMLineEditor -lLLVMMCDisassembler -lLLVMInstrumentation \
		-lLLVMInterpreter -lLLVMCodeGen -lLLVMScalarOpts -lLLVMInstCombine -lLLVMTransformUtils \
		-lLLVMipa -lLLVMAnalysis -lLLVMMCJIT -lLLVMTarget -lLLVMRuntimeDyld -lLLVMExecutionEngine \
		-lLLVMMC -lLLVMObject -lLLVMCore -lLLVMSupport 

OTHERLIBS = -lpthread -ldl -lcurses

LIBS = ${LLVMLIBS} ${OTHERLIBS}

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})


lacsap: ${OBJECTS} .depends
	${LD} ${LDFLAGS} -o $@ ${OBJECTS} ${LIBS}

clean:
	rm -f ${OBJECTS}

include .depends

.depends: Makefile ${SOURCES}
	${CXX} -MM ${DEFINES} ${SOURCES} > $@
