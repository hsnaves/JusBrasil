
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "plsa.h"
#include "args.h"
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
void plsa_initialize_random(plsa *pl, int retrain_dt)
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
	if (retrain_dt) return;

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
double plsa_iteration(plsa *pl, const docinfo *doc,
                      int update_dt, int update_tw)
{
	unsigned pos, pos2;
	unsigned int i, j, k, l, num_wordstats;
	double dotprod, val, sum, likelihood, total_weight;
	docinfo_wordstats *wordstats;
	docinfo_document *document;
	size_t size;

	if (update_dt) {
		size = pl->num_documents * pl->num_topics * sizeof(double);
		memset(pl->dt2, 0, size);
	}

	if (update_tw) {
		size = pl->num_topics * pl->num_words * sizeof(double);
		memset(pl->tw2, 0, size);
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
			dotprod += pl->dt[pos] * pl->tw[pos2];
		}
		likelihood += wordstats->count * log(dotprod);
		total_weight += wordstats->count;

		for (j = 0; j < pl->num_topics; j++) {
			pos = k * pl->num_topics + j;
			pos2 = j * pl->num_words + i;
			val = wordstats->count * pl->dt[pos]
			        * pl->tw[pos2] / dotprod;
			if (update_dt)
				pl->dt2[pos] += val / document->word_count;
			if (update_tw)
				pl->tw2[pos2] += val;
		}
	}

	if (update_tw) {
		for (j = 0; j < pl->num_topics; j++) {
			sum = 0;
			for (i = 0; i < pl->num_words; i++) {
				pos2 = j * pl->num_words + i;
				sum += pl->tw2[pos2];
			}
			for (i = 0; i < pl->num_words; i++) {
				pos2 = j * pl->num_words + i;
				pl->tw2[pos2] /= sum;
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

	if (pl->num_documents != num_documents
	    || pl->num_topics != num_topics) {
		if (pl->dt) {
			free(pl->dt);
			pl->dt = NULL;
		}
		if (pl->dt2) {
			free(pl->dt2);
			pl->dt2 = NULL;
		}
	}

	if (pl->num_topics != num_topics || pl->num_words != num_words) {
		if (pl->tw) {
			free(pl->tw);
			pl->tw = NULL;
		}
		if (pl->tw2) {
			free(pl->tw2);
			pl->tw2 = NULL;
		}
	}

	pl->num_documents = num_documents;
	pl->num_words = num_words;
	pl->num_topics = num_topics;

	size = pl->num_documents * num_topics * sizeof(double);
	if (!pl->dt) {
		pl->dt = (double *) xmalloc(size);
		if (!pl->dt) return FALSE;
	}

	if (!pl->dt2) {
		pl->dt2 = (double *) xmalloc(size);
		if (!pl->dt2) return FALSE;
	}

	size = num_topics * pl->num_words * sizeof(double);
	if (!pl->tw) {
		pl->tw = (double *) xmalloc(size);
		if (!pl->tw) return FALSE;
	}

	if (!pl->tw2) {
		pl->tw2 = (double *) xmalloc(size);
		if (!pl->tw2) return FALSE;
	}

	return TRUE;
}

int plsa_train(plsa *pl, const docinfo *doc, unsigned int num_topics,
               unsigned int max_iterations, double tol, int retrain_dt)
{
	double likelihood, old_likelihood;
	double *temp;
	unsigned int iter;

	if (!plsa_allocate_tables(pl, docinfo_num_different_words(doc),
	                          docinfo_num_documents(doc), num_topics))
		return FALSE;

	plsa_initialize_random(pl, retrain_dt);

	old_likelihood = 100 * tol;
	printf("Running PLSA on data...\n");
	for (iter = 0; iter < max_iterations; iter++) {
		likelihood = plsa_iteration(pl, doc, TRUE, !retrain_dt);
		printf("Iteration %d: likelihood = %g\n",
		       iter + 1, likelihood);

		if (fabs(likelihood - old_likelihood) < tol) {
			iter++;
			break;
		}
		old_likelihood = likelihood;

		temp = pl->dt;
		pl->dt = pl->dt2;
		pl->dt2 = temp;

		if (!retrain_dt) {
			temp = pl->tw;
			pl->tw = pl->tw2;
			pl->tw2 = temp;
		}
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

int plsa_print_topics(plsa *pl, const docinfo *doc, unsigned top_words)
{
	unsigned int j, l;
	const char *token;

	if (!plsa_allocate_temporary(pl))
		return FALSE;

	top_words = MIN(top_words, pl->num_words);
	printf("Summary of topics:\n\n");
	for (l = 0; l < pl->num_topics; l++) {
		for (j = 0; j < pl->num_words; j++) {
			pl->top[j].idx = j;
			pl->top[j].val = pl->tw[l * pl->num_words + j];
		}
		xsort(pl->top, pl->num_words, sizeof(plsa_topmost),
		      &cmp_topmost, NULL);

		printf("\nTopic %d:\n", l + 1);
		for (j = 0; j < top_words; j++) {
			token = docinfo_get_word(doc,
			                         pl->top[j].idx + 1);
			printf("%s: %g\n", token, pl->top[j].val);
		}
	}
	printf("\n\n");
	return TRUE;
}

int plsa_print_documents(plsa *pl, const docinfo *doc, unsigned top_topics)
{
	unsigned int i, j, l;

	if (!plsa_allocate_temporary(pl))
		return FALSE;

	top_topics = MIN(top_topics, pl->num_topics);
	for (i = 0; i < pl->num_documents; i++) {
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

int plsa_build_cached(plsa *pl, const char *plsa_file, const docinfo *doc,
                      unsigned int num_topics, unsigned int max_iter,
                      double tol)
{
	FILE *fp;
	int ret;

	plsa_reset(pl);
	if (plsa_file) {
		fp = fopen(plsa_file, "rb");
		if (fp) {
			printf("Loading PLSA `%s'...\n", plsa_file);
			ret = plsa_load(pl, fp);
			fclose(fp);
			return ret;
		}
	}

	if (!plsa_initialize(pl))
		return FALSE;

	if (!plsa_train(pl, doc, num_topics, max_iter, tol, FALSE)) {
		plsa_cleanup(pl);
		return FALSE;
	}

	if (plsa_file) {
		printf("Saving PLSA `%s'...\n", plsa_file);
		if (!plsa_save_easy(pl, plsa_file)) {
			plsa_cleanup(pl);
			return FALSE;
		}
	}

	return TRUE;
}

static
int do_main(const char *docinfo_file, const char *training_file,
            const char *ignore_file, const char *plsa_file,
            unsigned int num_topics, unsigned int max_iter, double tol,
            unsigned int top_words, const char *test_file,
            unsigned int top_topics)
{
	docinfo doc;
	plsa pl;

	docinfo_reset(&doc);
	plsa_reset(&pl);

	if (!docinfo_build_cached(&doc, docinfo_file,
	                          training_file, ignore_file))
		goto error_main;

	if (!plsa_build_cached(&pl, plsa_file, &doc,
	                       num_topics, max_iter, tol))
		goto error_main;

	if (top_words > 0) {
		if (!plsa_print_topics(&pl,  &doc, top_words))
			goto error_main;
	}

	if (test_file) {
		docinfo_clear(&doc, TRUE);
		if (!docinfo_process_file(&doc, test_file, FALSE))
			goto error_main;

		if (!plsa_train(&pl, &doc, num_topics, max_iter, tol, TRUE))
			goto error_main;

		if (top_topics > 0) {
			if (!plsa_print_documents(&pl,  &doc, top_topics))
				goto error_main;
		}
	}

	docinfo_cleanup(&doc);
	plsa_cleanup(&pl);
	return TRUE;

error_main:
	docinfo_cleanup(&doc);
	plsa_cleanup(&pl);
	return FALSE;
}

int main(int argc, char **argv)
{
	char *docinfo_file, *plsa_file;
	char *training_file, *ignore_file;
	char *test_file;
        unsigned int top_words, top_topics;
	unsigned int num_topics, max_iter;
	double tol;
	option opts[] = {
		{ "-d", NULL, ARGTYPE_FILE,
		  "specify the DOCINFO file" },
		{ "-t", NULL, ARGTYPE_FILE,
		  "specify the training file" },
		{ "-i", NULL, ARGTYPE_FILE,
		  "specify the ignore file" },
		{ "-p", NULL, ARGTYPE_FILE,
		  "specify the PLSA file" },
		{ "-q", NULL, ARGTYPE_UINT,
		  "the number of topics" },
		{ "-m", NULL, ARGTYPE_UINT,
		  "the maximum number of iterations" },
		{ "-e", NULL, ARGTYPE_DBL,
		  "the tolerance for convergence" },
		{ "-w", NULL, ARGTYPE_UINT,
		  "the number of words per topic" },
		{ "-y", NULL, ARGTYPE_FILE,
		  "specify the test file" },
		{ "-z", NULL, ARGTYPE_UINT,
		  "the number of topics per document" },
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
	opts[3].ptr = &plsa_file;
	opts[4].ptr = &num_topics;
	opts[5].ptr = &max_iter;
	opts[6].ptr = &tol;
	opts[7].ptr = &top_words;
	opts[8].ptr = &test_file;
	opts[9].ptr = &top_topics;

	genrand_randomize();

	docinfo_file = NULL;
	plsa_file = NULL;
	training_file = NULL;
	ignore_file = NULL;
	test_file = NULL;
	top_words = 0;
	top_topics = 0;
	num_topics = 0;
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
	             plsa_file, num_topics, max_iter, tol,
	             top_words, test_file, top_topics))
		return -1;

	return 0;
}
