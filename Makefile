CXX=g++
CFLAGS=-std=c++14 -g -msse2 -msse -march=native -maes
LIBS=
VPATH=src
OBJ = arm.o \
clock.o \
cp15.o \
gpio.o \
iphone2g.o \
iphone3gs.o \
main.o \
spi.o \
timer.o \
usb_phy.o \
vic.o \
wdt.o

all: $(OBJ)
	$(CXX) $(CFLAGS) $(OBJ) -o purplesapphire $(LIBS)

clean:
	rm *.o && rm purplesapphire

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $<