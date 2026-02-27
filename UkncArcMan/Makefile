# CXX := g++-mp-8
CXXFLAGS = -std=c++14 -O3 -Wall

SOURCES = main.cpp rad50.cpp rt11date.cpp format_fcu.cpp format_lzslza.cpp fcu_lzhuf.cpp lzslza_lzss.cpp
HEADERS = main.h rt11date.h

OBJECTS = main.o rad50.o rt11date.o format_fcu.o format_lzslza.o fcu_lzhuf.o lzslza_lzss.o

all: ukncarcman

ukncarcman: $(OBJECTS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o ukncarcman $(OBJECTS)

.PHONY: clean

.cpp.o:	$(HEADERS)
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJECTS)
