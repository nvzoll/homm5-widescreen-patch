#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

enum class GameId { Base, HammersOfFate, TribesOfTheEast };

struct KnownBuild {
    GameId game;
    std::wstring_view store;
    std::wstring_view label;
    std::string_view sha256;
};

struct Game {
    GameId id;
    std::wstring store;
    std::wstring storeVersion;
    std::wstring language;
    std::filesystem::path root;
    std::filesystem::path exe;
    std::string exeSha256;
    const KnownBuild* build = nullptr;
};

std::wstring_view GameName(GameId id);
bool IsEnglish(const Game& game);
bool IsSupported(const Game& game);
std::vector<Game> DetectGames();
