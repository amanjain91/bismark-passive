#include<limits.h>
#include<stdarg.h>
#include<stdio.h>
#include<string.h>

#include"bloom.h"
#include"hash-config.h"

#define SETBIT(a, n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GETBIT(a, n) (a[n/CHAR_BIT] & (1<<(n%CHAR_BIT)))


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

    /* printf("%d\n%d\n", (size+CHAR_BIT-1)/CHAR_BIT, sizeof(char)); */
    /* printf("Read %d bytes\n", nread); */

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

    return bloom;
}

int bloom_download()
{
    int res;
    res = system("curl -s -f -z filter.bin -O http://sites.noise.gatech.edu/~sarthak/files/bloomfilter/filter.bin");
    /* printf("Download result is %d\n", res); */

    return res;
}

int bloom_destroy(BLOOM *bloom)
{
    free(bloom->a);
    free(bloom->funcs);
    free(bloom);
    /* printf("Destroy\n"); */

    return 0;
}

int bloom_check(BLOOM *bloom, const char *s)
{
    size_t n;

    for(n=0; n<bloom->nfuncs; ++n) {
        if(!(GETBIT(bloom->a, bloom->funcs[n](s)%bloom->asize))) return 0;
    }
    /* printf("%s exists. return 1\n", s); */

    return 1;
}

/*
int bloom_add(BLOOM *bloom, const char *s)
{
    size_t n;
    printf("Add %s at time %d\n",s,(int)time(NULL));

    for(n=0; n<bloom->nfuncs; ++n) {
        SETBIT(bloom->a, bloom->funcs[n](s)%bloom->asize);
    }
    return 0;
}
*/

/* TODO ADD THIS FUNCTION TO main.c */
BLOOM* bloomFilter()
{
    FILE* fp;
    BLOOM *bloom;

    /* Download bloom filter */
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

    /* check block - enter name to check
    while(fgets(line, 1024, stdin)) {
        if((p=strchr(line, '\r'))) *p='\0';
        if((p=strchr(line, '\n'))) *p='\0';

        p=strtok(line, " \t,;\r\n!");
        while(p) {
            if(!bloom_check(bloom, p)) {
                printf("No match for word \"%s\"\n", p);
            }
            p=strtok(NULL, " \t,.;:\r\n?!-/()");
        }
    }

    bloom_destroy(bloom);
    */
