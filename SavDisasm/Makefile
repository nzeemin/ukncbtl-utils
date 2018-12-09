CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall

SOURCES = Disasm.cpp SavDisasm.cpp

OBJECTS = Disasm.o SavDisasm.o

all: savdisasm

savdisasm: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o savdisasm $(OBJECTS)

.PHONY: clean

clean:
	rm -f $(OBJECTS)
