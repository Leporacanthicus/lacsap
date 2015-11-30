CFLAGS    = -g -Wall -Werror -Wextra -std=c99 -O2
#CFLAGS    = -g -Wall -Werror -Wextra -std=c99 -O0

OBJECTS = main.o math.o fileio.o write.o read.o readbin.o writebin.o alloc.o set.o string.o array.o panic.o \
          clock.o rangeerror.o assign.o getput.o params.o
OBJECTS32 = main.o32 math.o32 fileio.o32 write.o32 read.o32 readbin.o32 writebin.o32 alloc.o32 set.o32 \
	   string.o32 array.o32 panic.o32 clock.o32 rangeerror.o32 assign.o32 getput.o32 params.o32
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

%c.%o:
	${CC} ${CFLAGS} -fPIC -c $< -o $@

.c.o32:
	${CC} ${CFLAGS} -fPIC -m32 -c $< -o $@

clean:
	rm -f ${OBJECTS} ${OBJECTS32} ${RUNTIME_LIB}  ${RUNTIME_LIB32}

include .depends

.depends: Makefile ${SOURCES}
	${CC} -MM ${CFLAGS} ${SOURCES} > $@
