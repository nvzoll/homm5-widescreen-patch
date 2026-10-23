#include "games.h"

#include "sha256.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <optional>

namespace {

constexpr std::array KNOWN_BUILDS = {
    KnownBuild{GameId::Base, L"GOG", L"1.6", "820786b0f85c62ca96a1a5b18d03d60185047ae8dab09ef6f0b299e3abcfba7c"},
    KnownBuild{GameId::HammersOfFate, L"GOG", L"2.1", "c15cc3c68a5bd1f3111d7d2fda7ef7607dbe96d1aedadaf463b50cbe41b40730"},
    KnownBuild{GameId::TribesOfTheEast, L"GOG", L"3.1", "3eb6fce81071f48631360202d81d41c67b09d8b1807105b1f22c402e15d16268"},
    KnownBuild{GameId::Base, L"Steam", L"1.6", "40c15cd4efcb1e2225c8d95039cdba096f739c1503023a1aefd9ada7359043aa"},
    KnownBuild{GameId::HammersOfFate, L"Steam", L"2.1", "57f9bb7cb760b90e35989aeebdc1c85f8dba41d8f778e38b9e7208de76bc8566"},
    KnownBuild{GameId::TribesOfTheEast, L"Steam", L"3.1", "07b44a30dba8d9039ffa21b00fc9cecb2a77fa9db003ca15383314220b442d25"},
};

struct StoreProduct {
    const wchar_t* id;
    std::array<std::pair<GameId, const wchar_t*>, 2> exes;
    size_t exeCount;
};

constexpr std::array GOG_PRODUCTS = {
    StoreProduct{L"1207661143", {{{GameId::Base, L"bin\\H5_Game.exe"}, {GameId::HammersOfFate, L"bina1\\H5_Game.exe"}}}, 2},
    StoreProduct{L"1207661193", {{{GameId::TribesOfTheEast, L"bin\\H5_Game.exe"}, {}}}, 1},
};

constexpr std::array STEAM_APPS = {
    StoreProduct{L"15170", {{{GameId::Base, L"bin\\H5_Game.exe"}, {}}}, 1},
    StoreProduct{L"15380", {{{GameId::HammersOfFate, L"bina1\\H5_Game.exe"}, {}}}, 1},
    StoreProduct{L"15370", {{{GameId::TribesOfTheEast, L"bin\\H5_Game.exe"}, {}}}, 1},
};

std::optional<std::wstring> ReadRegistryString(HKEY root, const std::wstring& subkey, const wchar_t* value) {
    DWORD size = 0;
    const DWORD flags = RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY;

    if (RegGetValueW(root, subkey.c_str(), value, flags, nullptr, nullptr, &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    std::wstring text(size / sizeof(wchar_t), L'\0');

    if (RegGetValueW(root, subkey.c_str(), value, flags, nullptr, text.data(), &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    text.resize(wcsnlen(text.c_str(), text.size()));
    return text;
}

const KnownBuild* FindBuild(GameId id, const std::string& sha256) {
    for (const auto& build : KNOWN_BUILDS) {
        if (build.game == id && build.sha256 == sha256) {
            return &build;
        }
    }

    return nullptr;
}

void AddProductGames(const StoreProduct& product, const wchar_t* store, const std::wstring& version, const std::wstring& language,
                     const std::filesystem::path& root, std::vector<Game>& games) {
    for (size_t i = 0; i < product.exeCount; ++i) {
        const auto& [id, relative] = product.exes[i];
        Game game{id, store, version, language, root, root / relative};
        std::error_code ec{};

        if (!std::filesystem::exists(game.exe, ec)) {
            continue;
        }

        game.exeSha256 = Sha256File(game.exe).value_or("");
        game.build = FindBuild(id, game.exeSha256);
        games.push_back(std::move(game));
    }
}

void DetectGog(std::vector<Game>& games) {
    for (const auto& product : GOG_PRODUCTS) {
        const std::wstring key = std::wstring(L"SOFTWARE\\GOG.com\\Games\\") + product.id;
        const auto path = ReadRegistryString(HKEY_LOCAL_MACHINE, key, L"path");
        if (!path) {
            continue;
        }

        auto version = ReadRegistryString(HKEY_LOCAL_MACHINE, key, L"ver");
        if (version && version->ends_with(L" GOG")) {
            version->resize(version->size() - 4);
        }

        const auto language = ReadRegistryString(HKEY_LOCAL_MACHINE, key, L"language");
        AddProductGames(product, L"GOG", version.value_or(L""), language.value_or(L""), *path, games);
    }
}

struct VdfToken {
    std::wstring text;
    bool quoted;
};

std::vector<VdfToken> ReadVdf(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    const std::string utf8((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::wstring text(utf8.size(), L'\0');
    text.resize(MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), text.data(), static_cast<int>(text.size())));

    std::vector<VdfToken> tokens{};

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'{' || text[i] == L'}') {
            tokens.push_back({std::wstring(1, text[i]), false});
        } else if (text[i] == L'"') {
            std::wstring value{};

            for (++i; i < text.size() && text[i] != L'"'; ++i) {
                if (text[i] == L'\\' && i + 1 < text.size()) {
                    ++i;
                }

                value += text[i];
            }

            tokens.push_back({std::move(value), true});
        }
    }

    return tokens;
}

std::vector<std::wstring> VdfValues(const std::vector<VdfToken>& tokens, const wchar_t* key) {
    std::vector<std::wstring> values{};

    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        if (tokens[i].quoted && tokens[i + 1].quoted && _wcsicmp(tokens[i].text.c_str(), key) == 0) {
            values.push_back(tokens[++i].text);
        }
    }

    return values;
}

std::vector<std::filesystem::path> SteamLibraries() {
    auto steam = ReadRegistryString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (!steam) {
        steam = ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath");
    }

    if (!steam) {
        return {};
    }

    std::vector<std::filesystem::path> libraries{std::filesystem::path(*steam).lexically_normal()};

    for (const auto& path : VdfValues(ReadVdf(libraries[0] / L"steamapps\\libraryfolders.vdf"), L"path")) {
        const auto library = std::filesystem::path(path).lexically_normal();
        const auto same = [&](const std::filesystem::path& known) { return _wcsicmp(known.c_str(), library.c_str()) == 0; };

        if (std::none_of(libraries.begin(), libraries.end(), same)) {
            libraries.push_back(library);
        }
    }

    return libraries;
}

// libraryfolders.vdf's per-library app lists go stale, so each library's manifests are probed directly.
void DetectSteam(std::vector<Game>& games) {
    for (const auto& library : SteamLibraries()) {
        for (const auto& app : STEAM_APPS) {
            const auto manifest = ReadVdf(library / L"steamapps" / (std::wstring(L"appmanifest_") + app.id + L".acf"));
            const auto installDir = VdfValues(manifest, L"installdir");
            if (installDir.empty()) {
                continue;
            }

            const auto languages = VdfValues(manifest, L"language");
            const std::wstring language = languages.empty() ? L"" : languages.front();
            AddProductGames(app, L"Steam", L"", language, library / L"steamapps\\common" / installDir.front(), games);
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
    std::wstring language = game.language;
    CharLowerBuffW(language.data(), static_cast<DWORD>(language.size()));
    return language.find(L"chin") == std::wstring::npos && !language.starts_with(L"zh");
}

bool IsSupported(const Game& game) {
    return game.build != nullptr;
}

std::vector<Game> DetectGames() {
    std::vector<Game> games{};
    DetectGog(games);
    DetectSteam(games);
    return games;
}
