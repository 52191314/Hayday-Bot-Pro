#!/system/bin/sh
set -e
cd /data/local/tmp/magisk_memu_patch
rm -f ramdisk.cpio ramdisk.cpio.stock ramdisk.patched.cpio ramdisk.patched magisk.xz stub.xz init-ld.xz config
cp -f ramdisk ramdisk.stock.gz
./magiskboot decompress ramdisk.stock.gz ramdisk.cpio.stock
cp -f ramdisk.cpio.stock ramdisk.patched.cpio
./magiskboot compress=xz magisk magisk.xz
./magiskboot compress=xz stub.apk stub.xz
./magiskboot compress=xz init-ld init-ld.xz
SHA1=$(./magiskboot sha1 ramdisk.stock.gz 2>/dev/null || sha1sum ramdisk.stock.gz | awk '{print $1}')
{
  echo KEEPVERITY=false
  echo KEEPFORCEENCRYPT=false
  echo RECOVERYMODE=false
  echo VENDORBOOT=false
  echo SHA1=$SHA1
} > config
echo "== config =="
cat config
echo "== patch cpio =="
./magiskboot cpio ramdisk.patched.cpio \
  "add 0750 init magiskinit" \
  "mkdir 0750 overlay.d" \
  "mkdir 0750 overlay.d/sbin" \
  "add 0644 overlay.d/sbin/magisk.xz magisk.xz" \
  "add 0644 overlay.d/sbin/stub.xz stub.xz" \
  "add 0644 overlay.d/sbin/init-ld.xz init-ld.xz" \
  "patch" \
  "backup ramdisk.cpio.stock" \
  "mkdir 000 .backup" \
  "add 000 .backup/.magisk config"
echo "== patched cpio test =="
./magiskboot cpio ramdisk.patched.cpio test
./magiskboot cpio ramdisk.patched.cpio "ls" 2>&1 | grep -E '(^[-dl].*(init$|overlay|\.backup|magisk|stub|init-ld))|STATUS=' || true
./magiskboot compress=gzip ramdisk.patched.cpio ramdisk.patched
echo "== output files =="
ls -l ramdisk.stock.gz ramdisk.cpio.stock ramdisk.patched.cpio ramdisk.patched
echo "== patched gzip magic =="
od -An -tx1 -N16 ramdisk.patched