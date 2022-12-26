CFLAGS    = -g -Wall -Werror -Wextra -std=c11 -O2
#CFLAGS    = -g -Wall -Werror -Wextra -std=c11 -O0

OBJECTS = main.o math.o fileio.o write.o read.o readbin.o writebin.o alloc.o set.o string.o array.o panic.o \
          clock.o rangeerror.o assign.o getput.o params.o val.o gettimestamp.o bind.o seek.o cmath.o
OBJECTS32 = $(patsubst %.o,%.o32,${OBJECTS})
SOURCES = $(patsubst %.o,%.c,${OBJECTS})

.SUFFIXES: .o32
RUNTIME_LIB = libruntime.a
RUNTIME_LIB32 = libruntime-m32.a
LIBS = ${RUNTIME_LIB}
RUNTIME=../${RUNTIME_LIB} 

ifeq (${M32}, 1)
  RUNTIME += ../${RUNTIME_LIB32}
  LIBS += ${RUNTIME_LIB32}
endif

all: ${RUNTIME}

${RUNTIME} : ${LIBS}
	cp $^ ..

${RUNTIME_LIB} : ${OBJECTS}
	ar r $@ ${OBJECTS}

${RUNTIME_LIB32} : ${OBJECTS32}
	ar r $@ ${OBJECTS32}

.c.o:
	${CC} ${CFLAGS} -fPIC -c $< -o $@

.c.o32:
	${CC} ${CFLAGS} -fPIC -m32 -c $< -o $@

clean:
	rm -f ${OBJECTS} ${OBJECTS32} ${RUNTIME_LIB}  ${RUNTIME_LIB32}

-include .depends
-include .depends32

.depends: Makefile ${SOURCES}
	${CC} -MM ${CFLAGS} ${SOURCES} > $@
	sed $@ -e s/\\.o/\\.o32/g > .depends32
