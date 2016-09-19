
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "hmm.h"
#include "docinfo.h"
#include "utils.h"
#include "random.h"

void hmm_reset(hmm *h)
{
	h->ss = NULL;
	h->ss2 = NULL;
	h->sw = NULL;
	h->sw2 = NULL;

	h->opt_ss = NULL;
	h->opt_ss_i = NULL;
	h->opt_sw = NULL;
	h->opt_sw_i = NULL;
}

int hmm_initialize(hmm *h)
{
	hmm_reset(h);
	return TRUE;
}

static
void hmm_cleanup_tables(hmm *h)
{
	if (h->ss) {
		free(h->ss);
		h->ss = NULL;
	}
	if (h->ss2) {
		free(h->ss2);
		h->ss2 = NULL;
	}
	if (h->sw) {
		free(h->sw);
		h->sw = NULL;
	}
	if (h->sw2) {
		free(h->sw2);
		h->sw2 = NULL;
	}
}

static
void hmm_cleanup_optimization_tables(hmm *h)
{
	if (h->opt_ss) {
		free(h->opt_ss);
		h->opt_ss = NULL;
	}
	if (h->opt_ss_i) {
		free(h->opt_ss_i);
		h->opt_ss_i = NULL;
	}
	if (h->opt_sw) {
		free(h->opt_sw);
		h->opt_sw = NULL;
	}
	if (h->opt_sw_i) {
		free(h->opt_sw_i);
		h->opt_sw_i = NULL;
	}
}

void hmm_cleanup(hmm *h)
{
	hmm_cleanup_tables(h);
	hmm_cleanup_optimization_tables(h);
}

static
void hmm_initialize_random(hmm *h)
{
	unsigned int i, j, k, pos;
	double sum;

	for (i = 0; i < h->num_states; i++) {
		sum = 0;

		pos = i * h->num_states;
		h->ss[pos] = 0.0;
		if (i != 1) {
			for (j = 1; j < h->num_states; j++) {
				pos = i * h->num_states + j;
				h->ss[pos] = -log(genrand_real1());
				sum += h->ss[pos];
			}
			for (j = 1; j < h->num_states; j++) {
				pos = i * h->num_states + j;
				h->ss[pos] /= sum;
			}
		} else {
			pos = h->num_states + 1;
			h->ss[pos] = 1.0;
			for (j = 2; j < h->num_states; j++) {
				pos = i * h->num_states + j;
				h->ss[pos] = 0.0;
			}
		}

		if (i <= 1) continue;
		sum = 0;
		for (k = 0; k < h->num_words; k++) {
			pos = i * h->num_words + k;
			h->sw[pos] = -log(genrand_real1());
			sum += h->sw[pos];
		}
		for (k = 0; k < h->num_words; k++) {
			pos = i * h->num_words + k;
			h->sw[pos] /= sum;
		}
	}
}

int hmm_train(hmm *h, docinfo *doc, unsigned int num_states,
              unsigned int max_iterations, double tol)
{
	size_t size;

	hmm_cleanup_tables(h);
	h->num_documents = docinfo_num_documents(doc);
	h->num_words = docinfo_num_different_words(doc);
	h->num_states = num_states;

	size = num_states * num_states * sizeof(double);
	h->ss = (double *) xmalloc(size);
	if (!h->ss) return FALSE;

	h->ss2 = (double *) xmalloc(size);
	if (!h->ss2) return FALSE;

	size = num_states * h->num_words * sizeof(double);
	h->sw = (double *) xmalloc(size);
	if (!h->sw) return FALSE;

	h->sw2 = (double *) xmalloc(size);
	if (!h->sw2) return FALSE;

	hmm_initialize_random(h);

	return TRUE;
}

int hmm_optimize_generator(hmm *h)
{
	size_t size;

	hmm_cleanup_optimization_tables(h);

	size = h->num_states * h->num_states * sizeof(double);
	h->opt_ss = (double *) xmalloc(size);
	if (!h->opt_ss) return FALSE;

	size = h->num_states * h->num_states * sizeof(unsigned int);
	h->opt_ss_i = (unsigned int *) xmalloc(size);
	if (!h->opt_ss_i) return FALSE;

	size = h->num_states * h->num_words * sizeof(double);
	h->opt_sw = (double *) xmalloc(size);
	if (!h->opt_sw) return FALSE;

	size = h->num_states * h->num_words * sizeof(unsigned int);
	h->opt_sw_i = (unsigned int *) xmalloc(size);
	if (!h->opt_sw_i) return FALSE;

	return TRUE;
}

void hmm_generate_text(hmm *h, docinfo *doc)
{

}


int main(int argc, char **argv)
{
	docinfo doc;
	hmm h;

#if 1
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
#endif

	genrand_randomize();
	docinfo_reset(&doc);
	hmm_reset(&h);

	if (!docinfo_initialize(&doc))
		goto error_main;

	if (!hmm_initialize(&h))
		goto error_main;

	if (!docinfo_add_default_ignored(&doc))
		goto error_main;

	if (!docinfo_process_files(&doc, "reuters/training", 14818))
		goto error_main;

	printf("\n");
	if (!hmm_train(&h, &doc, 100, 1000, 0.0001))
		goto error_main;


	docinfo_cleanup(&doc);
	hmm_cleanup(&h);
	return 0;

error_main:
	docinfo_cleanup(&doc);
	hmm_cleanup(&h);
	return -1;
}