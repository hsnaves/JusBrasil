
#ifndef __HASHTABLE_H
#define __HASHTABLE_H

/* Data structures and types */
typedef
struct hashtable_docinfo_st {
	unsigned int document;
	unsigned int count;
	unsigned int next;
} hashtable_docinfo;

typedef
struct hashtable_entry_st {
	unsigned int hash;
	unsigned int str;
	unsigned int count;
	unsigned int docinfo;
	void *extra;
	unsigned int next;
} hashtable_entry;

typedef
struct hashtable_st {
	unsigned int table_size, table_used;
	unsigned int entries_capacity, entries_length;
	unsigned int docinfos_capacity, docinfos_length;
	unsigned int strs_capacity, strs_length;
	unsigned int *table;
	hashtable_entry *entries;
	hashtable_docinfo *docinfos;
	char *strs;
} hashtable;

/* Functions */
void hashtable_reset(hashtable *ht);
int hashtable_initialize(hashtable *ht);
void hashtable_cleanup(hashtable *ht);

hashtable_entry *hashtable_find(hashtable *ht, const char *str, int add);
hashtable_docinfo *hashtable_append_info(hashtable *ht, hashtable_entry *entry);
unsigned long hashtable_hash(const char *str);


#endif /* __HASHTABLE_H */