#include "bloom-whitelist.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define SETBIT(a, n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GETBIT(a, n) (a[n/CHAR_BIT] & (1<<(n%CHAR_BIT)))

/* hash functions for bloom filter */
unsigned int sax_hash(const char *key)
{
    unsigned int h=0;
    while(*key) h^=(h<<5)+(h>>2)+(unsigned char)*key++;
    return h;
}

unsigned int sdbm_hash(const char *key)
{
    unsigned int h=0;
    while(*key) h=(unsigned char)*key++ + (h<<6) + (h<<16) - h;
    return h;
}


void bloom_whitelist_init(bloom_whitelist_t* bloom) {
    bloom->asize = BLOOMSIZE;
    bloom->nfuncs = NUMHASH;
}

int bloom_whitelist_load(bloom_whitelist_t* bloom) {
    int nread;
    FILE* fp;

    /* if (DEBUG_BLOOM) printf("\t\t\t1. Addresses %d\t%d\n", bloom->a, bloom->funcs);
     * if (DEBUG_BLOOM) printf("Memory allocation %d, %d, %d\n", ((bloom->asize+CHAR_BIT-1)/CHAR_BIT), CHAR_BIT, sizeof(char));
     */
    /* Read downloaded bloom filter into array buffer */
    if(!(bloom->a=calloc(((bloom->asize+CHAR_BIT-1)/CHAR_BIT), sizeof(char)))) {
        /* free(bloom); */
        perror("Error initializing bloom filter\n");
        free(bloom->a);
        return -1;
    }
    if(NULL == (fp = fopen(PATH_TO_FILTER, "rb"))) {
        perror("Error opening bloom filter file.\n");
        return -1;
    }
    nread = fread(bloom->a, ((bloom->asize+CHAR_BIT-1)/CHAR_BIT), sizeof(char), fp);
    if (DEBUG_BLOOM) printf("CHECK wl_load(): read %d bytes into bloom->a.\n", nread);

    if(!(bloom->funcs=(hashfunc_t*)malloc(bloom->nfuncs*sizeof(hashfunc_t)))) {
        /* free(bloom); */
        perror("assign functions space error\n");
        return -1;
    }

    /* assign functions */
    bloom->funcs[0]=sax_hash;
    bloom->funcs[1]=sdbm_hash;
    /* if (DEBUG_BLOOM) printf("Print bloom = %d, nfuncs = %d, asize = %d\n", bloom, bloom->nfuncs, bloom->asize); */
    return 0;
}

void bloom_whitelist_destroy(bloom_whitelist_t* bloom) {
    free(bloom->a);
    free(bloom->funcs);
    /* free(bloom); */
    if (DEBUG_BLOOM) printf("Destroy\n");
}

int bloom_whitelist_lookup(bloom_whitelist_t* bloom, const char* const domain) {
    size_t n;
    if (DEBUG_BLOOM) {
        char str[80];
        sprintf(str, "\t\tSearch for %s\n", domain);
        perror(str);
    }
    /* if (DEBUG_BLOOM) printf("Enter bloom_whitelist_lookup(). bloom = %d, nfuncs = %d.\n", bloom, bloom->nfuncs); */
    for (n=0; n<bloom->nfuncs; ++n) {
        if (DEBUG_BLOOM) printf("Bits = %d\t", GETBIT(bloom->a, bloom->funcs[n](domain)%bloom->asize));
        if (!(GETBIT(bloom->a, bloom->funcs[n](domain)%bloom->asize))) return -1;
    }
    return 0;
}
