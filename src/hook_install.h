#pragma once

#include "games.h"

#include <string>

enum class HookState { NotInstalled, Installed, Foreign };

HookState QueryHook(const Game& game);
bool ReadSkipFade(const Game& game);
// On failure returns false and sets `error`.
bool InstallHook(const Game& game, bool skipFade, std::wstring& error);
bool UninstallHook(const Game& game, std::wstring& error);
