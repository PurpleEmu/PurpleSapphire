CXX=g++
CFLAGS=-std=c++14 -g
LIBS=-lmingw32 -mwindows
VPATH=src
OBJ = iphone2g.o main.o

all: $(OBJ)
	$(CXX) $(CFLAGS) $(OBJ) -o purplesapphire $(LIBS)

clean:
	rm o && rm purplesapphire

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $<