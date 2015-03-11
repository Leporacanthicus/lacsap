CFLAGS    = -g -Wall -Werror -Wextra -std=c99 -O2
#CFLAGS    = -g -Wall -Werror -Wextra -std=c99 -O0
CC  = clang

OBJECTS = main.o math.o fileio.o write.o read.o readbin.o writebin.o alloc.o set.o string.o array.o panic.o \
          clock.o rangeerror.o assign.o getput.o params.o
SOURCES = $(patsubst %.o,%.c,${OBJECTS})

RUNTIME_LIB = libruntime.a

RUNTIME=../${RUNTIME_LIB}


all: ${RUNTIME}
${RUNTIME} : ${RUNTIME_LIB}
	cp $< $@

${RUNTIME_LIB} : ${OBJECTS}
	ar r $@ ${OBJECTS}

%c.%o:
	${CC} ${CFLAGS} -fPIC -c $< -o $@

clean:
	rm -f ${OBJECTS} ${RUNTIME_LIB}

include .depends

.depends: Makefile ${SOURCES}
	${CC} -MM ${CFLAGS} ${SOURCES} > $@
