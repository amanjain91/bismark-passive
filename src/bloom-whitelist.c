#include "bloom-whitelist.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bloom_whitelist_init(bloom_whitelist_t* bloom) {
    FILE* fp;

    if(NULL == (fp = fopen(PATH_TO_FILTER, "rb"))) {
        perror("Error opening bloom filter file.\n");
        return -1;
    }
    printf("CHECK: BLOOMSIZE = %d, NUMHASH = %d\n", BLOOMSIZE, NUMHASH);
    if(NULL == (bloom = bloom_create(fp, BLOOMSIZE, NUMHASH, sax_hash, sdbm_hash))) {
        perror("Error creating bloom filter.\n");
        return -1;
    }
    fclose(fp);

    return 0;
}

void bloom_whitelist_destroy(bloom_whitelist_t* bloom) {
    free(bloom->a);
    free(bloom->funcs);
    free(bloom);
    printf("Destroy\n");
}

int bloom_whitelist_lookup(bloom_whitelist_t* bloom,
                                                        const char* const domain) {
    int res;
    res = bloom_check(bloom, domain);
    printf("domain: %s, exists: %d\n", domain, res);
    return res;

    /* TODO search for domains even if prepended by "mail.", "www.", "ns." etc. */
    /*
    int n;

    for (n=0; n<bloom->nfuncs; ++n) {
        if (!(GET_BIT(bloom->a, bloom->funcs[n](domain)%bloom->asize))) return -1;
    }
    p = strtok(domain, "www.");
    while (p != NULL) {
        for (n=0; n<bloom->nfuncs; ++n) {
            if (!(GET_BIT(bloom->a, bloom->funcs[n](p)%bloom->asize))) return -1;
        }
        p = strtok(NULL, "www.");
    }
    return 0;
    */
}
