#!/system/bin/sh
set -e
PATCH=/data/local/tmp/magisk_memu_patch/ramdisk.nobackup.gz
STOCK=/data/local/tmp/magisk_memu_patch/ramdisk.stock.gz
MP=/data/local/tmp/sda3rw
[ -s "$PATCH" ]
[ -s "$STOCK" ]
for m in $(awk '$1=="/dev/block/sda3"{print $2}' /proc/mounts); do umount "$m" 2>/dev/null || true; done
rm -rf "$MP"
mkdir -p "$MP"
mount -o rw /dev/block/sda3 "$MP"
echo "== before =="
df -k "$MP" || true
ls -lni "$MP"
echo "stock_on_partition=$(sha1sum "$MP/ramdisk" | awk '{print $1}')"
echo "patch_to_write=$(sha1sum "$PATCH" | awk '{print $1}')"
if ! cp -f "$PATCH" "$MP/ramdisk"; then
  echo "copy failed; restoring stock"
  cp -f "$STOCK" "$MP/ramdisk" || true
  sync
  umount "$MP" || true
  exit 1
fi
chown 1000:1000 "$MP/ramdisk"
chmod 0644 "$MP/ramdisk"
sync
echo "== after =="
df -k "$MP" || true
ls -lni "$MP"
echo "new_on_partition=$(sha1sum "$MP/ramdisk" | awk '{print $1}')"
umount "$MP"