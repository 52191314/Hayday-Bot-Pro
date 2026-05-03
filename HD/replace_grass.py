import io

with io.open(r'c:\Users\XY\Desktop\XCoder-master\HD\BotEngine.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

old_str = """    // Last resort: tap the right-center of the screen (grass area)
    // On 640x480 this is (590, 240). Scale proportionally for other resolutions.
    int safeX = screen.empty() ? 590 : static_cast<int>(screen.cols * 0.922f); // ~590/640
    int safeY = screen.empty() ? 240 : static_cast<int>(screen.rows * 0.500f); // ~240/480
    AddLog(instanceId, std::string("RECOVERY: no cross found during ") + reason + ". Tapping safe grass area (" + std::to_string(safeX) + "," + std::to_string(safeY) + ") as last resort.", ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    AdbTap(instanceId, safeX, safeY);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    return true;"""

new_str = """    AddLog(instanceId, std::string("RECOVERY: no cross found during ") + reason + ". Not touching grass per user request. Overlay dismissal failed.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    return false;"""

text = text.replace(old_str, new_str)
with io.open(r'c:\Users\XY\Desktop\XCoder-master\HD\BotEngine.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
