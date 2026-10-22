#include "games.h"

#include "sha256.h"

#include <windows.h>

#include <array>
#include <optional>

namespace {

constexpr std::array kKnownBuilds = {
    KnownBuild{GameId::Base, L"GOG", L"1.6", "820786b0f85c62ca96a1a5b18d03d60185047ae8dab09ef6f0b299e3abcfba7c"},
    KnownBuild{GameId::HammersOfFate, L"GOG", L"2.1", "c15cc3c68a5bd1f3111d7d2fda7ef7607dbe96d1aedadaf463b50cbe41b40730"},
    KnownBuild{GameId::TribesOfTheEast, L"GOG", L"3.1", "3eb6fce81071f48631360202d81d41c67b09d8b1807105b1f22c402e15d16268"},
};

struct GogProduct {
    const wchar_t* id;
    std::array<std::pair<GameId, const wchar_t*>, 2> exes;
    size_t exeCount;
};

constexpr std::array kGogProducts = {
    GogProduct{L"1207661143", {{{GameId::Base, L"bin\\H5_Game.exe"}, {GameId::HammersOfFate, L"bina1\\H5_Game.exe"}}}, 2},
    GogProduct{L"1207661193", {{{GameId::TribesOfTheEast, L"bin\\H5_Game.exe"}, {}}}, 1},
};

std::optional<std::wstring> ReadRegistryString(HKEY root, const std::wstring& subkey, const wchar_t* value) {
    DWORD size = 0;
    const DWORD flags = RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY;
    if (RegGetValueW(root, subkey.c_str(), value, flags, nullptr, nullptr, &size) != ERROR_SUCCESS) return std::nullopt;
    std::wstring text(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, subkey.c_str(), value, flags, nullptr, text.data(), &size) != ERROR_SUCCESS) return std::nullopt;
    text.resize(wcsnlen(text.c_str(), text.size()));
    return text;
}

const KnownBuild* FindBuild(GameId id, const std::string& sha256) {
    for (const auto& build : kKnownBuilds) {
        if (build.game == id && build.sha256 == sha256) return &build;
    }
    return nullptr;
}

void DetectGog(std::vector<Game>& games) {
    for (const auto& product : kGogProducts) {
        const std::wstring key = std::wstring(L"SOFTWARE\\GOG.com\\Games\\") + product.id;
        const auto path = ReadRegistryString(HKEY_LOCAL_MACHINE, key, L"path");
        if (!path) continue;
        auto version = ReadRegistryString(HKEY_LOCAL_MACHINE, key, L"ver");
        if (version && version->ends_with(L" GOG")) version->resize(version->size() - 4);
        const auto language = ReadRegistryString(HKEY_LOCAL_MACHINE, key, L"language");
        for (size_t i = 0; i < product.exeCount; ++i) {
            const auto& [id, relative] = product.exes[i];
            Game game{id, L"GOG", version.value_or(L""), language.value_or(L""), *path, std::filesystem::path(*path) / relative};
            if (!std::filesystem::exists(game.exe)) continue;
            game.exeSha256 = Sha256File(game.exe).value_or("");
            game.build = FindBuild(id, game.exeSha256);
            games.push_back(std::move(game));
        }
    }
}

}  // namespace

std::wstring_view GameName(GameId id) {
    switch (id) {
        case GameId::Base: return L"Heroes V";
        case GameId::HammersOfFate: return L"Hammers of Fate";
        case GameId::TribesOfTheEast: return L"Tribes of the East";
    }
    return L"?";
}

bool SupportsReadableFonts(const Game& game) {
    return _wcsnicmp(game.language.c_str(), L"chin", 4) != 0 && _wcsnicmp(game.language.c_str(), L"zh", 2) != 0;
}

bool IsSupported(const Game& game) {
    return game.build != nullptr;
}

std::vector<Game> DetectGames() {
    std::vector<Game> games;
    DetectGog(games);
    return games;
}
