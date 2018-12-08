CXX = g++
CXXFLAGS = -O3 -Wall

SOURCES = LZSS.cpp Sav2Cart.cpp

OBJECTS = LZSS.o Sav2Cart.o

all: sav2cart

sav2cart: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o sav2cart $(OBJECTS)

.PHONY: clean

clean:
	rm -f $(OBJECTS)
