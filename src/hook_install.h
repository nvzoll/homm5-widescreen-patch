#pragma once

#include "games.h"

#include <filesystem>
#include <string>

enum class HookState { NotInstalled, Installed, Foreign };

std::filesystem::path HookDllPath(const Game& game);
// The dinput8.dll shipped next to the patcher; the user copies it into the game.
std::filesystem::path PackagedHookDll();
HookState QueryHook(const Game& game);
bool ReadSkipFade(const Game& game);
// Needs our dinput8.dll already in place. On failure returns false and sets `error`.
bool WriteHookOptions(const Game& game, bool skipFade, std::wstring& error);
bool UninstallHook(const Game& game, std::wstring& error);
