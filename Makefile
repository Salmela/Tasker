CXXFLAGS := -c -g -Wall -ggdb3
LDFLAGS := -g

SOURCES := backend.cpp

all: tasker test

OBJECTS := $(SOURCES:.cpp=.o)
override CPPFLAGS += -MMD

-include $(subst .cpp,.d, $(SOURCES))

test: tests.o $(OBJECTS)
	$(CXX) $(LDFLAGS) tests.o $(OBJECTS) -o $@

tasker: main.o cli.o $(OBJECTS)
	$(CXX) $(LDFLAGS) main.o cli.o $(OBJECTS) -o $@

clean:
	rm -f test tasker *.o *.d
