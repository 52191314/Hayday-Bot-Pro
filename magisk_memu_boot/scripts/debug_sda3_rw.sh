#!/system/bin/sh
set -x
for m in $(awk '$1=="/dev/block/sda3"{print $2}' /proc/mounts); do umount "$m" 2>/dev/null || true; done
rm -rf /data/local/tmp/sda3rw
mkdir -p /data/local/tmp/sda3rw
blockdev --getro /dev/block/sda3 2>/dev/null || true
mount -t ext4 -o rw /dev/block/sda3 /data/local/tmp/sda3rw 2>&1 || true
grep sda3 /proc/mounts || true
mount | grep sda3 || true
touch /data/local/tmp/sda3rw/.rw_test 2>&1 || true
ls -la /data/local/tmp/sda3rw | head || true
rm -f /data/local/tmp/sda3rw/.rw_test 2>/dev/null || true
umount /data/local/tmp/sda3rw 2>/dev/null || true