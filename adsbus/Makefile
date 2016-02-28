COMP ?= clang
DISABLED_WARNINGS ?= -Wno-padded -Wno-disabled-macro-expansion
CFLAGS ?= -Weverything -Werror -O3 -g --std=gnu11 --pedantic-errors -fPIE -fstack-protector-strong -pthread -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(DISABLED_WARNINGS)
LDFLAGS ?= $(CFLAGS) -Wl,-z,relro -Wl,-z,now -pie
LIBS ?= -ljansson -lprotobuf-c

TESTCASE_DIR ?= testcase
TESTOUT_DIR ?= testout
VALGRIND ?= valgrind
VALGRIND_FLAGS ?= --error-exitcode=1 --track-fds=yes --show-leak-kinds=all --leak-check=full
ADSBUS_TEST_FLAGS ?= --stdin --stdout=airspy_adsb --stdout=beast --stdout=json --stdout=proto --stdout=raw --stdout=stats

OBJ_NETWORK = incoming.o outgoing.o receive.o send.o
OBJ_PROTOCOL = airspy_adsb.o beast.o json.o proto.o raw.o stats.o
OBJ_UTIL = buf.o hex.o list.o opts.o packet.o peer.o rand.o resolve.o server.o socket.o uuid.o wakeup.o
OBJ_PROTO = adsb.pb-c.o

all: adsbus

clean:
	rm -rf *.o adsbus testout findings

%.o: %.c *.h
	$(COMP) -c $(CFLAGS) $< -o $@

adsb.pb-c.c: ../proto/adsb.proto
	protoc-c --c_out=./ --proto_path=$(dir $<) $<

adsbus: adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL) $(OBJ_PROTO)
	$(COMP) $(LDFLAGS) -o adsbus adsbus.o $(OBJ_NETWORK) $(OBJ_PROTOCOL) $(OBJ_UTIL) $(OBJ_PROTO) $(LIBS)

fuzz:
	rm -rf findings
	mkdir findings
	$(MAKE) clean
	COMP=afl-clang $(MAKE) adsbus
	afl-fuzz -i testcase/ -o findings/ ./adsbus --stdin --stdout=airspy_adsb --stdout=beast --stdout=json --stdout=proto --stdout=raw --stdout=stats

$(TESTOUT_DIR)/%: $(TESTCASE_DIR)/% adsbus
	$(VALGRIND) $(VALGRIND_FLAGS) ./adsbus $(ADSBUS_TEST_FLAGS) >/dev/null 2>$@ < $<

test:
	test -d $(TESTOUT_DIR) || mkdir $(TESTOUT_DIR)
	$(MAKE) realtest

realtest: $(patsubst $(TESTCASE_DIR)/%,$(TESTOUT_DIR)/%,$(wildcard $(TESTCASE_DIR)/*))
