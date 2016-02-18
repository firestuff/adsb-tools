#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include "common.h"
#include "wakeup.h"

#include "receive.h"
#include "send.h"

#include "incoming.h"
#include "outgoing.h"

#include "airspy_adsb.h"
#include "beast.h"
#include "json.h"
#include "stats.h"

static void print_usage(const char *name) {
	fprintf(stderr,
			"\n"
			"Usage: %s [OPTION]...\n"
			"\n"
			"Options:\n"
			"\t--help\n"
			"\t--dump=FORMAT\n"
			"\t--connect-receive=HOST/PORT\n"
			"\t--connect-send=FORMAT=HOST/PORT\n"
			"\t--listen-receive=[HOST/]PORT\n"
			"\t--listen-send=FORMAT=[HOST/]PORT\n"
			, name);
	receive_print_usage();
	send_print_usage();
}

static bool add_dump(char *arg) {
	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		fprintf(stderr, "Unknown --dump=FORMAT: %s\n", arg);
		return false;
	}
	send_add(1, serializer);
	return true;
}

static bool add_connect_receive(char *arg) {
	char *port = strrchr(arg, '/');
	if (!port) {
		fprintf(stderr, "Invalid --connect-receive=HOST/PORT (missing \"/\"): %s\n", arg);
		return false;
	}
	*(port++) = '\0';

	outgoing_new(arg, port, receive_new, NULL);
	return true;
}

static bool add_connect_send(char *arg) {
	char *host_port = strchr(arg, '=');
	if (!host_port) {
		fprintf(stderr, "Invalid --connect-send=FORMAT=HOST/PORT (missing \"=\"): %s\n", arg);
		return false;
	}
	*(host_port++) = '\0';

	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		fprintf(stderr, "Unknown --connect-send=FORMAT=HOST/PORT format: %s\n", arg);
		return false;
	}

	char *port = strrchr(host_port, '/');
	if (!port) {
		fprintf(stderr, "Invalid --connect-send=FORMAT=HOST/PORT (missing \"/\"): %s\n", host_port);
		return false;
	}
	*(port++) = '\0';

	incoming_new(host_port, port, send_add_wrapper, serializer);
	return true;
}

static bool add_listen_receive(char *arg){
	char *port = strrchr(arg, '/');
	if (port) {
		*(port++) = '\0';
		incoming_new(arg, port, receive_new, NULL);
	} else {
		incoming_new(NULL, arg, receive_new, NULL);
	}
	return true;
}

static bool add_listen_send(char *arg) {
	char *host_port = strchr(arg, '=');
	if (!host_port) {
		fprintf(stderr, "Invalid --listen-send=FORMAT=[HOST/]PORT (missing \"=\"): %s\n", arg);
		return false;
	}
	*(host_port++) = '\0';

	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		fprintf(stderr, "Unknown --listen-send=FORMAT=[HOST/]PORT format: %s\n", arg);
		return false;
	}

	char *port = strrchr(host_port, '/');
	if (port) {
		*(port++) = '\0';
		incoming_new(host_port, port, send_add_wrapper, serializer);
	} else {
		incoming_new(NULL, host_port, send_add_wrapper, serializer);
	}
	return true;
}

static bool parse_opts(int argc, char *argv[]) {
	static struct option long_options[] = {
		{"dump",            required_argument, 0, 'd'},
		{"connect-receive", required_argument, 0, 'c'},
		{"connect-send",    required_argument, 0, 's'},
		{"listen-receive",  required_argument, 0, 'l'},
		{"listen-send",     required_argument, 0, 'm'},
		{"help",            no_argument,       0, 'h'},
		{0,                 0,                 0, 0  },
	};

	int opt;
	while ((opt = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
		bool (*handler)(char *) = NULL;
		switch (opt) {
		  case 'd':
				handler = add_dump;
				break;

			case 'c':
				handler = add_connect_receive;
				break;

			case 's':
				handler = add_connect_send;
				break;

			case 'l':
				handler = add_listen_receive;
				break;

			case 'm':
				handler = add_listen_send;
				break;

			case 'h':
			default:
				print_usage(argv[0]);
				return false;
		}

		if (handler) {
			if (!handler(optarg)) {
				print_usage(argv[0]);
				return false;
			}
		}
	}

	if (optind != argc) {
		fprintf(stderr, "Not a flag: %s\n", argv[optind]);
		print_usage(argv[0]);
		return false;
	}

	return true;
}

int main(int argc, char *argv[]) {
	hex_init();

	peer_init();
	wakeup_init();

	send_init();

	airspy_adsb_init();
	beast_init();
	json_init();
	stats_init();

	if (!parse_opts(argc, argv)) {
		return EXIT_FAILURE;
	}

	peer_loop();
	return EXIT_SUCCESS;
}
