#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "cpp/src/infra/AhjieCrypto.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cwchar>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

namespace ahjie {
namespace {

constexpr unsigned char kMagic[] = { 'A', 'h', 'j', 'i', 'e' };
constexpr unsigned char kVersion = 1;
constexpr size_t kMagicSize = sizeof(kMagic);
constexpr size_t kHeaderSize = kMagicSize + 2;
constexpr size_t kNonceSize = 12;
constexpr size_t kTagSize = 16;
constexpr size_t kKeySize = 32;

const unsigned char kAssetKeySeed[] =
    "HaydayMod.Ahjie.asset-payloads.v1.NXRTH_NATURE_KEY.replacement";

struct BCryptAlgHandle {
    BCRYPT_ALG_HANDLE value = nullptr;
    ~BCryptAlgHandle() {
        if (value) BCryptCloseAlgorithmProvider(value, 0);
    }
};

struct BCryptKeyHandle {
    BCRYPT_KEY_HANDLE value = nullptr;
    ~BCryptKeyHandle() {
        if (value) BCryptDestroyKey(value);
    }
};

std::string StatusError(const char* label, NTSTATUS status) {
    return std::string(label) + " failed: NTSTATUS=0x" + [] (NTSTATUS code) {
        char buf[16];
        sprintf_s(buf, "%08X", static_cast<unsigned int>(code));
        return std::string(buf);
    }(status);
}

bool ReadAllBytes(const std::string& path, std::string& data, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Could not open " + path;
        return false;
    }
    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool WriteAllBytes(const std::string& path, const std::string& data, std::string& error) {
    std::filesystem::path outPath(path);
    std::error_code ec;
    if (!outPath.parent_path().empty()) {
        std::filesystem::create_directories(outPath.parent_path(), ec);
        if (ec) {
            error = "Could not create " + outPath.parent_path().string() + ": " + ec.message();
            return false;
        }
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        error = "Could not write " + path;
        return false;
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

std::filesystem::path AccountKeyPath() {
    const char* appData = std::getenv("APPDATA");
    std::filesystem::path root = (appData && *appData)
        ? std::filesystem::path(appData)
        : std::filesystem::current_path();
    return root / "NXRTH_Premium" / "Ahjie" / "account_master.key.dpapi";
}

bool RandomBytes(unsigned char* data, size_t size, std::string& error) {
    NTSTATUS status = BCryptGenRandom(nullptr, data, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status < 0) {
        error = StatusError("BCryptGenRandom", status);
        return false;
    }
    return true;
}

bool HashSha256(const unsigned char* data, size_t size, std::array<unsigned char, kKeySize>& hash, std::string& error) {
    BCryptAlgHandle alg;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0) {
        error = StatusError("BCryptOpenAlgorithmProvider(SHA256)", status);
        return false;
    }

    DWORD objectLength = 0;
    DWORD written = 0;
    status = BCryptGetProperty(alg.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &written, 0);
    if (status < 0) {
        error = StatusError("BCryptGetProperty(hash object length)", status);
        return false;
    }

    std::vector<unsigned char> hashObject(objectLength);
    BCRYPT_HASH_HANDLE hashHandle = nullptr;
    status = BCryptCreateHash(alg.value, &hashHandle, hashObject.data(), objectLength, nullptr, 0, 0);
    if (status < 0) {
        error = StatusError("BCryptCreateHash", status);
        return false;
    }

    status = BCryptHashData(hashHandle, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0);
    if (status >= 0) {
        status = BCryptFinishHash(hashHandle, hash.data(), static_cast<ULONG>(hash.size()), 0);
    }
    BCryptDestroyHash(hashHandle);

    if (status < 0) {
        error = StatusError("BCryptHashData/FinishHash", status);
        return false;
    }
    return true;
}

bool ProtectAccountKey(const std::array<unsigned char, kKeySize>& key, std::string& protectedData, std::string& error) {
    DATA_BLOB input{};
    input.pbData = const_cast<BYTE*>(key.data());
    input.cbData = static_cast<DWORD>(key.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"Ahjie account key", nullptr, nullptr, nullptr, 0, &output)) {
        error = "CryptProtectData failed: " + std::to_string(GetLastError());
        return false;
    }
    protectedData.assign(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return true;
}

bool UnprotectAccountKey(const std::string& protectedData, std::array<unsigned char, kKeySize>& key, std::string& error) {
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(protectedData.data()));
    input.cbData = static_cast<DWORD>(protectedData.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        error = "CryptUnprotectData failed: " + std::to_string(GetLastError());
        return false;
    }
    if (output.cbData != key.size()) {
        LocalFree(output.pbData);
        error = "DPAPI account key has invalid length";
        return false;
    }
    memcpy(key.data(), output.pbData, key.size());
    LocalFree(output.pbData);
    return true;
}

bool LoadOrCreateAccountKey(std::array<unsigned char, kKeySize>& key, std::string& error) {
    const std::filesystem::path keyPath = AccountKeyPath();
    std::string protectedData;
    if (std::filesystem::exists(keyPath)) {
        if (!ReadAllBytes(keyPath.string(), protectedData, error)) return false;
        return UnprotectAccountKey(protectedData, key, error);
    }

    if (!RandomBytes(key.data(), key.size(), error)) return false;
    if (!ProtectAccountKey(key, protectedData, error)) return false;
    return WriteAllBytes(keyPath.string(), protectedData, error);
}

bool KeyForPurpose(Purpose purpose, std::array<unsigned char, kKeySize>& key, std::string& error) {
    if (purpose == Purpose::Account) {
        return LoadOrCreateAccountKey(key, error);
    }
    if (purpose == Purpose::Asset) {
        return HashSha256(kAssetKeySeed, sizeof(kAssetKeySeed) - 1, key, error);
    }
    error = "Unknown Ahjie purpose";
    return false;
}

bool OpenAesGcm(const std::array<unsigned char, kKeySize>& key, BCryptAlgHandle& alg, BCryptKeyHandle& keyHandle, std::vector<unsigned char>& keyObject, std::string& error) {
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg.value, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (status < 0) {
        error = StatusError("BCryptOpenAlgorithmProvider(AES)", status);
        return false;
    }

    status = BCryptSetProperty(
        alg.value,
        BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
        static_cast<ULONG>((wcslen(BCRYPT_CHAIN_MODE_GCM) + 1) * sizeof(wchar_t)),
        0);
    if (status < 0) {
        error = StatusError("BCryptSetProperty(GCM)", status);
        return false;
    }

    DWORD objectLength = 0;
    DWORD written = 0;
    status = BCryptGetProperty(alg.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &written, 0);
    if (status < 0) {
        error = StatusError("BCryptGetProperty(AES object length)", status);
        return false;
    }

    keyObject.assign(objectLength, 0);
    status = BCryptGenerateSymmetricKey(
        alg.value,
        &keyHandle.value,
        keyObject.data(),
        objectLength,
        const_cast<PUCHAR>(key.data()),
        static_cast<ULONG>(key.size()),
        0);
    if (status < 0) {
        error = StatusError("BCryptGenerateSymmetricKey", status);
        return false;
    }
    return true;
}

std::array<unsigned char, kHeaderSize> HeaderFor(Purpose purpose) {
    std::array<unsigned char, kHeaderSize> header{};
    memcpy(header.data(), kMagic, kMagicSize);
    header[kMagicSize] = kVersion;
    header[kMagicSize + 1] = static_cast<unsigned char>(purpose);
    return header;
}

} // namespace

bool IsAhjieBytes(const std::string& data) {
    return data.size() >= kHeaderSize
        && memcmp(data.data(), kMagic, kMagicSize) == 0
        && static_cast<unsigned char>(data[kMagicSize]) == kVersion;
}

bool EncryptBytes(const std::string& plaintext, Purpose purpose, std::string& encrypted, std::string& error) {
    std::array<unsigned char, kKeySize> key{};
    if (!KeyForPurpose(purpose, key, error)) return false;

    std::array<unsigned char, kNonceSize> nonce{};
    if (!RandomBytes(nonce.data(), nonce.size(), error)) return false;

    BCryptAlgHandle alg;
    BCryptKeyHandle keyHandle;
    std::vector<unsigned char> keyObject;
    if (!OpenAesGcm(key, alg, keyHandle, keyObject, error)) return false;

    std::array<unsigned char, kTagSize> tag{};
    std::vector<unsigned char> ciphertext(plaintext.size());
    auto header = HeaderFor(purpose);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = static_cast<ULONG>(nonce.size());
    authInfo.pbAuthData = header.data();
    authInfo.cbAuthData = static_cast<ULONG>(header.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());

    ULONG written = 0;
    NTSTATUS status = BCryptEncrypt(
        keyHandle.value,
        reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
        static_cast<ULONG>(plaintext.size()),
        &authInfo,
        nullptr,
        0,
        ciphertext.empty() ? nullptr : ciphertext.data(),
        static_cast<ULONG>(ciphertext.size()),
        &written,
        0);
    if (status < 0) {
        error = StatusError("BCryptEncrypt", status);
        return false;
    }

    encrypted.assign(reinterpret_cast<const char*>(header.data()), header.size());
    encrypted.append(reinterpret_cast<const char*>(nonce.data()), nonce.size());
    encrypted.append(reinterpret_cast<const char*>(tag.data()), tag.size());
    if (!ciphertext.empty()) encrypted.append(reinterpret_cast<const char*>(ciphertext.data()), written);
    return true;
}

bool DecryptBytes(const std::string& encrypted, Purpose expectedPurpose, std::string& plaintext, std::string& error) {
    if (encrypted.size() < kHeaderSize + kNonceSize + kTagSize || !IsAhjieBytes(encrypted)) {
        error = "Not an Ahjie payload";
        return false;
    }
    const unsigned char purposeByte = static_cast<unsigned char>(encrypted[kMagicSize + 1]);
    if (purposeByte != static_cast<unsigned char>(expectedPurpose)) {
        error = "Ahjie purpose mismatch";
        return false;
    }

    std::array<unsigned char, kKeySize> key{};
    if (!KeyForPurpose(expectedPurpose, key, error)) return false;

    BCryptAlgHandle alg;
    BCryptKeyHandle keyHandle;
    std::vector<unsigned char> keyObject;
    if (!OpenAesGcm(key, alg, keyHandle, keyObject, error)) return false;

    const unsigned char* header = reinterpret_cast<const unsigned char*>(encrypted.data());
    const unsigned char* nonce = header + kHeaderSize;
    const unsigned char* tag = nonce + kNonceSize;
    const unsigned char* ciphertext = tag + kTagSize;
    const size_t ciphertextSize = encrypted.size() - kHeaderSize - kNonceSize - kTagSize;
    std::vector<unsigned char> output(ciphertextSize);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = const_cast<PUCHAR>(nonce);
    authInfo.cbNonce = static_cast<ULONG>(kNonceSize);
    authInfo.pbAuthData = const_cast<PUCHAR>(header);
    authInfo.cbAuthData = static_cast<ULONG>(kHeaderSize);
    authInfo.pbTag = const_cast<PUCHAR>(tag);
    authInfo.cbTag = static_cast<ULONG>(kTagSize);

    ULONG written = 0;
    NTSTATUS status = BCryptDecrypt(
        keyHandle.value,
        const_cast<PUCHAR>(ciphertext),
        static_cast<ULONG>(ciphertextSize),
        &authInfo,
        nullptr,
        0,
        output.empty() ? nullptr : output.data(),
        static_cast<ULONG>(output.size()),
        &written,
        0);
    if (status < 0) {
        error = StatusError("BCryptDecrypt", status);
        return false;
    }

    plaintext.assign(reinterpret_cast<const char*>(output.data()), written);
    return true;
}

bool EncryptFileFromBytes(const std::string& plaintext, const std::string& outputPath, Purpose purpose, std::string& error) {
    std::string encrypted;
    if (!EncryptBytes(plaintext, purpose, encrypted, error)) return false;
    return WriteAllBytes(outputPath, encrypted, error);
}

bool DecryptFileToBytes(const std::string& inputPath, Purpose expectedPurpose, std::string& plaintext, std::string& error) {
    std::string encrypted;
    if (!ReadAllBytes(inputPath, encrypted, error)) return false;
    return DecryptBytes(encrypted, expectedPurpose, plaintext, error);
}

} // namespace ahjie
