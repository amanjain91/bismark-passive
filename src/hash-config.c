#include<stdlib.h>

#include"hash-config.h"

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
