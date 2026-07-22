#!/system/bin/sh
set -e
echo "== magisk version =="
magisk -v
magisk -V
echo "== db schema =="
magisk --sqlite "SELECT name, sql FROM sqlite_master WHERE type='table';" || true
echo "== settings =="
magisk --sqlite "SELECT key, value FROM settings;" || true
echo "== path =="
magisk --path || true