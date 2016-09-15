
#ifndef __PLSA_H
#define __PLSA_H

#include "hashtable.h"
#include "reader.h"

/* Data structures and types */
typedef
struct plsa_st {
	reader r;
	hashtable ht;
} plsa;

/* Functions */
void plsa_reset(plsa *pl);
int plsa_initialize(plsa *pl);
void plsa_cleanup(plsa *pl);

int plsa_process_files(plsa *pl, const char *directory, unsigned int num_files);

#endif /* __PLSA_H */