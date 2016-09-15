
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
	unsigned int *str_idx;
	unsigned int *word_count;
	double *dt, *tw;
	double *dt2, *tw2;
} plsa;

/* Functions */
void plsa_reset(plsa *pl);
int plsa_initialize(plsa *pl);
void plsa_cleanup(plsa *pl);

int plsa_process_files(plsa *pl, const char *directory,
                       unsigned int num_files);

int plsa_compute(plsa *pl, unsigned int num_topics,
                 unsigned int max_iterations, double tol);


#endif /* __PLSA_H */