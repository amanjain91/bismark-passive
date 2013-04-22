#include<stdio.h>
#include<string.h>
#include<time.h>

#include"bloom-whitelist.h"

static bloom_whitelist_t bloom_wl;

static int initialize_bloom_whitelist() {
    /* if (DEBUG_BLOOM) printf("initialize_bloom_whitelist() : &bloom_wl = %d\n", &bloom_wl); */
    bloom_whitelist_init(&bloom_wl);

    if (bloom_whitelist_load(&bloom_wl) < 0) {
        fprintf(stderr, "Error loading bloom filter.\n");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char* p;
    char line[1024];

    if(initialize_bloom_whitelist()) {
        fprintf(stderr,"Bloom filter not created\n");
        return -1;
    }

    /* if (DEBUG_BLOOM) printf("main(): &bloom_wl = %d, nfunc = %d, asize = %d\n", &bloom_wl, (&bloom_wl)->nfuncs,(& bloom_wl)->asize); */

    printf("Enter domains to check.\n");

    /* check block - enter name to check */
    while(fgets(line, 1024, stdin)) {
        if((p=strchr(line, '\r'))) *p='\0';
        if((p=strchr(line, '\n'))) *p='\0';

        if(!bloom_whitelist_lookup(&bloom_wl, line)) printf("Found word \"%s\"\n", line);
        else printf("Not Found  \"%s\"\n", line);

        /*
        p=strtok(line, " \t\r\n");
        while(p) {
            if(!bloom_whitelist_lookup(&bloom_wl, p)) {
                printf("No match for word \"%s\"\n", p);
            }
            p=strtok(NULL, " \t\r\n");
        }
        */
    }

    bloom_whitelist_destroy(&bloom_wl);
    return 0;
}
