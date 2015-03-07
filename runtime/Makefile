all: runtime_lib

CFLAGS    = -g -Wall -Werror -Wextra -std=c99 -O2
CC  = clang

runtime_lib: runtime.o
	cp runtime.o ..

runtime.o : runtime.c
	${CC} ${CFLAGS} -fPIC -c $< -o $@

clean:
	rm runtime.o
