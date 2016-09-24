
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

void docinfo_clear(docinfo *doc, int keep_strings);

void docinfo_clear_ignored(docinfo *doc);
int docinfo_add_ignored(docinfo *doc, const char *word);
int docinfo_add_ignored_from_file(docinfo *doc, const char *filename);

int docinfo_add(docinfo *doc, const char *str, unsigned int doc_id,
                int add_to_hash);
int docinfo_process_file(docinfo *doc, const char *master_file,
                         int add_to_hash);

unsigned int docinfo_num_documents(const docinfo *doc);
unsigned int docinfo_num_different_words(const docinfo *doc);
unsigned int docinfo_num_words(const docinfo *doc);
unsigned int docinfo_num_wordstats(const docinfo *doc);

docinfo_wordstats *docinfo_get_wordstats(const docinfo *doc, unsigned int idx);
docinfo_document *docinfo_get_document(const docinfo *doc, unsigned int idx);
const char *docinfo_get_word(const docinfo *doc, unsigned int idx);
unsigned int docinfo_get_wordidx_in_doc(const docinfo *doc,
                                        const docinfo_document *document,
                                        unsigned int idx);
const char *docinfo_get_word_in_doc(const docinfo *doc,
                                    const docinfo_document *document,
                                    unsigned int idx);

unsigned int docinfo_get_max_document_length(const docinfo *doc);

int docinfo_save(const docinfo *doc, FILE *fp);
int docinfo_save_easy(docinfo *doc, const char *filename);
int docinfo_load(docinfo *doc, FILE *fp);
int docinfo_load_easy(docinfo *doc, const char *filename);

int docinfo_build_cached(docinfo *doc, const char *docinfo_file,
                         const char *master_file, const char *ignore_file);


#endif /* __DOCINFO_H */
