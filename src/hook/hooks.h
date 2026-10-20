#pragma once

#include "image.h"

#include <array>
#include <cstdint>
#include <string>

struct Options {
    bool skipFade = false;
};

// Each returns false when the running exe is a different build.
bool InstallBase16(const Image& image, const Options& options, std::string& log);
bool InstallHof21(const Image& image, const Options& options, std::string& log);
bool InstallToe31(const Image& image, const Options& options, std::string& log);

inline void InstallSkipFade(const Image& image, const std::array<uintptr_t, 2>& calls,
                            const std::array<std::array<uint8_t, 5>, 2>& original, std::string& log) {
    if (!image.Matches(calls[0], original[0]) || !image.Matches(calls[1], original[1])) {
        log += "  skip_fade: unexpected bytes, skipped\r\n";
        return;
    }
    for (const uintptr_t call : calls) image.Nop(call, 5);
    log += "  skip_fade: installed\r\n";
}
