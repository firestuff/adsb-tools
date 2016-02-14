#include <stdint.h>
#include <unistd.h>


//////// buf

#define BUF_LEN 4096
struct buf {
	char buf[BUF_LEN];
	size_t start;
	size_t length;
};

#define buf_chr(buff, at) ((buff)->buf[(buff)->start + (at)])
#define buf_at(buff, at) (&buf_chr(buff, at))

ssize_t buf_fill(struct buf *, int);
void buf_consume(struct buf *, size_t);


//////// packet

#define MLAT_HZ 60000000
#define DATA_MAX 14
struct packet {
	enum {
		MODE_AC,
		MODE_S_SHORT,
		MODE_S_LONG,
	} type;
	char data[DATA_MAX];
	uint64_t mlat_timestamp;
	uint32_t rssi;
};


//////// hex
