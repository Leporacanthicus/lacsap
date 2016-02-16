all: testrunner

OBJECTS = testrunner.o

LD  = ${CXX}

CXXFLAGS  = -g -Wall -Werror -Wextra -Wno-unused-private-field -std=c++11
ifeq (${M32}, 0)
  CXXFLAGS += -DM32_DISABLE=1
endif

SOURCES = $(patsubst %.o,%.cpp,${OBJECTS})

testrunner: ${OBJECTS}
	${LD} ${LDFLAGS} -o $@ ${OBJECTS}

fulltests: testrunner
	./testrunner

fasttests: testrunner
	./testrunner -O1

debugtests: testrunner
	./testrunner -g

clean:
	rm -f ${OBJECTS}
