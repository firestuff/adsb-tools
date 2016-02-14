#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h> 
#include <unistd.h>
#include <sys/epoll.h>


struct opts {
	char *backend_node;
	char *backend_service;
};


#define BUF_LEN 4096
struct buf {
	char buf[BUF_LEN];
	size_t start;
	size_t length;
};


int parseOpts(int argc, char *argv[], struct opts *opts) {
	int opt;
	while ((opt = getopt(argc, argv, "h:p:")) != -1) {
		switch (opt) {
		  case 'h':
			  opts->backend_node = optarg;
				break;

			case 'p':
				opts->backend_service = optarg;
				break;

			default:
				return -1;
		}
	}
	return 0;
}


int connectBackend(struct opts *opts) {
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *addrs;

	int gai_err = getaddrinfo(opts->backend_node, opts->backend_service, &hints, &addrs);
	if (gai_err) {
		fprintf(stderr, "getaddrinfo(%s/%s): %s\n", opts->backend_node, opts->backend_service, gai_strerror(gai_err));
		return -1;
	}

	int bfd;
	struct addrinfo *addr;
  for (addr = addrs; addr != NULL; addr = addr->ai_next) {
		bfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (bfd == -1) {
			perror("socket");
			continue;
		}

		if (connect(bfd, addr->ai_addr, addr->ai_addrlen) != -1) {
			break;
		}

		close(bfd);
	}

	if (addr == NULL) {
		freeaddrinfo(addrs);
		fprintf(stderr, "Can't connect to %s/%s\n", opts->backend_node, opts->backend_service);
		return -1;
	}

  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  if (getnameinfo(addr->ai_addr, addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
		fprintf(stderr, "Connected to %s/%s\n", hbuf, sbuf);
	}

	freeaddrinfo(addrs);

	return bfd;
}


int readToBuf(int fd, struct buf *buf) {
	struct iovec iov[2];
	int iovcnt;
	size_t space = BUF_LEN - buf->length;
	size_t end = (buf->start + buf->length) % BUF_LEN;
	if (end + space > BUF_LEN) {
	  // Wraps around
		iovcnt = 2;
		iov[0].iov_base = &buf->buf[end];
		iov[0].iov_len = BUF_LEN - end;
		iov[1].iov_base = 0;
		iov[1].iov_len = space - iov[0].iov_len;
	} else {
		iovcnt = 1;
		iov[0].iov_base = &buf->buf[end];
		iov[0].iov_len = space;
	}
	ssize_t in = readv(fd, iov, iovcnt);
	if (in < 0) {
		return in;
	}
	buf->length += in;
	return in;
}


int loop(int bfd) {
	int efd = epoll_create(10);
  if (efd == -1) {
		perror("epoll_create");
		return -1;
  }

	{
		struct epoll_event ev = {
			.events = EPOLLIN,
			.data = {
				.fd = bfd,
			},
		};
		if (epoll_ctl(efd, EPOLL_CTL_ADD, bfd, &ev) == -1) {
			perror("epoll_ctl");
			return -1;
		}
	}

	struct buf buf = {
		.start = 0,
		.length = 0,
	};

	while (1) {
#define MAX_EVENTS 10
		struct epoll_event events[MAX_EVENTS];
		int nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			perror("epoll_wait");
			return -1;
		}

    for (int n = 0; n < nfds; n++) {
			if (events[n].data.fd == bfd) {
				if (readToBuf(bfd, &buf) < 0) {
					fprintf(stderr, "Connection closed by backend\n");
					return -1;
				}
				fprintf(stderr, "buf len is now %d\n", buf.length);
				if (buf.length == BUF_LEN) {
					return -1;
				}
			}
		}
	}

	return 0;
}


int main(int argc, char *argv[]) {
	struct opts opts = {
		.backend_node = "localhost",
		.backend_service = "30006",
	};
	if (parseOpts(argc, argv, &opts)) {
		fprintf(stderr, "Usage: %s [-h backend_host] [-p backend_port]\n", argv[0]);
		return EXIT_FAILURE;
	}

	int bfd = connectBackend(&opts);
	if (bfd < 0) {
		fprintf(stderr, "Unable to connect to %s/%s\n", opts.backend_node, opts.backend_service);
		return EXIT_FAILURE;
	}

	if (loop(bfd)) {
		fprintf(stderr, "Main loop exited with error\n");
		return EXIT_FAILURE;
	}

	close(bfd);
	return EXIT_SUCCESS;
}
