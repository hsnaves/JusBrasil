
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "utils.h"
#include "random.h"

void error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr) error("memory exhausted");
	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	void *nptr = realloc(ptr, size);
	if (!nptr) error("memory exhausted");
	return nptr;
}

char *xstrdup(const char *str)
{
	size_t size;
	char *s;

	size = strlen(str);
	s = xmalloc(size + 1);
	if (s) memcpy(s, str, size + 1);
	return s;
}

static
void swap_memory(void *ptr1, void *ptr2, size_t size)
{
	char *cptr1, *cptr2, tmp;
	char buffer[16];

	cptr1 = (char *) ptr1;
	cptr2 = (char *) ptr2;

	while (size >= sizeof(buffer)) {
		memcpy(buffer, cptr1, sizeof(buffer));
		memcpy(cptr1, cptr2, sizeof(buffer));
		memcpy(cptr2, buffer, sizeof(buffer));
		cptr1 += sizeof(buffer);
		cptr2 += sizeof(buffer);
		size -= sizeof(buffer);
	}

	while (size > 0) {
		tmp = *cptr1;
		*cptr1 = *cptr2;
		*cptr2 = tmp;
		cptr1++;
		cptr2++;
		size--;
	}
}

void xsort(void *ptr, size_t nmemb, size_t size,
           int (*cmpfunc)(const void *, const void *, void *), void *arg)
{
	unsigned int l, r, mid;
	char *cptr;
	int cmp;

	cptr = (char *) ptr;
	if (nmemb <= 1) return;
	if (nmemb < 4) {
		cmp = (*cmpfunc)(ptr, &cptr[size], arg);
		if (cmp > 0)
			swap_memory(ptr, &cptr[size], size);
		if (nmemb == 2) return;

		cmp = (*cmpfunc)(ptr, &cptr[2 * size], arg);
		if (cmp > 0)
			swap_memory(ptr, &cptr[2 * size], size);

		cmp = (*cmpfunc)(&cptr[size], &cptr[2 * size], arg);
		if (cmp > 0)
			swap_memory(&cptr[size], &cptr[2 * size], size);
		return;
	}
	l = 0;
	r = (unsigned int) (nmemb - 1);
	mid = genrand_int32() % ((unsigned int) nmemb);
	printf("mid = %u, nmemb = %u\n", mid, nmemb);
	while (TRUE) {
		while (l < r) {
			cmp = (*cmpfunc)(&cptr[l * size],
			                 &cptr[mid * size], arg);
			if (cmp > 0) break;
			l++;
		}
		if (l == r) break;

		while (l < r) {
			cmp = (*cmpfunc)(&cptr[mid * size],
			                 &cptr[r * size], arg);
			if (cmp > 0) break;
			r--;
		}
		if (l == r) break;

		swap_memory(&cptr[l * size], &cptr[r * size], size);
		if (l == mid) {
			mid = r;
		} else if (r == mid) {
			mid = l;
		}
		if (r == l + 1) break;
		l++;
		r--;
	}
	xsort(ptr, l + 1, size, cmpfunc, arg);
	xsort(&cptr[(l + 1) * size], nmemb - l - 1, size, cmpfunc, arg);
}
