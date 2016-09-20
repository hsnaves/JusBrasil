
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
	h->tmp_i = NULL;
	h->tmp_d = NULL;
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
	if (h->tmp_i) {
		free(h->tmp_i);
		h->tmp_i = NULL;
	}
	if (h->tmp_d) {
		free(h->tmp_d);
		h->tmp_d = NULL;
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

static
int cmp_dbl_indirect(const void *p1, const void *p2, void *arg)
{
	const unsigned int *i1 = (const unsigned int *) p1;
	const unsigned int *i2 = (const unsigned int *) p2;
	const double *d = (const double *) arg;
	if (d[*i1] < d[*i2]) return -1;
	if (d[*i1] > d[*i2]) return 1;
	return 0;
}

static
void hmm_optimize_array(hmm *h, double *array, unsigned int size,
                        double *opt_v, unsigned int *opt_i)
{
	double thresh;
	unsigned int j, l, r;

	thresh = 1.0 / size;
	for (j = 0; j < size; j++) {
		h->tmp_i[j] = j;
		h->tmp_d[j] = array[j];
	}
	xsort(h->tmp_i, size, sizeof(unsigned int),
	      &cmp_dbl_indirect, h->tmp_d);

	j = 0;
	l = 0;
	r = size - 1;
	while (l < r) {
		if (h->tmp_d[l] + h->tmp_d[r] < thresh) {
			opt_v[j] = h->tmp_d[r - 1] / thresh;
			opt_i[2 * j] = h->tmp_i[r - 1];
			opt_i[2 * j + 1] = h->tmp_i[r];
			h->tmp_d[r - 1] += h->tmp_d[r] - thresh;
			r--;
			j++;
		} else {
			opt_v[j] = h->tmp_d[l] / thresh;
			opt_i[2 * j] = h->tmp_i[l];
			opt_i[2 * j + 1] = h->tmp_i[r];
			h->tmp_d[r] += h->tmp_d[l] - thresh;
			l++;
			j++;
		}
	}
	opt_v[j] = 1.0;
	opt_i[2 * j] = h->tmp_i[l];
	opt_i[2 * j + 1] = h->tmp_i[l];
}

int hmm_optimize_generator(hmm *h)
{
	size_t size, num;
	unsigned int i, pos;

	hmm_cleanup_optimization_tables(h);

	size = h->num_states * h->num_states * sizeof(double);
	h->opt_ss = (double *) xmalloc(size);
	if (!h->opt_ss) return FALSE;

	size = 2 * h->num_states * h->num_states * sizeof(unsigned int);
	h->opt_ss_i = (unsigned int *) xmalloc(size);
	if (!h->opt_ss_i) return FALSE;

	size = h->num_states * h->num_words * sizeof(double);
	h->opt_sw = (double *) xmalloc(size);
	if (!h->opt_sw) return FALSE;

	size = 2 * h->num_states * h->num_words * sizeof(unsigned int);
	h->opt_sw_i = (unsigned int *) xmalloc(size);
	if (!h->opt_sw_i) return FALSE;

	num = MAX(h->num_states, h->num_words);
	size = num * sizeof(unsigned int);
	h->tmp_i = (unsigned int *) xmalloc(size);
	if (!h->tmp_i) return FALSE;

	size = num * sizeof(double);
	h->tmp_d = (double *) xmalloc(size);
	if (!h->tmp_d) return FALSE;

	for (i = 0; i < h->num_states; i++) {
		pos = i * h->num_states;
		hmm_optimize_array(h, &h->ss[pos], h->num_states,
		                   &h->opt_ss[pos], &h->opt_ss_i[2 * pos]);
	}

	for (i = 0; i < h->num_states; i++) {
		pos = i * h->num_words;
		hmm_optimize_array(h, &h->sw[pos], h->num_words,
		                   &h->opt_sw[pos], &h->opt_sw_i[2 * pos]);
	}
	return TRUE;
}

void hmm_generate_text(hmm *h, docinfo *doc)
{
	unsigned int state, idx, pos, word_idx;
	double val;

	state = 0;
	while (state != 1) {
		idx = (unsigned int) (genrand_int32() % h->num_states);
		val = genrand_real1();
		pos = state * h->num_states + idx;
		if (val >= h->opt_ss[pos]) {
			state = h->opt_ss_i[2 * pos + 1];
		} else {
			state = h->opt_ss_i[2 * pos];
		}
		if (state > 1) {
			idx = (unsigned int) (genrand_int32() % h->num_words);
			val = genrand_real1();
			pos = state * h->num_words + idx;
			if (val >= h->opt_sw[pos]) {
				word_idx = h->opt_sw_i[2 * pos + 1];
			} else {
				word_idx = h->opt_sw_i[2 * pos];
			}
			printf("%s ", docinfo_get_word(doc, word_idx + 1));
		}
	}
	printf("\n");
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

	if (!hmm_optimize_generator(&h))
		goto error_train;

	hmm_generate_text(&h, &doc);
	hmm_generate_text(&h, &doc);
	hmm_generate_text(&h, &doc);


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