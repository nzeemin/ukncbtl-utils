CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall

SOURCES = aout2sav.cpp

OBJECTS = aout2sav.o

all: sav2cart

sav2cart: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o aout2sav $(OBJECTS)

.PHONY: clean

clean:
	rm -f $(OBJECTS)
