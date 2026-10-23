#include "hooks.h"
#include "own_pak.h"
#include "virtual_screen.h"

#include <windows.h>

#include <array>
#include <cstdint>

namespace {

#include "hof21_edits.inc"

constexpr uint32_t TIMESTAMP = 0x6847E484;
constexpr uint32_t SIZE_OF_IMAGE = 0xF62000;

constexpr uintptr_t PRESENT_PARAMS_WINDOWED = 0x1134CF8;
constexpr uintptr_t MAIN_HWND = 0x1135010;
constexpr DWORD WINDOWED_STYLE = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
constexpr DWORD FULLSCREEN_STYLE = WS_POPUP | WS_VISIBLE | WS_SYSMENU;

constexpr uintptr_t SET_WINDOW_POS_CALL = 0x629041;
constexpr std::array<uint8_t, 6> SET_WINDOW_POS_CALL_ORIGINAL = {0xFF, 0x15, 0x00, 0x53, 0xF8, 0x00};  // call [SetWindowPos]

constexpr std::array<uintptr_t, 2> CURSOR_GET_WINDOW_RECT_CALLS = {0x7279F3, 0x727ADD};
constexpr std::array<uint8_t, 6> GET_WINDOW_RECT_CALL_ORIGINAL = {0xFF, 0x15, 0xFC, 0x52, 0xF8, 0x00};  // call [GetWindowRect]
constexpr std::array<size_t, 1> INDIRECT_CALL_ABS = {2};

constexpr uintptr_t SCREEN_SIZE_SETTER = 0xE500D0;
constexpr std::array<uint8_t, 37> SCREEN_SIZE_SETTER_ORIGINAL = {
    0x66, 0x0F, 0x6E, 0x44, 0x24, 0x04,        // movd xmm0, [esp+4]
    0x0F, 0x5B, 0xC0,                          // cvtdq2ps xmm0, xmm0
    0xF3, 0x0F, 0x11, 0x05, 0x2C, 0x4E, 0x0D, 0x01,  // movss [screen_w], xmm0
    0x66, 0x0F, 0x6E, 0x44, 0x24, 0x08,        // movd xmm0, [esp+8]
    0x0F, 0x5B, 0xC0,                          // cvtdq2ps xmm0, xmm0
    0xF3, 0x0F, 0x11, 0x05, 0x30, 0x4E, 0x0D, 0x01,  // movss [screen_h], xmm0
    0xC2, 0x08, 0x00,                          // ret 8
};
constexpr std::array<size_t, 2> SCREEN_SIZE_SETTER_ABS = {13, 30};
constexpr uintptr_t SCREEN_W = 0x10D4E2C, SCREEN_H = 0x10D4E30;
constexpr uintptr_t VIRTUAL_W = 0x10D4E24, VIRTUAL_H = 0x10D4E28;

constexpr uintptr_t ROOT_RECT_SITE = 0xE7F6B2;
constexpr std::array<uint8_t, 32> ROOT_RECT_ORIGINAL = {
    0xC7, 0x44, 0x24, 0x0C, 0x00, 0x00, 0x00, 0x00,  // left = 0
    0xC7, 0x44, 0x24, 0x14, 0x00, 0x00, 0x80, 0x44,  // right = 1024
    0xC7, 0x44, 0x24, 0x10, 0x00, 0x00, 0x00, 0x00,  // top = 0
    0xC7, 0x44, 0x24, 0x18, 0x00, 0x00, 0x40, 0x44,  // bottom = 768
};
constexpr size_t SCREEN_DB_OFFSET = 0xC8;
constexpr size_t VER_ALIGN_OFFSET = 0x94, HOR_ALIGN_OFFSET = 0x9C;
enum Align { ALIGN_HIGH_END = 2, ALIGN_SCREEN = 3 };

constexpr std::array<uintptr_t, 2> FADE_CALLS = {0x60C598, 0x60CB9E};
constexpr std::array<std::array<uint8_t, 5>, 2> FADE_CALLS_ORIGINAL = {{
    {0xE8, 0xE3, 0x11, 0x00, 0x00},
    {0xE8, 0x2D, 0x0C, 0x00, 0x00},
}};

constexpr VfsFactorySite VFS_FACTORY = {0x6ED274, {0xE8, 0x47, 0x06, 0x30, 0x00}, 0x9ED8C0, false, 0x9EE3D0, "dataa1"};

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
        SetWindowLongA(main, GWL_STYLE, WINDOWED_STYLE);
        SetWindowPos(main, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
        RECT window{}, client{};
        GetWindowRect(main, &window);
        GetClientRect(main, &client);
        cx += (window.right - window.left) - client.right;
        cy += (window.bottom - window.top) - client.bottom;
    } else {
        SetWindowLongA(main, GWL_STYLE, static_cast<LONG>(FULLSCREEN_STYLE));
    }

    return SetWindowPos(hwnd, insertAfter, x, y, cx, cy, flags);
}

// NUI::CCursor maps the mouse through GetWindowRect, which is the client area only without a frame.
BOOL WINAPI ClientRectOnScreen(HWND hwnd, RECT* rect) {
    if (!GetClientRect(hwnd, rect)) {
        return FALSE;
    }

    POINT origin{};
    ClientToScreen(hwnd, &origin);
    OffsetRect(rect, origin.x, origin.y);
    return TRUE;
}

void __stdcall SetScreenSize(int width, int height) {
    *g_screenW = static_cast<float>(width);
    *g_screenH = static_cast<float>(height);

    if (height == 0) {
        return;
    }

    const VirtualScreen screen = ToeVirtualScreen(width, height, 1024.0f, 768.0f);
    *g_virtualW = screen.width;
    *g_virtualH = screen.height;
}

void PlaceAxis(float virtualSize, float designSize, int align, float& low, float& high) {
    if (align == ALIGN_SCREEN) {
        low = 0.0f;
        high = virtualSize;
        return;
    }

    low = virtualSize - designSize;

    if (align != ALIGN_HIGH_END) {
        low *= 0.5f;
    }

    high = low + designSize;
}

// HoF hard-codes every root screen to (0, 0, 1024, 768). The screen's own Placement alignment
// (unused for roots in vanilla) stands in for ToE's HorAlign/VerAlign: LOW_END and CENTER centre,
// HIGH_END goes right/bottom, MARGIN covers the whole virtual screen.
void __cdecl PlaceRootRect(const uint8_t* window, float* rect) {
    const uint8_t* db = *reinterpret_cast<const uint8_t* const*>(window + SCREEN_DB_OFFSET);
    const int horAlign = db ? *reinterpret_cast<const int*>(db + HOR_ALIGN_OFFSET) : 0;
    const int verAlign = db ? *reinterpret_cast<const int*>(db + VER_ALIGN_OFFSET) : 0;
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
    if (!image.IsBuild(TIMESTAMP, SIZE_OF_IMAGE)) {
        return false;
    }

    log += "Heroes V: Hammers of Fate 2.1 (";
    log += image.store();
    log += ")\r\n";

    g_windowed = image.At<const BOOL>(PRESENT_PARAMS_WINDOWED);
    g_mainHwnd = image.At<const HWND>(MAIN_HWND);
    g_screenW = image.At<float>(SCREEN_W);
    g_screenH = image.At<float>(SCREEN_H);
    g_virtualW = image.At<float>(VIRTUAL_W);
    g_virtualH = image.At<float>(VIRTUAL_H);

    const bool frame = image.Matches(SET_WINDOW_POS_CALL, SET_WINDOW_POS_CALL_ORIGINAL, INDIRECT_CALL_ABS);
    if (frame) {
        image.WriteCall(SET_WINDOW_POS_CALL, reinterpret_cast<const void*>(&FramedSetWindowPos), SET_WINDOW_POS_CALL_ORIGINAL.size());
    }

    Report(log, "window frame", frame);

    const bool cursor = frame && image.Matches(CURSOR_GET_WINDOW_RECT_CALLS[0], GET_WINDOW_RECT_CALL_ORIGINAL, INDIRECT_CALL_ABS) &&
                        image.Matches(CURSOR_GET_WINDOW_RECT_CALLS[1], GET_WINDOW_RECT_CALL_ORIGINAL, INDIRECT_CALL_ABS);
    if (cursor) {
        for (const uintptr_t call : CURSOR_GET_WINDOW_RECT_CALLS) {
            image.WriteCall(call, reinterpret_cast<const void*>(&ClientRectOnScreen), GET_WINDOW_RECT_CALL_ORIGINAL.size());
        }
    }

    Report(log, "cursor offset", cursor);

    const bool pak = InstallOwnPak(image, VFS_FACTORY, log);
    const bool widescreen = pak && image.Matches(SCREEN_SIZE_SETTER, SCREEN_SIZE_SETTER_ORIGINAL, SCREEN_SIZE_SETTER_ABS) &&
                            image.MatchesAll(WIDESCREEN_EDITS) && image.Matches(ROOT_RECT_SITE, ROOT_RECT_ORIGINAL) &&
                            image.MatchesAll(MINIMAP_EDITS);
    if (widescreen) {
        image.WriteJump(SCREEN_SIZE_SETTER, reinterpret_cast<const void*>(&SetScreenSize));
        image.ApplyAll(WIDESCREEN_EDITS);
        image.WriteCall(ROOT_RECT_SITE, reinterpret_cast<const void*>(&PlaceRootRectStub), ROOT_RECT_ORIGINAL.size());
        image.ApplyAll(MINIMAP_EDITS);
    }

    if (pak) {
        Report(log, "widescreen", widescreen);
    } else {
        log += "  widescreen: skipped, needs the pak\r\n";
    }

    if (options.skipFade) {
        InstallSkipFade(image, FADE_CALLS, FADE_CALLS_ORIGINAL, log);
    }

    return true;
}
