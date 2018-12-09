
CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall

SRCZLIB = zlib/adler32.c zlib/compress.c zlib/crc32.c zlib/deflate.c zlib/gzclose.c zlib/gzlib.c zlib/gzread.c zlib/gzwrite.c \
          zlib/infback.c zlib/inffast.c zlib/inflate.c zlib/inftrees.c zlib/trees.c zlib/uncompr.c zlib/zutil.c
SOURCES = Drivers.cpp ESCParser.cpp Interpreter.cpp RobotronFont.cpp $(SRCZLIB)

OBJZLIB = $(SRCZLIB:.c=.o)
OBJECTS = Drivers.o ESCParser.o Interpreter.o RobotronFont.o $(OBJZLIB)

all: ESCParser

ESCParser: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o ESCParser $(OBJECTS)

.PHONY: clean

clean:
	rm -f $(OBJECTS)
