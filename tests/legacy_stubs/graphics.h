#pragma once
// Test-only no-op graphics/input adapter. This is NOT an EasyX implementation.
#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
using std::min;
using TCHAR = char;
#define _T(value) value
template <std::size_t N, typename... Args>
int _stprintf_s(char (&buffer)[N], const char* format, Args... args) {
    return std::snprintf(buffer, N, format, args...);
}
inline constexpr int PS_SOLID = 0;
inline constexpr int PS_ENDCAP_FLAT = 0;
inline constexpr int EM_MOUSE = 1;
inline constexpr int WM_LBUTTONDOWN = 0x201;
inline constexpr int WM_LBUTTONUP = 0x202;
inline constexpr int WM_RBUTTONDOWN = 0x204;
inline constexpr int WM_RBUTTONUP = 0x205;
inline constexpr int VK_LCONTROL = 0xA2;
struct ExMessage { int message = 0; int x = 0; int y = 0; };
namespace legacy_io {
inline std::array<bool, 256> keys{};
inline std::deque<ExMessage> messages;
}
inline int GetAsyncKeyState(int key) { return legacy_io::keys.at(key) ? 0x8000 : 0; }
inline bool peekmessage(ExMessage* message, int) {
    if (legacy_io::messages.empty()) return false;
    *message = legacy_io::messages.front();
    legacy_io::messages.pop_front();
    return true;
}
inline int RGB(int r, int g, int b) { return r | (g << 8) | (b << 16); }
inline void setfillcolor(int) {}
inline void solidrectangle(int, int, int, int) {}
inline void setlinecolor(int) {}
inline void setlinestyle(int, int) {}
inline void line(int, int, int, int) {}
inline void outtextxy(int, int, const char*) {}
inline void BeginBatchDraw() {}
inline void FlushBatchDraw() {}
inline void cleardevice() {}
inline void Sleep(int) {}
inline void initgraph(int, int) {}
