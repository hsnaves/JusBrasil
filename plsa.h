
#ifndef __PLSA_H
#define __PLSA_H

#include "hashtable.h"
#include "reader.h"

/* Data structures and types */
typedef
struct plsa_st {
	unsigned int num_words;
	unsigned int num_documents;
	unsigned int num_topics;
	reader r;
	hashtable ht;
	hashtable ignored;
	unsigned int *word_count;
	unsigned int *topmost;
	double *dt, *tw;
	double *dt2, *tw2;
} plsa;

/* Functions */
void plsa_reset(plsa *pl);
int plsa_initialize(plsa *pl);
void plsa_cleanup(plsa *pl);

void plsa_clear_ignored(plsa *pl);
int plsa_add_ignored(plsa *pl, const char *word);

int plsa_process_files(plsa *pl, const char *directory,
                       unsigned int num_files);
int plsa_compute(plsa *pl, unsigned int num_topics,
                 unsigned int max_iterations, double tol);
int plsa_print_best(plsa *pl, unsigned top_words);

#endif /* __PLSA_H */