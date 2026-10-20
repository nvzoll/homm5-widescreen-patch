#pragma once

#include "games.h"

#include <filesystem>
#include <string>

std::filesystem::path GamePakPath(const Game& game);
bool ReadFontsOption(const Game& game);
// Builds the game's pak from the user's own data (plus the intro files in video\); on failure returns false and sets `error`.
bool InstallGamePak(const Game& game, bool fonts, bool skipIntro, std::wstring& error);
bool UninstallGamePak(const Game& game, std::wstring& error);
