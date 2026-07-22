#!/system/bin/sh
set -e
cd /data/local/tmp/magisk_memu_patch
echo "== staged files =="
ls -l
echo "== ramdisk magic =="
od -An -tx1 -N16 ramdisk
echo "== cpio test =="
set +e
./magiskboot cpio ramdisk test
status=$?
set -e
echo "cpio_test_status=$status"
echo "== cpio listing =="
./magiskboot cpio ramdisk "ls" 2>&1 | head -80 || true
exit $status