CXXFLAGS := -c -g -Wall -ggdb3 -std=gnu++11
LDFLAGS := -g

SOURCES := backend.cpp

all: tasker test

OBJECTS := $(SOURCES:.cpp=.o)
override CPPFLAGS += -MMD

-include $(subst .cpp,.d, $(SOURCES))

test: tests.o $(OBJECTS)
	$(CXX) $(LDFLAGS) tests.o $(OBJECTS) -o $@

tasker: cli.o $(OBJECTS)
	$(CXX) $(LDFLAGS) cli.o $(OBJECTS) -o $@

clean:
	rm -f test tasker *.o *.d
