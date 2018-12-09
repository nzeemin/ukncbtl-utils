CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall

SOURCES = diskimage.cpp hardimage.cpp rad50.cpp rt11dsk.cpp

OBJECTS = diskimage.o hardimage.o rad50.o rt11dsk.o

all: rt11dsk

rt11dsk: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o rt11dsk $(OBJECTS)

.PHONY: clean

clean:
	rm -f $(OBJECTS)
