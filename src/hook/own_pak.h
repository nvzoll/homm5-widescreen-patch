#pragma once

#include "image.h"

#include <array>
#include <cstdint>
#include <string>

// The exe's one call to its CWinVFS factory, which mounts data\*.pak and the hard-coded pak table.
struct VfsFactorySite {
    uintptr_t call;
    std::array<uint8_t, 5> original;
    uintptr_t factory;
    bool factoryHasStackArg;
    uintptr_t mountPattern;  // __thiscall (CWinVFS*, const GameString* pattern, int priority)
    const char* folder;      // our pak's folder next to bin\ / bina1\.
};

// Mounts <game root>\<folder>\homm5-widescreen.pak right after the table, above every shipped pak.
// False when the pak is missing or cannot be mounted.
bool InstallOwnPak(const Image& image, const VfsFactorySite& site, std::string& log);
