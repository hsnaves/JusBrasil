
#ifndef __ARGS_H
#define __ARGS_H

/* Constants */
#define ARGTYPE_NONE   0
#define ARGTYPE_FILE   1
#define ARGTYPE_UINT   2
#define ARGTYPE_DBL    3

/* Data structures and types */
typedef
struct option_st {
	const char *name;
	void *ptr;
	int argtype;
	const char *help;
} option;

/* Functions */
void print_help(const char *prog_name, option *opts, unsigned int num_opts);
int process_args(int argc, char **argv, option *opts, unsigned int num_opts);

#endif /* __ARGS_H */
