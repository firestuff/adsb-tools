#include <stdint.h>
#include <unistd.h>


#define BUF_LEN 4096
struct buf {
	char *buf;
	char *tmp;
	size_t start;
	size_t length;
};


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


#define buf_at(buff, at) (&(buff)->buf[(buff)->start + (at)])

void buf_init(struct buf *, char *, char *);
void buf_alias(struct buf *, struct buf *);

ssize_t buf_fill(struct buf *, int);
void buf_consume(struct buf *, size_t);
