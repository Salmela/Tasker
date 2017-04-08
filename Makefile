CFLAGS := -c -g -Wall -rdynamic -D_DEBUG -ggdb3
LDFLAGS := -g -rdynamic

SOURCES := backend.cpp

all: tasker test

OBJECTS := $(SOURCES:.cpp=.o)
override CPPFLAGS += -MMD

-include $(subst .cpp,.d, $(SOURCES))

test: tests.o $(OBJECTS)
	$(CXX) $(LDFLAGS) tests.o $(OBJECTS) -o $@

tasker: main.o $(OBJECTS)
	$(CXX) $(LDFLAGS) main.o $(OBJECTS) -o $@

clean:
	rm -f test tasker *.o *.d
