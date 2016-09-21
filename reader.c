
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "reader.h"
#include "utils.h"

#define INITIAL_BUFFER_SIZE 8192

void reader_reset(reader *r)
{
	r->filename = NULL;
	r->buffer = NULL;
	r->fp = NULL;
}

int reader_initialize(reader *r)
{
	reader_reset(r);

	r->buffer = xmalloc(INITIAL_BUFFER_SIZE);
	if (!r->buffer) goto error_init;

	r->buffer_length = 0;
	r->buffer_capacity = INITIAL_BUFFER_SIZE;

	return TRUE;
error_init:
	reader_cleanup(r);
	return FALSE;
}

int reader_open(reader *r, const char *filename)
{
	r->filename = xstrdup(filename);
	if (!r->filename) goto error_open;

	r->fp = fopen(filename, "rb");
	if (!r->fp) {
		error("could not open `%s' for reading", filename);
		goto error_open;
	}

	r->eof = FALSE;
	return TRUE;

error_open:
	reader_close(r);
	return FALSE;
}

void reader_close(reader *r)
{
	if (r->filename) {
		free(r->filename);
		r->filename = NULL;
	}
	if (r->fp) {
		fclose(r->fp);
		r->fp = NULL;
	}
}

void reader_cleanup(reader *r)
{
	reader_close(r);
	if (r->buffer) {
		free(r->buffer);
		r->buffer = NULL;
	}
}

static
int reader_putc(reader *r, char c)
{
	if (r->buffer_length >= r->buffer_capacity) {
		void *ptr = xrealloc(r->buffer, 2 * r->buffer_capacity);
		if (!ptr) return FALSE;
		r->buffer_capacity *= 2;
		r->buffer = (char *) ptr;
	}
	r->buffer[r->buffer_length++] = c;
	return TRUE;
}

char *reader_read(reader *r)
{
	int c, started;

	started = FALSE;
	r->buffer_length = 0;
	while (!r->eof) {
		c = fgetc(r->fp);
		if (c == EOF) {
			r->eof = TRUE;
			break;
		}
		if (isspace(c)) {
			if (started) break;
		} else {
			started = TRUE;
			if (!reader_putc(r, (char) c))
				return NULL;
		}
	}
	r->buffer[r->buffer_length] = '\0';
	return r->buffer;
}
