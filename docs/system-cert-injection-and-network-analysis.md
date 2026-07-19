# Hay Day Network Analysis & System Certificate Injection

## Objective
To capture and analyze Hay Day's HTTPS and binary protocol (port 9339) traffic to automate account progression bypass (bypassing the initial tutorial). 

Hay Day uses Android's `network_security_config.xml` to strictly enforce `system` trust anchors, effectively blocking user-installed certificates. Furthermore, the game is protected by **Promon Shield**, an anti-tamper mechanism that detects modified APKs and crashes the game.

## Approach History

### 1. APK Patching (Failed)
*   **Method**: Extracted `base.apk`, patched `network_security_config.xml` to allow `<certificates src="user" />`, and re-signed using a custom JAR v1 signer.
*   **Result**: ❌ Failed. Promon Shield detected the modified APK signatures/contents and forcefully crashed the game upon launch.

### 2. System Certificate Injection via MEmu VM Image (Successful)
Since modifying the APK triggers anti-cheat, the only viable approach was to inject our proxy's CA certificate directly into the Android OS **System Certificate Store**. This allows the **unmodified, original APK** to trust our MITM proxy.

#### Step-by-Step Injection Process
This process was performed on the MEmu emulator's underlying virtual disk using WSL (Windows Subsystem for Linux).

1.  **Locate the VMDK**: The MEmu system disk is located at `C:\Program Files\Microvirt\MEmu\image\96\MEmu96-2026041700027FFF-disk1.vmdk`. Note: Despite the `.vmdk` extension, it is actually a **VPC (VHD)** formatted image.
2.  **Convert to Raw Image**:
    ```bash
    qemu-img convert -f vpc -O raw system_disk1.vmdk system_disk1.raw
    ```
3.  **Extract System Partition**: 
    Using `fdisk -l`, we identified the `ext4` Linux system partition (Partition 6). We extracted it using `dd`:
    ```bash
    dd if=system_disk1.raw of=system_part6.img bs=512 skip=68532 count=5242880 status=progress
    ```
4.  **Inject Certificate via `debugfs`**:
    Using `debugfs`, we injected the `mitmproxy` CA certificate (renamed to its hash, `c8750f0d.0`) directly into the `ext4` filesystem and set the correct permissions:
    ```bash
    debugfs -w -R "write c8750f0d.0 etc/security/cacerts/c8750f0d.0" system_part6.img
    debugfs -w -R "set_inode_field etc/security/cacerts/c8750f0d.0 mode 0100644" system_part6.img
    ```
5.  **Reassemble and Convert**:
    We wrote the modified partition back to the raw image, then converted it back to VPC format:
    ```bash
    dd if=system_part6.img of=system_disk1.raw bs=512 seek=68532 count=5242880 conv=notrunc status=progress
    qemu-img convert -f raw -O vpc system_disk1.raw system_disk1_patched.vmdk
    ```
6.  **Deploy**: 
    Replaced the original MEmu VMDK with the patched one and booted the emulator.
    *   **Result**: ✅ Success. The `mitmproxy` certificate is now trusted by the Android System. The unmodified Hay Day APK launches without Promon Shield interference.

---

## Network Routing & Capture Challenges

With the system certificate in place, we attempted to capture the traffic. 

### Findings
1.  **Direct Connections**: Hay Day's native C++ networking stack **ignores** the Android OS-level HTTP proxy settings. 
2.  **Ports in Use**: `tcpdump` analysis of the game's active connections (`/proc/net/tcp` and `/proc/net/tcp6`) revealed:
    *   Direct HTTPS connections (Port `443`) to Supercell servers (e.g., `inventory.mtech.supercell.com`).
    *   Direct custom binary protocol connections (Port `9339`) for real-time game state.
3.  **Capture Attempts**:
    *   **`tcpdump`**: Successfully captured 18MB of game traffic on the device. However, because it only sniffs raw packets without performing a MITM TLS handshake, the HTTPS traffic remains encrypted with Perfect Forward Secrecy (PFS) and cannot be decrypted offline.
    *   **`mitmproxy --mode local`**: Failed to capture MEmu traffic because the emulator's network stack bypasses the Windows host's local loopback interception.
    *   **`mitmproxy --mode wireguard`**: Successfully initialized a WireGuard server, but Android configuration integration within MEmu requires further manual networking setup.

## Next Steps for Full Decryption

To successfully decrypt the PFS-encrypted HTTPS traffic, we must force the game's traffic to flow *through* `mitmproxy` so it can actively terminate and re-encrypt the TLS connections.

**Proposed Solutions:**
1.  **On-Device iptables + redsocks (Recommended)**:
    *   Install and configure `redsocks` directly on the MEmu Android environment.
    *   Use Android's `iptables` to `REDIRECT` all outgoing traffic on ports `443` and `9339` from the Hay Day UID (`10072`) to the local `redsocks` port.
    *   `redsocks` will forward the traffic to `mitmproxy` running as a SOCKS5 proxy on the Windows host.
2.  **Host-side iptables / Routing**:
    *   Use MEmu's network bridge mode to expose the Android VM as a distinct IP on the LAN.
    *   Configure the host machine as the VM's gateway and use Linux/WSL `iptables` to transparently DNAT traffic to `mitmproxy`.

### TL;DR
- Anti-cheat (Promon) forces us to use the **original APK**.
- Original APK requires a **System Certificate**, which we successfully injected by hex-editing the MEmu VMDK disk image.
- Game ignores proxy settings, so we must use **iptables redirection** (transparent proxying) to force traffic into `mitmproxy` for decryption.
