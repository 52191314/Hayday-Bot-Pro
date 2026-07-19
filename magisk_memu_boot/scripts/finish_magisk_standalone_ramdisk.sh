#!/system/bin/sh
set -e
cd /data/local/tmp/magisk_memu_patch
echo "== patched cpio test status =="
set +e
./magiskboot cpio ramdisk.patched.cpio test
test_status=$?
set -e
echo "patched_cpio_test_status=$test_status"
echo "== patched cpio key entries =="
./magiskboot cpio ramdisk.patched.cpio "ls" 2>&1 | grep -E '(^[-dl].*(init$|overlay|\.backup|magisk|stub|init-ld))|STATUS=' || true
rm -f ramdisk.patched
./magiskboot compress=gzip ramdisk.patched.cpio ramdisk.patched
echo "== output files =="
ls -l ramdisk.stock.gz ramdisk.cpio.stock ramdisk.patched.cpio ramdisk.patched
echo "== stock/patched sha1 =="
sha1sum ramdisk.stock.gz ramdisk.patched ramdisk.cpio.stock ramdisk.patched.cpio
echo "== patched gzip magic =="
od -An -tx1 -N16 ramdisk.patched