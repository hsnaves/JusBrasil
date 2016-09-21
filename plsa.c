
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
double plsa_iteration(plsa *pl, docinfo *doc, double *dt, double *tw,
                      double *dt_old, double *tw_old)
{
	unsigned pos, pos2;
	unsigned int i, j, k, l, num_wordstats;
	double dotprod, val, sum, likelihood, total_weight;
	docinfo_wordstats *wordstats;
	docinfo_document *document;
	size_t size;

	if (dt) {
		size = pl->num_documents * pl->num_topics * sizeof(double);
		memset(dt, 0, size);
	}

	if (tw) {
		size = pl->num_topics * pl->num_words * sizeof(double);
		memset(tw, 0, size);
	}

	likelihood = 0;
	total_weight = 0;
	num_wordstats = docinfo_num_wordstats(doc);
	for (l = 0; l < num_wordstats; l++) {
		wordstats = docinfo_get_wordstats(doc, l + 1);
		k = wordstats->document - 1;
		i = wordstats->word - 1;
		document = docinfo_get_document(doc, wordstats->document);
		dotprod = 0;
		for (j = 0; j < pl->num_topics; j++) {
			pos = k * pl->num_topics + j;
			pos2 = j * pl->num_words + i;
			dotprod += dt_old[pos] * tw_old[pos2];
		}
		likelihood += wordstats->count * log(dotprod);
		total_weight += wordstats->count;

		for (j = 0; j < pl->num_topics; j++) {
			pos = k * pl->num_topics + j;
			pos2 = j * pl->num_words + i;
			val = wordstats->count * dt_old[pos]
			        * tw_old[pos2] / dotprod;
			if (dt) dt[pos] += val / document->word_count;
			if (tw) tw[pos2] += val;
		}
	}
	if (tw) {
		for (j = 0; j < pl->num_topics; j++) {
			sum = 0;
			for (i = 0; i < pl->num_words; i++) {
				pos2 = j * pl->num_words + i;
				sum += tw[pos2];
			}
			for (i = 0; i < pl->num_words; i++) {
				pos2 = j * pl->num_words + i;
				tw[pos2] /= sum;
			}
		}
	}
	return likelihood / total_weight;
}

static
int plsa_allocate_tables(plsa *pl, unsigned int num_words,
                         unsigned int num_documents, unsigned int num_topics)
{
	size_t size;

	plsa_cleanup_tables(pl);
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
	double *dt, *tw, *dt_old, *tw_old;
	unsigned int iter;
	size_t size;

	if (!plsa_allocate_tables(pl, docinfo_num_different_words(doc),
	                          docinfo_num_documents(doc), num_topics))
		return FALSE;

	plsa_initialize_random(pl);
	old_likelihood = 100 * tol;
	printf("Running PLSA on data...\n");
	for (iter = 0; iter < max_iterations; iter++) {
		if (iter & 1) {
			dt = pl->dt;
			dt_old = pl->dt2;
			tw = pl->tw;
			tw_old = pl->tw2;
		} else {
			dt = pl->dt2;
			dt_old = pl->dt;
			tw = pl->tw2;
			tw_old = pl->tw;
		}
		likelihood = plsa_iteration(pl, doc, dt, tw, dt_old, tw_old);
		printf("Iteration %d: likelihood = %g\n",
		       iter + 1, likelihood);

		if (fabs(likelihood - old_likelihood) < tol) {
			iter++;
			break;
		}
		old_likelihood = likelihood;
	}
	if (iter & 1) {
		size = pl->num_documents * pl->num_topics * sizeof(double);
		memcpy(pl->dt, pl->dt2, size);

		size = pl->num_topics * pl->num_words * sizeof(double);
		memcpy(pl->tw, pl->tw2, size);
	}
	return TRUE;
}

static
int cmp_topmost(const void *p1, const void *p2, void *arg)
{
	const plsa_topmost *t1 = (const plsa_topmost *) p1;
	const plsa_topmost *t2 = (const plsa_topmost *) p2;
	if (t1->val < t2->val) return 1;
	if (t1->val > t2->val) return -1;
	return 0;
}

static
int plsa_allocate_temporary(plsa *pl)
{
	size_t size;

	plsa_cleanup_temporary(pl);
	size = MAX(pl->num_words, pl->num_topics) * sizeof(plsa_topmost);
	pl->top = (plsa_topmost *) xmalloc(size);
	if (!pl->top) return FALSE;

	return TRUE;
}

int plsa_print_best(plsa *pl, const docinfo *doc, unsigned top_words,
                    unsigned int top_topics, unsigned int num_documents)
{
	unsigned int i, j, l;

	if (!plsa_allocate_temporary(pl))
		return FALSE;

	top_words = MIN(top_words, pl->num_words);
	for (l = 0; l < pl->num_topics; l++) {
		for (j = 0; j < pl->num_words; j++) {
			pl->top[j].idx = j;
			pl->top[j].val = pl->tw[l * pl->num_words + j];
		}
		xsort(pl->top, pl->num_words, sizeof(plsa_topmost),
		      &cmp_topmost, NULL);

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
		xsort(pl->top, pl->num_topics, sizeof(plsa_topmost),
		      &cmp_topmost, NULL);
		for (l = 0; l < top_topics; l++) {
			printf("%u: %.4f, ", pl->top[l].idx + 1,
			       pl->top[l].val);
		}
		printf("\n\n");
	}
	return TRUE;
}

int plsa_save(const plsa *pl, FILE *fp)
{
	unsigned int nmemb;

	if (fwrite(&pl->num_words, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fwrite(&pl->num_documents, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fwrite(&pl->num_topics, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;

	nmemb = pl->num_documents * pl->num_topics;
	if (fwrite(pl->dt, sizeof(double), nmemb, fp) != nmemb)
		return FALSE;

	nmemb = pl->num_topics * pl->num_words;
	if (fwrite(pl->tw, sizeof(double), nmemb, fp) != nmemb)
		return FALSE;

	return TRUE;
}

int plsa_save_easy(const plsa *pl, const char *filename)
{
	FILE *fp;
	int ret;

	fp = fopen(filename, "wb");
	if (!fp) {
		error("could not open `%s' for writing", filename);
		return FALSE;
	}
	ret = plsa_save(pl, fp);
	fclose(fp);
	return ret;
}

int plsa_load(plsa *pl, FILE *fp)
{
	unsigned int nmemb, num_words, num_documents, num_topics;

	plsa_reset(pl);
	if (!plsa_initialize(pl))
		return FALSE;

	if (fread(&num_words, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fread(&num_documents, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fread(&num_topics, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;

	if (!plsa_allocate_tables(pl, num_words, num_documents, num_topics))
		goto error_load;

	nmemb = pl->num_documents * pl->num_topics;
	if (fread(pl->dt, sizeof(double), nmemb, fp) != nmemb)
		goto error_load;

	nmemb = pl->num_topics * pl->num_words;
	if (fread(pl->tw, sizeof(double), nmemb, fp) != nmemb)
		goto error_load;

	return TRUE;

error_load:
	error("could not load PLSA");
	plsa_cleanup(pl);
	return FALSE;
}

int plsa_load_easy(plsa *pl, const char *filename)
{
	FILE *fp;
	int ret;

	fp = fopen(filename, "rb");
	if (!fp) {
		error("could not open `%s' for reading", filename);
		return FALSE;
	}
	ret = plsa_load(pl, fp);
	fclose(fp);
	return ret;
}

static
int train_dataset(const char *directory, unsigned int num_files,
                  unsigned int num_topics, unsigned int max_iter, double tol,
                  const char *docinfo_file, const char *plsa_file,
                  const char *ignore_file)
{
	docinfo doc;
	plsa pl;

	docinfo_reset(&doc);
	plsa_reset(&pl);

	if (!docinfo_initialize(&doc))
		goto error_train;

	if (!plsa_initialize(&pl))
		goto error_train;

	if (ignore_file) {
		if (!docinfo_add_ignored_from_file(&doc, ignore_file))
			goto error_train;
	}

	if (!docinfo_process_files(&doc, directory, num_files))
		goto error_train;

	printf("\n");
	if (!plsa_train(&pl, &doc, num_topics, max_iter, tol))
		goto error_train;

	printf("Saving PLSA to `%s'...\n", plsa_file);
	if (!plsa_save_easy(&pl, plsa_file))
		goto error_train;

	printf("Saving DOCINFO to `%s'...\n", docinfo_file);
	if (!docinfo_save_easy(&doc, docinfo_file))
		goto error_train;

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
	docinfo doc;
	plsa pl;

	docinfo_reset(&doc);
	plsa_reset(&pl);

	printf("Loading PLSA from `%s'...\n", plsa_file);
	if (!plsa_load_easy(&pl, plsa_file))
		goto error_print;

	printf("Loading DOCINFO from `%s'...\n", docinfo_file);
	if (!docinfo_load_easy(&doc, docinfo_file))
		goto error_print;

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

#if 0
	train_dataset("texts", 54562, 80, 1000, 0.001, "result.docinfo",
	              "result.plsa", "ignore.txt");
#else
	print_results("result.docinfo", "result.plsa", 30, 5, 100);
#endif
	return 0;
}
