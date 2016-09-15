
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "plsa.h"
#include "hashtable.h"
#include "reader.h"
#include "utils.h"
#include "random.h"

void plsa_reset(plsa *pl)
{
	hashtable_reset(&pl->ht);
	reader_reset(&pl->r);
}

int plsa_initialize(plsa *pl)
{
	plsa_reset(pl);

	if (!hashtable_initialize(&pl->ht)) goto error_init;
	if (!reader_initialize(&pl->r)) goto error_init;

	return TRUE;
error_init:
	plsa_cleanup(pl);
	return FALSE;
}

void plsa_cleanup(plsa *pl)
{
	hashtable_cleanup(&pl->ht);
	reader_cleanup(&pl->r);
}

int plsa_process_files(plsa *pl, const char *directory, unsigned int num_files)
{
	unsigned int i, j;
	char old_dir[PATH_MAX];
	char filename[PATH_MAX];
	hashtable_entry *entry;
	hashtable *ht;
	char *token;

	getcwd(old_dir, sizeof(old_dir));
	if (chdir(directory) < 0) {
		error("could not change directory to `%s'", directory);
		return FALSE;
	}

	for (i = 0; i < num_files; i++) {
		sprintf(filename, "%d", i + 1);
		if (access(filename, F_OK) == -1)
			continue;
		if (!reader_open(&pl->r, filename))
			return FALSE;
		printf("Processing `%s'...\n", filename);
		while (TRUE) {
			token = reader_read(&pl->r);
			if (!token) return FALSE;
			if (token[0] == '\0') break;
			/* printf("%s\n", token); */
			entry = hashtable_find(&pl->ht, token, TRUE);
			if (!entry) return FALSE;
			entry->count++;
		}
		reader_close(&pl->r);
	}

	ht = &pl->ht;
	for(j = 0; j < ht->table_size; j++) {
		if (!ht->table[j]) continue;
		entry = &ht->entries[ht->table[j] - 1];
		printf("%s: %d\n", &ht->strs[entry->str - 1], entry->count);
	}

	if (chdir(old_dir) < 0) {
		error("could not change directory back to `%s'", old_dir);
		return FALSE;
	}
	return TRUE;
}

int main(int argc, char **argv)
{
	plsa pl;
	if (!plsa_initialize(&pl))
		return -1;
	if (!plsa_process_files(&pl, "reuters/training", 1000))
		return -1;
	plsa_cleanup(&pl);
	return 0;
}