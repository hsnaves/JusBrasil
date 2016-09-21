
#ifndef __HASHTABLE_H
#define __HASHTABLE_H

#include <stdio.h>

/* Data structures and types */
typedef
union hashtable_val_st {
	int intval;
	unsigned int uintval;
	double dblval;
	void *ptrval;
} hashtable_val;

typedef
struct hashtable_entry_st {
	unsigned int hash;
	unsigned int str;
	unsigned int count;
	hashtable_val val;
	hashtable_val extra;
	unsigned int next;
} hashtable_entry;

typedef
struct hashtable_st {
	unsigned int table_size;
	unsigned int entries_capacity, entries_length;
	unsigned int strs_capacity, strs_length;
	unsigned int *table;
	hashtable_entry *entries;
	char *strs;
} hashtable;

typedef int (*hashtable_save_cb)(const hashtable *ht, FILE *fp,
                                 const hashtable_entry *entry, void *arg);
typedef int (*hashtable_load_cb)(hashtable *ht, FILE *fp,
                                 hashtable_entry *entry, void *arg);

/* Functions */
void hashtable_reset(hashtable *ht);
int hashtable_initialize(hashtable *ht);
void hashtable_cleanup(hashtable *ht);

void hashtable_clear(hashtable *ht);
void hashtable_clear_counters(hashtable *ht);

hashtable_entry *hashtable_find(hashtable *ht, const char *str, int add);

unsigned int hashtable_num_entries(const hashtable *ht);
hashtable_entry *hashtable_get_entry(const hashtable *ht, unsigned int idx);
unsigned int hashtable_get_entry_idx(const hashtable *ht,
                                     const hashtable_entry *entry);
const char *hashtable_str(const hashtable *ht, const hashtable_entry *entry);
unsigned long hashtable_hash(const char *str);

int hashtable_save(const hashtable *ht, FILE *fp,
                   hashtable_save_cb cb, void *arg);
int hashtable_save_easy(const hashtable *ht, const char *filename,
                        hashtable_save_cb cb, void *arg);
int hashtable_load(hashtable *ht, FILE *fp,
                   hashtable_load_cb cb, void *arg);
int hashtable_load_easy(hashtable *ht, const char *filename,
                        hashtable_load_cb cb, void *arg);

#endif /* __HASHTABLE_H */
