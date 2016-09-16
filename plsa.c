
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "plsa.h"
#include "docinfo.h"
#include "hashtable.h"
#include "reader.h"
#include "utils.h"
#include "random.h"

void plsa_reset(plsa *pl)
{
	docinfo_reset(&pl->doc);
	hashtable_reset(&pl->ignored);
	reader_reset(&pl->r);
	pl->dt = NULL;
	pl->dt2 = NULL;
	pl->tw = NULL;
	pl->tw2 = NULL;
	pl->topmost = NULL;
}

int plsa_initialize(plsa *pl)
{
	plsa_reset(pl);

	if (!docinfo_initialize(&pl->doc)) goto error_init;
	if (!hashtable_initialize(&pl->ignored)) goto error_init;
	if (!reader_initialize(&pl->r)) goto error_init;

	return TRUE;
error_init:
	plsa_cleanup(pl);
	return FALSE;
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
	docinfo_cleanup(&pl->doc);
	hashtable_cleanup(&pl->ignored);
	reader_cleanup(&pl->r);
	plsa_cleanup_tables(pl);
	plsa_cleanup_temporary(pl);
}

void plsa_clear_ignored(plsa *pl)
{
	hashtable_clear(&pl->ignored);
}

int plsa_add_ignored(plsa *pl, const char *word)
{
	return (hashtable_find(&pl->ignored, word, TRUE) != NULL);
}

int plsa_process_files(plsa *pl, const char *directory, unsigned int num_files)
{
	unsigned int i;
	char old_dir[PATH_MAX];
	char filename[PATH_MAX];
	docinfo *doc;
	char *token;

	getcwd(old_dir, sizeof(old_dir));
	if (chdir(directory) < 0) {
		error("could not change directory to `%s'", directory);
		return FALSE;
	}

	doc = &pl->doc;
	docinfo_clear(doc);
	reader_close(&pl->r);

	for (i = 0; i < num_files; i++) {
		sprintf(filename, "file_%d.txt", i + 1);
		if (access(filename, F_OK) == -1)
			continue;
		if (!reader_open(&pl->r, filename))
			goto error_process;

		/* printf("Processing `%s'...\n", filename); */
		while (TRUE) {
			token = reader_read(&pl->r);
			if (!token) goto error_process;
			if (token[0] == '\0') break;
			if (hashtable_find(&pl->ignored, token, FALSE))
				continue;

			if (!docinfo_add(doc, token, i + 1))
				goto error_process;
		}
		reader_close(&pl->r);
	}

	printf("Done reading!\n");
	pl->num_documents = docinfo_num_documents(doc);
	pl->num_words = docinfo_num_different_words(doc);

	if (chdir(old_dir) < 0) {
		error("could not change directory back to `%s'", old_dir);
		return FALSE;
	}
	return TRUE;

error_process:
	if (chdir(old_dir) < 0)
		error("could not change directory back to `%s'", old_dir);
	return FALSE;
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
double plsa_iteration(plsa *pl)
{
	unsigned int i, j, k, l, num_wordstats;
	unsigned pos, pos2;
	double tmp, den, likelihood, total_weight;
	docinfo_wordstats *wordstats;
	docinfo_document *document;
	docinfo *doc;
	size_t size;

	size = pl->num_documents * pl->num_topics * sizeof(double);
	memcpy(pl->dt2, pl->dt, size);
	memset(pl->dt, 0, size);

	size = pl->num_topics * pl->num_words * sizeof(double);
	memcpy(pl->tw2, pl->tw, size);
	memset(pl->tw, 0, size);

	likelihood = 0;
	total_weight = 0;
	doc = &pl->doc;
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

int plsa_compute(plsa *pl, unsigned int num_topics,
                 unsigned int max_iterations, double tol)
{
	double likelihood, old_likelihood;
	unsigned int iter;
	size_t size;

	plsa_cleanup_tables(pl);

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
	for (iter = 0; iter < max_iterations; iter++) {
		likelihood = plsa_iteration(pl);
		printf("Iteration %d: likelihood = %g\n",
		       iter + 1, likelihood);
		if (fabs(likelihood - old_likelihood) < tol) break;
		old_likelihood = likelihood;
	}

	return TRUE;
}

int plsa_print_best(plsa *pl, unsigned top_words)
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
			       docinfo_get_word(&pl->doc, pl->topmost[j] + 1),
			       pl->tw2[pl->topmost[j]]);
		}
	}
	return TRUE;
}

int main(int argc, char **argv)
{
	static const char *ignored[] = {
		"a", "an", "the", "of", "to", "in", "and", "for", "at",
		"on", "is", "it", "''", "that", "this", "said", "not",
		"by", "was", "would", "with", "has", "from", "will",
		"its", "be", "as", "but", "he", "we", "are", "'s",
		"``", "or", "than", "were", "have", "which", "they",
		"any", 
	};
	unsigned int i;
	plsa pl;

#if 1
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
#endif

	genrand_randomize();
	if (!plsa_initialize(&pl))
		return -1;

	for (i = 0; i < sizeof(ignored) / sizeof(const char *); i++) {
		if (!plsa_add_ignored(&pl, ignored[i]))
			return -1;
	}

	if (!plsa_process_files(&pl, "reuters/training", 14818))
		return -1;
	if (!plsa_compute(&pl, 100, 1000, 0.0001))
		return -1;
	if (!plsa_print_best(&pl, 10))
		return -1;

	plsa_cleanup(&pl);
	return 0;
}