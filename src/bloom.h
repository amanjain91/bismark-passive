#ifndef BLOOM_WHITELIST_H
#define BLOOM_WHITELIST_H

#include<stdlib.h>
#include<zlib.h>

typedef unsigned int (*hashfunc_t)(const char *);
typedef struct {
	size_t asize;
	unsigned char *a;
	size_t nfuncs;
	hashfunc_t *funcs;
    int timestamp;
} BLOOM;

BLOOM* bloom_create(FILE* fp, size_t size, size_t nfuncs, ...);
int bloom_download();
int bloom_destroy(BLOOM *bloom);
/* return 1 if s in hash filter */
int bloom_check(BLOOM *bloom, const char *s);
BLOOM* bloomFilter();
/* int bloom_add(BLOOM *bloom, const char *s);
 * int bloom_dump(BLOOM *bloom, const char* filepath);
 */

#endif
