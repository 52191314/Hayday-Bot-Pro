import os
import subprocess
import re
import sys

anchorBytes = bytes.fromhex("665f932f024d060b")
replacementBytes = bytes.fromhex("5e2e00002929000047620000da440000841800003cc400007400000029660000cda90000a9b10000d4a000001cd40000a076000060e700006efd0000ec2700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000")

def run_adb(args):
    cmd = ['adb'] + args
    res = subprocess.run(cmd, capture_output=True)
    return res

def get_pid():
    res = run_adb(['shell', 'pidof com.supercell.hayday'])
    if res.returncode == 0:
        return res.stdout.decode('utf-8').strip()
    return None

def get_maps(pid):
    res = run_adb(['shell', f'su -c "cat /proc/{pid}/maps"'])
    if res.returncode != 0:
        return []
    lines = res.stdout.decode('utf-8', errors='ignore').splitlines()
    maps = []
    for line in lines:
        parts = line.split()
        if len(parts) >= 2:
            addr_range = parts[0]
            perms = parts[1]
            name = parts[-1] if len(parts) >= 6 else ""
            if 'r' in perms:
                start_str, end_str = addr_range.split('-')
                start = int(start_str, 16)
                end = int(end_str, 16)
                maps.append((start, end, perms, name))
    return maps

def patch_pid(pid):
    print(f"[*] Patching PID {pid}...")
    maps = get_maps(pid)
    
    # Prioritize libsupercell_hayday.so
    lib_maps = [m for m in maps if 'libsupercell_hayday.so' in m[3]]
    other_maps = [m for m in maps if 'libsupercell_hayday.so' not in m[3]]
    
    sorted_maps = lib_maps + other_maps
    
    found = False
    for start, end, perms, name in sorted_maps:
        length = end - start
        if length <= 0 or length > 100 * 1024 * 1024: # Skip massive regions
            continue
            
        skip = start // 4096
        count = length // 4096
        if count == 0:
            continue
            
        # Run dd to dump memory
        cmd = ['adb', 'shell', f'su -c "dd if=/proc/{pid}/mem bs=4096 skip={skip} count={count} 2>/dev/null"']
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        mem_data, _ = proc.communicate()
        
        match_idx = mem_data.find(anchorBytes)
        if match_idx != -1:
            patch_addr = start + match_idx - len(replacementBytes)
            print(f"[+] Found anchor at {hex(start + match_idx)} in {name}!")
            print(f"[*] Patching 128 bytes at {hex(patch_addr)}...")
            
            # Write to local file first
            temp_file = "patch_bytes.bin"
            with open(temp_file, "wb") as f:
                f.write(replacementBytes)
                
            # Push to emulator
            run_adb(['push', temp_file, '/data/local/tmp/patch_bytes.bin'])
            
            # Write using dd from the pushed file
            write_cmd = ['adb', 'shell', f'su -c "dd if=/data/local/tmp/patch_bytes.bin of=/proc/{pid}/mem bs=1 seek={patch_addr} conv=notrunc"']
            w_proc = subprocess.run(write_cmd, capture_output=True)
            
            # Clean up
            if os.path.exists(temp_file):
                os.remove(temp_file)
            run_adb(['shell', 'rm /data/local/tmp/patch_bytes.bin'])
            
            if w_proc.returncode == 0:
                print(f"[+] Successfully patched!")
                found = True
                break
            else:
                print(f"[!] Write failed: {w_proc.stderr.decode('utf-8')}")
                
    if not found:
        print("[!] Anchor bytes not found in any memory map region.")

if __name__ == "__main__":
    pid = get_pid()
    if not pid:
        print("[!] Hay Day process not found. Is it running?")
        sys.exit(1)
    patch_pid(pid)
