#!/system/bin/sh
echo "== boot =="
getprop sys.boot_completed
uname -a
echo "== magisk =="
magisk -v
magisk --sqlite "SELECT key,value FROM settings;"
echo "== modules =="
find /data/adb/modules -maxdepth 3 -type f -o -type d | sort
echo "== lspd dir =="
find /data/adb/lspd -maxdepth 3 -type f -o -type d 2>/dev/null | sort | head -120 || true
echo "== ps =="
ps -A | grep -iE 'magisk|lspd|lsposed|zygote' || true
echo "== props =="
getprop | grep -iE 'zygisk|lsposed|lspd|magisk' || true
echo "== logcat tail =="
logcat -d -t 600 | grep -iE 'zygisk|lsposed|lspd|magisk' | tail -160 || true