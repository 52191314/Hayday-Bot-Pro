#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "cpp/src/infra/MinitouchClient.h"

#pragma comment(lib, "ws2_32.lib")

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

int MinitouchPort(int instanceId) {
    return 1111 + instanceId;
}

namespace {
// Initialize Winsock exactly once for the whole process.
void EnsureWinsockInitialized() {
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    });
}
} // namespace

MinitouchConnection::MinitouchConnection(int instanceId) {
    EnsureWinsockInitialized();

    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock == INVALID_SOCKET) return;

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<u_short>(MinitouchPort(instanceId)));
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    m_connected = (connect(m_sock, reinterpret_cast<struct sockaddr*>(&server), sizeof(server)) == 0);
}

MinitouchConnection::~MinitouchConnection() {
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    m_connected = false;
}

bool MinitouchConnection::IsConnected() const {
    return m_connected;
}

void MinitouchConnection::Down(int slot, int x, int y, int pressure) {
    m_buf += "d " + std::to_string(slot) + " " +
             std::to_string(x) + " " + std::to_string(y) + " " +
             std::to_string(pressure) + "\n";
}

void MinitouchConnection::Move(int slot, int x, int y, int pressure) {
    m_buf += "m " + std::to_string(slot) + " " +
             std::to_string(x) + " " + std::to_string(y) + " " +
             std::to_string(pressure) + "\n";
}

void MinitouchConnection::Up(int slot) {
    m_buf += "u " + std::to_string(slot) + "\n";
}

bool MinitouchConnection::SendBuffer() {
    if (!m_connected || m_sock == INVALID_SOCKET) return false;
    if (m_buf.empty()) return true;
    int sent = send(m_sock, m_buf.c_str(), static_cast<int>(m_buf.size()), 0);
    m_buf.clear();
    return sent != SOCKET_ERROR;
}

bool MinitouchConnection::Commit() {
    m_buf += "c\n";
    return SendBuffer();
}

namespace Minitouch {

bool DragPath(int instanceId, const std::vector<POINT>& path,
              int holdMs, int stepDelayMs, int settleMs) {
    if (path.size() < 2) return false;
    MinitouchConnection conn(instanceId);
    if (!conn.IsConnected()) return false;

    conn.Down(0, path.front().x, path.front().y);
    conn.Commit();
    Sleep(holdMs);

    for (size_t seg = 1; seg < path.size(); ++seg) {
        POINT from = path[seg - 1];
        POINT to   = path[seg];
        float dist = std::hypot(static_cast<float>(to.x - from.x), static_cast<float>(to.y - from.y));
        int steps = (std::max)(12, static_cast<int>(dist / 6.0f));
        for (int i = 1; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            int cx = from.x + static_cast<int>((to.x - from.x) * t);
            int cy = from.y + static_cast<int>((to.y - from.y) * t);
            conn.Move(0, cx, cy);
            conn.Commit();
            Sleep(stepDelayMs);
        }
    }

    conn.Up(0);
    conn.Commit();
    Sleep(settleMs);
    return true;
}

bool TwoFingerPinch(int instanceId,
                    POINT aStart, POINT aEnd,
                    POINT bStart, POINT bEnd,
                    int holdMs, int stepDelayMs, int settleMs) {
    MinitouchConnection conn(instanceId);
    if (!conn.IsConnected()) return false;

    conn.Down(0, aStart.x, aStart.y);
    conn.Down(1, bStart.x, bStart.y);
    conn.Commit();
    Sleep(holdMs);

    float distA = std::hypot(static_cast<float>(aEnd.x - aStart.x), static_cast<float>(aEnd.y - aStart.y));
    float distB = std::hypot(static_cast<float>(bEnd.x - bStart.x), static_cast<float>(bEnd.y - bStart.y));
    int steps = (std::max)(14, static_cast<int>((std::max)(distA, distB) / 6.0f));

    for (int i = 1; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        int ax = aStart.x + static_cast<int>((aEnd.x - aStart.x) * t);
        int ay = aStart.y + static_cast<int>((aEnd.y - aStart.y) * t);
        int bx = bStart.x + static_cast<int>((bEnd.x - bStart.x) * t);
        int by = bStart.y + static_cast<int>((bEnd.y - bStart.y) * t);
        conn.Move(0, ax, ay);
        conn.Move(1, bx, by);
        conn.Commit();
        Sleep(stepDelayMs);
    }

    conn.Up(0);
    conn.Up(1);
    conn.Commit();
    Sleep(settleMs);
    return true;
}

bool TwoFingerSweep(int instanceId,
                    POINT aStart, POINT bStart,
                    const std::vector<POINT>& aWaypoints,
                    const std::vector<POINT>& bWaypoints,
                    int holdMs, int stepDelayMs, int settleMs) {
    if (aWaypoints.empty() || aWaypoints.size() != bWaypoints.size()) return false;

    MinitouchConnection conn(instanceId);
    if (!conn.IsConnected()) return false;

    conn.Down(0, aStart.x, aStart.y);
    conn.Down(1, bStart.x, bStart.y);
    conn.Commit();
    Sleep(holdMs);

    POINT aPrev = aStart;
    POINT bPrev = bStart;
    for (size_t seg = 0; seg < aWaypoints.size(); ++seg) {
        POINT aTo = aWaypoints[seg];
        POINT bTo = bWaypoints[seg];
        float distA = std::hypot(static_cast<float>(aTo.x - aPrev.x), static_cast<float>(aTo.y - aPrev.y));
        float distB = std::hypot(static_cast<float>(bTo.x - bPrev.x), static_cast<float>(bTo.y - bPrev.y));
        int steps = (std::max)(20, static_cast<int>((std::max)(distA, distB) / 5.0f));

        for (int i = 1; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            int ax = aPrev.x + static_cast<int>((aTo.x - aPrev.x) * t);
            int ay = aPrev.y + static_cast<int>((aTo.y - aPrev.y) * t);
            int bx = bPrev.x + static_cast<int>((bTo.x - bPrev.x) * t);
            int by = bPrev.y + static_cast<int>((bTo.y - bPrev.y) * t);
            conn.Move(0, ax, ay);
            conn.Move(1, bx, by);
            conn.Commit();
            Sleep(stepDelayMs);
        }
        aPrev = aTo;
        bPrev = bTo;
    }

    conn.Up(0);
    conn.Commit();
    Sleep(20);
    conn.Up(1);
    conn.Commit();
    Sleep(settleMs);
    return true;
}
} // namespace Minitouch
