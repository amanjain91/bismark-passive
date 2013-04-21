#!/bin/ash
curl -s -f -z filter.bin -O http://sites.noise.gatech.edu/~sarthak/files/bloomfilter/filter.bin
/etc/init.d/bismark-passive restart

#chrontab -e
#0 0 * * * /tmp/bloom_download.sh
