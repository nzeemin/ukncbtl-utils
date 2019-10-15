CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall

SOURCES = LZSS.cpp LZ4.cpp Sav2Cart.cpp

OBJECTS = LZSS.o LZ4.o Sav2Cart.o

all: sav2cart

sav2cart: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o sav2cart $(OBJECTS)

.PHONY: clean

clean:
	rm -f $(OBJECTS)
