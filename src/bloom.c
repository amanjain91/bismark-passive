#include<limits.h>
#include<stdarg.h>
#include<stdio.h>
#include<string.h>

#include"bloom.h"

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

/* Initializer */
BLOOM *bloom_create(FILE* fptr, size_t size, size_t nfuncs, ...)
{
    BLOOM *bloom;
    va_list l;
    int n, nread;
    unsigned char *bf;

    if(!(bloom=malloc(sizeof(BLOOM)))) return NULL;
    if(!(bloom->funcs=(hashfunc_t*)malloc(nfuncs*sizeof(hashfunc_t)))) {
        free(bloom);
        return NULL;
    }
    if(!(bf=calloc((size+CHAR_BIT-1)/CHAR_BIT, sizeof(char)))) {
        free(bloom);
        free(bf);
        return NULL;
    }

    /* Read downloaded bloom filter into array buffer */
    nread = fread(bf, (size+CHAR_BIT-1)/CHAR_BIT, sizeof(char), fptr);

    printf("CHECK: read %d bytes into bf.\n", nread);
    printf("%d\n%d\n", (size+CHAR_BIT-1)/CHAR_BIT, sizeof(char));

    /* assign functions */
    va_start(l, nfuncs);
    for(n=0; n<nfuncs; ++n) {
        bloom->funcs[n]=va_arg(l, hashfunc_t);
    }
    va_end(l);

    bloom->nfuncs=nfuncs;
    bloom->asize=size;

    /* assign bloom->a */
    bloom->a = bf;

    printf("Bloom filter success.\n");
    return bloom;
}

/* TODO remove download from here and add it as a chron job */
int bloom_download()
{
    int res;
    res = system("curl -s -f -z filter.bin -O http://sites.noise.gatech.edu/~sarthak/files/bloomfilter/filter.bin");
    /* printf("Download result is %d\n", res); */

    return res;
}

/* Call bloom_check from bloom-whitelist.c */
int bloom_check(BLOOM *bloom, const char *s)
{
    size_t n;
    printf("Enter bloom_check()\n nfuncs = %d.", bloom->nfuncs);

    for(n=0; n<bloom->nfuncs; ++n) {
        printf("GETBIT: %s, %d\n", s, GETBIT(bloom->a, bloom->funcs[n](s)%bloom->asize));
        if(!(GETBIT(bloom->a, bloom->funcs[n](s)%bloom->asize))) return -1;
    }
    printf("%s exists. return 0\n", s);

    return 0;
}

/* shift bloomFilter() to bloom-whitelist.c
BLOOM* bloomFilter()
{
    FILE* fp;
    BLOOM *bloom;

    if(bloom_download()) {
        perror("Could not download filter from server.");
    }
    if (!(fp=fopen(PATH_TO_FILTER, "rb"))) {
        perror("Could not open bloom filter file.");
        return NULL;
    }

    if(!(bloom=bloom_create(fp, BLOOMSIZE, NUMHASH, sax_hash, sdbm_hash))) {
        fprintf(stderr, "ERROR: Could not create bloom filter\n");
        return NULL;
    }
    fclose(fp);

    return bloom;
}
*/
