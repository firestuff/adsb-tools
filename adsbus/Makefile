CC ?= clang
CFLAGS ?= -Wall -Werror -O4 -g --std=gnu11 --pedantic-errors -fPIE -pie -fstack-protector-strong
LDFLAGS ?= $(CFLAGS) -Wl,-z,relro -Wl,-z,now
LIBS ?= -ljansson

OBJ_NETWORK = incoming.o outgoing.o receive.o send.o peer.o
OBJ_PROTOCOL = airspy_adsb.o beast.o json.o raw.o stats.o
OBJ_UTIL = buf.o hex.o opts.o rand.o uuid.o wakeup.o common.o

all: adsbus

clean:
	rm -f *.o adsbus

%.o: %.c *.h
	$(CC) -c $(CFLAGS) $< -o $@

adsbus: adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL)
	$(CC) $(LDFLAGS) -o adsbus adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL) $(LIBS)
