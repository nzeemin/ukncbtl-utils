CXX = g++
CXXFLAGS = -O3 -Wall

SOURCES = Sav2Wav.cpp

OBJECTS = Sav2Wav.o

all: sav2cart

sav2cart: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o sav2wav $(OBJECTS)

.PHONY: clean

clean:
	rm -f $(OBJECTS)
