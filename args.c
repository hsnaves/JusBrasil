
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"
#include "utils.h"

static
const char *argtype_names[] = { "", "file", "uint", "dbl" };

void print_help(const char *prog_name, option *opts, unsigned int num_opts)
{
	unsigned int i;
	int len;

	fprintf(stderr, "Usage:\n");
	fprintf(stderr, " %s [options]\n", prog_name);
	fprintf(stderr, "where the possible options are:\n");
	for (i = 0; i < num_opts; i++) {
		len = fprintf(stderr, "  %s %s ",
		              opts[i].name, argtype_names[opts[i].argtype]);
		while (len++ < 20) fprintf(stderr, " ");
		fprintf(stderr, "%s\n", opts[i].help);
	}
}

int process_args(int argc, char **argv, option *opts, unsigned int num_opts)
{
	unsigned int j;
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (j = 0; j < num_opts; j++) {
				if (strcmp(opts[j].name, argv[i]) != 0)
					continue;
				break;
			}

			if (j >= num_opts) {
				error("invalid option `%s'", argv[i]);
				return -1;
			}

			if (opts[j].argtype != ARGTYPE_NONE) {
				char **pstr;
				unsigned int *puint;
				double *pdbl;

				if (i == argc - 1) {
					error("option `%s' needs "
					      "argument", argv[i]);
					return -1;
				}
				switch(opts[j].argtype) {
				case ARGTYPE_FILE:
					pstr = (char **) opts[j].ptr;
					*pstr = argv[++i];
					break;
				case ARGTYPE_UINT:
					puint = (unsigned int *) opts[j].ptr;
					*puint = strtoul(argv[++i], NULL, 10);
					break;
				case ARGTYPE_DBL:
					pdbl = (double *) opts[j].ptr;
					*pdbl = strtof(argv[++i], NULL);
					break;
				default:
					break;
				}
			} else {
				print_help(argv[0], opts, num_opts);
				return 0;
			}
		} else {
			error("invalid argument `%s'", argv[i]);
			return -1;
		}
	}
	return 1;
}
