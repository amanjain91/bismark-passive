#include "bloom-filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SETBIT(a, n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GETBIT(a, n) (a[n/CHAR_BIT] & (1<<(n%CHAR_BIT)))

bloom_filter_t* bf_init(size_t bfsize, size_t num_hash_func, ...) {
    bloom_filter_t* bloom_filter;
    va_list l;
    int n;
    if (!(bloom_filter = malloc(sizeof(bloom_filter_t)))) return NULL;
    if (!(bloom_filter->bf = calloc((size+CHAR_BIT-1)/CHAR_BIT, sizeof(char)))) {
        free(bloom_filter);
        return NULL;
    }
    if (!(bloom->hf = (hashfunc_t*)malloc(num_hash_func *sizeof(hashfunc_t)))) {
        free(bloom->bf);
        free(bloom);
        return NULL;
    }

    va_start(l, num_hash_func);
    for(n=0; n<num_hash_func; ++n) {
        bloom_filter->hf[n] = va_arg(l, hashfunc_t);
    }
    va_end(l);

    /* number of hash functions = 10 */
    bloom_filter->num_hash_func = num_hash_func;
    /* bloom filter size ~ 15000 */
    bloom_filter->bfsize = bfsize;

    /* set timestamp and version to 0 - update when we download new filter */
    bloom_filter->ts = 0;
    // bloom_filter->version = 0;

    return bloom_filter
}

int bloom_filter_destroy(bloom_filter_t* bloom_filter) {
    free(bloom_filter->bf);
    free(bloom->hf);
    free(bloom);

    return 0;
}

/* check for update by comparing ts
 * if needed GET and load new bloom filter
 * return 0 if updated else return -1
 * Can use curl option to download file only if it has changed*/
int check_for_update(bloom_filter_t* bloom_filter) {
    /* check TS */
    int curr_time = (int)time(NULL)
    if ((bloom_filter-> ts - curr_time) > TIME_DIFF) {
        /* TODO use curl download option from #L 105 */
        // TODO https://github.com/projectbismark/bismark-packages/blob/master/utils/bismark-experiments-manager/files/usr/bin/merge-experiments#L105
        
        return 0
    }
    return -1
}


/*
 * Add element to bloom filter
int* add_domain(const bloom_filter_t* bloom_filter, const char* domain) {
    //int match_length = strlen(domain);
    size_t n;
    for(n=0; n<bloom_filter->num_hash_func; ++n) {
        SETBIT(bloom_filter->bf, bloom_filter->hf[n](domain)%bloom_flter->bfsize);
    }
    return 0;
}
*/

/* calculate the position array based on bloom filter hash functions - make sure only
 * correct subdomains are used to do this */
int check_domain(const bloom_filter_t* bloom_filter, const char* domain) {
    size_t n;
    for(n=0; n<bloom_filter->num_hash_func; ++n) {
        if(!(GETBIT(bloom_filter->bf, bloom_filter->hf[n](domain)%bloom_filter->bfsize))) return 0;
    }
    return 1;
}
