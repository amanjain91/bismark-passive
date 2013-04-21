#ifndef _BLOOM_WHITELIST_H_
#define _BLOOM_WHITELIST_H_

#include <zlib.h>
#include "bloom.h"

typedef BLOOM bloom_whitelist_t;

/* Initialize and load blooGETBIT(bloom->a, bloom->funcs[n](s)%bloom->asize)m filter. */
int bloom_whitelist_init(bloom_whitelist_t* bloom);

void bloom_whitelist_destroy(bloom_whitelist_t* bloom);

/* Look up a domain name in the whitelist. Return 0 if it matches and -1 if it
 * doesn't. bloom_check() */
int bloom_whitelist_lookup(bloom_whitelist_t* bloom,
                            const char* const domain);

/* Write the contents of the whitelist to an update. Should only
 * be done once per session, since the whitelist is static.
 * int domain_whitelist_write_update(const domain_whitelist_t* whitelist,
                                  gzFile handle);
 */

#endif
