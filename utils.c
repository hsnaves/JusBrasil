
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
	char buffer[32];

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

	while (size >= sizeof(int)) {
		int *i1, *i2, tmp;
		i1 = (int *) cptr1;
		i2 = (int *) cptr2;
		tmp = i1[0];
		i1[0] = i2[0];
		i2[0] = tmp;
		cptr1 += sizeof(int);
		cptr2 += sizeof(int);
		size -= sizeof(int);
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
	int cmp, cmp1, cmp2;
	char *cptr;

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
	cmp1 = 1;
	cmp2 = 0;

partition:
	l = 0;
	r = (unsigned int) (nmemb - 1);
	mid = genrand_int32() % ((unsigned int) nmemb);
	while (r + 1 != l) {
		while (r + 1 != l) {
			cmp = (*cmpfunc)(&cptr[l * size],
			                 &cptr[mid * size], arg);
			if (cmp >= cmp1) break;
			l++;
		}
		if (r + 1 == l) break;

		while (r + 1 != l) {
			cmp = (*cmpfunc)(&cptr[mid * size],
			                 &cptr[r * size], arg);
			if (cmp >= cmp2) break;
			r--;
		}
		if (r + 1 == l) break;

		swap_memory(&cptr[l * size], &cptr[r * size], size);
		if (l == mid) {
			mid = r;
		} else if (r == mid) {
			mid = l;
		}
		l++;
		r--;
	}
	if (l == nmemb) {
		cmp1 = 0;
		cmp2 = 1;
		goto partition;
	} else if (l == 0) {
		return;
	} else {
		xsort(ptr, (size_t) l, size, cmpfunc, arg);
		xsort(&cptr[l * size], (size_t) (nmemb - l),
		      size, cmpfunc, arg);
	}
}
