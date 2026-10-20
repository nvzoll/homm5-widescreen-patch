#pragma once

#include "../games.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

bool BuildDataPak(GameId game, const std::filesystem::path& root, std::span<const uint8_t> recipe,
                  const std::filesystem::path& out, std::wstring& error);
