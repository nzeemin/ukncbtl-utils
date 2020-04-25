CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall -Ilzsa

SOURCES = Loaders.cpp LZSS.cpp LZ4.cpp
SOURCES += lzsa/divsufsort.c lzsa/frame.c lzsa/sssort.c lzsa/trsort.c lzsa/expand_block_v1.c lzsa/expand_block_v2.c lzsa/shrink_block_v1.c lzsa/shrink_block_v2.c
OBJECTS += lzsa/matchfinder.c lzsa/expand_inmem.c lzsa/shrink_inmem.c lzsa/shrink_context.c lzsa/expand_context.c
SOURCES += Sav2Cart.cpp

OBJECTS = Loaders.o LZSS.o LZ4.o
OBJECTS += lzsa/divsufsort.o lzsa/frame.o lzsa/sssort.o lzsa/trsort.o lzsa/expand_block_v1.o lzsa/expand_block_v2.o lzsa/shrink_block_v1.o lzsa/shrink_block_v2.o
OBJECTS += lzsa/matchfinder.o lzsa/expand_inmem.o lzsa/shrink_inmem.o lzsa/shrink_context.o lzsa/expand_context.o
OBJECTS += Sav2Cart.o

all: sav2cart

sav2cart: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o sav2cart $(OBJECTS)

.PHONY: clean

clean:
	rm -f $(OBJECTS)
