#!/system/bin/sh
set -e
for m in $(awk '$1=="/dev/block/sda3"{print $2}' /proc/mounts); do umount "$m" 2>/dev/null || true; done
rm -rf /data/local/tmp/sda3check
mkdir -p /data/local/tmp/sda3check
mount -o ro /dev/block/sda3 /data/local/tmp/sda3check
echo "== df =="
df -k /data/local/tmp/sda3check || true
echo "== files =="
ls -lni /data/local/tmp/sda3check
echo "== sizes =="
wc -c /data/local/tmp/sda3check/ramdisk /data/local/tmp/magisk_memu_patch/ramdisk.patched
umount /data/local/tmp/sda3check