
#ifndef __HMM_H
#define __HMM_H

#include <stdio.h>

#include "docinfo.h"

/* Data structures and types */
typedef
struct hmm_st {
	unsigned int num_words;
	unsigned int num_documents;
	unsigned int num_states;

	double *ss, *ss2;
	double *sw, *sw2;

	double *dps, *dpe;
	double *dps_s, *dpe_s;

	double *opt_ss, *opt_sw;
	unsigned int *opt_ss_i, *opt_sw_i;

	double *tmp_d;
	unsigned int *tmp_i;
} hmm;

/* Functions */
void hmm_reset(hmm *h);
int hmm_initialize(hmm *h);
void hmm_cleanup(hmm *h);

int hmm_train(hmm *h, const docinfo *doc, unsigned int num_states,
              unsigned int max_iterations, double tol);
int hmm_optimize_generator(hmm *h);
void hmm_generate_text(const hmm *h, const docinfo *doc);

int hmm_save(const hmm *h, FILE *fp);
int hmm_save_easy(const hmm *h, const char *filename);
int hmm_load(hmm *h, FILE *fp);
int hmm_load_easy(hmm *h, const char *filename);

#endif /* __HMM_H */
