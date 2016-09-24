
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "docinfo.h"
#include "hashtable.h"
#include "utils.h"

#define INITIAL_WORDSTATS_CAPACITY  8192
#define INITIAL_DOCUMENTS_CAPACITY  1024
#define INITIAL_WORDS_CAPACITY      8192

void docinfo_reset(docinfo *doc)
{
	hashtable_reset(&doc->ht);
	hashtable_reset(&doc->ignored);
	reader_reset(&doc->r);
	doc->wordstats = NULL;
	doc->documents = NULL;
	doc->words = NULL;
}

static
int docinfo_initialize_aux(docinfo *doc, unsigned int wordstats_capacity,
                           unsigned int documents_capacity,
                           unsigned int words_capacity,
                           int initialize_hashtables)
{
	size_t size;
	docinfo_reset(doc);

	if (initialize_hashtables) {
		if (!hashtable_initialize(&doc->ht)) goto error_init;
		if (!hashtable_initialize(&doc->ignored)) goto error_init;
	}
	if (!reader_initialize(&doc->r)) goto error_init;

	size = wordstats_capacity * sizeof(docinfo_wordstats);
	doc->wordstats = (docinfo_wordstats *) xmalloc(size);
	if (!doc->wordstats) goto error_init;

	size = documents_capacity * sizeof(docinfo_document);
	doc->documents = (docinfo_document *) xmalloc(size);
	if (!doc->documents) goto error_init;

	size = words_capacity * sizeof(unsigned int);
	doc->words = (unsigned int *) xmalloc(size);
	if (!doc->words) goto error_init;

	doc->wordstats_capacity = wordstats_capacity;
	doc->wordstats_length = 0;

	doc->documents_capacity = documents_capacity;
	doc->documents_length = 0;

	doc->words_capacity = words_capacity;
	doc->words_length = 0;
	return TRUE;
error_init:
	docinfo_cleanup(doc);
	return FALSE;
}

int docinfo_initialize(docinfo *doc)
{
	return docinfo_initialize_aux(doc, INITIAL_WORDSTATS_CAPACITY,
	                              INITIAL_DOCUMENTS_CAPACITY,
	                              INITIAL_WORDS_CAPACITY, TRUE);
}

void docinfo_cleanup(docinfo *doc)
{
	hashtable_cleanup(&doc->ht);
	hashtable_cleanup(&doc->ignored);
	reader_cleanup(&doc->r);
	if (doc->wordstats) {
		free(doc->wordstats);
		doc->wordstats = NULL;
	}
	if (doc->documents) {
		free(doc->documents);
		doc->documents = NULL;
	}
	if (doc->words) {
		free(doc->words);
		doc->words = NULL;
	}
}

void docinfo_clear(docinfo *doc, int keep_strings)
{
	if (keep_strings) {
		hashtable_clear_counters(&doc->ht);
	} else {
		hashtable_clear(&doc->ht);
	}
	doc->wordstats_length = 0;
	doc->documents_length = 0;
	doc->words_length = 0;
}

void docinfo_clear_ignored(docinfo *doc)
{
	hashtable_clear(&doc->ignored);
}

int docinfo_add_ignored(docinfo *doc, const char *word)
{
	return (hashtable_find(&doc->ignored, word, TRUE) != NULL);
}

int docinfo_add_ignored_from_file(docinfo *doc, const char *filename)
{
	int ret = TRUE;
	char *token;
	reader_close(&doc->r);

	if (!reader_open(&doc->r, filename))
		return FALSE;

	while (TRUE) {
		token = reader_read(&doc->r);
		if (!token) {
			ret = FALSE;
			break;
		}
		if (token[0] == '\0') break;
		if (!docinfo_add_ignored(doc, token)) {
			ret = FALSE;
			break;
		}
	}
	reader_close(&doc->r);
	return ret;
}


static
unsigned int docinfo_new_wordstats(docinfo *doc)
{
	if (doc->wordstats_length == doc->wordstats_capacity) {
		size_t size;
		void *ptr;

		size = 2 * doc->wordstats_capacity * sizeof(docinfo_wordstats);
		ptr = xrealloc(doc->wordstats, size);

		if (!ptr) return 0;
		doc->wordstats = (docinfo_wordstats *) ptr;
		doc->wordstats_capacity *= 2;
	}
	return ++(doc->wordstats_length);
}

static
unsigned int docinfo_new_document(docinfo *doc)
{
	if (doc->documents_length == doc->documents_capacity) {
		size_t size;
		void *ptr;

		size = 2 * doc->documents_capacity * sizeof(docinfo_document);
		ptr = xrealloc(doc->documents, size);

		if (!ptr) return 0;
		doc->documents = (docinfo_document *) ptr;
		doc->documents_capacity *= 2;
	}
	return ++(doc->documents_length);
}

static
unsigned int docinfo_new_word(docinfo *doc)
{
	if (doc->words_length == doc->words_capacity) {
		size_t size;
		void *ptr;

		size = 2 * doc->words_capacity * sizeof(unsigned int);
		ptr = xrealloc(doc->words, size);

		if (!ptr) return 0;
		doc->words = (unsigned int *) ptr;
		doc->words_capacity *= 2;
	}
	return ++(doc->words_length);
}

int docinfo_add(docinfo *doc, const char *str, unsigned int doc_id,
                int add_to_hash)
{
	hashtable_entry *entry;
	docinfo_document *document;
	docinfo_wordstats *wordstats;
	unsigned int word_idx, entry_idx, document_idx, stats_idx;

	if (hashtable_find(&doc->ignored, str, FALSE))
		return TRUE;

	document = NULL;
	document_idx = 0;
	if (doc->documents_length > 0) {
		document = &doc->documents[doc->documents_length - 1];
		if (document->doc_id != doc_id)
			document = NULL;
		else
			document_idx = doc->documents_length;
	}
	if (!document) {
		document_idx = docinfo_new_document(doc);
		if (!document_idx) return FALSE;
		document = &doc->documents[document_idx - 1];
		document->doc_id = doc_id;
		document->word_count = 0;
		document->words = doc->words_length + 1;
	}

	entry = hashtable_find(&doc->ht, str, add_to_hash);
	if (!entry) {
		if (add_to_hash) return FALSE;
		else {
			error("could not find word `%s' in dictionary", str);
			return TRUE;
		}
	}

	document->word_count++;
	word_idx = docinfo_new_word(doc);
	if (!word_idx) return FALSE;

	entry_idx = hashtable_get_entry_idx(&doc->ht, entry);
	doc->words[word_idx - 1] = entry_idx;

	if (entry->count == 1) {
		stats_idx = docinfo_new_wordstats(doc);
		if (!stats_idx) return FALSE;
		entry->val.uintval = stats_idx;
		wordstats = &doc->wordstats[stats_idx - 1];
		wordstats->count = 1;
		wordstats->next = 0;
		wordstats->word = entry_idx;
		wordstats->document = document_idx;
	} else {
		stats_idx = entry->val.uintval;
		wordstats = &doc->wordstats[stats_idx - 1];
		if (wordstats->document != document_idx) {
			stats_idx = docinfo_new_wordstats(doc);
			if (!stats_idx) return FALSE;
			wordstats = &doc->wordstats[stats_idx - 1];
			wordstats->count = 0;
			wordstats->next = entry->val.uintval;
			wordstats->word = entry_idx;
			wordstats->document = document_idx;
			entry->val.uintval = stats_idx;
		}
		wordstats->count++;
	}
	return TRUE;
}

int docinfo_process_file(docinfo *doc, const char *master_file,
                         int add_to_hash)
{
	unsigned int j, doc_id = 0;
	char *token;
	int first;

	reader_close(&doc->r);
	if (!reader_open(&doc->r, master_file))
		return FALSE;

	first = TRUE;
	while (TRUE) {
		token = reader_read(&doc->r);
		if (!token) goto error_process;
		if (token[0] == '\0') break;
		if (first) {
			doc_id = strtoul(token, NULL, 10);
			first = FALSE;
			continue;
		}

		for(j = 0; token[j] == '-'; j++);
		if (token[j] == '\0' && j >= 8) {
			first = TRUE;
			continue;
		}
		if (!docinfo_add(doc, token, doc_id, add_to_hash))
			goto error_process;
	}
	reader_close(&doc->r);
	return TRUE;

error_process:
	reader_close(&doc->r);
	return FALSE;
}

unsigned int docinfo_num_documents(const docinfo *doc)
{
	return doc->documents_length;
}

unsigned int docinfo_num_different_words(const docinfo *doc)
{
	return hashtable_num_entries(&doc->ht);
}

unsigned int docinfo_num_words(const docinfo *doc)
{
	return doc->words_length;
}

unsigned int docinfo_num_wordstats(const docinfo *doc)
{
	return doc->wordstats_length;
}

docinfo_wordstats *docinfo_get_wordstats(const docinfo *doc, unsigned int idx)
{
	return &doc->wordstats[idx - 1];
}

docinfo_document *docinfo_get_document(const docinfo *doc, unsigned int idx)
{
	return &doc->documents[idx - 1];
}

const char *docinfo_get_word(const docinfo *doc, unsigned int idx)
{
	hashtable_entry *entry;
	entry = hashtable_get_entry(&doc->ht, idx);
	return hashtable_str(&doc->ht, entry);
}

unsigned int docinfo_get_wordidx_in_doc(const docinfo *doc,
                                        const docinfo_document *document,
                                        unsigned int idx)
{
	return doc->words[document->words + idx - 2];
}

const char *docinfo_get_word_in_doc(const docinfo *doc,
                                    const docinfo_document *document,
                                    unsigned int idx)
{
	unsigned int word;
	word = docinfo_get_wordidx_in_doc(doc, document, idx);
	return docinfo_get_word(doc, word);
}

unsigned int docinfo_get_max_document_length(const docinfo *doc)
{
	docinfo_document *document;
	unsigned int i, max_len;

	max_len = 0;
	for (i = 0; i < doc->documents_length; i++) {
		document = &doc->documents[i];
		max_len = MAX(max_len, document->word_count);
	}
	return max_len;
}

static
int hashtable_save_uintval(const hashtable *ht, FILE *fp,
                           const hashtable_entry *entry, void *arg)
{
	if (fwrite(&entry->val.uintval, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	return TRUE;
}

int docinfo_save(const docinfo *doc, FILE *fp)
{
	if (fwrite(&doc->wordstats_length, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fwrite(&doc->wordstats_capacity, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;

	if (fwrite(&doc->documents_length, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fwrite(&doc->documents_capacity, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;

	if (fwrite(&doc->words_length, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fwrite(&doc->words_capacity, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;

	if (!hashtable_save(&doc->ignored, fp,
	                    &hashtable_save_uintval, NULL))
		return FALSE;

	if (!hashtable_save(&doc->ht, fp,
	                    &hashtable_save_uintval, NULL))
		return FALSE;

	if (fwrite(doc->wordstats, sizeof(docinfo_wordstats),
	           doc->wordstats_length, fp) != doc->wordstats_length)
		return FALSE;

	if (fwrite(doc->documents, sizeof(docinfo_document),
	           doc->documents_length, fp) != doc->documents_length)
		return FALSE;

	if (fwrite(doc->words, sizeof(unsigned int),
	           doc->words_length, fp) != doc->words_length)
		return FALSE;

	return TRUE;
}

int docinfo_save_easy(docinfo *doc, const char *filename)
{
	FILE *fp;
	int ret;

	fp = fopen(filename, "wb");
	if (!fp) {
		error("could not open `%s' for writing", filename);
		return FALSE;
	}
	ret = docinfo_save(doc, fp);
	fclose(fp);
	return ret;
}

static
int hashtable_load_uintval(hashtable *ht, FILE *fp,
                           hashtable_entry *entry, void *arg)
{
	if (fread(&entry->val.uintval, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	return TRUE;
}

int docinfo_load(docinfo *doc, FILE *fp)
{
	unsigned int wordstats_length, wordstats_capacity;
	unsigned int documents_length, documents_capacity;
	unsigned int words_length, words_capacity;

	docinfo_reset(doc);

	if (fread(&wordstats_length, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fread(&wordstats_capacity, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;

	if (fread(&documents_length, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fread(&documents_capacity, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;

	if (fread(&words_length, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;
	if (fread(&words_capacity, sizeof(unsigned int), 1, fp) != 1)
		return FALSE;

	if (!docinfo_initialize_aux(doc, wordstats_capacity,
	                            documents_capacity, words_capacity, FALSE))
		return FALSE;

	if (!hashtable_load(&doc->ignored, fp,
	                    &hashtable_load_uintval, NULL))
		goto error_load;

	if (!hashtable_load(&doc->ht, fp,
	                    &hashtable_load_uintval, NULL))
		goto error_load;

	doc->wordstats_length = wordstats_length;
	if (fread(doc->wordstats, sizeof(docinfo_wordstats),
	          doc->wordstats_length, fp) != doc->wordstats_length)
		return FALSE;

	doc->documents_length = documents_length;
	if (fread(doc->documents, sizeof(docinfo_document),
	          doc->documents_length, fp) != doc->documents_length)
		return FALSE;

	doc->words_length = words_length;
	if (fread(doc->words, sizeof(unsigned int),
	          doc->words_length, fp) != doc->words_length)
		return FALSE;

	return TRUE;

error_load:
	error("could not load DOCINFO");
	docinfo_cleanup(doc);
	return FALSE;
}

int docinfo_load_easy(docinfo *doc, const char *filename)
{
	FILE *fp;
	int ret;

	fp = fopen(filename, "rb");
	if (!fp) {
		error("could not open `%s' for reading", filename);
		return FALSE;
	}
	ret = docinfo_load(doc, fp);
	fclose(fp);
	return ret;
}

int docinfo_build_cached(docinfo *doc, const char *docinfo_file,
                         const char *master_file, const char *ignore_file)
{
	FILE *fp;
	int ret;

	docinfo_reset(doc);
	if (docinfo_file) {
		fp = fopen(docinfo_file, "rb");
		if (fp) {
			printf("Loading DOCINFO `%s'...\n", docinfo_file);
			ret = docinfo_load(doc, fp);
			fclose(fp);
			return ret;
		}
	}

	if (!docinfo_initialize(doc))
		return FALSE;

	if (ignore_file) {
		if (!docinfo_add_ignored_from_file(doc, ignore_file)) {
			docinfo_cleanup(doc);
			return FALSE;
		}
	}

	printf("Building DOCINFO from `%s'...\n", master_file);
	if (!docinfo_process_file(doc, master_file, TRUE)) {
		docinfo_cleanup(doc);
		return FALSE;
	}
	printf("Num documents: %u\n", docinfo_num_documents(doc));
	printf("Num different words: %u\n", docinfo_num_different_words(doc));
	printf("Num wordstats: %u\n", docinfo_num_wordstats(doc));
	printf("Total word count: %u\n", docinfo_num_words(doc));

	if (docinfo_file) {
		printf("Saving DOCINFO `%s'...\n", docinfo_file);
		if (!docinfo_save_easy(doc, docinfo_file)) {
			docinfo_cleanup(doc);
			return FALSE;
		}
	}

	return TRUE;
}
