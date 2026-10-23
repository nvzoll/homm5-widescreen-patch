#pragma once

#include "games.h"

#include <filesystem>
#include <string>

enum class HookState { NotInstalled, Installed, Outdated, Foreign };

std::filesystem::path HookDllPath(const Game& game);
std::filesystem::path PackagedHookDll();
HookState QueryHook(const Game& game);
bool ReadSkipFade(const Game& game);
// On failure returns false and sets `error`.
bool WriteHookOptions(const Game& game, bool skipFade, std::wstring& error);
bool UninstallHook(const Game& game, std::wstring& error);
