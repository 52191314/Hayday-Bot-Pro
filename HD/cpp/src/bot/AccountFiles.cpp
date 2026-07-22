#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/bot/AccountFiles.h"
#include "cpp/src/bot/EmulatorControl.h"
#include "cpp/src/infra/AhjieCrypto.h"

#include <set>

namespace fs = std::filesystem;

// INJECT FOLDERS
const std::string GAME_DATA_PATH = "/data/data/com.supercell.hayday/shared_prefs/storage_new.xml";
const std::string ZOOM_DATA_PATH = "/data/data/com.supercell.hayday/update/data/game_config.csv";

std::string DecryptPureXORHex(const std::string& hexStr, const std::string& key);

namespace {
struct AccountFileValidation {
    bool safeToLoad = false;
    size_t rawBytes = 0;
    int keyCount = 0;
    std::string detail;
};

std::string BuildAccountFolderPath(int instanceId) {
    return GetAppDataPath() + "\\Backups\\Instance_" + std::to_string(instanceId);
}

std::string BuildAccountSlotPath(int instanceId, int slotIndex) {
    return BuildAccountFolderPath(instanceId) + "\\account_" + std::to_string(slotIndex + 1) + ahjie::kExtension;
}

std::string BuildLegacyAccountSlotPath(int instanceId, int slotIndex) {
    return BuildAccountFolderPath(instanceId) + "\\account_" + std::to_string(slotIndex + 1) + ".nxrth";
}

std::string GetExecutableDirectory() {
    char buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        std::error_code ec;
        return fs::current_path(ec).string();
    }
    std::string path(buffer, len);
    const std::string::size_type pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        std::error_code ec;
        return fs::current_path(ec).string();
    }
    return path.substr(0, pos);
}

std::string ResolveRuntimeRootFolder() {
    std::error_code ec;
    fs::path probe = GetExecutableDirectory();
    fs::path resolvedRoot;
    bool found = false;
    for (int depth = 0; depth < 5; ++depth) {
        if (fs::exists(probe / "injecthacks", ec) || fs::exists(probe / "templates", ec)) {
            resolvedRoot = probe;
            found = true;
        }
        if (!probe.has_parent_path()) break;
        probe = probe.parent_path();
    }
    if (found) {
        return resolvedRoot.string();
    }
    return GetExecutableDirectory();
}

bool IsAllDigits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

void AppendAutoDiscoveredExtraVisualPayloads(std::vector<std::pair<std::string, std::string>>& extraVisualMap, const fs::path& injectDir) {
    std::set<std::string> mappedPayloads;
    for (const auto& item : extraVisualMap) {
        mappedPayloads.insert(item.first);
    }

    std::error_code ec;
    if (!fs::is_directory(injectDir, ec)) return;

    const std::string prefix = "inject_extra_";
    for (const fs::directory_entry& entry : fs::directory_iterator(injectDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        const fs::path path = entry.path();
        if (path.extension().string() != ahjie::kExtension) continue;

        const std::string fileName = path.filename().string();
        if (mappedPayloads.find(fileName) != mappedPayloads.end()) continue;

        const std::string stem = path.stem().string();
        if (stem.rfind(prefix, 0) != 0) continue;
        const std::string targetStem = stem.substr(prefix.size());
        const std::string::size_type lastUnderscore = targetStem.find_last_of('_');
        if (lastUnderscore == std::string::npos) continue;
        if (!IsAllDigits(targetStem.substr(lastUnderscore + 1))) continue;

        extraVisualMap.push_back({ fileName, targetStem + ".sctx" });
        mappedPayloads.insert(fileName);
    }
}

std::string BuildRootAccountBackupFolderPath(int instanceId) {
    return (fs::path(ResolveRuntimeRootFolder()) / "Account_backup" / ("Instance_" + std::to_string(instanceId))).string();
}

std::vector<fs::path> BuildAccountBackupMirrorFolders(int instanceId) {
    std::vector<fs::path> folders;
    const std::string instanceName = "Instance_" + std::to_string(instanceId);
    auto addUnique = [&](const fs::path& folder) {
        if (folder.empty()) return;
        for (const fs::path& existing : folders) {
            if (existing == folder) return;
        }
        folders.push_back(folder);
    };
    auto addAccountBackupUnder = [&](const fs::path& baseDir) {
        if (baseDir.empty()) return;
        addUnique(baseDir / "Account_backup" / instanceName);
    };
    auto addReleaseCandidate = [&](const fs::path& baseDir) {
        if (baseDir.empty()) return;
        const fs::path releaseDir = baseDir / "Release";
        std::error_code relEc;
        if (fs::exists(releaseDir, relEc) && fs::is_directory(releaseDir, relEc)) {
            addAccountBackupUnder(releaseDir);
        }
    };

    const fs::path exeDir = fs::path(GetExecutableDirectory());
    addAccountBackupUnder(exeDir);
    addReleaseCandidate(exeDir);
    if (exeDir.has_parent_path()) {
        addAccountBackupUnder(exeDir.parent_path());
        addReleaseCandidate(exeDir.parent_path());
        if (exeDir.parent_path().has_parent_path()) {
            addReleaseCandidate(exeDir.parent_path().parent_path());
        }
    }

    const fs::path runtimeRoot = fs::path(ResolveRuntimeRootFolder());
    addAccountBackupUnder(runtimeRoot);
    addReleaseCandidate(runtimeRoot);
    if (runtimeRoot.has_parent_path()) {
        addReleaseCandidate(runtimeRoot.parent_path());
    }

    return folders;
}

std::string MakeAccountBackupTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
    localtime_s(&localTime, &nowTime);

    std::ostringstream stamp;
    stamp << std::put_time(&localTime, "%Y%m%d_%H%M%S")
        << "_" << std::setw(3) << std::setfill('0') << millis.count();
    return stamp.str();
}

bool BackupExistingAccountFile(int instanceId, int slotIndex, const std::string& accountPath) {
    std::error_code ec;
    if (!fs::exists(accountPath, ec)) return true;

    ec.clear();
    if (fs::file_size(accountPath, ec) == 0 || ec) return true;

    const std::string historyDir = BuildAccountFolderPath(instanceId) + "\\slot_history";
    fs::create_directories(historyDir, ec);
    if (ec) {
        AddLog(instanceId, "Save aborted: could not create account backup folder.", ImVec4(1, 0, 0, 1));
        return false;
    }

    const std::string slotName = "account_" + std::to_string(slotIndex + 1);
    std::string backupFileName = slotName + "_" + MakeAccountBackupTimestamp() + "_before_save" + ahjie::kExtension;
    fs::path backupPath = fs::path(historyDir) / backupFileName;
    for (int i = 1; i <= 50; ++i) {
        ec.clear();
        if (!fs::exists(backupPath, ec)) break;
        backupFileName = slotName + "_" + MakeAccountBackupTimestamp() + "_" + std::to_string(i) + "_before_save" + ahjie::kExtension;
        backupPath = fs::path(historyDir) / backupFileName;
    }

    ec.clear();
    fs::copy_file(accountPath, backupPath, fs::copy_options::none, ec);
    if (ec) {
        AddLog(instanceId, "Save aborted: could not back up existing slot "
            + std::to_string(slotIndex + 1) + " account file.", ImVec4(1, 0, 0, 1));
        return false;
    }

    AddLog(instanceId, "Account slot " + std::to_string(slotIndex + 1)
        + " backup created before overwrite.", ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
    return true;
}

bool AutoBackupAccountSessionToRoot(int instanceId, int slotIndex, const std::string& accountPath) {
    std::error_code ec;
    if (!fs::exists(accountPath, ec)) return false;
    ec.clear();
    if (fs::file_size(accountPath, ec) == 0 || ec) return false;

    const std::string slotName = "account_" + std::to_string(slotIndex + 1);
    int syncedFolders = 0;

    for (const fs::path& instanceBackupDir : BuildAccountBackupMirrorFolders(instanceId)) {
        ec.clear();
        fs::create_directories(instanceBackupDir, ec);
        if (ec) {
            AddLog(instanceId, "Auto backup warning: could not create Account_backup folder: "
                + instanceBackupDir.string(), ImVec4(1.0f, 0.55f, 0.25f, 1.0f));
            continue;
        }

        const fs::path latestPath = instanceBackupDir / (slotName + ahjie::kExtension);
        ec.clear();
        fs::copy_file(accountPath, latestPath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            AddLog(instanceId, "Auto backup warning: failed to update backup for slot "
                + std::to_string(slotIndex + 1) + " in " + instanceBackupDir.string() + ".",
                ImVec4(1.0f, 0.55f, 0.25f, 1.0f));
            continue;
        }

        if (fs::exists(latestPath, ec)) {
            const fs::path historyDir = instanceBackupDir / "history";
            fs::create_directories(historyDir, ec);
            std::string historyFileName = slotName + "_" + MakeAccountBackupTimestamp() + "_session" + ahjie::kExtension;
            fs::path historyPath = historyDir / historyFileName;
            for (int i = 1; i <= 50; ++i) {
                ec.clear();
                if (!fs::exists(historyPath, ec)) break;
                historyFileName = slotName + "_" + MakeAccountBackupTimestamp() + "_" + std::to_string(i) + "_session" + ahjie::kExtension;
                historyPath = historyDir / historyFileName;
            }
            ec.clear();
            fs::copy_file(accountPath, historyPath, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                AddLog(instanceId, "Auto backup warning: failed to write session history copy for slot "
                    + std::to_string(slotIndex + 1) + " in " + instanceBackupDir.string() + ".",
                    ImVec4(1.0f, 0.55f, 0.25f, 1.0f));
            }
        }

        ++syncedFolders;
    }

    if (syncedFolders > 0) {
        AddLog(instanceId, "Auto backup synced slot " + std::to_string(slotIndex + 1)
            + " to " + std::to_string(syncedFolders) + " Account_backup folder(s).",
            ImVec4(0.55f, 0.9f, 0.9f, 1.0f));
        return true;
    }
    return false;
}

int CountAccountPreferenceEntries(const std::string& xml) {
    int count = 0;
    size_t pos = 0;
    while ((pos = xml.find(" name=\"", pos)) != std::string::npos) {
        ++count;
        pos += 7;
    }
    return count;
}

AccountFileValidation ValidateDecryptedAccountXml(const std::string& rawData) {
    constexpr size_t kMinAccountBytes = 3000;
    constexpr int kMinPreferenceKeys = 25;

    AccountFileValidation result;
    result.rawBytes = rawData.size();
    result.keyCount = CountAccountPreferenceEntries(rawData);

    const bool hasXmlHeader = rawData.find("<?xml") != std::string::npos;
    const bool hasMapRoot = rawData.find("<map") != std::string::npos;
    result.safeToLoad = hasXmlHeader
        && hasMapRoot
        && result.rawBytes >= kMinAccountBytes
        && result.keyCount >= kMinPreferenceKeys;

    std::ostringstream detail;
    detail << "raw_bytes=" << result.rawBytes
        << ", keys=" << result.keyCount
        << ", xml=" << (hasXmlHeader ? "yes" : "no")
        << ", map=" << (hasMapRoot ? "yes" : "no")
        << ", min_bytes=" << kMinAccountBytes
        << ", min_keys=" << kMinPreferenceKeys;
    result.detail = detail.str();
    return result;
}

bool TryDecryptLegacyAccountFile(const std::string& legacyPath, std::string& rawData, std::string& error) {
    std::ifstream inFile(legacyPath, std::ios::binary);
    if (!inFile) {
        error = "legacy slot file is not readable";
        return false;
    }

    std::string encryptedData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    rawData = DecryptXORHex(encryptedData, "NXRTH_LOCAL_ACCOUNT_KEY");
    if (rawData.find("<?xml") == std::string::npos) {
        rawData = DecryptPureXORHex(encryptedData, "NXRTH_LOCAL_ACCOUNT_KEY");
    }
    if (rawData.empty() || rawData.find("<?xml") == std::string::npos) {
        error = "legacy .nxrth decrypt failed";
        return false;
    }
    return true;
}

bool MigrateLegacyAccountSlotIfNeeded(int instanceId, int slotIndex, const std::string& ahjiePath) {
    std::error_code ec;
    if (fs::exists(ahjiePath, ec)) {
        std::ifstream inFile(ahjiePath, std::ios::binary);
        if (inFile) {
            std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
            if (ahjie::IsAhjieBytes(content)) {
                return true;
            }
        }
    }

    const std::string legacyPath = BuildLegacyAccountSlotPath(instanceId, slotIndex);
    ec.clear();
    if (!fs::exists(legacyPath, ec)) return false;

    std::string rawData;
    std::string error;
    if (!TryDecryptLegacyAccountFile(legacyPath, rawData, error)) {
        AddLog(instanceId, "Legacy account migration failed for slot "
            + std::to_string(slotIndex + 1) + ": " + error, ImVec4(1, 0.25f, 0.25f, 1));
        return false;
    }

    AccountFileValidation validation = ValidateDecryptedAccountXml(rawData);
    if (!validation.safeToLoad) {
        AddLog(instanceId, "Legacy account migration blocked for slot "
            + std::to_string(slotIndex + 1) + ": " + validation.detail, ImVec4(1, 0.25f, 0.25f, 1));
        return false;
    }

    const fs::path legacyBackupDir = fs::path(BuildAccountFolderPath(instanceId)) / "legacy_nxrth_backup";
    fs::create_directories(legacyBackupDir, ec);
    if (!ec) {
        const fs::path backupPath = legacyBackupDir / ("account_" + std::to_string(slotIndex + 1)
            + "_" + MakeAccountBackupTimestamp() + ".nxrth");
        fs::copy_file(legacyPath, backupPath, fs::copy_options::skip_existing, ec);
    }

    std::string encryptError;
    if (!ahjie::EncryptFileFromBytes(rawData, ahjiePath, ahjie::Purpose::Account, encryptError)) {
        AddLog(instanceId, "Legacy account migration failed for slot "
            + std::to_string(slotIndex + 1) + ": " + encryptError, ImVec4(1, 0.25f, 0.25f, 1));
        return false;
    }

    AddLog(instanceId, "Migrated account slot " + std::to_string(slotIndex + 1)
        + " to secure format. Legacy file was backed up.", ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
    return true;
}

std::string TrimAscii(std::string text) {
    auto notSpace = [](unsigned char c) { return c != ' ' && c != '\t' && c != '\r' && c != '\n'; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
    text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(), text.end());
    return text;
}

bool TryReadRemoteZoomSize(int instanceId, long long& outSize, std::string* outRaw = nullptr) {
    outSize = -1;

    auto parseLargestPositiveInt = [](const std::string& text, long long& parsed) {
        parsed = -1;
        std::string digits;
        auto flushDigits = [&]() {
            if (digits.empty()) return;
            try {
                const long long value = std::stoll(digits);
                if (value > parsed) parsed = value;
            }
            catch (...) {
            }
            digits.clear();
        };

        for (char ch : text) {
            if (std::isdigit(static_cast<unsigned char>(ch))) {
                digits.push_back(ch);
            }
            else {
                flushDigits();
            }
        }
        flushDigits();
        return parsed > 0;
    };

    const std::vector<std::string> probes = {
        "shell \"su -c 'wc -c < " + ZOOM_DATA_PATH + " 2>/dev/null || echo MISSING'\"",
        "shell \"su -c 'stat -c %s " + ZOOM_DATA_PATH + " 2>/dev/null || toybox stat -c %s " + ZOOM_DATA_PATH + " 2>/dev/null || echo MISSING'\"",
        "shell \"su -c 'ls -l " + ZOOM_DATA_PATH + " 2>/dev/null || echo MISSING'\""
    };

    std::string combinedProbe;
    for (const std::string& probe : probes) {
        std::string trimmed = TrimAscii(GetAdbOutput(instanceId, probe));
        if (!combinedProbe.empty()) combinedProbe += " | ";
        combinedProbe += trimmed;
        if (trimmed.empty()) continue;
        if (trimmed.find("MISSING") != std::string::npos) continue;
        if (trimmed.find("No such file") != std::string::npos) continue;
        if (trimmed.find("Permission denied") != std::string::npos) continue;

        long long parsed = -1;
        if (parseLargestPositiveInt(trimmed, parsed)) {
            outSize = parsed;
            if (outRaw) *outRaw = combinedProbe;
            return true;
        }
    }

    if (outRaw) *outRaw = combinedProbe;
    return false;
}

struct ZoomInstallResult {
    bool mkdirOk = false;
    bool pushOk = false;
    bool copyOk = false;
    bool chmodOk = false;
    bool discoveredCopyOk = false;
    bool verifyOk = false;
    long long localBytes = -1;
    long long remoteBytes = -1;
    std::string verifyProbe;
};

bool InstallZoomHackFile(int instanceId, const std::string& zoomFile, ZoomInstallResult& result) {
    std::error_code ec;
    result.localBytes = static_cast<long long>(fs::file_size(zoomFile, ec));
    if (ec || result.localBytes <= 0) {
        result.verifyProbe = "local game_config.csv missing or empty";
        return false;
    }

    const std::string dataDir = "/data/data/com.supercell.hayday/update/data/";
    const std::string tempZoom = "/sdcard/temp_zoom_config.csv";

    result.mkdirOk = RunAdbCommand(instanceId, "shell \"su -c 'mkdir -p " + dataDir + "'\"");
    result.pushOk = RunAdbCommand(instanceId, "push \"" + zoomFile + "\" " + tempZoom);
    result.copyOk = RunAdbCommand(instanceId, "shell \"su -c 'cp -f " + tempZoom + " " + ZOOM_DATA_PATH + "'\"");
    result.chmodOk = RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 " + ZOOM_DATA_PATH + "'\"");
    result.discoveredCopyOk = RunAdbCommand(instanceId,
        "shell \"su -c 'for f in $(find /data/data/com.supercell.hayday -name game_config.csv 2>/dev/null); do cp -f "
        + tempZoom + " $f; chmod 777 $f; done'\"");
    RunAdbCommand(instanceId, "shell rm -f " + tempZoom);

    result.verifyOk = TryReadRemoteZoomSize(instanceId, result.remoteBytes, &result.verifyProbe)
        && result.remoteBytes == result.localBytes;
    return result.mkdirOk && result.pushOk && result.copyOk && result.chmodOk;
}

std::string BuildZoomInstallSummary(const ZoomInstallResult& result) {
    return "mkdir=" + std::string(result.mkdirOk ? "ok" : "fail")
        + " push=" + std::string(result.pushOk ? "ok" : "fail")
        + " copy=" + std::string(result.copyOk ? "ok" : "fail")
        + " chmod=" + std::string(result.chmodOk ? "ok" : "fail")
        + " discovered_copy=" + std::string(result.discoveredCopyOk ? "ok" : "fail")
        + " verify=" + std::string(result.verifyOk ? "ok" : "fail")
        + " remote_bytes=" + std::to_string(result.remoteBytes)
        + " local_bytes=" + std::to_string(result.localBytes)
        + " probe=\"" + result.verifyProbe + "\"";
}
}

void InjectImportantFiles(int instanceId) {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    std::string exeDir = std::string(buffer).substr(0, pos);

    std::string fontFile = exeDir + "\\injecthacks\\languages.csv";
    std::string zoomFile = exeDir + "\\injecthacks\\game_config.csv";
    std::string minitouchFile = exeDir + "\\injecthacks\\minitouch";

    struct RequiredVisualPayload {
        std::string file;
        bool required;
    };
    std::vector<RequiredVisualPayload> visualPayloads = {
        {"inject.nxrth", true},
        {"inject2.nxrth", true},
        {"inject3.nxrth", true},
        {"inject4.nxrth", true},
        {"inject5.nxrth", false}
    };

    for (const auto& payload : visualPayloads) {
        if (payload.required && !fs::exists(exeDir + "\\injecthacks\\" + payload.file)) {
            AddLog(instanceId, "Error: Missing encrypted " + payload.file + " in injecthacks folder!", ImVec4(1, 0, 0, 1));
            return;
        }
    }
    if (!fs::exists(fontFile) || !fs::exists(zoomFile) || !fs::exists(minitouchFile)) {
        AddLog(instanceId, Tr("Error: Basic hack files (zoom/font/minitouch) missing!"), ImVec4(1, 0, 0, 1));
        return;
    }

    AddLog(instanceId, Tr("Starting Project Wheat injection (This will take 10-15 seconds)..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

    std::thread([instanceId, exeDir, fontFile, zoomFile, minitouchFile]() {
        auto decryptAhjieToFile = [&](const std::string& nxPath, const std::string& tempPath) {
            std::string rawData;
            std::string decryptError;
            if (!ahjie::DecryptFileToBytes(nxPath, ahjie::Purpose::Asset, rawData, decryptError)) {
                AddLog(instanceId, "Error: Failed to decrypt " + nxPath + " (" + decryptError + ")", ImVec4(1, 0, 0, 1));
                return false;
            }

            std::ofstream outFile(tempPath, std::ios::binary);
            outFile.write(rawData.data(), rawData.size());
            outFile.close();
            return fs::exists(tempPath) && fs::file_size(tempPath) > 0;
        };

        auto installExtraTemplate = [&](const std::string& sourceName, const std::string& targetName) {
            std::string sourcePath = exeDir + "\\injecthacks\\extra_visuals\\templates\\" + sourceName;
            if (!fs::exists(sourcePath)) return false;

            std::string templateDir = exeDir + "\\templates";
            std::string backupDir = templateDir + "\\pre_extra_visuals_backup";
            std::string targetPath = templateDir + "\\" + targetName;
            std::string backupPath = backupDir + "\\" + targetName;

            fs::create_directories(templateDir);
            if (fs::exists(targetPath) && !fs::exists(backupPath)) {
                fs::create_directories(backupDir);
                fs::copy_file(targetPath, backupPath, fs::copy_options::overwrite_existing);
            }
            fs::copy_file(sourcePath, targetPath, fs::copy_options::overwrite_existing);
            return true;
        };

        int extraTemplateCount = 0;
        try {
            if (installExtraTemplate("cow_pasture.png", "cow_pasture.png")) extraTemplateCount++;
            if (installExtraTemplate("feed_mill.png", "feed_mill.png")) extraTemplateCount++;
        }
        catch (...) {
            AddLog(instanceId, "Warning: Could not install extra tagged templates.", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
        }
        if (extraTemplateCount > 0) {
            AddLog(instanceId, "Extra visuals: installed tagged CP/FM templates.", ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
        }

		// 1. FORCE CLOSE HAY DAY TO AVOID FILE LOCKS
        AddLog(instanceId, Tr("Force stopping Hay Day..."), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // WAIT FOR EMULATOR JUST IN CASE

        // 2. CREATE TARGET FOLDERS
        std::string dataDir = "/data/data/com.supercell.hayday/update/data/";
        std::string scDir = "/data/data/com.supercell.hayday/update/sc/";
        RunAdbCommand(instanceId, "shell \"su -c 'mkdir -p " + dataDir + "'\"");
        RunAdbCommand(instanceId, "shell \"su -c 'mkdir -p " + scDir + "'\"");
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        // 3. FONT & LANGUAGE HACK
        AddLog(instanceId, Tr("1/4: Injecting Font & Language Hack..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        std::string tempFont = "/sdcard/temp_languages.csv";
        RunAdbCommand(instanceId, "push \"" + fontFile + "\" " + tempFont);
        RunAdbCommand(instanceId, "shell \"su -c 'cp " + tempFont + " " + dataDir + "languages.csv'\"");
        RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 " + dataDir + "languages.csv'\"");
        RunAdbCommand(instanceId, "shell rm " + tempFont);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 

        // 4. ZOOM HACK (game_config.csv)
        AddLog(instanceId, Tr("2/4: Optimizing View Distance (Zoom Hack)..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        ZoomInstallResult zoomInstall;
        if (!InstallZoomHackFile(instanceId, zoomFile, zoomInstall)) {
            AddLog(instanceId, "Zoom hack injection failed. " + BuildZoomInstallSummary(zoomInstall), ImVec4(1, 0.25f, 0.25f, 1));
            return;
        }
        if (zoomInstall.verifyOk) {
            AddLog(instanceId, "Zoom hack injected and verified. " + BuildZoomInstallSummary(zoomInstall), ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
        }
        else {
            AddLog(instanceId, "Zoom hack copied; verification could not confirm byte count. " + BuildZoomInstallSummary(zoomInstall), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // 5. NATURE HACK (DECRYPTION ON THE FLY)
        AddLog(instanceId, Tr("3/4: Decrypting & Injecting Secure Visuals (Nature Hack)..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));

        struct NatureVisualPayload {
            std::string encryptedFile;
            std::string targetFile;
            bool required;
        };
        std::vector<NatureVisualPayload> natureMap = {
            {"inject.nxrth", "nature_new.sc", true},
            {"inject2.nxrth", "nature_new_0.sctx", true},
            {"inject3.nxrth", "nature_new_1.sctx", true},
            {"inject4.nxrth", "nature_new_2.sctx", true},
            {"inject5.nxrth", "nature_new_3.sctx", false}
        };

        for (const auto& payload : natureMap) {
            std::string nxPath = exeDir + "\\injecthacks\\" + payload.encryptedFile;
            std::string tempPath = exeDir + "\\injecthacks\\temp_" + payload.targetFile;

            if (!fs::exists(nxPath)) {
                if (payload.required) {
                    AddLog(instanceId, "Error: Missing required visual payload " + payload.encryptedFile, ImVec4(1, 0, 0, 1));
                    return;
                }
                AddLog(instanceId, "Skipping optional visual payload not present in installed assets: " + payload.targetFile, ImVec4(1.0f, 0.65f, 0.2f, 1.0f));
                continue;
            }

            if (!decryptAhjieToFile(nxPath, tempPath)) {
                AddLog(instanceId, "Error: Could not prepare " + payload.targetFile, ImVec4(1, 0, 0, 1));
                return;
            }

            // 4. PUSH FILES
            bool pushOk = RunAdbCommand(instanceId, "push \"" + tempPath + "\" /data/local/tmp/" + payload.targetFile);

            // 5. DELETE TEMP FILES
            fs::remove(tempPath);

            if (!pushOk) {
                AddLog(instanceId,
                    "Error: Failed to push visual payload " + payload.targetFile
                    + ". Check the ADB connection and reinject.",
                    ImVec4(1, 0, 0, 1));
                return;
            }

            // FILES TOO BIG SO WE KINDA WAIT JUST IN CASE.
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }

        // USE EMULATOR'S TEMP FOLDER
        AddLog(instanceId, "Applying decrypted assets to game engine...", ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        bool natureCopyOk = RunAdbCommand(instanceId, "shell su -c 'cp /data/local/tmp/nature_new* " + scDir + "'");
        bool natureChmodOk = RunAdbCommand(instanceId, "shell su -c 'chmod 777 " + scDir + "nature_new*'");

        // DELETE STUFF IN THAT TEMP FOLDER
        RunAdbCommand(instanceId, "shell rm /data/local/tmp/nature_new*");
        if (!natureCopyOk || !natureChmodOk) {
            AddLog(instanceId,
                "Error: Failed to apply core visual payloads to Hay Day. copy="
                + std::string(natureCopyOk ? "ok" : "fail")
                + " chmod=" + std::string(natureChmodOk ? "ok" : "fail")
                + ". Verify emulator root and ADB, then reinject.",
                ImVec4(1, 0, 0, 1));
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        std::vector<std::pair<std::string, std::string>> extraVisualMap = {
            {"inject_extra_trees_0.nxrth", "nature_new_0.sctx"},
            {"inject_extra_trees_1.nxrth", "nature_new_1.sctx"},
            {"inject_extra_trees.nxrth", "nature_new_2.sctx"},
            {"inject_extra_animals_0.nxrth", "animals_0.sctx"},
            {"inject_extra_animals_1.nxrth", "animals_1.sctx"},
            {"inject_extra_animals_2.nxrth", "animals_2.sctx"},
            {"inject_extra_animals_3.nxrth", "animals_3.sctx"},
            {"inject_extra_animals_4.nxrth", "animals_4.sctx"},
            {"inject_extra_animals_5.nxrth", "animals_5.sctx"},
            {"inject_extra_animals_6.nxrth", "animals_6.sctx"},
            {"inject_extra_animals_7.nxrth", "animals_7.sctx"},
            {"inject_extra_animals_8.nxrth", "animals_8.sctx"},
            {"inject_extra_animals_9.nxrth", "animals_9.sctx"},
            {"inject_extra_animals_10.nxrth", "animals_10.sctx"},
            {"inject_extra_animals_11.nxrth", "animals_11.sctx"},
            {"inject_extra_animals_12.nxrth", "animals_12.sctx"},
            {"inject_extra_animals_13.nxrth", "animals_13.sctx"},
            {"inject_extra_animals_14.nxrth", "animals_14.sctx"},
            {"inject_extra_animals_15.nxrth", "animals_15.sctx"},
            {"inject_extra_animals_16.nxrth", "animals_16.sctx"},
            {"inject_extra_animals02_0.nxrth", "animals02_0.sctx"},
            {"inject_extra_animals02_1.nxrth", "animals02_1.sctx"},
            {"inject_extra_animals02_2.nxrth", "animals02_2.sctx"},
            {"inject_extra_animals02_3.nxrth", "animals02_3.sctx"},
            {"inject_extra_animals02_4.nxrth", "animals02_4.sctx"},
            {"inject_extra_animals03_0.nxrth", "animals03_0.sctx"},
            {"inject_extra_animals03_1.nxrth", "animals03_1.sctx"},
            {"inject_extra_animals03_2.nxrth", "animals03_2.sctx"},
            {"inject_extra_animals03_3.nxrth", "animals03_3.sctx"},
            {"inject_extra_animals03_4.nxrth", "animals03_4.sctx"},
            {"inject_extra_animals03_5.nxrth", "animals03_5.sctx"},
            {"inject_extra_animals03_6.nxrth", "animals03_6.sctx"},
            {"inject_extra_animals04_0.nxrth", "animals04_0.sctx"},
            {"inject_extra_animals04_1.nxrth", "animals04_1.sctx"},
            {"inject_extra_animals04_2.nxrth", "animals04_2.sctx"},
            {"inject_extra_animals04_3.nxrth", "animals04_3.sctx"},
            {"inject_extra_animals04_4.nxrth", "animals04_4.sctx"},
            {"inject_extra_animals04_5.nxrth", "animals04_5.sctx"},
            {"inject_extra_animals04_6.nxrth", "animals04_6.sctx"},
            {"inject_extra_animals04_7.nxrth", "animals04_7.sctx"},
            {"inject_extra_animals04_8.nxrth", "animals04_8.sctx"},
            {"inject_extra_animals04_9.nxrth", "animals04_9.sctx"},
            {"inject_extra_animals05_0.nxrth", "animals05_0.sctx"},
            {"inject_extra_animals05_1.nxrth", "animals05_1.sctx"},
            {"inject_extra_animals05_2.nxrth", "animals05_2.sctx"},
            {"inject_extra_animals05_3.nxrth", "animals05_3.sctx"},
            {"inject_extra_animals05_4.nxrth", "animals05_4.sctx"},
            {"inject_extra_animals05_5.nxrth", "animals05_5.sctx"},
            {"inject_extra_animals05_6.nxrth", "animals05_6.sctx"},
            {"inject_extra_animals05_7.nxrth", "animals05_7.sctx"},
            {"inject_extra_animals05_8.nxrth", "animals05_8.sctx"},
            {"inject_extra_animals05_9.nxrth", "animals05_9.sctx"},
            {"inject_extra_animal_accessories_0.nxrth", "animal_accessories_0.sctx"},
            {"inject_extra_animal_accessories_1.nxrth", "animal_accessories_1.sctx"},
            {"inject_extra_buildings_new_0.nxrth", "buildings_new_0.sctx"},
            {"inject_extra_buildings_new_6.nxrth", "buildings_new_6.sctx"},
            {"inject_extra_buildings_new_2.nxrth", "buildings_new_2.sctx"},
            {"inject_extra_decos01_generic_5.nxrth", "decos01_generic_5.sctx"},
            {"inject_extra_decos01_generic_6.nxrth", "decos01_generic_6.sctx"},
            {"inject_extra_decos01_generic_8.nxrth", "decos01_generic_8.sctx"},
            {"inject_extra_decos01_generic_10.nxrth", "decos01_generic_10.sctx"},
            {"inject_extra_decos03_features_0.nxrth", "decos03_features_0.sctx"},
            {"inject_extra_decos03_features_2.nxrth", "decos03_features_2.sctx"},
            {"inject_extra_decos03_features_4.nxrth", "decos03_features_4.sctx"},
            {"inject_extra_decos03_features_8.nxrth", "decos03_features_8.sctx"},
            {"inject_extra_map_0.nxrth", "map_0.sctx"},
            {"inject_extra_map_1.nxrth", "map_1.sctx"},
            {"inject_extra_map_2.nxrth", "map_2.sctx"},
            {"inject_extra_map_3.nxrth", "map_3.sctx"}
        };
        AppendAutoDiscoveredExtraVisualPayloads(extraVisualMap, fs::path(exeDir) / "injecthacks");

        for (const auto& pair : extraVisualMap) {
            std::string extraPayload = exeDir + "\\injecthacks\\" + pair.first;
            if (!fs::exists(extraPayload)) continue;

            AddLog(instanceId, "Extra visuals: applying " + pair.second + "...", ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
            std::string tempExtra = exeDir + "\\injecthacks\\temp_extra_" + pair.second;
            if (decryptAhjieToFile(extraPayload, tempExtra)) {
                RunAdbCommand(instanceId, "push \"" + tempExtra + "\" /data/local/tmp/" + pair.second);
                RunAdbCommand(instanceId, "shell \"su -c 'cp /data/local/tmp/" + pair.second + " " + scDir + pair.second + "'\"");
                RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 " + scDir + pair.second + "'\"");
                RunAdbCommand(instanceId, "shell rm /data/local/tmp/" + pair.second);
                fs::remove(tempExtra);
            }
            else {
                AddLog(instanceId, "Warning: Extra visual payload could not be prepared: " + pair.first, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
            }
        }

        // 6. MINITOUCH PUSH
        AddLog(instanceId, Tr("4/4: Installing Minitouch Input Agent..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        RunAdbCommand(instanceId, "push \"" + minitouchFile + "\" /data/local/tmp/minitouch");
        RunAdbCommand(instanceId, "shell chmod 777 /data/local/tmp/minitouch");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        StartMinitouchStealth(instanceId);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        AddLog(instanceId, Tr("ALL HACKS INJECTED SUCCESSFULLY! Start the game and Set game Language To English."), ImVec4(0, 1, 0, 1));
        }).detach();
}
// ACCOUNT SAVER
void SaveAccountToSlot(int instanceId, int slotIndex) {
    AddLog(instanceId, Tr("Saving & Encrypting Account Data..."), ImVec4(1, 1, 0, 1));

    // DOÄžRUDAN SENÄ°N EXTERN FONKSÄ°YONUNU KULLANIYORUZ
    std::string folderPath = BuildAccountFolderPath(instanceId);
    if (!fs::exists(folderPath)) fs::create_directories(folderPath);

    std::string pcFileName = BuildAccountSlotPath(instanceId, slotIndex);
    std::string tempRawFile = folderPath + "\\temp_raw.xml";
    std::string tempSdFile = "/sdcard/temp_backup_" + std::to_string(instanceId) + ".xml";

    std::string copyCmd = "shell \"su -c 'cat " + GAME_DATA_PATH + " > " + tempSdFile + "'\"";
    RunAdbCommand(instanceId, copyCmd);
    std::string pullCmd = "pull " + tempSdFile + " \"" + tempRawFile + "\"";
    RunAdbCommand(instanceId, pullCmd);
    RunAdbCommand(instanceId, "shell rm " + tempSdFile);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (fs::exists(tempRawFile) && fs::file_size(tempRawFile) > 0) {
        std::ifstream inFile(tempRawFile, std::ios::binary);
        std::string rawData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        AccountFileValidation validation = ValidateDecryptedAccountXml(rawData);
        if (!validation.safeToLoad) {
            fs::remove(tempRawFile);
            AddLog(instanceId, "Save aborted: pulled account data for slot "
                + std::to_string(slotIndex + 1) + " looks incomplete. " + validation.detail,
                ImVec4(1, 0.25f, 0.25f, 1));
            return;
        }

        if (!BackupExistingAccountFile(instanceId, slotIndex, pcFileName)) {
            fs::remove(tempRawFile);
            return;
        }

        std::string tempEncryptedFile = pcFileName + ".tmp";
        std::string encryptError;
        if (!ahjie::EncryptFileFromBytes(rawData, tempEncryptedFile, ahjie::Purpose::Account, encryptError)) {
            fs::remove(tempRawFile);
            AddLog(instanceId, "Save failed: could not encrypt account slot "
                + std::to_string(slotIndex + 1) + " with .Ahjie. " + encryptError, ImVec4(1, 0, 0, 1));
            return;
        }

        std::error_code ec;
        fs::copy_file(tempEncryptedFile, pcFileName, fs::copy_options::overwrite_existing, ec);
        fs::remove(tempEncryptedFile);
        if (ec) {
            fs::remove(tempRawFile);
            AddLog(instanceId, "Save failed: could not write encrypted account slot "
                + std::to_string(slotIndex + 1) + " after backup.", ImVec4(1, 0, 0, 1));
            return;
        }

        fs::remove(tempRawFile);

        g_Bots[instanceId].accounts[slotIndex].hasFile = true;
        g_Bots[instanceId].accounts[slotIndex].fileName = pcFileName;
        AutoBackupAccountSessionToRoot(instanceId, slotIndex, pcFileName);
        AddLog(instanceId, Tr("Account Saved & Encrypted Successfully."), ImVec4(0, 1, 0, 1));
    }
    else {
        AddLog(instanceId, Tr("Save Failed. Check Settings."), ImVec4(1, 0, 0, 1));
    }
}

// =========================================================
// DECRYPT FUNCTION FOR ACCOUNTS N OTHER STUFF USED.
// =========================================================
std::string DecryptPureXORHex(const std::string& hexStr, const std::string& key) {
    std::string text;
    if (hexStr.length() % 2 != 0) return "";
    for (size_t i = 0; i < hexStr.length(); i += 2) {
        std::string byteString = hexStr.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), NULL, 16);
        text += (byte ^ key[(i / 2) % key.length()]);
    }
    return text;
}

// =========================================================
// ZOOM HACK VERIFICATION
// Checks if game_config.csv exists on the device. If missing
// (game update, data wipe, or first run), re-pushes it.
// Called once at bot startup and after Janitor emulator reboots.
// =========================================================
void EnsureZoomHackPresent(int instanceId) {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    std::string exeDir = std::string(buffer).substr(0, pos);
    std::string zoomFile = exeDir + "\\injecthacks\\game_config.csv";

    if (!fs::exists(zoomFile)) {
        AddLog(instanceId, "Zoom check skipped: game_config.csv not found in injecthacks folder.", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
        return;
    }

    long long remoteZoomSize = -1;
    std::string remoteProbe;
    bool zoomExists = TryReadRemoteZoomSize(instanceId, remoteZoomSize, &remoteProbe);
    long long localZoomSize = -1;
    std::error_code zoomEc;
    localZoomSize = static_cast<long long>(fs::file_size(zoomFile, zoomEc));
    if (zoomEc) localZoomSize = -1;

    if (zoomExists && remoteZoomSize == localZoomSize) {
        AddLog(instanceId, "Zoom hack verified: game_config.csv matches local injecthacks copy. remote_bytes="
            + std::to_string(remoteZoomSize) + " local_bytes=" + std::to_string(localZoomSize), ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
        return;
    }

    // Missing â€” re-inject
    AddLog(instanceId,
        "Zoom hack missing or stale on device. Re-injecting game_config.csv... remote_bytes="
        + std::to_string(remoteZoomSize) + " local_bytes=" + std::to_string(localZoomSize)
        + " probe=\"" + remoteProbe + "\"",
        ImVec4(1.0f, 0.5f, 0.0f, 1.0f));

    RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    ZoomInstallResult zoomInstall;
    if (!InstallZoomHackFile(instanceId, zoomFile, zoomInstall)) {
        AddLog(instanceId, "Zoom hack re-injection failed. " + BuildZoomInstallSummary(zoomInstall), ImVec4(1, 0.25f, 0.25f, 1));
        return;
    }

    if (zoomInstall.verifyOk) {
        AddLog(instanceId, "Zoom hack re-injected and verified. " + BuildZoomInstallSummary(zoomInstall), ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
    }
    else {
        AddLog(instanceId, "Zoom hack re-injected; verification could not confirm byte count. " + BuildZoomInstallSummary(zoomInstall), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    }
}

// =========================================================
// ACCOUNT LOADER
// =========================================================
void LoadAccountFromSlot(int instanceId, int slotIndex) {
    // DOÄžRUDAN SENÄ°N EXTERN FONKSÄ°YONUNU KULLANIYORUZ
    std::string folderPath = BuildAccountFolderPath(instanceId);
    std::string pcFileName = BuildAccountSlotPath(instanceId, slotIndex);

    if (!fs::exists(pcFileName) && !MigrateLegacyAccountSlotIfNeeded(instanceId, slotIndex, pcFileName)) {
        AddLog(instanceId, Tr("Error: Slot file empty or missing!"), ImVec4(1, 0, 0, 1));
        return;
    }
    AddLog(instanceId, Tr("Decrypting & Switching Account..."), ImVec4(1, 1, 0, 1));
    if (std::string(g_Bots[instanceId].adbSerial).empty()) {
        AddLog(instanceId, "Account load aborted: this instance has no ADB serial configured.", ImVec4(1, 0.35f, 0.35f, 1));
        return;
    }

    std::string rawData;
    std::string decryptError;
    if (!ahjie::DecryptFileToBytes(pcFileName, ahjie::Purpose::Account, rawData, decryptError)
        || rawData.find("<?xml") == std::string::npos) {
        AddLog(instanceId, std::string(Tr("Error: File decryption failed! Corrupted data."))
            + " " + decryptError, ImVec4(1, 0, 0, 1));
        return;
    }

    AccountFileValidation validation = ValidateDecryptedAccountXml(rawData);
    if (!validation.safeToLoad) {
        AddLog(instanceId, "Account load blocked: slot " + std::to_string(slotIndex + 1)
            + " file looks incomplete or unsafe. " + validation.detail
            + ". Re-save this slot from a valid in-game account before using it.",
            ImVec4(1, 0.25f, 0.25f, 1));
        return;
    }

    std::string tempRawFile = folderPath + "\\temp_decrypted.xml";
    std::ofstream outFile(tempRawFile, std::ios::binary);
    outFile << rawData;
    outFile.close();

    bool forceStopOk = RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
    std::string tempSdFile = "/sdcard/temp_restore_" + std::to_string(instanceId) + ".xml";

    std::string pushCmd = "push \"" + tempRawFile + "\" " + tempSdFile;
    bool pushOk = RunAdbCommand(instanceId, pushCmd);

    // RENAME THE ACTUAL FILE TO STORAGE_NEW.XML
    std::string moveCmd = "shell \"su -c 'cat " + tempSdFile + " > /data/data/com.supercell.hayday/shared_prefs/storage_new.xml'\"";
    bool moveOk = RunAdbCommand(instanceId, moveCmd);
    bool chmodOk = RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 /data/data/com.supercell.hayday/shared_prefs/storage_new.xml'\"");
    RunAdbCommand(instanceId, "shell rm " + tempSdFile);

    fs::remove(tempRawFile);

    if (!forceStopOk || !pushOk || !moveOk || !chmodOk) {
        AddLog(instanceId, "Account load failed before launch. Check that the emulator is online, the ADB serial is correct, and root file access still works.", ImVec4(1, 0.25f, 0.25f, 1));
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    bool launchOk = RunAdbCommand(instanceId, "shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1");
    if (!launchOk) {
        AddLog(instanceId, "Account data was restored, but Hay Day did not relaunch. Check that ADB still sees the instance.", ImVec4(1, 0.35f, 0.35f, 1));
        return;
    }
    AddLog(instanceId, Tr("Account Switched. Game Restarting."), ImVec4(0, 1, 0, 1));
}
// ==============================================================================
// 
// 2-FINGER SWIPE 
// 
// ==============================================================================

// FUNCTION NAME TELLS IT ALL, SCANS STUFF WITH COLOR. BECAUSE BOT CHANGING COLORS OF THE STUFF IN THE GAME
void SendRotationReport(int instanceId) {
    if (!g_EnableBarnWebhook) return;
    BotInstance& bot = g_Bots[instanceId];

    std::string msg = "ðŸ”„ **" + std::string(Tr("ROTATION COMPLETE | INSTANCE ")) + std::to_string(instanceId + 1) + "** ðŸ”„\n";
    msg += "--------------------------------------\n";

    InventoryData totalCurrent;
    InventoryData totalPrev;

    for (int i = 0; i < MAX_ACCOUNT_SLOTS; i++) {
        if (bot.accounts[i].hasFile) {
            totalCurrent.bolt += bot.accounts[i].currentInv.bolt;
            totalPrev.bolt += bot.accounts[i].previousInv.bolt;
            totalCurrent.tape += bot.accounts[i].currentInv.tape;
            totalPrev.tape += bot.accounts[i].previousInv.tape;
            totalCurrent.plank += bot.accounts[i].currentInv.plank;
            totalPrev.plank += bot.accounts[i].previousInv.plank;
            totalCurrent.nail += bot.accounts[i].currentInv.nail;
            totalPrev.nail += bot.accounts[i].previousInv.nail;
            totalCurrent.screw += bot.accounts[i].currentInv.screw;
            totalPrev.screw += bot.accounts[i].previousInv.screw;
            totalCurrent.panel += bot.accounts[i].currentInv.panel;
            totalPrev.panel += bot.accounts[i].previousInv.panel;
            totalCurrent.deed += bot.accounts[i].currentInv.deed;
            totalPrev.deed += bot.accounts[i].previousInv.deed;
            totalCurrent.mallet += bot.accounts[i].currentInv.mallet;
            totalPrev.mallet += bot.accounts[i].previousInv.mallet;
            totalCurrent.marker += bot.accounts[i].currentInv.marker;
            totalPrev.marker += bot.accounts[i].previousInv.marker;
            totalCurrent.map += bot.accounts[i].currentInv.map;
            totalPrev.map += bot.accounts[i].previousInv.map;
            totalCurrent.dynamite += bot.accounts[i].currentInv.dynamite;
            totalPrev.dynamite += bot.accounts[i].previousInv.dynamite;
            totalCurrent.tnt += bot.accounts[i].currentInv.tnt;
            totalPrev.tnt += bot.accounts[i].previousInv.tnt;
            totalCurrent.axe += bot.accounts[i].currentInv.axe;
            totalPrev.axe += bot.accounts[i].previousInv.axe;
            totalCurrent.saw += bot.accounts[i].currentInv.saw;
            totalPrev.saw += bot.accounts[i].previousInv.saw;
            totalCurrent.shovel += bot.accounts[i].currentInv.shovel;
            totalPrev.shovel += bot.accounts[i].previousInv.shovel;

            bot.accounts[i].previousInv = bot.accounts[i].currentInv;
        }
    }

    auto formatDelta = [](int current, int prev) -> std::string {
        int diff = current - prev;
        if (diff > 0) return " (+**" + std::to_string(diff) + "**)";
        if (diff < 0) return " (" + std::to_string(diff) + ")";
        return " (+0)";
        };

    msg += "ðŸ’° **" + std::string(Tr("BARN INVENTORY TOTALS")) + "**\n";
    msg += "<:Bolt:304466477527072768> Bolt: **" + std::to_string(totalCurrent.bolt) + "**" + formatDelta(totalCurrent.bolt, totalPrev.bolt) + " | ";
    msg += "<:Plank:304466477409370113> Plank: **" + std::to_string(totalCurrent.plank) + "**" + formatDelta(totalCurrent.plank, totalPrev.plank) + " | ";
    msg += "<:DuctTape:304466477564690432> Tape: **" + std::to_string(totalCurrent.tape) + "**" + formatDelta(totalCurrent.tape, totalPrev.tape) + "\n";
    msg += "<:Nail:304466477489324042> Nail: **" + std::to_string(totalCurrent.nail) + "**" + formatDelta(totalCurrent.nail, totalPrev.nail) + " | ";
    msg += "<:Screw:304466477438992394> Screw: **" + std::to_string(totalCurrent.screw) + "**" + formatDelta(totalCurrent.screw, totalPrev.screw) + " | ";
    msg += "<:WoodPanel:304466476977356802> Wood Panel: **" + std::to_string(totalCurrent.panel) + "**" + formatDelta(totalCurrent.panel, totalPrev.panel) + "\n";
    msg += "Deed: **" + std::to_string(totalCurrent.deed) + "**" + formatDelta(totalCurrent.deed, totalPrev.deed) + " | ";
    msg += "Mallet: **" + std::to_string(totalCurrent.mallet) + "**" + formatDelta(totalCurrent.mallet, totalPrev.mallet) + " | ";
    msg += "Marker: **" + std::to_string(totalCurrent.marker) + "**" + formatDelta(totalCurrent.marker, totalPrev.marker) + " | ";
    msg += "Map: **" + std::to_string(totalCurrent.map) + "**" + formatDelta(totalCurrent.map, totalPrev.map) + "\n";
    msg += "Dynamite: **" + std::to_string(totalCurrent.dynamite) + "**" + formatDelta(totalCurrent.dynamite, totalPrev.dynamite) + " | ";
    msg += "TNT: **" + std::to_string(totalCurrent.tnt) + "**" + formatDelta(totalCurrent.tnt, totalPrev.tnt) + " | ";
    msg += "Axe: **" + std::to_string(totalCurrent.axe) + "**" + formatDelta(totalCurrent.axe, totalPrev.axe) + " | ";
    msg += "Saw: **" + std::to_string(totalCurrent.saw) + "**" + formatDelta(totalCurrent.saw, totalPrev.saw) + " | ";
    msg += "Shovel: **" + std::to_string(totalCurrent.shovel) + "**" + formatDelta(totalCurrent.shovel, totalPrev.shovel) + "\n";

    std::thread([=]() { Discord::SendWebhookMessage(msg); }).detach();
    AddLog(instanceId, Tr("Rotation Delta Report Sent!"), ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
}
