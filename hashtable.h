
#ifndef __HASHTABLE_H
#define __HASHTABLE_H

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
	unsigned int table_size, table_used;
	unsigned int entries_capacity, entries_length;
	unsigned int strs_capacity, strs_length;
	unsigned int *table;
	hashtable_entry *entries;
	char *strs;
} hashtable;

/* Functions */
void hashtable_reset(hashtable *ht);
int hashtable_initialize(hashtable *ht);
void hashtable_cleanup(hashtable *ht);
void hashtable_clear(hashtable *ht);

hashtable_entry *hashtable_find(hashtable *ht, const char *str, int add);
hashtable_entry *hashtable_get_entry(hashtable *ht, unsigned int idx);
unsigned int hashtable_get_entry_idx(hashtable *ht, hashtable_entry *entry);
const char *hashtable_str(hashtable *ht, hashtable_entry *entry);
unsigned long hashtable_hash(const char *str);


#endif /* __HASHTABLE_H */