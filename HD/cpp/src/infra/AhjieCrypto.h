#pragma once

#include <cstdint>
#include <string>

namespace ahjie {

enum class Purpose : std::uint8_t {
    Account = 1,
    Asset = 2,
};

constexpr const char* kExtension = ".Ahjie";

bool EncryptBytes(const std::string& plaintext, Purpose purpose, std::string& encrypted, std::string& error);
bool DecryptBytes(const std::string& encrypted, Purpose expectedPurpose, std::string& plaintext, std::string& error);
bool EncryptFileFromBytes(const std::string& plaintext, const std::string& outputPath, Purpose purpose, std::string& error);
bool DecryptFileToBytes(const std::string& inputPath, Purpose expectedPurpose, std::string& plaintext, std::string& error);
bool IsAhjieBytes(const std::string& data);

} // namespace ahjie
