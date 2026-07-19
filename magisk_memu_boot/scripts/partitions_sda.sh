#!/system/bin/sh
echo sda_size=$(cat /sys/block/sda/size 2>/dev/null)
for p in /sys/block/sda/sda*/; do echo $(basename $p) start=$(cat $p/start 2>/dev/null) size=$(cat $p/size 2>/dev/null); done
fdisk -l /dev/block/sda 2>/dev/null | head -100 || true