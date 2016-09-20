
#ifndef __HMM_H
#define __HMM_H

#include "docinfo.h"

/* Data structures and types */
typedef
struct hmm_st {
	unsigned int num_words;
	unsigned int num_documents;
	unsigned int num_states;

	double *ss, *ss2;
	double *sw, *sw2;

	double *opt_ss, *opt_sw;
	unsigned int *opt_ss_i, *opt_sw_i;
} hmm;

/* Functions */
void hmm_reset(hmm *h);
int hmm_initialize(hmm *h);
void hmm_cleanup(hmm *h);

int hmm_train(hmm *h, docinfo *doc, unsigned int num_states,
              unsigned int max_iterations, double tol);
int hmm_optimize_generator(hmm *h);
void hmm_generate_text(hmm *h, docinfo *doc);

int hmm_save(FILE *fp, hmm *h);
int hmm_load(FILE *fp, hmm *h);

#endif /* __HMM_H */