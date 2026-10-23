#include "hooks.h"

#include <array>
#include <cstdint>

namespace {

constexpr uint32_t kTimestamp = 0x4EA07667;
constexpr uint32_t kSizeOfImage = 0xDE9000;

constexpr std::array<uintptr_t, 2> kFadeCalls = {0x4014D1, 0x401968};
constexpr std::array<std::array<uint8_t, 5>, 2> kFadeCallsOriginal = {{
    {0xE8, 0x6A, 0x13, 0x00, 0x00},
    {0xE8, 0x33, 0x0C, 0x00, 0x00},
}};

}  // namespace

bool InstallMmh55(const Image& image, const Options& options, std::string& log) {
    if (!image.IsBuild(kTimestamp, kSizeOfImage)) {
        return false;
    }

    log += "Heroes 5.5 (MMH55.exe, Tribes of the East 3.1 2011)\r\n";

    if (options.skipFade) {
        InstallSkipFade(image, kFadeCalls, kFadeCallsOriginal, log);
    }

    return true;
}
