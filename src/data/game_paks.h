#pragma once

#include "../games.h"
#include "pak.h"

#include <filesystem>
#include <string>
#include <vector>

struct GamePak {
    std::filesystem::path path;
    int priority;
};

// The paks the game's exe mounts, in mount order, minus the pak this utility writes; empty if data\ has none.
std::vector<GamePak> GamePakFiles(GameId game, const std::filesystem::path& root);

bool MountGamePaks(GameId game, const std::filesystem::path& root, PakArchives& archives, std::wstring& error);
