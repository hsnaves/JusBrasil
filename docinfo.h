
#ifndef __DOCINFO_H
#define __DOCINFO_H

#include <stdio.h>
#include "hashtable.h"
#include "reader.h"

/* Data structures and types */
typedef
struct docinfo_wordstats_st {
	unsigned int document;
	unsigned int word;
	unsigned int count;
	unsigned int next;
} docinfo_wordstats;

typedef
struct docinfo_document_st {
	unsigned int doc_id;
	unsigned int word_count;
	unsigned int words;
} docinfo_document;

typedef
struct docinfo_st {
	hashtable ht;
	hashtable ignored;
	reader r;

	unsigned int wordstats_capacity, wordstats_length;
	unsigned int documents_capacity, documents_length;
	unsigned int words_capacity, words_length;
	docinfo_wordstats *wordstats;
	docinfo_document *documents;
	unsigned int *words;
} docinfo;

/* Functions */
void docinfo_reset(docinfo *doc);
int docinfo_initialize(docinfo *doc);
void docinfo_cleanup(docinfo *doc);
void docinfo_clear(docinfo *doc);

int docinfo_add_default_ignored(docinfo *doc);
void docinfo_clear_ignored(docinfo *doc);
int docinfo_add_ignored(docinfo *doc, const char *word);
int docinfo_add(docinfo *doc, const char *str, unsigned int doc_id);
int docinfo_process_files(docinfo *doc, const char *directory,
                          unsigned int num_files);

unsigned int docinfo_num_documents(docinfo *doc);
unsigned int docinfo_num_different_words(docinfo *doc);
unsigned int docinfo_num_words(docinfo *doc);
unsigned int docinfo_num_wordstats(docinfo *doc);

docinfo_wordstats *docinfo_get_wordstats(docinfo *doc, unsigned int idx);
docinfo_document *docinfo_get_document(docinfo *doc, unsigned int idx);
const char *docinfo_get_word(docinfo *doc, unsigned int idx);
const char *docinfo_get_word_in_doc(docinfo *doc, docinfo_document *document,
                                    unsigned int idx);
unsigned int docinfo_get_max_document_length(docinfo *doc);

int docinfo_save(FILE *fp, docinfo *doc);
int docinfo_load(FILE *fp, docinfo *doc);

#endif /* __DOCINFO_H */