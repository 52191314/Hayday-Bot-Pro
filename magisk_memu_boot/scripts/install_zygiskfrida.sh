#!/system/bin/sh
set -e
rm -rf /data/adb/modules/zygiskfrida /data/adb/modules_update/zygiskfrida /data/local/tmp/re.zyg.fri
magisk --sqlite "INSERT OR REPLACE INTO settings (key,value) VALUES('zygisk',1);"
echo "== install ZygiskFrida =="
magisk --install-module /data/local/tmp/ZygiskFrida-v1.9.0-release.zip
echo "== module dirs =="
find /data/adb/modules /data/adb/modules_update -maxdepth 3 \( -type f -o -type d \) 2>/dev/null | sort | head -160 || true
echo "== zygisk frida tmp =="
find /data/local/tmp/re.zyg.fri -maxdepth 2 \( -type f -o -type d \) 2>/dev/null | sort | head -80 || true