#!/system/bin/sh
set -e
for m in $(awk '$1=="/dev/block/sda3"{print $2}' /proc/mounts); do umount "$m" 2>/dev/null || true; done
dd if=/dev/block/sda3 of=/data/local/tmp/sda3_first4096.bin bs=4096 count=1
dd if=/data/local/tmp/sda3_first4096.bin of=/dev/block/sda3 bs=4096 count=1 seek=0
sync
echo raw_write_status=$?