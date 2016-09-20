
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "plsa.h"
#include "docinfo.h"
#include "utils.h"
#include "random.h"

#define EPS 1e-10

void plsa_reset(plsa *pl)
{
	pl->dt = NULL;
	pl->dt2 = NULL;
	pl->tw = NULL;
	pl->tw2 = NULL;
	pl->top = NULL;
}

int plsa_initialize(plsa *pl)
{
	plsa_reset(pl);
	return TRUE;
}

static
void plsa_cleanup_tables(plsa *pl)
{
	if (pl->dt) {
		free(pl->dt);
		pl->dt = NULL;
	}
	if (pl->dt2) {
		free(pl->dt2);
		pl->dt2 = NULL;
	}
	if (pl->tw) {
		free(pl->tw);
		pl->tw = NULL;
	}
	if (pl->tw2) {
		free(pl->tw2);
		pl->tw2 = NULL;
	}
}

static
void plsa_cleanup_temporary(plsa *pl)
{
	if (pl->top) {
		free(pl->top);
		pl->top = NULL;
	}
}

void plsa_cleanup(plsa *pl)
{
	plsa_cleanup_tables(pl);
	plsa_cleanup_temporary(pl);
}

static
void plsa_initialize_random(plsa *pl)
{
	unsigned int i, j, k, pos;
	double sum;

	for (i = 0; i < pl->num_documents; i++) {
		sum = 0;
		for (j = 0; j < pl->num_topics; j++) {
			pos = i * pl->num_topics + j;
			pl->dt[pos] = -log(genrand_real1());
			sum += pl->dt[pos];
		}
		for (j = 0; j < pl->num_topics; j++) {
			pos = i * pl->num_topics + j;
			pl->dt[pos] /= sum;
		}
	}
	for (j = 0; j < pl->num_topics; j++) {
		sum = 0;
		for (k = 0; k < pl->num_words; k++) {
			pos = j * pl->num_words + k;
			pl->tw[pos] = -log(genrand_real1());
			sum += pl->tw[pos];
		}
		for (k = 0; k < pl->num_words; k++) {
			pos = j * pl->num_words + k;
			pl->tw[pos] /= sum;
		}
	}
}

static
double plsa_iteration(plsa *pl, docinfo *doc)
{
	unsigned int i, j, k, l, num_wordstats;
	unsigned pos, pos2;
	double tmp, den, likelihood, total_weight;
	docinfo_wordstats *wordstats;
	docinfo_document *document;
	size_t size;

	size = pl->num_documents * pl->num_topics * sizeof(double);
	memcpy(pl->dt2, pl->dt, size);
	memset(pl->dt, 0, size);

	size = pl->num_topics * pl->num_words * sizeof(double);
	memcpy(pl->tw2, pl->tw, size);
	memset(pl->tw, 0, size);

	likelihood = 0;
	total_weight = 0;
	num_wordstats = docinfo_num_wordstats(doc);
	for (l = 0; l < num_wordstats; l++) {
		wordstats = docinfo_get_wordstats(doc, l + 1);
		k = wordstats->document - 1;
		i = wordstats->word - 1;
		document = docinfo_get_document(doc, wordstats->document);
		tmp = 0;
		for (j = 0; j < pl->num_topics; j++) {
			pos = k * pl->num_topics + j;
			pos2 = j * pl->num_words + i;
			tmp += pl->dt2[pos] * pl->tw2[pos2];
		}
		likelihood += wordstats->count * log(tmp);
		total_weight += wordstats->count;
		den = tmp * document->word_count;

		for (j = 0; j < pl->num_topics; j++) {
			pos = k * pl->num_topics + j;
			pos2 = j * pl->num_words + i;
			pl->dt[pos] += wordstats->count
			  * pl->dt2[pos] * pl->tw2[pos2] / den;
			pl->tw[pos2] += wordstats->count
			  * pl->dt2[pos] * pl->tw2[pos2] / tmp;
		}
	}
	for (j = 0; j < pl->num_topics; j++) {
		tmp = 0;
		for (i = 0; i < pl->num_words; i++) {
			pos2 = j * pl->num_words + i;
			tmp += pl->tw[pos2];
		}
		for (i = 0; i < pl->num_words; i++) {
			pos2 = j * pl->num_words + i;
			pl->tw[pos2] /= tmp;
		}
	}
	return likelihood / total_weight;
}

static
int plsa_allocate(plsa *pl, unsigned int num_words,
                  unsigned int num_documents, unsigned int num_topics)
{
	size_t size;

	pl->num_documents = num_documents;
	pl->num_words = num_words;
	pl->num_topics = num_topics;

	size = pl->num_documents * num_topics * sizeof(double);
	pl->dt = (double *) xmalloc(size);
	if (!pl->dt) return FALSE;

	pl->dt2 = (double *) xmalloc(size);
	if (!pl->dt2) return FALSE;

	size = num_topics * pl->num_words * sizeof(double);
	pl->tw = (double *) xmalloc(size);
	if (!pl->tw) return FALSE;

	pl->tw2 = (double *) xmalloc(size);
	if (!pl->tw2) return FALSE;

	return TRUE;
}
int plsa_train(plsa *pl, docinfo *doc, unsigned int num_topics,
               unsigned int max_iterations, double tol)
{
	double likelihood, old_likelihood;
	unsigned int iter;

	plsa_cleanup_tables(pl);
	if (!plsa_allocate(pl, docinfo_num_different_words(doc),
	                   docinfo_num_documents(doc), num_topics))
		return FALSE;

	plsa_initialize_random(pl);
	old_likelihood = 100 * tol;
	printf("Running PLSA on data...\n");
	for (iter = 0; iter < max_iterations; iter++) {
		likelihood = plsa_iteration(pl, doc);
		printf("Iteration %d: likelihood = %g\n",
		       iter + 1, likelihood);
		if (fabs(likelihood - old_likelihood) < tol) break;
		old_likelihood = likelihood;
	}
	return TRUE;
}

static
int cmp_topmost(const void *p1, const void *p2)
{
	const plsa_topmost *t1 = (const plsa_topmost *) p1;
	const plsa_topmost *t2 = (const plsa_topmost *) p2;
	if (t1->val < t2->val) return 1;
	if (t1->val > t2->val) return -1;
	return 0;
}

int plsa_print_best(plsa *pl, docinfo *doc, unsigned top_words,
                    unsigned int top_topics, unsigned int num_documents)
{
	unsigned int i, j, l;
	size_t size;

	plsa_cleanup_temporary(pl);
	size = MAX(pl->num_words, pl->num_topics) * sizeof(plsa_topmost);
	pl->top = (plsa_topmost *) xmalloc(size);
	if (!pl->top) return FALSE;

	top_words = MIN(top_words, pl->num_words);
	for (l = 0; l < pl->num_topics; l++) {
		for (j = 0; j < pl->num_words; j++) {
			pl->top[j].idx = j;
			pl->top[j].val = pl->tw[l * pl->num_words + j];
		}
		qsort(pl->top, pl->num_words, sizeof(plsa_topmost),
		      &cmp_topmost);

		printf("\nTopic %d:\n", l + 1);
		for (j = 0; j < top_words; j++) {
			printf("%s: %g\n",
			       docinfo_get_word(doc, pl->top[j].idx + 1),
			       pl->top[j].val);
		}
	}
	printf("\n\n");

	top_topics = MIN(top_topics, pl->num_topics);
	num_documents = MIN(num_documents, pl->num_documents);
	for (i = 0; i < num_documents; i++) {
		docinfo_document *document;

		printf("Document %u:\n", i + 1);
		document = docinfo_get_document(doc, i + 1);
		for (j = 0; j < document->word_count; j++) {
			printf("%s ",
			       docinfo_get_word_in_doc(doc, document, j + 1));
		}
		printf("\n");

		for (l = 0; l < pl->num_topics; l++) {
			pl->top[l].idx = l;
			pl->top[l].val = pl->dt[i * pl->num_topics + l];
		}
		qsort(pl->top, pl->num_topics, sizeof(plsa_topmost),
		      &cmp_topmost);
		for (l = 0; l < top_topics; l++) {
			printf("%u: %.4f, ", pl->top[l].idx + 1,
			       pl->top[l].val);
		}
		printf("\n\n");
	}
	return TRUE;
}

int plsa_save(FILE *fp, plsa *pl)
{
	unsigned int i, j, k, pos, pos2;

	fprintf(fp, "%u %u %u\n", pl->num_words,
	       pl->num_documents, pl->num_topics);
	for (i = 0; i < pl->num_documents; i++) {
		for (j = 0; j < pl->num_topics; j++) {
			pos = i * pl->num_topics + j;
			if (fabs(pl->dt[pos]) < EPS)
				fprintf(fp, "0 ");
			else
				fprintf(fp, "%g ", pl->dt[pos]);
		}
		fprintf(fp, "\n");
	}
	for (j = 0; j < pl->num_topics; j++) {
		for (k = 0; k < pl->num_words; k++) {
			pos2 = j * pl->num_words + k;
			if (fabs(pl->tw[pos2]) < EPS)
				fprintf(fp, "0 ");
			else
				fprintf(fp, "%g ", pl->tw[pos2]);
		}
		fprintf(fp, "\n");
	}
	return TRUE;
}

int plsa_load(FILE *fp, plsa *pl)
{
	unsigned int i, j, k, pos, pos2;
	unsigned int num_words, num_documents, num_topics;

	plsa_reset(pl);
	if (!plsa_initialize(plsa *pl))
		return FALSE;

	if (fscanf(fp, "%u %u %u\n", &num_words,
	           &num_documents, &num_topics) != 3)
		return FALSE;

	if (!plsa_allocate(pl, num_words, num_documents, num_topics))
		goto error_load;

	for (i = 0; i < pl->num_documents; i++) {
		for (j = 0; j < pl->num_topics; j++) {
			pos = i * pl->num_topics + j;
			if (fscanf(fp, "%lf", &pl->dt[pos]) != 1)
				goto error_load;
		}
	}
	for (j = 0; j < pl->num_topics; j++) {
		for (k = 0; k < pl->num_words; k++) {
			pos2 = j * pl->num_words + k;
			if (fscanf(fp, "%lf", &pl->tw[pos2]) != 1)
				goto error_load;
		}
	}
	return TRUE;

error_load:
	error("could not load PLSA");
	plsa_cleanup(pl);
	return FALSE;
}

static
int train_dataset(const char *directory, unsigned int num_files,
                  unsigned int num_topics, unsigned int max_iter, double tol,
                  const char *docinfo_file, const char *plsa_file)
{
	FILE *fp;
	docinfo doc;
	plsa pl;

	docinfo_reset(&doc);
	plsa_reset(&pl);

	if (!docinfo_initialize(&doc))
		goto error_train;

	if (!plsa_initialize(&pl))
		goto error_train;

	if (!docinfo_add_default_ignored(&doc))
		goto error_train;

	if (!docinfo_process_files(&doc, directory, num_files))
		goto error_train;

	printf("\n");
	if (!plsa_train(&pl, &doc, num_topics, max_iter, tol))
		goto error_train;

	fp = fopen(plsa_file, "wb");
	if (!fp) {
		error("could not save `%s'", plsa_file);
		goto error_train;
	}
	plsa_save(fp, &pl);
	fclose(fp);

	fp = fopen(docinfo_file, "wb");
	if (!fp) {
		error("could not save `%s'", docinfo_file);
		goto error_train;
	}
	docinfo_save(fp, &doc);
	fclose(fp);

	docinfo_cleanup(&doc);
	plsa_cleanup(&pl);
	return TRUE;

error_train:
	docinfo_cleanup(&doc);
	plsa_cleanup(&pl);
	return FALSE;
}

static
int print_results(const char *docinfo_file, const char *plsa_file,
                  unsigned int top_words, unsigned int top_topics,
                  unsigned int num_documents)
{
	FILE *fp;
	docinfo doc;
	plsa pl;

	docinfo_reset(&doc);
	plsa_reset(&pl);

	fp = fopen(plsa_file, "rb");
	if (!fp) {
		error("could not load `%s'", plsa_file);
		goto error_print;
	}
	if (!plsa_load(fp, &pl)) {
		fclose(fp);
		goto error_print;
	}
	fclose(fp);

	fp = fopen(docinfo_file, "rb");
	if (!fp) {
		error("could not load `%s'", docinfo_file);
		goto error_print;
	}
	if (!docinfo_load(fp, &doc)) {
		fclose(fp);
		goto error_print;
	}
	fclose(fp);

	if (!plsa_print_best(&pl,  &doc, top_words, top_topics, num_documents))
		goto error_print;

	return TRUE;

error_print:
	docinfo_cleanup(&doc);
	plsa_cleanup(&pl);
	return FALSE;
}


int main(int argc, char **argv)
{
#if 1
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
#endif

	genrand_randomize();

#if 1
	train_dataset("texts", 54562, 80, 1000, 0.001, "result.docinfo",
	              "result.plsa");
#else
	print_results("result.docinfo", "result.plsa", 30, 5, 100);
#endif
	return 0;
}