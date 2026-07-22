#!/system/bin/sh
set -e
cd /data/local/tmp/magisk_memu_patch
echo "== payload compressed sizes =="
ls -l magisk.xz stub.xz init-ld.xz magisk stub.apk init-ld 2>/dev/null || true
rm -f ramdisk.patched.zopfli ramdisk.patched.xz ramdisk.patched.lzma
./magiskboot compress=zopfli ramdisk.patched.cpio ramdisk.patched.zopfli
./magiskboot compress=xz ramdisk.patched.cpio ramdisk.patched.xz
./magiskboot compress=lzma ramdisk.patched.cpio ramdisk.patched.lzma
echo "== stock/default/zopfli/xz/lzma sizes =="
ls -l ramdisk.stock.gz ramdisk.patched ramdisk.patched.zopfli ramdisk.patched.xz ramdisk.patched.lzma
echo "== magic =="
for f in ramdisk.patched.zopfli ramdisk.patched.xz ramdisk.patched.lzma; do echo $f; od -An -tx1 -N8 $f; done