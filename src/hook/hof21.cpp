#include "hooks.h"
#include "own_pak.h"
#include "virtual_screen.h"

#include <windows.h>

#include <array>
#include <cstdint>

namespace {

#include "hof21_edits.inc"

constexpr uint32_t kTimestamp = 0x6847E484;
constexpr uint32_t kSizeOfImage = 0xF62000;

constexpr uintptr_t kPresentParamsWindowed = 0x1134CF8;
constexpr uintptr_t kMainHwnd = 0x1135010;
constexpr DWORD kWindowedStyle = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
constexpr DWORD kFullscreenStyle = WS_POPUP | WS_VISIBLE | WS_SYSMENU;

constexpr uintptr_t kSetWindowPosCall = 0x629041;
constexpr std::array<uint8_t, 6> kSetWindowPosCallOriginal = {0xFF, 0x15, 0x00, 0x53, 0xF8, 0x00};  // call [SetWindowPos]

constexpr std::array<uintptr_t, 2> kCursorGetWindowRectCalls = {0x7279F3, 0x727ADD};
constexpr std::array<uint8_t, 6> kGetWindowRectCallOriginal = {0xFF, 0x15, 0xFC, 0x52, 0xF8, 0x00};  // call [GetWindowRect]
constexpr std::array<size_t, 1> kIndirectCallAbs = {2};

constexpr uintptr_t kScreenSizeSetter = 0xE500D0;
constexpr std::array<uint8_t, 37> kScreenSizeSetterOriginal = {
    0x66, 0x0F, 0x6E, 0x44, 0x24, 0x04,        // movd xmm0, [esp+4]
    0x0F, 0x5B, 0xC0,                          // cvtdq2ps xmm0, xmm0
    0xF3, 0x0F, 0x11, 0x05, 0x2C, 0x4E, 0x0D, 0x01,  // movss [screen_w], xmm0
    0x66, 0x0F, 0x6E, 0x44, 0x24, 0x08,        // movd xmm0, [esp+8]
    0x0F, 0x5B, 0xC0,                          // cvtdq2ps xmm0, xmm0
    0xF3, 0x0F, 0x11, 0x05, 0x30, 0x4E, 0x0D, 0x01,  // movss [screen_h], xmm0
    0xC2, 0x08, 0x00,                          // ret 8
};
constexpr std::array<size_t, 2> kScreenSizeSetterAbs = {13, 30};
constexpr uintptr_t kScreenW = 0x10D4E2C, kScreenH = 0x10D4E30;
constexpr uintptr_t kVirtualW = 0x10D4E24, kVirtualH = 0x10D4E28;

constexpr uintptr_t kRootRectSite = 0xE7F6B2;
constexpr std::array<uint8_t, 32> kRootRectOriginal = {
    0xC7, 0x44, 0x24, 0x0C, 0x00, 0x00, 0x00, 0x00,  // left = 0
    0xC7, 0x44, 0x24, 0x14, 0x00, 0x00, 0x80, 0x44,  // right = 1024
    0xC7, 0x44, 0x24, 0x10, 0x00, 0x00, 0x00, 0x00,  // top = 0
    0xC7, 0x44, 0x24, 0x18, 0x00, 0x00, 0x40, 0x44,  // bottom = 768
};
constexpr size_t kScreenDbOffset = 0xC8;
constexpr size_t kVerAlignOffset = 0x94, kHorAlignOffset = 0x9C;
enum Align { kAlignHighEnd = 2, kAlignScreen = 3 };

constexpr std::array<uintptr_t, 2> kFadeCalls = {0x60C598, 0x60CB9E};
constexpr std::array<std::array<uint8_t, 5>, 2> kFadeCallsOriginal = {{
    {0xE8, 0xE3, 0x11, 0x00, 0x00},
    {0xE8, 0x2D, 0x0C, 0x00, 0x00},
}};

constexpr VfsFactorySite kVfsFactory = {0x6ED274, {0xE8, 0x47, 0x06, 0x30, 0x00}, 0x9ED8C0, false, 0x9EE3D0, "dataa1"};

const BOOL* g_windowed;
const HWND* g_mainHwnd;
float* g_screenW;
float* g_screenH;
float* g_virtualW;
float* g_virtualH;

// HoF creates a frameless popup even in windowed mode; give it a caption and grow it so the
// client area keeps the backbuffer size.
BOOL WINAPI FramedSetWindowPos(HWND hwnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags) {
    const HWND main = *g_mainHwnd;
    if (*g_windowed) {
        SetWindowLongA(main, GWL_STYLE, kWindowedStyle);
        SetWindowPos(main, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
        RECT window, client;
        GetWindowRect(main, &window);
        GetClientRect(main, &client);
        cx += (window.right - window.left) - client.right;
        cy += (window.bottom - window.top) - client.bottom;
    } else {
        SetWindowLongA(main, GWL_STYLE, static_cast<LONG>(kFullscreenStyle));
    }
    return SetWindowPos(hwnd, insertAfter, x, y, cx, cy, flags);
}

// NUI::CCursor maps the mouse through GetWindowRect, which is the client area only without a frame.
BOOL WINAPI ClientRectOnScreen(HWND hwnd, RECT* rect) {
    if (!GetClientRect(hwnd, rect)) return FALSE;
    POINT origin{};
    ClientToScreen(hwnd, &origin);
    OffsetRect(rect, origin.x, origin.y);
    return TRUE;
}

void __stdcall SetScreenSize(int width, int height) {
    *g_screenW = static_cast<float>(width);
    *g_screenH = static_cast<float>(height);
    if (height == 0) return;
    const VirtualScreen screen = ToeVirtualScreen(width, height, 1024.0f, 768.0f);
    *g_virtualW = screen.width;
    *g_virtualH = screen.height;
}

void PlaceAxis(float virtualSize, float designSize, int align, float& low, float& high) {
    if (align == kAlignScreen) {
        low = 0.0f;
        high = virtualSize;
        return;
    }
    low = virtualSize - designSize;
    if (align != kAlignHighEnd) low *= 0.5f;
    high = low + designSize;
}

// HoF hard-codes every root screen to (0, 0, 1024, 768). The screen's own Placement alignment
// (unused for roots in vanilla) stands in for ToE's HorAlign/VerAlign: LOW_END and CENTER centre,
// HIGH_END goes right/bottom, MARGIN covers the whole virtual screen.
void __cdecl PlaceRootRect(const uint8_t* window, float* rect) {
    const uint8_t* db = *reinterpret_cast<const uint8_t* const*>(window + kScreenDbOffset);
    const int horAlign = db ? *reinterpret_cast<const int*>(db + kHorAlignOffset) : 0;
    const int verAlign = db ? *reinterpret_cast<const int*>(db + kVerAlignOffset) : 0;
    PlaceAxis(*g_virtualW, 1024.0f, horAlign, rect[0], rect[2]);
    PlaceAxis(*g_virtualH, 768.0f, verAlign, rect[1], rect[3]);
}

// In: edi = window, rect {left, top, right, bottom} at the site's esp+0xc. The next instruction
// is a virtual call through eax with ecx = this.
__declspec(naked) void PlaceRootRectStub() {
    __asm {
        push eax
        push ecx
        lea eax, [esp + 0x18]
        push eax
        push edi
        call PlaceRootRect
        add esp, 8
        pop ecx
        pop eax
        ret
    }
}

void Report(std::string& log, const char* feature, bool installed) {
    log += "  ";
    log += feature;
    log += installed ? ": installed\r\n" : ": unexpected bytes, skipped\r\n";
}

}  // namespace

bool InstallHof21(const Image& image, const Options& options, std::string& log) {
    if (!image.IsBuild(kTimestamp, kSizeOfImage)) return false;
    log += "Heroes V: Hammers of Fate 2.1 (";
    log += image.store();
    log += ")\r\n";

    g_windowed = image.At<const BOOL>(kPresentParamsWindowed);
    g_mainHwnd = image.At<const HWND>(kMainHwnd);
    g_screenW = image.At<float>(kScreenW);
    g_screenH = image.At<float>(kScreenH);
    g_virtualW = image.At<float>(kVirtualW);
    g_virtualH = image.At<float>(kVirtualH);

    const bool frame = image.Matches(kSetWindowPosCall, kSetWindowPosCallOriginal, kIndirectCallAbs);
    if (frame) image.WriteCall(kSetWindowPosCall, reinterpret_cast<const void*>(&FramedSetWindowPos), kSetWindowPosCallOriginal.size());
    Report(log, "window frame", frame);

    const bool cursor = frame && image.Matches(kCursorGetWindowRectCalls[0], kGetWindowRectCallOriginal, kIndirectCallAbs) &&
                        image.Matches(kCursorGetWindowRectCalls[1], kGetWindowRectCallOriginal, kIndirectCallAbs);
    if (cursor) {
        for (const uintptr_t call : kCursorGetWindowRectCalls) {
            image.WriteCall(call, reinterpret_cast<const void*>(&ClientRectOnScreen), kGetWindowRectCallOriginal.size());
        }
    }
    Report(log, "cursor offset", cursor);

    const bool pak = InstallOwnPak(image, kVfsFactory, log);
    const bool widescreen = pak && image.Matches(kScreenSizeSetter, kScreenSizeSetterOriginal, kScreenSizeSetterAbs) &&
                            image.MatchesAll(kWidescreenEdits) && image.Matches(kRootRectSite, kRootRectOriginal) &&
                            image.MatchesAll(kMinimapEdits);
    if (widescreen) {
        image.WriteJump(kScreenSizeSetter, reinterpret_cast<const void*>(&SetScreenSize));
        image.ApplyAll(kWidescreenEdits);
        image.WriteCall(kRootRectSite, reinterpret_cast<const void*>(&PlaceRootRectStub), kRootRectOriginal.size());
        image.ApplyAll(kMinimapEdits);
    }
    if (pak) {
        Report(log, "widescreen", widescreen);
    } else {
        log += "  widescreen: skipped, needs the pak\r\n";
    }

    if (options.skipFade) InstallSkipFade(image, kFadeCalls, kFadeCallsOriginal, log);
    return true;
}
