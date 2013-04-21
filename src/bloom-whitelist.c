#include "bloom-whitelist.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bloom_whitelist_init(bloom_whitelist_t* bloom) {
    FILE* fp;
    if(!(fp=fopen(PATH_TO_FILTER, "rb"))) {
        perror("Error opening bloom filter file");
        return -1;
    }
    if(!(bloom=bloom_create(fp, BLOOMSIZE, NUMHASH, sax_hash, sdbm_hash))) {
        perror("Error creating bloom filter");
        return -1;
    }
    fcloze(fp);

    return 0;
}

void bloom_whitelist_destroy(const bloom_whitelist_t* bloom) {
    bloom_destroy(bloom_whitelist_t *bloom);
}

int bloom_whitelist_lookup(const bloom_whitelist_t* bloom,
                                                        const char* const domain) {
    int n;

    for (n=0; n<bloom->nfuncs; ++n) {
        if (!(GET_BIT(bloom->a, bloom->funcs[n](domain)%bloom->asize))) return -1;
    }
    /* TODO search for domains even if prepended by "mail.", "www.", "ns." etc. */
    /*
    p = strtok(domain, "www.");
    while (p != NULL) {
        for (n=0; n<bloom->nfuncs; ++n) {
            if (!(GET_BIT(bloom->a, bloom->funcs[n](p)%bloom->asize))) return 0;
        }
        p = strtok(NULL, "www.");
    }
    */

    /* Return 1 if found else return 0 */
    return 0;
}
