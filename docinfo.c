
#include <stdio.h>
#include <stdlib.h>

#include "docinfo.h"
#include "hashtable.h"
#include "utils.h"

#define INITIAL_WORDSTATS_SIZE  8192
#define INITIAL_DOCUMENTS_SIZE  1024
#define INITIAL_WORDS_SIZE      8192

void docinfo_reset(docinfo *doc)
{
	hashtable_reset(&doc->ht);
	doc->wordstats = NULL;
	doc->documents = NULL;
	doc->words = NULL;
}

int docinfo_initialize(docinfo *doc)
{
	size_t size;
	docinfo_reset(doc);

	if (!hashtable_initialize(&doc->ht)) goto error_init;

	size = INITIAL_WORDSTATS_SIZE * sizeof(docinfo_wordstats);
	doc->wordstats = (docinfo_wordstats *) xmalloc(size);
	if (!doc->wordstats) goto error_init;

	size = INITIAL_DOCUMENTS_SIZE * sizeof(docinfo_document);
	doc->documents = (docinfo_document *) xmalloc(size);
	if (!doc->documents) goto error_init;

	size = INITIAL_WORDS_SIZE * sizeof(unsigned int);
	doc->words = (unsigned int *) xmalloc(size);
	if (!doc->words) goto error_init;

	doc->wordstats_capacity = INITIAL_WORDSTATS_SIZE;
	doc->wordstats_length = 0;

	doc->documents_capacity = INITIAL_DOCUMENTS_SIZE;
	doc->documents_length = 0;

	doc->words_capacity = INITIAL_WORDS_SIZE;
	doc->words_length = 0;
	return TRUE;
error_init:
	docinfo_cleanup(doc);
	return FALSE;
}

void docinfo_cleanup(docinfo *doc)
{
	hashtable_cleanup(&doc->ht);
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

void docinfo_clear(docinfo *doc)
{
	hashtable_clear(&doc->ht);
	doc->wordstats_length = 0;
	doc->documents_length = 0;
	doc->words_length = 0;
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

int docinfo_add(docinfo *doc, const char *str, unsigned int doc_id)
{
	hashtable_entry *entry;
	docinfo_document *document;
	docinfo_wordstats *wordstats;
	unsigned int word_idx, entry_idx, document_idx, stats_idx;

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

	entry = hashtable_find(&doc->ht, str, TRUE);
	if (!entry) return FALSE;

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

unsigned int docinfo_num_documents(docinfo *doc)
{
	return doc->documents_length;
}

unsigned int docinfo_num_different_words(docinfo *doc)
{
	return doc->ht.entries_length;
}

unsigned int docinfo_num_words(docinfo *doc)
{
	return doc->words_length;
}

unsigned int docinfo_num_wordstats(docinfo *doc)
{
	return doc->wordstats_length;
}

docinfo_wordstats *docinfo_get_wordstats(docinfo *doc, unsigned int idx)
{
	return &doc->wordstats[idx - 1];
}

docinfo_document *docinfo_get_document(docinfo *doc, unsigned int idx)
{
	return &doc->documents[idx - 1];
}

const char *docinfo_get_word(docinfo *doc, unsigned int idx)
{
	hashtable_entry *entry;
	entry = hashtable_get_entry(&doc->ht, idx);
	return hashtable_str(&doc->ht, entry);
}