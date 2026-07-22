#!/system/bin/sh
set -e
cd /data/local/tmp/magisk_memu_patch
rm -f ramdisk.nobackup.cpio ramdisk.nobackup.gz
cp -f ramdisk.cpio.stock ramdisk.nobackup.cpio
./magiskboot cpio ramdisk.nobackup.cpio \
  "add 0750 init magiskinit" \
  "mkdir 0750 overlay.d" \
  "mkdir 0750 overlay.d/sbin" \
  "add 0644 overlay.d/sbin/magisk.xz magisk.xz" \
  "add 0644 overlay.d/sbin/stub.xz stub.xz" \
  "add 0644 overlay.d/sbin/init-ld.xz init-ld.xz" \
  "patch" \
  "mkdir 000 .backup" \
  "add 000 .backup/.magisk config"
set +e
./magiskboot cpio ramdisk.nobackup.cpio test
test_status=$?
set -e
echo "nobackup_cpio_test_status=$test_status"
./magiskboot compress=gzip ramdisk.nobackup.cpio ramdisk.nobackup.gz
echo "== sizes =="
ls -l ramdisk.stock.gz ramdisk.nobackup.cpio ramdisk.nobackup.gz
echo "== sha1 =="
sha1sum ramdisk.stock.gz ramdisk.nobackup.gz
echo "== magic =="
od -An -tx1 -N16 ramdisk.nobackup.gz