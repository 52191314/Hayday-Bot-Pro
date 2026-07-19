#!/system/bin/sh
set -e
cd /data/local/tmp/magisk_memu_patch
rm -f ramdisk.full_nocharger.cpio ramdisk.full_nocharger.gz
cp -f ramdisk.patched.cpio ramdisk.full_nocharger.cpio
./magiskboot cpio ramdisk.full_nocharger.cpio "rm sbin/charger"
set +e
./magiskboot cpio ramdisk.full_nocharger.cpio test
test_status=$?
set -e
echo "full_nocharger_cpio_test_status=$test_status"
./magiskboot cpio ramdisk.full_nocharger.cpio "ls" 2>&1 | grep -E 'sbin/charger|\.backup|overlay.d|init$' || true
./magiskboot compress=gzip ramdisk.full_nocharger.cpio ramdisk.full_nocharger.gz
echo "== sizes =="
ls -l ramdisk.stock.gz ramdisk.patched ramdisk.full_nocharger.cpio ramdisk.full_nocharger.gz
echo "== sha1 =="
sha1sum ramdisk.full_nocharger.gz
od -An -tx1 -N16 ramdisk.full_nocharger.gz