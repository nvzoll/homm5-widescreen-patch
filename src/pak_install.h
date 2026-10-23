#pragma once

#include "games.h"

#include <filesystem>
#include <string>

std::filesystem::path GamePakPath(const Game& game);
bool ReadFontsOption(const Game& game);
// Builds the game's pak from the user's own data (plus the intro files in video\); on failure returns false and sets `error`.
bool InstallGamePak(const Game& game, bool fonts, bool skipIntro, bool keepSharedIntro, std::wstring& error);
bool GamePakComplete(const Game& game, bool skipIntro);
bool GamePakOutdated(const Game& game);
bool UninstallGamePak(const Game& game, bool keepSharedIntro, std::wstring& error);
