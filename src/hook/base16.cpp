#include "hooks.h"
#include "own_pak.h"
#include "virtual_screen.h"

#include <array>
#include <cstdint>

namespace {

constexpr uint32_t kTimestamp = 0x6847E45E;
constexpr uint32_t kSizeOfImage = 0xF83000;

constexpr uintptr_t kSetVirtualScreenHook = 0xE3811F;
constexpr std::array<uint8_t, 34> kSetVirtualScreenOriginal = {
    0x66, 0x0F, 0x6E, 0xD1,              // movd xmm2, ecx
    0x8B, 0x0D, 0xF0, 0x2C, 0x0D, 0x01,  // mov ecx, [design_h]
    0xA1, 0xEC, 0x2C, 0x0D, 0x01,        // mov eax, [design_w]
    0x0F, 0x5B, 0xD2,                    // cvtdq2ps xmm2, xmm2
    0x89, 0x0D, 0xE8, 0x2C, 0x0D, 0x01,  // mov [virtual_h], ecx
    0xB9, 0x5C, 0x53, 0x13, 0x01,        // mov ecx, log_stream
    0xA3, 0xE4, 0x2C, 0x0D, 0x01,        // mov [virtual_w], eax
};
constexpr std::array<size_t, 5> kSetVirtualScreenAbs = {6, 11, 20, 25, 30};
constexpr uintptr_t kVirtualW = 0x10D2CE4, kVirtualH = 0x10D2CE8;
constexpr uintptr_t kDesignW = 0x10D2CEC, kDesignH = 0x10D2CF0;
constexpr uintptr_t kLogStream = 0x113535C;

constexpr std::array<uintptr_t, 2> kFadeCalls = {0x5E24E3, 0x5E2BAD};
constexpr std::array<std::array<uint8_t, 5>, 2> kFadeCallsOriginal = {{
    {0xE8, 0x18, 0x15, 0x00, 0x00},
    {0xE8, 0x9E, 0x0E, 0x00, 0x00},
}};

constexpr VfsFactorySite kVfsFactory = {0x6C510C, {0xE8, 0x1F, 0x1A, 0x34, 0x00}, 0xA06B30, true, 0xA07680, "datals"};

float* g_virtualW;
float* g_virtualH;
const float* g_designW;
const float* g_designH;
uintptr_t g_logStream;

void __cdecl SetVirtualScreen(int width, int height) {
    const VirtualScreen screen = ToeVirtualScreen(width, height, *g_designW, *g_designH);
    *g_virtualW = screen.width;
    *g_virtualH = screen.height;
}

// In: ecx = width, edx = height. The setter continues with edx, xmm2 = (float)width and
// ecx = log stream; eax, xmm0 and xmm1 are dead there.
__declspec(naked) void SetVirtualScreenStub() {
    __asm {
        push ecx
        push edx
        push edx
        push ecx
        call SetVirtualScreen
        add esp, 8
        pop edx
        pop ecx
        cvtsi2ss xmm2, ecx
        mov ecx, g_logStream
        ret
    }
}

}  // namespace

bool InstallBase16(const Image& image, const Options& options, std::string& log) {
    if (!image.IsBuild(kTimestamp, kSizeOfImage)) return false;
    log += "Heroes V 1.6 (";
    log += image.store();
    log += ")\r\n";

    if (!InstallOwnPak(image, kVfsFactory, log)) {
        log += "  widescreen: skipped, needs the pak\r\n";
    } else if (image.Matches(kSetVirtualScreenHook, kSetVirtualScreenOriginal, kSetVirtualScreenAbs)) {
        g_virtualW = image.At<float>(kVirtualW);
        g_virtualH = image.At<float>(kVirtualH);
        g_designW = image.At<const float>(kDesignW);
        g_designH = image.At<const float>(kDesignH);
        g_logStream = reinterpret_cast<uintptr_t>(image.At<void>(kLogStream));
        image.WriteCall(kSetVirtualScreenHook, reinterpret_cast<const void*>(&SetVirtualScreenStub), kSetVirtualScreenOriginal.size());
        log += "  widescreen: installed\r\n";
    } else {
        log += "  widescreen: unexpected bytes, skipped\r\n";
    }

    if (options.skipFade) InstallSkipFade(image, kFadeCalls, kFadeCallsOriginal, log);
    return true;
}
