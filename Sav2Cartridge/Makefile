CXX = g++
CXXFLAGS = -O3 -Wall

SOURCES = Sav2Cart.cpp

OBJECTS = Sav2Cart.o

all: sav2cart

sav2cart: $(OBJECTS)
        $(CXX) $(CXXFLAGS) -o ESCParser $(OBJECTS)

.PHONY: clean

clean:
        rm -f $(OBJECTS)
