# Hay Day Tutorial Skip — Remaining Steps

## Current Status — Verified 2026-05-26
- **MEmu**: Installed at `D:\Program Files\Microvirt\MEmu`; VM can boot and ADB connects at `127.0.0.1:21503`.
- **Current VM state after verification**: Running.
- **System cert**: Installed and verified at `/system/etc/security/cacerts/c8750f0d.0`.
- **Cert verification**: `-rw-r--r-- 1 root root 1172 ... c8750f0d.0`; SHA-256 matches `tools\mitmproxy_ca.pem`.
- **Cert injection method**: Patched the dynamic VHD directly with `tools\inject_cert_direct_vhd.py`; WSL/qemu/debugfs were not required.
- **VHD backup**: `D:\Program Files\Microvirt\MEmu\image\96\MEmu96-2026041700027FFF-disk1.vmdk.bak`.
- **Hay Day**: Installed (`package:com.supercell.hayday`).
- **WireGuard**: Installed (`package:com.wireguard.android`).
- **WG client config**: Endpoint fixed to `172.27.64.1:51820`, pushed to `/sdcard/wg_client.conf`, and copied into WireGuard's imported config.
- **mitmproxy**: Running in WireGuard mode on UDP `51820`.
- **WireGuard tunnel**: Enabled; Android reports VPN `CONNECTED` on `tun0`.
- **Capture output**: `logs\hayday_capture.json` is populated and valid JSON (`743` entries at verification time).
- **Redacted summary**: `logs\hayday_capture_summary.json` generated from `tools\summarize_capture.py`.

---

## Step 1: Verify cert injection
```powershell
$MEMUC = "D:\Program Files\Microvirt\MEmu\memuc.exe"
& $MEMUC execcmd -n MEmu "ls -la /system/etc/security/cacerts/c8750f0d.0"
```
Expected and verified: `-rw-r--r-- ... 1172 ... c8750f0d.0`

If this ever needs to be repeated, stop MEmu and run:

```powershell
python tools\inject_cert_direct_vhd.py --write --backup
```

Do **not** use the live remount sequence on this VM. It caused ADB/memuc shell commands to hang and required stopping/restarting the VM.

---

## Step 2: Install Hay Day (original unmodified APK) — Done
The APKM bundle is at: `d:\HD\HaydayMod\com.supercell.hayday_1.70.96-...apkm`

Need to:
1. Extract the APKM (it's a ZIP) to get split APKs
2. Install via `adb install-multiple`

```powershell
$ADB = "D:\Program Files\Microvirt\MEmu\adb.exe"
# Extract APKM splits first, then:
& $ADB -s 127.0.0.1:21503 install-multiple -t base.apk split_config.arm64_v8a.apk split_config.en.apk split_config.xhdpi.apk split_config.ldpi.apk split_install_time_asset_pack.apk
```

Verified: `package:com.supercell.hayday`

---

## Step 3: Install WireGuard app — Done
```powershell
$ADB = "D:\Program Files\Microvirt\MEmu\adb.exe"
& $ADB -s 127.0.0.1:21503 install -r -t tools\wireguard.apk
```

Verified: `package:com.wireguard.android`

---

## Step 4: Push WireGuard config to device — Done
```powershell
& $ADB -s 127.0.0.1:21503 push tools\wg_client.conf /sdcard/wg_client.conf
```

Verified: config uses `Endpoint = 172.27.64.1:51820`.

The original `192.168.232.1:51820` endpoint was wrong for this MEmu setup. Android can reach the Windows host at `172.27.64.1` through `vEthernet (MEmuSwitch)`.

---

## Step 5: Start mitmproxy in WireGuard mode (on Windows host) — Done
```powershell
mitmdump --mode wireguard -s tools\hayday_capture.py --ssl-insecure --set block_global=false
```
This listens on UDP 51820 and creates a WireGuard VPN server that MITMs all traffic.

Verified: UDP `51820` is owned by the mitmproxy Python worker.

---

## Step 6: Import and enable WireGuard tunnel — Done
> **This requires your manual interaction in MEmu's GUI:**

1. Open the **WireGuard** app on the emulator
2. Tap **"+"** (add tunnel)
3. Select **"Import from file or archive"**
4. Navigate to `/sdcard/wg_client.conf`
5. Tap to import
6. **Toggle the tunnel ON**
7. When Android asks to allow VPN, tap **"OK"**

After this, ALL device traffic routes through mitmproxy.

Verified: Android reports VPN `CONNECTED` on `tun0`.

---

## Step 7: Launch Hay Day and capture traffic — Done
```powershell
$ADB = "D:\Program Files\Microvirt\MEmu\adb.exe"
& $ADB -s 127.0.0.1:21503 shell "am start -n com.supercell.hayday/.GameApp"
```

Verified: `logs\hayday_capture.json` is populated and valid JSON.

For a body/header-free triage summary, run:

```powershell
python tools\summarize_capture.py
```

---

## Step 8: Analyze captured API calls
Once we have the decrypted HTTPS traffic, identify:
- Account creation endpoints
- Tutorial progression API calls
- Authentication/session flow metadata; do not publish raw tokens
- Game state manipulation requests

This data enables building the automated tutorial-skip pipeline.

---

## File Locations
| File | Path |
|------|------|
| MEmu | `D:\Program Files\Microvirt\MEmu\` |
| mitmproxy CA cert | `tools\mitmproxy_ca.pem` |
| VHD cert injector | `tools\inject_cert_direct_vhd.py` |
| VHD backup | `D:\Program Files\Microvirt\MEmu\image\96\MEmu96-2026041700027FFF-disk1.vmdk.bak` |
| WireGuard APK | `tools\wireguard.apk` |
| WG client config | `tools\wg_client.conf` |
| Capture addon | `tools\hayday_capture.py` |
| Capture summary script | `tools\summarize_capture.py` |
| Hay Day APKM | `com.supercell.hayday_1.70.96-...apkm` |
| Capture output | `logs\hayday_capture.json` |
| Redacted capture summary | `logs\hayday_capture_summary.json` |
