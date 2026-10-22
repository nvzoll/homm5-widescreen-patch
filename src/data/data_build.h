#pragma once

#include "../games.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

// Rebuilds the golden layout pak, keeping the recipe's entry date (dev check).
bool BuildDataPak(GameId game, const std::filesystem::path& root, std::span<const uint8_t> recipe,
                  const std::filesystem::path& out, std::wstring& error);

struct GamePakOptions {
    bool fonts = false;
    bool skipIntro = false;
};

// The pak the patch installs for one game: layout recipe plus the optional parts.
bool BuildGamePak(GameId game, const std::filesystem::path& root, std::span<const uint8_t> recipe, const GamePakOptions& options,
                  const std::filesystem::path& out, std::wstring& error);
