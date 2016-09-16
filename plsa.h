
#ifndef __PLSA_H
#define __PLSA_H

#include "docinfo.h"

/* Data structures and types */
typedef
struct plsa_st {
	unsigned int num_words;
	unsigned int num_documents;
	unsigned int num_topics;
	unsigned int *topmost;
	double *dt, *tw;
	double *dt2, *tw2;
} plsa;

/* Functions */
void plsa_reset(plsa *pl);
int plsa_initialize(plsa *pl);
void plsa_cleanup(plsa *pl);

int plsa_train(plsa *pl, docinfo *doc, unsigned int num_topics,
               unsigned int max_iterations, double tol);
int plsa_print_best(plsa *pl, docinfo *doc, unsigned top_words);

#endif /* __PLSA_H */