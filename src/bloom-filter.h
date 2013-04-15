#ifndef _BISMARK_PASSIVE_BLOOM_FILTER_H_
#define _BISMARK_PASSIVE_BLOOM_FILTER_H_

#include <zlib.h>

const int ONE_MILLION = 1000000
// TODO should we do 2 million instead?
const int SIZE_BITS = ONE_MILLION
const int ONE_DAY = 24 * 60 * 60
// TODO is one day too large? reduce time diff?
const int TIME_DIFF = ONE_DAY

// TODO version not needed anymore due to curl update?
typedef unsigned int (*hashfunc_t) (const char *);
typedef struct {
    //int version;
    int ts;
    size_t num_hash_func;
    hashfunc_t *hf;
    size_t bfsize;
    unsigned char* bf;
} bloom_filter_t;

/* Initialize an empty bloom-filter. */
bloom_filter_t* bf_init(size_t bfsize, size_t num_hash_func, ...);

/* destructor */
int bloom_filter_destroy(bloom_filter_t* bloom_filter);

/* TODO Check server for timestamp and version.
 * Update once a day. */
int check_for_update(bloom_filter_t* bloom_filter);

// int add_domain(bloom_filter_t* bloom_filter, const char* domain);
/* Check for hashed bits of bloom filter array for particular domain.
 * Subdomain matching should work correctly, i.e. www.foo.com and foo.com
 * hash to the same correct positions.
 * Need to use appropriate hash functions here. 
 * Returns TRUE if domain exists in bf else FALSE */
int check_domain(const bloom_filter_t* bloom_filter, const char* domain);

#endif
