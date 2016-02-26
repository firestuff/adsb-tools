COMP ?= clang
DISABLED_WARNINGS ?= -Wno-padded -Wno-disabled-macro-expansion
CFLAGS ?= -Weverything -Werror -O3 -g --std=gnu11 --pedantic-errors -fPIE -fstack-protector-strong -pthread -D_GNU_SOURCE $(DISABLED_WARNINGS)
LDFLAGS ?= $(CFLAGS) -Wl,-z,relro -Wl,-z,now -pie
LIBS ?= -ljansson -lprotobuf-c

OBJ_NETWORK = incoming.o outgoing.o receive.o send.o
OBJ_PROTOCOL = airspy_adsb.o beast.o json.o proto.o raw.o stats.o
OBJ_UTIL = buf.o hex.o list.o opts.o packet.o peer.o rand.o resolve.o server.o socket.o uuid.o wakeup.o
OBJ_PROTO = adsb.pb-c.o

all: adsbus

clean:
	rm -f *.o adsbus

%.o: %.c *.h
	$(COMP) -c $(CFLAGS) $< -o $@

adsb.pb-c.c: ../proto/adsb.proto
	protoc-c --c_out=./ --proto_path=$(dir $<) $<

adsbus: adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL) $(OBJ_PROTO)
	$(COMP) $(LDFLAGS) -o adsbus adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL) $(OBJ_PROTO) $(LIBS)
