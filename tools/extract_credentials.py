#!/usr/bin/env python3
"""
extract_credentials.py
Decrypts Hay Day storage_new.xml and extracts login credentials for the headless client.

AES key derivation from docs/storage-encryption.md:
  Base seed : 0HMDC=MI9726MM<AGE35
  AES key   : SHA-256(seed)
  ECB mode  : preference names
  CBC mode  : preference values  IV=fldsjfodasjifuds
"""

import base64
import hashlib
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

try:
    from Crypto.Cipher import AES
except ImportError:
    print("[!] pycryptodome not found. Run: pip install pycryptodome")
    sys.exit(1)

# ── Key material ──────────────────────────────────────────────────────────────
HD_BASE_KEY = b"0HMDC=MI9726MM<AGE35"
HD_AES_KEY  = hashlib.sha256(HD_BASE_KEY).digest()
HD_CBC_IV   = b"fldsjfodasjifuds"

def pkcs5_unpad(data: bytes) -> bytes:
    pad_len = data[-1]
    return data[:-pad_len]

def decrypt_name(b64_ct: str) -> str | None:
    try:
        ct = base64.b64decode(b64_ct)
        cipher = AES.new(HD_AES_KEY, AES.MODE_ECB)
        return pkcs5_unpad(cipher.decrypt(ct)).decode("utf-8")
    except Exception:
        return None

def decrypt_value(b64_ct: str) -> str | None:
    try:
        ct = base64.b64decode(b64_ct)
        cipher = AES.new(HD_AES_KEY, AES.MODE_CBC, HD_CBC_IV)
        return pkcs5_unpad(cipher.decrypt(ct)).decode("utf-8")
    except Exception:
        return None

# ── Fields of interest ───────────────────────────────────────────────────────
# We decrypt ALL keys; the ones we care about for login:
LOGIN_FIELDS = {
    "SCIDGuestHigh_env3",    # AccountId (high 32 bits)
    "SCIDGuestLow_env3",     # AccountId (low 32 bits)
    "SCIDGuestToken_env3",   # PassToken
    "SCIDGuestUdid_env3",    # Device UID (UdId)
    "higher_env3",           # Drop group (informational)
    # Alternate names seen in some versions:
    "user_id",
    "pass_token",
    "account_id",
    "udid",
    "token",
    "device_model",
    "os_version",
    "locale",
    "mac_address",
    "open_udid",
    "ad_id",
}

def decrypt_xml(xml_path: Path) -> dict:
    try:
        content = xml_path.read_text(encoding="utf-16")
    except UnicodeDecodeError:
        content = xml_path.read_text(encoding="utf-8")
    
    if "</map>" in content:
        content = content[:content.find("</map>") + 6]
    
    root = ET.fromstring(content)
    result = {}
    for elem in root.iter("string"):
        enc_name = elem.get("name", "")
        enc_val  = (elem.text or "").strip()
        plain_name = decrypt_name(enc_name)
        plain_val  = decrypt_value(enc_val) if enc_val else ""
        if plain_name is not None:
            result[plain_name] = plain_val
    return result

def main():
    repo_root = Path(__file__).parent.parent

    # Prefer the live_farm snapshot (most recent active account)
    candidates = [
        repo_root / "analysis_artifacts" / "pulled_data" / "2026-05-06_live_farm" / "storage_new.xml",
        repo_root / "Account_backup" / "Instance_0" / "account_1_storage_new.xml",
        repo_root / "Account_backup" / "Instance_0" / "account_2_storage_new.xml",
    ]

    # Allow override via command-line argument (handles shortcuts like '1', '2' or 'live')
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg.isdigit():
            candidates = [repo_root / "Account_backup" / "Instance_0" / f"account_{arg}_storage_new.xml"]
        elif arg.lower() == "live":
            candidates = [repo_root / "analysis_artifacts" / "pulled_data" / "2026-05-06_live_farm" / "storage_new.xml"]
        else:
            candidates = [Path(arg)]

    decrypted = {}
    used_file = None
    for candidate in candidates:
        if candidate.exists():
            print(f"[*] Decrypting {candidate} ...")
            decrypted = decrypt_xml(candidate)
            used_file = candidate
            break

    if not decrypted:
        print(f"[!] No storage_new.xml found at the expected paths: {[str(p) for p in candidates]}")
        sys.exit(1)

    print(f"\n[+] Decrypted {len(decrypted)} preference entries from {used_file}\n")

    # ── Print ALL decrypted keys ──────────────────────────────────────────────
    print("=== All Decrypted Preferences ===")
    for k, v in sorted(decrypted.items()):
        display_v = v if len(v) < 80 else v[:77] + "..."
        print(f"  {k!r:50s} = {display_v!r}")

    # ── Extract login credentials ─────────────────────────────────────────────
    print("\n=== Login Credentials ===")
    creds = {}
    
    # 1. Look for SCID_PROD_ACCOUNTS and parse JWT Token
    scid_accounts_raw = decrypted.get("SCID_PROD_ACCOUNTS")
    scid_token = None
    scid_id = None
    prod_high = None
    prod_low = None
    
    if scid_accounts_raw:
        try:
            accounts_data = json.loads(scid_accounts_raw)
            current_scid = decrypted.get("SCID_PROD_CURRENT_ACCOUNT_SUPERCELL_ID")
            
            # Find the active account details
            account_details = None
            if current_scid and current_scid in accounts_data:
                account_details = accounts_data[current_scid]
            elif accounts_data:
                # Fallback to the first account in the dict
                first_key = list(accounts_data.keys())[0]
                account_details = accounts_data[first_key]
                
            if account_details:
                scid_token = account_details.get("token")
                scid_id = account_details.get("supercellId")
                print(f"  [+] Found Supercell ID: {scid_id}")
                
                # Parse JWT payload to extract production account ID
                if scid_token:
                    try:
                        parts = scid_token.split(".")
                        if len(parts) == 3:
                            payload_b64 = parts[1]
                            payload_b64 += "=" * ((4 - len(payload_b64) % 4) % 4)
                            payload = json.loads(base64.urlsafe_b64decode(payload_b64).decode("utf-8"))
                            pid = payload.get("pid") or payload.get("https://id.supercell.com/appAccountId")
                            if pid and "-" in pid:
                                h_str, l_str = pid.split("-", 1)
                                prod_high = int(h_str)
                                prod_low = int(l_str)
                                print(f"  [+] Parsed production account ID from JWT: {prod_high}-{prod_low}")
                    except Exception as e:
                        print(f"  [!] Failed to parse JWT payload: {e}")
        except Exception as e:
            print(f"  [!] Failed to parse SCID_PROD_ACCOUNTS: {e}")

    # 2. Extract standard guest/login credentials
    for field in sorted(LOGIN_FIELDS):
        val = decrypted.get(field)
        if val is not None:
            creds[field] = val

    # 3. Determine AccountIdHigh, AccountIdLow, and PassToken
    # If production ID was parsed from JWT, prioritize it!
    if prod_high is not None and prod_low is not None:
        creds["AccountIdHigh"] = prod_high
        creds["AccountIdLow"] = prod_low
    else:
        # Fallback to guest high/low
        high_str = decrypted.get("SCIDGuestHigh_env3") or decrypted.get("account_id_high")
        low_str  = decrypted.get("SCIDGuestLow_env3")  or decrypted.get("account_id_low")
        if high_str and low_str:
            try:
                creds["AccountIdHigh"] = int(high_str)
                creds["AccountIdLow"] = int(low_str)
            except ValueError:
                pass
                
    # Extract PassToken
    guest_pass = decrypted.get("SCIDGuestPass_env3") or decrypted.get("passToken_env3") or decrypted.get("pass_token")
    if guest_pass:
        creds["PassToken"] = guest_pass
        
    if scid_token:
        creds["ScIdToken"] = scid_token
    if scid_id:
        creds["SupercellId"] = scid_id

    # Compute AccountIdFull for convenience
    if "AccountIdHigh" in creds and "AccountIdLow" in creds:
        account_id_full = (creds["AccountIdHigh"] << 32) | creds["AccountIdLow"]
        creds["AccountIdFull"] = account_id_full
        print(f"  Computed AccountIdHigh = {creds['AccountIdHigh']}")
        print(f"  Computed AccountIdLow  = {creds['AccountIdLow']}")
        print(f"  Computed AccountIdFull = {account_id_full}")

    # ── Write credentials.json ────────────────────────────────────────────────
    out_path = repo_root / "tools" / "credentials.json"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(creds, f, indent=2)
    print(f"\n[+] Credentials saved to {out_path}")

if __name__ == "__main__":
    main()
