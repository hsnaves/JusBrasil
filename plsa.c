
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "plsa.h"
#include "docinfo.h"
#include "utils.h"
#include "random.h"

void plsa_reset(plsa *pl)
{
	pl->dt = NULL;
	pl->dt2 = NULL;
	pl->tw = NULL;
	pl->tw2 = NULL;
	pl->topmost = NULL;
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
	if (pl->topmost) {
		free(pl->topmost);
		pl->topmost = NULL;
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

int plsa_compute(plsa *pl, docinfo *doc, unsigned int num_topics,
                 unsigned int max_iterations, double tol)
{
	double likelihood, old_likelihood;
	unsigned int iter;
	size_t size;

	plsa_cleanup_tables(pl);

	pl->num_documents = docinfo_num_documents(doc);
	pl->num_words = docinfo_num_different_words(doc);
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

int plsa_print_best(plsa *pl, docinfo *doc, unsigned top_words)
{
	unsigned int i, j, l, tmp;
	size_t size;

	plsa_cleanup_temporary(pl);

	size = pl->num_words * sizeof(unsigned int);
	pl->topmost = (unsigned int *) xmalloc(size);
	if (!pl->topmost) return FALSE;

	for (j = 0; j < pl->num_words; j++) {
		pl->topmost[j] = j;
	}
	if (top_words > pl->num_words)
		top_words = pl->num_words;

	for (l = 0; l < pl->num_topics; l++) {
		memcpy(pl->tw2, &pl->tw[l * pl->num_words],
		       pl->num_words * sizeof(double));
		for (i = 0; i < top_words; i++) {
			for (j = i + 1; j < pl->num_words; j++) {
				if (pl->tw2[pl->topmost[i]] <
				    pl->tw2[pl->topmost[j]]) {
					tmp = pl->topmost[i];
					pl->topmost[i] = pl->topmost[j];
					pl->topmost[j] = tmp;
				}
			}
		}
		printf("\nTopic %d:\n", l + 1);
		for (j = 0; j < top_words; j++) {
			printf("%s: %g\n",
			       docinfo_get_word(doc, pl->topmost[j] + 1),
			       pl->tw2[pl->topmost[j]]);
		}
	}
	return TRUE;
}

int main(int argc, char **argv)
{
	docinfo doc;
	plsa pl;

#if 1
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
#endif

	genrand_randomize();
	docinfo_reset(&doc);
	plsa_reset(&pl);

	if (!docinfo_initialize(&doc))
		goto error_main;

	if (!plsa_initialize(&pl))
		goto error_main;

	if (!docinfo_add_default_ignored(&doc))
		goto error_main;

	if (!docinfo_process_files(&doc, "reuters/training", 14818))
		goto error_main;

	printf("\n");
	if (!plsa_compute(&pl, &doc, 100, 1000, 0.0001))
		goto error_main;

	if (!plsa_print_best(&pl,  &doc, 10))
		goto error_main;

	docinfo_cleanup(&doc);
	plsa_cleanup(&pl);
	return 0;

error_main:
	docinfo_cleanup(&doc);
	plsa_cleanup(&pl);
	return -1;
}