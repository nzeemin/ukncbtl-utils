# CXX := g++-mp-8
CXXFLAGS = -std=c++11 -O3 -Wall

SOURCES = diskimage.cpp hardimage.cpp rad50.cpp rt11dsk.cpp rt11date.cpp hostfile.cpp
HEADERS = diskimage.h hardimage.h hostfile.h rt11date.h rt11dsk.h

OBJECTS = diskimage.o hardimage.o rad50.o rt11dsk.o rt11date.o hostfile.o

all: rt11dsk

rt11dsk: $(OBJECTS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o rt11dsk $(OBJECTS)

.PHONY: clean

.cpp.o:	$(HEADERS)
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJECTS)
