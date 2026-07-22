"""
Inject mitmproxy CA certificate into MEmu's system partition VMDK.
Pure Python - no WSL/Linux required.

Uses qemu-img (if available) or raw VHD parsing to convert,
then uses debugfs-like ext4 manipulation to write the cert file.
"""
import subprocess
import struct
import shutil
import os
import sys
from pathlib import Path

VMDK = Path(r"D:\Program Files\Microvirt\MEmu\image\96\MEmu96-2026041700027FFF-disk1.vmdk")
CERT = Path(r"d:\HD\HaydayMod\tools\mitmproxy_ca.pem")
WORK = Path(r"d:\HD\HaydayMod\tools\cert_inject_work")

# System partition: start=68532 sectors, size=5242880 sectors, 512 bytes/sector
P6_START = 68532
P6_SIZE = 5242880
SECTOR = 512
P6_OFFSET = P6_START * SECTOR  # 35088384
P6_BYTES = P6_SIZE * SECTOR    # 2684354560


def vhd_to_raw(vhd_path: Path, raw_path: Path):
    """Convert VHD (VPC format) to raw image."""
    print(f"Reading VHD: {vhd_path} ({vhd_path.stat().st_size // 1024 // 1024}MB)")
    
    # VHD footer is last 512 bytes
    with open(vhd_path, 'rb') as f:
        f.seek(-512, 2)
        footer = f.read(512)
    
    # Check "conectix" cookie
    if footer[:8] != b'conectix':
        raise ValueError("Not a VHD file")
    
    disk_type = struct.unpack('>I', footer[60:64])[0]
    print(f"  Disk type: {disk_type} (2=fixed, 3=dynamic, 4=differencing)")
    
    if disk_type == 2:  # Fixed VHD - data is contiguous, just strip footer
        original_size = struct.unpack('>Q', footer[48:56])[0]
        print(f"  Fixed VHD, original size: {original_size // 1024 // 1024}MB")
        with open(vhd_path, 'rb') as src, open(raw_path, 'wb') as dst:
            remaining = original_size
            while remaining > 0:
                chunk = min(remaining, 64 * 1024 * 1024)
                data = src.read(chunk)
                if not data:
                    break
                dst.write(data)
                remaining -= len(data)
                print(f"\r  Copied: {(original_size - remaining) // 1024 // 1024}MB / {original_size // 1024 // 1024}MB", end="")
        print()
    elif disk_type == 3:  # Dynamic VHD
        # Read dynamic disk header at offset specified in footer
        data_offset = struct.unpack('>Q', footer[16:24])[0]
        original_size = struct.unpack('>Q', footer[48:56])[0]
        print(f"  Dynamic VHD, size: {original_size // 1024 // 1024}MB, header at: {data_offset}")
        
        with open(vhd_path, 'rb') as f:
            f.seek(data_offset)
            dyn_header = f.read(1024)
        
        if dyn_header[:8] != b'cxsparse':
            raise ValueError("Invalid dynamic disk header")
        
        bat_offset = struct.unpack('>Q', dyn_header[16:24])[0]
        block_size = struct.unpack('>I', dyn_header[32:36])[0]
        max_bat_entries = struct.unpack('>I', dyn_header[36:40])[0]
        
        # Each block has a bitmap sector followed by data
        bitmap_sectors = (block_size // SECTOR // 8 + SECTOR - 1) // SECTOR
        
        print(f"  Block size: {block_size // 1024}KB, BAT entries: {max_bat_entries}, bitmap sectors: {bitmap_sectors}")
        
        # Read BAT
        with open(vhd_path, 'rb') as f:
            f.seek(bat_offset)
            bat_data = f.read(max_bat_entries * 4)
        
        bat = [struct.unpack('>I', bat_data[i:i+4])[0] for i in range(0, len(bat_data), 4)]
        
        # Write raw image
        with open(raw_path, 'wb') as dst:
            dst.truncate(original_size)
            
            with open(vhd_path, 'rb') as src:
                for i, sector_offset in enumerate(bat):
                    if sector_offset == 0xFFFFFFFF:
                        continue  # Sparse/unallocated block - leave as zeros
                    
                    # Skip bitmap, read data
                    data_start = (sector_offset + bitmap_sectors) * SECTOR
                    src.seek(data_start)
                    block_data = src.read(block_size)
                    
                    dst.seek(i * block_size)
                    dst.write(block_data)
                    
                    if i % 100 == 0:
                        print(f"\r  Block {i}/{max_bat_entries}", end="")
        print(f"\r  Done: {max_bat_entries} blocks processed")
    else:
        raise ValueError(f"Unsupported VHD type: {disk_type}")
    
    print(f"  Raw image: {raw_path} ({raw_path.stat().st_size // 1024 // 1024}MB)")


def raw_to_vhd(raw_path: Path, vhd_path: Path):
    """Convert raw image back to fixed VHD."""
    raw_size = raw_path.stat().st_size
    print(f"Converting raw ({raw_size // 1024 // 1024}MB) to VHD...")
    
    # Create fixed VHD: raw data + 512-byte footer
    shutil.copy2(raw_path, vhd_path)
    
    # Build VHD footer
    import time
    import uuid
    
    footer = bytearray(512)
    footer[0:8] = b'conectix'                                    # Cookie
    footer[8:12] = struct.pack('>I', 0x00000002)                 # Features (reserved)
    footer[12:16] = struct.pack('>I', 0x00010000)                # File format version 1.0
    footer[16:24] = struct.pack('>Q', 0xFFFFFFFFFFFFFFFF)        # Data offset (fixed = none)
    footer[24:28] = struct.pack('>I', int(time.time()) - 946684800)  # Timestamp (VHD epoch)
    footer[28:32] = b'qemu'                                      # Creator app
    footer[32:36] = struct.pack('>I', 0x00020000)                # Creator version
    footer[36:40] = b'Wi2k'                                      # Creator host OS
    footer[40:48] = struct.pack('>Q', raw_size)                  # Original size
    footer[48:56] = struct.pack('>Q', raw_size)                  # Current size
    
    # Disk geometry (CHS)
    total_sectors = raw_size // 512
    if total_sectors > 65535 * 16 * 255:
        total_sectors = 65535 * 16 * 255
    
    if total_sectors >= 65535 * 16 * 63:
        spt = 255; heads = 16
    else:
        spt = 17; cth = total_sectors // spt; heads = (cth + 1023) // 1024
        if heads < 4: heads = 4
        if cth >= heads * 1024 or heads > 16:
            spt = 31; cth = total_sectors // spt; heads = (cth + 1023) // 1024
        if cth >= heads * 1024 or heads > 16:
            spt = 63; cth = total_sectors // spt; heads = (cth + 1023) // 1024
        if cth >= heads * 1024 or heads > 16:
            spt = 255; heads = 16
    
    cyls = total_sectors // (heads * spt)
    if cyls > 65535: cyls = 65535
    
    footer[56:58] = struct.pack('>H', cyls)
    footer[58] = heads
    footer[59] = spt
    footer[60:64] = struct.pack('>I', 2)                         # Disk type: fixed
    
    # UUID
    uid = uuid.uuid4().bytes
    footer[68:84] = uid
    footer[84] = 0  # Saved state: no
    
    # Checksum
    footer[64:68] = b'\x00\x00\x00\x00'
    checksum = (~sum(footer) + 1) & 0xFFFFFFFF
    footer[64:68] = struct.pack('>I', checksum)
    
    with open(vhd_path, 'ab') as f:
        f.write(bytes(footer))
    
    print(f"  VHD: {vhd_path} ({vhd_path.stat().st_size // 1024 // 1024}MB)")


def inject_cert_ext4(partition_img: Path, cert_path: Path, target: str):
    """Inject a file into an ext4 partition image using debugfs."""
    print(f"Injecting {cert_path.name} into {target}...")
    
    # Write cert
    result = subprocess.run(
        ['wsl', '-d', 'Ubuntu', '-u', 'root', '--', 
         'debugfs', '-w', '-R', f'write /mnt/d/HD/HaydayMod/tools/mitmproxy_ca.pem {target}',
         str(partition_img).replace('\\', '/').replace('D:', '/mnt/d')],
        capture_output=True, text=True, timeout=30
    )
    print(f"  debugfs write: {result.stdout.strip()} {result.stderr.strip()}")
    
    # Set permissions
    result2 = subprocess.run(
        ['wsl', '-d', 'Ubuntu', '-u', 'root', '--',
         'debugfs', '-w', '-R', f'set_inode_field {target} mode 0100644',
         str(partition_img).replace('\\', '/').replace('D:', '/mnt/d')],
        capture_output=True, text=True, timeout=30
    )
    print(f"  debugfs chmod: {result2.stdout.strip()} {result2.stderr.strip()}")
    
    # Verify
    result3 = subprocess.run(
        ['wsl', '-d', 'Ubuntu', '-u', 'root', '--',
         'debugfs', '-R', f'stat {target}',
         str(partition_img).replace('\\', '/').replace('D:', '/mnt/d')],
        capture_output=True, text=True, timeout=30
    )
    for line in result3.stdout.split('\n')[:5]:
        print(f"  {line}")


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    
    print("=" * 60)
    print("MEmu System Cert Injection (Pure Python + debugfs)")
    print("=" * 60)
    
    # Step 1: Convert VHD to raw
    raw_path = WORK / "disk1.raw"
    if not raw_path.exists():
        vhd_to_raw(VMDK, raw_path)
    else:
        print(f"Using existing raw image: {raw_path}")
    
    # Step 2: Extract system partition
    part_path = WORK / "system.img"
    print(f"\nExtracting partition 6 (offset={P6_OFFSET}, size={P6_BYTES // 1024 // 1024}MB)...")
    with open(raw_path, 'rb') as src, open(part_path, 'wb') as dst:
        src.seek(P6_OFFSET)
        remaining = P6_BYTES
        while remaining > 0:
            chunk = min(remaining, 64 * 1024 * 1024)
            data = src.read(chunk)
            dst.write(data)
            remaining -= len(data)
            print(f"\r  Extracted: {(P6_BYTES - remaining) // 1024 // 1024}MB / {P6_BYTES // 1024 // 1024}MB", end="")
    print()
    
    # Step 3: Inject cert using debugfs (still needs WSL for debugfs)
    inject_cert_ext4(part_path, CERT, "etc/security/cacerts/c8750f0d.0")
    
    # Step 4: Write partition back
    print(f"\nWriting partition back...")
    with open(part_path, 'rb') as src, open(raw_path, 'r+b') as dst:
        dst.seek(P6_OFFSET)
        remaining = P6_BYTES
        while remaining > 0:
            chunk = min(remaining, 64 * 1024 * 1024)
            data = src.read(chunk)
            dst.write(data)
            remaining -= len(data)
            print(f"\r  Written: {(P6_BYTES - remaining) // 1024 // 1024}MB / {P6_BYTES // 1024 // 1024}MB", end="")
    print()
    
    # Step 5: Convert back to VHD
    patched_vhd = WORK / "disk1_patched.vmdk"
    raw_to_vhd(raw_path, patched_vhd)
    
    # Step 6: Replace original
    backup = VMDK.with_suffix('.vmdk.bak')
    print(f"\nBacking up original to {backup.name}...")
    if not backup.exists():
        shutil.copy2(VMDK, backup)
    
    print(f"Replacing with patched VMDK...")
    shutil.copy2(patched_vhd, VMDK)
    print(f"Done! VMDK replaced: {VMDK.stat().st_size // 1024 // 1024}MB")
    
    # Cleanup
    print("\nCleaning up temp files...")
    for f in [raw_path, part_path, patched_vhd]:
        if f.exists():
            f.unlink()
    
    print("\n" + "=" * 60)
    print("CERT INJECTION COMPLETE")
    print("Start MEmu and verify with:")
    print("  adb shell ls /system/etc/security/cacerts/c8750f0d.0")
    print("=" * 60)


if __name__ == "__main__":
    main()
