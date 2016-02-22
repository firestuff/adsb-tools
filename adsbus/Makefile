CC ?= clang
CFLAGS ?= -Wall -Werror -O4 -g --std=gnu11 --pedantic-errors -fPIE -pie -fstack-protector-strong
LDFLAGS ?= $(CFLAGS) -Wl,-z,relro -Wl,-z,now
LIBS ?= -luuid -ljansson

all: adsbus

clean:
	rm -f *.o adsbus

%.o: %.c *.h
	$(CC) -c $(CFLAGS) $< -o $@

adsbus: adsbus.o receive.o send.o incoming.o outgoing.o airspy_adsb.o beast.o json.o raw.o stats.o wakeup.o opts.o common.o
	$(CC) $(LDFLAGS) -o adsbus adsbus.o receive.o send.o incoming.o outgoing.o airspy_adsb.o beast.o json.o raw.o stats.o wakeup.o opts.o common.o $(LIBS)
