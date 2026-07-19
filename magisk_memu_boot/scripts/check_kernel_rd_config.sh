#!/system/bin/sh
echo "== /proc/config.gz =="
ls -l /proc/config.gz 2>/dev/null || true
if [ -r /proc/config.gz ]; then
  zcat /proc/config.gz 2>/dev/null | grep -E 'CONFIG_RD_|CONFIG_INITRAMFS_COMPRESSION|DECOMPRESS' | head -120 || true
fi
echo "== kernel release =="
uname -a