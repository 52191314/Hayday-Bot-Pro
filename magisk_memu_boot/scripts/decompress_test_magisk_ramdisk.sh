#!/system/bin/sh
set -e
cd /data/local/tmp/magisk_memu_patch
rm -f ramdisk.cpio ramdisk.patched.cpio ramdisk.patched
cp -f ramdisk ramdisk.stock.gz
./magiskboot decompress ramdisk ramdisk.cpio
echo "== raw cpio magic =="
od -An -tx1 -N16 ramdisk.cpio
echo "== cpio test raw =="
set +e
./magiskboot cpio ramdisk.cpio test
status=$?
set -e
echo "cpio_test_status=$status"
echo "== cpio listing raw =="
./magiskboot cpio ramdisk.cpio "ls" 2>&1 | head -100 || true
exit $status