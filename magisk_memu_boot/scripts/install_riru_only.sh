#!/system/bin/sh
set -e
rm -rf /data/adb/modules/riru-core /data/adb/modules_update/riru-core /data/adb/riru
mkdir -p /data/adb/modules /data/adb/modules_update
magisk --sqlite "INSERT OR REPLACE INTO settings (key,value) VALUES('zygisk',0);"
echo "== settings before install =="
magisk --sqlite "SELECT key,value FROM settings;"
echo "== install Riru =="
magisk --install-module /data/local/tmp/riru-v26.1.7.r530.ab3086ec9f-release.zip
echo "== module dirs =="
find /data/adb/modules /data/adb/modules_update -maxdepth 4 \( -type f -o -type d \) 2>/dev/null | sort | head -200 || true