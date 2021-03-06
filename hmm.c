
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hmm.h"
#include "args.h"
#include "docinfo.h"
#include "utils.h"
#include "random.h"

#define EPS 1e-12

void hmm_reset(hmm *h)
{
	h->ss = NULL;
	h->ss2 = NULL;
	h->sw = NULL;
	h->sw2 = NULL;

	h->dps = NULL;
	h->dpe = NULL;
	h->dps_s = NULL;
	h->dpe_s = NULL;

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
	h->likelihood = 1;
	h->old_likelihood = 1;
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
void hmm_cleanup_dp_tables(hmm *h)
{
	if (h->dps) {
		free(h->dps);
		h->dps = NULL;
	}
	if (h->dpe) {
		free(h->dpe);
		h->dpe = NULL;
	}
	if (h->dps_s) {
		free(h->dps_s);
		h->dps_s = NULL;
	}
	if (h->dpe_s) {
		free(h->dpe_s);
		h->dpe_s = NULL;
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
	hmm_cleanup_dp_tables(h);
	hmm_cleanup_optimization_tables(h);
}

static
void hmm_normalize_tables(hmm *h, double *ss, double *sw)
{
	unsigned int i, j, k, pos;
	double sum;

	for (i = 0; i < h->num_states; i++) {
		sum = 0;
		for (j = 0; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			sum += ss[pos];
		}
		if (fabs(sum) >= EPS) {
			for (j = 0; j < h->num_states; j++) {
				pos = i * h->num_states + j;
				ss[pos] /= sum;
			}
		}

		sum = 0;
		for (k = 0; k < h->num_words; k++) {
			pos = i * h->num_words + k;
			sum += sw[pos];
		}
		if (fabs(sum) >= EPS) {
			for (k = 0; k < h->num_words; k++) {
				pos = i * h->num_words + k;
				sw[pos] /= sum;
			}
		}
	}
}

static
void hmm_initialize_random(hmm *h)
{
	unsigned int i, j, k, pos;

	for (i = 0; i < h->num_states; i++) {
		pos = i * h->num_states;
		h->ss[pos] = 0.0;
		if (i != 1) {
			for (j = 1; j < h->num_states; j++) {
				pos = i * h->num_states + j;
				h->ss[pos] = -log(genrand_real1());
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
		for (k = 0; k < h->num_words; k++) {
			pos = i * h->num_words + k;
			h->sw[pos] = -log(genrand_real1());
		}
	}
	hmm_normalize_tables(h, h->ss, h->sw);
}

static
int hmm_allocate_tables(hmm *h, unsigned int num_words,
                        unsigned int num_documents, unsigned int num_states)
{
	size_t size;

	if (h->num_states != num_states) {
		hmm_cleanup_tables(h);
	} else if (h->num_words != num_words) {
		if (h->sw) {
			free(h->sw);
			h->sw = NULL;
		}
		if (h->sw2) {
			free(h->sw2);
			h->sw2 = NULL;
		}
	}
	h->num_documents = num_documents;
	h->num_words = num_words;
	h->num_states = num_states;

	size = num_states * num_states * sizeof(double);
	if (!h->ss) {
		h->ss = (double *) xmalloc(size);
		if (!h->ss) return FALSE;
	}

	if (!h->ss2) {
		h->ss2 = (double *) xmalloc(size);
		if (!h->ss2) return FALSE;
	}

	size = num_states * h->num_words * sizeof(double);
	if (!h->sw) {
		h->sw = (double *) xmalloc(size);
		if (!h->sw) return FALSE;
	}

	if (!h->sw2) {
		h->sw2 = (double *) xmalloc(size);
		if (!h->sw2) return FALSE;
	}

	return TRUE;
}

static
int hmm_allocate_dp_tables(hmm *h, unsigned int max_document_length)
{
	size_t size;

	hmm_cleanup_dp_tables(h);

	size = (max_document_length + 2) * sizeof(double);
	h->dps = (double *) xmalloc(size);
	if (!h->dps) return FALSE;

	h->dpe = (double *) xmalloc(size);
	if (!h->dpe) return FALSE;

	size = h->num_states * (max_document_length + 2) * sizeof(double);
	h->dps_s = (double *) xmalloc(size);
	if (!h->dps_s) return FALSE;

	h->dpe_s = (double *) xmalloc(size);
	if (!h->dpe_s) return FALSE;

	return TRUE;
}

static
double hmm_compute_dp_tables(hmm *h, const docinfo *doc,
                             const docinfo_document *document)
{
	unsigned int i, j, k, l;
	unsigned int pos, pos2, pos3, pos4;
	double likelihood;

	likelihood = 0;
	h->dps[0] = 1;
	memset(h->dps_s, 0, h->num_states * sizeof(double));
	h->dps_s[0] = 1;
	for (i = 1; i <= document->word_count; i++) {
		h->dps[i] = 0;
		l = docinfo_get_wordidx_in_doc(doc, document, i) - 1;
		for (j = 0; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			h->dps_s[pos] = 0;
			if (j == 0 || j == 1) continue;
			for (k = 0; k < h->num_states; k++) {
				pos2 = (i - 1) * h->num_states + k;
				pos3 = k * h->num_states + j;
				h->dps_s[pos] += h->ss[pos3] * h->dps_s[pos2];
			}
			pos4 = j * h->num_words + l;
			h->dps_s[pos] *= h->sw[pos4];
			h->dps[i] += h->dps_s[pos];
		}
		for (j = 2; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			h->dps_s[pos] /= h->dps[i];
		}
		likelihood += log(h->dps[i]);
	}
	memset(&h->dps_s[i * h->num_states], 0,
	       h->num_states * sizeof(double));
	pos = i * h->num_states + 1;
	for (k = 0; k < h->num_states; k++) {
		pos2 = (i - 1) * h->num_states + k;
		pos3 = k * h->num_states + 1;
		h->dps_s[pos] += h->ss[pos3] * h->dps_s[pos2];
	}
	h->dps[i] = h->dps_s[pos];
	h->dps_s[pos] = 1;
	likelihood += log(h->dps[i]);

	i = document->word_count + 1;
	memset(&h->dpe_s[i * h->num_states], 0,
	       h->num_states * sizeof(double));
	pos = i * h->num_states + 1;
	h->dpe_s[pos] = 1;
	h->dpe[i] = 1;
	for (i = document->word_count; i >= 1; i--) {
		h->dpe[i] = 0;
		l = docinfo_get_wordidx_in_doc(doc, document, i) - 1;
		for (j = 0; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			h->dpe_s[pos] = 0;
			if (j == 0 || j == 1) continue;
			for (k = 0; k < h->num_states; k++) {
				pos2 = (i + 1) * h->num_states + k;
				pos3 = j * h->num_states + k;
				h->dpe_s[pos] += h->ss[pos3] * h->dpe_s[pos2];
			}
			pos4 = j * h->num_words + l;
			h->dpe_s[pos] *= h->sw[pos4];
			h->dpe[i] += h->dpe_s[pos];
		}
		for (j = 2; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			h->dpe_s[pos] /= h->dpe[i];
		}
	}
	memset(h->dpe_s, 0, h->num_states * sizeof(double));
	pos = 0;
	for (k = 0; k < h->num_states; k++) {
		pos2 = 1 * h->num_states + k;
		pos3 = k;
		h->dpe_s[pos] += h->ss[pos3] * h->dpe_s[pos2];
	}
	h->dpe[0] = h->dpe_s[pos];
	h->dpe_s[pos] = 1;

	return likelihood;
}

static
void hmm_iteration_aux(hmm *h, const docinfo *doc,
                       const docinfo_document *document)
{
	unsigned int i, j, k, l;
	unsigned int pos, pos2, pos3;
	double factor;

	factor = h->dps[0] / h->dpe[0];
	for (i = 1; i <= document->word_count; i++) {
		l = docinfo_get_wordidx_in_doc(doc, document, i) - 1;
		factor *= h->dps[i];
		for (j = 2; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			pos2 = j * h->num_words + l;
			if (h->sw[pos2] < EPS) continue;
			h->sw2[pos2] += h->dps_s[pos] * h->dpe_s[pos] * factor
			                / h->sw[pos2];
		}
		factor /= h->dpe[i];
	}
	factor = 1;
	for (i = 0; i <= document->word_count; i++) {
		factor *= h->dps[i] / h->dpe[i];
		for (j = 0; j < h->num_states; j++) {
			for (k = 0; k < h->num_states; k++) {
				pos = i * h->num_states + j;
				pos2 = (i + 1) * h->num_states + k;
				pos3 = j * h->num_states + k;
				h->ss2[pos3] += h->dps_s[pos] * h->dpe_s[pos2]
				                * h->ss[pos3] * factor;
			}
		}
	}
}

static
double hmm_iteration(hmm *h, const docinfo *doc)
{
	docinfo_document *document;
	unsigned int d, total_words;
	double likelihood;
	size_t size;

	size = h->num_states * h->num_states * sizeof(double);
	memset(h->ss2, 0, size);

	size = h->num_states * h->num_words * sizeof(double);
	memset(h->sw2, 0, size);

	likelihood = 0;
	total_words = 0;
	for (d = 0; d < docinfo_num_documents(doc); d++) {
		document = docinfo_get_document(doc, d + 1);
		total_words += document->word_count;
		likelihood += hmm_compute_dp_tables(h, doc, document);
		hmm_iteration_aux(h, doc, document);
	}
	h->ss2[h->num_states + 1] = 1.0;
	hmm_normalize_tables(h, h->ss2, h->sw2);
	return likelihood / total_words;
}

int hmm_train(hmm *h, const docinfo *doc, unsigned int num_states,
              unsigned int max_iterations, double tol,
              const char *hmm_filename)
{
	unsigned int iter;
	double *temp;

	if (!hmm_allocate_tables(h, docinfo_num_different_words(doc),
	                         docinfo_num_documents(doc), num_states))
		return FALSE;

	if (!hmm_allocate_dp_tables(h, docinfo_get_max_document_length(doc)))
		return FALSE;

	if (h->likelihood >= 0)
		hmm_initialize_random(h);
	else if (fabs(h->likelihood - h->old_likelihood) < tol) {
		return TRUE;
	}

	printf("Training HMM on data...\n");
	for (iter = 0; iter < max_iterations; iter++) {
		h->old_likelihood = h->likelihood;
		h->likelihood = hmm_iteration(h, doc);
		printf("Iteration %d: likelihood = %g\n",
		       iter + 1, h->likelihood);

		temp = h->ss;
		h->ss = h->ss2;
		h->ss2 = temp;

		temp = h->sw;
		h->sw = h->sw2;
		h->sw2 = temp;

		if ((iter % 10) == 9 && hmm_filename) {
			printf("Saving temporary HMM `%s'...\n", hmm_filename);
			if (!hmm_save_easy(h, hmm_filename))
				return FALSE;
		}

		if (h->old_likelihood < 0
		    && fabs(h->likelihood - h->old_likelihood) < tol)
			break;
	}
	if (hmm_filename) {
		printf("Saving HMM `%s'...\n", hmm_filename);
		if (!hmm_save_easy(h, hmm_filename))
			return FALSE;
	}

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
	double *td;
	unsigned int *ti;
	unsigned int j, l, r;

	ti = h->tmp_i;
	td = h->tmp_d;
	thresh = 1.0 / size;
	for (j = 0; j < size; j++) {
		ti[j] = j;
		td[j] = array[j];
	}
	xsort(ti, size, sizeof(unsigned int), &cmp_dbl_indirect, td);

	j = 0;
	l = 0;
	r = size - 1;
	while (l < r) {
		if (td[ti[l]] + td[ti[r]] < thresh) {
			opt_v[j] = td[ti[r]] / thresh;
			opt_i[2 * j] = ti[r];
			opt_i[2 * j + 1] = ti[r - 1];
			td[ti[r - 1]] += td[ti[r]] - thresh;
			r--;
			j++;
		} else {
			opt_v[j] = td[ti[l]] / thresh;
			opt_i[2 * j] = ti[l];
			opt_i[2 * j + 1] = ti[r];
			td[ti[r]] += td[ti[l]] - thresh;
			l++;
			j++;
		}
	}
	opt_v[j] = 1.0;
	opt_i[2 * j] = ti[l];
	opt_i[2 * j + 1] = ti[l];
}

static
int hmm_allocate_optimization_tables(hmm *h)
{
	size_t size, num;

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

	return TRUE;
}

int hmm_optimize_generator(hmm *h)
{
	unsigned int i, pos;

	if (!hmm_allocate_optimization_tables(h))
		return FALSE;

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

void hmm_generate_text(const hmm *h, const docinfo *doc)
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

int hmm_save(const hmm *h, FILE *fp)
{
	unsigned int nmemb;

	if (fwrite(&h->num_words, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fwrite(&h->num_documents, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fwrite(&h->num_states, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fwrite(&h->likelihood, sizeof(double), 1, fp) != 1)
		return FALSE;
	if (fwrite(&h->old_likelihood, sizeof(double), 1, fp) != 1)
		return FALSE;

	nmemb = h->num_states * h->num_states;
	if (fwrite(h->ss, sizeof(double), nmemb, fp) != nmemb)
		return FALSE;

	nmemb = h->num_states * h->num_words;
	if (fwrite(h->sw, sizeof(double), nmemb, fp) != nmemb)
		return FALSE;

	return TRUE;
}

int hmm_save_easy(const hmm *h, const char *filename)
{
	FILE *fp;
	int ret;

	fp = fopen(filename, "wb");
	if (!fp) {
		error("could not open `%s' for writing", filename);
		return FALSE;
	}
	ret = hmm_save(h, fp);
	fclose(fp);
	return ret;
}

int hmm_load(hmm *h, FILE *fp)
{
	unsigned int nmemb, num_words, num_documents, num_states;

	hmm_reset(h);
	if (!hmm_initialize(h))
		return FALSE;

	if (fread(&num_words, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fread(&num_documents, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fread(&num_states, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fread(&h->likelihood, sizeof(double), 1, fp) != 1)
		return FALSE;
	if (fread(&h->old_likelihood, sizeof(double), 1, fp) != 1)
		return FALSE;

	if (!hmm_allocate_tables(h, num_words, num_documents, num_states))
		return FALSE;

	nmemb = h->num_states * h->num_states;
	if (fread(h->ss, sizeof(double), nmemb, fp) != nmemb)
		goto error_load;

	nmemb = h->num_states * h->num_words;
	if (fread(h->sw, sizeof(double), nmemb, fp) != nmemb)
		goto error_load;

	return TRUE;

error_load:
	error("could not load HMM");
	hmm_cleanup(h);
	return FALSE;
}

int hmm_load_easy(hmm *h, const char *filename)
{
	FILE *fp;
	int ret;

	fp = fopen(filename, "rb");
	if (!fp) {
		error("could not open `%s' for reading", filename);
		return FALSE;
	}
	ret = hmm_load(h, fp);
	fclose(fp);
	return ret;
}

void hmm_print(const hmm *h, const docinfo *doc)
{
	unsigned int i, j, k, pos;

	printf("\nTrained HMM:\n");
	printf("SS:\n");
	for (i = 0; i < h->num_states; i++) {
		for (j = 0; j < h->num_states; j++) {
			pos = i * h->num_states + j;
			printf("%.3f ", h->ss[pos]);
		}
		printf("\n");
	}
	printf("SW:\n");
	for (i = 0; i < h->num_states; i++) {
		for (k = 0; k < h->num_words; k++) {
			pos = i * h->num_words + k;
			printf("%.3f ", h->sw[pos]);
		}
		printf("\n");
	}
	printf("Words:\n");
	for (k = 0; k < h->num_words; k++) {
		printf("%s ", docinfo_get_word(doc, k + 1));
	}
	printf("\n");
}

int hmm_build_cached(hmm *h, const char *hmm_file, const docinfo *doc,
                     unsigned int num_states, unsigned int max_iter,
                     double tol)
{
	FILE *fp = NULL;
	int ret;

	hmm_reset(h);
	if (hmm_file) fp = fopen(hmm_file, "rb");
	if (fp) {
		printf("Loading HMM `%s'...\n", hmm_file);
		ret = hmm_load(h, fp);
		fclose(fp);
		if (!ret) return FALSE;
	} else {
		if (!hmm_initialize(h))
			return FALSE;
	}

	if (!hmm_train(h, doc, num_states, max_iter, tol, hmm_file)) {
		hmm_cleanup(h);
		return FALSE;
	}

	if (h->num_states < 10) {
		hmm_print(h, doc);
	}

	return TRUE;
}

static
int do_main(const char *docinfo_file, const char *training_file,
            const char *ignore_file, const char *hmm_file,
            unsigned int num_states, unsigned int max_iter, double tol,
            unsigned int num_generated_texts)
{
	unsigned int i;
	docinfo doc;
	hmm h;

	docinfo_reset(&doc);
	hmm_reset(&h);

	if (!docinfo_build_cached(&doc, docinfo_file,
	                          training_file, ignore_file))
		goto error_main;

	if (!hmm_build_cached(&h, hmm_file, &doc,
	                      num_states, max_iter, tol))
		goto error_main;

	if (!hmm_optimize_generator(&h))
		goto error_main;

	for (i = 0; i < num_generated_texts; i++) {
		printf("Text %u:\n", i + 1);
		hmm_generate_text(&h, &doc);
		printf("\n\n");
	}

	docinfo_cleanup(&doc);
	hmm_cleanup(&h);
	return TRUE;

error_main:
	docinfo_cleanup(&doc);
	hmm_cleanup(&h);
	return FALSE;
}

int main(int argc, char **argv)
{
	char *docinfo_file, *hmm_file;
	char *training_file, *ignore_file;
	unsigned int num_states, max_iter;
	unsigned int num_generated_texts;
	double tol;
	option opts[] = {
		{ "-d", NULL, ARGTYPE_FILE,
		  "specify the DOCINFO file" },
		{ "-t", NULL, ARGTYPE_FILE,
		  "specify the training file" },
		{ "-i", NULL, ARGTYPE_FILE,
		  "specify the ignore file" },
		{ "-h", NULL, ARGTYPE_FILE,
		  "specify the HMM file" },
		{ "-q", NULL, ARGTYPE_UINT,
		  "the number of states" },
		{ "-m", NULL, ARGTYPE_UINT,
		  "the maximum number of iterations" },
		{ "-e", NULL, ARGTYPE_DBL,
		  "the tolerance for convergence" },
		{ "-n", NULL, ARGTYPE_UINT,
		  "the number of generated texts" },
		{ "--help", NULL, ARGTYPE_NONE,
		  "print this help" },
	};
	unsigned int num_opts;
	int ret;

#if 1
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
#endif

	opts[0].ptr = &docinfo_file;
	opts[1].ptr = &training_file;
	opts[2].ptr = &ignore_file;
	opts[3].ptr = &hmm_file;
	opts[4].ptr = &num_states;
	opts[5].ptr = &max_iter;
	opts[6].ptr = &tol;
	opts[7].ptr = &num_generated_texts;

	genrand_randomize();

	docinfo_file = NULL;
	hmm_file = NULL;
	training_file = NULL;
	ignore_file = NULL;
	num_states = 0;
	max_iter = 0;
	tol = 0;

	num_opts = sizeof(opts) / sizeof(option);
	if (argc == 1) {
		print_help(argv[0], opts, num_opts);
		return 0;
	}
	ret = process_args(argc, argv, opts, num_opts);
	if (ret <= 0) return ret;

	if (!do_main(docinfo_file, training_file, ignore_file,
	             hmm_file, num_states, max_iter, tol,
	             num_generated_texts))
		return -1;

	return 0;
}
