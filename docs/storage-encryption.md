# Hay Day Storage Encryption (`storage_new.xml`)

This document details the encryption method used by Hay Day to protect local application preferences stored in `storage_new.xml`.

## Key Material and Derivation

The Hay Day `storage_new.xml` AES key has been found and validated. Supercell avoids storing the raw AES encryption key directly in memory. Instead, the 32-byte AES key is dynamically derived at runtime by hashing a specific, hardcoded seed string.

* **Base Seed String**: `0HMDC=MI9726MM<AGE35`
* **Key Derivation**: The raw AES key is the `SHA-256` hash of the Base Seed String.
  * **Derived AES Key (Hex)**: `395701810b2ca08b8162b49bc14914d5bff3852f5e112fe79845019c50c6bbd8`
  * **Derived AES Key (Base64)**: `OVcBgQssoIuBYrSbwUkU1b/zhS9eES/nmEUBnFDGu9g=`
* **CBC Initialization Vector (IV)**: `fldsjfodasjifuds` (16 bytes)
* **Padding**: Standard PKCS#5 Padding.

Validation status as of 2026-05-18:

- `tools/hayday_decrypt_storage.py` uses this seed, derived key, and IV to decrypt preference names and values.
- `tools/hayday_validate_aes_key.py` can validate proposed key material against base64-looking SharedPreferences entries.
- `kdsBDLvTJkt5ScQihjAVGA==` decrypts to `higher_env3`, confirming the known drop-group preference name.

## Encryption Schema

The `storage_new.xml` file consists of `<string>` tags where both the `name` attribute (the key) and the text content (the value) are encrypted and then Base64-encoded.

1. **Preference Names (Keys)**
   * **Mode**: AES-256-ECB (Electronic Codebook)
   * **Process**: `Base64(AES_ECB_Encrypt(PKCS5_Pad(Plaintext_Name), Derived_AES_Key))`
   * *Note: ECB mode does not use an IV. This is why the same preference name always produces the exact same encrypted Base64 string across all devices and accounts.*

2. **Preference Values**
   * **Mode**: AES-256-CBC (Cipher Block Chaining)
   * **Process**: `Base64(AES_CBC_Encrypt(PKCS5_Pad(Plaintext_Value), Derived_AES_Key, IV))`

## Known Drop Group Mappings

The drop rate configuration is stored inside `storage_new.xml`. The encrypted key `kdsBDLvTJkt5ScQihjAVGA==` decrypts to `higher_env3`, which dictates the account's drop group. The mirror key is `SCIDGuestHigh_env3`.

The encrypted values for this preference map to integers from `0` to `25`. Below is the complete decryption mapping:

| Encrypted Token (AES-CBC) | Decrypted Integer | Known Label / Notes |
| :--- | :--- | :--- |
| `Jl5SbAzoQwaeg2RjwlcQOw==` | `0` | |
| `TXKNN1ArtjKBJc2ljiqU8g==` | `1` | |
| `K9t6nqOjkfMlMXxpMk1RkA==` | `2` | |
| `bLYR4rRV4nyh0dHwrmv7mQ==` | `3` | |
| `ntmNPvN5kt8QTw4Y6JQ+Ag==` | `4` | Current Live Value (from tests) |
| `FJfTIXasc/lRURJ+IFkUAw==` | `5` | |
| `AQGSk0QIJwUE/5cDG9PuNQ==` | `6` | |
| `FP8NMEcEAYL4fqaWAgnA+Q==` | `7` | |
| `/iyEXNO/aVGkepdL2jTGvw==` | `8` | |
| `U7LGjFbjcjqnLLaECHNnYg==` | `9` | |
| `ZPGDNIgLEAT3TH4JpNj5yA==` | `10` | |
| `6hN2iWcjO7L64as2ecPD+A==` | `11` | |
| `iDAgilwxSHVWvIIT50RGFw==` | `12` | |
| `dqIbyTvSRWefYz3hSclnhQ==` | `13` | **Panel** |
| `U0T54PrutCEs/ZcLH4JaNw==` | `14` | |
| `2KkQmlKTdiX0EJ4nraacRQ==` | `15` | |
| `0DyYoY9bFLlMulLs4DluIw==` | `16` | |
| `a/kqNh1qfhLbR59W/DZXAw==` | `17` | |
| `2M5AGA8P7Bi43GVxol7xJQ==` | `18` | |
| `Fz1xfIU6iG4VWnFCnOV1Nw==` | `19` | |
| `ho0laYGTlc0c8b6gkwCuUA==` | `20` | |
| `Y/o2rdafcnLxV9dA5WzN5w==` | `21` | **High Tape (slot 4)** |
| `UjS3MX/EFs0qVATFMn2gOw==` | `22` | |
| `f511juiL1/Tp/k/tEaLdow==` | `23` | |
| `albqTh4suymNYvoeYJW8xQ==` | `24` | |
| `JyQYP8z7U7/pjnSOGOPb2A==` | `25` | Highest possible group |

## Implementation Code

A Python utility to decrypt `storage_new.xml` is available in `tools/hayday_decrypt_storage.py`. It uses the `pycryptodome` library.

### Decryption Logic Snippet
```python
import base64
import hashlib
from Crypto.Cipher import AES

HD_BASE_KEY = b"0HMDC=MI9726MM<AGE35"
HD_AES_KEY = hashlib.sha256(HD_BASE_KEY).digest()
HD_CBC_IV  = b"fldsjfodasjifuds"

def pkcs5_unpad(data: bytes) -> bytes:
    pad_len = data[-1]
    return data[:-pad_len]

def decrypt_name(b64_ct: str) -> str:
    ct = base64.b64decode(b64_ct)
    cipher = AES.new(HD_AES_KEY, AES.MODE_ECB)
    return pkcs5_unpad(cipher.decrypt(ct)).decode('utf-8')

def decrypt_value(b64_ct: str) -> str:
    ct = base64.b64decode(b64_ct)
    cipher = AES.new(HD_AES_KEY, AES.MODE_CBC, HD_CBC_IV)
    return pkcs5_unpad(cipher.decrypt(ct)).decode('utf-8')
```
