CC ?= clang
CFLAGS ?= -Wall -Werror -O4 -g --std=gnu11 --pedantic-errors -fPIE -pie -fstack-protector-strong -pthread
LDFLAGS ?= $(CFLAGS) -Wl,-z,relro -Wl,-z,now
LIBS ?= -ljansson

OBJ_NETWORK = incoming.o outgoing.o receive.o send.o
OBJ_PROTOCOL = airspy_adsb.o beast.o json.o raw.o stats.o
OBJ_UTIL = buf.o hex.o opts.o packet.o peer.o rand.o resolve.o server.o uuid.o wakeup.o

all: adsbus

clean:
	rm -f *.o adsbus

%.o: %.c *.h
	$(CC) -c $(CFLAGS) $< -o $@

adsbus: adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL)
	$(CC) $(LDFLAGS) -o adsbus adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL) $(LIBS)
