
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "hmm.h"
#include "docinfo.h"
#include "utils.h"
#include "random.h"

#define EPS 1e-10

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

static
int hmm_allocate(hmm *h, unsigned int num_words,
                 unsigned int num_documents, unsigned int num_states)
{
	size_t size;

	hmm_cleanup_tables(h);
	h->num_documents = num_documents;
	h->num_words = num_words;
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

	return TRUE;
}

int hmm_train(hmm *h, docinfo *doc, unsigned int num_states,
              unsigned int max_iterations, double tol)
{
	if (!hmm_allocate(h, docinfo_num_different_words(doc),
	                   docinfo_num_documents(doc), num_states))
		return FALSE;

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

int hmm_save(FILE *fp, hmm *h)
{
	unsigned int i, j, k, pos, pos2;

	fprintf(fp, "%u %u %u\n", h->num_words,
	       h->num_documents, h->num_states);
	for (i = 0; i < h->num_states; i++) {
		for (j = 0; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			if (fabs(h->ss[pos]) < EPS)
				fprintf(fp, "0 ");
			else
				fprintf(fp, "%g ", h->ss[pos]);
		}
		fprintf(fp, "\n");
	}
	for (j = 0; j < h->num_states; j++) {
		for (k = 0; k < h->num_words; k++) {
			pos2 = j * h->num_words + k;
			if (fabs(h->sw[pos2]) < EPS)
				fprintf(fp, "0 ");
			else
				fprintf(fp, "%g ", h->sw[pos2]);
		}
		fprintf(fp, "\n");
	}
	return TRUE;
}

int hmm_load(FILE *fp, hmm *h)
{
	unsigned int i, j, k, pos, pos2;
	unsigned int num_words, num_documents, num_states;

	hmm_reset(h);
	if (!hmm_initialize(h))
		return FALSE;

	if (fscanf(fp, "%u %u %u\n", &num_words,
	           &num_documents, &num_states) != 3)
		return FALSE;

	if (!hmm_allocate(h, num_words, num_documents, num_states))
		goto error_load;

	for (i = 0; i < h->num_states; i++) {
		for (j = 0; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			if (fscanf(fp, "%lf", &h->ss[pos]) != 1)
				goto error_load;
		}
	}
	for (j = 0; j < h->num_states; j++) {
		for (k = 0; k < h->num_words; k++) {
			pos2 = j * h->num_words + k;
			if (fscanf(fp, "%lf", &h->sw[pos2]) != 1)
				goto error_load;
		}
	}
	return TRUE;

error_load:
	error("could not load PLSA");
	hmm_cleanup(h);
	return FALSE;
}

static
int train_dataset(const char *directory, unsigned int num_files,
                  unsigned int num_states, unsigned int max_iter, double tol,
                  const char *docinfo_file, const char *hmm_file)
{
	FILE *fp;
	docinfo doc;
	hmm h;

	docinfo_reset(&doc);
	hmm_reset(&h);

	if (!docinfo_initialize(&doc))
		goto error_train;

	if (!hmm_initialize(&h))
		goto error_train;

	if (!docinfo_add_default_ignored(&doc))
		goto error_train;

	if (!docinfo_process_files(&doc, directory, num_files))
		goto error_train;

	printf("\n");
	if (!hmm_train(&h, &doc, num_states, max_iter, tol))
		goto error_train;

	fp = fopen(hmm_file, "wb");
	if (!fp) {
		error("could not save `%s'", hmm_file);
		goto error_train;
	}
	hmm_save(fp, &h);
	fclose(fp);

	fp = fopen(docinfo_file, "wb");
	if (!fp) {
		error("could not save `%s'", docinfo_file);
		goto error_train;
	}
	docinfo_save(fp, &doc);
	fclose(fp);

	docinfo_cleanup(&doc);
	hmm_cleanup(&h);
	return TRUE;

error_train:
	docinfo_cleanup(&doc);
	hmm_cleanup(&h);
	return FALSE;
}

int main(int argc, char **argv)
{
#if 1
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
#endif

	genrand_randomize();
	train_dataset("texts", 54562, 80, 1000, 0.001, "result.docinfo",
	              "result.hmm");
	return 0;
}