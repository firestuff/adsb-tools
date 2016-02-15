CC ?= clang
CFLAGS ?= -Wall -Werror -O4 -g --std=gnu11 --pedantic-errors
LDFLAGS ?= -Wall -O4

all: adsbus

clean:
	rm -f *.o adsbus

%.o: %.c *.h
	$(CC) -c $(CFLAGS) $< -o $@

adsbus: adsbus.o backend.o airspy_adsb.o common.o
	$(CC) $(LDFLAGS) -o adsbus adsbus.o backend.o airspy_adsb.o common.o
