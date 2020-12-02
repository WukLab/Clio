#
# This simple script will setup the hugepage.
# You should adjust the following numbers according to your system setting.
#

NR_2MB_PAGES=1024
NR_1GB_PAGES=8

# 2MB page
echo $NR_2MB_PAGES > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

# 1GB page
echo $NR_1GB_PAGES > /sys/devices/system/node/node0/hugepages/hugepages-1048576kB/nr_hugepages
