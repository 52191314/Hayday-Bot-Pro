#!/bin/bash
# Fast system cert injection - works inside WSL native filesystem for speed
set -e

VMDK_WIN="D:\\Program Files\\Microvirt\\MEmu\\image\\96\\MEmu96-2026041700027FFF-disk1.vmdk"
VMDK_WSL="/mnt/d/Program Files/Microvirt/MEmu/image/96/MEmu96-2026041700027FFF-disk1.vmdk"
CERT_WSL="/mnt/d/HD/HaydayMod/tools/mitmproxy_ca.pem"
WORK="/tmp/memu_cert_inject"

mkdir -p "$WORK"

echo "=== Step 1: Copy VMDK to WSL native fs (fast I/O) ==="
cp "$VMDK_WSL" "$WORK/disk1.vmdk"
ls -lh "$WORK/disk1.vmdk"

echo ""
echo "=== Step 2: Convert VPC/VHD to raw ==="
qemu-img info "$WORK/disk1.vmdk" | head -5
qemu-img convert -f vpc -O raw "$WORK/disk1.vmdk" "$WORK/disk1.raw"
ls -lh "$WORK/disk1.raw"

echo ""
echo "=== Step 3: Check partitions ==="
fdisk -l "$WORK/disk1.raw" 2>/dev/null | grep -E "Device|disk1"

echo ""
echo "=== Step 4: Extract system partition (partition 6) ==="
dd if="$WORK/disk1.raw" of="$WORK/system.img" bs=512 skip=68532 count=5242880 status=progress

echo ""
echo "=== Step 5: Verify ext4 filesystem ==="
file "$WORK/system.img"
debugfs -R "ls etc/security/cacerts" "$WORK/system.img" 2>/dev/null | head -3

echo ""
echo "=== Step 6: Inject certificate ==="
debugfs -w -R "write $CERT_WSL etc/security/cacerts/c8750f0d.0" "$WORK/system.img" 2>&1
debugfs -w -R "set_inode_field etc/security/cacerts/c8750f0d.0 mode 0100644" "$WORK/system.img" 2>&1

echo ""
echo "=== Step 7: Verify injection ==="
debugfs -R "stat etc/security/cacerts/c8750f0d.0" "$WORK/system.img" 2>&1 | head -5

echo ""
echo "=== Step 8: Write partition back to raw image ==="
dd if="$WORK/system.img" of="$WORK/disk1.raw" bs=512 seek=68532 count=5242880 conv=notrunc status=progress

echo ""
echo "=== Step 9: Convert raw back to VPC/VHD ==="
qemu-img convert -f raw -O vpc "$WORK/disk1.raw" "$WORK/disk1_patched.vmdk"
ls -lh "$WORK/disk1_patched.vmdk"

echo ""
echo "=== Step 10: Copy patched VMDK back ==="
# Backup original
cp "$VMDK_WSL" "$VMDK_WSL.bak"
# Replace with patched
cp "$WORK/disk1_patched.vmdk" "$VMDK_WSL"
ls -lh "$VMDK_WSL"

echo ""
echo "=== CERT INJECTION COMPLETE ==="

# Cleanup large temp files
rm -f "$WORK/disk1.raw" "$WORK/system.img" "$WORK/disk1.vmdk"
echo "Temp files cleaned up"
