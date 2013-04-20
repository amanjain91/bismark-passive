#include<stdlib.h>

/* 312500 bytes = 305.2 KB*/
static const int BLOOMSIZE = 2500000;
static const int NUMHASH = 2;
static const char* PATH_TO_FILTER = "/tmp/filter.bin";

unsigned int sax_hash(const char *);
unsigned int sdbm_hash(const char *);
