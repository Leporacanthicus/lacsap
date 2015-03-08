CFLAGS    = -g -Wall -Werror -Wextra -std=c99 -O2
CC  = clang

OBJECTS = runtime.o

RUNTIME_LIB = libruntime.a

RUNTIME=../${RUNTIME_LIB}


all: ${RUNTIME}
${RUNTIME}: ${RUNTIME_LIB}
	cp $< $@

${RUNTIME_LIB} : ${OBJECTS}
	ar r $@ ${OBJECTS}

%c.%o:
	${CC} ${CFLAGS} -fPIC -c $< -o $@

clean:
	rm runtime.o
