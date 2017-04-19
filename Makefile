CXXFLAGS := -c -g -Wall -ggdb3 -std=gnu++11 -Ofast
LDFLAGS := -g

FJSON_SOURCES := fjson/fjson.cpp
SOURCES := backend.cpp

all: tasker test fjson-test fjson-example

FJSON_OBJECTS := $(FJSON_SOURCES:.cpp=.o)
OBJECTS := $(SOURCES:.cpp=.o) $(FJSON_OBJECTS)
override CPPFLAGS += -MMD

-include $(subst .cpp,.d, $(SOURCES))
-include $(subst .cpp,.d, $(FJSON_SOURCES))

test: tests.o $(OBJECTS)
	$(CXX) $(LDFLAGS) tests.o $(OBJECTS) -o $@

fjson-test: fjson/tests.o $(FJSON_OBJECTS)
	$(CXX) $(LDFLAGS) fjson/tests.o $(FJSON_OBJECTS) -o $@

fjson-example: fjson/example.o $(FJSON_OBJECTS)
	$(CXX) $(LDFLAGS) fjson/example.o $(FJSON_OBJECTS) -o $@

tasker: cli.o $(OBJECTS)
	$(CXX) $(LDFLAGS) cli.o $(OBJECTS) -o $@

clean:
	rm -f test tasker *.o *.d fjson/*.o fjson/*.d
