#include<stdio.h>
#include<string.h>
#include<time.h>

#include"bloom-whitelist.h"

int main(int argc, char *argv[])
{
    bloom_whitelist_t* bloom;
    char* p;
    char line[1024];

    if(bloom_whitelist_init(bloom)) {
        fprintf(stderr, "Bloom filter not created\n");
        return -1;
    }

    printf("Enter domains to check.\n");

    /* check block - enter name to check */
    while(fgets(line, 1024, stdin)) {
        if((p=strchr(line, '\r'))) *p='\0';
        if((p=strchr(line, '\n'))) *p='\0';

        p=strtok(line, " \t\r\n");
        while(p) {
            if(!bloom_whitelist_lookup(bloom, p)) {
                printf("No match for word \"%s\"\n", p);
            }
            p=strtok(NULL, " \t\r\n");
        }
    }

    bloom_whitelist_destroy(bloom);
    return EXIT_SUCCESS;
}
