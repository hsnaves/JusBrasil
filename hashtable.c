
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"
#include "utils.h"

#define INITIAL_TABLE_SIZE        1024
#define INITIAL_ENTRIES_CAPACITY  1024
#define INITIAL_STRS_CAPACITY     8192

void hashtable_reset(hashtable *ht)
{
	ht->table = NULL;
	ht->entries = NULL;
	ht->strs = NULL;
}

static
int hashtable_initialize_aux(hashtable *ht, unsigned int table_size,
                             unsigned int entries_capacity,
                             unsigned int strs_capacity)
{
	size_t size;

	hashtable_reset(ht);

	size = table_size * sizeof(unsigned int);
	ht->table = (unsigned int *) xmalloc(size);
	if (!ht->table) goto error_init;

	size = entries_capacity * sizeof(hashtable_entry);
	ht->entries = (hashtable_entry *) xmalloc(size);
	if (!ht->entries) goto error_init;

	ht->strs = (char *) xmalloc(strs_capacity);
	if (!ht->strs) goto error_init;

	ht->table_size = table_size;
	memset(ht->table, 0, ht->table_size * sizeof(unsigned int));

	ht->entries_length = 0;
	ht->entries_capacity = entries_capacity;

	ht->strs_length = 0;
	ht->strs_capacity = strs_capacity;
	return TRUE;

error_init:
	hashtable_cleanup(ht);
	return FALSE;
}

int hashtable_initialize(hashtable *ht)
{
	return hashtable_initialize_aux(ht, INITIAL_TABLE_SIZE,
	                                INITIAL_ENTRIES_CAPACITY,
	                                INITIAL_STRS_CAPACITY);
}

void hashtable_cleanup(hashtable *ht)
{
	if (ht->table) {
		free(ht->table);
		ht->table = NULL;
	}
	if (ht->entries) {
		free(ht->entries);
		ht->entries = NULL;
	}
	if (ht->strs) {
		free(ht->strs);
		ht->strs = NULL;
	}
}

void hashtable_clear(hashtable *ht)
{
	ht->entries_length = 0;
	ht->strs_length = 0;
	memset(ht->table, 0, ht->table_size * sizeof(unsigned int));
}

static
unsigned int hashtable_new_entry(hashtable *ht)
{
	if (ht->entries_length == ht->entries_capacity) {
		size_t size;
		void *ptr;

		size = 2 * ht->entries_capacity * sizeof(hashtable_entry);
		ptr = xrealloc(ht->entries, size);

		if (!ptr) return 0;
		ht->entries = (hashtable_entry *) ptr;
		ht->entries_capacity *= 2;
	}
	return ++(ht->entries_length);
}

static
unsigned int hashtable_new_str(hashtable *ht, const char *str)
{
	unsigned int len, pos;
	len = (unsigned int) strlen(str);
	while(ht->strs_length + len >= ht->strs_capacity) {
		size_t size;
		void *ptr;

		size = 2 * ht->strs_capacity;
		ptr = xrealloc(ht->strs, size);

		if (!ptr) return 0;
		ht->strs = (char *) ptr;
		ht->strs_capacity *= 2;
	}
	pos = ht->strs_length;
	memcpy(&ht->strs[pos], str, len + 1);
	ht->strs_length += len + 1;
	return pos + 1;
}

static
int hashtable_rehash(hashtable *ht)
{
	unsigned int i, idx;
	unsigned int new_size, *new_table;
	hashtable_entry *entry;

	new_size = 2 * ht->table_size;
	new_table = (unsigned int *) xmalloc(new_size * sizeof(unsigned int));
	if (!new_table) return FALSE;
	free(ht->table);

	ht->table = new_table;
	ht->table_size = new_size;
	memset(new_table, 0, new_size * sizeof(unsigned int));
	for (i = 0; i < ht->entries_length; i++) {
		entry = &ht->entries[i];
		idx = (entry->hash % new_size);
		entry->next = new_table[idx];
		new_table[idx] = i + 1;
	}
	return TRUE;
}

hashtable_entry *hashtable_find(hashtable *ht, const char *str, int add)
{
	unsigned long hash;
	unsigned int idx, e;
	hashtable_entry *entry;
	unsigned int str_pos;

	hash = hashtable_hash(str);
	idx = hash % ht->table_size;
	e = ht->table[idx];
	while (e) {
		entry = &ht->entries[e - 1];
		if (entry->hash == hash) {
			if (strcmp(&ht->strs[entry->str - 1], str) == 0) {
				if (add) entry->count++;
				return entry;
			}
		}
		e = entry->next;
	}
	if (!add) return NULL;
	if (2 * ht->entries_length >= ht->table_size) {
		if (!hashtable_rehash(ht)) return NULL;
		idx = hash % ht->table_size;
	}

	str_pos = hashtable_new_str(ht, str);
	if (!str_pos) return NULL;

	e = hashtable_new_entry(ht);
	if (!e) return NULL;

	entry = &ht->entries[e - 1];
	entry->hash = hash;
	entry->str = str_pos;
	entry->count = 1;
	entry->next = ht->table[idx];
	ht->table[idx] = e;

	return entry;
}

hashtable_entry *hashtable_get_entry(hashtable *ht, unsigned int idx)
{
	return &ht->entries[idx - 1];
}

unsigned int hashtable_get_entry_idx(hashtable *ht, hashtable_entry *entry)
{
	ptrdiff_t diff;
	diff = entry - ht->entries;
	return 1 + (unsigned int) (diff);
}

const char *hashtable_str(hashtable *ht, hashtable_entry *entry)
{
	if (!entry->str) return NULL;
	return &ht->strs[entry->str - 1];
}

unsigned long hashtable_hash(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + ((unsigned long) c);
	}
	return hash;
}

int hashtable_save(FILE *fp, hashtable *ht,
                   hashtable_callback_fn cb, void *arg)
{
	unsigned int i, j;
	hashtable_entry *entry;

	fprintf(fp, "%u\n", ht->table_size);
	fprintf(fp, "%u %u\n", ht->entries_length, ht->entries_capacity);
	fprintf(fp, "%u %u\n", ht->strs_length, ht->strs_capacity);

	for (i = 0; i < ht->entries_length; i++) {
		entry = &ht->entries[i];
		fprintf(fp, "%u %u %u ", entry->hash,
		        entry->str, entry->count);
		if (!cb(fp, ht, entry, arg))
			return FALSE;
		fprintf(fp, "\n");
	}

	for (j = 0; j < ht->strs_length; j++) {
		unsigned char c = (unsigned char) ht->strs[j];
		fprintf(fp, "%u ", (unsigned int) c);
	}
	fprintf(fp, "\n");

	return TRUE;
}

int hashtable_load(FILE *fp, hashtable *ht,
                   hashtable_callback_fn cb, void *arg)
{
	unsigned int i, j, idx;
	unsigned int table_size;
	unsigned int entries_length, strs_length;
	unsigned int entries_capacity, strs_capacity;
	hashtable_entry *entry;

	if (fscanf(fp, "%u", &table_size) != 1)
		return FALSE;

	if (fscanf(fp, "%u %u", &entries_length, &entries_capacity) != 2)
		return FALSE;

	if (fscanf(fp, "%u %u", &strs_length, &strs_capacity) != 2)
		return FALSE;

	if (!hashtable_initialize_aux(ht, table_size, entries_capacity,
	                              strs_capacity))
		return FALSE;

	hashtable_clear(ht);

	ht->entries_length = entries_length;
	for (i = 0; i < entries_length; i++) {
		entry = &ht->entries[i];
		if (fscanf(fp, "%u %u %u", &entry->hash, &entry->str,
		           &entry->count) != 3)
			goto error_load;

		if (!cb(fp, ht, entry, arg))
			goto error_load;

		idx = entry->hash % table_size;
		entry->next = ht->table[idx];
		ht->table[idx] = i + 1;
	}

	ht->strs_length = strs_length;
	for (j = 0; j < strs_length; j++) {
		unsigned int c;
		if (fscanf(fp, "%u", &c) != 1)
			goto error_load;
		ht->strs[j] = (char) c;
	}

	return TRUE;

error_load:
	error("could not load hash table");
	hashtable_cleanup(ht);
	return FALSE;
}
