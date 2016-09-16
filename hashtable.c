
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"
#include "utils.h"

#define INITIAL_TABLE_SIZE    1024
#define INITIAL_ENTRIES_SIZE  1024
#define INITIAL_STRS_SIZE     8192

void hashtable_reset(hashtable *ht)
{
	ht->table = NULL;
	ht->entries = NULL;
	ht->strs = NULL;
}

int hashtable_initialize(hashtable *ht)
{
	size_t size;

	hashtable_reset(ht);

	size = INITIAL_TABLE_SIZE * sizeof(unsigned int);
	ht->table = (unsigned int *) xmalloc(size);
	if (!ht->table) goto error_init;

	size = INITIAL_ENTRIES_SIZE * sizeof(hashtable_entry);
	ht->entries = (hashtable_entry *) xmalloc(size);
	if (!ht->entries) goto error_init;

	ht->strs = (char *) xmalloc(INITIAL_STRS_SIZE);
	if (!ht->strs) goto error_init;

	ht->table_used = 0;
	ht->table_size = INITIAL_TABLE_SIZE;
	memset(ht->table, 0, ht->table_size * sizeof(unsigned int));

	ht->entries_length = 0;
	ht->entries_capacity = INITIAL_ENTRIES_SIZE;

	ht->strs_length = 0;
	ht->strs_capacity = INITIAL_STRS_SIZE;
	return TRUE;

error_init:
	hashtable_cleanup(ht);
	return FALSE;
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
	ht->table_used = 0;
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
	if (2 * ht->table_used >= ht->table_size) {
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
	ht->table_used++;

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
	return 1 + (unsigned int) (((size_t) diff) / sizeof(hashtable_entry));
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
