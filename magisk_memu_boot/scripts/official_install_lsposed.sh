#!/system/bin/sh
set -e
rm -rf /data/adb/modules/zygisk_lsposed /data/adb/modules_update/zygisk_lsposed /data/adb/lspd
magisk --sqlite "INSERT OR REPLACE INTO settings (key,value) VALUES('zygisk',1);"
echo "== settings =="
magisk --sqlite "SELECT key,value FROM settings;"
echo "== install module =="
set +e
magisk --install-module /data/local/tmp/LSPosed-v1.9.2-7024-zygisk-release.zip
rc=$?
set -e
echo "install_exit=$rc"
echo "== module dirs =="
find /data/adb/modules /data/adb/modules_update -maxdepth 3 \( -type f -o -type d \) 2>/dev/null | sort | head -200 || true
exit $rc