"""
Inject mitmproxy's CA cert into MEmu's Android system cert store without WSL.

The MEmu disk has a .vmdk extension, but it is a dynamic VHD/VPC image. This
script patches the ext4 system partition in-place through the dynamic VHD block
map. It is intentionally narrow: it only adds /etc/security/cacerts/c8750f0d.0.
"""

from __future__ import annotations

import argparse
import math
import shutil
import struct
import time
from pathlib import Path


SECTOR = 512
PARTITION_START_SECTOR = 68532
PARTITION_OFFSET = PARTITION_START_SECTOR * SECTOR
TARGET_PATH = "/etc/security/cacerts/c8750f0d.0"
TARGET_NAME = "c8750f0d.0"

EXT4_SUPER_MAGIC = 0xEF53
EXT4_EXTENTS_FL = 0x00080000
EXT4_INDEX_FL = 0x00001000
EXT4_NOCOMPR_FL = 0x00000080
EXTENT_HEADER_MAGIC = 0xF30A


def le16(buf: bytes | bytearray, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def le32(buf: bytes | bytearray, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def put_le16(buf: bytearray, off: int, val: int) -> None:
    struct.pack_into("<H", buf, off, val)


def put_le32(buf: bytearray, off: int, val: int) -> None:
    struct.pack_into("<I", buf, off, val)


def min_dir_rec_len(name_len: int) -> int:
    return ((8 + name_len + 3) // 4) * 4


class DynamicVhd:
    def __init__(self, image: Path, writable: bool = False) -> None:
        self.image = image
        self.writable = writable
        self.f = image.open("r+b" if writable else "rb")
        self.f.seek(-SECTOR, 2)
        self.footer_offset = self.f.tell()
        self.footer = self.f.read(SECTOR)
        if self.footer[:8] != b"conectix":
            raise RuntimeError(f"{image} is not a VHD/VPC image")

        disk_type = struct.unpack_from(">I", self.footer, 60)[0]
        if disk_type != 3:
            raise RuntimeError(f"expected dynamic VHD disk type 3, got {disk_type}")

        data_offset = struct.unpack_from(">Q", self.footer, 16)[0]
        self.f.seek(data_offset)
        header = self.f.read(1024)
        if header[:8] != b"cxsparse":
            raise RuntimeError("invalid dynamic VHD header")

        self.table_offset = struct.unpack_from(">Q", header, 16)[0]
        self.max_entries = struct.unpack_from(">I", header, 28)[0]
        self.block_size = struct.unpack_from(">I", header, 32)[0]
        self.bitmap_sectors = math.ceil((self.block_size // SECTOR) / 8 / SECTOR)
        self.f.seek(self.table_offset)
        table = self.f.read(self.max_entries * 4)
        self.bat = [
            struct.unpack_from(">I", table, i)[0]
            for i in range(0, len(table), 4)
        ]

    def close(self) -> None:
        self.f.close()

    def is_allocated_range(self, raw_off: int, length: int) -> bool:
        while length:
            block_index = raw_off // self.block_size
            block_off = raw_off % self.block_size
            take = min(length, self.block_size - block_off)
            if self.bat[block_index] == 0xFFFFFFFF:
                return False
            raw_off += take
            length -= take
        return True

    def read(self, raw_off: int, length: int) -> bytes:
        out = bytearray()
        while length:
            block_index = raw_off // self.block_size
            block_off = raw_off % self.block_size
            take = min(length, self.block_size - block_off)
            bat_entry = self.bat[block_index]
            if bat_entry == 0xFFFFFFFF:
                out.extend(b"\0" * take)
            else:
                data_off = (bat_entry + self.bitmap_sectors) * SECTOR + block_off
                self.f.seek(data_off)
                out.extend(self.f.read(take))
            raw_off += take
            length -= take
        return bytes(out)

    def write(self, raw_off: int, data: bytes) -> None:
        if not self.writable:
            raise RuntimeError("VHD was opened read-only")

        pos = 0
        length = len(data)
        while length:
            block_index = raw_off // self.block_size
            block_off = raw_off % self.block_size
            take = min(length, self.block_size - block_off)
            bat_entry = self.bat[block_index]
            if bat_entry == 0xFFFFFFFF:
                raise RuntimeError(
                    "write would require allocating a new dynamic VHD block; "
                    "choose an already allocated ext4 block instead"
                )

            block_base = bat_entry * SECTOR
            data_off = block_base + self.bitmap_sectors * SECTOR + block_off
            self.f.seek(data_off)
            self.f.write(data[pos : pos + take])
            self._mark_bitmap(block_base, block_off, take)

            raw_off += take
            pos += take
            length -= take

    def _mark_bitmap(self, block_base: int, block_off: int, length: int) -> None:
        start_sector = block_off // SECTOR
        end_sector = (block_off + length + SECTOR - 1) // SECTOR
        bitmap = bytearray(self.read_vhd(block_base, self.bitmap_sectors * SECTOR))
        for sector in range(start_sector, end_sector):
            byte_index = sector // 8
            bit = 7 - (sector % 8)
            bitmap[byte_index] |= 1 << bit
        self.write_vhd(block_base, bitmap)

    def read_vhd(self, off: int, length: int) -> bytes:
        self.f.seek(off)
        return self.f.read(length)

    def write_vhd(self, off: int, data: bytes) -> None:
        self.f.seek(off)
        self.f.write(data)


class Ext4Image:
    def __init__(self, disk: DynamicVhd) -> None:
        self.disk = disk
        self.base = PARTITION_OFFSET
        self.super_off = self.base + 1024
        self.super = bytearray(self.disk.read(self.super_off, 1024))
        if le16(self.super, 56) != EXT4_SUPER_MAGIC:
            raise RuntimeError("partition 6 does not look like ext4")

        self.block_size = 1024 << le32(self.super, 24)
        self.blocks_count = le32(self.super, 4)
        self.free_blocks = le32(self.super, 12)
        self.free_inodes = le32(self.super, 16)
        self.blocks_per_group = le32(self.super, 32)
        self.inodes_per_group = le32(self.super, 40)
        self.inode_size = le16(self.super, 88)
        self.groups = math.ceil(self.blocks_count / self.blocks_per_group)
        self.gdt_off = self.base + self.block_size
        self.gdt = bytearray(self.disk.read(self.gdt_off, self.groups * 32))

    def group_desc(self, group: int) -> dict[str, int]:
        off = group * 32
        d = self.gdt[off : off + 32]
        return {
            "block_bitmap": le32(d, 0),
            "inode_bitmap": le32(d, 4),
            "inode_table": le32(d, 8),
            "free_blocks": le16(d, 12),
            "free_inodes": le16(d, 14),
            "used_dirs": le16(d, 16),
        }

    def put_group_count(self, group: int, free_blocks: int | None = None, free_inodes: int | None = None) -> None:
        off = group * 32
        if free_blocks is not None:
            put_le16(self.gdt, off + 12, free_blocks)
        if free_inodes is not None:
            put_le16(self.gdt, off + 14, free_inodes)

    def block_off(self, block: int) -> int:
        return self.base + block * self.block_size

    def read_block(self, block: int) -> bytes:
        return self.disk.read(self.block_off(block), self.block_size)

    def write_block(self, block: int, data: bytes) -> None:
        if len(data) != self.block_size:
            raise ValueError("write_block requires one full filesystem block")
        self.disk.write(self.block_off(block), data)

    def inode_off(self, inode_no: int) -> int:
        group = (inode_no - 1) // self.inodes_per_group
        index = (inode_no - 1) % self.inodes_per_group
        table = self.group_desc(group)["inode_table"]
        return self.block_off(table) + index * self.inode_size

    def read_inode(self, inode_no: int) -> bytearray:
        return bytearray(self.disk.read(self.inode_off(inode_no), self.inode_size))

    def write_inode(self, inode_no: int, inode: bytes) -> None:
        if len(inode) != self.inode_size:
            raise ValueError("inode has wrong size")
        self.disk.write(self.inode_off(inode_no), inode)

    def extents(self, inode: bytes | bytearray) -> list[tuple[int, int, int]]:
        data = inode[40:100]
        magic = le16(data, 0)
        entries = le16(data, 2)
        depth = le16(data, 6)
        if magic != EXTENT_HEADER_MAGIC:
            raise RuntimeError(f"inode does not use extents: {magic:#x}")
        if depth != 0:
            raise RuntimeError("extent trees with depth > 0 are not supported")

        out = []
        for i in range(entries):
            off = 12 + i * 12
            logical = le32(data, off)
            length = le16(data, off + 4)
            start_hi = le16(data, off + 6)
            start_lo = le32(data, off + 8)
            out.append((logical, length, (start_hi << 32) | start_lo))
        return out

    def file_blocks(self, inode: bytes | bytearray) -> tuple[int, list[int]]:
        size = le32(inode, 4) | (le32(inode, 108) << 32)
        blocks: list[int] = []
        for _logical, length, start in self.extents(inode):
            blocks.extend(start + i for i in range(length))
        return size, blocks

    def directory_entries(self, inode_no: int) -> list[dict[str, int | str]]:
        inode = self.read_inode(inode_no)
        _size, blocks = self.file_blocks(inode)
        entries: list[dict[str, int | str]] = []
        for block in blocks:
            data = self.read_block(block)
            pos = 0
            while pos < self.block_size:
                inode_ref = le32(data, pos)
                rec_len = le16(data, pos + 4)
                name_len = data[pos + 6]
                file_type = data[pos + 7]
                if rec_len < 8:
                    break
                name = data[pos + 8 : pos + 8 + name_len].decode("utf-8", "replace")
                if inode_ref:
                    entries.append(
                        {
                            "name": name,
                            "inode": inode_ref,
                            "file_type": file_type,
                            "block": block,
                            "pos": pos,
                            "rec_len": rec_len,
                            "name_len": name_len,
                        }
                    )
                pos += rec_len
        return entries

    def lookup(self, path: str) -> int:
        inode_no = 2
        for part in [p for p in path.split("/") if p]:
            found = None
            for entry in self.directory_entries(inode_no):
                if entry["name"] == part:
                    found = int(entry["inode"])
                    break
            if found is None:
                raise FileNotFoundError(path)
            inode_no = found
        return inode_no

    def find_free_inode(self, preferred_group: int = 0) -> int:
        for group in list(range(preferred_group, self.groups)) + list(range(0, preferred_group)):
            gd = self.group_desc(group)
            if gd["free_inodes"] == 0:
                continue
            bitmap = self.read_block(gd["inode_bitmap"])
            for idx in range(self.inodes_per_group):
                byte = bitmap[idx // 8]
                if not (byte & (1 << (idx % 8))):
                    return group * self.inodes_per_group + idx + 1
        raise RuntimeError("no free inode found")

    def find_free_block(self, preferred_group: int) -> int:
        groups = list(range(preferred_group, self.groups)) + list(range(0, preferred_group))
        for group in groups:
            gd = self.group_desc(group)
            if gd["free_blocks"] == 0:
                continue
            bitmap = self.read_block(gd["block_bitmap"])
            group_start = group * self.blocks_per_group
            group_limit = min(self.blocks_per_group, self.blocks_count - group_start)
            for idx in range(group_limit):
                if bitmap[idx // 8] & (1 << (idx % 8)):
                    continue
                block = group_start + idx
                raw_off = self.block_off(block)
                if self.disk.is_allocated_range(raw_off, self.block_size):
                    return block
        raise RuntimeError("no free block found in allocated VHD ranges")

    def set_inode_bitmap(self, inode_no: int) -> None:
        group = (inode_no - 1) // self.inodes_per_group
        idx = (inode_no - 1) % self.inodes_per_group
        gd = self.group_desc(group)
        bitmap = bytearray(self.read_block(gd["inode_bitmap"]))
        if bitmap[idx // 8] & (1 << (idx % 8)):
            raise RuntimeError(f"inode {inode_no} is already allocated")
        bitmap[idx // 8] |= 1 << (idx % 8)
        self.write_block(gd["inode_bitmap"], bitmap)
        self.put_group_count(group, free_inodes=gd["free_inodes"] - 1)
        put_le32(self.super, 16, le32(self.super, 16) - 1)

    def set_block_bitmap(self, block: int) -> None:
        group = block // self.blocks_per_group
        idx = block % self.blocks_per_group
        gd = self.group_desc(group)
        bitmap = bytearray(self.read_block(gd["block_bitmap"]))
        if bitmap[idx // 8] & (1 << (idx % 8)):
            raise RuntimeError(f"block {block} is already allocated")
        bitmap[idx // 8] |= 1 << (idx % 8)
        self.write_block(gd["block_bitmap"], bitmap)
        self.put_group_count(group, free_blocks=gd["free_blocks"] - 1)
        put_le32(self.super, 12, le32(self.super, 12) - 1)

    def write_super_and_gdt(self) -> None:
        put_le32(self.super, 48, int(time.time()))
        self.disk.write(self.super_off, self.super)
        self.disk.write(self.gdt_off, self.gdt)

    def add_dir_entry(self, dir_inode_no: int, new_inode_no: int, name: str) -> None:
        entries = self.directory_entries(dir_inode_no)
        if any(e["name"] == name for e in entries):
            raise RuntimeError(f"{name} already exists")

        required = min_dir_rec_len(len(name))
        for entry in entries:
            current_min = min_dir_rec_len(int(entry["name_len"]))
            slack = int(entry["rec_len"]) - current_min
            if slack < required:
                continue

            block_no = int(entry["block"])
            pos = int(entry["pos"])
            block = bytearray(self.read_block(block_no))
            put_le16(block, pos + 4, current_min)

            new_pos = pos + current_min
            new_rec_len = int(entry["rec_len"]) - current_min
            put_le32(block, new_pos, new_inode_no)
            put_le16(block, new_pos + 4, new_rec_len)
            block[new_pos + 6] = len(name)
            block[new_pos + 7] = 1  # EXT4_FT_REG_FILE
            block[new_pos + 8 : new_pos + 8 + len(name)] = name.encode("ascii")
            self.write_block(block_no, block)
            return

        raise RuntimeError("no directory slack available for new cert entry")

    def build_regular_inode(self, data_len: int, block: int) -> bytearray:
        inode = bytearray(self.inode_size)
        now = int(time.time())
        put_le16(inode, 0, 0o100644)
        put_le32(inode, 4, data_len)
        put_le32(inode, 8, now)
        put_le32(inode, 12, now)
        put_le32(inode, 16, now)
        put_le16(inode, 26, 1)
        put_le32(inode, 28, self.block_size // SECTOR)
        put_le32(inode, 32, EXT4_EXTENTS_FL | EXT4_NOCOMPR_FL)
        put_le16(inode, 40, EXTENT_HEADER_MAGIC)
        put_le16(inode, 42, 1)  # entries
        put_le16(inode, 44, 4)  # max extents in inode body
        put_le16(inode, 46, 0)  # depth
        put_le32(inode, 52, 0)  # ee_block
        put_le16(inode, 56, 1)  # ee_len
        put_le16(inode, 58, (block >> 32) & 0xFFFF)
        put_le32(inode, 60, block & 0xFFFFFFFF)
        put_le16(inode, 128, 28)  # i_extra_isize
        return inode

    def inject_cert(self, cert_data: bytes, dry_run: bool) -> dict[str, int]:
        parent = "/etc/security/cacerts"
        parent_inode = self.lookup(parent)
        if any(e["name"] == TARGET_NAME for e in self.directory_entries(parent_inode)):
            raise RuntimeError(f"{TARGET_PATH} already exists")

        parent_blocks = self.file_blocks(self.read_inode(parent_inode))[1]
        preferred_block_group = parent_blocks[0] // self.blocks_per_group
        new_inode = self.find_free_inode(0)
        new_block = self.find_free_block(preferred_block_group)
        inode_group = (new_inode - 1) // self.inodes_per_group
        block_group = new_block // self.blocks_per_group

        plan = {
            "parent_inode": parent_inode,
            "new_inode": new_inode,
            "new_block": new_block,
            "inode_group": inode_group,
            "block_group": block_group,
        }
        if dry_run:
            return plan

        self.set_inode_bitmap(new_inode)
        self.set_block_bitmap(new_block)

        payload = cert_data + b"\0" * (self.block_size - len(cert_data))
        self.write_block(new_block, payload)
        self.write_inode(new_inode, self.build_regular_inode(len(cert_data), new_block))
        self.add_dir_entry(parent_inode, new_inode, TARGET_NAME)
        self.write_super_and_gdt()
        return plan


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--image",
        type=Path,
        default=Path(r"D:\Program Files\Microvirt\MEmu\image\96\MEmu96-2026041700027FFF-disk1.vmdk"),
    )
    parser.add_argument(
        "--cert",
        type=Path,
        default=Path(r"D:\HD\HaydayMod\tools\mitmproxy_ca.pem"),
    )
    parser.add_argument("--write", action="store_true", help="modify the VHD")
    parser.add_argument("--backup", action="store_true", help="create .bak before writing")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    cert_data = args.cert.read_bytes()
    if len(cert_data) > 4096:
        raise RuntimeError("certificate is too large for the narrow one-block injector")

    backup = args.image.with_suffix(args.image.suffix + ".bak")
    if args.write and args.backup and not backup.exists():
        print(f"Backing up {args.image} -> {backup}")
        shutil.copy2(args.image, backup)

    disk = DynamicVhd(args.image, writable=args.write)
    try:
        fs = Ext4Image(disk)
        plan = fs.inject_cert(cert_data, dry_run=not args.write)
        mode = "WRITE" if args.write else "DRY-RUN"
        print(f"{mode}: {TARGET_PATH}")
        for key, value in plan.items():
            print(f"  {key}: {value}")
    finally:
        disk.close()


if __name__ == "__main__":
    main()
