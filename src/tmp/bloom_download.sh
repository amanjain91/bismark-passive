#!/bin/ash
curl -s -f -z filter.bin -O http://sites.noise.gatech.edu/~sarthak/files/bloomfilter/filter.bin

#chrontab -e
#0 0 * * * /tmp/bloom_download.sh
