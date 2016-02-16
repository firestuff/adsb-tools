CC ?= clang
CFLAGS ?= -Wall -Werror -O4 -g --std=gnu11 --pedantic-errors
LDFLAGS ?= -Wall -O4
LIBS ?= -luuid

all: adsbus

clean:
	rm -f *.o adsbus

%.o: %.c *.h
	$(CC) -c $(CFLAGS) $< -o $@

adsbus: adsbus.o backend.o client.o airspy_adsb.o json.o common.o
	$(CC) $(LDFLAGS) -o adsbus adsbus.o backend.o client.o airspy_adsb.o json.o common.o $(LIBS)
