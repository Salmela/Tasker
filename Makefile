CXXFLAGS := -c -g -Wall -ggdb3 -std=gnu++11
LDFLAGS := -g

SOURCES := backend.cpp fjson/fjson.cpp

all: tasker test fjson-test fjson-example

OBJECTS := $(SOURCES:.cpp=.o)
override CPPFLAGS += -MMD

-include $(subst .cpp,.d, $(SOURCES))

test: tests.o $(OBJECTS)
	$(CXX) $(LDFLAGS) tests.o $(OBJECTS) -o $@

fjson-test: fjson/tests.o $(OBJECTS)
	$(CXX) $(LDFLAGS) fjson/tests.o $(OBJECTS) -o $@

fjson-example: fjson/example.o $(OBJECTS)
	$(CXX) $(LDFLAGS) fjson/example.o $(OBJECTS) -o $@

tasker: cli.o $(OBJECTS)
	$(CXX) $(LDFLAGS) cli.o $(OBJECTS) -o $@

clean:
	rm -f test tasker *.o *.d
