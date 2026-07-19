# `.YaJing` Encryption

`.YaJing` replaces the old `.nxrth` XOR+hex wrapper for account slots and visual injector payloads.

## File Format

Every `.YaJing` file uses this binary layout:

```text
offset  size  value
0       6     ASCII "YaJing"
6       1     version = 1
7       1     purpose
8       12    AES-GCM nonce
20      16    AES-GCM tag
36      *     AES-GCM ciphertext
```

Purpose values:

- `1`: account backup payload
- `2`: visual asset payload

The 8-byte header is authenticated data for AES-GCM. Changing the magic, version, purpose, nonce, tag, or ciphertext makes decrypt fail.

## Crypto

- Algorithm: AES-256-GCM
- Nonce: 12 random bytes per file
- Tag: 16 bytes
- Account key size: 32 bytes
- Asset key size: 32 bytes

## Account Key

Account saves use per-user key material protected by Windows DPAPI.

Key path:

```text
%APPDATA%\NXRTH_Premium\YaJing\account_master.key.dpapi
```

The first account save or account migration creates a random 32-byte account key, protects it with `CryptProtectData`, and stores it at that path. Later account decrypts unprotect it with `CryptUnprotectData`.

This means account `.YaJing` files are tied to the Windows user profile that owns the DPAPI key. Export/import through the app re-encrypts imported legacy account data into the current user's `.YaJing` key.

## Asset Key

Visual injector payloads use a deterministic compiled asset key so the C++ injector and Python rebuild script can decrypt the same files.

The current asset key is:

```text
SHA256("HaydayMod.YaJing.asset-payloads.v1.NXRTH_NATURE_KEY.replacement")
```

Changing this string requires rebuilding all `.YaJing` visual payloads and rebuilding the C++ app with the same seed.

The 2026-05-18 active visual rebuild at `installed_backup/fresh_sctx_build_20260518_165805` was encrypted with this asset-key seed and copied into `HD/x64/Release/injecthacks`.

## Legacy `.nxrth`

Legacy `.nxrth` is XOR+hex with a 16-character prefix and suffix. It is retained only for migration:

- Account load migrates `account_N.nxrth` to `account_N.YaJing` if no `.YaJing` exists.
- `tools/convert_nxrth_to_yajing.py` converts legacy account or asset files without changing decrypted bytes.
- The visual injector no longer consumes `.nxrth` payloads directly.

## Migration Examples

Convert one legacy asset payload:

```powershell
python tools\convert_nxrth_to_yajing.py --purpose asset HD\x64\Release\injecthacks\inject.nxrth
```

Convert a folder of legacy account backups:

```powershell
python tools\convert_nxrth_to_yajing.py --purpose account --recursive "%APPDATA%\NXRTH_Premium\Backups"
```

The converter writes `.YaJing` next to the source file by default and leaves the old `.nxrth` file untouched.

## Verification

Expected checks:

- Encrypt/decrypt round-trip returns the exact original bytes.
- Flipping one ciphertext or tag byte makes decrypt fail.
- Legacy `.nxrth` conversion decrypts to the exact same raw payload before and after conversion.
