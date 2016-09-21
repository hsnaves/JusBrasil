
#ifndef __READER_H
#define __READER_H

#include <stdio.h>

/* Data structures and types */
typedef
struct reader_st {
	char *filename;
	char *buffer;
	unsigned int buffer_capacity;
	unsigned int buffer_length;
	int eof;
	FILE *fp;
} reader;

/* Functions */
void reader_reset(reader *r);
int reader_initialize(reader *r);
int reader_open(reader *r, const char *filename);
void reader_close(reader *r);
void reader_cleanup(reader *r);
char *reader_read(reader *r);

#endif /* __READER_H */
