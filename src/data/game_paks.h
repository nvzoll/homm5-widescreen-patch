#pragma once

#include "../games.h"
#include "pak.h"

#include <filesystem>
#include <string>

// Mounts the user's paks the way the game's exe does, minus the pak this utility writes.
bool MountGamePaks(GameId game, const std::filesystem::path& root, PakArchives& archives, std::wstring& error);
