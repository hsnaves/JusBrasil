
#ifndef __PLSA_H
#define __PLSA_H

#include <stdio.h>
#include "docinfo.h"

/* Data structures and types */
typedef
struct plsa_topmost_st {
	unsigned int idx;
	double val;
} plsa_topmost;

typedef
struct plsa_st {
	unsigned int num_words;
	unsigned int num_documents;
	unsigned int num_topics;
	plsa_topmost *top;
	double *dt, *tw;
	double *dt2, *tw2;
} plsa;

/* Functions */
void plsa_reset(plsa *pl);
int plsa_initialize(plsa *pl);
void plsa_cleanup(plsa *pl);

int plsa_train(plsa *pl, docinfo *doc, unsigned int num_topics,
               unsigned int max_iterations, double tol);
int plsa_print_best(plsa *pl, docinfo *doc, unsigned top_words,
                    unsigned int top_topics, unsigned int num_documents);

int plsa_save(FILE *fp, plsa *pl);
int plsa_load(FILE *fp, plsa *pl);

#endif /* __PLSA_H */