CC ?= clang
CFLAGS ?= -Wall -Werror -O4 -g --std=gnu11 --pedantic-errors -fPIC -pie
LDFLAGS ?= -Wall -O4 -fPIC -pie
LIBS ?= -luuid -ljansson

all: adsbus

clean:
	rm -f *.o adsbus

%.o: %.c *.h
	$(CC) -c $(CFLAGS) $< -o $@

adsbus: adsbus.o backend.o client.o airspy_adsb.o beast.o json.o stats.o common.o
	$(CC) $(LDFLAGS) -o adsbus adsbus.o backend.o client.o airspy_adsb.o beast.o json.o stats.o common.o $(LIBS)
