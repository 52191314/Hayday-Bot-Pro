#pragma once

#include <string>
#include <vector>
#include <winsock2.h>
#include <windows.h>

// Single source of truth for the minitouch port formula.
int MinitouchPort(int instanceId);  // returns 1111 + instanceId

// RAII connection to the minitouch socket. One per gesture (matches current behavior).
// Buffers protocol commands and flushes on Commit() — produces identical wire bytes
// to the current "build one string, send() once" pattern.
class MinitouchConnection {
public:
    explicit MinitouchConnection(int instanceId);
    ~MinitouchConnection();  // closesocket

    bool IsConnected() const;

    // Protocol primitives — append to internal buffer (no send yet).
    void Down(int slot, int x, int y, int pressure = 50);
    void Move(int slot, int x, int y, int pressure = 50);
    void Up(int slot);

    // Flush buffer + append "c\n", send all in one send() call. Returns send result.
    bool Commit();

private:
    SOCKET       m_sock = INVALID_SOCKET;
    bool         m_connected = false;
    std::string  m_buf;
    bool SendBuffer();
};

// === High-level gestures (each preserves its original timing/step formula) ===

namespace Minitouch {
// 1 finger, multi-point path. Replaces Animals.cpp::ExecuteSingleFingerDragPath.
bool DragPath(int instanceId, const std::vector<POINT>& path,
              int holdMs = 260, int stepDelayMs = 18, int settleMs = 220);

// 2 fingers, 1 segment (pinch/zoom). Replaces ScreenRecovery.cpp::ExecuteTwoFingerPinchGesture.
bool TwoFingerPinch(int instanceId,
                    POINT aStart, POINT aEnd,
                    POINT bStart, POINT bEnd,
                    int holdMs = 220, int stepDelayMs = 18, int settleMs = 260);

// 2 fingers, multi-segment V-sweep. Replaces Farming.cpp::ExecuteDenseGridGesture.
// Original returned void; this returns bool (callers ignored the result).
bool TwoFingerSweep(int instanceId,
                    POINT aStart, POINT bStart,
                    const std::vector<POINT>& aWaypoints,
                    const std::vector<POINT>& bWaypoints,
                    int holdMs = 350, int stepDelayMs = 20, int settleMs = 100);
} // namespace Minitouch
