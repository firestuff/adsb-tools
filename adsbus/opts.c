#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "send.h"

#include "opts.h"

#define OPTS_MAX 128

static struct {
	const char *arg_help;
	opts_handler handler;
	void *group;
} opts[OPTS_MAX];

static struct option opts_long[OPTS_MAX];
static size_t opts_num = 0;
static int opts_argc;
static char **opts_argv;
static opts_group opts_group_internal;

static struct serializer *opts_get_serializer(const char **arg) {
	char *format = opts_split(arg, '=');
	if (!format) {
		return NULL;
	}

	struct serializer *serializer = send_get_serializer(format);
	free(format);
	if (!serializer) {
		return NULL;
	}

	return serializer;
}

static void opts_print_usage() {
	fprintf(stderr,
			"Usage: %s [OPTION]...\n"
			"\n"
			"Options:\n"
			, opts_argv[0]);
	for (size_t i = 0; i < opts_num; i++) {
		fprintf(stderr, "\t--%s%s%s\n", opts_long[i].name, opts[i].arg_help ? "=" : "", opts[i].arg_help ? opts[i].arg_help : "");
	}
}

static bool opts_help(const char __attribute__ ((unused)) *arg) {
	opts_print_usage();
	exit(EXIT_SUCCESS);
}

void opts_init(int argc, char *argv[]) {
	opts_argc = argc;
	opts_argv = argv;
	opts_add("help", NULL, opts_help, opts_group_internal);

	assert(opts_num < OPTS_MAX);
	opts_long[opts_num].name = NULL;
	opts_long[opts_num].has_arg = 0;
	opts_long[opts_num].flag = NULL;
	opts_long[opts_num].val = 0;

	opts_call(opts_group_internal);
}

void opts_add(const char *name, const char *arg_help, opts_handler handler, opts_group group) {
	assert(opts_num < OPTS_MAX);
	opts[opts_num].arg_help = arg_help;
	opts[opts_num].handler = handler;
	opts[opts_num].group = group;
	opts_long[opts_num].name = name;
	opts_long[opts_num].has_arg = arg_help ? required_argument : no_argument;
	opts_long[opts_num].flag = NULL;
	opts_long[opts_num].val = 0;
	opts_num++;
}

void opts_call(opts_group group) {
	optind = 1;
	int opt, longindex;
	while ((opt = getopt_long_only(opts_argc, opts_argv, "", opts_long, &longindex)) == 0) {
		if (opts[longindex].group != group) {
			continue;
		}
		if (!opts[longindex].handler(optarg)) {
			fprintf(stderr, "Invalid option value: %s\n", opts_argv[optind - 1]);
			exit(EXIT_FAILURE);
		}
	}
	if (opt != -1) {
		opts_print_usage();
		exit(EXIT_FAILURE);
	}

	if (optind != opts_argc) {
		fprintf(stderr, "Not a flag: %s\n", opts_argv[optind]);
		exit(EXIT_FAILURE);
	}
}

char *opts_split(const char **arg, char delim) {
	char *split = strchr(*arg, delim);
	if (!split) {
		return NULL;
	}
	char *ret = strndup(*arg, split - *arg);
	*arg = split + 1;
	return ret;
}

bool opts_add_send(bool (*next)(const char *, struct flow *, void *), struct flow *flow, const char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}
	return next(arg, flow, serializer);
}
