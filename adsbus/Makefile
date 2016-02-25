CC ?= clang
CFLAGS ?= -Wall -Werror -O4 -g --std=gnu11 --pedantic-errors -fPIE -pie -fstack-protector-strong -pthread
LDFLAGS ?= $(CFLAGS) -Wl,-z,relro -Wl,-z,now
LIBS ?= -ljansson -lprotobuf-c

OBJ_NETWORK = incoming.o outgoing.o receive.o send.o
OBJ_PROTOCOL = airspy_adsb.o beast.o json.o proto.o raw.o stats.o
OBJ_UTIL = buf.o hex.o opts.o packet.o peer.o rand.o resolve.o server.o uuid.o wakeup.o
OBJ_PROTO = adsb.pb-c.o

all: adsbus

clean:
	rm -f *.o adsbus

%.o: %.c *.h
	$(CC) -c $(CFLAGS) $< -o $@

adsb.pb-c.c: ../proto/adsb.proto
	protoc-c --c_out=./ --proto_path=$(dir $<) $<

adsbus: adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL) $(OBJ_PROTO)
	$(CC) $(LDFLAGS) -o adsbus adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL) $(OBJ_PROTO) $(LIBS)
