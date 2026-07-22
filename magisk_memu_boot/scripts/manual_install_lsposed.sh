#!/system/bin/sh
set -e
magisk --sqlite "INSERT OR REPLACE INTO settings (key,value) VALUES('zygisk',1);"
rm -rf /data/adb/modules/zygisk_lsposed
mkdir -p /data/adb/modules
cp -af /data/local/tmp/zygisk_lsposed_manual /data/adb/modules/zygisk_lsposed
chown -R 0:0 /data/adb/modules/zygisk_lsposed
find /data/adb/modules/zygisk_lsposed -type d -exec chmod 0755 {} \;
find /data/adb/modules/zygisk_lsposed -type f -exec chmod 0644 {} \;
chmod 0755 /data/adb/modules/zygisk_lsposed/post-fs-data.sh /data/adb/modules/zygisk_lsposed/service.sh /data/adb/modules/zygisk_lsposed/uninstall.sh
chmod 0744 /data/adb/modules/zygisk_lsposed/daemon
chmod 0755 /data/adb/modules/zygisk_lsposed/zygisk/x86.so /data/adb/modules/zygisk_lsposed/zygisk/x86_64.so
chmod 0644 /data/adb/modules/zygisk_lsposed/module.prop /data/adb/modules/zygisk_lsposed/sepolicy.rule /data/adb/modules/zygisk_lsposed/system.prop
echo "== settings =="
magisk --sqlite "SELECT key,value FROM settings;"
echo "== module files =="
find /data/adb/modules/zygisk_lsposed -maxdepth 3 -type f -o -type d | sort