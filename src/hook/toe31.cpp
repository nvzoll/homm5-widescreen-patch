#include "hooks.h"

#include <array>
#include <cstdint>

namespace {

constexpr uint32_t TIMESTAMP = 0x6841A475;
constexpr uint32_t SIZE_OF_IMAGE = 0xF05000;

constexpr std::array<uintptr_t, 2> FADE_CALLS = {0x4DBA01, 0x4DC09F};
constexpr std::array<std::array<uint8_t, 5>, 2> FADE_CALLS_ORIGINAL = {{
    {0xE8, 0x6A, 0x14, 0x00, 0x00},
    {0xE8, 0x1C, 0x0E, 0x00, 0x00},
}};

}  // namespace

bool InstallToe31(const Image& image, const Options& options, std::string& log) {
    if (!image.IsBuild(TIMESTAMP, SIZE_OF_IMAGE)) {
        return false;
    }

    log += "Heroes V: Tribes of the East 3.1 (";
    log += image.store();
    log += ")\r\n";

    if (options.skipFade) {
        InstallSkipFade(image, FADE_CALLS, FADE_CALLS_ORIGINAL, log);
    }

    return true;
}
