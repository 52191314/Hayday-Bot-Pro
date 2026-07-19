#!/system/bin/sh
set -e
echo "== enable zygisk setting =="
magisk --sqlite "INSERT OR REPLACE INTO settings (key,value) VALUES('zygisk',1);"
magisk --sqlite "SELECT key,value FROM settings;"
echo "== install LSPosed =="
magisk --install-module /data/local/tmp/LSPosed-v1.9.2-7024-zygisk-release.zip
echo "== modules dir =="
find /data/adb/modules -maxdepth 2 -type f -o -type d | sort | head -120