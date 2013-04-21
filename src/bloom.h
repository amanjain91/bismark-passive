#include<stdlib.h>
#include<stdio.h>
#include<zlib.h>

unsigned int sax_hash(const char *);
unsigned int sdbm_hash(const char *);

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
/* return 0 if matches, else -1 */
int bloom_check(BLOOM *bloom, const char *s);
/* BLOOM* bloomFilter();
 * int bloom_add(BLOOM *bloom, const char *s);
 * int bloom_dump(BLOOM *bloom, const char* filepath);
 */
