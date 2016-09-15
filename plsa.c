
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "plsa.h"
#include "hashtable.h"
#include "reader.h"
#include "utils.h"
#include "random.h"

void plsa_reset(plsa *pl)
{
	hashtable_reset(&pl->ht);
	reader_reset(&pl->r);
	pl->str_idx = NULL;
	pl->word_count = NULL;
	pl->dt = NULL;
	pl->dt2 = NULL;
	pl->tw = NULL;
	pl->tw2 = NULL;
}

int plsa_initialize(plsa *pl)
{
	plsa_reset(pl);

	if (!hashtable_initialize(&pl->ht)) goto error_init;
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
void plsa_cleanup_stats(plsa *pl)
{
	if (pl->str_idx) {
		free(pl->str_idx);
		pl->str_idx = NULL;
	}
	if (pl->word_count) {
		free(pl->word_count);
		pl->word_count = NULL;
	}
}

void plsa_cleanup(plsa *pl)
{
	hashtable_cleanup(&pl->ht);
	reader_cleanup(&pl->r);
	plsa_cleanup_tables(pl);
	plsa_cleanup_stats(pl);
}

int plsa_process_files(plsa *pl, const char *directory, unsigned int num_files)
{
	unsigned int i, j;
	char old_dir[PATH_MAX];
	char filename[PATH_MAX];
	hashtable_entry *entry;
	hashtable_docinfo *doc;
	hashtable *ht;
	char *token;
	size_t size;

	getcwd(old_dir, sizeof(old_dir));
	if (chdir(directory) < 0) {
		error("could not change directory to `%s'", directory);
		return FALSE;
	}

	ht = &pl->ht;
	hashtable_clear(ht);
	reader_close(&pl->r);
	plsa_cleanup_stats(pl);

	size = num_files * sizeof(unsigned int);
	pl->word_count = (unsigned int *) xmalloc(size);
	if (!pl->word_count) goto error_process;

	for (i = 0; i < num_files; i++) {
		sprintf(filename, "file_%d.txt", i + 1);
		if (access(filename, F_OK) == -1)
			continue;
		if (!reader_open(&pl->r, filename))
			goto error_process;

		pl->word_count[i] = 0;
		/* printf("Processing `%s'...\n", filename); */
		while (TRUE) {
			token = reader_read(&pl->r);
			if (!token) goto error_process;
			if (token[0] == '\0') break;

			pl->word_count[i]++;
			entry = hashtable_find(ht, token, TRUE);
			if (!entry) goto error_process;
			entry->count++;

			doc = hashtable_append_info(ht, entry, i + 1);
			if (!doc) goto error_process;
			doc->count++;
		}
		reader_close(&pl->r);
	}

	printf("Done reading!\n");
	pl->num_documents = num_files;
	pl->num_words = pl->ht.table_used;

	size = pl->num_words * sizeof(unsigned int);
	pl->str_idx = (unsigned int *) xmalloc(size);
	if (!pl->str_idx) goto error_process;

	j = 0;
	for (i = 0; i < ht->table_size; i++) {
		if (ht->table[i] == 0) continue;
		entry = &ht->entries[ht->table[i] - 1];
		pl->str_idx[j++] = entry->str;
	}

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
	unsigned int i, j, k, l, e, doc;
	unsigned pos, pos2;
	double tmp, den, likelihood, total_weight;
	hashtable *ht;
	hashtable_entry *entry;
	hashtable_docinfo *docinfo;
	size_t size;

	size = pl->num_documents * pl->num_topics * sizeof(double);
	memcpy(pl->dt2, pl->dt, size);
	memset(pl->dt, 0, size);

	size = pl->num_topics * pl->num_words * sizeof(double);
	memcpy(pl->tw2, pl->tw, size);
	memset(pl->tw, 0, size);

	likelihood = 0;
	total_weight = 0;
	ht = &pl->ht;
	j = 0;
	for (i = 0; i < ht->table_size; i++) {
		e = ht->table[i];
		while (e) {
			entry = &ht->entries[e - 1];
			doc = entry->docinfo;
			while (doc) {
				docinfo = &ht->docinfos[doc - 1];
				k = docinfo->document - 1;
				tmp = 0;
				for (l = 0; l < pl->num_topics; l++) {
					pos = k * pl->num_topics + l;
					pos2 = l * pl->num_words + j;
					tmp += pl->dt2[pos] * pl->tw2[pos2];
				}
				likelihood += docinfo->count * log(tmp);
				total_weight += docinfo->count;
				den = tmp * pl->word_count[k];

				for (l = 0; l < pl->num_topics; l++) {
					pos = k * pl->num_topics + l;
					pos2 = l * pl->num_words + j;
					pl->dt[pos] += docinfo->count
					  * pl->dt2[pos] * pl->tw2[pos2] / den;
					pl->tw[pos2] += docinfo->count
					  * pl->dt2[pos] * pl->tw2[pos2] / tmp;
				}
				doc = docinfo->next;
			}
			e = entry->next;
			j++;
		}
	}
	for (l = 0; l < pl->num_topics; l++) {
		tmp = 0;
		for (j = 0; j < pl->num_words; j++) {
			pos2 = l * pl->num_words + j;
			tmp += pl->tw[pos2];
		}
		for (j = 0; j < pl->num_words; j++) {
			pos2 = l * pl->num_words + j;
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
	old_likelihood = -100 * tol;
	for (iter = 0; iter < max_iterations; iter++) {
		likelihood = plsa_iteration(pl);
		printf("Iteration %d: likelihood = %g\n",
		       iter + 1, likelihood);
		if (fabs(likelihood - old_likelihood) < tol) break;
		old_likelihood = likelihood;
	}

	return TRUE;
}

int main(int argc, char **argv)
{
	plsa pl;
	genrand_randomize();
	if (!plsa_initialize(&pl))
		return -1;
	if (!plsa_process_files(&pl, "reuters/training", 14818))
		return -1;
	if (!plsa_compute(&pl, 100, 1000, 0.001))
		return -1;
	plsa_cleanup(&pl);
	return 0;
}