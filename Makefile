CXX=g++
CFLAGS=-std=c++14 -g
LIBS=-lcrypto
VPATH=src
OBJ = aes.o \
arm.o \
clock.o \
cp15.o \
dmac.o \
gpio.o \
iphone2g.o \
iphone3gs.o \
main.o \
power.o \
sha1.o \
spi.o \
timer.o \
usb_otg.o \
usb_phy.o \
vic.o \
wdt.o

all: $(OBJ)
	$(CXX) $(CFLAGS) $(OBJ) -o purplesapphire $(LIBS)

clean:
	rm *.o && rm purplesapphire

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $<